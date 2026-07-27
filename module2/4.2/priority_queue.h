#ifndef PRIORITY_QUEUE_H
#define PRIORITY_QUEUE_H

#define MAX_MESSAGE_TEXT 128

#define MIN_PRIORITY 0u
#define MAX_PRIORITY 255u

#define DEMO_MIN_PRIORITY 1u
#define DEMO_MAX_PRIORITY 13u

/*
  Хранит данные одного сообщения:
  уникальный ID и текст.
 */
typedef struct {
    unsigned int id;
    char text[MAX_MESSAGE_TEXT];
} Message;

/*
  Представляет один динамический узел очереди:
  сообщение, его приоритет и связь со следующим узлом.
 */
typedef struct MessageNode {
    Message message;
    unsigned int priority;
    struct MessageNode *next;
} MessageNode;

/*
  Хранит адреса первого и последнего узлов
  для извлечения из начала и добавления в конец.
 */
typedef struct {
    MessageNode *front;
    MessageNode *rear;
} PriorityQueue;

/*
  Проверяет, входит ли приоритет
  в допустимый диапазон от 0 до 255.
 */
int priority_is_valid(unsigned int priority);

/*
  Проверяет, является ли фактический приоритет
  не ниже заданного. Меньшее число означает
  более высокий приоритет.
 */
int priority_is_not_lower(unsigned int actual_priority, unsigned int requested_priority);

/*
  Создаёт узел с сообщением и добавляет
  его в конец очереди.
 */
int enqueue_message(PriorityQueue *queue, const Message *message, unsigned int priority);

/*
  Извлекает первый элемент очереди независимо
  от его приоритета и освобождает память узла.
 */
int dequeue_first(PriorityQueue *queue, Message *result_message, unsigned int *result_priority);

/*
  Находит и извлекает первый элемент
  с точно указанным приоритетом.
 */
int dequeue_by_priority(
    PriorityQueue *queue,
    unsigned int requested_priority,
    Message *result_message,
    unsigned int *result_priority
);

/*
  Находит и извлекает первый элемент
  с приоритетом не ниже заданного.
 */
int dequeue_not_lower(
    PriorityQueue *queue,
    unsigned int requested_priority,
    Message *result_message,
    unsigned int *result_priority
);

/*
  Освобождает память всех узлов
  и переводит очередь в пустое состояние.
 */
void clear_queue(PriorityQueue *queue);

#endif
