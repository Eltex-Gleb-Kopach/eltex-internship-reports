#ifndef UDP_CHAT_H
#define UDP_CHAT_H

/* Максимальный размер имени вместе с завершающим '\0'. */
#define USER_NAME_SIZE 32

/* Общий UDP-порт всех участников чата. */
#define CHAT_PORT 45000

/* Максимальный размер одной UDP-датаграммы нашего чата. */
#define MESSAGE_SIZE 1024

/* Запускает работу группового UDP-чата. */
int run_udp_chat(const char *user_name);

#endif