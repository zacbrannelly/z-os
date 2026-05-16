#include "vector.h"

#include <stddef.h>

#include "../kmalloc.h"
#include "../memory.h"

int vector_init(vector_t *vector, uint64_t capacity, uint64_t element_size) {
    if (vector == NULL || capacity == 0) {
        return -1;
    }

    vector->data = (void *)kmalloc(capacity * element_size);
    if (vector->data == NULL) {
        return -1;
    }

    vector->size = 0;
    vector->capacity = capacity;
    vector->element_size = element_size;

    return 0;
}

int vector_destroy(vector_t *vector) {
    if (vector == NULL) {
        return -1;
    }

    kfree(vector->data);
    vector->data = NULL;
    vector->size = 0;
    vector->capacity = 0;
    vector->element_size = 0;
    return 0;
}

static int expand_capacity(vector_t *vector) {
    if (vector == NULL) {
        return -1;
    }

    vector->capacity *= 2;
    void* data = (void *)kmalloc(vector->capacity * vector->element_size);
    if (data == NULL) {
        return -1;
    }

    memory_copy(data, vector->data, vector->size * vector->element_size);
    kfree(vector->data);
    vector->data = data;

    return 0;
}

static int shrink_capacity(vector_t *vector) {
    if (vector == NULL) {
        return -1;
    }

    if (vector->size >= vector->capacity / 2) {
        return 0;
    }

    vector->capacity /= 2;
    void* data = (void *)kmalloc(vector->capacity * vector->element_size);
    if (data == NULL) {
        return -1;
    }

    memory_copy(data, vector->data, vector->size * vector->element_size);
    kfree(vector->data);
    vector->data = data;

    return 0;
}

int vector_push_back(vector_t *vector, void *data) {
    if (vector == NULL || data == NULL) {
        return -1;
    }

    if (vector->size >= vector->capacity) {
        if (expand_capacity(vector) < 0) {
            return -1;
        }
    }

    memory_copy(vector->data + vector->size * vector->element_size, data, vector->element_size);
    vector->size++;

    return 0;
}

int vector_pop_back(vector_t *vector, void *data) {
    if (vector == NULL || data == NULL) {
        return -1;
    }

    if (vector->size == 0) {
        return -1;
    }

    memory_copy(data, vector->data + (vector->size - 1) * vector->element_size, vector->element_size);
    vector->size--;

    if (vector->size < vector->capacity / 2) {
        if (shrink_capacity(vector) < 0) {
            return -1;
        }
    }

    return 0;
}

int vector_get(vector_t *vector, uint64_t index, void **data) {
    if (vector == NULL || data == NULL) {
        return -1;
    }

    if (index >= vector->size) {
        return -1;
    }

    *data = vector->data + index * vector->element_size;
    return 0;
}

int vector_set(vector_t *vector, uint64_t index, void *data) {
    if (vector == NULL || data == NULL) {
        return -1;
    }

    if (index >= vector->size) {
        return -1;
    }

    memory_copy(vector->data + index * vector->element_size, data, vector->element_size);
    return 0;
}
