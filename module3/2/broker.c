#define _POSIX_C_SOURCE 200809L

#include "broker.h"
#include "publisher_list.h"

#include <errno.h>
#include <limits.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "message_queue.h"
#include "protocol.h"
#include "subscriptions.h"
#include "signal_control.h"

#define SHUTDOWN_TIMEOUT_SECONDS 3
#define SHUTDOWN_POLL_NANOSECONDS 100000000L

/*
 * Разбирает subscribe,<pid>,<topic>
 * или unsubscribe,<pid>,<topic>.
 */
static int parse_subscription_message(const char *message_text,
                                      const char *command,
                                      pid_t *subscriber_pid,
                                      char topic[TOPIC_SIZE])
{
    size_t command_length = strlen(command);

    if (strncmp(message_text,
                command,
                command_length) != 0
        || message_text[command_length] != ',') {
        return -1;
    }

    const char *pid_text =
        message_text + command_length + 1;

    char *pid_end = NULL;

    errno = 0;

    long pid_value = strtol(pid_text,
                            &pid_end,
                            10);

    if (errno != 0
        || pid_end == pid_text
        || *pid_end != ','
        || pid_value <= 0
        || pid_value > INT_MAX) {
        return -1;
    }

    const char *topic_text = pid_end + 1;
    size_t topic_length = strlen(topic_text);

    if (topic_length == 0
        || topic_length >= TOPIC_SIZE
        || strchr(topic_text, ',') != NULL) {
        return -1;
    }

    *subscriber_pid = (pid_t)pid_value;

    memcpy(topic,
           topic_text,
           topic_length + 1);

    return 0;
}

/* Разбирает команду command,<pid>. */
static int parse_publisher_message(const char *message_text,
                                   const char *command,
                                   pid_t *publisher_pid)
{
    size_t command_length = strlen(command);

    if (strncmp(message_text,
                command,
                command_length) != 0
        || message_text[command_length] != ',') {
        return -1;
    }

    const char *pid_text =
        message_text + command_length + 1;

    char *pid_end = NULL;

    errno = 0;

    long pid_value = strtol(pid_text,
                            &pid_end,
                            10);

    if (errno != 0
        || pid_end == pid_text
        || *pid_end != '\0'
        || pid_value <= 0
        || pid_value > INT_MAX) {
        return -1;
    }

    *publisher_pid = (pid_t)pid_value;

    return 0;
}

/* Разбирает send,<pid>,<topic>,<payload>. */
static int parse_send_message(const char *message_text,
                              pid_t *publisher_pid,
                              char topic[TOPIC_SIZE],
                              const char **payload)
{
    const char prefix[] = "send,";

    if (strncmp(message_text,
                prefix,
                sizeof(prefix) - 1) != 0) {
        return -1;
    }

    const char *pid_text =
        message_text + sizeof(prefix) - 1;

    char *pid_end = NULL;

    errno = 0;

    long pid_value = strtol(pid_text,
                            &pid_end,
                            10);

    if (errno != 0
        || pid_end == pid_text
        || *pid_end != ','
        || pid_value <= 0
        || pid_value > INT_MAX) {
        return -1;
    }

    const char *topic_text = pid_end + 1;
    const char *topic_end = strchr(topic_text, ',');

    if (topic_end == NULL) {
        return -1;
    }

    size_t topic_length =
        (size_t)(topic_end - topic_text);

    const char *payload_text = topic_end + 1;
    size_t payload_length = strlen(payload_text);

    if (topic_length == 0
        || topic_length >= TOPIC_SIZE
        || payload_length == 0
        || payload_length >= PAYLOAD_SIZE) {
        return -1;
    }

    *publisher_pid = (pid_t)pid_value;

    memcpy(topic,
           topic_text,
           topic_length);

    topic[topic_length] = '\0';
    *payload = payload_text;

    return 0;
}

/* Отправляет SIGINT всем зарегистрированным издателям. */
static void notify_publishers(
    const struct publisher_list *publishers)
{
    for (size_t i = 0; i < publishers->count; ++i) {
        pid_t publisher_pid = publishers->items[i];

        if (kill(publisher_pid, SIGINT) == -1) {
            if (errno != ESRCH) {
                perror("kill publisher");
            }

            continue;
        }

        printf("[Брокер] SIGINT отправлен издателю PID=%ld.\n",
               (long)publisher_pid);
    }
}

/* Отправляет SIGINT каждому подписчику только один раз. */
static void notify_subscribers(
    const struct subscription_list *subscriptions)
{
    for (size_t i = 0; i < subscriptions->count; ++i) {
        pid_t subscriber_pid =
            subscriptions->items[i].subscriber_pid;

        int already_notified = 0;

        for (size_t j = 0; j < i; ++j) {
            if (subscriptions->items[j].subscriber_pid
                == subscriber_pid) {
                already_notified = 1;
                break;
            }
        }

        if (already_notified) {
            continue;
        }

        if (kill(subscriber_pid, SIGINT) == -1) {
            if (errno != ESRCH) {
                perror("kill subscriber");
            }

            continue;
        }

        printf("[Брокер] SIGINT отправлен подписчику PID=%ld.\n",
               (long)subscriber_pid);
    }
}

/* Проверяет, истёк ли таймаут ожидания участников. */
static int shutdown_timeout_expired(
    const struct timespec *started_at)
{
    struct timespec current_time;

    if (clock_gettime(CLOCK_MONOTONIC,
                      &current_time) == -1) {
        return -1;
    }

    time_t elapsed_seconds =
        current_time.tv_sec - started_at->tv_sec;

    return elapsed_seconds >= SHUTDOWN_TIMEOUT_SECONDS;
}

/*
 * Принимает сообщения о завершении участников,
 * пока списки не опустеют или не закончится таймаут.
 */
static int wait_for_participants(
    int queue_id,
    struct publisher_list *publishers,
    struct subscription_list *subscriptions)
{
    struct timespec started_at;

    if (clock_gettime(CLOCK_MONOTONIC,
                      &started_at) == -1) {
        return -1;
    }

    while (1) {
        size_t queued_message_count;

        if (get_message_queue_count(
                queue_id,
                &queued_message_count) == -1) {
            return -1;
        }

        /*
         * Удалять очередь можно только после завершения
         * участников и чтения всех оставшихся сообщений.
         */
        if (publishers->count == 0
            && subscriptions->count == 0
            && queued_message_count == 0) {
            return 1;
        }

        int timeout_result =
            shutdown_timeout_expired(&started_at);

        if (timeout_result == -1) {
            return -1;
        }

        if (timeout_result == 1) {
            return 0;
        }

        struct queue_message message;

        ssize_t bytes_received =
            receive_queue_message_nowait(
                queue_id,
                BROKER_MESSAGE_TYPE,
                &message);

        if (bytes_received == -1) {
            if (errno != ENOMSG) {
                return -1;
            }

            const struct timespec delay = {
                .tv_sec = 0,
                .tv_nsec = SHUTDOWN_POLL_NANOSECONDS
            };

            (void)nanosleep(&delay, NULL);
            continue;
        }

        pid_t process_pid;

        if (parse_publisher_message(
                message.text,
                "publisher_unregister",
                &process_pid) == 0) {
            (void)remove_publisher(publishers,
                                   process_pid);

            printf("[Брокер] Издатель PID=%ld подтвердил завершение.\n",
                   (long)process_pid);
            continue;
        }

        char topic[TOPIC_SIZE];

        if (parse_subscription_message(
                message.text,
                "unsubscribe",
                &process_pid,
                topic) == 0) {
            (void)remove_subscription(subscriptions,
                                      process_pid,
                                      topic);

            printf("[Брокер] Подписчик PID=%ld отписался от %s.\n",
                   (long)process_pid,
                   topic);
            continue;
        }

        printf("[Брокер] При завершении сообщение отброшено: %s\n",
               message.text);
    }
}

int run_broker(void)
{
    int queue_id = create_message_queue();

    if (queue_id == -1) {
        if (errno == EEXIST) {
            fprintf(stderr,
                    "Ошибка: другой брокер уже работает.\n");
        } else {
            perror("msgget");
        }

        return EXIT_FAILURE;
    }
    
    /* Настраиваем корректное завершение брокера по Ctrl+C. */
    if (install_stop_signal_handler() == -1) {
        perror("sigaction");
        remove_message_queue(queue_id);
        return EXIT_FAILURE;
    }

    /*
     * Сначала список пустой:
     * items = NULL, count = 0, capacity = 0.
     */
    struct subscription_list subscriptions = {0};

    /* Список работающих издателей первоначально пуст. */
    struct publisher_list publishers = {0};

    printf("Режим: брокер сообщений.\n");
    printf("Очередь создана, ID=%d.\n", queue_id);
    printf("Брокер ожидает сообщения. Для завершения нажмите Ctrl+C.\n");

    int exit_code = EXIT_SUCCESS;

    while (!is_stop_requested()) {
        struct queue_message message;

        /*Пытаемся получить сообщение для брокера без блокировки.
         * Если очередь пуста, немного ждём; при другой ошибке завершаем работу.*/
        ssize_t bytes_received =
            receive_queue_message_nowait(
                queue_id,
                BROKER_MESSAGE_TYPE,
                &message);

        if (bytes_received == -1) {
            if (errno == EINTR) {
                continue;
            }

            if (errno == ENOMSG) {
                const struct timespec delay = {
                    .tv_sec = 0,
                    .tv_nsec = SHUTDOWN_POLL_NANOSECONDS
                };

                (void)nanosleep(&delay, NULL);
                continue;
            }

            perror("msgrcv");
            exit_code = EXIT_FAILURE;
            break;
        }
        
        pid_t process_pid;
        char topic[TOPIC_SIZE];

        /* Получаем PID из команды регистрации и добавляем издателя в список. */
        if (parse_publisher_message(message.text,
                                    "publisher_register",
                                    &process_pid) == 0) {
            int add_result =
                add_publisher(&publishers,
                              process_pid);

            if (add_result == -1) {
                perror("add_publisher");
                exit_code = EXIT_FAILURE;
                break;
            }

            if (add_result == 1) {
                printf("[Брокер] Зарегистрирован издатель PID=%ld.\n",
                       (long)process_pid);
            }

            continue;
        }

        /* Удаляем завершившийся процесс-издатель из списка. */
        if (parse_publisher_message(message.text,
                                    "publisher_unregister",
                                    &process_pid) == 0) {
            int remove_result =
                remove_publisher(&publishers,
                                 process_pid);

            if (remove_result == -1) {
                perror("remove_publisher");
                exit_code = EXIT_FAILURE;
                break;
            }

            if (remove_result == 1) {
                printf("[Брокер] Издатель PID=%ld завершился.\n",
                       (long)process_pid);
            }

            continue;
        }

        /* Обрабатываем команду подписки. */
        if (parse_subscription_message(message.text,
                                    "subscribe",
                                    &process_pid,
                                    topic) == 0) {
            int add_result =
                add_subscription(&subscriptions,
                                process_pid,
                                topic);

            if (add_result == -1) {
                perror("add_subscription");
                exit_code = EXIT_FAILURE;
                break;
            }

            if (add_result == 0) {
                printf("[Брокер] PID=%ld уже подписан на тему %s.\n",
                    (long)process_pid,
                    topic);
            } else {
                printf("[Брокер] Добавлена подписка: PID=%ld, тема=%s.\n",
                    (long)process_pid,
                    topic);
            }

            continue;
        }

        /* Обрабатываем команду отписки. */
        if (parse_subscription_message(message.text,
                                    "unsubscribe",
                                    &process_pid,
                                    topic) == 0) {
            int remove_result =
                remove_subscription(&subscriptions,
                                    process_pid,
                                    topic);

            if (remove_result == -1) {
                perror("remove_subscription");
                exit_code = EXIT_FAILURE;
                break;
            }

            if (remove_result == 1) {
                printf("[Брокер] Удалена подписка: PID=%ld, тема=%s.\n",
                    (long)process_pid,
                    topic);
            } else {
                printf("[Брокер] Подписка не найдена: PID=%ld, тема=%s.\n",
                    (long)process_pid,
                    topic);
            }

            continue;
        }

        /* Обрабатываем публикацию издателя. */
        const char *payload;

        if (parse_send_message(message.text,
                            &process_pid,
                            topic,
                            &payload) == 0) {
            size_t delivered_count = 0;

            for (size_t i = 0;
                i < subscriptions.count;
                ++i) {
                struct subscription *subscription =
                    &subscriptions.items[i];

                if (strcmp(subscription->topic, topic) != 0) {
                    continue;
                }

                /*
                * PID подписчика используется как тип сообщения,
                * чтобы публикацию получил именно этот процесс.
                */
                if (send_queue_message(
                        queue_id,
                        (long)subscription->subscriber_pid,
                        message.text) == -1) {
                    perror("msgsnd publication");
                    exit_code = EXIT_FAILURE;
                    break;
                }

                ++delivered_count;
            }

            if (exit_code == EXIT_FAILURE) {
                break;
            }

            printf("[Брокер] Публикация PID=%ld, тема=%s, текст=%s.\n",
                (long)process_pid,
                topic,
                payload);

            printf("[Брокер] Доставлено подписчикам: %zu.\n",
                delivered_count);

            continue;
        }

        fprintf(stderr,
                "[Брокер] Получено неизвестное сообщение: %s\n",
                message.text);
    }

    printf("[Брокер] Начато завершение участников.\n");

    notify_publishers(&publishers);
    notify_subscribers(&subscriptions);

    int wait_result =
        wait_for_participants(queue_id,
                              &publishers,
                              &subscriptions);

    if (wait_result == -1) {
        perror("wait_for_participants");
        exit_code = EXIT_FAILURE;
    } else if (wait_result == 0) {
        printf("[Брокер] Таймаут ожидания участников истёк.\n");
    } else {
        printf("[Брокер] Все участники подтвердили завершение.\n");
    }

    /* Освобождаем динамические списки участников. */
    destroy_publisher_list(&publishers);
    destroy_subscription_list(&subscriptions);

    if (remove_message_queue(queue_id) == -1) {
        perror("msgctl IPC_RMID");
        return EXIT_FAILURE;
    }

    printf("\nОчередь удалена. Брокер завершён.\n");

    return exit_code;
}
