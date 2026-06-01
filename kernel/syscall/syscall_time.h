#pragma once

#include <stdint.h>

// Forward declarations.
typedef struct exception_frame_t exception_frame_t;

uint64_t syscall_get_time_ns_impl(exception_frame_t *frame);
uint64_t syscall_get_time_ms_impl(exception_frame_t *frame);
