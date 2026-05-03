#include "linked_list.h"

#include <stddef.h>

#include "../kmalloc.h"

int linked_list_init(linked_list_t *list) {
    list->head = NULL;
    list->tail = NULL;
    return 0;
}

int linked_list_insert(linked_list_t *list, void* data, linked_list_node_t **node) {
    if (list == NULL || node == NULL) {
        return -1;
    }

    linked_list_node_t *new_node = (linked_list_node_t *)kmalloc(sizeof(linked_list_node_t));
    if (new_node == NULL) {
        return -1;
    }

    new_node->data = data;
    new_node->next = NULL;
    new_node->prev = NULL;

    if (list->head == NULL) {
        list->head = new_node;
        list->tail = new_node;
    } else {
        list->tail->next = new_node;
        new_node->prev = list->tail;
        list->tail = new_node;
    }

    *node = new_node;
    return 0;
}

int linked_list_remove(linked_list_t *list, linked_list_node_t *node) {
    if (list == NULL || node == NULL) {
        return -1;
    }

    if (node->prev != NULL) {
        node->prev->next = node->next;
    } else {
        list->head = node->next;
    }
    
    if (node->next != NULL) {
        node->next->prev = node->prev;
    } else {
        list->tail = node->prev;
    }

    kfree(node);
    return 0;
}
