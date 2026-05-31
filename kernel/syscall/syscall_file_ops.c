#include "syscall_file_ops.h"
#include "../files/file.h"
#include "../exception_vector_table.h"

uint64_t syscall_open_impl(exception_frame_t *frame) {
    return file_open(
        (const char *)frame->registers[0],
        (handle_t *)frame->registers[1],
        (int)frame->registers[2]
    );
}

uint64_t syscall_read_impl(exception_frame_t *frame) {
    return file_read((handle_t)frame->registers[0], (void *)frame->registers[1], frame->registers[2]);
}

uint64_t syscall_write_impl(exception_frame_t *frame) {
    return file_write((handle_t)frame->registers[0], (const void *)frame->registers[1], frame->registers[2]);
}

uint64_t syscall_close_impl(exception_frame_t *frame) {
    return file_close((handle_t)frame->registers[0]);
}
