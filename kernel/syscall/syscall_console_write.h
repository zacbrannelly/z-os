#pragma once

#include <stdint.h>

// Forward declarations.
typedef struct exception_frame_t exception_frame_t;

void syscall_console_write(const char *message);

void syscall_console_write_impl(exception_frame_t *frame);
