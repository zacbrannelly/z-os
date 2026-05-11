#pragma once

#include <stdint.h>

// Forward declarations.
typedef struct linked_list_node_t linked_list_node_t;
typedef struct process_t process_t;

typedef enum thread_state_t {
    THREAD_STATE_READY,
    THREAD_STATE_RUNNING,
    THREAD_STATE_SUSPENDED,
    THREAD_STATE_TERMINATED,
} thread_state_t;

typedef enum thread_type_t {
    THREAD_TYPE_KERNEL,
    THREAD_TYPE_USER,
} thread_type_t;

typedef struct thread_context_t {
    uint64_t entry_point;
    uint64_t sp;
    uint64_t registers[32];
    uint64_t spsr;
    uint64_t elr;
} thread_context_t;

typedef struct thread_t {
    process_t *process;
    uint64_t stack_top;
    thread_state_t state;
    thread_type_t type;
    thread_context_t ctx;
    linked_list_node_t *run_queue_node;
} thread_t;

int thread_init(
    thread_t *thread,
    uint64_t entry_point,
    uint64_t stack_top,
    thread_type_t type
);

int thread_start(thread_t *thread);
