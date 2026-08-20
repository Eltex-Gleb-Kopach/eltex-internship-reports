#ifndef SEMAPHORE_CONTROL_H
#define SEMAPHORE_CONTROL_H

#include <semaphore.h>

/* Создаёт семафор со значением 1. */
sem_t *create_semaphore(void);

/* Открывает именованный семафор, созданный производителем. */
sem_t *open_semaphore(void);

/* Захватывает семафор перед работой с разделяемой памятью. */
int lock_semaphore(sem_t *semaphore);

/* Освобождает семафор после работы с разделяемой памятью. */
int unlock_semaphore(sem_t *semaphore);

/* Закрывает ссылку текущего процесса на семафор. */
int close_semaphore(sem_t *semaphore);

/* Удаляет имя POSIX-семафора из системы. */
int remove_semaphore(void);

#endif