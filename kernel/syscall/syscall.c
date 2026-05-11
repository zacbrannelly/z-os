#include "syscall.h"

#include "../console.h"
#include "../assert.h"

#include "syscall_test.h"
#include "syscall_console_write.h"
#include "syscall_yield.h"
#include "syscall_exit.h"

/**
* Calling conventions 
* x0: Return value
* x0-x5: Arguments
* x8: Syscall index
*/
uint64_t syscall_call(uint64_t syscall_idx, uint64_t arg0, uint64_t arg1, uint64_t arg2, uint64_t arg3, uint64_t arg4, uint64_t arg5) {
    register uint64_t x0 asm("x0") = arg0;
    register uint64_t x1 asm("x1") = arg1;
    register uint64_t x2 asm("x2") = arg2;
    register uint64_t x3 asm("x3") = arg3;
    register uint64_t x4 asm("x4") = arg4;
    register uint64_t x5 asm("x5") = arg5;
    register uint64_t x8 asm("x8") = syscall_idx;

    __asm__ volatile (
        "svc #0"
        : "+r" (x0)
        : "r" (x1), "r" (x2), "r" (x3), "r" (x4), "r" (x5), "r" (x8)
        : "memory"
    );

    return x0;
}

void syscall_handler(exception_frame_t *frame) {
    uint64_t syscall_idx = frame->registers[8];
    switch (syscall_idx) {
        case SYSCALL_TEST:
            frame->registers[0] = syscall_test_impl(frame);
            break;
        case SYSCALL_CONSOLE_WRITE:
            syscall_console_write_impl(frame);
            break;
        case SYSCALL_YIELD:
            syscall_yield_impl(frame);
            break;
        case SYSCALL_EXIT:
            syscall_exit_impl(frame);
            break;
        default:
            console_write("Unknown syscall index: ");
            console_write_hex(syscall_idx);
            console_write("\r\n");
            assert(0);
    }
}
