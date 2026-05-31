#include "input_device.h"

#include <stddef.h>

#include "../assert.h"
#include "../kmalloc.h"
#include "../memory.h"
#include "../scheduler/scheduler.h"
#include "../scheduler/thread.h"
#include "../files/file_table.h"
#include "../files/file.h"
#include "../process/input_file.h"

// Allocate an input device and register in the global file table.
int input_device_create(const char *path, input_device_type_t type, input_device_t **input_device_ptr) {
    if (input_device_ptr == NULL) {
        return -1;
    }

    // Allocate the input device object.
    input_device_t *input_device = (input_device_t *)kmalloc(sizeof(input_device_t));
    if (input_device == NULL) {
        return -1;
    }
    memory_set(input_device, 0, sizeof(input_device_t));

    // Initialize the input device object.
    input_device->type = type;
    assert(linked_list_init(&input_device->input_files) == 0);

    // Register the device as a file in the global file table.
    file_t file_descriptor;
    file_descriptor.path = (char *)path;
    file_descriptor.ref_count = 1;
    file_descriptor.private_data = (void *)input_device;
    file_descriptor.ops.open = input_file_open;
    file_descriptor.ops.read = NULL;
    file_descriptor.ops.write = NULL;
    file_descriptor.ops.close = NULL;
    file_descriptor.flags = 0;
    assert(file_table_open(path, file_descriptor, &input_device->handle) == 0);

    *input_device_ptr = input_device;
    return 0;
}

// Destroy an input device and unregister from the global file table.
int input_device_destroy(input_device_t *input_device) {
    if (input_device == NULL) {
        return -1;
    }

    linked_list_destroy(&input_device->input_files);
    file_table_close(input_device->handle);
    kfree(input_device);
    return 0;
}

// Register an input file to an input device.
int input_device_open(input_device_t *input_device, input_file_t *input_file) {
    if (input_device == NULL || input_file == NULL) {
        return -1;
    }

    linked_list_node_t *node = NULL;
    assert(linked_list_insert(&input_device->input_files, (void *)input_file, &node) == 0);
    return 0;
}

// Unregister an input file from an input device.
int input_device_close(input_device_t *input_device, input_file_t *input_file) {
    if (input_device == NULL || input_file == NULL) {
        return -1;
    }

    linked_list_node_t *node = input_device->input_files.head;
    while (node != NULL) {
        if (node->data == (void *)input_file) {
            linked_list_remove(&input_device->input_files, node);
            return 0;
        }
        node = node->next;
    }

    return -1;
}

// Emit an event on each input file attached to the input device.
int input_device_emit(input_device_t *input_device, input_device_event_t *event) {
    if (input_device == NULL || event == NULL) {
        return -1;
    }

    // Fan out the event to all input files attached to the device.
    linked_list_node_t *input_file = input_device->input_files.head;
    while (input_file != NULL) {
        // Get the input file object.
        input_file_t *input_file_object = (input_file_t *)input_file->data;
        assert(input_file_object != NULL);

        // Emit the event on the input file.
        input_file_emit(input_file_object, event);

        // Go to the next input file.
        input_file = input_file->next;
    }

    return 0;
}
