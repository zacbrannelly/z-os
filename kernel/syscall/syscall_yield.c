#include "syscall_yield.h"

#include "../scheduler/scheduler.h"

void syscall_yield_impl(exception_frame_t *frame) {
    scheduler_yield(frame);
}
