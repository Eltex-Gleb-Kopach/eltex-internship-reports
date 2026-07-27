#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "../priority_queue.h"

/*
  Создаёт тестовое сообщение с заданным ID
  и автоматически формирует его текст.
 */
static Message create_test_message(unsigned int id)
{
    Message message = {0};

    message.id = id;

    snprintf(
        message.text,
        sizeof(message.text),
        "Сообщение %u",
        id
    );

    return message;
}

/*
  Создаёт тестовое сообщение и проверяет,
  что оно успешно добавилось в очередь.
 */
static void enqueue_test_message(
    PriorityQueue *queue,
    unsigned int id,
    unsigned int priority
)
{
    Message message =
        create_test_message(id);

    assert(
        enqueue_message(
            queue,
            &message,
            priority
        ) == 1
    );
}

/*
  Проверяет добавление первого сообщения в пустую очередь
  и последующую очистку единственного узла.
 */
static void test_enqueue_into_empty_queue(void)
{
    PriorityQueue queue = {
        .front = NULL,
        .rear = NULL
    };

    Message message = {
        .id = 1,
        .text = "Первое сообщение"
    };

    int result =
        enqueue_message(
            &queue,
            &message,
            5
        );

    assert(result == 1);

    assert(queue.front != NULL);
    assert(queue.rear != NULL);
    assert(queue.front == queue.rear);

    assert(queue.front->message.id == 1);

    assert(
        strcmp(
            queue.front->message.text,
            "Первое сообщение"
        ) == 0
    );

    assert(queue.front->priority == 5);
    assert(queue.front->next == NULL);

    clear_queue(&queue);

    assert(queue.front == NULL);
    assert(queue.rear == NULL);
}

/*
  Проверяет добавление нескольких сообщений, FIFO-порядок,
  связи next и перемещение указателя rear.
 */
static void test_enqueue_multiple_messages(void)
{
    PriorityQueue queue = {
        .front = NULL,
        .rear = NULL
    };

    enqueue_test_message(&queue, 1, 5);
    enqueue_test_message(&queue, 2, 10);
    enqueue_test_message(&queue, 3, 7);

    MessageNode *first_node = queue.front;
    MessageNode *second_node = first_node->next;
    MessageNode *third_node = second_node->next;

    assert(first_node->message.id == 1);
    assert(first_node->priority == 5);

    assert(second_node != NULL);
    assert(second_node->message.id == 2);
    assert(second_node->priority == 10);

    assert(third_node != NULL);
    assert(third_node->message.id == 3);
    assert(third_node->priority == 7);

    assert(queue.front == first_node);
    assert(queue.rear == third_node);
    assert(third_node->next == NULL);

    clear_queue(&queue);

    assert(queue.front == NULL);
    assert(queue.rear == NULL);
}

/*
  Проверяет допустимый диапазон и правило:
  меньшее число означает более высокий приоритет.
 */
static void test_priority_rules(void)
{
    assert(priority_is_valid(0) == 1);
    assert(priority_is_valid(255) == 1);
    assert(priority_is_valid(256) == 0);

    assert(priority_is_not_lower(3, 5) == 1);
    assert(priority_is_not_lower(5, 5) == 1);
    assert(priority_is_not_lower(8, 5) == 0);

    assert(priority_is_not_lower(256, 5) == 0);
    assert(priority_is_not_lower(5, 256) == 0);
}

/*
  Проверяет последовательное извлечение первого сообщения,
  обновление front и rear, а также отказ для пустой очереди.
 */
static void test_dequeue_first(void)
{
    PriorityQueue queue = {
        .front = NULL,
        .rear = NULL
    };

    enqueue_test_message(&queue, 1, 5);
    enqueue_test_message(&queue, 2, 10);
    enqueue_test_message(&queue, 3, 7);

    Message result_message = {0};
    unsigned int result_priority = 0;

    assert(
        dequeue_first(
            &queue,
            &result_message,
            &result_priority
        ) == 1
    );

    assert(result_message.id == 1);
    assert(result_priority == 5);
    assert(queue.front->message.id == 2);
    assert(queue.rear->message.id == 3);

    assert(
        dequeue_first(
            &queue,
            &result_message,
            &result_priority
        ) == 1
    );

    assert(result_message.id == 2);
    assert(result_priority == 10);
    assert(queue.front == queue.rear);
    assert(queue.front->message.id == 3);

    assert(
        dequeue_first(
            &queue,
            &result_message,
            &result_priority
        ) == 1
    );

    assert(result_message.id == 3);
    assert(result_priority == 7);
    assert(queue.front == NULL);
    assert(queue.rear == NULL);

    assert(
        dequeue_first(
            &queue,
            &result_message,
            &result_priority
        ) == 0
    );
}

/*
  Проверяет извлечение первого сообщения с точным приоритетом
  из начала, середины и конца, а также отсутствие совпадения.
 */
static void test_dequeue_by_priority(void)
{
    PriorityQueue queue = {
        .front = NULL,
        .rear = NULL
    };

    enqueue_test_message(&queue, 1, 5);
    enqueue_test_message(&queue, 2, 10);
    enqueue_test_message(&queue, 3, 7);
    enqueue_test_message(&queue, 4, 10);

    Message result_message = {0};
    unsigned int result_priority = 0;

    /* Первым с приоритетом 10 должен быть узел id=2. */
    assert(
        dequeue_by_priority(
            &queue,
            10,
            &result_message,
            &result_priority
        ) == 1
    );

    assert(result_message.id == 2);
    assert(result_priority == 10);
    assert(queue.front->message.id == 1);
    assert(queue.front->next->message.id == 3);
    assert(queue.front->next->next == queue.rear);
    assert(queue.rear->message.id == 4);

    /* Повторный запрос удаляет второе совпадение из конца. */
    assert(
        dequeue_by_priority(
            &queue,
            10,
            &result_message,
            &result_priority
        ) == 1
    );

    assert(result_message.id == 4);
    assert(result_priority == 10);
    assert(queue.rear->message.id == 3);
    assert(queue.rear->next == NULL);

    /* Несуществующий приоритет не должен изменять очередь. */
    MessageNode *old_front = queue.front;
    MessageNode *old_rear = queue.rear;

    assert(
        dequeue_by_priority(
            &queue,
            100,
            &result_message,
            &result_priority
        ) == 0
    );

    assert(queue.front == old_front);
    assert(queue.rear == old_rear);

    /* Приоритет 5 находится в начале очереди. */
    assert(
        dequeue_by_priority(
            &queue,
            5,
            &result_message,
            &result_priority
        ) == 1
    );

    assert(result_message.id == 1);
    assert(result_priority == 5);
    assert(queue.front->message.id == 3);
    assert(queue.front == queue.rear);

    clear_queue(&queue);
}

/*
  Проверяет извлечение первого сообщения с приоритетом
  не ниже заданного из разных положений очереди.
 */
static void test_dequeue_not_lower(void)
{
    PriorityQueue queue = {
        .front = NULL,
        .rear = NULL
    };

    enqueue_test_message(&queue, 1, 8);
    enqueue_test_message(&queue, 2, 3);
    enqueue_test_message(&queue, 3, 5);
    enqueue_test_message(&queue, 4, 1);

    Message result_message = {0};
    unsigned int result_priority = 0;

    /*
      При пороге 5 узел p=8 пропускается,
      а первым подходящим становится id=2, p=3.
     */
    assert(
        dequeue_not_lower(
            &queue,
            5,
            &result_message,
            &result_priority
        ) == 1
    );

    assert(result_message.id == 2);
    assert(result_priority == 3);
    assert(queue.front->message.id == 1);
    assert(queue.front->next->message.id == 3);

    /* При пороге 1 подходящий узел id=4 находится в конце. */
    assert(
        dequeue_not_lower(
            &queue,
            1,
            &result_message,
            &result_priority
        ) == 1
    );

    assert(result_message.id == 4);
    assert(result_priority == 1);
    assert(queue.rear->message.id == 3);
    assert(queue.rear->next == NULL);

    /* Для порога 2 среди оставшихся узлов совпадений нет. */
    MessageNode *old_front = queue.front;
    MessageNode *old_rear = queue.rear;

    assert(
        dequeue_not_lower(
            &queue,
            2,
            &result_message,
            &result_priority
        ) == 0
    );

    assert(queue.front == old_front);
    assert(queue.rear == old_rear);

    /* При пороге 8 подходит первый узел id=1. */
    assert(
        dequeue_not_lower(
            &queue,
            8,
            &result_message,
            &result_priority
        ) == 1
    );

    assert(result_message.id == 1);
    assert(result_priority == 8);
    assert(queue.front->message.id == 3);
    assert(queue.front == queue.rear);

    clear_queue(&queue);
}

/*
  Проверяет неправильные аргументы, недопустимый приоритет
  и повторную безопасную очистку очереди.
 */
static void test_invalid_arguments_and_clear(void)
{
    PriorityQueue queue = {
        .front = NULL,
        .rear = NULL
    };

    Message message =
        create_test_message(1);

    Message result_message = {0};
    unsigned int result_priority = 0;

    assert(
        enqueue_message(
            NULL,
            &message,
            5
        ) == 0
    );

    assert(
        enqueue_message(
            &queue,
            NULL,
            5
        ) == 0
    );

    assert(
        enqueue_message(
            &queue,
            &message,
            256
        ) == 0
    );

    assert(queue.front == NULL);
    assert(queue.rear == NULL);

    assert(
        dequeue_first(
            NULL,
            &result_message,
            &result_priority
        ) == 0
    );

    assert(
        dequeue_first(
            &queue,
            &result_message,
            &result_priority
        ) == 0
    );

    enqueue_test_message(&queue, 1, 5);
    enqueue_test_message(&queue, 2, 10);

    MessageNode *old_front = queue.front;
    MessageNode *old_rear = queue.rear;

    assert(
        dequeue_by_priority(
            &queue,
            256,
            &result_message,
            &result_priority
        ) == 0
    );

    assert(
        dequeue_by_priority(
            &queue,
            5,
            NULL,
            &result_priority
        ) == 0
    );

    assert(
        dequeue_not_lower(
            &queue,
            5,
            &result_message,
            NULL
        ) == 0
    );

    assert(queue.front == old_front);
    assert(queue.rear == old_rear);

    clear_queue(&queue);

    assert(queue.front == NULL);
    assert(queue.rear == NULL);

    clear_queue(&queue);
    clear_queue(NULL);

    assert(queue.front == NULL);
    assert(queue.rear == NULL);
}

int main(void)
{
    test_enqueue_into_empty_queue();
    test_enqueue_multiple_messages();
    test_priority_rules();
    test_dequeue_first();
    test_dequeue_by_priority();
    test_dequeue_not_lower();
    test_invalid_arguments_and_clear();

    printf("Все тесты пройдены\n");

    return 0;
}
