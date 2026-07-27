#include <stdio.h>
#include <stdlib.h>

#include "phonebook.h"

int main(void){
    /* root хранит адрес корневого узла; NULL обозначает пустое дерево. */
    ContactTreeNode *root = NULL;

    /* Количество успешных добавлений после последней балансировки. */
    int additions_since_balance = 0;

    /* Следующий свободный ID, который получит новый контакт. */
    unsigned int next_id = 1;

    int menu = 0;
    char command = ' ';

    /* Главное меню повторяется до выбора команды завершения. */
    while (1) {
        printf("\nMenu:\n");
        printf("1. Add contact\n");
        printf("2. Show contacts\n");
        printf("3. Delete contact\n");
        printf("4. Edit contact\n");
        printf("5. Search contact\n");
        printf("6. Load demo contacts\n");
        printf("0. Exit\n");
        printf("Choose: ");

        if (scanf("%d", &menu) != 1) {
            printf("Input error\n");
            free_contact_tree(&root);
            return 1;
        }

        /* Удаляем оставшуюся после scanf() часть строки, включая '\n'. */
        clear_input();
        switch (menu) {
            case 1:
                while (1) {
                    /*
                      Ввод заполняет временную структуру, но ещё
                      не добавляет контакт в телефонную книгу.
                     */
                    Contact new_contact = {0};

                    if (!input_contact(
                            &new_contact,
                            next_id)) {

                        break;
                    }

                    /* Создаём динамический узел дерева. */
                    ContactTreeNode *new_node =
                        create_contact_tree_node(
                            &new_contact
                        );

                    if (new_node == NULL) {
                        printf("Memory allocation error\n");
                        break;
                    }

                    /* Вставляем новый узел в бинарное дерево поиска. */
                    if (!insert_contact_tree(
                            &root,
                            new_node)) {

                        printf("Contact insertion error\n");

                        /*
                          Если вставка не выполнена, узел не принадлежит
                          дереву и его нужно освободить отдельно.
                         */
                        free(new_node);
                        break;
                    }

                    /* ID увеличивается только после успешного добавления. */
                    next_id++;

                    additions_since_balance++;

                    printf("Contact added\n");

                    /*
                      После каждых десяти успешных добавлений полностью
                      перестраиваем дерево в сбалансированном виде.
                     */
                    if (additions_since_balance >= 10) {
                        if (balance_contact_tree(&root)) {
                            additions_since_balance = 0;
                            printf("Tree balanced\n");
                        } else {
                            printf("Tree balancing error\n");
                        }
                    }

                    printf("Add another contact? y/n: ");

                    if (scanf(" %c", &command) != 1) {
                        printf("Input error\n");
                        clear_input();
                        break;
                    }

                    clear_input();

                    if (command == 'n') {
                        break;
                    }

                    if (command != 'y') {
                        printf("Unknown command\n");
                        break;
                    }
                }

                break;
            case 2:
                print_contacts_tree(root);
                break;
            case 3: {
                if (root == NULL) {
                    printf("No contacts\n");
                    break;
                }

                print_contacts_tree(root);

                unsigned int delete_id = 0;

                printf("Enter contact ID to delete: ");

                if (scanf("%u", &delete_id) != 1) {
                    printf("Input error\n");
                    clear_input();
                    break;
                }

                clear_input();

                /*
                  Сначала убеждаемся, что контакт с указанным ID
                  действительно существует в дереве.
                 */
                ContactTreeNode *node_to_delete =
                    find_contact_by_id(
                        root,
                        delete_id
                    );

                if (node_to_delete == NULL) {
                    printf("Contact not found\n");
                    break;
                }

                /*
                  Функция перестраивает необходимые связи дерева
                  и освобождает память удалённого узла.
                 */
                if (!delete_contact_by_id(
                        &root,
                        delete_id)) {

                    printf("Contact deletion error\n");
                    break;
                }

                printf("Contact deleted\n");

                break;
            }
            case 4: {
                if (root == NULL) {
                    printf("No contacts\n");
                    break;
                }

                print_contacts_tree(root);

                unsigned int edit_id = 0;

                printf("Enter contact ID to edit: ");

                if (scanf("%u", &edit_id) != 1) {
                    printf("Input error\n");
                    clear_input();
                    break;
                }

                clear_input();

                /* ID используется для выбора конкретного контакта. */
                ContactTreeNode *node_to_edit =
                    find_contact_by_id(
                        root,
                        edit_id
                    );

                if (node_to_edit == NULL) {
                    printf("Contact not found\n");
                    break;
                }

                /*
                  Сохраняем исходные данные и редактируем копию.
                  При ошибке ввода контакт в дереве не изменится.
                 */
                Contact original_contact =
                    node_to_edit->contact;

                Contact updated_contact =
                    original_contact;

                if (!edit_contact_data(
                        &updated_contact)) {

                    printf("Contact was not changed\n");
                    break;
                }

                /*
                  После изменения Ф.И.О. положение узла в дереве
                  может стать неправильным, поэтому отсоединяем его.
                 */
                ContactTreeNode *detached_node =
                    detach_contact_tree_node(
                        &root,
                        edit_id
                    );

                if (detached_node == NULL) {
                    printf("Contact detachment error\n");
                    break;
                }

                /* Записываем изменённые данные в отсоединённый узел. */
                detached_node->contact =
                    updated_contact;

                /* Вставляем узел обратно согласно изменённому Ф.И.О. */
                if (!insert_contact_tree(
                        &root,
                        detached_node)) {

                    /*
                      Если вставка изменённого контакта не удалась,
                      восстанавливаем исходные данные и положение.
                     */
                    detached_node->contact =
                        original_contact;

                    if (!insert_contact_tree(
                            &root,
                            detached_node)) {

                        printf("Contact restoration error\n");
                        free(detached_node);
                    } else {
                        printf("Contact insertion error\n");
                    }

                    break;
                }

                printf("Contact edited\n");

                break;
            }
            case 5: {
                if (root == NULL) {
                    printf("No contacts\n");
                    break;
                }

                char search_surname[MAX_TEXT] = {0};
                char search_name[MAX_TEXT] = {0};
                char search_patronomic[MAX_TEXT] = {0};

                /*
                  Пользователь может заполнить любое сочетание полей.
                  Пустая строка означает, что поле не участвует в поиске.
                 */
                printf("Enter surname, Enter = skip: ");

                if (!read_line(
                        search_surname,
                        sizeof(search_surname))) {

                    printf("Input error\n");
                    break;
                }

                printf("Enter name, Enter = skip: ");

                if (!read_line(
                        search_name,
                        sizeof(search_name))) {

                    printf("Input error\n");
                    break;
                }

                printf("Enter patronymic, Enter = skip: ");

                if (!read_line(
                        search_patronomic,
                        sizeof(search_patronomic))) {

                    printf("Input error\n");
                    break;
                }

                if (search_surname[0] == '\0' &&
                    search_name[0] == '\0' &&
                    search_patronomic[0] == '\0') {

                    printf("Enter at least one search field\n");
                    break;
                }

                /*
                  Сначала только считаем совпадения. Если они есть,
                  отдельная функция выполнит их вывод.
                 */
                int found_count =
                    count_matching_contacts(
                        root,
                        search_surname,
                        search_name,
                        search_patronomic
                    );

                if (found_count == 0) {
                    printf("Contacts not found\n");
                } else {
                    print_matching_contacts(
                        root,
                        search_surname,
                        search_name,
                        search_patronomic
                    );
                }

                break;
            }
            case 6: {
                /* Повторная загрузка не допускается, чтобы избежать дубликатов. */
                if (root != NULL) {
                    printf(
                        "Demo contacts can be loaded "
                        "only into an empty phonebook\n"
                    );

                    break;
                }

                int loaded_count =
                    load_demo_contacts(
                        &root,
                        &next_id
                    );

                if (loaded_count == 0) {
                    printf("Demo contacts loading error\n");
                    break;
                }

                additions_since_balance +=
                    loaded_count;

                printf(
                    "%d demo contacts loaded\n",
                    loaded_count
                );

                /*
                  Демонстрационные контакты также считаются добавлениями.
                  После загрузки десяти контактов балансируем дерево.
                 */
                if (additions_since_balance >= 10) {
                    if (balance_contact_tree(&root)) {
                        additions_since_balance = 0;
                        printf("Tree balanced\n");
                    } else {
                        printf("Tree balancing error\n");
                    }
                }

                break;
            }
            case 0:
                /* Перед завершением освобождаем все динамические узлы. */
                free_contact_tree(&root);
                return 0;

            default:
                printf("Unknown command\n");
                break;
        }
    }
    free_contact_tree(&root);
    return 0;
}
