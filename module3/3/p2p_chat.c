#define _POSIX_C_SOURCE 200809L

#include "p2p_chat.h"

#include <errno.h>
#include <fcntl.h>
#include <mqueue.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <pthread.h>
#include <poll.h>
#include <signal.h>
#include <stdatomic.h>
#include <time.h>
#include <unistd.h>

#define QUEUE_NAME_SIZE 256
#define MESSAGE_SIZE 1024
#define NORMAL_PRIORITY 1
#define FINISH_PRIORITY 0
#define INPUT_PROMPT "Введите сообщение (/exit или Ctrl+C — завершение): "

/* Данные, передаваемые потоку-получателю. */
struct receiver_context {
    mqd_t receive_queue;
    int receive_failed;
    atomic_int peer_finished;
    atomic_int local_finished;
};

/* Флаг устанавливается при нажатии Ctrl+C. */
static volatile sig_atomic_t stop_requested = 0;

/* Не даёт двум потокам одновременно печатать в терминал. */
static pthread_mutex_t output_mutex = PTHREAD_MUTEX_INITIALIZER;

/* Выводит приглашение для ввода сообщения. */
static void print_input_prompt(void)
{
    pthread_mutex_lock(&output_mutex);
    printf("%s", INPUT_PROMPT);
    fflush(stdout);
    pthread_mutex_unlock(&output_mutex);
}

/* Очищает незавершённую строку приглашения в интерактивном терминале. */
static void clear_input_prompt(void)
{
    if (isatty(STDOUT_FILENO)) {
        printf("\r\033[2K");
    } else {
        printf("\n");
    }
}

static void handle_sigint(int signal_number)
{
    (void)signal_number;
    stop_requested = 1;
}

/* Настраивает обработку Ctrl+C без немедленного завершения процесса. */
static int install_sigint_handler(void)
{
    struct sigaction action = {0};

    action.sa_handler = handle_sigint;
    sigemptyset(&action.sa_mask);
    action.sa_flags = 0;

    return sigaction(SIGINT, &action, NULL);
}

/* Постоянно получает сообщения второй стороны до команды завершения. */
static void *receive_message(void *argument)
{
    struct receiver_context *context = argument;
    int finish_wait_timeouts = 0;

    while (!stop_requested) {
        char incoming_message[MESSAGE_SIZE];
        unsigned int received_priority;

        /*
         * Ограничиваем ожидание 0,1 секунды, чтобы поток мог
         * заметить Ctrl+C даже при отсутствии второй стороны.
         */
        struct timespec deadline;

        if (clock_gettime(CLOCK_REALTIME, &deadline) == -1) {
            perror("clock_gettime");
            context->receive_failed = 1;
            return NULL;
        }

        deadline.tv_nsec += 100000000L;

        if (deadline.tv_nsec >= 1000000000L) {
            ++deadline.tv_sec;
            deadline.tv_nsec -= 1000000000L;
        }

        ssize_t bytes_received =
            mq_timedreceive(context->receive_queue,
                            incoming_message,
                            sizeof(incoming_message),
                            &received_priority,
                            &deadline);

        if (bytes_received == -1) {
            /*
            * Если ожидание сообщения прервал Ctrl+C,
            * поток-получатель корректно завершается.
            */
            if (errno == EINTR) {
                continue;
            }

            if (errno == ETIMEDOUT) {
                /*
                 * После локального /exit некоторое время ждём ответ.
                 * Если второй стороны нет, завершаем поток самостоятельно.
                 */
                if (atomic_load(&context->local_finished)) {
                    ++finish_wait_timeouts;

                    if (finish_wait_timeouts >= 10) {
                        return NULL;
                    }
                }

                continue;
            }

            perror("mq_receive");
            context->receive_failed = 1;
            return NULL;
        }

        /*
         * Гарантируем наличие конца строки,
         * даже если отправитель не передал '\0'.
         */
        if ((size_t)bytes_received < sizeof(incoming_message)) {
            incoming_message[bytes_received] = '\0';
        } else {
            incoming_message[sizeof(incoming_message) - 1] = '\0';
        }

        /*
         * Сообщение с приоритетом FINISH_PRIORITY
         * уведомляет о завершении второй стороны.
         */
        if (received_priority == FINISH_PRIORITY) {
            pthread_mutex_lock(&output_mutex);
            clear_input_prompt();
            printf("Вторая сторона завершает работу.\n");
            fflush(stdout);
            pthread_mutex_unlock(&output_mutex);

            atomic_store(&context->peer_finished, 1);
            return NULL;
        }

        pthread_mutex_lock(&output_mutex);
        clear_input_prompt();
        printf("Получено сообщение: %s\n", incoming_message);
        printf("%s", INPUT_PROMPT);
        fflush(stdout);
        pthread_mutex_unlock(&output_mutex);
    }

    return NULL;
}

int run_p2p_chat(const char *base_name)
{
    /*
     * Пользователь может указать chat или /chat.
     * Внутри оставляем имя без начального символа '/'.
     */
    if (base_name[0] == '/') {
        ++base_name;
    }

    if (base_name[0] == '\0'
        || strchr(base_name, '/') != NULL) {
        fprintf(stderr,
                "Ошибка: неправильное имя очереди.\n");

        return EXIT_FAILURE;
    }

    char queue_1_name[QUEUE_NAME_SIZE];
    char queue_2_name[QUEUE_NAME_SIZE];

    int first_written =
        snprintf(queue_1_name,
                 sizeof(queue_1_name),
                 "/%s_1",
                 base_name);

    int second_written =
        snprintf(queue_2_name,
                 sizeof(queue_2_name),
                 "/%s_2",
                 base_name);

    if (first_written < 0
        || (size_t)first_written >= sizeof(queue_1_name)
        || second_written < 0
        || (size_t)second_written >= sizeof(queue_2_name)) {
        fprintf(stderr,
                "Ошибка: имя очереди слишком длинное.\n");

        return EXIT_FAILURE;
    }

    /*
    * Задаём максимальное количество сообщений
    * и максимальный размер одного сообщения.
    */
    struct mq_attr attributes = {0};

    attributes.mq_maxmsg = 10;
    attributes.mq_msgsize = 1024;

    /*
    * Сначала пытаемся эксклюзивно создать первую очередь.
    * Если это удалось, данный процесс является создателем.
    */
    mqd_t queue_1 =
        mq_open(queue_1_name,
                O_CREAT | O_EXCL | O_RDWR,
                0600,
                &attributes);

    mqd_t queue_2;
    int is_creator = 0;

    if (queue_1 != (mqd_t)-1) {
        is_creator = 1;

        queue_2 =
            mq_open(queue_2_name,
                    O_CREAT | O_EXCL | O_RDWR,
                    0600,
                    &attributes);

        if (queue_2 == (mqd_t)-1) {
            perror("mq_open queue_2");

            mq_close(queue_1);
            mq_unlink(queue_1_name);

            return EXIT_FAILURE;
        }
    } else if (errno == EEXIST) {
        /*
        * Первая очередь уже существует:
        * подключаемся как второй участник.
        */
        queue_1 = mq_open(queue_1_name, O_RDWR);
        queue_2 = mq_open(queue_2_name, O_RDWR);

        if (queue_1 == (mqd_t)-1
            || queue_2 == (mqd_t)-1) {
            perror("mq_open existing queues");

            if (queue_1 != (mqd_t)-1) {
                mq_close(queue_1);
            }

            if (queue_2 != (mqd_t)-1) {
                mq_close(queue_2);
            }

            return EXIT_FAILURE;
        }
    } else {
        perror("mq_open queue_1");
        return EXIT_FAILURE;
    }

    printf("Первая очередь: %s\n", queue_1_name);
    printf("Вторая очередь: %s\n", queue_2_name);

    /*
    * Создатель принимает через первую очередь и отправляет через вторую.
    * Второй участник использует очереди в обратном направлении.
    */
    mqd_t receive_queue;
    mqd_t send_queue;

    if (is_creator) {
        receive_queue = queue_1;
        send_queue = queue_2;
    } else {
        send_queue = queue_1;
        receive_queue = queue_2;
    }

    if (is_creator) {
        printf("Роль: создатель очередей.\n");
        printf("Приём: %s\n", queue_1_name);
        printf("Отправка: %s\n", queue_2_name);
    } else {
        printf("Роль: второй участник.\n");
        printf("Отправка: %s\n", queue_1_name);
        printf("Приём: %s\n", queue_2_name);
    }

    if (install_sigint_handler() == -1) {
        perror("sigaction");

        mq_close(queue_1);
        mq_close(queue_2);

        if (is_creator) {
            mq_unlink(queue_1_name);
            mq_unlink(queue_2_name);
        }

        return EXIT_FAILURE;
    }

    /*
     * Отключаем чтение stdin с опережением, чтобы poll()
     * видел каждую следующую строку перед вызовом fgets().
     */
    if (setvbuf(stdin, NULL, _IONBF, 0) != 0) {
        fprintf(stderr,
                "Ошибка: не удалось настроить стандартный ввод.\n");

        mq_close(queue_1);
        mq_close(queue_2);

        if (is_creator) {
            mq_unlink(queue_1_name);
            mq_unlink(queue_2_name);
        }

        return EXIT_FAILURE;
    }

    struct receiver_context receiver = {
        .receive_queue = receive_queue,
        .receive_failed = 0
    };

    atomic_init(&receiver.peer_finished, 0);
    atomic_init(&receiver.local_finished, 0);

    pthread_t receiver_thread;
    /* Создаём дополнительный поток для приёма сообщений второй стороны. */
    int thread_error =
        pthread_create(&receiver_thread,
                    NULL,
                    receive_message,
                    &receiver);

    if (thread_error != 0) {
        fprintf(stderr,
                "Ошибка: не удалось создать поток-получатель.\n");

        mq_close(queue_1);
        mq_close(queue_2);

        if (is_creator) {
            mq_unlink(queue_1_name);
            mq_unlink(queue_2_name);
        }

        return EXIT_FAILURE;
    }

    int send_failed = 0;

    print_input_prompt();
    /* Ожидаем, проверяем и отправляем сообщения пользователя. */
    while (!stop_requested
        && !atomic_load(&receiver.peer_finished)) {
        char outgoing_message[MESSAGE_SIZE];

        /*
        * Проверяем наличие ввода каждые 0,1 секунды.
        * Это позволяет заметить SIGINT или завершение второй стороны.
        */
        struct pollfd input = {
            .fd = STDIN_FILENO,
            .events = POLLIN
        };

        int poll_result = poll(&input, 1, 100);
        /* poll() возвращает -1 при ошибке, 0 при тайм-ауте и >0 при готовности. */
        if (poll_result == -1) {
            if (errno == EINTR) {
                continue;
            }

            perror("poll");
            send_failed = 1;
            break;
        }
        /* Если ввода нет, продолжаем ожидание. */
        if (poll_result == 0) {
            continue;
        }

        if ((input.revents & (POLLERR | POLLNVAL)) != 0) {
            fprintf(stderr,
                    "Ошибка ожидания пользовательского ввода.\n");
            send_failed = 1;
            break;
        }

        if ((input.revents & (POLLIN | POLLHUP)) == 0) {
            continue;
        }
        /* Читаем сообщение из стандартного ввода.*/
        if (fgets(outgoing_message,
                sizeof(outgoing_message),
                stdin) == NULL) {
            if (stop_requested || feof(stdin)) {
                break;
            }

            if (errno == EINTR) {
                clearerr(stdin);
                continue;
            }

            perror("fgets");
            send_failed = 1;
            break;
        }
        /*
         * Удаляем символ перевода строки, если он присутствует.
         */
        char *newline = strchr(outgoing_message, '\n');

        if (newline != NULL) {
            *newline = '\0';
        } else if (!feof(stdin)) {
            /*
             * Строка не поместилась в буфер. Удаляем её остаток
             * из stdin, чтобы он не стал следующим сообщением.
             */
            int character;

            do {
                character = getchar();
            } while (character != '\n'
                     && character != EOF);

            fprintf(stderr,
                    "Ошибка: сообщение слишком длинное.\n");

            print_input_prompt();
            continue;
        }

        if (outgoing_message[0] == '\0') {
            fprintf(stderr,
                    "Ошибка: сообщение не может быть пустым.\n");

            print_input_prompt();
            continue;
        }

        if (strcmp(outgoing_message, "/exit") == 0) {
            break;
        }
        /* Отправляем сообщение второй стороне. */
        if (mq_send(send_queue,
                    outgoing_message,
                    strlen(outgoing_message) + 1,
                    NORMAL_PRIORITY) == -1) {
            perror("mq_send");
            send_failed = 1;
            break;
        }

        printf("Сообщение отправлено.\n");
        print_input_prompt();
    }

    /* Сообщаем потоку-получателю, что локальный ввод завершён. */
    atomic_store(&receiver.local_finished, 1);

    /*
    * Уведомляем вторую сторону при /exit, Ctrl+C,
    * закрытии ввода или получении её уведомления.
    */
    if (!send_failed) {
        if (mq_send(send_queue,
                    "FINISH",
                    sizeof("FINISH"),
                    FINISH_PRIORITY) == -1) {
            perror("mq_send FINISH");
            send_failed = 1;
        } else {
            printf("Уведомление о завершении отправлено.\n");
        }
    }

    /* Ожидаем завершения потока-получателя. */
    thread_error = pthread_join(receiver_thread, NULL);

    if (thread_error != 0) {
        fprintf(stderr,
                "Ошибка: не удалось дождаться потока-получателя.\n");

        mq_close(queue_1);
        mq_close(queue_2);

        if (is_creator) {
            mq_unlink(queue_1_name);
            mq_unlink(queue_2_name);
        }

        return EXIT_FAILURE;
    }

    if (receiver.receive_failed) {
        mq_close(queue_1);
        mq_close(queue_2);

        if (is_creator) {
            mq_unlink(queue_1_name);
            mq_unlink(queue_2_name);
        }

        return EXIT_FAILURE;
    }

    mq_close(queue_1);
    mq_close(queue_2);

    if (is_creator) {
        mq_unlink(queue_1_name);
        mq_unlink(queue_2_name);

        printf("Очереди удалены.\n");
    }

    return send_failed
        ? EXIT_FAILURE
        : EXIT_SUCCESS;
}
