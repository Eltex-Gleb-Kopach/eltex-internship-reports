#include <stdio.h>
#include <stdlib.h>

#include "p2p_chat.h"

int main(int argc, char *argv[])
{
    if (argc != 2) {
        fprintf(stderr,
                "Использование: %s <имя очереди>\n",
                argv[0]);

        return EXIT_FAILURE;
    }

    return run_p2p_chat(argv[1]);
}