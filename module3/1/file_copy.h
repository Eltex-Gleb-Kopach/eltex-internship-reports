#ifndef FILE_COPY_H
#define FILE_COPY_H

/*Структура хранит настройки одного запуска программы.*/
struct program_options {
    const char *channel_name;
    int first_file_index;
};

int parse_arguments(int argc, char *argv[], struct program_options *options);

int run_file_copy(const struct program_options *options, int argc, char *argv[]);

#endif
