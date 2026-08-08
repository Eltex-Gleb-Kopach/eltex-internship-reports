#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "broker.h"
#include "protocol.h"
#include "publisher.h"
#include "subscriber.h"

static int is_valid_topic(const char *topic)
{
    size_t topic_length = strlen(topic);

    if (topic_length == 0 || topic_length >= TOPIC_SIZE) {
        return 0;
    }

    /*
     * Запятая используется как разделитель полей
     * внутри сообщений нашего протокола.
     */
    if (strchr(topic, ',') != NULL) {
        return 0;
    }

    return 1;
}

static void print_usage(const char *program_name)
{
    fprintf(stderr,
            "Использование:\n"
            "  %s -b\n"
            "  %s -p <тема>\n"
            "  %s -s <тема1> [тема2 ...]\n",
            program_name,
            program_name,
            program_name);
}

int main(int argc, char *argv[])
{
    if (argc < 2) {
        fprintf(stderr, "Ошибка: не указана роль процесса.\n");
        print_usage(argv[0]);
        return EXIT_FAILURE;
    }

    if (strcmp(argv[1], "-b") == 0) {
        if (argc != 2) {
            fprintf(stderr,
                    "Ошибка: для брокера дополнительные аргументы не нужны.\n");
            return EXIT_FAILURE;
        }

        return run_broker();
    }

    if (strcmp(argv[1], "-p") == 0) {
        if (argc != 3) {
            fprintf(stderr,
                    "Ошибка: издателю нужно указать одну тему.\n");
            return EXIT_FAILURE;
        }

        if (!is_valid_topic(argv[2])) {
            fprintf(stderr,
                    "Ошибка: неправильная или слишком длинная тема.\n");
            return EXIT_FAILURE;
        }

        return run_publisher(argv[2]);
    }

    if (strcmp(argv[1], "-s") == 0) {
        if (argc < 3) {
            fprintf(stderr,
                    "Ошибка: подписчику нужно указать хотя бы одну тему.\n");
            return EXIT_FAILURE;
        }

        for (int i = 2; i < argc; ++i) {
            if (!is_valid_topic(argv[i])) {
                fprintf(stderr,
                        "Ошибка: неправильная тема %s.\n",
                        argv[i]);
                return EXIT_FAILURE;
            }
        }

        /*
         * argc - 2 — количество тем.
         * &argv[2] — адрес первого названия темы.
         */
        return run_subscriber(argc - 2, &argv[2]);
    }

    fprintf(stderr, "Ошибка: неизвестный ключ %s.\n", argv[1]);
    print_usage(argv[0]);

    return EXIT_FAILURE;
}