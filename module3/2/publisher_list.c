#include "publisher_list.h"

#include <errno.h>
#include <stdlib.h>
#include <string.h>

int add_publisher(struct publisher_list *list,
                  pid_t publisher_pid)
{
    if (list == NULL || publisher_pid <= 0) {
        errno = EINVAL;
        return -1;
    }

    for (size_t i = 0; i < list->count; ++i) {
        if (list->items[i] == publisher_pid) {
            return 0;
        }
    }

    if (list->count == list->capacity) {
        size_t new_capacity =
            list->capacity == 0 ? 8 : list->capacity * 2;

        pid_t *new_items =
            realloc(list->items,
                    new_capacity * sizeof(*new_items));

        if (new_items == NULL) {
            return -1;
        }

        list->items = new_items;
        list->capacity = new_capacity;
    }

    list->items[list->count] = publisher_pid;
    ++list->count;

    return 1;
}

int remove_publisher(struct publisher_list *list,
                     pid_t publisher_pid)
{
    if (list == NULL || publisher_pid <= 0) {
        errno = EINVAL;
        return -1;
    }

    for (size_t i = 0; i < list->count; ++i) {
        if (list->items[i] == publisher_pid) {
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

void destroy_publisher_list(struct publisher_list *list)
{
    if (list == NULL) {
        return;
    }

    free(list->items);

    list->items = NULL;
    list->count = 0;
    list->capacity = 0;
}