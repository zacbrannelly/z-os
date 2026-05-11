#pragma once

#include "../vmap.h"

typedef struct address_space_t {
    vmap_t page_table;
} address_space_t;

int address_space_init(address_space_t *address_space);
int address_space_destroy(address_space_t *address_space);
