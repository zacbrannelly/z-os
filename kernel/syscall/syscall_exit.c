#include "syscall_exit.h"
#include "syscall.h"

#include "../scheduler/scheduler.h"

void syscall_exit(void) {
    syscall_call(SYSCALL_EXIT, 0, 0, 0, 0, 0, 0);
}

void syscall_exit_impl(exception_frame_t *frame) {
    scheduler_terminate();
}
