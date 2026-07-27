#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "phonebook.h"

void clear_input(void){
    int c;

    /* getchar() извлекает и отбрасывает символы до Enter или конца потока. */
    while ((c = getchar()) != '\n' && c != EOF){
    }
}

int read_line(char text[], int size){
    if (fgets(text, size, stdin) == NULL){
        return 0;
    }

    /* Если Enter поместился в массив, заменяем '\n' на конец строки. */
    if (strchr(text, '\n') != NULL) {
        text[strcspn(text, "\n")] = '\0';
    }
    else {
        /*
          Если строка не поместилась, удаляем её оставшуюся часть
          из stdin, чтобы она не попала в следующий ввод.
         */
        clear_input();
    }

    return 1;
}

ContactTreeNode *create_contact_tree_node(const Contact *contact){
    if (contact == NULL) {
        return NULL;
    }

    /* Каждый контакт хранится в отдельном динамическом узле дерева. */
    ContactTreeNode *new_node =
        malloc(sizeof(ContactTreeNode));

    if (new_node == NULL) {
        return NULL;
    }

    /* Копируем всю структуру Contact, включая её строковые массивы. */
    new_node->contact = *contact;

    /* До вставки новый узел ещё не связан с другими узлами. */
    new_node->left = NULL;
    new_node->right = NULL;

    return new_node;
}

int compare_contacts(const Contact *first, const Contact *second){
    /*
      Контакты сортируются по фамилии, затем по имени и отчеству.
      ID определяет порядок контактов с полностью одинаковым Ф.И.О.
      strcmp() возвращает отрицательное число, ноль или положительное
      число в зависимости от порядка сравниваемых строк.
     */

    /* В первую очередь сравниваем фамилии. */
    int result = strcmp(first->surname, second->surname);

    if (result != 0) {
        return result;
    }
    /* Если фамилии одинаковые, определяем порядок по именам контактов. */
    result = strcmp(first->name, second->name);

    if (result != 0) {
        return result;
    }
    /* Если фамилии и имена одинаковые, сравниваем отчества. */
    result = strcmp(first->patronomic, second->patronomic);

    if (result != 0) {
        return result;
    }
    /*
      При полностью одинаковом Ф.И.О. используем ID,
      чтобы сохранить однозначный порядок контактов.
     */
    if (first->id < second->id) {
        return -1;
    }

    if (first->id > second->id) {
        return 1;
    }

    return 0;
}

int insert_contact_tree(ContactTreeNode **root, ContactTreeNode *new_node){
    if (root == NULL || new_node == NULL) {
        return 0;
    }

    /* Пустое место в дереве становится положением нового узла. */
    if (*root == NULL) {
        new_node->left = NULL;
        new_node->right = NULL;

        *root = new_node;

        return 1;
    }

    int compare_result =
        compare_contacts(
            &new_node->contact,
            &(*root)->contact
        );

    /* Меньший контакт рекурсивно вставляется в левое поддерево. */
    if (compare_result < 0) {
        return insert_contact_tree(
            &(*root)->left,
            new_node
        );
    }

    /* Больший контакт рекурсивно вставляется в правое поддерево. */
    if (compare_result > 0) {
        return insert_contact_tree(
            &(*root)->right,
            new_node
        );
    }

    /* Полностью одинаковый ключ повторно в дерево не вставляется. */
    return 0;
}

int input_contact(Contact *contact, unsigned int id){
    if (contact == NULL) {
        return 0;
    }

    /* ID назначается автоматически и не запрашивается у пользователя. */
    contact->id = id;

    /* Названия полей используются при выводе приглашений на ввод. */
    const char *field_names[] = {
        "name",
        "surname",
        "patronymic",
        "work",
        "position",
        "phone",
        "email",
        "social",
        "messenger"
    };

    /*
      Каждый элемент fields указывает на соответствующее строковое
      поле переданной структуры Contact.
     */
    char *fields[] = {
        contact->name,
        contact->surname,
        contact->patronomic,
        contact->work,
        contact->position,
        contact->phone,
        contact->email,
        contact->social,
        contact->messenger
    };

    /*
      Массив required имеет те же индексы, что field_names и fields:
      1 обозначает обязательное поле, 0 — необязательное.
     */
    const int required[] = {
        1, /* name */
        1, /* surname */
        0, /* patronymic */
        0, /* work */
        0, /* position */
        0, /* phone */
        0, /* email */
        0, /* social */
        0  /* messenger */
    };

    /* Количество полей вычисляется автоматически по размеру массива. */
    int fields_count =
        sizeof(fields) / sizeof(fields[0]);

    for (int i = 0; i < fields_count; i++) {
        /* Обязательное поле запрашивается повторно, пока оно пустое. */
        while (1) {
            if (required[i]) {
                printf(
                    "Enter %s (required): ",
                    field_names[i]
                );
            } else {
                printf(
                    "Enter %s (optional): ",
                    field_names[i]
                );
            }

            if (!read_line(fields[i], MAX_TEXT)) {
                printf("Input error\n");
                return 0;
            }

            if (required[i] &&
                fields[i][0] == '\0') {

                printf(
                    "%s cannot be empty\n",
                    field_names[i]
                );

                continue;
            }

            break;
        }
    }

    return 1;
}

void print_one_contact(const Contact *contact){
    if (contact == NULL) {
        return;
    }

    printf("ID: %u\n", contact->id);
    printf("Name: %s\n", contact->name);
    printf("Surname: %s\n", contact->surname);
    printf("Patronymic: %s\n", contact->patronomic);
    printf("Work: %s\n", contact->work);
    printf("Position: %s\n", contact->position);
    printf("Phone: %s\n", contact->phone);
    printf("Email: %s\n", contact->email);
    printf("Social: %s\n", contact->social);
    printf("Messenger: %s\n", contact->messenger);
}

static void print_contacts_in_order(
    const ContactTreeNode *root,
    int *contact_number){
    if (root == NULL || contact_number == NULL) {
        return;
    }

    /*
      Симметричный обход: левое поддерево, текущий узел, правое.
      Благодаря этому контакты выводятся в отсортированном порядке.
     */
    print_contacts_in_order(
        root->left,
        contact_number
    );

    printf(
        "\nContact %d\n",
        *contact_number
    );

    print_one_contact(&root->contact);

    (*contact_number)++;

    print_contacts_in_order(
        root->right,
        contact_number
    );
}

int print_contacts_tree(const ContactTreeNode *root){
    if (root == NULL) {
        printf("No contacts\n");
        return 0;
    }

    int contact_number = 1;

    printf("\nContacts:\n");

    print_contacts_in_order(
        root,
        &contact_number
    );

    return 1;
}

static int contact_matches(
    const Contact *contact,
    const char surname[],
    const char name[],
    const char patronomic[]){
    if (contact == NULL ||
        surname == NULL ||
        name == NULL ||
        patronomic == NULL) {

        return 0;
    }

    /*
      Пустой критерий не участвует в поиске. Заполненный критерий
      должен полностью совпасть с соответствующим полем контакта.
     */
    int surname_matches =
        surname[0] == '\0' ||
        strcmp(contact->surname, surname) == 0;

    int name_matches =
        name[0] == '\0' ||
        strcmp(contact->name, name) == 0;

    int patronomic_matches =
        patronomic[0] == '\0' ||
        strcmp(contact->patronomic, patronomic) == 0;

    return surname_matches &&
           name_matches &&
           patronomic_matches;
}

static int count_matching_contacts_in_order(
    const ContactTreeNode *root,
    const char surname[],
    const char name[],
    const char patronomic[]){
    if (root == NULL) {
        return 0;
    }

    /* Результат равен сумме совпадений слева, в узле и справа. */
    int current_matches =
        contact_matches(
            &root->contact,
            surname,
            name,
            patronomic
        );

    return count_matching_contacts_in_order(
               root->left,
               surname,
               name,
               patronomic
           ) +
           current_matches +
           count_matching_contacts_in_order(
               root->right,
               surname,
               name,
               patronomic
           );
}

int count_matching_contacts(
    const ContactTreeNode *root,
    const char surname[],
    const char name[],
    const char patronomic[]){
    if (surname == NULL ||
        name == NULL ||
        patronomic == NULL) {

        return 0;
    }

    if (surname[0] == '\0' &&
        name[0] == '\0' &&
        patronomic[0] == '\0') {

        return 0;
    }

    return count_matching_contacts_in_order(
        root,
        surname,
        name,
        patronomic
    );
}

static void print_matching_contacts_in_order(
    const ContactTreeNode *root,
    const char surname[],
    const char name[],
    const char patronomic[],
    int *found_count){
    if (root == NULL || found_count == NULL) {
        return;
    }

    /* Обходим дерево симметрично, чтобы результаты были отсортированы. */
    print_matching_contacts_in_order(
        root->left,
        surname,
        name,
        patronomic,
        found_count
    );

    if (contact_matches(
            &root->contact,
            surname,
            name,
            patronomic
        )) {

        (*found_count)++;

        printf(
            "\nFound contact %d\n",
            *found_count
        );

        print_one_contact(&root->contact);
    }

    print_matching_contacts_in_order(
        root->right,
        surname,
        name,
        patronomic,
        found_count
    );
}

void print_matching_contacts(
    const ContactTreeNode *root,
    const char surname[],
    const char name[],
    const char patronomic[]){
    if (surname == NULL ||
        name == NULL ||
        patronomic == NULL) {

        return;
    }

    if (surname[0] == '\0' &&
        name[0] == '\0' &&
        patronomic[0] == '\0') {

        return;
    }

    int found_count = 0;

    print_matching_contacts_in_order(
        root,
        surname,
        name,
        patronomic,
        &found_count
    );
}

ContactTreeNode *find_contact_by_id(ContactTreeNode *root, unsigned int id){
    if (root == NULL || id == 0) {
        return NULL;
    }

    if (root->contact.id == id) {
        return root;
    }

    /*
      Дерево упорядочено по Ф.И.О., а не по ID, поэтому по значению
      ID нельзя выбрать направление: приходится проверить обе ветви.
     */
    ContactTreeNode *found_node =
        find_contact_by_id(
            root->left,
            id
        );

    if (found_node != NULL) {
        return found_node;
    }

    return find_contact_by_id(
        root->right,
        id
    );
}

static ContactTreeNode *detach_smallest_node(ContactTreeNode **root){
    if (root == NULL || *root == NULL) {
        return NULL;
    }

    /* Самый левый узел является наименьшим в данном поддереве. */
    if ((*root)->left == NULL) {
        ContactTreeNode *smallest_node = *root;

        /* Его правый потомок занимает освободившееся место. */
        *root = smallest_node->right;

        smallest_node->left = NULL;
        smallest_node->right = NULL;

        return smallest_node;
    }

    return detach_smallest_node(
        &(*root)->left
    );
}

ContactTreeNode *detach_contact_tree_node(ContactTreeNode **root, unsigned int id){
    if (root == NULL ||
        *root == NULL ||
        id == 0) {

        return NULL;
    }

    if ((*root)->contact.id == id) {
        ContactTreeNode *removed_node = *root;

        /* Без левого потомка место узла занимает правое поддерево. */
        if (removed_node->left == NULL) {
            *root = removed_node->right;
        } else if (removed_node->right == NULL) {
            /* Без правого потомка место узла занимает левое поддерево. */
            *root = removed_node->left;
        } else {
            /*
              При двух потомках заменой становится наименьший узел
              правого поддерева, сохраняющий порядок дерева поиска.
             */
            ContactTreeNode *replacement_node =
                detach_smallest_node(
                    &removed_node->right
                );

            replacement_node->left =
                removed_node->left;

            replacement_node->right =
                removed_node->right;

            *root = replacement_node;
        }

        /* Возвращаемый узел полностью отделяется от дерева. */
        removed_node->left = NULL;
        removed_node->right = NULL;

        return removed_node;
    }

    ContactTreeNode *removed_node =
        detach_contact_tree_node(
            &(*root)->left,
            id
        );

    if (removed_node != NULL) {
        return removed_node;
    }

    return detach_contact_tree_node(
        &(*root)->right,
        id
    );
}

void free_contact_tree(ContactTreeNode **root){
    if (root == NULL || *root == NULL) {
        return;
    }

    /* Сначала освобождаем оба поддерева и только затем текущий узел. */
    free_contact_tree(
        &(*root)->left
    );

    free_contact_tree(
        &(*root)->right
    );

    free(*root);

    *root = NULL;
}

int edit_contact_data(Contact *contact){
    if (contact == NULL) {
        return 0;
    }

    char buffer[MAX_TEXT];

    /* Названия и адреса полей связаны одинаковыми индексами. */
    const char *field_names[] = {
        "name",
        "surname",
        "patronymic",
        "work",
        "position",
        "phone",
        "email",
        "social",
        "messenger"
    };

    char *fields[] = {
        contact->name,
        contact->surname,
        contact->patronomic,
        contact->work,
        contact->position,
        contact->phone,
        contact->email,
        contact->social,
        contact->messenger
    };

    int fields_count =
        sizeof(fields) / sizeof(fields[0]);

    for (int i = 0; i < fields_count; i++) {
        printf(
            "Current %s: %s\n",
            field_names[i],
            fields[i]
        );

        printf(
            "New %s, Enter = keep old: ",
            field_names[i]
        );

        if (!read_line(buffer, sizeof(buffer))) {
            printf("Input error\n");
            return 0;
        }

        /* Пустой Enter сохраняет старое значение поля. */
        if (buffer[0] != '\0') {
            strcpy(fields[i], buffer);
        }
    }

    return 1;
}

int load_demo_contacts(ContactTreeNode **root, unsigned int *next_id){
    if (root == NULL ||
        next_id == NULL ||
        *root != NULL) {

        return 0;
    }

    /*
      Массив используется только как источник демонстрационных данных.
      В телефонной книге контакты хранятся в узлах дерева.
     */
    const Contact demo_contacts[] = {
        {
            .name = "Petr",
            .surname = "Petrov",
            .patronomic = "Petrovich",
            .work = "Eltex",
            .position = "Engineer",
            .phone = "111111",
            .email = "petrov@example.com"
        },
        {
            .name = "Ivan",
            .surname = "Ivanov",
            .patronomic = "Ivanovich",
            .work = "Factory",
            .position = "Developer",
            .phone = "222222",
            .email = "ivanov@example.com"
        },
        {
            .name = "Sergey",
            .surname = "Sidorov",
            .patronomic = "Sergeevich",
            .phone = "333333"
        },
        {
            .name = "Anna",
            .surname = "Smirnova",
            .patronomic = "Andreevna",
            .email = "smirnova@example.com"
        },
        {
            .name = "Oleg",
            .surname = "Antonov",
            .patronomic = "Olegovich",
            .work = "Office"
        },
        {
            .name = "Maria",
            .surname = "Kuznetsova",
            .patronomic = "Pavlovna",
            .position = "Manager"
        },
        {
            .name = "Alexey",
            .surname = "Volkov",
            .patronomic = "Petrovich",
            .phone = "777777"
        },
        {
            .name = "Elena",
            .surname = "Fedorova",
            .patronomic = "Ivanovna",
            .email = "fedorova@example.com"
        },
        {
            .name = "Petr",
            .surname = "Ivanov",
            .patronomic = "Petrovich",
            .phone = "999999"
        },
        {
            .name = "Olga",
            .surname = "Alekseeva",
            .patronomic = "Sergeevna",
            .work = "University",
            .position = "Teacher"
        }
    };

    int contacts_count =
        sizeof(demo_contacts) /
        sizeof(demo_contacts[0]);

    /*
      Работаем с локальной копией ID. Настоящий next_id изменится
      только после успешной загрузки всех демонстрационных контактов.
     */
    unsigned int current_id = *next_id;

    for (int i = 0; i < contacts_count; i++) {
        Contact contact = demo_contacts[i];

        /* ID присваивается контакту до создания и вставки узла. */
        contact.id = current_id;

        ContactTreeNode *new_node =
            create_contact_tree_node(&contact);

        if (new_node == NULL) {
            /* При ошибке отменяем всю частично выполненную загрузку. */
            free_contact_tree(root);
            return 0;
        }

        if (!insert_contact_tree(
                root,
                new_node)) {

            free(new_node);
            free_contact_tree(root);
            return 0;
        }

        current_id++;
    }

    /* Сохраняем номер, который получит следующий новый контакт. */
    *next_id = current_id;

    return contacts_count;
}

int delete_contact_by_id(ContactTreeNode **root, unsigned int id){
    /* Отделяем поиск и изменение связей дерева от освобождения памяти. */
    ContactTreeNode *removed_node =
        detach_contact_tree_node(
            root,
            id
        );

    if (removed_node == NULL) {
        return 0;
    }

    free(removed_node);

    return 1;
}

int count_contact_tree_nodes(const ContactTreeNode *root){
    if (root == NULL) {
        return 0;
    }

    /* Учитываем текущий узел и рекурсивно считаем оба поддерева. */
    int left_count =
        count_contact_tree_nodes(
            root->left
        );

    int right_count =
        count_contact_tree_nodes(
            root->right
        );

    return 1 + left_count + right_count;
}

static void copy_contacts_to_array(
    const ContactTreeNode *root,
    Contact contacts[],
    int *index){
    if (root == NULL ||
        contacts == NULL ||
        index == NULL) {

        return;
    }

    /*
      Симметричный обход копирует контакты в массив уже
      в отсортированном порядке.
     */
    copy_contacts_to_array(
        root->left,
        contacts,
        index
    );

    contacts[*index] = root->contact;
    (*index)++;

    copy_contacts_to_array(
        root->right,
        contacts,
        index
    );
}

static ContactTreeNode *build_balanced_tree(
    const Contact contacts[],
    int first,
    int last,
    int *success){
    if (success == NULL) {
        return NULL;
    }

    if (contacts == NULL) {
        *success = 0;
        return NULL;
    }

    if (!*success || first > last) {
        return NULL;
    }

    /* Средний элемент становится корнем текущего поддерева. */
    int middle =
        first + (last - first) / 2;

    ContactTreeNode *new_root =
        create_contact_tree_node(
            &contacts[middle]
        );

    if (new_root == NULL) {
        *success = 0;
        return NULL;
    }

    /* Из левой половины массива строится левое поддерево. */
    new_root->left =
        build_balanced_tree(
            contacts,
            first,
            middle - 1,
            success
        );

    if (!*success) {
        free_contact_tree(&new_root);
        return NULL;
    }

    /* Из правой половины массива строится правое поддерево. */
    new_root->right =
        build_balanced_tree(
            contacts,
            middle + 1,
            last,
            success
        );

    if (!*success) {
        free_contact_tree(&new_root);
        return NULL;
    }

    return new_root;
}

int balance_contact_tree(ContactTreeNode **root){
    if (root == NULL) {
        return 0;
    }

    /* Размер дерева определяет размер временного массива контактов. */
    int contacts_count =
        count_contact_tree_nodes(*root);

    if (contacts_count < 2) {
        return 1;
    }

    Contact *contacts =
        malloc(
            sizeof(Contact) *
            (size_t)contacts_count
        );

    if (contacts == NULL) {
        return 0;
    }

    int index = 0;

    /* Получаем отсортированный массив, не изменяя старое дерево. */
    copy_contacts_to_array(
        *root,
        contacts,
        &index
    );

    int success = 1;

    /* По отсортированному массиву создаём новое сбалансированное дерево. */
    ContactTreeNode *new_root =
        build_balanced_tree(
            contacts,
            0,
            contacts_count - 1,
            &success
        );

    free(contacts);

    if (!success || new_root == NULL) {
        free_contact_tree(&new_root);
        return 0;
    }

    /*
      Старое дерево освобождается только после успешного построения
      нового, поэтому при ошибке исходные контакты не теряются.
     */
    free_contact_tree(root);

    *root = new_root;

    return 1;
}
