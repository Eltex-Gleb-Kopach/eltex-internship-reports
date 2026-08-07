#include "file_copy.h"

#include <stdlib.h>

int main(int argc, char *argv[]){
    struct program_options options;

    /*Заполняет options. При ошибке функция возвращает -1.*/
    if (parse_arguments(argc, argv, &options) == -1) {
        return EXIT_FAILURE;
    }
    /*Создаёт каналы, процессы и выполняет текущую логику копирования.*/
    return run_file_copy(&options, argc, argv);
}