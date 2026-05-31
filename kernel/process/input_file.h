#pragma once

#include <stdint.h>
#include <libz/handle.h>

#include "../input/input_device.h"
#include "../utils/linked_list.h"
#include "../utils/ring_buffer.h"

#define INPUT_FILE_EVENT_BUFFER_SIZE 200

// Forward declarations.
typedef struct file_t file_t;

typedef struct input_file_t {
    input_device_t *input_device;
    linked_list_t event_waiters;
    input_device_event_t event_buffer[INPUT_FILE_EVENT_BUFFER_SIZE];
    ring_buffer_t event_ring_buffer;
} input_file_t;

/**
 * ------------ Kernel API for input file ------------
 */

// Emit an event on an input file.
int input_file_emit(input_file_t *input_file, input_device_event_t *event);

/**
 * ------------ Library API for input file ------------
 */

// Open an input file and store in process file table.
int input_file_open(file_t *file, handle_t *handle);

// Read an event from an input file.
int input_file_read(file_t *file, void *buffer, uint64_t size);
