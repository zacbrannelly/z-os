#pragma once

#include <stdint.h>

// Forward declarations.
typedef struct exception_frame_t exception_frame_t;

uint64_t syscall_shm_open_impl(exception_frame_t *frame);
uint64_t syscall_shm_unlink_impl(exception_frame_t *frame);
