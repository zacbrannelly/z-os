#include "scheduler.h"

#include <stddef.h>

#include "../assert.h"
#include "../kmalloc.h"
#include "../memory.h"
#include "../exception_vector_table.h"

typedef struct scheduler_t {
    uint64_t kernel_stack_top;
    thread_t *current_thread;
    linked_list_t run_queue;
    thread_t idle_thread;
} scheduler_t;

static scheduler_t g_scheduler;

#define KERNEL_STACK_SIZE (8 * 4096)

static void idle_thread_entry(void) {
    while (1) {
        __asm__ volatile("wfe");
    }
}

int scheduler_init(void) {
    memory_set(&g_scheduler, 0, sizeof(scheduler_t));

    // Allocate the kernel stack.
    g_scheduler.kernel_stack_top = (uint64_t)kmalloc(KERNEL_STACK_SIZE);
    if (g_scheduler.kernel_stack_top == 0) {
        return -1;
    }
    g_scheduler.kernel_stack_top += KERNEL_STACK_SIZE;

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

static void __attribute__((noreturn)) context_switch(thread_t *next_thread) {
    register uint64_t x0 asm("x0") = (uint64_t)&next_thread->ctx;
    __asm__ volatile("b scheduler_switch_to_thread" : : "r" (x0) : "memory");
    __builtin_unreachable();
}

static void restore_kernel_stack(void) {
    __asm__ volatile("mov sp, %0" : : "r" (g_scheduler.kernel_stack_top));
}

static void store_kernel_stack(void) {
    __asm__ volatile("mov %0, sp" : "=r" (g_scheduler.kernel_stack_top));
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
        store_kernel_stack();
        context_switch(next_thread);
    }
}

void scheduler_yield(exception_frame_t *frame) {
    restore_kernel_stack();

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

    // Run the scheduler.
    scheduler_run();
}

void scheduler_terminate(void) {
    restore_kernel_stack();

    // Mark the thread TERMINATED.
    g_scheduler.current_thread->state = THREAD_STATE_TERMINATED;

    // Run the scheduler.
    scheduler_run();
}
