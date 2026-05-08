.global scheduler_switch_to_thread
.type scheduler_switch_to_thread, %function

// void scheduler_switch_to_thread(thread_context_t *next_context);
scheduler_switch_to_thread:
    // Save the next context pointer on the stack (used during restore).
    mov x2, x0

    // Allocate space for the registers to be restored.
    sub sp, sp, #(36 * 8)

    mov x3, x2             // Set x3 to the thread's context.
    mov x4, sp             // Set x4 to SP
    add x5, x3, #(36 * 8)  // Calculate the end of the loop.

// Loop through the registers and copy them to the stack.
loop:
    ldp x0, x1, [x3, #0]
    stp x0, x1, [x4, #0]

    add x3, x3, #16
    add x4, x4, #16
    cmp x3, x5
    b.ne loop

    // Restore the registers from the stack.
    ldp x0, x1,   [sp, #(2 * 8)]
    ldp x2, x3,   [sp, #(4 * 8)]
    ldp x4, x5,   [sp, #(6 * 8)]
    ldp x6, x7,   [sp, #(8 * 8)]
    ldp x8, x9,   [sp, #(10 * 8)]
    ldp x10, x11, [sp, #(12 * 8)]
    ldp x12, x13, [sp, #(14 * 8)]
    ldp x14, x15, [sp, #(16 * 8)]
    ldp x16, x17, [sp, #(18 * 8)]
    ldp x18, x19, [sp, #(20 * 8)]
    ldp x20, x21, [sp, #(22 * 8)]
    ldp x22, x23, [sp, #(24 * 8)]
    ldp x24, x25, [sp, #(26 * 8)]
    ldp x26, x27, [sp, #(28 * 8)]
    ldp x28, x29, [sp, #(30 * 8)]
    ldp x30, xzr, [sp, #(32 * 8)]

    // Read ELR_EL1 from the thread context.
    ldr x0, [sp, #(35 * 8)]

    // Read entry point from the thread context.
    ldr x2, [sp, #0]

    // Read the SPSR_EL1 from the thread context.
    ldr x3, [sp, #(34 * 8)]
    
    // Restore the thread's SP from the stack.
    ldr x1, [sp, #8]
    mov sp, x1

    // If ELR_EL1 is 0x0, then we are starting a thread.
    cmp x0, #0
    b.eq start_thread

    // Otherwise we are resuming a thread.
    b resume_thread

start_thread:
    // Branch to the thread's entry point.
    blr x2

    // Terminate the thread on return.
    b scheduler_terminate

resume_thread:
    // Restore ELR_EL1.
    msr ELR_EL1, x0

    // Read and restore SPSR_EL1.
    msr SPSR_EL1, x3

    // Resume the thread from where it was interrupted.
    eret
