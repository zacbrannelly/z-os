#include "syscall_handler.h"

#include <libz/syscall.h>

#include "../console.h"
#include "../assert.h"
#include "../exception_vector_table.h"

#include "syscall_console_write.h"
#include "syscall_yield.h"
#include "syscall_exit.h"
#include "syscall_mmap.h"
#include "syscall_shm.h"

void syscall_handler(exception_frame_t *frame) {
    uint64_t syscall_idx = frame->registers[8];
    switch (syscall_idx) {
        case SYSCALL_CONSOLE_WRITE:
            syscall_console_write_impl(frame);
            break;
        case SYSCALL_YIELD:
            syscall_yield_impl(frame);
            break;
        case SYSCALL_EXIT:
            syscall_exit_impl(frame);
            break;
        case SYSCALL_MMAP:
            frame->registers[0] = syscall_mmap_impl(frame);
            break;
        case SYSCALL_MUNMAP:
            syscall_munmap_impl(frame);
            break;
        case SYSCALL_SHM_OPEN:
            frame->registers[0] = syscall_shm_open_impl(frame);
            break;
        case SYSCALL_SHM_UNLINK:
            frame->registers[0] = syscall_shm_unlink_impl(frame);
            break;
        default:
            console_write("Unknown syscall index: ");
            console_write_hex(syscall_idx);
            console_write("\r\n");
            assert(0);
    }
}
