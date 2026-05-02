#pragma once

#include <stdint.h>

// Forward declarations.
typedef struct exception_frame_t exception_frame_t;

void syscall_test(void);

uint64_t syscall_test_impl(exception_frame_t *frame);
