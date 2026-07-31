#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "../calculator.h"
#include "../plugin_loader.h"

#define PLUGIN_DIRECTORY "./plugins"

/* Ищет загруженную команду по имени независимо от порядка файлов. */
static Command *find_command(Command commands[], int commands_count, const char name[]){
    for (int index = 0; index < commands_count; index++) {
        if (strcmp(commands[index].name, name) == 0) {
            return &commands[index];
        }
    }

    return NULL;
}

/*
  Проверяет загрузку всех библиотек, получение названий и адресов
  функций, а также сохранение дескрипторов открытых библиотек.
 */
void test_load_plugins(void){
    Command commands[MAX_COMMANDS] = {0};

    int commands_count = load_plugins(
        PLUGIN_DIRECTORY,
        commands,
        MAX_COMMANDS
    );

    assert(commands_count == 4);

    const char *expected_names[] = {
        "add",
        "subtract",
        "multiply",
        "divide"
    };

    int expected_count =
        (int)(sizeof(expected_names) / sizeof(expected_names[0]));

    for (int index = 0; index < expected_count; index++) {
        Command *command = find_command(
            commands,
            commands_count,
            expected_names[index]
        );

        assert(command != NULL);
        assert(command->function != NULL);
        assert(command->library_handle != NULL);
    }

    unload_plugins(commands, commands_count);
}

/*
  Проверяет вызов каждой арифметической операции через указатель,
  который загрузчик получил из соответствующей библиотеки.
 */
void test_plugin_operations(void){
    Command commands[MAX_COMMANDS] = {0};

    int commands_count = load_plugins(
        PLUGIN_DIRECTORY,
        commands,
        MAX_COMMANDS
    );

    assert(commands_count == 4);

    Command *add = find_command(commands, commands_count, "add");
    Command *subtract = find_command(commands, commands_count, "subtract");
    Command *multiply = find_command(commands, commands_count, "multiply");
    Command *divide = find_command(commands, commands_count, "divide");

    assert(add != NULL);
    assert(subtract != NULL);
    assert(multiply != NULL);
    assert(divide != NULL);

    double result = 0.0;

    assert(add->function(8.0, 2.0, &result) == 1);
    assert(result == 10.0);

    assert(subtract->function(8.0, 2.0, &result) == 1);
    assert(result == 6.0);

    assert(multiply->function(8.0, 2.0, &result) == 1);
    assert(result == 16.0);

    assert(divide->function(8.0, 2.0, &result) == 1);
    assert(result == 4.0);

    unload_plugins(commands, commands_count);
}

/*
  Проверяет отказ при делении на ноль и при передаче нулевого
  указателя для сохранения результата.
 */
void test_plugin_errors(void){
    Command commands[MAX_COMMANDS] = {0};

    int commands_count = load_plugins(
        PLUGIN_DIRECTORY,
        commands,
        MAX_COMMANDS
    );

    assert(commands_count == 4);

    Command *add = find_command(commands, commands_count, "add");
    Command *subtract = find_command(commands, commands_count, "subtract");
    Command *multiply = find_command(commands, commands_count, "multiply");
    Command *divide = find_command(commands, commands_count, "divide");

    assert(add != NULL);
    assert(subtract != NULL);
    assert(multiply != NULL);
    assert(divide != NULL);

    double result = 123.0;

    assert(divide->function(8.0, 0.0, &result) == 0);
    assert(result == 123.0);

    assert(add->function(8.0, 2.0, NULL) == 0);
    assert(subtract->function(8.0, 2.0, NULL) == 0);
    assert(multiply->function(8.0, 2.0, NULL) == 0);
    assert(divide->function(8.0, 2.0, NULL) == 0);

    unload_plugins(commands, commands_count);
}

/*
  Проверяет ограничение размера массива и отказ загрузчика
  при неправильных аргументах.
 */
void test_loader_limits(void){
    Command commands[2] = {0};

    int commands_count = load_plugins(
        PLUGIN_DIRECTORY,
        commands,
        2
    );

    assert(commands_count == 2);

    unload_plugins(commands, commands_count);

    assert(load_plugins(NULL, commands, 2) == 0);
    assert(load_plugins(PLUGIN_DIRECTORY, NULL, 2) == 0);
    assert(load_plugins(PLUGIN_DIRECTORY, commands, 0) == 0);
}

/*
  Проверяет, что выгрузка закрывает библиотеки и очищает
  сохранённые названия, адреса функций и дескрипторы.
 */
void test_unload_plugins(void){
    Command commands[MAX_COMMANDS] = {0};

    int commands_count = load_plugins(
        PLUGIN_DIRECTORY,
        commands,
        MAX_COMMANDS
    );

    assert(commands_count == 4);

    unload_plugins(commands, commands_count);

    for (int index = 0; index < commands_count; index++) {
        assert(commands[index].name[0] == '\0');
        assert(commands[index].function == NULL);
        assert(commands[index].library_handle == NULL);
    }
}

int main(void){
    test_load_plugins();
    test_plugin_operations();
    test_plugin_errors();
    test_loader_limits();
    test_unload_plugins();

    printf("All tests passed\n");

    return 0;
}
