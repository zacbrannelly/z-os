#include "scheduler.h"

#include <stddef.h>

#include "../assert.h"
#include "../kmalloc.h"
#include "../memory.h"
#include "../mmap.h"
#include "../process/process.h"
#include "../exception_vector_table.h"
#include "thread_offsets.h"

typedef struct scheduler_t {
    thread_t *current_thread;
    linked_list_t run_queue;
    thread_t idle_thread;
} scheduler_t;

static scheduler_t g_scheduler;

#define KERNEL_STACK_SIZE (8 * 4096)

// Forward declarations.
void scheduler_save_and_block(thread_t *thread);

static void idle_thread_entry(void) {
    while (1) {
        __asm__ volatile("wfe");
    }
}

int scheduler_init(void) {
    memory_set(&g_scheduler, 0, sizeof(scheduler_t));

    linked_list_init(&g_scheduler.run_queue);
    g_scheduler.current_thread = NULL;

    void *idle_thread_stack = kmalloc(4096);
    if (idle_thread_stack == NULL) {
        return -1;
    }
    thread_init(
        &g_scheduler.idle_thread,
        (uint64_t)idle_thread_entry,
        (uint64_t)idle_thread_stack + 4096,
        THREAD_TYPE_KERNEL
    );
    return 0;
}

int scheduler_add_thread(thread_t *thread) {
    assert(thread->run_queue_node == NULL);
    assert(thread->state == THREAD_STATE_READY || thread->state == THREAD_STATE_SUSPENDED);

    linked_list_node_t *node = NULL;
    if (linked_list_insert(&g_scheduler.run_queue, thread, &node) < 0) {
        return -1;
    }
    thread->run_queue_node = node;
    return 0;
}

int scheduler_remove_thread(thread_t *thread) {
    assert(thread->run_queue_node != NULL);
    linked_list_remove(&g_scheduler.run_queue, thread->run_queue_node);
    thread->run_queue_node = NULL;
    return 0;
}

thread_t *scheduler_get_current_thread(void) {
    return g_scheduler.current_thread;
}

static void __attribute__((noreturn)) context_switch(thread_t *next_thread) {
    if (next_thread->process != NULL) {
        assert(vmap_apply_table(&next_thread->process->address_space.page_table, VMAP_DESTINATION_USER) == 0);
    }

    // Switch stack pointer to the next thread's kernel stack.
    // Then do a context switch to the next thread.
    register uint64_t ctx_ptr asm("x0") = (uint64_t)&next_thread->ctx;
    register uint64_t new_sp asm("x1") = next_thread->kernel_stack_top;
    __asm__ volatile(
        "mov sp, %0\n\t"
        "b scheduler_switch_to_thread\n\t"
        : : "r" (new_sp), "r" (ctx_ptr) : "memory");

    __builtin_unreachable();
}

void scheduler_run(void) {
    thread_t *prev_thread = g_scheduler.current_thread;
    thread_t *next_thread = NULL;

    if (g_scheduler.run_queue.head) {
        next_thread = (thread_t*)g_scheduler.run_queue.head->data;
        scheduler_remove_thread(next_thread);

        // Find the next thread to run.
        while (
            g_scheduler.run_queue.head != NULL && 
            next_thread->state == THREAD_STATE_TERMINATED
        ) {
            next_thread = (thread_t*)g_scheduler.run_queue.head->data;
            scheduler_remove_thread(next_thread);
        }
    }

    if (!next_thread) {
        next_thread = &g_scheduler.idle_thread;
    }

    g_scheduler.current_thread = next_thread;
    next_thread->state = THREAD_STATE_RUNNING;

    if (prev_thread != next_thread) {
        context_switch(next_thread);
    }
}

void scheduler_yield(exception_frame_t *frame) {
    thread_t *current_thread = g_scheduler.current_thread;
    assert(current_thread != NULL);

    // Store the latest thread context.
    for (int i = 0; i < 32; i++) {
        current_thread->ctx.registers[i] = frame->registers[i];
    }
    current_thread->ctx.sp = frame->sp;
    current_thread->ctx.spsr = frame->spsr;
    current_thread->ctx.elr = frame->elr;

    // Mark the thread SUSPENDED.
    current_thread->state = THREAD_STATE_SUSPENDED;

    // Add the thread back to the run queue.
    scheduler_add_thread(current_thread);

    // Restore the kernel VA mapping.
    assert(mmap_apply_mappings() == 0);

    // Context switch (or resume if no other threads)
    scheduler_run();
}

void scheduler_wait_for_event(void) {
    scheduler_save_and_block(g_scheduler.current_thread);
}

void scheduler_wake_up(thread_t *thread) {
    assert(thread->state == THREAD_STATE_BLOCKED);

    // Mark the thread SUSPENDED (like it has been yielded).
    thread->state = THREAD_STATE_SUSPENDED;

    // Add the thread back to the run queue.
    scheduler_add_thread(thread);
}

void scheduler_terminate(void) {
    // Mark the thread TERMINATED.
    g_scheduler.current_thread->state = THREAD_STATE_TERMINATED;

    // Restore the kernel VA mapping.
    assert(mmap_apply_mappings() == 0);

    // Run the scheduler.
    scheduler_run();
}
