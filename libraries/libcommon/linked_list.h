#pragma once

#include <stdint.h>

typedef struct linked_list_node_t {
    void *data;
    struct linked_list_node_t *next;
    struct linked_list_node_t *prev;
    uint8_t list_is_owner;
} linked_list_node_t;

typedef struct linked_list_t {
    linked_list_node_t *head;
    linked_list_node_t *tail;
} linked_list_t;

int linked_list_init(linked_list_t *list);
int linked_list_destroy(linked_list_t *list);

// Allocates a new node and inserts.
int linked_list_insert(linked_list_t *list, void *data, linked_list_node_t **node);

// Inserts an existing node (memory for the node is managed by the caller).
int linked_list_insert_node(linked_list_t *list, linked_list_node_t *node);

// Removes a node from the list (and frees the node if the list is the owner).
int linked_list_remove(linked_list_t *list, linked_list_node_t *node);
