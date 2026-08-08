#include "subscriptions.h"

#include <errno.h>
#include <stdlib.h>
#include <string.h>

int add_subscription(struct subscription_list *list,
                     pid_t subscriber_pid,
                     const char *topic)
{
    if (list == NULL || subscriber_pid <= 0 || topic == NULL) {
        errno = EINVAL;
        return -1;
    }

    size_t topic_length = strlen(topic);

    if (topic_length == 0 || topic_length >= TOPIC_SIZE) {
        errno = EINVAL;
        return -1;
    }

    /*
     * Проверяем, не подписан ли этот процесс
     * на указанную тему ранее.
     */
    for (size_t i = 0; i < list->count; ++i) {
        if (list->items[i].subscriber_pid == subscriber_pid
            && strcmp(list->items[i].topic, topic) == 0) {
            return 0;
        }
    }

    /*
     * Если массив заполнен, увеличиваем его вместимость.
     * Первый раз выделяется место для восьми подписок.
     */
    if (list->count == list->capacity) {
        size_t new_capacity =
            list->capacity == 0 ? 8 : list->capacity * 2;

        struct subscription *new_items =
            realloc(list->items,
                    new_capacity * sizeof(*new_items));

        if (new_items == NULL) {
            return -1;
        }

        list->items = new_items;
        list->capacity = new_capacity;
    }

    struct subscription *new_subscription =
        &list->items[list->count];

    new_subscription->subscriber_pid = subscriber_pid;

    memcpy(new_subscription->topic,
           topic,
           topic_length + 1);

    ++list->count;

    return 1;
}

int remove_subscription(struct subscription_list *list,
                        pid_t subscriber_pid,
                        const char *topic)
{
    if (list == NULL || subscriber_pid <= 0 || topic == NULL) {
        errno = EINVAL;
        return -1;
    }

    for (size_t i = 0; i < list->count; ++i) {
        if (list->items[i].subscriber_pid == subscriber_pid
            && strcmp(list->items[i].topic, topic) == 0) {
            /*
             * Сдвигаем оставшиеся элементы на место
             * удалённой подписки.
             */
            memmove(&list->items[i],
                    &list->items[i + 1],
                    (list->count - i - 1)
                        * sizeof(list->items[i]));

            --list->count;
            return 1;
        }
    }

    return 0;
}

void destroy_subscription_list(struct subscription_list *list)
{
    if (list == NULL) {
        return;
    }

    free(list->items);

    list->items = NULL;
    list->count = 0;
    list->capacity = 0;
}