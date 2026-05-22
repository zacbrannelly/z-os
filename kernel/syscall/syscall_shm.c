#include "syscall_shm.h"

#include <stddef.h>
#include <libz/handle.h>

#include "../exception_vector_table.h"
#include "../process/shared_memory.h"
#include "../scheduler/scheduler.h"
#include "../scheduler/thread.h"
#include "../assert.h"

uint64_t syscall_shm_open_impl(exception_frame_t *frame) {
    thread_t *thread = scheduler_get_current_thread();
    assert(thread != NULL);
    assert(thread->process != NULL);

    return shared_memory_open(
        (const char *)frame->registers[0],
        frame->registers[1],
        (handle_t *)&frame->registers[2]
    );
}

uint64_t syscall_shm_unlink_impl(exception_frame_t *frame) {
    thread_t *thread = scheduler_get_current_thread();
    assert(thread != NULL);
    assert(thread->process != NULL);

    return shared_memory_unlink(
        (const char *)frame->registers[0]
    );
}
