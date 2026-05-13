#pragma once

#include <stdint.h>

// Forward declarations.
typedef struct exception_frame_t exception_frame_t;

// Handles a syscall.
void syscall_handler(exception_frame_t *frame);
