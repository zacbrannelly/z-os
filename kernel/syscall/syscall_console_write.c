#include "syscall_console_write.h"
#include "syscall.h"

#include "../exception_vector_table.h"
#include "../console.h"
#include "../assert.h"

void syscall_console_write(const char *message) {
    syscall_call(
        SYSCALL_CONSOLE_WRITE,
        (uint64_t)message,
        0,
        0,
        0,
        0,
        0
    );
}

void syscall_console_write_impl(exception_frame_t *frame) {
    const char *message = (const char *)frame->registers[0];
    console_write(message);
}
