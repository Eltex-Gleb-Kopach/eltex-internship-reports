#ifndef CALCULATOR_H
#define CALCULATOR_H

#define MAX_COMMANDS 10
#define MAX_COMMAND_NAME 64

/* Общий тип функции, которую должен предоставлять каждый плагин. */
typedef int (*Operation)(
    double first,
    double second,
    double *result
);

/*
  Хранит название загруженной команды, адрес её функции
  и дескриптор открытой динамической библиотеки.
 */
typedef struct {
    char name[MAX_COMMAND_NAME];
    Operation function;
    void *library_handle;
} Command;

#endif
