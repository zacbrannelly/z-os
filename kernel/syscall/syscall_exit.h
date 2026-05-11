#pragma once

#include <stdint.h>

// Forward declarations.
typedef struct exception_frame_t exception_frame_t;

void syscall_exit(void);
void syscall_exit_impl(exception_frame_t *frame);
