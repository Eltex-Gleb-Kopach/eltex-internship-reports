#define _POSIX_C_SOURCE 200809L

#include "producer.h"
#include "shared_data.h"
#include "semaphore_control.h"
#include <time.h>
#include <unistd.h>

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ipc.h>
#include <sys/shm.h>

/* В одном блоке будет от 5 до 15 случайных чисел. */
#define MIN_ELEMENT_COUNT 5
#define MAX_ELEMENT_COUNT 15

/* Случайные числа будут находиться в диапазоне от -100 до 100. */
#define MIN_RANDOM_VALUE (-100)
#define MAX_RANDOM_VALUE 100

/* Задержка между созданием блоков — 0,05 секунды. */
#define PRODUCER_DELAY_NANOSECONDS 50000000L

/* Задержка между проверками обработки блоков — 0,1 секунды. */
#define PRODUCER_WAIT_NANOSECONDS 100000000L

/*
 * Округляет смещение вверх, чтобы структура блока
 * начиналась с правильно выровненного адреса.
 */
static size_t align_up(size_t value,
                       size_t alignment)
{
    size_t remainder = value % alignment;

    if (remainder == 0) {
        return value;
    }

    return value + alignment - remainder;
}

/*
 * Создаёт общие ресурсы System V, периодически добавляет
 * в разделяемую память блоки со случайными числами и
 * связывает эти блоки в односвязный список.
 */
int run_producer(void)
{
    printf("Режим: производитель.\n");

    /*
     * Создаём один семафор со значением 1.
     * Его ID понадобится при каждом обращении к общей памяти.
     */
    int semaphore_id = create_semaphore();

    if (semaphore_id == -1) {
        if (errno == EEXIST) {
            fprintf(stderr,
                    "Ошибка: семафор уже существует. "
                    "Возможно, производитель уже запущен.\n");
        } else {
            perror("semget");
        }

        return EXIT_FAILURE;
    }

    /*
     * Создаём новый сегмент разделяемой памяти.
     *
     * IPC_CREAT — создать сегмент.
     * IPC_EXCL — вернуть ошибку, если сегмент уже существует.
     * 0600 — чтение и запись разрешены только владельцу.
     */
    int shared_memory_id =
        shmget(SHARED_MEMORY_KEY,
               SHARED_MEMORY_SIZE,
               IPC_CREAT | IPC_EXCL | 0600);

    if (shared_memory_id == -1) {
        if (errno == EEXIST) {
            fprintf(stderr,
                    "Ошибка: производитель уже запущен "
                    "или остался старый сегмент памяти.\n");
        } else {
            perror("shmget");
        }
        remove_semaphore(semaphore_id);
        return EXIT_FAILURE;
    }

    /*
     * Подключаем созданный сегмент к адресному
     * пространству процесса-производителя.
     */
    void *shared_memory =
        shmat(shared_memory_id, NULL, 0);

    if (shared_memory == (void *)-1) {
        perror("shmat");

        shmctl(shared_memory_id, IPC_RMID, NULL);
        remove_semaphore(semaphore_id);
        return EXIT_FAILURE;
    }

    /*
    * Инициализируем генератор случайных чисел.
    * PID помогает разным запускам получать разные последовательности.
    */
    srand((unsigned int)time(NULL)
        ^ (unsigned int)getpid());

    /*
    * Изначально закрываем семафор,
    * чтобы подготовить общий заголовок.
    */
    if (lock_semaphore(semaphore_id) == -1) {
        perror("semop lock");

        shmdt(shared_memory);
        shmctl(shared_memory_id, IPC_RMID, NULL);
        remove_semaphore(semaphore_id);

        return EXIT_FAILURE;
    }

    /*
     * Обнуляем все 4096 байт сегмента, чтобы в ещё не
     * использованной части не оставалось случайных данных.
     */
    memset(shared_memory, 0, SHARED_MEMORY_SIZE);

    /*
     * shared_memory имеет тип void *, то есть это просто адрес.
     * Первые байты по этому адресу считаем shared_header.
     */
    struct shared_header *header =
        (struct shared_header *)shared_memory;

    /*
     * Пока занят только shared_header, первого блока ещё нет,
     * а значение 0 сообщает, что генерация ещё продолжается.
     */
    header->used_size =
        sizeof(struct shared_header);
    header->first_block_offset = 0;
    header->producer_finished = 0;

    /* Общий заголовок подготовлен. */
    if (unlock_semaphore(semaphore_id) == -1) {
        perror("semop unlock");

        shmdt(shared_memory);
        shmctl(shared_memory_id, IPC_RMID, NULL);
        remove_semaphore(semaphore_id);

        return EXIT_FAILURE;
    }

    /* Счётчик нужен только для итогового сообщения производителя. */
    size_t block_count = 0;

    /*
    * Смещение последнего блока.
    * Оно понадобится, чтобы связать его с новым блоком.
    */
    size_t last_block_offset = 0;

    while (1) {
        /*
        * На каждой итерации производитель получает
        * исключительный доступ к общей памяти.
        */
        if (lock_semaphore(semaphore_id) == -1) {
            perror("semop lock");

            shmdt(shared_memory);
            shmctl(shared_memory_id, IPC_RMID, NULL);
            remove_semaphore(semaphore_id);

            return EXIT_FAILURE;
        }

        /*
         * used_size указывает конец занятых данных. align_up()
         * при необходимости сдвигает начало новой структуры
         * вперёд до подходящей для процессора границы.
         */
        size_t block_offset =
            align_up(header->used_size,
                    _Alignof(struct data_block));

        /* Самый маленький допустимый блок: заголовок и 5 чисел. */
        size_t minimum_block_size =
            sizeof(struct data_block)
            + MIN_ELEMENT_COUNT * sizeof(int);

        /*
         * Сначала проверяем само смещение, чтобы выражение
         * SHARED_MEMORY_SIZE - block_offset не вышло за границы размера.
         */
        int memory_exhausted =
            block_offset > SHARED_MEMORY_SIZE;

        /* Число свободных байтов от начала нового блока до конца. */
        size_t available_size = 0;

        if (!memory_exhausted) {
            available_size =
                SHARED_MEMORY_SIZE - block_offset;

            if (available_size < minimum_block_size) {
                memory_exhausted = 1;
            }
        }

        /*
        * Если новый блок уже не помещается,
        * сообщаем потребителям о завершении генерации.
        */
        if (memory_exhausted) {
            header->producer_finished = 1;

            if (unlock_semaphore(semaphore_id) == -1) {
                perror("semop unlock");

                shmdt(shared_memory);
                shmctl(shared_memory_id,
                    IPC_RMID,
                    NULL);
                remove_semaphore(semaphore_id);

                return EXIT_FAILURE;
            }

            break;
        }

        /*
        * Определяем максимально допустимое количество
        * чисел в новом блоке.
        */
        size_t maximum_fitting_count =
            (available_size
            - sizeof(struct data_block))
            / sizeof(int);

        if (maximum_fitting_count
            > MAX_ELEMENT_COUNT) {
            maximum_fitting_count =
                MAX_ELEMENT_COUNT;
        }

        /* Количество возможных значений для случайного размера. */
        size_t possible_count =
            maximum_fitting_count
            - MIN_ELEMENT_COUNT
            + 1;

        /* Случайный размер массива в допустимых границах. */
        size_t element_count =
            MIN_ELEMENT_COUNT
            + (size_t)rand() % possible_count;

        /* Полный размер: служебные поля блока плюс массив int. */
        size_t block_size =
            sizeof(struct data_block)
            + element_count * sizeof(int);

        /*
         * Превращаем рассчитанное место внутри общего сегмента
         * в указатель, через который можно заполнять data_block.
         */
        struct data_block *new_block =
            (struct data_block *)(
                (char *)shared_memory
                + block_offset);

        /* Ненулевой element_count означает: блок ещё не обработан. */
        new_block->element_count =
            element_count;

        /* Новый блок пока последний, поэтому ссылки дальше нет. */
        new_block->next_block_offset = 0;

        /* Заполняем переменную часть блока случайными числами. */
        for (size_t i = 0;
            i < element_count;
            ++i) {
            new_block->numbers[i] =
                MIN_RANDOM_VALUE
                + rand() % (MAX_RANDOM_VALUE
                            - MIN_RANDOM_VALUE
                            + 1);
        }

        if (last_block_offset == 0) {
            /*
            * Это первый блок списка.
            */
            header->first_block_offset =
                block_offset;
        } else {
            /*
            * Находим предыдущий последний блок
            * и связываем его с новым.
            */
            struct data_block *last_block =
                (struct data_block *)(
                    (char *)shared_memory
                    + last_block_offset);

            last_block->next_block_offset =
                block_offset;
        }

        /*
         * Запоминаем новый конец списка. На следующей итерации
         * именно этот блок будет связан со следующим.
         */
        last_block_offset = block_offset;

        /* Следующий блок будет размещаться после только что созданного. */
        header->used_size =
            block_offset + block_size;

        ++block_count;

        /*
        * Новый блок полностью записан,
        * поэтому разрешаем работу потребителям.
        */
        if (unlock_semaphore(semaphore_id) == -1) {
            perror("semop unlock");

            shmdt(shared_memory);
            shmctl(shared_memory_id, IPC_RMID, NULL);
            remove_semaphore(semaphore_id);

            return EXIT_FAILURE;
        }

        /*
        * По условию производитель периодически
        * создаёт новые наборы, поэтому делает паузу.
        */
        const struct timespec delay = {
            .tv_sec = 0,
            .tv_nsec =
                PRODUCER_DELAY_NANOSECONDS
        };

        (void)nanosleep(&delay, NULL);
    }

    printf("Разделяемая память создана.\n");
    printf("ID сегмента: %d.\n", shared_memory_id);
    printf("ID семафора: %d.\n", semaphore_id);
    printf("Размер сегмента: %zu байт.\n",
           (size_t)SHARED_MEMORY_SIZE);
    printf("Создано блоков: %zu.\n",
        block_count);
    printf("Занято памяти: %zu байт.\n",
        header->used_size);
    printf("Осталось свободно: %zu байт.\n",
        (size_t)SHARED_MEMORY_SIZE
        - header->used_size);
    printf("Генерация блоков завершена.\n");

    printf("Производитель ожидает обработки всех блоков.\n");

    while (1) {
        /*
        * Закрываем семафор, чтобы проверить блоки
        * одновременно не изменял ни один потребитель.
        */
        if (lock_semaphore(semaphore_id) == -1) {
            perror("semop lock");

            shmdt(shared_memory);
            shmctl(shared_memory_id, IPC_RMID, NULL);
            remove_semaphore(semaphore_id);

            return EXIT_FAILURE;
        }

        size_t current_offset =
            header->first_block_offset;

        int all_blocks_processed = 1;

        /*
        * Проходим по всей цепочке.
        * Ненулевой element_count означает,
        * что блок ещё не обработан.
        */
        while (current_offset != 0) {
            struct data_block *current_block =
                (struct data_block *)(
                    (char *)shared_memory
                    + current_offset);

            if (current_block->element_count != 0) {
                all_blocks_processed = 0;
                break;
            }

            current_offset =
                current_block->next_block_offset;
        }

        if (unlock_semaphore(semaphore_id) == -1) {
            perror("semop unlock");

            shmdt(shared_memory);
            shmctl(shared_memory_id, IPC_RMID, NULL);
            remove_semaphore(semaphore_id);

            return EXIT_FAILURE;
        }

        /*
        * Если все блоки обработаны, дополнительно проверяем,
        * отключились ли уже процессы-потребители.
        */
        if (all_blocks_processed) {
            struct shmid_ds segment_information;

            if (shmctl(shared_memory_id,
                    IPC_STAT,
                    &segment_information) == -1) {
                perror("shmctl IPC_STAT");

                shmdt(shared_memory);
                shmctl(shared_memory_id,
                    IPC_RMID,
                    NULL);
                remove_semaphore(semaphore_id);

                return EXIT_FAILURE;
            }

            /*
            * Значение 1 означает, что к сегменту подключён
            * только сам производитель.
            */
            if (segment_information.shm_nattch == 1) {
                break;
            }
        }

        /*
        * Ещё остались необработанные блоки или подключённые
        * потребители. Производитель засыпает и проверяет снова.
        */
        const struct timespec delay = {
            .tv_sec = 0,
            .tv_nsec =
                PRODUCER_WAIT_NANOSECONDS
        };

        (void)nanosleep(&delay, NULL);
    }

    printf("Все блоки обработаны, потребители отключились.\n");

    /* Отключаем сегмент от процесса. */
    if (shmdt(shared_memory) == -1) {
        perror("shmdt");

        shmctl(shared_memory_id, IPC_RMID, NULL);
        remove_semaphore(semaphore_id);

        return EXIT_FAILURE;
    }

    /* Помечаем сегмент разделяемой памяти на удаление. */
    if (shmctl(shared_memory_id,
            IPC_RMID,
            NULL) == -1) {
        perror("shmctl IPC_RMID");

        remove_semaphore(semaphore_id);

        return EXIT_FAILURE;
    }

    printf("Разделяемая память удалена.\n");

    /* Удаляем созданный производителем семафор. */
    if (remove_semaphore(semaphore_id) == -1) {
        perror("semctl IPC_RMID");
        return EXIT_FAILURE;
    }

    printf("Семафор удалён.\n");

    return EXIT_SUCCESS;
}
