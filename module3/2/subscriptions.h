#ifndef SUBSCRIPTIONS_H
#define SUBSCRIPTIONS_H

#include <stddef.h>
#include <sys/types.h>

#include "protocol.h"

/* Одна подписка определённого процесса на одну тему. */
struct subscription {
    pid_t subscriber_pid;
    char topic[TOPIC_SIZE];
};

/* Динамический массив всех известных брокеру подписок. */
struct subscription_list {
    struct subscription *items;
    size_t count;
    size_t capacity;
};

/*
 * Добавляет подписку:
 *  1 — подписка добавлена;
 *  0 — такая подписка уже существует;
 * -1 — ошибка.
 */
int add_subscription(struct subscription_list *list,
                     pid_t subscriber_pid,
                     const char *topic);

/*
 * Удаляет подписку:
 *  1 — подписка удалена;
 *  0 — подписка не найдена;
 * -1 — ошибка.
 */
int remove_subscription(struct subscription_list *list,
                        pid_t subscriber_pid,
                        const char *topic);

/* Освобождает память списка подписок. */
void destroy_subscription_list(struct subscription_list *list);

#endif