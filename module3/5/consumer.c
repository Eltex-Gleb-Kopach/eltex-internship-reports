#define _POSIX_C_SOURCE 200809L

#include "consumer.h"
#include "semaphore_control.h"
#include "shared_data.h"

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

/* Задержка потребителя после обработки одного блока — 0,1 секунды. */
#define CONSUMER_DELAY_NANOSECONDS 100000000L

/* Уменьшает общий счётчик перед отключением потребителя. */
static int unregister_consumer(struct shared_header *header,
                               sem_t *semaphore)
{
    if (lock_semaphore(semaphore) == -1) {
        return -1;
    }

    if (header->active_consumer_count == 0) {
        errno = EINVAL;

        (void)unlock_semaphore(semaphore);
        return -1;
    }

    --header->active_consumer_count;

    return unlock_semaphore(semaphore);
}

/*
 * Подключается к ресурсам производителя и периодически
 * обрабатывает по одному ещё не обработанному блоку.
 */
int run_consumer(void)
{
    printf("Режим: потребитель.\n");

    /* Открываем POSIX-память, ранее созданную производителем. */
    int shared_memory_fd =
        shm_open(SHARED_MEMORY_NAME,
                 O_RDWR,
                 0);

    if (shared_memory_fd == -1) {
        if (errno == ENOENT) {
            fprintf(stderr,
                    "Ошибка: производитель ещё не запущен.\n");
        } else {
            perror("shm_open");
        }

        return EXIT_FAILURE;
    }

    /* Проверяем, что производитель задал ожидаемый размер объекта. */
    struct stat memory_information;

    if (fstat(shared_memory_fd,
              &memory_information) == -1) {
        perror("fstat");

        close(shared_memory_fd);
        return EXIT_FAILURE;
    }

    if (memory_information.st_size
        != (off_t)SHARED_MEMORY_SIZE) {
        fprintf(stderr,
                "Ошибка: неправильный размер разделяемой памяти.\n");

        close(shared_memory_fd);
        return EXIT_FAILURE;
    }

    /* Открываем именованный POSIX-семафор производителя. */
    sem_t *semaphore = open_semaphore();

    if (semaphore == SEM_FAILED) {
        if (errno == ENOENT) {
            fprintf(stderr,
                    "Ошибка: семафор производителя не найден.\n");
        } else {
            perror("sem_open");
        }

        close(shared_memory_fd);
        return EXIT_FAILURE;
    }

    /* Отображаем POSIX-память в адресное пространство потребителя. */
    void *shared_memory =
        mmap(NULL,
             SHARED_MEMORY_SIZE,
             PROT_READ | PROT_WRITE,
             MAP_SHARED,
             shared_memory_fd,
             0);

    if (shared_memory == MAP_FAILED) {
        perror("mmap");

        close(shared_memory_fd);
        close_semaphore(semaphore);
        return EXIT_FAILURE;
    }

    /* После mmap() дескриптор памяти потребителю больше не нужен. */
    if (close(shared_memory_fd) == -1) {
        perror("close shared memory");

        munmap(shared_memory,
               SHARED_MEMORY_SIZE);
        close_semaphore(semaphore);
        return EXIT_FAILURE;
    }

    /*
     * Начало памяти рассматриваем
     * как структуру общего заголовка.
     */
    struct shared_header *header =
        (struct shared_header *)shared_memory;

    /* Регистрируем подключение потребителя в общем заголовке. */
    if (lock_semaphore(semaphore) == -1) {
        perror("sem_wait");

        munmap(shared_memory,
               SHARED_MEMORY_SIZE);
        close_semaphore(semaphore);
        return EXIT_FAILURE;
    }

    ++header->active_consumer_count;

    if (unlock_semaphore(semaphore) == -1) {
        perror("sem_post");

        munmap(shared_memory,
               SHARED_MEMORY_SIZE);
        close_semaphore(semaphore);
        return EXIT_FAILURE;
    }

    printf("Потребитель подключился к памяти.\n");
    printf("Имя памяти: %s.\n", SHARED_MEMORY_NAME);
    printf("Имя семафора: %s.\n", SEMAPHORE_NAME);

    /* Локальный счётчик работы именно этого экземпляра потребителя. */
    size_t processed_block_count = 0;

    while (1) {
        /*
        * Закрываем семафор перед поиском блока.
        * Другие потребители в это время ждут.
        */
        if (lock_semaphore(semaphore) == -1) {
            perror("sem_wait");

            munmap(shared_memory,
                   SHARED_MEMORY_SIZE);
            close_semaphore(semaphore);
            return EXIT_FAILURE;
        }

        /*
         * Каждую проверку начинаем с первого блока списка.
         * Обработанные блоки будут пропущены внутри цикла.
         */
        size_t current_offset =
            header->first_block_offset;

        /*
         * Эти переменные являются локальными: в них сохраняется
         * результат текущей итерации для вывода после unlock.
         */
        int block_found = 0;
        size_t element_count = 0;
        size_t processed_block_offset = 0;
        int minimum = 0;
        int maximum = 0;

        /*
        * Проходим по цепочке до первого
        * необработанного блока.
        */
        while (current_offset != 0) {
            /*
             * Смещение хранится относительно начала сегмента.
             * Прибавив его, получаем адрес текущего data_block.
             */
            struct data_block *current_block =
                (struct data_block *)(
                    (char *)shared_memory
                    + current_offset);

            if (current_block->element_count != 0) {
                /* Сохраняем размер до того, как пометим блок нулём. */
                element_count =
                    current_block->element_count;

                /* Первое число даёт начальные значения min и max. */
                minimum =
                    current_block->numbers[0];
                maximum =
                    current_block->numbers[0];

                for (size_t i = 1;
                    i < element_count;
                    ++i) {
                    if (current_block->numbers[i]
                        < minimum) {
                        minimum =
                            current_block->numbers[i];
                    }

                    if (current_block->numbers[i]
                        > maximum) {
                        maximum =
                            current_block->numbers[i];
                    }
                }

                /*
                * Запоминаем смещение для вывода
                * и помечаем блок обработанным.
                */
                processed_block_offset =
                    current_offset;

                /*
                 * Ноль является общей отметкой для всех процессов:
                 * этот блок уже обработан и повторно брать его нельзя.
                 */
                current_block->element_count = 0;
                block_found = 1;

                break;
            }

            /*
            * Текущий блок уже обработан.
            * Переходим к следующему по его смещению.
            */
            current_offset =
                current_block->next_block_offset;
        }

        /*
        * Значение понадобится после освобождения семафора,
        * поэтому сохраняем его в локальную переменную.
        */
        int producer_finished =
            header->producer_finished;

        if (unlock_semaphore(semaphore) == -1) {
            perror("sem_post");

            munmap(shared_memory,
                   SHARED_MEMORY_SIZE);
            close_semaphore(semaphore);
            return EXIT_FAILURE;
        }

        if (block_found) {
            /* Семафор уже открыт; печать не задерживает другие процессы. */
            ++processed_block_count;

            printf("Обработан блок со смещением %zu: "
                "элементов=%zu, min=%d, max=%d.\n",
                processed_block_offset,
                element_count,
                minimum,
                maximum);

            /*
            * По условию после обработки одного набора
            * потребитель ненадолго засыпает.
            */
            const struct timespec delay = {
                .tv_sec = 0,
                .tv_nsec =
                    CONSUMER_DELAY_NANOSECONDS
            };

            (void)nanosleep(&delay, NULL);

            continue;
        }

        /*
        * Необработанных блоков нет и производитель
        * сообщил, что новые блоки больше не появятся.
        */
        if (producer_finished) {
            /* Нет работы и новых блоков уже не будет. */
            break;
        }

        /*
        * Если производитель ещё работает,
        * ждём и затем снова проверяем список.
        */
        const struct timespec delay = {
            .tv_sec = 0,
            .tv_nsec =
                CONSUMER_DELAY_NANOSECONDS
        };

        (void)nanosleep(&delay, NULL);
    }

    printf("Все доступные блоки обработаны.\n");
    printf("Этот потребитель обработал блоков: %zu.\n",
        processed_block_count);

    /*
     * Сообщаем производителю, что этот потребитель
     * больше не использует общую память.
     */
    if (unregister_consumer(header,
                            semaphore) == -1) {
        perror("unregister consumer");

        munmap(shared_memory,
               SHARED_MEMORY_SIZE);
        close_semaphore(semaphore);
        return EXIT_FAILURE;
    }

    /* Удаляем отображение памяти из адресного пространства. */
    if (munmap(shared_memory,
               SHARED_MEMORY_SIZE) == -1) {
        perror("munmap");

        close_semaphore(semaphore);
        return EXIT_FAILURE;
    }

    printf("Потребитель отключился от памяти.\n");

    /* Закрываем ссылку потребителя на именованный семафор. */
    if (close_semaphore(semaphore) == -1) {
        perror("sem_close");
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
