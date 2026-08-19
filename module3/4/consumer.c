#define _POSIX_C_SOURCE 200809L

#include "consumer.h"
#include "semaphore_control.h"
#include "shared_data.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <time.h>

/* Задержка потребителя после обработки одного блока — 0,1 секунды. */
#define CONSUMER_DELAY_NANOSECONDS 100000000L

/*
 * Подключается к ресурсам производителя и периодически
 * обрабатывает по одному ещё не обработанному блоку.
 */
int run_consumer(void)
{
    printf("Режим: потребитель.\n");

    /*
     * Находим сегмент, ранее созданный производителем.
     * Потребитель сам разделяемую память не создаёт.
     */
    int shared_memory_id =
        shmget(SHARED_MEMORY_KEY,
               SHARED_MEMORY_SIZE,
               0600);

    if (shared_memory_id == -1) {
        if (errno == ENOENT) {
            fprintf(stderr,
                    "Ошибка: производитель ещё не запущен.\n");
        } else {
            perror("shmget");
        }

        return EXIT_FAILURE;
    }

    /* Находим семафор, созданный производителем. */
    int semaphore_id = open_semaphore();

    if (semaphore_id == -1) {
        if (errno == ENOENT) {
            fprintf(stderr,
                    "Ошибка: семафор производителя не найден.\n");
        } else {
            perror("semget");
        }

        return EXIT_FAILURE;
    }

    /* Подключаем разделяемую память к потребителю. */
    void *shared_memory =
        shmat(shared_memory_id, NULL, 0);

    if (shared_memory == (void *)-1) {
        perror("shmat");
        return EXIT_FAILURE;
    }

    /*
     * Начало памяти рассматриваем
     * как структуру общего заголовка.
     */
    struct shared_header *header =
        (struct shared_header *)shared_memory;

    printf("Потребитель подключился к памяти.\n");
    printf("ID сегмента: %d.\n", shared_memory_id);
    printf("ID семафора: %d.\n", semaphore_id);

    /* Локальный счётчик работы именно этого экземпляра потребителя. */
    size_t processed_block_count = 0;

    while (1) {
        /*
        * Закрываем семафор перед поиском блока.
        * Другие потребители в это время ждут.
        */
        if (lock_semaphore(semaphore_id) == -1) {
            perror("semop lock");

            shmdt(shared_memory);
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

        if (unlock_semaphore(semaphore_id) == -1) {
            perror("semop unlock");

            shmdt(shared_memory);
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
     * Потребитель отключается от памяти,
     * но не удаляет её и не удаляет семафор.
     */
    if (shmdt(shared_memory) == -1) {
        perror("shmdt");
        return EXIT_FAILURE;
    }

    printf("Потребитель отключился от памяти.\n");

    return EXIT_SUCCESS;
}
