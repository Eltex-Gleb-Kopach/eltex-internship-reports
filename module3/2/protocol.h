#ifndef PROTOCOL_H
#define PROTOCOL_H

/*
 * Все сообщения от издателей и подписчиков сначала отправляются
 * брокеру с типом 1.
 */
#define BROKER_MESSAGE_TYPE 1L

/* Максимальные размеры темы, полезной нагрузки и текста сообщения. */
#define TOPIC_SIZE 64
#define PAYLOAD_SIZE 512
#define MESSAGE_TEXT_SIZE 640

/*
 * Формат сообщения для очереди System V.
 * Поле message_type обязательно должно находиться первым.
 */
struct queue_message {
    long message_type;
    char text[MESSAGE_TEXT_SIZE];
};

#endif