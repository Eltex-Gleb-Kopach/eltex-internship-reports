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

ContactNode *create_contact_node(const Contact *contact){
    if (contact == NULL) {
        return NULL;
    }

    ContactNode *new_node = malloc(sizeof(ContactNode));

    if (new_node == NULL) {
        return NULL;
    }

    new_node->contact = *contact;

    new_node->prev = NULL;
    new_node->next = NULL;

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

int insert_contact_sorted(ContactNode **head, ContactNode *new_node){
    if (head == NULL || new_node == NULL) {
        return 0;
    }
    /*
      Перед вставкой сбрасываем связи узла:
      prev и next устанавливаются в NULL.
     */
    new_node->prev = NULL;
    new_node->next = NULL;
    /* В пустом списке новый узел сразу становится головой. */
    if (*head == NULL) {
        *head = new_node;
        return 1;
    }
    /*
      Если новый контакт должен находиться раньше текущей головы,
      вставляем его в начало и переносим указатель head.
     */
    if (compare_contacts(
            &new_node->contact,
            &(*head)->contact) < 0) {

        new_node->next = *head;
        (*head)->prev = new_node;
        *head = new_node;

        return 1;
    }
    /*
      current_node используется для обхода списка
      и поиска места, после которого нужно вставить новый узел.
     */
    ContactNode *current_node = *head;
    /*
      Двигаемся вперёд, пока следующий контакт должен находиться
      раньше нового контакта или имеет такой же порядок.
     */
    while (current_node->next != NULL &&
           compare_contacts(
               &current_node->next->contact,
               &new_node->contact) <= 0) {

        current_node = current_node->next;
    }
    /*
      Сохраняем адреса элементов, между которыми будет вставлен узел:
      предыдущий — current_node, следующий — current_node->next.
     */
    new_node->prev = current_node;
    new_node->next = current_node->next;
    /*
      Если справа существует узел, меняем его обратную связь,
      чтобы он указывал назад на новый элемент.
     */
    if (current_node->next != NULL) {
        current_node->next->prev = new_node;
    }
    /* Предыдущий элемент теперь указывает вперёд на новый узел. */
    current_node->next = new_node;

    return 1;
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

int print_contacts(const ContactNode *head){
    if (head == NULL) {
        printf("No contacts\n");
        return 0;
    }

    /* Временный указатель движется по списку, не изменяя head. */
    const ContactNode *current_node = head;
    int contact_number = 1;

    printf("\nContacts:\n");

    while (current_node != NULL) {
        printf("\nContact %d\n", contact_number);

        print_one_contact(&current_node->contact);

        current_node = current_node->next;
        contact_number++;
    }

    return 1;
}

ContactNode *find_contact(ContactNode *start_node, const char surname[], const char name[], const char patronomic[]){
    if (surname == NULL ||
        name == NULL ||
        patronomic == NULL) {

        return NULL;
    }

    if (surname[0] == '\0' &&
        name[0] == '\0' &&
        patronomic[0] == '\0') {

        /* Поиск без единого заполненного критерия не выполняется. */
        return NULL;
    }

    ContactNode *current_node = start_node;

    while (current_node != NULL) {
        /*
          Пустой критерий пропускается, а заполненный должен точно
          совпасть с соответствующим полем текущего контакта.
         */
        int surname_matches =
            surname[0] == '\0' ||
            strcmp(
                current_node->contact.surname,
                surname
            ) == 0;

        int name_matches =
            name[0] == '\0' ||
            strcmp(
                current_node->contact.name,
                name
            ) == 0;

        int patronomic_matches =
            patronomic[0] == '\0' ||
            strcmp(
                current_node->contact.patronomic,
                patronomic
            ) == 0;

        if (surname_matches &&
            name_matches &&
            patronomic_matches) {

            /* Возвращаем первое совпадение начиная со start_node. */
            return current_node;
        }

        current_node = current_node->next;
    }

    return NULL;
}

ContactNode *find_contact_by_id(ContactNode *head, unsigned int id){
    if (id == 0) {
        return NULL;
    }

    ContactNode *current_node = head;

    /* ID уникален, поэтому достаточно вернуть первое совпадение. */
    while (current_node != NULL) {
        if (current_node->contact.id == id) {
            return current_node;
        }

        current_node = current_node->next;
    }

    return NULL;
}

int detach_contact_node(ContactNode **head, ContactNode *node){
    if (head == NULL ||
        *head == NULL ||
        node == NULL) {

        return 0;
    }

    ContactNode *current_node = *head;

    /* Проверяем, что переданный узел действительно принадлежит списку. */
    while (current_node != NULL &&
           current_node != node) {

        current_node = current_node->next;
    }

    if (current_node == NULL) {
        return 0;
    }

    /*
      Предыдущий элемент начинает указывать на следующий.
      Если предыдущего элемента нет, отсоединяется голова списка.
     */
    if (node->prev != NULL) {
        node->prev->next = node->next;
    } else {
        *head = node->next;
    }

    /* Следующий элемент начинает указывать на предыдущий. */
    if (node->next != NULL) {
        node->next->prev = node->prev;
    }

    /* Отсоединённый узел остаётся в памяти, а его связи сбрасываются. */
    node->prev = NULL;
    node->next = NULL;

    return 1;
}

int delete_contact_node(ContactNode **head, ContactNode *node){
    /* Память освобождается только после успешного отсоединения узла. */
    if (!detach_contact_node(head, node)) {
        return 0;
    }

    free(node);

    return 1;
}

void free_contact_list(ContactNode **head){
    if (head == NULL) {
        return;
    }

    ContactNode *current_node = *head;

    while (current_node != NULL) {
        /*
          Адрес следующего узла сохраняется до free(), потому что после
          освобождения обращаться к current_node->next уже нельзя.
         */
        ContactNode *next_node =
            current_node->next;

        free(current_node);

        current_node = next_node;
    }

    *head = NULL;
}

int edit_contact_data(Contact *contact)
{
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

int load_demo_contacts(ContactNode **head, unsigned int *next_id){
    if (head == NULL ||
        next_id == NULL ||
        *head != NULL) {

        return 0;
    }

    /*
      Массив используется только как источник демонстрационных данных.
      В телефонной книге контакты всё равно хранятся в узлах списка.
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

        ContactNode *new_node =
            create_contact_node(&contact);

        if (new_node == NULL) {
            /* При ошибке отменяем частично выполненную загрузку. */
            free_contact_list(head);
            return 0;
        }

        if (!insert_contact_sorted(
                head,
                new_node)) {

            free(new_node);
            free_contact_list(head);
            return 0;
        }

        current_id++;
    }

    /* Сохраняем номер, который получит следующий новый контакт. */
    *next_id = current_id;

    return contacts_count;
}
