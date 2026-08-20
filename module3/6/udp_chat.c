#define _POSIX_C_SOURCE 200809L

#include "udp_chat.h"

#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <unistd.h>
#include <netinet/in.h>
#include <string.h>
#include <errno.h>
#include <poll.h>

/* Флаг устанавливается обработчиком после нажатия Ctrl+C. */
static volatile sig_atomic_t stop_requested = 0;

/* Обработчик SIGINT выполняет только безопасное изменение флага. */
static void handle_sigint(int signal_number)
{
    (void)signal_number;
    stop_requested = 1;
}

/* Настраивает корректное завершение программы по Ctrl+C. */
static int install_sigint_handler(void)
{
    struct sigaction action = {0};

    action.sa_handler = handle_sigint;
    sigemptyset(&action.sa_mask);
    action.sa_flags = 0;

    return sigaction(SIGINT, &action, NULL);
}

/*
 * Отправляет готовое сообщение на широковещательный адрес.
 * Используется для JOIN, MESSAGE и LEAVE.
 */
static int send_broadcast_message(
    int socket_fd,
    const struct sockaddr_in *broadcast_address,
    const char *message)
{
    size_t message_length = strlen(message);
    ssize_t bytes_sent;

    /*
     * Если sendto() был прерван сигналом,
     * повторяем отправку.
     */
    do {
        bytes_sent =
            sendto(socket_fd,
                   message,
                   message_length,
                   0,
                   (const struct sockaddr *)broadcast_address,
                   sizeof(*broadcast_address));
    } while (bytes_sent == -1
             && errno == EINTR);

    if (bytes_sent == -1) {
        return -1;
    }

    /*
     * UDP-датаграмма должна быть отправлена целиком.
     */
    if ((size_t)bytes_sent != message_length) {
        errno = EIO;
        return -1;
    }

    return 0;
}

/* Разбирает и выводит принятое сообщение протокола чата. */
static void display_received_message(const char *message,
                                     const char *user_name)
{
    const char join_prefix[] = "JOIN|";
    const char message_prefix[] = "MESSAGE|";
    const char leave_prefix[] = "LEAVE|";

    /* Сообщение о подключении: JOIN|имя. */
    if (strncmp(message,
                join_prefix,
                sizeof(join_prefix) - 1) == 0) {
        const char *sender_name =
            message + sizeof(join_prefix) - 1;

        if (sender_name[0] == '\0'
            || strlen(sender_name) >= USER_NAME_SIZE
            || strchr(sender_name, '|') != NULL) {
            fprintf(stderr,
                    "Получено неправильное сообщение JOIN.\n");
            return;
        }

        if (strcmp(sender_name, user_name) != 0) {
            printf("[Система] В сети появился участник %s.\n",
                   sender_name);
        }

        return;
    }

    /* Сообщение об отключении: LEAVE|имя. */
    if (strncmp(message,
                leave_prefix,
                sizeof(leave_prefix) - 1) == 0) {
        const char *sender_name =
            message + sizeof(leave_prefix) - 1;

        if (sender_name[0] == '\0'
            || strlen(sender_name) >= USER_NAME_SIZE
            || strchr(sender_name, '|') != NULL) {
            fprintf(stderr,
                    "Получено неправильное сообщение LEAVE.\n");
            return;
        }

        if (strcmp(sender_name, user_name) != 0) {
            printf("[Система] Участник %s вышел из сети.\n",
                   sender_name);
        }

        return;
    }

    /* Обычное сообщение: MESSAGE|имя|текст. */
    if (strncmp(message,
                message_prefix,
                sizeof(message_prefix) - 1) == 0) {
        const char *sender_name =
            message + sizeof(message_prefix) - 1;

        const char *separator =
            strchr(sender_name, '|');

        if (separator == NULL
            || separator == sender_name
            || separator[1] == '\0') {
            fprintf(stderr,
                    "Получено неправильное сообщение MESSAGE.\n");
            return;
        }

        size_t sender_name_length =
            (size_t)(separator - sender_name);

        if (sender_name_length >= USER_NAME_SIZE) {
            fprintf(stderr,
                    "Получено слишком длинное имя отправителя.\n");
            return;
        }

        /*
         * Собственное широковещательное сообщение
         * может вернуться в этот же сокет.
         */
        if (strlen(user_name) == sender_name_length
            && strncmp(sender_name,
                       user_name,
                       sender_name_length) == 0) {
            return;
        }

        const char *message_text = separator + 1;

        printf("[%.*s] %s\n",
               (int)sender_name_length,
               sender_name,
               message_text);

        return;
    }

    fprintf(stderr,
            "Получено неизвестное сообщение: %s\n",
            message);
}

int run_udp_chat(const char *user_name)
{
    printf("Режим: групповой UDP-чат.\n");
    printf("Имя участника: %s.\n", user_name);

    if (install_sigint_handler() == -1) {
        perror("sigaction");
        return EXIT_FAILURE;
    }

    /*
     * Создаём UDP-сокет:
     * AF_INET — используем IPv4;
     * SOCK_DGRAM — передаём отдельные UDP-датаграммы;
     * 0 — ядро само выбирает протокол UDP.
     */
    int socket_fd = socket(AF_INET, SOCK_DGRAM, 0);

    if (socket_fd == -1) {
        perror("socket");
        return EXIT_FAILURE;
    }

    /*
    * Разрешаем сокету отправлять широковещательные сообщения
    * сразу всем устройствам в выбранной сети.
    */
    int broadcast_enabled = 1;

    if (setsockopt(socket_fd,
                SOL_SOCKET,
                SO_BROADCAST,
                &broadcast_enabled,
                sizeof(broadcast_enabled)) == -1) {
        perror("setsockopt SO_BROADCAST");
        close(socket_fd);
        return EXIT_FAILURE;
    }

    /*
    * Разрешаем нескольким экземплярам чата
    * привязываться к одному UDP-порту.
    */
    int address_reuse_enabled = 1;

    if (setsockopt(socket_fd,
                SOL_SOCKET,
                SO_REUSEADDR,
                &address_reuse_enabled,
                sizeof(address_reuse_enabled)) == -1) {
        perror("setsockopt SO_REUSEADDR");
        close(socket_fd);
        return EXIT_FAILURE;
    }

    /*
    * Указываем локальный адрес, на котором программа
    * будет принимать сообщения остальных участников.
    */
    struct sockaddr_in local_address = {
        .sin_family = AF_INET,
        .sin_port = htons(CHAT_PORT),
        .sin_addr.s_addr = htonl(INADDR_ANY)
    };

    if (bind(socket_fd,
            (struct sockaddr *)&local_address,
            sizeof(local_address)) == -1) {
        perror("bind");
        close(socket_fd);
        return EXIT_FAILURE;
    }

    printf("UDP-сокет привязан к порту %d.\n",
        CHAT_PORT);

    printf("Широковещательная отправка разрешена.\n");

    printf("UDP-сокет создан. Дескриптор: %d.\n",
           socket_fd);

    /*
    * Формируем адрес широковещательной рассылки.
    * 255.255.255.255 означает отправку всем устройствам
    * доступной локальной сети.
    */
    struct sockaddr_in broadcast_address = {
        .sin_family = AF_INET,
        .sin_port = htons(CHAT_PORT),
        .sin_addr.s_addr = htonl(INADDR_BROADCAST)
    };

    /* Формируем служебное сообщение о подключении участника. */
    char join_message[MESSAGE_SIZE];

    int written = snprintf(join_message,
                        sizeof(join_message),
                        "JOIN|%s",
                        user_name);

    if (written < 0
        || (size_t)written >= sizeof(join_message)) {
        fprintf(stderr,
                "Ошибка: сообщение о подключении слишком длинное.\n");

        close(socket_fd);
        return EXIT_FAILURE;
    }

    /* Отправляем сообщение о подключении всем участникам сети. */
    if (send_broadcast_message(socket_fd,
                               &broadcast_address,
                               join_message) == -1) {
        perror("sendto JOIN");
        close(socket_fd);
        return EXIT_FAILURE;
    }

    printf("Сообщение о подключении отправлено: %s.\n",
        join_message);

    /*
    * poll() будет одновременно следить:
    * за UDP-сокетом и стандартным вводом.
    */
    struct pollfd monitored_fds[2] = {
        {
            .fd = socket_fd,
            .events = POLLIN
        },
        {
            .fd = STDIN_FILENO,
            .events = POLLIN
        }
    };

    printf("Чат запущен. Введите сообщение "
        "или /exit для завершения.\n");

    while (!stop_requested) {
        int poll_result =
            poll(monitored_fds, 2, -1);

        if (poll_result == -1) {
            if (errno == EINTR) {
                continue;
            }

            perror("poll");
            close(socket_fd);
            return EXIT_FAILURE;
        }

        /*
        * Проверяем ошибки UDP-сокета.
        * Эти события не означают наличие обычного сообщения.
        */
        if ((monitored_fds[0].revents
            & (POLLERR | POLLHUP | POLLNVAL)) != 0) {
            fprintf(stderr,
                    "Ошибка: UDP-сокет стал недоступен.\n");

            close(socket_fd);
            return EXIT_FAILURE;
        }

        /*
        * POLLHUP для stdin обрабатывается ниже через fgets() и feof().
        * Здесь проверяем только настоящую ошибку и неправильный дескриптор.
        */
        if ((monitored_fds[1].revents
            & (POLLERR | POLLNVAL)) != 0) {
            fprintf(stderr,
                    "Ошибка: стандартный ввод стал недоступен.\n");

            close(socket_fd);
            return EXIT_FAILURE;
        }

        /*
        * В UDP-сокете появилась входящая датаграмма.
        */
        if ((monitored_fds[0].revents & POLLIN) != 0) {
            char received_message[MESSAGE_SIZE];

            ssize_t bytes_received =
                recvfrom(socket_fd,
                        received_message,
                        sizeof(received_message) - 1,
                        0,
                        NULL,
                        NULL);

            if (bytes_received == -1) {
                perror("recvfrom");
                close(socket_fd);
                return EXIT_FAILURE;
            }

            received_message[bytes_received] = '\0';

            display_received_message(received_message,
                                    user_name);
        }

        /*
        * Пользователь ввёл строку с клавиатуры.
        */
        if ((monitored_fds[1].revents
            & (POLLIN | POLLHUP)) != 0) {
            char input_message[MESSAGE_SIZE];

            if (fgets(input_message,
                    sizeof(input_message),
                    stdin) == NULL) {
                if (feof(stdin)) {
                    break;
                }

                perror("fgets");
                close(socket_fd);
                return EXIT_FAILURE;
            }

            char *newline =
                strchr(input_message, '\n');

            if (newline != NULL) {
                *newline = '\0';
            } else if (!feof(stdin)) {
                /*
                * Строка не поместилась в буфер.
                * Удаляем оставшиеся символы из stdin.
                */
                int character;

                do {
                    character = getchar();
                } while (character != '\n'
                        && character != EOF);

                fprintf(stderr,
                        "Ошибка: сообщение слишком длинное.\n");
                continue;
            }

            if (input_message[0] == '\0') {
                fprintf(stderr,
                        "Ошибка: сообщение не может быть пустым.\n");
                continue;
            }

            if (strcmp(input_message, "/exit") == 0) {
                break;
            }

            /*
            * Формируем сообщение:
            * MESSAGE|имя|текст
            */
            char chat_message[MESSAGE_SIZE];

            int message_length =
                snprintf(chat_message,
                        sizeof(chat_message),
                        "MESSAGE|%s|%s",
                        user_name,
                        input_message);

            if (message_length < 0
                || (size_t)message_length
                    >= sizeof(chat_message)) {
                fprintf(stderr,
                        "Ошибка: сообщение слишком длинное.\n");
                continue;
            }

            if (send_broadcast_message(socket_fd,
                                       &broadcast_address,
                                       chat_message) == -1) {
                perror("sendto MESSAGE");
                close(socket_fd);
                return EXIT_FAILURE;
            }

            printf("Сообщение отправлено.\n");
        }
    }

    if (stop_requested) {
        printf("\nПолучен SIGINT. Начато завершение чата.\n");
    }

    /* Формируем сообщение об отключении участника. */
    char leave_message[MESSAGE_SIZE];

    int leave_length =
        snprintf(leave_message,
                sizeof(leave_message),
                "LEAVE|%s",
                user_name);

    if (leave_length < 0
        || (size_t)leave_length >= sizeof(leave_message)) {
        fprintf(stderr,
                "Ошибка: сообщение об отключении слишком длинное.\n");

        close(socket_fd);
        return EXIT_FAILURE;
    }

    /* Уведомляем остальных участников о выходе из чата. */
    if (send_broadcast_message(socket_fd,
                               &broadcast_address,
                               leave_message) == -1) {
        perror("sendto LEAVE");
        close(socket_fd);
        return EXIT_FAILURE;
    }

    printf("Сообщение об отключении отправлено: %s.\n",
        leave_message);

    /* Закрываем сокет после /exit или окончания стандартного ввода. */
    if (close(socket_fd) == -1) {
        perror("close socket");
        return EXIT_FAILURE;
    }

    printf("UDP-сокет закрыт.\n");

    return EXIT_SUCCESS;
}
