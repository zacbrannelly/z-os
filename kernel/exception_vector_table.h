#pragma once

#include <stdint.h>

typedef struct exception_frame_t {
    uint64_t registers[32];
    uint64_t sp;
    uint64_t elr;
    uint64_t spsr;
} exception_frame_t;

// Forward declaration from `kernel/exception_vector_table.S`.
void exception_vector_table(void);

// Called when a sync exception occurs.
void handle_sync_exception(exception_frame_t *frame, uint64_t esr);

// Injects the exception vector table into the CPU.
int exception_vector_table_init(void);
