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

/*
  Узел бинарного дерева хранит контакт
  и адреса левого и правого потомков.
 */
typedef struct ContactTreeNode {
    Contact contact;
    struct ContactTreeNode *left;
    struct ContactTreeNode *right;
} ContactTreeNode;

/* Читает строку и удаляет символ перехода на новую строку. */
int read_line(char text[], int size);

/* Очищает оставшиеся символы из потока ввода. */
void clear_input(void);

/* Сравнивает контакты для определения их порядка в дереве. */
int compare_contacts(const Contact *first, const Contact *second);

/* Создаёт новый узел дерева с копией переданного контакта. */
ContactTreeNode *create_contact_tree_node(const Contact *contact);

/* Вставляет новый узел в бинарное дерево поиска. */
int insert_contact_tree(ContactTreeNode **root, ContactTreeNode *new_node);

/* Запрашивает у пользователя данные нового контакта. */
int input_contact(Contact *contact, unsigned int id);

/* Выводит данные одного контакта. */
void print_one_contact(const Contact *contact);

/* Выводит все контакты симметричным обходом дерева. */
int print_contacts_tree(const ContactTreeNode *root);

/* Ищет в дереве контакт по уникальному ID. */
ContactTreeNode *find_contact_by_id(ContactTreeNode *root, unsigned int id);

/* Редактирует данные контакта, сохраняя его ID. */
int edit_contact_data(Contact *contact);

/* Возвращает количество контактов, подходящих под заданное Ф.И.О. */
int count_matching_contacts(
    const ContactTreeNode *root,
    const char surname[],
    const char name[],
    const char patronomic[]);

/* Выводит контакты, подходящие под заданное Ф.И.О. */
void print_matching_contacts(
    const ContactTreeNode *root,
    const char surname[],
    const char name[],
    const char patronomic[]);

/* Отсоединяет от дерева и возвращает узел с указанным ID. */
ContactTreeNode *detach_contact_tree_node(ContactTreeNode **root, unsigned int id);

/* Удаляет из дерева контакт с указанным ID. */
int delete_contact_by_id(ContactTreeNode **root, unsigned int id);

/* Освобождает память всех узлов дерева. */
void free_contact_tree(ContactTreeNode **root);

/* Загружает в пустое дерево десять демонстрационных контактов. */
int load_demo_contacts(ContactTreeNode **root, unsigned int *next_id);

/* Возвращает количество узлов в дереве контактов. */
int count_contact_tree_nodes(const ContactTreeNode *root);

/* Полностью перестраивает дерево в сбалансированном виде. */
int balance_contact_tree(ContactTreeNode **root);

#endif
