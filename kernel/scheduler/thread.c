#include "thread.h"
#include "scheduler.h"

#include "../memory.h"

int thread_init(
    thread_t *thread,
    uint64_t entry_point,
    uint64_t stack_top,
    thread_type_t type
) {
    memory_set(thread, 0, sizeof(thread_t));
    thread->stack_top = stack_top;
    thread->type = type;
    thread->state = THREAD_STATE_READY;

    // Prepare context for the thread.
    thread->ctx.entry_point = entry_point;
    thread->ctx.sp = stack_top;
    return 0;
}

int thread_start(thread_t *thread) {
    return scheduler_add_thread(thread);
}
