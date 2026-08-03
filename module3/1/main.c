#include "file_copy.h"

#include <stdlib.h>

int main(int argc, char *argv[])
{
    struct program_options options;

    if (parse_arguments(argc, argv, &options) == -1) {
        return EXIT_FAILURE;
    }

    return file_copy_run(&options, argc, argv);
}