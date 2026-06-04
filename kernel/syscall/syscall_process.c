#include "syscall_process.h"

#include <stddef.h>

#include "../exception_vector_table.h"
#include "../scheduler/scheduler.h"
#include "../scheduler/thread.h"
#include "../process/process.h"
#include "../assert.h"

uint64_t syscall_process_get_handle_impl(exception_frame_t *frame) {
    thread_t *thread = scheduler_get_current_thread();
    assert(thread != NULL);
    assert(thread->process != NULL);
    return (uint64_t)thread->process->handle;
}
