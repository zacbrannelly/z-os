#include "file_table.h"

#include <stddef.h>
#include <libz/string.h>

#include "../memory.h"
#include "../assert.h"
#include "../kmalloc.h"
#include "../utils/handle_table.h"
#include "../utils/hash_table.h"

#define DEFAULT_FILE_TABLE_CAPACITY 64
#define DEFAULT_FILE_PATH_CAPACITY 64

static handle_table_t g_file_table;
static hash_table_t g_file_path_table;

int file_table_init(void) {
    if (handle_table_init(&g_file_table, DEFAULT_FILE_TABLE_CAPACITY) < 0) {
        return -1;
    }

    if (hash_table_init(&g_file_path_table, DEFAULT_FILE_PATH_CAPACITY) < 0) {
        return -1;
    }

    return 0;
}

int file_table_destroy(void) {
    return handle_table_destroy(&g_file_table);
}

int file_table_open(const char *path, file_t file, handle_t *handle) {
    file_t *file_ptr = (file_t *)kmalloc(sizeof(file_t));
    if (file_ptr == NULL) {
        return -1;
    }

    if (handle_table_insert(&g_file_table, file_ptr, handle) < 0) {
        kfree(file_ptr);
        return -1;
    }

    char* path_copy = NULL;
    if (path != NULL) {
        // Create copy of the path string.
        path_copy = (char*)kmalloc(strlen(path) + 1);
        assert(path_copy != NULL);
        memory_copy((void*)path_copy, (void*)path, strlen(path) + 1);

        hash_key_t key = hash_key_create_data((void *)path_copy, strlen(path) + 1);
        if (hash_table_set(&g_file_path_table, &key, file_ptr) < 0) {
            handle_table_remove(&g_file_table, *handle);
            kfree(file_ptr);
            return -1;
        }
    }

    *file_ptr = file;
    file_ptr->path = path_copy;
    file_ptr->handle = *handle;

    return 0;
}

int file_table_close(handle_t handle) {
    file_t *file_ptr = NULL;
    if (handle_table_get(&g_file_table, handle, (void **)&file_ptr) < 0) {
        return -1;
    }

    if (file_ptr == NULL) {
        return -1;
    }

    if (file_ptr->path != NULL) {
        hash_key_t key = hash_key_create_data((void *)file_ptr->path, strlen(file_ptr->path) + 1);
        hash_table_remove(&g_file_path_table, &key);
    }

    handle_table_remove(&g_file_table, handle);
    kfree(file_ptr);
    return 0;
}

int file_table_get(handle_t handle, file_t **file) {
    if (file == NULL) {
        return -1;
    }

    return handle_table_get(&g_file_table, handle, (void **)file);
}

int file_table_get_by_path(const char *path, file_t **file) {
    if (path == NULL || file == NULL) {
        return -1;
    }

    hash_key_t key = hash_key_create_data((void *)path, strlen(path) + 1);
    return hash_table_get(&g_file_path_table, &key, (void **)file);
}
