#pragma once

#include <stdint.h>

typedef struct vector_t {
    void *data;
    uint64_t size;
    uint64_t capacity;
    uint64_t element_size;
} vector_t;

int vector_init(vector_t *vector, uint64_t capacity, uint64_t element_size);
int vector_destroy(vector_t *vector);

int vector_push_back(vector_t *vector, void *data);
int vector_pop_back(vector_t *vector, void *data);

int vector_get(vector_t *vector, uint64_t index, void **data);
int vector_set(vector_t *vector, uint64_t index, void *data);
