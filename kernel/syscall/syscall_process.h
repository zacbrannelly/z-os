#pragma once

#include <stdint.h>

// Forward declarations.
typedef struct exception_frame_t exception_frame_t;

uint64_t syscall_process_get_handle_impl(exception_frame_t *frame);
