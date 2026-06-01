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
#include "syscall_channel.h"
#include "syscall_shutdown.h"
#include "syscall_file_ops.h"
#include "syscall_time.h"

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
        case SYSCALL_CHANNEL_OPEN:
            frame->registers[0] = syscall_channel_open_impl(frame);
            break;
        case SYSCALL_CHANNEL_CLOSE:
            frame->registers[0] = syscall_channel_close_impl(frame);
            break;
        case SYSCALL_CHANNEL_SEND:
            frame->registers[0] = syscall_channel_send_impl(frame);
            break;
        case SYSCALL_CHANNEL_RECV:
            frame->registers[0] = syscall_channel_recv_impl(frame);
            break;
        case SYSCALL_SHUTDOWN:
            frame->registers[0] = syscall_shutdown_impl(frame);
            break;
        case SYSCALL_OPEN:
            frame->registers[0] = syscall_open_impl(frame);
            break;
        case SYSCALL_READ:
            frame->registers[0] = syscall_read_impl(frame);
            break;
        case SYSCALL_WRITE:
            frame->registers[0] = syscall_write_impl(frame);
            break;
        case SYSCALL_CLOSE:
            frame->registers[0] = syscall_close_impl(frame);
            break;
        case SYSCALL_GET_TIME_NS:
            frame->registers[0] = syscall_get_time_ns_impl(frame);
            break;
        case SYSCALL_GET_TIME_MS:
            frame->registers[0] = syscall_get_time_ms_impl(frame);
            break;
        default:
            console_write("Unknown syscall index: ");
            console_write_hex(syscall_idx);
            console_write("\r\n");
            assert(0);
    }
}
