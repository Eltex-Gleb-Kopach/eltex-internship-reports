#include "semaphore_control.h"
#include "shared_data.h"

#include <errno.h>
#include <sys/ipc.h>
#include <sys/sem.h>

/*
 * В Linux union semun обычно должен определить
 * пользователь программы.
 */
union semun {
    int val;
    struct semid_ds *buf;
    unsigned short *array;
};

/* Создаёт один новый семафор и устанавливает значение 1. */
int create_semaphore(void)
{
    int semaphore_id =
        semget(SEMAPHORE_KEY,
               1,
               IPC_CREAT | IPC_EXCL | 0600);

    if (semaphore_id == -1) {
        return -1;
    }

    union semun argument = {
        .val = 1
    };

    if (semctl(semaphore_id,
               0,
               SETVAL,
               argument) == -1) {
        int saved_errno = errno;

        semctl(semaphore_id, 0, IPC_RMID);

        errno = saved_errno;
        return -1;
    }

    return semaphore_id;
}

/* Находит семафор, ранее созданный производителем. */
int open_semaphore(void)
{
    return semget(SEMAPHORE_KEY, 1, 0600);
}

/*
 * Выполняет операцию над значением семафора.
 * -1 захватывает семафор, +1 освобождает.
 */
static int change_semaphore(int semaphore_id,
                            short change)
{
    struct sembuf operation = {
        .sem_num = 0,
        .sem_op = change,
        .sem_flg = SEM_UNDO
    };

    while (semop(semaphore_id,
                 &operation,
                 1) == -1) {
        if (errno != EINTR) {
            return -1;
        }
    }

    return 0;
}

int lock_semaphore(int semaphore_id)
{
    return change_semaphore(semaphore_id, -1);
}

int unlock_semaphore(int semaphore_id)
{
    return change_semaphore(semaphore_id, 1);
}

int remove_semaphore(int semaphore_id)
{
    return semctl(semaphore_id,
                  0,
                  IPC_RMID);
}
