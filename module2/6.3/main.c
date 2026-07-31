#include <stdio.h>

#include "plugin_loader.h"

#define PLUGIN_DIRECTORY "./plugins"

int main(void){
    Command commands[MAX_COMMANDS] = {0};

    /*
      Состав команд заранее неизвестен: загрузчик формирует его
      из динамических библиотек, найденных в каталоге plugins.
     */
    int commands_count = load_plugins(
        PLUGIN_DIRECTORY,
        commands,
        MAX_COMMANDS
    );

    if (commands_count == 0) {
        printf("No plugins found\n");
        return 1;
    }

    int exit_status = 0;
    int choice = 0;

    double first = 0.0;
    double second = 0.0;
    double result = 0.0;

    while (1) {
        printf("\nCalculator\n");

        /* Выводим только те операции, которые удалось загрузить. */
        for (int index = 0; index < commands_count; index++) {
            printf("%d. %s\n", index + 1, commands[index].name);
        }

        printf("0. Exit\n");
        printf("Choose operation: ");

        if (scanf("%d", &choice) != 1) {
            printf("Input error\n");
            exit_status = 1;
            break;
        }

        if (choice == 0) {
            break;
        }

        if (choice < 1 || choice > commands_count) {
            printf("Unknown operation\n");
            continue;
        }

        printf("Enter first number: ");

        if (scanf("%lf", &first) != 1) {
            printf("Input error\n");
            exit_status = 1;
            break;
        }

        printf("Enter second number: ");

        if (scanf("%lf", &second) != 1) {
            printf("Input error\n");
            exit_status = 1;
            break;
        }

        int command_index = choice - 1;

        /* Вызываем функцию из выбранной динамической библиотеки. */
        int operation_success = commands[command_index].function(
            first,
            second,
            &result
        );

        if (operation_success) {
            printf("Operation completed successfully\n");
            printf("Result: %.10g\n", result);
        } else {
            printf("Operation failed\n");
        }
    }

    /* Перед завершением закрываем все открытые библиотеки. */
    unload_plugins(commands, commands_count);
    printf("Calculator closed\n");

    return exit_status;
}
