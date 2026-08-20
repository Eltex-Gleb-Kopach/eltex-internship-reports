#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "udp_chat.h"

/* Показывает правильный формат запуска программы. */
static void print_usage(const char *program_name)
{
    fprintf(stderr,
            "Использование: %s <имя участника>\n",
            program_name);
}

int main(int argc, char *argv[])
{
    /* После имени программы должен быть указан один аргумент — имя. */
    if (argc != 2) {
        print_usage(argv[0]);
        return EXIT_FAILURE;
    }

    const char *user_name = argv[1];
    size_t user_name_length = strlen(user_name);

    /*
     * Имя не должно быть пустым или превышать размер буфера.
     * Символ '|' позже будет разделителем частей сетевого сообщения.
     */
    if (user_name_length == 0
        || user_name_length >= USER_NAME_SIZE
        || strchr(user_name, '|') != NULL) {
        fprintf(stderr,
                "Ошибка: неправильное имя участника.\n");

        return EXIT_FAILURE;
    }

    return run_udp_chat(user_name);
}