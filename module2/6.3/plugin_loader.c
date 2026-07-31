#include <dirent.h>
#include <dlfcn.h>
#include <stdio.h>
#include <string.h>

#include "plugin_loader.h"

#define MAX_PLUGIN_PATH 512

/* Проверяет, заканчивается ли имя файла расширением .so. */
static int has_so_extension(const char filename[]){
    size_t length = strlen(filename);

    if (length <= 3) {
        return 0;
    }

    return strcmp(filename + length - 3, ".so") == 0;
}

/*
  Получает название команды из имени библиотеки:
  libadd.so превращается в add.
 */
static int copy_command_name(char destination[], int destination_size, const char filename[]){
    size_t filename_length = strlen(filename);
    size_t name_start = 0;
    size_t name_length = filename_length - 3;

    if (strncmp(filename, "lib", 3) == 0) {
        name_start = 3;
        name_length -= 3;
    }

    if (name_length == 0 ||
        name_length >= (size_t)destination_size) {
        return 0;
    }

    memcpy(destination, filename + name_start, name_length);
    destination[name_length] = '\0';

    return 1;
}

/*
  Просматривает каталог, открывает найденные библиотеки
  и добавляет их функции operation в массив команд.
 */
int load_plugins(const char directory[], Command commands[], int commands_capacity){
    if (directory == NULL ||
        commands == NULL ||
        commands_capacity <= 0) {
        return 0;
    }

    DIR *plugin_directory = opendir(directory);

    if (plugin_directory == NULL) {
        perror("Cannot open plugin directory");
        return 0;
    }

    int commands_count = 0;
    struct dirent *entry = NULL;

    while (commands_count < commands_capacity &&
           (entry = readdir(plugin_directory)) != NULL) {
        if (!has_so_extension(entry->d_name)) {
            continue;
        }

        /* Составляем полный путь, который будет передан в dlopen. */
        char plugin_path[MAX_PLUGIN_PATH];

        int path_length = snprintf(
            plugin_path,
            sizeof(plugin_path),
            "%s/%s",
            directory,
            entry->d_name
        );

        if (path_length < 0 ||
            path_length >= (int)sizeof(plugin_path)) {
            fprintf(stderr, "Plugin path is too long: %s\n", entry->d_name);
            continue;
        }

        /* Открываем библиотеку и сразу проверяем её символы. */
        void *library_handle = dlopen(plugin_path, RTLD_NOW);

        if (library_handle == NULL) {
            fprintf(stderr, "Cannot load %s: %s\n", plugin_path, dlerror());
            continue;
        }

        /* Очищаем предыдущую ошибку перед вызовом dlsym. */
        dlerror();

        Operation function = NULL;

        /*
          dlsym возвращает универсальный адрес void *. Записываем
          найденный адрес в указатель на функцию типа Operation.
         */
        *(void **)(&function) = dlsym(library_handle, "operation");

        const char *error = dlerror();

        if (error != NULL) {
            fprintf(stderr, "Cannot find operation in %s: %s\n",
                    plugin_path, error);
            dlclose(library_handle);
            continue;
        }

        Command *command = &commands[commands_count];

        if (!copy_command_name(
                command->name,
                sizeof(command->name),
                entry->d_name
            )) {
            fprintf(stderr, "Wrong plugin filename: %s\n", entry->d_name);
            dlclose(library_handle);
            continue;
        }

        /*
          Дескриптор сохраняется вместе с функцией, потому что
          библиотека должна оставаться открытой до конца работы.
         */
        command->function = function;
        command->library_handle = library_handle;
        commands_count++;
    }

    closedir(plugin_directory);

    return commands_count;
}

/* Закрывает каждую библиотеку и очищает соответствующую команду. */
void unload_plugins(Command commands[], int commands_count){
    if (commands == NULL || commands_count <= 0) {
        return;
    }

    for (int index = 0; index < commands_count; index++) {
        if (commands[index].library_handle != NULL) {
            dlclose(commands[index].library_handle);
        }

        commands[index].name[0] = '\0';
        commands[index].function = NULL;
        commands[index].library_handle = NULL;
    }
}
