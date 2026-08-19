#ifndef SEMAPHORE_CONTROL_H
#define SEMAPHORE_CONTROL_H

/* Создаёт семафор со значением 1. */
int create_semaphore(void);

/* Находит уже существующий семафор. */
int open_semaphore(void);

/* Захватывает семафор перед работой с разделяемой памятью. */
int lock_semaphore(int semaphore_id);

/* Освобождает семафор после работы с разделяемой памятью. */
int unlock_semaphore(int semaphore_id);

/* Удаляет семафор из ядра. */
int remove_semaphore(int semaphore_id);

#endif
