#include "input_file.h"

#include <stddef.h>

#include "../files/file.h"
#include "../scheduler/thread.h"
#include "../scheduler/scheduler.h"
#include "../input/input_device.h"
#include "../files/file_table.h"
#include "../files/file.h"
#include "../process/fd_table.h"
#include "../process/process.h"
#include "../assert.h"
#include "../kmalloc.h"
#include "../memory.h"
#include "fd_table.h"

/**
 * ------------ Kernel API for input file ------------
 */

int input_file_emit(input_file_t *input_file, input_device_event_t *event) {
    if (input_file == NULL || event == NULL) {
        return -1;
    }

    // Start dropping events if the ring buffer is full.
    if (ring_buffer_is_full(&input_file->event_ring_buffer)) {
        return -1;
    }

    // Enqueue the event on the ring buffer.
    assert(ring_buffer_enqueue(&input_file->event_ring_buffer, event, sizeof(input_device_event_t)) == 0);

    // Wake up any threads waiting for an event.
    linked_list_node_t *waiter = input_file->event_waiters.head;
    while (waiter != NULL) {
        // Pop the thread off the wait queue.
        thread_t *thread = (thread_t *)waiter->data;
        linked_list_remove(&input_file->event_waiters, waiter);

        // Wake up the thread.
        scheduler_wake_up(thread);

        // Go to the next waiter.
        waiter = input_file->event_waiters.head;
    }

    return 0;
}


/**
 * ------------ Library API for input file ------------
 */

// Create an input file in the global file table.
static int input_file_create(const char *path, input_device_t *input_device, handle_t *global_handle) {
    if (path == NULL || input_device == NULL || global_handle == NULL) {
        return -1;
    }

    input_file_t *input_file = (input_file_t *)kmalloc(sizeof(input_file_t));
    assert(input_file != NULL);

    memory_set(input_file, 0, sizeof(input_file_t));
    input_file->input_device = input_device;

    assert(ring_buffer_init(
        &input_file->event_ring_buffer,
        (uint64_t)input_file->event_buffer,
        INPUT_FILE_EVENT_BUFFER_SIZE,
        sizeof(input_device_event_t)
    ) == 0);
    assert(linked_list_init(&input_file->event_waiters) == 0);

    file_t file_descriptor;
    file_descriptor.path = NULL; // No path, it is not shared with other processes.
    file_descriptor.ref_count = 1;
    file_descriptor.private_data = (void *)input_file;
    file_descriptor.ops.open = NULL;
    file_descriptor.ops.read = input_file_read;
    // TODO: Implement write and close operations.
    file_descriptor.ops.write = NULL;
    file_descriptor.ops.close = NULL;
    file_descriptor.flags = 0;
    assert(file_table_open(NULL, file_descriptor, global_handle) == 0);

    // Register the input file with the input device.
    assert(input_device_open(input_device, input_file) == 0);

    return 0;
}

// Open an input file and store in process file table.
int input_file_open(file_t *file, handle_t *fd, int flags) {
    if (file == NULL || fd == NULL) {
        return -1;
    }

    thread_t *thread = scheduler_get_current_thread();
    assert(thread != NULL);
    assert(thread->process != NULL);

    // Fetch the input device object from the file (i.e. /dev/mouse0).
    input_device_t *input_device = (input_device_t *)file->private_data;
    assert(input_device != NULL);

    // Create a new input file object.
    handle_t global_handle = -1;
    assert(input_file_create(file->path, input_device, &global_handle) == 0);
    assert(global_handle >= 0);

    // Fetch the global file object.
    file_t *input_file = NULL;
    assert(file_table_get(global_handle, &input_file) == 0);

    // Set the flags for the input file.
    input_file->flags = flags;

    // Register the input file in the process file table.
    fd_table_t *fd_table = &thread->process->fd_table;
    return fd_table_open(fd_table, input_file, fd);
}

// Read an event from an input file.
int input_file_read(file_t *file, void *buffer, uint64_t size) {
    if (file == NULL || buffer == NULL || size == 0) {
        return -1;
    }

    if (size < sizeof(input_device_event_t)) {
        return -1;
    }

    input_file_t *input_file = (input_file_t *)file->private_data;
    assert(input_file != NULL);

    if (file->flags & O_NONBLOCK) {
        if (ring_buffer_is_empty(&input_file->event_ring_buffer)) {
            // TODO: Return more descriptive error code.
            return -1;
        }
    } else {
        while (ring_buffer_is_empty(&input_file->event_ring_buffer)) {
            // Register as a waiter for an event.
            thread_t *thread = scheduler_get_current_thread();
            assert(thread != NULL);
            assert(thread->process != NULL);

            linked_list_node_t *node = NULL;
            assert(linked_list_insert(&input_file->event_waiters, thread, &node) == 0);

            // Block the thread until an event is available.
            scheduler_wait_for_event();
        }
    }

    // Dequeue the event from the ring buffer.
    assert(ring_buffer_dequeue(
        &input_file->event_ring_buffer,
        buffer,
        sizeof(input_device_event_t)
    ) == 0);

    // Return the number of bytes read.
    return sizeof(input_device_event_t);
}
