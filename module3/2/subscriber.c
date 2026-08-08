#define _POSIX_C_SOURCE 200809L

#include "subscriber.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

#include "message_queue.h"
#include "protocol.h"
#include "signal_control.h"

#define RECEIVE_POLL_NANOSECONDS 100000000L

/* Отправляет брокеру команду подписки или отписки. */
static int send_subscription_command(int queue_id,
                                     const char *command,
                                     long subscriber_pid,
                                     const char *topic)
{
    char message_text[MESSAGE_TEXT_SIZE];

    int written = snprintf(message_text,
                           sizeof(message_text),
                           "%s,%ld,%s",
                           command,
                           subscriber_pid,
                           topic);

    if (written < 0
        || (size_t)written >= sizeof(message_text)) {
        errno = EMSGSIZE;
        return -1;
    }

    return send_queue_message(queue_id,
                              BROKER_MESSAGE_TYPE,
                              message_text);
}

int run_subscriber(int topic_count, char *topics[])
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

    long subscriber_pid = (long)getpid();

    printf("Режим: подписчик. PID=%ld.\n",
           subscriber_pid);

    /* Отправляем брокеру подписку на каждую тему. */
    for (int i = 0; i < topic_count; ++i) {
        if (send_subscription_command(queue_id,
                                      "subscribe",
                                      subscriber_pid,
                                      topics[i]) == -1) {
            perror("msgsnd subscribe");
            return EXIT_FAILURE;
        }

        printf("[Подписчик] Подписка на тему %s отправлена.\n",
               topics[i]);
    }

    printf("[Подписчик] Ожидание публикаций. "
           "Для завершения нажмите Ctrl+C.\n");

    int exit_code = EXIT_SUCCESS;
    int queue_available = 1;

    while (!is_stop_requested()) {
        struct queue_message publication;

        /*
         * Подписчик получает только сообщения,
         * тип которых равен его PID.
         */
        if (receive_queue_message_nowait(
                queue_id,
                subscriber_pid,
                &publication) == -1) {
            if (errno == EINTR) {
                continue;
            }

            if (errno == ENOMSG) {
                const struct timespec delay = {
                    .tv_sec = 0,
                    .tv_nsec = RECEIVE_POLL_NANOSECONDS
                };

                (void)nanosleep(&delay, NULL);
                continue;
            }

            /*
             * EIDRM — очередь была удалена.
             * EINVAL — идентификатор очереди больше недействителен.
             */
            if (errno == EIDRM || errno == EINVAL) {
                printf("[Подписчик] Очередь брокера недоступна.\n");
                queue_available = 0;
                break;
            }

            perror("msgrcv publication");
            exit_code = EXIT_FAILURE;
            break;
        }

        printf("[Подписчик] Получено: %s\n",
               publication.text);
    }

    /*
     * Если очередь ещё существует, перед завершением
     * сообщаем брокеру об отписке от каждой темы.
     */
    if (queue_available) {
        for (int i = 0; i < topic_count; ++i) {
            if (send_subscription_command(queue_id,
                                          "unsubscribe",
                                          subscriber_pid,
                                          topics[i]) == -1) {
                if (errno == EIDRM || errno == EINVAL) {
                    printf("[Подписчик] Очередь брокера удалена.\n");
                    break;
                }

                perror("msgsnd unsubscribe");
                exit_code = EXIT_FAILURE;
                break;
            }

            printf("[Подписчик] Отписка от темы %s отправлена.\n",
                   topics[i]);
        }
    }

    printf("[Подписчик] Работа завершена.\n");

    return exit_code;
}
