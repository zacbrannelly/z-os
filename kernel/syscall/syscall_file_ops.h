#pragma once

#include <stdint.h>
#include <libz/handle.h>

// Forward declarations.
typedef struct exception_frame_t exception_frame_t;

uint64_t syscall_open_impl(exception_frame_t *frame);
uint64_t syscall_read_impl(exception_frame_t *frame);
uint64_t syscall_write_impl(exception_frame_t *frame);
uint64_t syscall_close_impl(exception_frame_t *frame);
