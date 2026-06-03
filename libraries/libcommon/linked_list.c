#include "linked_list.h"

#include <stddef.h>
#include <libz/malloc.h>

int linked_list_init(linked_list_t *list) {
    list->head = NULL;
    list->tail = NULL;
    return 0;
}

int linked_list_insert(linked_list_t *list, void* data, linked_list_node_t **node) {
    if (list == NULL || node == NULL) {
        return -1;
    }

    linked_list_node_t *new_node = (linked_list_node_t *)malloc(sizeof(linked_list_node_t));
    if (new_node == NULL) {
        return -1;
    }

    new_node->data = data;
    new_node->next = NULL;
    new_node->prev = NULL;
    new_node->list_is_owner = 1;

    if (linked_list_insert_node(list, new_node) < 0) {
        free(new_node);
        return -1;
    }

    *node = new_node;
    return 0;
}

int linked_list_insert_node(linked_list_t *list, linked_list_node_t *node) {
    if (list == NULL || node == NULL) {
        return -1;
    }

    if (list->head == NULL) {
        list->head = node;
        list->tail = node;
    } else {
        list->tail->next = node;
        node->prev = list->tail;
        list->tail = node;
    }

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

    node->next = NULL;
    node->prev = NULL;

    if (node->list_is_owner) {
        free(node);
    }

    return 0;
}

int linked_list_destroy(linked_list_t *list) {
    if (list == NULL) {
        return -1;
    }

    linked_list_node_t *node = list->head;
    while (node != NULL) {
        linked_list_node_t *next_node = node->next;
        if (node->list_is_owner) {
            free(node);
        }
        node = next_node;
    }

    list->head = NULL;
    list->tail = NULL;
    return 0;
}
