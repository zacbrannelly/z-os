#include "syscall_mmap.h"

#include <stddef.h>

#include "../process/process.h"
#include "../process/process_mmap.h"
#include "../scheduler/scheduler.h"
#include "../scheduler/thread.h"
#include "../exception_vector_table.h"
#include "../assert.h"

uint64_t syscall_mmap_impl(exception_frame_t *frame) {
    thread_t *thread = scheduler_get_current_thread();
    assert(thread != NULL);
    assert(thread->process != NULL);
    return process_mmap(thread->process, frame->registers[0], frame->registers[1], frame->registers[2]);
}

void syscall_munmap_impl(exception_frame_t *frame) {
    thread_t *thread = scheduler_get_current_thread();
    assert(thread != NULL);
    assert(thread->process != NULL);
    process_munmap(thread->process, (void *)frame->registers[0], frame->registers[1]);
}
