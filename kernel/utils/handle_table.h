#pragma once

#include <stdint.h>
#include <libz/handle.h>

#include "vector.h"

typedef struct handle_table_t {
    vector_t handles;
} handle_table_t;

int handle_table_init(handle_table_t *handle_table, uint64_t capacity);
int handle_table_destroy(handle_table_t *handle_table);

int handle_table_insert(handle_table_t *handle_table, void *data, handle_t *handle);
int handle_table_remove(handle_table_t *handle_table, handle_t handle);

int handle_table_get(handle_table_t *handle_table, handle_t handle, void **data);
int handle_table_set(handle_table_t *handle_table, handle_t handle, void *data);
