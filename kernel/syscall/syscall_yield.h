#pragma once

#include <stdint.h>

// Forward declarations.
typedef struct exception_frame_t exception_frame_t;

void syscall_yield(void);
void syscall_yield_impl(exception_frame_t *frame);
