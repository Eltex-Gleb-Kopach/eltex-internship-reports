#include "file_copy.h"

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#define FILE_NAME_BUFFER_SIZE 4096
#define DATA_BUFFER_SIZE 4096
#define COPY_SUFFIX ".copy"

enum message_type {
    MESSAGE_FILE = 1,
    MESSAGE_DONE = 2
};

struct file_metadata {
    enum message_type type;
    char file_name[FILE_NAME_BUFFER_SIZE];
    off_t file_size;
};

static int create_fifo_channel(const char *fifo_name, int fifo_fds[2]){
    /* mkfifo() создаёт в файловой системе специальный файл канала.
     * Права 0600 разрешают чтение и запись только владельцу.*/
    if (mkfifo(fifo_name, 0600) == -1) {
        perror(fifo_name);
        return -1;
    }

    /* Обычное открытие FIFO только для чтения ждёт писателя.
     * O_NONBLOCK позволяет сначала открыть читающий конец,
     * чтобы затем без зависания открыть конец для записи.*/
    fifo_fds[0] = open(fifo_name, O_RDONLY | O_NONBLOCK);

    if (fifo_fds[0] == -1) {
        perror("open FIFO for reading");
        unlink(fifo_name);
        return -1;
    }

    fifo_fds[1] = open(fifo_name, O_WRONLY);

    if (fifo_fds[1] == -1) {
        perror("open FIFO for writing");

        close(fifo_fds[0]);
        unlink(fifo_name);

        return -1;
    }

    /* Получаем текущие флаги читающего дескриптора. */
    int read_flags = fcntl(fifo_fds[0], F_GETFL);

    if (read_flags == -1
        || fcntl(fifo_fds[0],
                 F_SETFL,
                 read_flags & ~O_NONBLOCK) == -1) {
        perror("fcntl FIFO");

        close(fifo_fds[0]);
        close(fifo_fds[1]);
        unlink(fifo_name);

        return -1;
    }

    return 0;
}

static int write_all(int fd, const void *buffer, size_t byte_count){

    const char *current_position = buffer;

    /* Количество байтов, которые уже удалось записать. */
    size_t total_written = 0;

    while (total_written < byte_count) {
        /* Записываем ещё не переданную часть буфера.*/
        ssize_t bytes_written = write(
            fd,
            current_position + total_written,
            byte_count - total_written
        );

        if (bytes_written == -1) {
            if (errno == EINTR) {
                continue;
            }

            return -1;
        }

        if (bytes_written == 0) {
            errno = EIO;
            return -1;
        }

        total_written += (size_t)bytes_written;
    }

    return 0;
}

static ssize_t read_all(int fd, void *buffer, size_t byte_count){

    char *current_position = buffer;

    /* Количество уже прочитанных байтов. */
    size_t total_read = 0;

    while (total_read < byte_count) {
        ssize_t bytes_read = read(
            fd,
            current_position + total_read,
            byte_count - total_read
        );

        if (bytes_read == -1) {
            if (errno == EINTR) {
                continue;
            }

            return -1;
        }

        if (bytes_read == 0) {
            break;
        }

        total_read += (size_t)bytes_read;
    }

    return (ssize_t)total_read;
}

/*Родитель читает исходный файл блоками и отправляет каждый блок ребёнку через data_write_fd.*/
static int send_file_content(int source_fd, int data_write_fd, off_t file_size){
    /* Временный буфер для одного блока содержимого. */
    char buffer[DATA_BUFFER_SIZE];

    /* Сколько байтов файла ещё осталось отправить. */
    off_t bytes_remaining = file_size;

    while (bytes_remaining > 0) {

        size_t requested_size =
            bytes_remaining < (off_t)sizeof(buffer)
            ? (size_t)bytes_remaining
            : sizeof(buffer);
        /*Хранит фактическое количество прочитанных байтов*/
        ssize_t bytes_read;

        do {
            bytes_read = read(source_fd,
                              buffer,
                              requested_size);
        } while (bytes_read == -1 && errno == EINTR);

        if (bytes_read == -1) {
            perror("read source file");
            return EXIT_FAILURE;
        }

        if (bytes_read == 0) {
            fprintf(stderr,
                    "[Родитель] Исходный файл закончился "
                    "раньше ожидаемого.\n");
            return EXIT_FAILURE;
        }

        if (write_all(data_write_fd,
                      buffer,
                      (size_t)bytes_read) == -1) {
            perror("write file content");
            return EXIT_FAILURE;
        }

        bytes_remaining -= bytes_read;
    }

    return EXIT_SUCCESS;
}

/*Ребёнок принимает блоки из data_read_fd и записывает их в файл-копию.*/
static int receive_file_content(int data_read_fd, int copy_fd, off_t file_size){
    /* Временный буфер для одного принятого блока. */
    char buffer[DATA_BUFFER_SIZE];

    /* Сколько байтов ребёнок ещё должен получить. */
    off_t bytes_remaining = file_size;

    while (bytes_remaining > 0) {
        size_t requested_size =
            bytes_remaining < (off_t)sizeof(buffer)
            ? (size_t)bytes_remaining
            : sizeof(buffer);

        /*Получаем ровно requested_size байтов.*/
        ssize_t bytes_read = read_all(data_read_fd,
                                      buffer,
                                      requested_size);

        if (bytes_read == -1) {
            perror("read file content");
            return EXIT_FAILURE;
        }

        /*Канал закрылся до получения всего содержимого.*/
        if (bytes_read != (ssize_t)requested_size) {
            fprintf(stderr,
                    "[Ребёнок] Содержимое файла "
                    "получено не полностью.\n");
            return EXIT_FAILURE;
        }

        /*Записываем весь полученный блок в файл .copy.*/
        if (write_all(copy_fd,
                      buffer,
                      (size_t)bytes_read) == -1) {
            perror("write copy file");
            return EXIT_FAILURE;
        }

        bytes_remaining -= bytes_read;
    }

    return EXIT_SUCCESS;
}

int parse_arguments(int argc, char *argv[], struct program_options *options){
    options->channel_name = NULL;
    options->first_file_index = 1;

    if (argc < 2) {
        fprintf(stderr, "Ошибка: не указаны файлы.\n");
        return -1;
    }

    if (strcmp(argv[1], "-p") == 0) {
        if (argc < 4) {
            fprintf(stderr,
                    "Ошибка: после -p нужно указать имя канала "
                    "и хотя бы один файл.\n");
            return -1;
        }
    
        options->channel_name = argv[2];
        options->first_file_index = 3;

    } else if (argv[1][0] == '-') {
        fprintf(stderr,
                "Ошибка: неизвестный ключ %s.\n",
                argv[1]);
        return -1;
    }
    return 0;
}

/* Ожидает завершения конкретного дочернего процесса и проверяет причину его завершения.*/
static int wait_for_child(pid_t child_pid){

    int status;
    /* Ждём завершения ребёнка и получаем информацию о его состоянии. */
    if (waitpid(child_pid, &status, 0) == -1) {
        perror("waitpid");
        return EXIT_FAILURE;
    }
    /* Ребёнок завершился самостоятельно через return или exit(). */
    if (WIFEXITED(status)) {

        int child_exit_code = WEXITSTATUS(status);

        printf("[Родитель] Ребёнок завершился с кодом %d.\n",
               child_exit_code);

        return child_exit_code == EXIT_SUCCESS
            ? EXIT_SUCCESS
            : EXIT_FAILURE;
    }
    /* Ребёнок был принудительно завершён сигналом. */
    if (WIFSIGNALED(status)) {
        fprintf(stderr,
                "[Родитель] Ребёнок завершён сигналом %d.\n",
                WTERMSIG(status));
    }

    return EXIT_FAILURE;
}

static int run_child(int ready_write_fd, int data_read_fd){
    const char ready_message[] = "READY";

    printf("[Ребёнок] PID=%ld, PPID=%ld\n",
           (long)getpid(),
           (long)getppid());

    /* Сообщаем родителю, что ребёнок готов принимать файл.*/
    if (write_all(ready_write_fd,
                  ready_message,
                  sizeof(ready_message)) == -1) {
        perror("write READY");

        close(ready_write_fd);
        close(data_read_fd);

        return EXIT_FAILURE;
    }

    close(ready_write_fd);

    printf("[Ребёнок] Сообщение READY отправлено.\n");

    while (1) {
        /* Сюда read_all() поместит очередное служебное сообщение. */
        struct file_metadata received_metadata;

        ssize_t bytes_read = read_all(
            data_read_fd,
            &received_metadata,
            sizeof(received_metadata)
        );

        if (bytes_read == -1) {
            perror("read metadata");

            close(data_read_fd);
            return EXIT_FAILURE;
        }

        if (bytes_read != (ssize_t)sizeof(received_metadata)) {
            fprintf(stderr,
                    "[Ребёнок] Служебное сообщение "
                    "получено не полностью.\n");

            close(data_read_fd);
            return EXIT_FAILURE;
        }

        /* DONE означает, что родитель уже отправил все файлы. */
        if (received_metadata.type == MESSAGE_DONE) {
            printf("[Ребёнок] Получено сообщение DONE.\n");
            break;
        }

        if (received_metadata.type != MESSAGE_FILE) {
            fprintf(stderr,
                    "[Ребёнок] Получен неизвестный тип сообщения.\n");

            close(data_read_fd);
            return EXIT_FAILURE;
        }

        /* Проверяем, что имя содержит признак конца строки '\0'. */
        if (memchr(received_metadata.file_name,
                   '\0',
                   sizeof(received_metadata.file_name)) == NULL) {
            fprintf(stderr,
                    "[Ребёнок] Получено неправильное имя файла.\n");

            close(data_read_fd);
            return EXIT_FAILURE;
        }

        printf("[Ребёнок] Получено имя файла: %s\n",
               received_metadata.file_name);

        printf("[Ребёнок] Получен размер файла: %lld байт.\n",
               (long long)received_metadata.file_size);

        /* Формируем имя нового файла с окончанием ".copy". */
        char copy_file_name[
            FILE_NAME_BUFFER_SIZE + sizeof(COPY_SUFFIX)
        ];

        int name_length = snprintf(copy_file_name,
                                   sizeof(copy_file_name),
                                   "%s%s",
                                   received_metadata.file_name,
                                   COPY_SUFFIX);

        if (name_length < 0
            || (size_t)name_length >= sizeof(copy_file_name)) {
            fprintf(stderr,
                    "[Ребёнок] Не удалось сформировать имя копии.\n");

            close(data_read_fd);
            return EXIT_FAILURE;
        }

        /* Создаём файл, в который ребёнок запишет копию. */
        int copy_fd = open(copy_file_name,
                           O_WRONLY | O_CREAT | O_TRUNC,
                           0644);

        if (copy_fd == -1) {
            perror(copy_file_name);

            close(data_read_fd);
            return EXIT_FAILURE;
        }

        /* Получаем содержимое текущего файла из канала. */
        int receive_result = receive_file_content(
            data_read_fd,
            copy_fd,
            received_metadata.file_size
        );

        if (close(copy_fd) == -1) {
            perror("close copy file");

            close(data_read_fd);
            return EXIT_FAILURE;
        }

        if (receive_result != EXIT_SUCCESS) {
            close(data_read_fd);
            return EXIT_FAILURE;
        }

        printf("[Ребёнок] Создана копия: %s (%lld байт).\n",
               copy_file_name,
               (long long)received_metadata.file_size);
    }

    /* После DONE новые данные из канала уже не придут. */
    close(data_read_fd);

    return EXIT_SUCCESS;
}

static int run_parent(pid_t child_pid,
                      int ready_read_fd,
                      int data_write_fd,
                      const struct program_options *options,
                      int argc,
                      char *argv[]){
    /* Буфер для сообщения READY. */
    char received_message[sizeof("READY")];

    /* Ждём и читаем полное сообщение готовности.*/
    ssize_t bytes_read = read_all(
        ready_read_fd,
        received_message,
        sizeof(received_message)
    );

    if (bytes_read == -1) {
        perror("read READY");

        close(ready_read_fd);
        close(data_write_fd);

        (void)wait_for_child(child_pid);
        return EXIT_FAILURE;
    }

    close(ready_read_fd);

    if (bytes_read != (ssize_t)sizeof(received_message)
        || strcmp(received_message, "READY") != 0) {
        fprintf(stderr,
                "[Родитель] Получено неправильное "
                "сообщение готовности.\n");

        close(data_write_fd);

        (void)wait_for_child(child_pid);
        return EXIT_FAILURE;
    }

    printf("[Родитель] Получено сообщение: %s\n",
           received_message);

    printf("[Родитель] PID=%ld, PID ребёнка=%ld\n",
           (long)getpid(),
           (long)child_pid);

    if (options->channel_name == NULL) {
        printf("[Родитель] Режим: неименованные каналы.\n");
    } else {
        printf("[Родитель] Режим: именованный канал FIFO, "
               "имя: %s.\n",
               options->channel_name);
    }

    /* По очереди обрабатываем все имена файлов,
       переданные пользователем в командной строке.*/
    for (int file_index = options->first_file_index;
         file_index < argc;
         ++file_index) {
        const char *file_name = argv[file_index];

        /* Открываем очередной исходный файл только для чтения. */
        int source_fd = open(file_name, O_RDONLY);

        if (source_fd == -1) {
            perror(file_name);

            close(data_write_fd);

            (void)wait_for_child(child_pid);
            return EXIT_FAILURE;
        }

        /* fstat() получает размер открытого файла. */
        struct stat source_info;

        if (fstat(source_fd, &source_info) == -1) {
            perror("fstat");

            close(source_fd);
            close(data_write_fd);

            (void)wait_for_child(child_pid);
            return EXIT_FAILURE;
        }

        printf("[Родитель] Файл открыт: %s\n",
               file_name);

        printf("[Родитель] Размер файла: %lld байт.\n",
               (long long)source_info.st_size);

        /* Создаём сообщение FILE и обнуляем остальные поля. */
        struct file_metadata metadata = {0};
        metadata.type = MESSAGE_FILE;

        /* Количество байтов имени вместе с '\0'. */
        size_t file_name_size = strlen(file_name) + 1;

        if (file_name_size > sizeof(metadata.file_name)) {
            fprintf(stderr,
                    "Ошибка: имя файла слишком длинное.\n");

            close(source_fd);
            close(data_write_fd);

            (void)wait_for_child(child_pid);
            return EXIT_FAILURE;
        }

        /* Копируем имя и размер файла в служебное сообщение. */
        memcpy(metadata.file_name,
               file_name,
               file_name_size);

        metadata.file_size = source_info.st_size;

        /* Сначала передаём ребёнку сообщение с метаданными. */
        if (write_all(data_write_fd,
                      &metadata,
                      sizeof(metadata)) == -1) {
            perror("write metadata");

            close(source_fd);
            close(data_write_fd);

            (void)wait_for_child(child_pid);
            return EXIT_FAILURE;
        }

        printf("[Родитель] Отправлены метаданные: %s, %lld байт.\n",
               metadata.file_name,
               (long long)metadata.file_size);

        /* Затем отправляем содержимое текущего файла блоками. */
        int send_result = send_file_content(
            source_fd,
            data_write_fd,
            metadata.file_size
        );

        if (close(source_fd) == -1) {
            perror("close source file");

            close(data_write_fd);

            (void)wait_for_child(child_pid);
            return EXIT_FAILURE;
        }

        if (send_result != EXIT_SUCCESS) {
            close(data_write_fd);

            (void)wait_for_child(child_pid);
            return EXIT_FAILURE;
        }

        printf("[Родитель] Содержимое файла отправлено: "
               "%lld байт.\n",
               (long long)metadata.file_size);
    }

    /*Все файлы отправлены. Сообщение DONE.*/
    struct file_metadata done_message = {0};
    done_message.type = MESSAGE_DONE;

    if (write_all(data_write_fd,
                  &done_message,
                  sizeof(done_message)) == -1) {
        perror("write DONE");

        close(data_write_fd);

        (void)wait_for_child(child_pid);
        return EXIT_FAILURE;
    }

    printf("[Родитель] Все файлы отправлены. Отправлено DONE.\n");

    /* После DONE родителю больше нечего записывать в канал. */
    close(data_write_fd);

    return wait_for_child(child_pid);
}

int run_file_copy(const struct program_options *options, int argc, char *argv[]){

    int ready_pipe[2];
    int data_pipe[2];

    if (pipe(ready_pipe) == -1) {
        perror("pipe ready");
        return EXIT_FAILURE;
    }

    if (options->channel_name == NULL) {
        if (pipe(data_pipe) == -1) {
            perror("pipe data");

            close(ready_pipe[0]);
            close(ready_pipe[1]);

            return EXIT_FAILURE;
        }
    } else {
        if (create_fifo_channel(options->channel_name, data_pipe) == -1) {
            close(ready_pipe[0]);
            close(ready_pipe[1]);

            return EXIT_FAILURE;
        }
    }

    pid_t child_pid = fork();

    if (child_pid == -1) {
        perror("fork");

        close(ready_pipe[0]);
        close(ready_pipe[1]);
        close(data_pipe[0]);
        close(data_pipe[1]);

        if (options->channel_name != NULL) {
            unlink(options->channel_name);
        }

        return EXIT_FAILURE;
    }

    if (child_pid == 0) {

        close(ready_pipe[0]);
        close(data_pipe[1]);

        return run_child(ready_pipe[1], data_pipe[0]);
    }


    close(ready_pipe[1]);
    close(data_pipe[0]);

    int result = run_parent(child_pid,
                            ready_pipe[0],
                            data_pipe[1],
                            options,
                            argc,
                            argv);

    if (options->channel_name != NULL
        && unlink(options->channel_name) == -1) {
        perror("unlink FIFO");
        return EXIT_FAILURE;
    }

    return result;
}
