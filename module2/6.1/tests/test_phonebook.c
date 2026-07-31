#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "../phonebook.h"

/*
  Проверяет создание и вставку единственного узла в пустой список,
  а также корректное освобождение списка.
 */
void test_insert_into_empty_list(void)
{
    Contact contact = {
        .id = 1,
        .name = "Ivan",
        .surname = "Ivanov"
    };

    ContactNode *head = NULL;

    ContactNode *new_node =
        create_contact_node(&contact);

    assert(new_node != NULL);

    int result =
        insert_contact_sorted(
            &head,
            new_node
        );

    assert(result == 1);

    assert(head == new_node);
    assert(head->contact.id == 1);
    assert(head->prev == NULL);
    assert(head->next == NULL);

    free_contact_list(&head);

    assert(head == NULL);
}

/*
  Добавляет контакты не по алфавиту и проверяет итоговый порядок,
  прямые связи next и обратные связи prev.
 */
void test_sorted_insertion(void)
{
    Contact petrov = {
        .id = 1,
        .name = "Petr",
        .surname = "Petrov"
    };

    Contact antonov = {
        .id = 2,
        .name = "Anton",
        .surname = "Antonov"
    };

    Contact sidorov = {
        .id = 3,
        .name = "Sergey",
        .surname = "Sidorov"
    };

    Contact ivanov = {
        .id = 4,
        .name = "Ivan",
        .surname = "Ivanov"
    };

    ContactNode *head = NULL;

    ContactNode *petrov_node =
        create_contact_node(&petrov);

    ContactNode *antonov_node =
        create_contact_node(&antonov);

    ContactNode *sidorov_node =
        create_contact_node(&sidorov);

    ContactNode *ivanov_node =
        create_contact_node(&ivanov);

    assert(petrov_node != NULL);
    assert(antonov_node != NULL);
    assert(sidorov_node != NULL);
    assert(ivanov_node != NULL);

    /*
      Вставки последовательно проверяют пустой список, начало,
      конец и середину упорядоченного списка.
     */
    assert(
        insert_contact_sorted(
            &head,
            petrov_node
        ) == 1
    );

    assert(
        insert_contact_sorted(
            &head,
            antonov_node
        ) == 1
    );

    assert(
        insert_contact_sorted(
            &head,
            sidorov_node
        ) == 1
    );

    assert(
        insert_contact_sorted(
            &head,
            ivanov_node
        ) == 1
    );

    /* Ожидаемый порядок: Antonov, Ivanov, Petrov, Sidorov. */
    assert(head == antonov_node);

    assert(antonov_node->prev == NULL);
    assert(antonov_node->next == ivanov_node);

    assert(ivanov_node->prev == antonov_node);
    assert(ivanov_node->next == petrov_node);

    assert(petrov_node->prev == ivanov_node);
    assert(petrov_node->next == sidorov_node);

    assert(sidorov_node->prev == petrov_node);
    assert(sidorov_node->next == NULL);

    free_contact_list(&head);

    assert(head == NULL);
}

/*
  Проверяет поиск по одному или нескольким полям Ф.И.О.,
  несколько совпадений, отсутствие результата и пустые критерии.
 */
void test_find_contacts(void)
{
    Contact first_contact = {
        .id = 1,
        .name = "Ivan",
        .surname = "Ivanov",
        .patronomic = "Ivanovich"
    };

    Contact second_contact = {
        .id = 2,
        .name = "Petr",
        .surname = "Ivanov",
        .patronomic = "Petrovich"
    };

    Contact third_contact = {
        .id = 3,
        .name = "Ivan",
        .surname = "Petrov",
        .patronomic = "Ivanovich"
    };

    ContactNode *head = NULL;

    ContactNode *first_node =
        create_contact_node(&first_contact);

    ContactNode *second_node =
        create_contact_node(&second_contact);

    ContactNode *third_node =
        create_contact_node(&third_contact);

    assert(first_node != NULL);
    assert(second_node != NULL);
    assert(third_node != NULL);

    assert(
        insert_contact_sorted(
            &head,
            first_node
        ) == 1
    );

    assert(
        insert_contact_sorted(
            &head,
            second_node
        ) == 1
    );

    assert(
        insert_contact_sorted(
            &head,
            third_node
        ) == 1
    );

    /* По фамилии Ivanov должны последовательно найтись два контакта. */
    ContactNode *found_node =
        find_contact(
            head,
            "Ivanov",
            "",
            ""
        );

    assert(found_node == first_node);

    found_node =
        find_contact(
            found_node->next,
            "Ivanov",
            "",
            ""
        );

    assert(found_node == second_node);

    /* По имени Ivan также должны последовательно найтись два контакта. */
    found_node =
        find_contact(
            head,
            "",
            "Ivan",
            ""
        );

    assert(found_node == first_node);

    found_node =
        find_contact(
            found_node->next,
            "",
            "Ivan",
            ""
        );

    assert(found_node == third_node);

    /* Одновременный поиск по фамилии и имени. */
    found_node =
        find_contact(
            head,
            "Ivanov",
            "Petr",
            ""
        );

    assert(found_node == second_node);

    /* Поиск по полному Ф.И.О. */
    found_node =
        find_contact(
            head,
            "Petrov",
            "Ivan",
            "Ivanovich"
        );

    assert(found_node == third_node);

    /* Несуществующий контакт не должен быть найден. */
    found_node =
        find_contact(
            head,
            "Sidorov",
            "",
            ""
        );

    assert(found_node == NULL);

    /* Поиск без заполненных критериев не выполняется. */
    found_node =
        find_contact(
            head,
            "",
            "",
            ""
        );

    assert(found_node == NULL);

    free_contact_list(&head);

    assert(head == NULL);
}

/*
  Проверяет отказ при удалении постороннего узла, удаление
  элемента из середины, начала и конца, а также попытку
  удаления из пустого списка.
 */
void test_delete_nodes(void)
{
    Contact antonov = {
        .id = 1,
        .name = "Anton",
        .surname = "Antonov"
    };

    Contact ivanov = {
        .id = 2,
        .name = "Ivan",
        .surname = "Ivanov"
    };

    Contact petrov = {
        .id = 3,
        .name = "Petr",
        .surname = "Petrov"
    };

    Contact sidorov = {
        .id = 4,
        .name = "Sergey",
        .surname = "Sidorov"
    };

    Contact foreign_contact = {
        .id = 5,
        .name = "Foreign",
        .surname = "Foreign"
    };

    ContactNode *head = NULL;

    ContactNode *antonov_node =
        create_contact_node(&antonov);

    ContactNode *ivanov_node =
        create_contact_node(&ivanov);

    ContactNode *petrov_node =
        create_contact_node(&petrov);

    ContactNode *sidorov_node =
        create_contact_node(&sidorov);

    ContactNode *foreign_node =
        create_contact_node(&foreign_contact);

    assert(antonov_node != NULL);
    assert(ivanov_node != NULL);
    assert(petrov_node != NULL);
    assert(sidorov_node != NULL);
    assert(foreign_node != NULL);

    assert(
        insert_contact_sorted(
            &head,
            petrov_node
        ) == 1
    );

    assert(
        insert_contact_sorted(
            &head,
            antonov_node
        ) == 1
    );

    assert(
        insert_contact_sorted(
            &head,
            sidorov_node
        ) == 1
    );

    assert(
        insert_contact_sorted(
            &head,
            ivanov_node
        ) == 1
    );

    /* Узел, который не принадлежит списку, удалять нельзя. */
    assert(
        delete_contact_node(
            &head,
            foreign_node
        ) == 0
    );

    assert(head == antonov_node);
    assert(antonov_node->next == ivanov_node);
    assert(ivanov_node->prev == antonov_node);

    ContactNode *foreign_head =
        foreign_node;

    free_contact_list(&foreign_head);

    assert(foreign_head == NULL);

    /* Удаляем Ivanov из середины и проверяем восстановленные связи. */
    assert(
        delete_contact_node(
            &head,
            ivanov_node
        ) == 1
    );

    assert(head == antonov_node);
    assert(antonov_node->next == petrov_node);
    assert(petrov_node->prev == antonov_node);

    /* Удаляем Antonov из начала: головой должен стать Petrov. */
    assert(
        delete_contact_node(
            &head,
            antonov_node
        ) == 1
    );

    assert(head == petrov_node);
    assert(petrov_node->prev == NULL);
    assert(petrov_node->next == sidorov_node);

    /* Удаляем последний узел Sidorov. */
    assert(
        delete_contact_node(
            &head,
            sidorov_node
        ) == 1
    );

    assert(head == petrov_node);
    assert(petrov_node->prev == NULL);
    assert(petrov_node->next == NULL);

    /* Удаляем единственный оставшийся узел Petrov. */
    assert(
        delete_contact_node(
            &head,
            petrov_node
        ) == 1
    );

    assert(head == NULL);

    /* Повторное удаление из пустого списка должно завершиться отказом. */
    assert(
        delete_contact_node(
            &head,
            NULL
        ) == 0
    );
}

/*
  Имитирует редактирование поля сортировки: находит узел по ID,
  отсоединяет его, изменяет фамилию и вставляет обратно по порядку.
 */
void test_detach_edit_and_reinsert(void)
{
    Contact ivanov = {
        .id = 1,
        .name = "Ivan",
        .surname = "Ivanov"
    };

    Contact petrov = {
        .id = 2,
        .name = "Petr",
        .surname = "Petrov"
    };

    Contact sidorov = {
        .id = 3,
        .name = "Sergey",
        .surname = "Sidorov"
    };

    ContactNode *head = NULL;

    ContactNode *ivanov_node =
        create_contact_node(&ivanov);

    ContactNode *petrov_node =
        create_contact_node(&petrov);

    ContactNode *sidorov_node =
        create_contact_node(&sidorov);

    assert(ivanov_node != NULL);
    assert(petrov_node != NULL);
    assert(sidorov_node != NULL);

    assert(
        insert_contact_sorted(
            &head,
            petrov_node
        ) == 1
    );

    assert(
        insert_contact_sorted(
            &head,
            sidorov_node
        ) == 1
    );

    assert(
        insert_contact_sorted(
            &head,
            ivanov_node
        ) == 1
    );

    assert(head == ivanov_node);
    assert(ivanov_node->next == petrov_node);
    assert(petrov_node->next == sidorov_node);

    /* Для редактирования выбираем конкретный узел по уникальному ID. */
    ContactNode *node_to_edit =
        find_contact_by_id(head, 2);

    assert(node_to_edit == petrov_node);

    assert(
        find_contact_by_id(head, 0) == NULL
    );

    assert(
        find_contact_by_id(head, 100) == NULL
    );

    /* После отсоединения узел не должен иметь связей со списком. */
    assert(
        detach_contact_node(
            &head,
            node_to_edit
        ) == 1
    );

    assert(node_to_edit->prev == NULL);
    assert(node_to_edit->next == NULL);

    assert(head == ivanov_node);
    assert(ivanov_node->prev == NULL);
    assert(ivanov_node->next == sidorov_node);
    assert(sidorov_node->prev == ivanov_node);
    assert(sidorov_node->next == NULL);

    /* Изменение Petrov на Antonov должно перенести узел в начало. */
    strcpy(
        node_to_edit->contact.surname,
        "Antonov"
    );

    assert(node_to_edit->contact.id == 2);

    /* Повторная вставка восстанавливает упорядоченность списка. */
    assert(
        insert_contact_sorted(
            &head,
            node_to_edit
        ) == 1
    );

    assert(head == petrov_node);
    assert(petrov_node->contact.id == 2);
    assert(petrov_node->prev == NULL);
    assert(petrov_node->next == ivanov_node);

    assert(ivanov_node->prev == petrov_node);
    assert(ivanov_node->next == sidorov_node);

    assert(sidorov_node->prev == ivanov_node);
    assert(sidorov_node->next == NULL);

    assert(
        find_contact_by_id(head, 2) ==
        petrov_node
    );

    free_contact_list(&head);

    assert(head == NULL);
}

int main(void)
{
    /* Последовательно запускаем все независимые тестовые сценарии. */
    test_insert_into_empty_list();
    test_sorted_insertion();
    test_find_contacts();
    test_delete_nodes();
    test_detach_edit_and_reinsert();

    printf("All tests passed\n");

    return 0;
}
