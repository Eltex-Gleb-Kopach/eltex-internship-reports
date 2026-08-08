#ifndef PUBLISHER_LIST_H
#define PUBLISHER_LIST_H

#include <stddef.h>
#include <sys/types.h>

/* Динамический список PID работающих издателей. */
struct publisher_list {
    pid_t *items;
    size_t count;
    size_t capacity;
};

/*
 * Добавляет PID:
 *  1 — добавлен;
 *  0 — уже существовал;
 * -1 — ошибка.
 */
int add_publisher(struct publisher_list *list,
                  pid_t publisher_pid);

/*
 * Удаляет PID:
 *  1 — удалён;
 *  0 — не найден;
 * -1 — ошибка.
 */
int remove_publisher(struct publisher_list *list,
                     pid_t publisher_pid);

/* Освобождает память списка издателей. */
void destroy_publisher_list(struct publisher_list *list);

#endif
