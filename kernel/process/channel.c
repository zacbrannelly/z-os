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

static int channel_create(const char *path, channel_t **channel_ptr, handle_t *global_handle) {
    if (path == NULL || channel_ptr == NULL || global_handle == NULL) {
        return -1;
    }

    // Allocate channel object.
    channel_t *channel = (channel_t *)kmalloc(sizeof(channel_t));
    if (channel == NULL) {
        return -1;
    }

    if (linked_list_init(&channel->messages) < 0) {
        kfree(channel);
        return -1;
    }

    if (linked_list_init(&channel->recv_waiters) < 0) {
        kfree(channel);
        return -1;
    }

    *channel_ptr = channel;

    // Create system file table entry.
    file_t file_descriptor;
    file_descriptor.path = (char *)path;
    file_descriptor.ref_count = 1;
    file_descriptor.private_data = (void *)channel;
    file_descriptor.ops.read = channel_file_read;
    file_descriptor.ops.write = channel_file_write;
    file_descriptor.ops.close = channel_file_close;
    assert(file_table_open(path, file_descriptor, global_handle) == 0);

    return 0;
}

int channel_open(const char *path, handle_t *fd) {
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
        if (file_object->ops.read != channel_file_read) {
            return -1;
        }
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

    channel_t *channel = (channel_t *)file->private_data;
    assert(channel != NULL);

    // Check if there is a message available.
    while (channel->messages.head == NULL) {
        // Wait for a message to be available.
        thread_t *thread = scheduler_get_current_thread();
        assert(thread != NULL);
        assert(thread->state == THREAD_STATE_RUNNING);

        linked_list_node_t *node = NULL;
        if (linked_list_insert(&channel->recv_waiters, thread, &node) < 0) {
            return -1;
        }

        // Block the thread until a message is available.
        scheduler_wait_for_event();
    }

    // Pop the message off the list.
    linked_list_node_t *message_node = channel->messages.head;
    linked_list_remove(&channel->messages, message_node);

    // Copy the message to the user space.
    uint8_t *message = (uint8_t *)message_node->data;
    assert(message != NULL);
    memory_copy(data, message, size);

    // Free the message (from the kernel space).
    kfree(message);

    // TODO: Using this as status code for now, but POSIX returns the number of bytes read.
    return 0;
}

int channel_file_write(file_t *file, const void *data, uint64_t size) {
    if (file == NULL || data == NULL || size == 0) {
        return -1;
    }

    channel_t *channel = (channel_t *)file->private_data;
    assert(channel != NULL);

    uint8_t *message = (uint8_t *)kmalloc(size);
    assert(message != NULL);

    // Copy the message to kernel memory.
    memory_copy((void *)message, (void *)data, size);

    // Add the message to the channel.
    linked_list_node_t *node = NULL;
    if (linked_list_insert(&channel->messages, message, &node) < 0) {
        kfree(message);
        return -1;
    }

    // Wake up any threads waiting to receive from this channel.
    linked_list_node_t *recv_waiter = channel->recv_waiters.head;
    while (recv_waiter != NULL) {
        // Pop the thread off the wait queue.
        thread_t *thread = (thread_t *)recv_waiter->data;
        linked_list_remove(&channel->recv_waiters, recv_waiter);

        // Wake up the thread.
        scheduler_wake_up(thread);

        // Go to the next waiter.
        recv_waiter = channel->recv_waiters.head;
    }

    return 0;
}

int channel_file_close(file_t *file) {
    // TODO: Implement this.
    return -1;
}
