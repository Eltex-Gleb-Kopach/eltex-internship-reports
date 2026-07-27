#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "../phonebook.h"

/*
  Создаёт структуру контакта с минимальным набором данных.
  Остальные поля благодаря инициализации нулями остаются пустыми.
 */
static Contact make_contact(
    unsigned int id,
    const char name[],
    const char surname[],
    const char patronomic[]){

    Contact contact = {0};

    contact.id = id;
    strcpy(contact.name, name);
    strcpy(contact.surname, surname);
    strcpy(contact.patronomic, patronomic);

    return contact;
}

/*
  Создаёт узел и вставляет его в дерево.
  Вспомогательная функция сокращает повторяющийся код тестов.
 */
static ContactTreeNode *add_test_contact(
    ContactTreeNode **root,
    unsigned int id,
    const char name[],
    const char surname[]){

    Contact contact =
        make_contact(
            id,
            name,
            surname,
            ""
        );

    ContactTreeNode *new_node =
        create_contact_tree_node(&contact);

    assert(new_node != NULL);
    assert(insert_contact_tree(root, new_node) == 1);

    return new_node;
}

/* Возвращает высоту дерева для проверки результата балансировки. */
static int tree_height(
    const ContactTreeNode *root){
    if (root == NULL) {
        return 0;
    }

    int left_height =
        tree_height(root->left);

    int right_height =
        tree_height(root->right);

    if (left_height > right_height) {
        return left_height + 1;
    }

    return right_height + 1;
}

/*
  Проверяет, что каждый узел расположен между допустимыми
  минимальной и максимальной границами бинарного дерева поиска.
 */
static int tree_is_ordered(
    const ContactTreeNode *root,
    const Contact *minimum,
    const Contact *maximum){

    if (root == NULL) {
        return 1;
    }

    if (minimum != NULL &&
        compare_contacts(
            &root->contact,
            minimum
        ) <= 0) {

        return 0;
    }

    if (maximum != NULL &&
        compare_contacts(
            &root->contact,
            maximum
        ) >= 0) {

        return 0;
    }

    return tree_is_ordered(
               root->left,
               minimum,
               &root->contact
           ) &&
           tree_is_ordered(
               root->right,
               &root->contact,
               maximum
           );
}

/* Проверяет создание и вставку первого узла в пустое дерево. */
static void test_insert_into_empty_tree(void){

    Contact contact =
        make_contact(
            1,
            "Ivan",
            "Ivanov",
            "Ivanovich"
        );

    ContactTreeNode *root = NULL;

    ContactTreeNode *new_node =
        create_contact_tree_node(&contact);

    assert(new_node != NULL);
    assert(new_node->left == NULL);
    assert(new_node->right == NULL);

    assert(
        insert_contact_tree(
            &root,
            new_node
        ) == 1
    );

    assert(root == new_node);
    assert(root->contact.id == 1);
    assert(count_contact_tree_nodes(root) == 1);

    free_contact_tree(&root);

    assert(root == NULL);
}

/*
  Проверяет, что меньший контакт попадает в левое поддерево,
  а больший — в правое.
 */
static void test_tree_order(void){

    ContactTreeNode *root = NULL;

    ContactTreeNode *petrov =
        add_test_contact(
            &root,
            1,
            "Petr",
            "Petrov"
        );

    ContactTreeNode *ivanov =
        add_test_contact(
            &root,
            2,
            "Ivan",
            "Ivanov"
        );

    ContactTreeNode *sidorov =
        add_test_contact(
            &root,
            3,
            "Sergey",
            "Sidorov"
        );

    assert(root == petrov);
    assert(root->left == ivanov);
    assert(root->right == sidorov);
    assert(count_contact_tree_nodes(root) == 3);
    assert(tree_is_ordered(root, NULL, NULL));

    free_contact_tree(&root);
}

/* Проверяет поиск существующего и отсутствующего ID. */
static void test_find_by_id(void){

    ContactTreeNode *root = NULL;

    add_test_contact(&root, 1, "Petr", "Petrov");

    ContactTreeNode *ivanov =
        add_test_contact(
            &root,
            2,
            "Ivan",
            "Ivanov"
        );

    add_test_contact(&root, 3, "Sergey", "Sidorov");

    assert(find_contact_by_id(root, 2) == ivanov);
    assert(find_contact_by_id(root, 100) == NULL);
    assert(find_contact_by_id(root, 0) == NULL);

    free_contact_tree(&root);
}

/*
  Проверяет поиск по одному полю, сочетанию полей,
  отсутствие совпадений и отказ при пустых критериях.
 */
static void test_search_by_name_fields(void){

    ContactTreeNode *root = NULL;

    add_test_contact(&root, 1, "Ivan", "Ivanov");
    add_test_contact(&root, 2, "Petr", "Ivanov");
    add_test_contact(&root, 3, "Ivan", "Petrov");

    assert(
        count_matching_contacts(
            root,
            "Ivanov",
            "",
            ""
        ) == 2
    );

    assert(
        count_matching_contacts(
            root,
            "Ivanov",
            "Petr",
            ""
        ) == 1
    );

    assert(
        count_matching_contacts(
            root,
            "Sidorov",
            "",
            ""
        ) == 0
    );

    assert(
        count_matching_contacts(
            root,
            "",
            "",
            ""
        ) == 0
    );

    free_contact_tree(&root);
}

/* Проверяет удаление листа — узла без потомков. */
static void test_delete_leaf(void){

    ContactTreeNode *root = NULL;

    add_test_contact(&root, 1, "Petr", "Petrov");
    add_test_contact(&root, 2, "Ivan", "Ivanov");
    add_test_contact(&root, 3, "Sergey", "Sidorov");

    assert(delete_contact_by_id(&root, 2) == 1);
    assert(find_contact_by_id(root, 2) == NULL);
    assert(count_contact_tree_nodes(root) == 2);
    assert(tree_is_ordered(root, NULL, NULL));

    free_contact_tree(&root);
}

/*
  Проверяет удаление узла с одним потомком:
  потомок должен занять место удалённого узла.
 */
static void test_delete_node_with_one_child(void){

    ContactTreeNode *root = NULL;

    add_test_contact(&root, 1, "Petr", "Petrov");
    add_test_contact(&root, 2, "Ivan", "Ivanov");

    ContactTreeNode *antonov =
        add_test_contact(
            &root,
            3,
            "Oleg",
            "Antonov"
        );

    assert(delete_contact_by_id(&root, 2) == 1);
    assert(root->left == antonov);
    assert(find_contact_by_id(root, 2) == NULL);
    assert(count_contact_tree_nodes(root) == 2);
    assert(tree_is_ordered(root, NULL, NULL));

    free_contact_tree(&root);
}

/*
  Проверяет удаление корня с двумя потомками.
  Его место занимает наименьший узел правого поддерева.
 */
static void test_delete_node_with_two_children(void){

    ContactTreeNode *root = NULL;

    add_test_contact(&root, 1, "Petr", "Petrov");

    ContactTreeNode *ivanov =
        add_test_contact(
            &root,
            2,
            "Ivan",
            "Ivanov"
        );

    ContactTreeNode *volkov =
        add_test_contact(
            &root,
            3,
            "Alexey",
            "Volkov"
        );

    ContactTreeNode *sidorov =
        add_test_contact(
            &root,
            4,
            "Sergey",
            "Sidorov"
        );

    assert(delete_contact_by_id(&root, 1) == 1);

    assert(root == sidorov);
    assert(root->left == ivanov);
    assert(root->right == volkov);
    assert(find_contact_by_id(root, 1) == NULL);
    assert(count_contact_tree_nodes(root) == 3);
    assert(tree_is_ordered(root, NULL, NULL));

    free_contact_tree(&root);
}

/*
  Проверяет перемещение узла после изменения фамилии:
  узел отсоединяется и повторно вставляется в новое место.
 */
static void test_detach_edit_and_insert_again(void){

    ContactTreeNode *root = NULL;

    add_test_contact(&root, 1, "Petr", "Petrov");
    add_test_contact(&root, 2, "Ivan", "Ivanov");

    ContactTreeNode *detached_node =
        detach_contact_tree_node(
            &root,
            2
        );

    assert(detached_node != NULL);
    assert(detached_node->left == NULL);
    assert(detached_node->right == NULL);
    assert(find_contact_by_id(root, 2) == NULL);

    strcpy(
        detached_node->contact.surname,
        "Zaitsev"
    );

    assert(
        insert_contact_tree(
            &root,
            detached_node
        ) == 1
    );

    assert(root->right == detached_node);
    assert(find_contact_by_id(root, 2) == detached_node);
    assert(tree_is_ordered(root, NULL, NULL));

    free_contact_tree(&root);
}

/* Проверяет загрузку десяти заготовленных контактов и выдачу ID. */
static void test_load_demo_contacts(void){

    ContactTreeNode *root = NULL;
    unsigned int next_id = 1;

    int loaded_count =
        load_demo_contacts(
            &root,
            &next_id
        );

    assert(loaded_count == 10);
    assert(count_contact_tree_nodes(root) == 10);
    assert(next_id == 11);
    assert(find_contact_by_id(root, 1) != NULL);
    assert(find_contact_by_id(root, 10) != NULL);
    assert(tree_is_ordered(root, NULL, NULL));

    /* Загрузить демонстрационные данные в непустое дерево нельзя. */
    assert(
        load_demo_contacts(
            &root,
            &next_id
        ) == 0
    );

    free_contact_tree(&root);
}

/*
  Создаёт вытянутое дерево, балансирует его и проверяет,
  что высота уменьшилась, а все контакты сохранились.
 */
static void test_balance_tree(void){

    ContactTreeNode *root = NULL;

    const char *surnames[] = {
        "A",
        "B",
        "C",
        "D",
        "E",
        "F",
        "G"
    };

    int contacts_count =
        sizeof(surnames) /
        sizeof(surnames[0]);

    for (int index = 0;
         index < contacts_count;
         index++) {

        add_test_contact(
            &root,
            (unsigned int)index + 1,
            "Name",
            surnames[index]
        );
    }

    assert(tree_height(root) == 7);
    assert(count_contact_tree_nodes(root) == 7);

    assert(balance_contact_tree(&root) == 1);

    assert(tree_height(root) == 3);
    assert(count_contact_tree_nodes(root) == 7);
    assert(tree_is_ordered(root, NULL, NULL));

    for (unsigned int id = 1; id <= 7; id++) {
        assert(find_contact_by_id(root, id) != NULL);
    }

    free_contact_tree(&root);

    assert(root == NULL);
}

int main(void){
    test_insert_into_empty_tree();
    test_tree_order();
    test_find_by_id();
    test_search_by_name_fields();

    test_delete_leaf();
    test_delete_node_with_one_child();
    test_delete_node_with_two_children();
    test_detach_edit_and_insert_again();

    test_load_demo_contacts();
    test_balance_tree();

    printf("All tests passed\n");

    return 0;
}
