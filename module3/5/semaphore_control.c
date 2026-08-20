#include "semaphore_control.h"
#include "shared_data.h"

#include <errno.h>
#include <fcntl.h>
#include <semaphore.h>
#include <sys/stat.h>

/*
 * Создаёт именованный POSIX-семафор.
 *
 * O_CREAT — создать семафор.
 * O_EXCL — вернуть ошибку, если он уже существует.
 * 0600 — доступ на чтение и изменение только владельцу.
 * 1 — начальное значение: общая память свободна.
 */
sem_t *create_semaphore(void)
{
    return sem_open(SEMAPHORE_NAME,
                    O_CREAT | O_EXCL,
                    0600,
                    1);
}

/* Открывает существующий семафор по его имени. */
sem_t *open_semaphore(void)
{
    return sem_open(SEMAPHORE_NAME, 0);
}

/*
 * Захватывает семафор. Если ожидание было прервано
 * сигналом, повторяет sem_wait().
 */
int lock_semaphore(sem_t *semaphore)
{
    while (sem_wait(semaphore) == -1) {
        if (errno != EINTR) {
            return -1;
        }
    }

    return 0;
}

/* Освобождает семафор. */
int unlock_semaphore(sem_t *semaphore)
{
    return sem_post(semaphore);
}

/* Закрывает ссылку текущего процесса на семафор. */
int close_semaphore(sem_t *semaphore)
{
    return sem_close(semaphore);
}

/* Удаляет имя семафора из системы. */
int remove_semaphore(void)
{
    return sem_unlink(SEMAPHORE_NAME);
}