#pragma once

#include <stdint.h>

typedef struct linked_list_node_t {
    void *data;
    struct linked_list_node_t *next;
    struct linked_list_node_t *prev;
} linked_list_node_t;

typedef struct linked_list_t {
    linked_list_node_t *head;
    linked_list_node_t *tail;
} linked_list_t;

int linked_list_init(linked_list_t *list);
int linked_list_insert(linked_list_t *list, void *data, linked_list_node_t **node);
int linked_list_remove(linked_list_t *list, linked_list_node_t *node);
int linked_list_get_first(linked_list_t *list, linked_list_node_t **node);
