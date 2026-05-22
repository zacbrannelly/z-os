#include "handle_table.h"

#include <stddef.h>

#include "../memory.h"

int handle_table_init(handle_table_t *handle_table, uint64_t capacity) {
    if (handle_table == NULL || capacity == 0) {
        return -1;
    }

    if (vector_init(&handle_table->handles, capacity, sizeof(uint64_t)) < 0) {
        return -1;
    }

    return 0;
}

int handle_table_destroy(handle_table_t *handle_table) {
    if (handle_table == NULL) {
        return -1;
    }

    if (vector_destroy(&handle_table->handles) < 0) {
        return -1;
    }
    memory_set(handle_table, 0, sizeof(handle_table_t));

    return 0;
}

int handle_table_insert(handle_table_t *handle_table, void *data, handle_t *handle) {
    if (handle_table == NULL || data == NULL || handle == NULL) {
        return -1;
    }

    uint64_t handle_value = (uint64_t)data;
    if (vector_push_back(&handle_table->handles, &handle_value) < 0) {
        return -1;
    }

    *handle = (handle_t)handle_table->handles.size - 1;
    return 0;
}

int handle_table_remove(handle_table_t *handle_table, handle_t handle) {
    if (handle_table == NULL || handle < 0) {
        return -1;
    }

    if ((int32_t)handle >= (int32_t)handle_table->handles.size) {
        return -1;
    }

    // Just set the value 0 to indicate the handle is empty.
    // We don't remove because that breaks the indices of the handles.
    uint64_t empty_value = 0;
    if (vector_set(&handle_table->handles, handle, &empty_value) < 0) {
        return -1;
    }

    return 0;
}

int handle_table_get(handle_table_t *handle_table, handle_t handle, void **data) {
    if (handle_table == NULL || data == NULL || handle < 0) {
        return -1;
    }

    if ((int32_t)handle >= (int32_t)handle_table->handles.size) {
        return -1;
    }
    
    uint64_t* item_ptr = NULL;
    if (vector_get(&handle_table->handles, handle, (void **)&item_ptr) < 0) {
        return -1;
    }

    *data = (void*)(*item_ptr);

    return 0;
}

int handle_table_set(handle_table_t *handle_table, handle_t handle, void *data) {
    if (handle_table == NULL || data == NULL || handle < 0) {
        return -1;
    }

    if ((int32_t)handle >= (int32_t)handle_table->handles.size) {
        return -1;
    }
    
    uint64_t handle_value = (uint64_t)data;
    if (vector_set(&handle_table->handles, handle, &handle_value) < 0) {
        return -1;
    }

    return 0;
}
