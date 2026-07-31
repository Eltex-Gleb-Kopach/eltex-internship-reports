#include <stdio.h>
#include <stdlib.h>

#include "phonebook.h"

int main(void){
    /* head хранит адрес первого узла; NULL обозначает пустой список. */
    ContactNode *head = NULL;

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
            free_contact_list(&head);
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

                    /* Создаём динамический узел с копией введённых данных. */
                    ContactNode *new_node =
                        create_contact_node(&new_contact);

                    if (new_node == NULL) {
                        printf("Memory allocation error\n");
                        break;
                    }

                    /* Передаём узел функции упорядоченной вставки. */
                    if (!insert_contact_sorted(
                            &head,
                            new_node)) {

                        printf("Contact insertion error\n");

                        /*
                          При ошибке вставки узел не принадлежит списку,
                          поэтому его память освобождается здесь.
                         */
                        free(new_node);
                        break;
                    }

                    /* ID увеличивается только после успешного добавления. */
                    next_id++;

                    printf("Contact added\n");

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
                print_contacts(head);
                break;
            case 3: {
                if (head == NULL) {
                    printf("No contacts\n");
                    break;
                }

                print_contacts(head);

                unsigned int delete_id = 0;

                printf("Enter contact ID to delete: ");

                if (scanf("%u", &delete_id) != 1) {
                    printf("Input error\n");
                    clear_input();
                    break;
                }

                clear_input();

                /* ID однозначно определяет удаляемый узел. */
                ContactNode *node_to_delete =
                    find_contact_by_id(head, delete_id);

                if (node_to_delete == NULL) {
                    printf("Contact not found\n");
                    break;
                }

                /* Функция удаляет узел из списка и освобождает его память. */
                if (!delete_contact_node(
                        &head,
                        node_to_delete)) {

                    printf("Contact deletion error\n");
                    break;
                }

                printf("Contact deleted\n");

                break;
            }
            case 4: {
                if (head == NULL) {
                    printf("No contacts\n");
                    break;
                }

                print_contacts(head);

                unsigned int edit_id = 0;

                printf("Enter contact ID to edit: ");

                if (scanf("%u", &edit_id) != 1) {
                    printf("Input error\n");
                    clear_input();
                    break;
                }

                clear_input();

                /* ID используется для выбора конкретного изменяемого узла. */
                ContactNode *node_to_edit =
                    find_contact_by_id(head, edit_id);

                if (node_to_edit == NULL) {
                    printf("Contact not found\n");
                    break;
                }

                /*
                  Сначала редактируем копию. При ошибке ввода исходный
                  контакт внутри узла останется без изменений.
                 */
                Contact updated_contact =
                    node_to_edit->contact;

                if (!edit_contact_data(
                        &updated_contact)) {

                    printf("Contact was not changed\n");
                    break;
                }

                /*
                  После изменения Ф.И.О. старое положение узла может
                  нарушать сортировку, поэтому отсоединяем его от списка.
                 */
                if (!detach_contact_node(
                        &head,
                        node_to_edit)) {

                    printf("Contact detachment error\n");
                    break;
                }

                /* Применяем проверенные изменения к отсоединённому узлу. */
                node_to_edit->contact =
                    updated_contact;

                /* Повторно вставляем узел в правильное место списка. */
                if (!insert_contact_sorted(
                        &head,
                        node_to_edit)) {

                    printf("Contact insertion error\n");
                    free(node_to_edit);
                    break;
                }

                printf("Contact edited\n");

                break;
            }
            case 5: {
                if (head == NULL) {
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

                ContactNode *found_node =
                    find_contact(
                        head,
                        search_surname,
                        search_name,
                        search_patronomic
                    );

                int found_count = 0;

                /*
                  find_contact() возвращает одно совпадение, поэтому
                  следующий поиск начинается с found_node->next.
                 */
                while (found_node != NULL) {
                    found_count++;

                    printf(
                        "\nFound contact %d\n",
                        found_count
                    );

                    print_one_contact(
                        &found_node->contact
                    );

                    found_node =
                        find_contact(
                            found_node->next,
                            search_surname,
                            search_name,
                            search_patronomic
                        );
                }

                if (found_count == 0) {
                    printf("Contacts not found\n");
                }

                break;
            }
            case 6: {
                /* Повторная загрузка не допускается, чтобы избежать дубликатов. */
                if (head != NULL) {
                    printf(
                        "Demo contacts can be loaded "
                        "only into an empty phonebook\n"
                    );

                    break;
                }

                int loaded_count =
                    load_demo_contacts(
                        &head,
                        &next_id
                    );

                if (loaded_count == 0) {
                    printf("Demo contacts loading error\n");
                    break;
                }

                printf(
                    "%d demo contacts loaded\n",
                    loaded_count
                );

                break;
            }
            case 0:
                /* Перед завершением освобождаем все динамические узлы. */
                free_contact_list(&head);
                return 0;

            default:
                printf("Unknown command\n");
                break;
        }
    }
    free_contact_list(&head);
    return 0;
}
