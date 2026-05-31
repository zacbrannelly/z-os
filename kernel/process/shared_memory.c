#include "shared_memory.h"

#include <stddef.h>

#include "fd_table.h"
#include "process.h"
#include "../files/file.h"
#include "../files/file_table.h"
#include "../assert.h"
#include "../kmalloc.h"
#include "../page_alloc.h"
#include "../scheduler/thread.h"
#include "../scheduler/scheduler.h"

int shared_memory_create(
    const char *path,
    uint64_t size,
    shared_memory_t **shared_memory_ptr,
    handle_t *global_handle
) {
    if (path == NULL || shared_memory_ptr == NULL || global_handle == NULL) {
        return -1;
    }

    // Allocate shared memory object.
    shared_memory_t *shared_memory = (shared_memory_t *)kmalloc(sizeof(shared_memory_t));
    if (shared_memory == NULL) {
        return -1;
    }

    if (vector_init(&shared_memory->pages, 16, sizeof(uint64_t)) < 0) {
        kfree(shared_memory);
        return -1;
    }

    uint64_t num_pages = size / PAGE_SIZE;
    if (size % PAGE_SIZE != 0) {
        num_pages++;
    }

    // Allocate physical pages.
    for (uint64_t i = 0; i < num_pages; i++) {
        uint64_t page_address = 0;
        // TODO: Handle error cases properly.
        assert(page_alloc_block(PAGE_ALLOC_ORDER_4KB, &page_address) == 0);
        assert(vector_push_back(&shared_memory->pages, &page_address) == 0);
    }
    shared_memory->size = size;
    shared_memory->pages_owned = 1;

    *shared_memory_ptr = shared_memory;

    // Create system file table entry.
    file_t file_descriptor;
    file_descriptor.path = (char *)path;
    file_descriptor.ref_count = 1;
    file_descriptor.private_data = (void *)shared_memory;
    file_descriptor.ops.read = shared_memory_read;
    file_descriptor.ops.write = shared_memory_write;
    file_descriptor.ops.close = shared_memory_close;
    file_descriptor.flags = 0;
    assert(file_table_open(path, file_descriptor, global_handle) == 0);

    return 0;
}

int shared_memory_create_from_contiguous_pages(
    const char *path,
    uint64_t physical_start_address,
    uint64_t size,
    shared_memory_t **shared_memory_ptr,
    handle_t *global_handle
) {
    if (path == NULL || shared_memory_ptr == NULL || global_handle == NULL) {
        return -1;
    }

    // Allocate shared memory object.
    shared_memory_t *shared_memory = (shared_memory_t *)kmalloc(sizeof(shared_memory_t));
    if (shared_memory == NULL) {
        return -1;
    }

    if (vector_init(&shared_memory->pages, 16, sizeof(uint64_t)) < 0) {
        kfree(shared_memory);
        return -1;
    }

    uint64_t num_pages = size / PAGE_SIZE;
    if (size % PAGE_SIZE != 0) {
        num_pages++;
    }

    // Allocate physical pages.
    uint64_t page_address = physical_start_address;
    for (uint64_t i = 0; i < num_pages; i++) {
        assert(vector_push_back(&shared_memory->pages, &page_address) == 0);
        page_address += PAGE_SIZE;
    }
    shared_memory->size = size;
    shared_memory->pages_owned = 0;

    *shared_memory_ptr = shared_memory;

    // Create system file table entry.
    file_t file_descriptor;
    file_descriptor.path = (char *)path;
    file_descriptor.ref_count = 1;
    file_descriptor.private_data = (void *)shared_memory;
    file_descriptor.ops.read = shared_memory_read;
    file_descriptor.ops.write = shared_memory_write;
    file_descriptor.ops.close = shared_memory_close;
    file_descriptor.flags = 0;
    assert(file_table_open(path, file_descriptor, global_handle) == 0);

    return 0;
}

int shared_memory_destroy(handle_t global_handle) {
    // TODO: Implement this.
    assert(0);
    return 0;
}

int shared_memory_open(const char *path, uint64_t size, handle_t *fd) {
    if (path == NULL || fd == NULL) {
        return -1;
    }

    thread_t *thread = scheduler_get_current_thread();
    assert(thread != NULL);
    assert(thread->process != NULL);

    file_t *file_object = NULL;
    shared_memory_t *shared_memory = NULL;
    handle_t global_handle = -1;

    // Check if the file already exists.
    if (file_table_get_by_path(path, &file_object) == 0) {
        assert(file_object != NULL);
        if (file_object->ops.read != shared_memory_read) {
            // File is not a shared memory file.
            return -1;
        }
        shared_memory = (shared_memory_t *)file_object->private_data;
        file_object->ref_count++;
        global_handle = file_object->handle;
    } else {
        assert(shared_memory_create(path, size, &shared_memory, &global_handle) == 0);
        assert(file_table_get(global_handle, &file_object) == 0);
    }

    assert(shared_memory != NULL);
    assert(global_handle >= 0);
    assert(file_object != NULL);

    // Create process file table entry.
    fd_table_t *fd_table = &thread->process->fd_table;
    assert(fd_table_open(fd_table, file_object, fd) == 0);

    return 0;
}

int shared_memory_unlink(const char *path) {
    // TODO: Implement this.
    assert(0);
    return 0;
}

int shared_memory_close(file_t *file) {
    // TODO: Implement this.
    assert(0);
    return 0;
}

int shared_memory_read(file_t *file, void *buffer, uint64_t size) {
    // TODO: Implement this.
    assert(0);
    return 0;
}

int shared_memory_write(file_t *file, const void *buffer, uint64_t size) {
    // TODO: Implement this.
    assert(0);
    return 0;
}
