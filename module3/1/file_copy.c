#include "file_copy.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

static void print_usage(const char *program_name)
{
    fprintf(stderr,
            "Использование:\n"
            "  %s файл...\n"
            "  %s -p имя_канала файл...\n",
            program_name,
            program_name);
}

int parse_arguments(int argc,
                    char *argv[],
                    struct program_options *options)
{
    options->fifo_name = NULL;
    options->first_file_index = 1;

    if (argc < 2) {
        fprintf(stderr, "Ошибка: не указаны файлы.\n");
        print_usage(argv[0]);
        return -1;
    }

    if (strcmp(argv[1], "-p") == 0) {
        if (argc < 4) {
            fprintf(stderr,
                    "Ошибка: после -p нужно указать имя канала "
                    "и хотя бы один файл.\n");
            print_usage(argv[0]);
            return -1;
        }

        options->fifo_name = argv[2];
        options->first_file_index = 3;
    } else if (argv[1][0] == '-') {
        fprintf(stderr, "Ошибка: неизвестный ключ %s.\n", argv[1]);
        print_usage(argv[0]);
        return -1;
    }

    return 0;
}

static void print_selected_files(const char *role,
                                 int argc,
                                 char *argv[],
                                 int first_file_index)
{
    int file_number = 1;

    for (int i = first_file_index; i < argc; ++i) {
        printf("[%s] Файл %d: %s\n", role, file_number, argv[i]);
        ++file_number;
    }
}

static int run_child(const struct program_options *options,
                     int argc,
                     char *argv[])
{
    printf("[Ребёнок] PID=%ld, PPID=%ld.\n",
           (long)getpid(),
           (long)getppid());
    puts("[Ребёнок] В следующих этапах здесь будут создаваться файлы .copy.");

    print_selected_files("Ребёнок",
                         argc,
                         argv,
                         options->first_file_index);

    return EXIT_SUCCESS;
}

static int wait_for_child(pid_t child_pid)
{
    int status;
    pid_t wait_result;

    do {
        wait_result = waitpid(child_pid, &status, 0);
    } while (wait_result == -1 && errno == EINTR);

    if (wait_result == -1) {
        perror("waitpid");
        return EXIT_FAILURE;
    }

    if (WIFEXITED(status)) {
        int exit_code = WEXITSTATUS(status);

        printf("[Родитель] Ребёнок завершился с кодом %d.\n", exit_code);
        return exit_code == EXIT_SUCCESS ? EXIT_SUCCESS : EXIT_FAILURE;
    }

    if (WIFSIGNALED(status)) {
        fprintf(stderr,
                "[Родитель] Ребёнок завершён сигналом %d.\n",
                WTERMSIG(status));
    } else {
        fprintf(stderr, "[Родитель] Неожиданный статус ребёнка.\n");
    }

    return EXIT_FAILURE;
}

static int run_parent(pid_t child_pid,
                      const struct program_options *options,
                      int argc,
                      char *argv[])
{
    printf("[Родитель] PID=%ld, создан ребёнок PID=%ld.\n",
           (long)getpid(),
           (long)child_pid);

    if (options->fifo_name == NULL) {
        puts("[Родитель] Режим каналов: неименованные pipe.");
    } else {
        printf("[Родитель] Режим каналов: FIFO, базовое имя: %s.\n",
               options->fifo_name);
    }

    print_selected_files("Родитель",
                         argc,
                         argv,
                         options->first_file_index);

    return wait_for_child(child_pid);
}

int file_copy_run(const struct program_options *options,
                  int argc,
                  char *argv[])
{
    pid_t child_pid = fork();

    if (child_pid == -1) {
        perror("fork");
        return EXIT_FAILURE;
    }

    if (child_pid == 0) {
        return run_child(options, argc, argv);
    }

    return run_parent(child_pid, options, argc, argv);
}