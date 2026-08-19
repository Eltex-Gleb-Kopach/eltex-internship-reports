#include "consumer.h"
#include "producer.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void print_usage(const char *program_name)
{
    fprintf(stderr,
            "Использование:\n"
            "  %s producer\n"
            "  %s consumer\n",
            program_name,
            program_name);
}

int main(int argc, char *argv[])
{
    if (argc != 2) {
        print_usage(argv[0]);
        return EXIT_FAILURE;
    }

    if (strcmp(argv[1], "producer") == 0) {
        return run_producer();
    }

    if (strcmp(argv[1], "consumer") == 0) {
        return run_consumer();
    }

    fprintf(stderr,
            "Ошибка: неизвестный режим %s.\n",
            argv[1]);

    print_usage(argv[0]);

    return EXIT_FAILURE;
}