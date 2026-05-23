#include "syscall_channel.h"
#include "../process/channel.h"
#include "../exception_vector_table.h"

uint64_t syscall_channel_open_impl(exception_frame_t *frame) {
    return channel_open((const char *)frame->registers[0], (handle_t *)frame->registers[1]);
}

uint64_t syscall_channel_close_impl(exception_frame_t *frame) {
    return channel_close((handle_t)frame->registers[0]);
}

uint64_t syscall_channel_send_impl(exception_frame_t *frame) {
    return channel_send((handle_t)frame->registers[0], (void *)frame->registers[1], frame->registers[2]);
}

uint64_t syscall_channel_recv_impl(exception_frame_t *frame) {
    return channel_recv((handle_t)frame->registers[0], (void *)frame->registers[1], frame->registers[2]);
}
