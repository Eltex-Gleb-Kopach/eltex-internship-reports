#include "message_queue.h"

#include <errno.h>
#include <stddef.h>
#include <string.h>
#include <sys/ipc.h>
#include <sys/msg.h>

#define MESSAGE_QUEUE_KEY ((key_t)0x454C5458)

int create_message_queue(void)
{
    return msgget(MESSAGE_QUEUE_KEY,
                  IPC_CREAT | IPC_EXCL | 0600);
}

int open_message_queue(void)
{
    return msgget(MESSAGE_QUEUE_KEY, 0);
}

int remove_message_queue(int queue_id)
{
    return msgctl(queue_id, IPC_RMID, NULL);
}

int get_message_queue_count(int queue_id,
                            size_t *message_count)
{
    if (message_count == NULL) {
        errno = EINVAL;
        return -1;
    }

    struct msqid_ds queue_info;

    if (msgctl(queue_id,
               IPC_STAT,
               &queue_info) == -1) {
        return -1;
    }

    *message_count = (size_t)queue_info.msg_qnum;

    return 0;
}

int send_queue_message(int queue_id,
                       long message_type,
                       const char *text)
{
    if (message_type <= 0 || text == NULL) {
        errno = EINVAL;
        return -1;
    }

    struct queue_message message;
    size_t text_size = strlen(text) + 1;

    /*
     * Вместе с текстом отправляется завершающий байт '\0'.
     * Поэтому к результату strlen() прибавляется единица.
     */
    if (text_size > sizeof(message.text)) {
        errno = EMSGSIZE;
        return -1;
    }

    message.message_type = message_type;
    memcpy(message.text, text, text_size);

    /*
     * В msgsz передаётся размер данных после поля message_type.
     * Само поле long message_type в этот размер не входит.
     */
    return msgsnd(queue_id,
                  &message,
                  text_size,
                  0);
}

static ssize_t receive_queue_message_with_flags(
    int queue_id,
    long message_type,
    struct queue_message *message,
    int flags)
{
    if (message == NULL || message_type <= 0) {
        errno = EINVAL;
        return -1;
    }

    /*
     * msgrcv() извлекает первое сообщение с указанным типом.
     * Размер поля message_type здесь также не учитывается.
     */
    ssize_t bytes_received =
        msgrcv(queue_id,
               message,
               sizeof(message->text),
               message_type,
               flags);

    if (bytes_received == -1) {
        return -1;
    }

    /*
     * Защищаем дальнейшую работу со строкой, даже если сообщение
     * было отправлено некорректной сторонней программой.
     */
    if ((size_t)bytes_received < sizeof(message->text)) {
        message->text[bytes_received] = '\0';
    } else {
        message->text[sizeof(message->text) - 1] = '\0';
    }

    return bytes_received;
}

ssize_t receive_queue_message(int queue_id,
                              long message_type,
                              struct queue_message *message)
{
    return receive_queue_message_with_flags(queue_id,
                                            message_type,
                                            message,
                                            0);
}

ssize_t receive_queue_message_nowait(
    int queue_id,
    long message_type,
    struct queue_message *message)
{
    return receive_queue_message_with_flags(queue_id,
                                            message_type,
                                            message,
                                            IPC_NOWAIT);
}
