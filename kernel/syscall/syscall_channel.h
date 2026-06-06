#pragma once

#include <stdint.h>

// Forward declarations.
typedef struct exception_frame_t exception_frame_t;

uint64_t syscall_channel_open_impl(exception_frame_t *frame);
uint64_t syscall_channel_close_impl(exception_frame_t *frame);
uint64_t syscall_channel_send_impl(exception_frame_t *frame);
uint64_t syscall_channel_recv_impl(exception_frame_t *frame);
uint64_t syscall_channel_send_fd_impl(exception_frame_t *frame);
uint64_t syscall_channel_recv_fd_impl(exception_frame_t *frame);
