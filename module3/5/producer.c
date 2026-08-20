#define _POSIX_C_SOURCE 200809L

#include "producer.h"
#include "shared_data.h"
#include "semaphore_control.h"
#include <time.h>
#include <unistd.h>

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>

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

/* Освобождает уже созданные POSIX-ресурсы после ошибки производителя. */
static void cleanup_producer_resources(void *shared_memory,
                                       sem_t *semaphore)
{
    if (shared_memory != MAP_FAILED) {
        (void)munmap(shared_memory,
                     SHARED_MEMORY_SIZE);
    }

    (void)shm_unlink(SHARED_MEMORY_NAME);

    if (semaphore != SEM_FAILED) {
        (void)close_semaphore(semaphore);
    }

    (void)remove_semaphore();
}

/*
 * Создаёт разделяемую память и семафор POSIX, добавляет
 * в разделяемую память блоки со случайными числами и
 * связывает эти блоки в односвязный список.
 */
int run_producer(void)
{
    printf("Режим: производитель.\n");

    /*
     * Создаём именованный POSIX-семафор со значением 1.
     * Указатель понадобится при каждом обращении к общей памяти.
     */
    sem_t *semaphore = create_semaphore();

    if (semaphore == SEM_FAILED) {
        if (errno == EEXIST) {
            fprintf(stderr,
                    "Ошибка: семафор уже существует. "
                    "Возможно, производитель уже запущен.\n");
        } else {
            perror("sem_open");
        }

        return EXIT_FAILURE;
    }

    /*
     * Создаём новый POSIX-объект разделяемой памяти.
     *
     * O_CREAT — создать объект.
     * O_EXCL — вернуть ошибку, если объект уже существует.
     * O_RDWR — разрешить чтение и запись.
     * 0600 — доступ разрешён только владельцу.
     */
    int shared_memory_fd =
        shm_open(SHARED_MEMORY_NAME,
                 O_CREAT | O_EXCL | O_RDWR,
                 0600);

    if (shared_memory_fd == -1) {
        if (errno == EEXIST) {
            fprintf(stderr,
                    "Ошибка: производитель уже запущен "
                    "или остался старый объект памяти.\n");
        } else {
            perror("shm_open");
        }

        close_semaphore(semaphore);
        remove_semaphore();

        return EXIT_FAILURE;
    }

    /*
     * Новый POSIX-объект изначально имеет размер 0.
     * Задаём ему необходимый размер разделяемой памяти.
     */
    if (ftruncate(shared_memory_fd,
                  (off_t)SHARED_MEMORY_SIZE) == -1) {
        perror("ftruncate");

        close(shared_memory_fd);
        shm_unlink(SHARED_MEMORY_NAME);
        close_semaphore(semaphore);
        remove_semaphore();

        return EXIT_FAILURE;
    }

    /*
     * Отображаем POSIX-объект в адресное пространство производителя.
     * MAP_SHARED делает изменения видимыми другим процессам.
     */
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
        shm_unlink(SHARED_MEMORY_NAME);
        close_semaphore(semaphore);
        remove_semaphore();

        return EXIT_FAILURE;
    }

    /*
     * После mmap() файловый дескриптор больше не нужен.
     * Само отображение продолжает работать до munmap().
     */
    if (close(shared_memory_fd) == -1) {
        perror("close shared memory");

        munmap(shared_memory, SHARED_MEMORY_SIZE);
        shm_unlink(SHARED_MEMORY_NAME);
        close_semaphore(semaphore);
        remove_semaphore();

        return EXIT_FAILURE;
    }

    /*
    * Инициализируем генератор случайных чисел.
    * PID помогает разным запускам получать разные последовательности.
    */
    srand((unsigned int)time(NULL)
        ^ (unsigned int)getpid());

    /*
    * Изначально захватываем семафор,
    * чтобы подготовить общий заголовок.
    */
    if (lock_semaphore(semaphore) == -1) {
        perror("sem_wait");

        cleanup_producer_resources(shared_memory,
                                   semaphore);

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
    header->active_consumer_count = 0;
    header->producer_finished = 0;

    /* Общий заголовок подготовлен. */
    if (unlock_semaphore(semaphore) == -1) {
        perror("sem_post");

        cleanup_producer_resources(shared_memory,
                                   semaphore);

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
        if (lock_semaphore(semaphore) == -1) {
            perror("sem_wait");

            cleanup_producer_resources(shared_memory,
                                       semaphore);

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

            if (unlock_semaphore(semaphore) == -1) {
                perror("sem_post");

                cleanup_producer_resources(shared_memory,
                                           semaphore);

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
        if (unlock_semaphore(semaphore) == -1) {
            perror("sem_post");

            cleanup_producer_resources(shared_memory,
                                       semaphore);

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
    printf("Имя памяти: %s.\n", SHARED_MEMORY_NAME);
    printf("Имя семафора: %s.\n", SEMAPHORE_NAME);
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
        * Захватываем семафор, чтобы проверить блоки
        * одновременно не изменял ни один потребитель.
        */
        if (lock_semaphore(semaphore) == -1) {
            perror("sem_wait");

            cleanup_producer_resources(shared_memory,
                                       semaphore);

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

        /* Число подключённых потребителей храним в общем заголовке. */
        size_t active_consumer_count =
            header->active_consumer_count;

        if (unlock_semaphore(semaphore) == -1) {
            perror("sem_post");

            cleanup_producer_resources(shared_memory,
                                       semaphore);

            return EXIT_FAILURE;
        }

        /* Все блоки обработаны и все потребители отключились. */
        if (all_blocks_processed
            && active_consumer_count == 0) {
            break;
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

    /* Удаляем отображение POSIX-памяти из адресного пространства. */
    if (munmap(shared_memory,
               SHARED_MEMORY_SIZE) == -1) {
        perror("munmap");

        shm_unlink(SHARED_MEMORY_NAME);
        close_semaphore(semaphore);
        remove_semaphore();

        return EXIT_FAILURE;
    }

    /* Удаляем имя POSIX-объекта разделяемой памяти. */
    if (shm_unlink(SHARED_MEMORY_NAME) == -1) {
        perror("shm_unlink");

        close_semaphore(semaphore);
        remove_semaphore();

        return EXIT_FAILURE;
    }

    printf("Разделяемая память удалена.\n");

    /* Закрываем ссылку производителя на POSIX-семафор. */
    if (close_semaphore(semaphore) == -1) {
        perror("sem_close");

        remove_semaphore();
        return EXIT_FAILURE;
    }

    /* Удаляем имя POSIX-семафора из системы. */
    if (remove_semaphore() == -1) {
        perror("sem_unlink");
        return EXIT_FAILURE;
    }

    printf("Семафор удалён.\n");

    return EXIT_SUCCESS;
}
