#include "thread.h"
#include "scheduler.h"

#include "../memory.h"
#include "../kmalloc.h"

#define KERNEL_STACK_SIZE (8 * 4096)

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

    // Allocate some stack space used when in kernel mode.
    uint64_t kernel_stack = (uint64_t)kmalloc(KERNEL_STACK_SIZE);
    if (kernel_stack == 0) {
        return -1;
    }
    thread->kernel_stack_top = kernel_stack + KERNEL_STACK_SIZE;

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
