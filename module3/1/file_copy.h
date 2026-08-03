#ifndef FILE_COPY_H
#define FILE_COPY_H

struct program_options {
    const char *fifo_name;
    int first_file_index;
};

int parse_arguments(int argc,
                    char *argv[],
                    struct program_options *options);

int file_copy_run(const struct program_options *options,
                  int argc,
                  char *argv[]);

#endif