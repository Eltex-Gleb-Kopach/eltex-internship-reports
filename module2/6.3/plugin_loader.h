#ifndef PLUGIN_LOADER_H
#define PLUGIN_LOADER_H

#include "calculator.h"

/* Загружает найденные в каталоге плагины в массив команд. */
int load_plugins(const char directory[], Command commands[], int commands_capacity);

/* Закрывает библиотеки и очищает загруженные команды. */
void unload_plugins(Command commands[], int commands_count);

#endif
