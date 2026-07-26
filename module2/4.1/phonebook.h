#ifndef PHONEBOOK_H
#define PHONEBOOK_H

#define MAX_TEXT 128

typedef struct {
    unsigned int id;
    char name[MAX_TEXT];
    char surname[MAX_TEXT];
    char patronomic[MAX_TEXT];
    char work[MAX_TEXT];
    char position[MAX_TEXT];
    char phone[MAX_TEXT];
    char email[MAX_TEXT];
    char social[MAX_TEXT];
    char messenger[MAX_TEXT];
} Contact;

typedef struct ContactNode {
    Contact contact;
    struct ContactNode *prev;
    struct ContactNode *next;
} ContactNode;

/*Опишу прототипы функций и короткое описание их*/

/* Читает строку и удаляет символ перехода на новую строку. */
int read_line(char text[], int size);
/* Очищает оставшиеся символы из потока ввода. */
void clear_input(void);


/* Создаёт новый узел с копией переданного контакта. */
ContactNode *create_contact_node(const Contact *contact);

/* Сравнивает контакты для определения порядка сортировки. */
int compare_contacts(const Contact *first, const Contact *second);

/* Вставляет узел в правильное место упорядоченного списка. */
int insert_contact_sorted(ContactNode **head, ContactNode *new_node);

/* Запрашивает у пользователя данные нового контакта. */
int input_contact(Contact *contact, unsigned int id);

/* Выводит один контакт при просмотре книги или результатов поиска. */
void print_one_contact(const Contact *contact);

/* Обходит весь список и выводит каждый контакт через print_one_contact */
int print_contacts(const ContactNode *head);

/* Ищет первое совпадение по заданным полям Ф.И.О. */
ContactNode *find_contact(ContactNode *start_node, const char surname[], const char name[], const char patronomic[]);

/* Находит узел по ID для последующего удаления или редактирования. */
ContactNode *find_contact_by_id(ContactNode *head, unsigned int id);

/* Отсоединяет узел от списка, не освобождая его память. */
int detach_contact_node(ContactNode **head, ContactNode *node);

/* Удаляет узел из списка и освобождает его память. */
int delete_contact_node(ContactNode **head, ContactNode *node);

/* Освобождает память всех узлов и обнуляет голову списка. */
void free_contact_list(ContactNode **head);

/* Редактирует данные контакта, сохраняя его ID. */
int edit_contact_data(Contact *contact);

/* Загружает десять контактов для ручной проверки программы. */
int load_demo_contacts(ContactNode **head, unsigned int *next_id);

#endif
