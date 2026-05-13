#pragma once

#include <stdint.h>

// Forward declarations.
typedef struct exception_frame_t exception_frame_t;

uint64_t syscall_mmap_impl(exception_frame_t *frame);
void syscall_munmap_impl(exception_frame_t *frame);
