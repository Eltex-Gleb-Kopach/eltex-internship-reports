#include <stdlib.h>

#include "priority_queue.h"

int priority_is_valid(unsigned int priority){
    /* Беззнаковое значение уже не может быть меньше нуля. */
    return priority <= MAX_PRIORITY;
}

int priority_is_not_lower(unsigned int actual_priority, unsigned int requested_priority){
    if (!priority_is_valid(actual_priority) ||
        !priority_is_valid(requested_priority)) {

        return 0;
    }

    /*
      Меньшее число обозначает более высокий приоритет.
      Поэтому приоритет не ниже заданного должен быть
      численно меньше или равен заданному значению.
     */
    return actual_priority <= requested_priority;
}

int enqueue_message(PriorityQueue *queue, const Message *message, unsigned int priority){
    if (queue == NULL ||
        message == NULL ||
        !priority_is_valid(priority)) {

        return 0;
    }

    /* Создаём отдельный динамический узел для нового сообщения. */
    MessageNode *new_node =
        malloc(sizeof(MessageNode));

    if (new_node == NULL) {
        return 0;
    }

    /*
      Копируем сообщение в узел. Следующего элемента пока нет,
      так как новый узел добавляется в конец очереди.
     */
    new_node->message = *message;
    new_node->priority = priority;
    new_node->next = NULL;

    /* В пустой очереди первый узел одновременно front и rear. */
    if (queue->rear == NULL) {
        queue->front = new_node;
        queue->rear = new_node;

        return 1;
    }

    /*
      Старый последний узел связываем с новым,
      после чего переносим rear на новый конец очереди.
     */
    queue->rear->next = new_node;
    queue->rear = new_node;

    return 1;
}

/*
  Общая внутренняя функция извлечения уже найденного узла.
  Она перестраивает связи, возвращает данные и освобождает память.
 */
static int remove_found_node(
    PriorityQueue *queue,
    MessageNode *previous_node,
    MessageNode *current_node,
    Message *result_message,
    unsigned int *result_priority){
    if (queue == NULL ||
        current_node == NULL ||
        result_message == NULL ||
        result_priority == NULL) {

        return 0;
    }

    /* Копируем данные до освобождения памяти найденного узла. */
    *result_message = current_node->message;
    *result_priority = current_node->priority;

    /*
      Если предыдущего узла нет, удаляется front.
      Иначе предыдущий узел начинает указывать на следующий.
     */
    if (previous_node == NULL) {
        queue->front = current_node->next;
    } else {
        previous_node->next = current_node->next;
    }

    /* При удалении последнего узла обновляем rear. */
    if (current_node == queue->rear) {
        queue->rear = previous_node;
    }

    /* В пустой очереди оба указателя должны быть равны NULL. */
    if (queue->front == NULL) {
        queue->rear = NULL;
    }

    free(current_node);

    return 1;
}

int dequeue_first(PriorityQueue *queue, Message *result_message, unsigned int *result_priority){
    if (queue == NULL) {
        return 0;
    }

    /*
      Для первого узла previous_node отсутствует,
      а удаляемым узлом является queue->front.
     */
    return remove_found_node(
        queue,
        NULL,
        queue->front,
        result_message,
        result_priority
    );
}

int dequeue_by_priority(
    PriorityQueue *queue,
    unsigned int requested_priority,
    Message *result_message,
    unsigned int *result_priority){
    if (queue == NULL ||
        !priority_is_valid(requested_priority)) {

        return 0;
    }

    /*
      previous_node нужен для восстановления связи,
      если совпадение будет найдено не в начале очереди.
     */
    MessageNode *previous_node = NULL;
    MessageNode *current_node = queue->front;

    /* Идём от front до первого точного совпадения приоритета. */
    while (current_node != NULL &&
           current_node->priority !=
               requested_priority) {

        previous_node = current_node;
        current_node = current_node->next;
    }

    /*
      Если совпадение не найдено, current_node равен NULL
      и remove_found_node() вернёт 0 без изменения очереди.
     */
    return remove_found_node(
        queue,
        previous_node,
        current_node,
        result_message,
        result_priority
    );
}

int dequeue_not_lower(
    PriorityQueue *queue,
    unsigned int requested_priority,
    Message *result_message,
    unsigned int *result_priority){
    if (queue == NULL ||
        !priority_is_valid(requested_priority)) {

        return 0;
    }

    MessageNode *previous_node = NULL;
    MessageNode *current_node = queue->front;

    /*
      Пропускаем узлы, приоритет которых ниже заданного.
      Поиск заканчивается на первом подходящем элементе.
     */
    while (current_node != NULL &&
           !priority_is_not_lower(
               current_node->priority,
               requested_priority
           )) {

        previous_node = current_node;
        current_node = current_node->next;
    }

    /* Найденный узел извлекается общей функцией удаления. */
    return remove_found_node(
        queue,
        previous_node,
        current_node,
        result_message,
        result_priority
    );
}

void clear_queue(PriorityQueue *queue)
{
    if (queue == NULL) {
        return;
    }

    MessageNode *current_node = queue->front;

    while (current_node != NULL) {
        /*
          Сохраняем адрес следующего узла до free(), потому что
          после освобождения обращаться к current_node уже нельзя.
         */
        MessageNode *next_node =
            current_node->next;

        free(current_node);

        current_node = next_node;
    }

    /* После освобождения всех узлов очередь снова считается пустой. */
    queue->front = NULL;
    queue->rear = NULL;
}
