#ifndef MESSAGE_QUEUE_H
#define MESSAGE_QUEUE_H

#include <sys/types.h>

#include "protocol.h"

/* Создаёт новую очередь. Используется только брокером. */
int create_message_queue(void);

/* Находит существующую очередь. Используется участниками. */
int open_message_queue(void);

/* Удаляет очередь из ядра. Используется брокером. */
int remove_message_queue(int queue_id);

/* Возвращает количество сообщений, оставшихся в очереди. */
int get_message_queue_count(int queue_id,
                            size_t *message_count);

/* Отправляет одно текстовое сообщение в очередь. */
int send_queue_message(int queue_id,
                       long message_type,
                       const char *text);

/* Получает сообщение без ожидания, если очередь сейчас пуста. */
ssize_t receive_queue_message_nowait(
    int queue_id,
    long message_type,
    struct queue_message *message);

#endif
