#include "channel.h"

#include <stddef.h>

#include "fd_table.h"
#include "process.h"
#include "../memory.h"
#include "../files/file.h"
#include "../files/file_table.h"
#include "../assert.h"
#include "../kmalloc.h"
#include "../page_alloc.h"
#include "../scheduler/thread.h"
#include "../scheduler/scheduler.h"
#include "../utils/linked_list.h"

typedef struct channel_message_t {
    uint8_t *data;
    uint64_t size;
} channel_message_t;

typedef struct channel_t {
    linked_list_t endpoints;
} channel_t;

typedef struct channel_file_t {
    file_t *source_channel;
    int flags;

    linked_list_node_t *endpoint_node;
    linked_list_t messages;
    linked_list_t recv_waiters;
} channel_file_t;

static int channel_create(const char *path, channel_t **channel_ptr, handle_t *global_handle) {
    if (path == NULL || channel_ptr == NULL || global_handle == NULL) {
        return -1;
    }

    // Allocate channel object.
    channel_t *channel = (channel_t *)kmalloc(sizeof(channel_t));
    if (channel == NULL) {
        return -1;
    }

    if (linked_list_init(&channel->endpoints) < 0) {
        kfree(channel);
        return -1;
    }

    *channel_ptr = channel;

    // Create system file table entry.
    file_t file_descriptor;
    file_descriptor.path = (char *)path;
    file_descriptor.ref_count = 1;
    file_descriptor.private_data = (void *)channel;
    file_descriptor.ops.read = NULL;
    file_descriptor.ops.write = NULL;
    file_descriptor.ops.close = NULL;
    file_descriptor.flags = 0;
    assert(file_table_open(path, file_descriptor, global_handle) == 0);

    return 0;
}

int channel_open(const char *path, handle_t *fd, int flags) {
    if (path == NULL || fd == NULL) {
        return -1;
    }

    thread_t *thread = scheduler_get_current_thread();
    assert(thread != NULL);
    assert(thread->process != NULL);

    channel_t *channel = NULL;
    file_t *file_object = NULL;
    handle_t global_handle = -1;

    if (file_table_get_by_path(path, &file_object) == 0) {
        assert(file_object != NULL);
        // TODO: Validate file object is a channel.
        channel = (channel_t *)file_object->private_data;
        file_object->ref_count++;
        global_handle = file_object->handle;
    } else {
        assert(channel_create(path, &channel, &global_handle) == 0);
        assert(file_table_get(global_handle, &file_object) == 0);
    }

    assert(channel != NULL);
    assert(global_handle >= 0);
    assert(file_object != NULL);

    // Create process file table entry.
    channel_file_t *channel_file = (channel_file_t *)kmalloc(sizeof(channel_file_t));
    assert(channel_file != NULL);
    channel_file->source_channel = file_object;
    channel_file->flags = flags;

    // Init message queue and waiters (TODO: Move to ring buffer instead of linked list).
    assert(linked_list_init(&channel_file->messages) == 0);
    assert(linked_list_init(&channel_file->recv_waiters) == 0);

    // Register the channel file as an endpoint of the channel.
    assert(linked_list_insert(&channel->endpoints, channel_file, &channel_file->endpoint_node) == 0);

    // Store in system file table.
    file_t file_descriptor;
    file_descriptor.path = NULL; // No path, it is not shared with other processes.
    file_descriptor.ref_count = 1;
    file_descriptor.private_data = (void *)channel_file;
    file_descriptor.ops.read = channel_file_read;
    file_descriptor.ops.write = channel_file_write;
    file_descriptor.ops.close = channel_file_close;
    file_descriptor.flags = flags;
    assert(file_table_open(NULL, file_descriptor, &global_handle) == 0);
    assert(global_handle >= 0);

    // Fetch the new file object.
    assert(file_table_get(global_handle, &file_object) == 0);
    assert(file_object != NULL);

    // Store in process file table.
    fd_table_t *fd_table = &thread->process->fd_table;
    assert(fd_table_open(fd_table, file_object, fd) == 0);

    return 0;
}

int channel_close(handle_t fd) {
    // TODO: Implement this.
    return -1;
}

int channel_send(handle_t fd, const void *data, uint64_t size) {
    if (fd == -1 || data == NULL || size == 0) {
        return -1;
    }

    thread_t *thread = scheduler_get_current_thread();
    assert(thread != NULL);
    assert(thread->state == THREAD_STATE_RUNNING);
    assert(thread->process != NULL);

    file_t *file_object = NULL;
    assert(fd_table_get(&thread->process->fd_table, fd, &file_object) == 0);
    assert(file_object != NULL);
    assert(file_object->ops.write == channel_file_write);

    return file_object->ops.write(file_object, data, size);
}

int channel_recv(handle_t fd, void *data, uint64_t size) {
    if (fd == -1 || data == NULL || size == 0) {
        return -1;
    }

    thread_t *thread = scheduler_get_current_thread();
    assert(thread != NULL);
    assert(thread->state == THREAD_STATE_RUNNING);
    assert(thread->process != NULL);

    file_t *file_object = NULL;
    assert(fd_table_get(&thread->process->fd_table, fd, &file_object) == 0);
    assert(file_object != NULL);
    assert(file_object->ops.read == channel_file_read);

    return file_object->ops.read(file_object, data, size);
}

int channel_file_read(file_t *file, void *data, uint64_t size) {
    if (file == NULL || data == NULL || size == 0) {
        return -1;
    }

    channel_file_t *channel_file = (channel_file_t *)file->private_data;
    assert(channel_file != NULL);
    assert(channel_file->source_channel != NULL);

    if (channel_file->flags & O_NONBLOCK) {
        // Check if there is a message available.
        if (channel_file->messages.head == NULL) {
            return -1;
        }
    } else {
        // Check if there is a message available.
        while (channel_file->messages.head == NULL) {
            // Wait for a message to be available.
            thread_t *thread = scheduler_get_current_thread();
            assert(thread != NULL);
            assert(thread->state == THREAD_STATE_RUNNING);

            linked_list_node_t *node = NULL;
            if (linked_list_insert(&channel_file->recv_waiters, thread, &node) < 0) {
                return -1;
            }

            // Block the thread until a message is available.
            scheduler_wait_for_event();
        }
    }

    // Peek at the message.
    linked_list_node_t *message_node = channel_file->messages.head;

    // Copy the message to the user space.
    channel_message_t *message_object = (channel_message_t *)message_node->data;
    assert(message_object != NULL);

    if (message_object->size > size) {
        // TODO: Return more descriptive error code.
        return -1;
    }

    memory_copy(data, message_object->data, message_object->size);

    // Free the message (from the kernel space).
    kfree(message_object->data);
    kfree(message_object);

    // Pop the message off the list.
    linked_list_remove(&channel_file->messages, message_node);

    // Return the number of bytes read.
    return message_object->size;
}

int channel_file_write(file_t *file, const void *data, uint64_t size) {
    if (file == NULL || data == NULL || size == 0) {
        return -1;
    }

    channel_file_t *channel_file = (channel_file_t *)file->private_data;
    assert(channel_file != NULL);
    assert(channel_file->source_channel != NULL);

    channel_t *channel = (channel_t *)channel_file->source_channel->private_data;
    assert(channel != NULL);

    uint8_t *message = (uint8_t *)kmalloc(size);
    assert(message != NULL);

    // Copy the message to kernel memory.
    memory_copy((void *)message, (void *)data, size);

    channel_message_t *message_object = (channel_message_t *)kmalloc(sizeof(channel_message_t));
    assert(message_object != NULL);
    message_object->data = message;
    message_object->size = size;

    // Fan out the message to all endpoints (except ourselves).
    linked_list_node_t *endpoint_node = channel->endpoints.head;
    while (endpoint_node != NULL) {
        // Skip ourselves.
        if (endpoint_node == channel_file->endpoint_node) {
            endpoint_node = endpoint_node->next;
            continue;
        }

        channel_file_t *endpoint_file = (channel_file_t *)endpoint_node->data;
        assert(endpoint_file != NULL);

        linked_list_node_t *node = NULL;
        linked_list_insert(&endpoint_file->messages, message_object, &node);

        // Wake up any threads waiting to receive from this channel.
        linked_list_node_t *recv_waiter = endpoint_file->recv_waiters.head;
        while (recv_waiter != NULL) {
            // Pop the thread off the wait queue.
            thread_t *thread = (thread_t *)recv_waiter->data;
            linked_list_remove(&endpoint_file->recv_waiters, recv_waiter);

            // Wake up the thread.
            scheduler_wake_up(thread);

            // Go to the next waiter.
            recv_waiter = endpoint_file->recv_waiters.head;
        }

        endpoint_node = endpoint_node->next;
    }

    return 0;
}

int channel_file_close(file_t *file) {
    // TODO: Implement this.
    return -1;
}
