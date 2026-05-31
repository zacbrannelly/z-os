#pragma once

#include <stdint.h>
#include <libz/handle.h>
#include <libinput/input_device_event.h>
#include "../utils/linked_list.h"

// Forward declarations.
typedef struct input_file_t input_file_t;

typedef struct input_device_t {
    handle_t handle;
    input_device_type_t type;
    linked_list_t input_files;
} input_device_t;

// Allocate an input device and register in the global file table.
int input_device_create(const char *path, input_device_type_t type, input_device_t **input_device_ptr);

// Destroy an input device and unregister from the global file table.
int input_device_destroy(input_device_t *input_device);

// Emit an event on each input file attached to the input device.
int input_device_emit(input_device_t *input_device, input_device_event_t *event);

// Register an input file to an input device.
int input_device_open(input_device_t *input_device, input_file_t *input_file);

// Unregister an input file from an input device.
int input_device_close(input_device_t *input_device, input_file_t *input_file);
