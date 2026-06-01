#include "syscall_time.h"
#include "../time.h"

uint64_t syscall_get_time_ns_impl(exception_frame_t *frame) {
    return get_time_ns();
}

uint64_t syscall_get_time_ms_impl(exception_frame_t *frame) {
    return get_time_ms();
}
