#include "syscall_yield.h"
#include "syscall.h"

#include "../scheduler/scheduler.h"

void syscall_yield(void) {
    syscall_call(SYSCALL_YIELD, 0, 0, 0, 0, 0, 0);
}

void syscall_yield_impl(exception_frame_t *frame) {
    scheduler_yield(frame);
}
