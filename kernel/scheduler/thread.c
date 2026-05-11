#include "thread.h"
#include "scheduler.h"

#include "../memory.h"

#define SPSR_MODE_EL0   0x0 // User mode
#define SPSR_MODE_EL1_T 0x4 // Kernel mode (but use user stack)
#define SPSR_MODE_EL1_H 0x5 // Kernel mode (use kernel stack)

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
    thread->ctx.elr = entry_point;
    thread->ctx.sp = stack_top;

    switch (type) {
        case THREAD_TYPE_KERNEL:
            thread->ctx.spsr = SPSR_MODE_EL1_H;
            break;
        case THREAD_TYPE_USER:
            // On start/resume, jump to user mode (EL0).
            thread->ctx.spsr = SPSR_MODE_EL0;
            break;
    }

    return 0;
}

int thread_start(thread_t *thread) {
    return scheduler_add_thread(thread);
}
