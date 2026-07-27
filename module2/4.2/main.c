#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include "priority_queue.h"

static void print_message(const Message *message, unsigned int priority);

static void print_queue(const PriorityQueue *queue);

static int generate_messages(PriorityQueue *queue, unsigned int *next_id, int requested_count);

int main(void){
    /* Оба указателя NULL обозначают пустую очередь. */
    PriorityQueue queue = {
        .front = NULL,
        .rear = NULL
    };

    /* ID, который получит следующее сгенерированное сообщение. */
    unsigned int next_id = 1;
    int menu = 0;

    srand((unsigned int)time(NULL));

    while (1) {
        printf("\nМеню очереди с приоритетами:\n");
        printf("1. Сгенерировать сообщения\n");
        printf("2. Показать очередь\n");
        printf("3. Извлечь первое сообщение\n");
        printf("4. Извлечь сообщение с указанным приоритетом\n");
        printf("5. Извлечь сообщение с приоритетом не ниже заданного\n");
        printf("0. Выход\n");
        printf("Выберите действие: ");

        if (scanf("%d", &menu) != 1) {
            printf("Ошибка ввода\n");

            /* Перед досрочным завершением освобождаем все узлы. */
            clear_queue(&queue);
            return 1;
        }

        switch (menu) {
            case 1: {
                int requested_count = 0;

                printf("Введите количество сообщений: ");

                if (scanf("%d", &requested_count) != 1 ||
                    requested_count <= 0) {

                    printf("Некорректное количество сообщений\n");
                    clear_queue(&queue);
                    return 1;
                }

                int generated_count =
                    generate_messages(
                        &queue,
                        &next_id,
                        requested_count
                    );

                printf(
                    "Сгенерировано сообщений: %d\n",
                    generated_count
                );

                break;
            }
            case 2:
                print_queue(&queue);
                break;
            case 3: {
                Message result_message = {0};
                unsigned int result_priority = 0;

                if (!dequeue_first(
                        &queue,
                        &result_message,
                        &result_priority)) {

                    printf("Очередь пуста\n");
                    break;
                }

                printf("Извлечённое сообщение:\n");

                print_message(
                    &result_message,
                    result_priority
                );

                break;
            }
            case 4: {
                unsigned int requested_priority = 0;
                Message result_message = {0};
                unsigned int result_priority = 0;

                printf(
                    "Введите точный приоритет (%u-%u): ",
                    MIN_PRIORITY,
                    MAX_PRIORITY
                );

                if (scanf(
                        "%u",
                        &requested_priority) != 1) {

                    printf("Ошибка ввода\n");
                    clear_queue(&queue);
                    return 1;
                }

                if (!priority_is_valid(
                        requested_priority)) {

                    printf("Некорректный приоритет\n");
                    break;
                }

                if (!dequeue_by_priority(
                        &queue,
                        requested_priority,
                        &result_message,
                        &result_priority)) {

                    printf(
                        "Сообщение с приоритетом %u "
                        "не найдено\n",
                        requested_priority
                    );

                    break;
                }

                printf("Извлечённое сообщение:\n");

                print_message(
                    &result_message,
                    result_priority
                );

                break;
            }

            case 5: {
                unsigned int requested_priority = 0;
                Message result_message = {0};
                unsigned int result_priority = 0;

                printf(
                    "Введите порог приоритета (%u-%u): ",
                    MIN_PRIORITY,
                    MAX_PRIORITY
                );

                if (scanf(
                        "%u",
                        &requested_priority) != 1) {

                    printf("Ошибка ввода\n");
                    clear_queue(&queue);
                    return 1;
                }

                if (!priority_is_valid(
                        requested_priority)) {

                    printf("Некорректный приоритет\n");
                    break;
                }

                if (!dequeue_not_lower(
                        &queue,
                        requested_priority,
                        &result_message,
                        &result_priority)) {

                    printf(
                        "Сообщение с приоритетом не ниже "
                        "%u не найдено\n",
                        requested_priority
                    );

                    break;
                }

                printf("Извлечённое сообщение:\n");

                print_message(
                    &result_message,
                    result_priority
                );

                break;
            }
            case 0:
                clear_queue(&queue);
                return 0;
            default:
                printf("Неизвестная команда\n");
                break;
        }
    }
}

/* Выводит данные одного сообщения и переданный отдельно приоритет. */
static void print_message(const Message *message, unsigned int priority){
    if (message == NULL) {
        return;
    }

    printf(
        "ID: %u, приоритет: %u, текст: %s\n",
        message->id,
        priority,
        message->text
    );
}

/* Последовательно обходит очередь от front и выводит все сообщения. */
static void print_queue(const PriorityQueue *queue){
    if (queue == NULL || queue->front == NULL) {
        printf("Очередь пуста\n");
        return;
    }

    const MessageNode *current_node =
        queue->front;

    printf("\nСообщения в очереди:\n");

    while (current_node != NULL) {
        print_message(
            &current_node->message,
            current_node->priority
        );

        current_node = current_node->next;
    }
}

/*
  Создаёт заданное количество демонстрационных сообщений
  со случайными приоритетами от 1 до 13.
 */
static int generate_messages(PriorityQueue *queue, unsigned int *next_id, int requested_count){
    if (queue == NULL ||
        next_id == NULL ||
        requested_count <= 0) {

        return 0;
    }

    int generated_count = 0;

    for (int i = 0; i < requested_count; i++) {
        /* Временная структура заполняется до добавления в очередь. */
        Message message = {0};

        message.id = *next_id;

        /* Текст формируется автоматически на основе ID сообщения. */
        snprintf(
            message.text,
            sizeof(message.text),
            "Сообщение %u",
            message.id
        );

        /*
          Остаток от деления даёт значение от 0 до 12,
          а прибавление единицы переводит диапазон в 1-13.
         */
        unsigned int priority =
            DEMO_MIN_PRIORITY +
            (unsigned int)(
                rand() %
                (DEMO_MAX_PRIORITY -
                 DEMO_MIN_PRIORITY + 1u)
            );

        if (!enqueue_message(
                queue,
                &message,
                priority)) {

            break;
        }

        /* ID увеличивается только после успешного добавления. */
        (*next_id)++;
        generated_count++;
    }

    return generated_count;
}