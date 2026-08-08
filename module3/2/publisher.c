#include "publisher.h"

#include <errno.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "message_queue.h"
#include "protocol.h"
#include "signal_control.h"

/* Отправляет брокеру регистрацию или завершение издателя. */
static int send_publisher_command(int queue_id,
                                  const char *command,
                                  long publisher_pid)
{
    char message_text[MESSAGE_TEXT_SIZE];

    int written = snprintf(message_text,
                           sizeof(message_text),
                           "%s,%ld",
                           command,
                           publisher_pid);

    if (written < 0
        || (size_t)written >= sizeof(message_text)) {
        errno = EMSGSIZE;
        return -1;
    }

    return send_queue_message(queue_id,
                              BROKER_MESSAGE_TYPE,
                              message_text);
}

int run_publisher(const char *topic)
{
    int queue_id = open_message_queue();

    if (queue_id == -1) {
        if (errno == ENOENT) {
            fprintf(stderr,
                    "Ошибка: брокер ещё не запущен.\n");
        } else {
            perror("msgget");
        }

        return EXIT_FAILURE;
    }

    if (install_stop_signal_handler() == -1) {
        perror("sigaction");
        return EXIT_FAILURE;
    }

    long publisher_pid = (long)getpid();

    if (send_publisher_command(queue_id,
                               "publisher_register",
                               publisher_pid) == -1) {
        perror("msgsnd publisher_register");
        return EXIT_FAILURE;
    }

    printf("Режим: издатель. PID=%ld.\n", publisher_pid);
    printf("Тема: %s\n", topic);
    printf("Для завершения нажмите Ctrl+C или Ctrl+D.\n");

    int exit_code = EXIT_SUCCESS;
    int queue_available = 1;

    while (!is_stop_requested()) {
        char payload[PAYLOAD_SIZE];

        printf("Введите текст сообщения: ");
        fflush(stdout);

        /*
         * Периодически проверяем stdin, чтобы SIGINT не мог
         * оставить издателя навсегда заблокированным в fgets().
         */
        while (!is_stop_requested()) {
            struct pollfd input = {
                .fd = STDIN_FILENO,
                .events = POLLIN
            };

            int poll_result = poll(&input, 1, 100);

            if (poll_result == -1) {
                if (errno == EINTR) {
                    continue;
                }

                perror("poll");
                exit_code = EXIT_FAILURE;
                break;
            }

            if (poll_result == 0) {
                size_t queued_message_count;

                if (get_message_queue_count(
                        queue_id,
                        &queued_message_count) == -1) {
                    if (errno == EIDRM || errno == EINVAL) {
                        printf("\n[Издатель] Очередь брокера недоступна.\n");
                        queue_available = 0;
                        break;
                    }

                    perror("msgctl IPC_STAT");
                    exit_code = EXIT_FAILURE;
                    break;
                }

                continue;
            }

            if ((input.revents & (POLLIN | POLLHUP)) != 0) {
                break;
            }

            if ((input.revents & (POLLERR | POLLNVAL)) != 0) {
                fprintf(stderr,
                        "Ошибка ожидания пользовательского ввода.\n");
                exit_code = EXIT_FAILURE;
                break;
            }
        }

        if (is_stop_requested()
            || !queue_available
            || exit_code == EXIT_FAILURE) {
            break;
        }

        errno = 0;

        if (fgets(payload, sizeof(payload), stdin) == NULL) {
            if (is_stop_requested() || feof(stdin)) {
                break;
            }

            if (errno == EINTR) {
                clearerr(stdin);
                continue;
            }

            perror("fgets");
            exit_code = EXIT_FAILURE;
            break;
        }

        char *newline = strchr(payload, '\n');

        if (newline != NULL) {
            *newline = '\0';
        } else if (!feof(stdin)) {
            /*
             * Введённая строка не поместилась в payload.
             * Удаляем её оставшуюся часть из stdin.
             */
            int character;

            do {
                character = getchar();
            } while (character != '\n'
                     && character != EOF);

            fprintf(stderr,
                    "Ошибка: сообщение слишком длинное.\n");
            continue;
        }

        if (payload[0] == '\0') {
            fprintf(stderr,
                    "Ошибка: сообщение не может быть пустым.\n");
            continue;
        }

        char message_text[MESSAGE_TEXT_SIZE];

        int written = snprintf(message_text,
                               sizeof(message_text),
                               "send,%ld,%s,%s",
                               publisher_pid,
                               topic,
                               payload);

        if (written < 0
            || (size_t)written >= sizeof(message_text)) {
            fprintf(stderr,
                    "Ошибка: публикация слишком длинная.\n");
            continue;
        }

        if (send_queue_message(queue_id,
                               BROKER_MESSAGE_TYPE,
                               message_text) == -1) {
            if (errno == EIDRM || errno == EINVAL) {
                printf("\n[Издатель] Очередь брокера недоступна.\n");
                queue_available = 0;
                break;
            }

            perror("msgsnd send");
            exit_code = EXIT_FAILURE;
            break;
        }

        printf("[Издатель] Сообщение отправлено брокеру.\n");
    }

    /*
     * Если очередь существует, сообщаем брокеру,
     * что издатель завершает работу.
     */
    if (queue_available) {
        if (send_publisher_command(queue_id,
                                   "publisher_unregister",
                                   publisher_pid) == -1) {
            if (errno != EIDRM && errno != EINVAL) {
                perror("msgsnd publisher_unregister");
                exit_code = EXIT_FAILURE;
            }
        }
    }

    printf("\n[Издатель] Работа завершена.\n");

    return exit_code;
}
