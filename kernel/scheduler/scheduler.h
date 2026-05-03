#pragma once

#include <stdint.h>
#include "thread.h"
#include "../utils/linked_list.h"

typedef struct exception_frame_t exception_frame_t;

int scheduler_init(void);
int scheduler_add_thread(thread_t *thread);
int scheduler_remove_thread(thread_t *thread);

// Yield the current thread.
void scheduler_yield(exception_frame_t *frame);

// Terminate the current thread.
void scheduler_terminate(void);

// Run the scheduler loop.
void scheduler_run(void);
