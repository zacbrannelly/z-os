#include "syscall_exit.h"

#include "../scheduler/scheduler.h"

void syscall_exit_impl(exception_frame_t *frame) {
    scheduler_terminate();
}
