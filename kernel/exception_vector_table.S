.align 11 // Aligned to 2048 bytes (11 bits).
.global exception_vector_table

exception_vector_table:
    // 0x000 = current EL with SP_EL0 active (SPSEL == 0)
    b current_el_sp0_sync

    .align 7 // Align to 128 bytes
    b current_el_sp0_irq

    .align 7 // Align to 128 bytes
    b current_el_sp0_fiq

    .align 7 // Align to 128 bytes
    b current_el_sp0_serror

    // 0x200 = current EL with SP_EL1 active (SPSEL == 1)
    .align 7 // Align to 128 bytes
    b current_el_sp1_sync

    .align 7 // Align to 128 bytes
    b current_el_sp1_irq

    .align 7 // Align to 128 bytes
    b current_el_sp1_fiq

    .align 7 // Align to 128 bytes
    b current_el_sp1_serror

current_el_sp0_sync:
    eret

current_el_sp0_irq:
    eret

current_el_sp0_fiq:
    eret

current_el_sp0_serror:
    eret

current_el_sp1_sync:
    // Store all registers on the stack.
    sub sp, sp, #(32 * 8)
    stp x0,  x1,  [sp, #(0 * 16)]
    stp x2,  x3,  [sp, #(1 * 16)]
    stp x4,  x5,  [sp, #(2 * 16)]
    stp x6,  x7,  [sp, #(3 * 16)]
    stp x8,  x9,  [sp, #(4 * 16)]
    stp x10, x11, [sp, #(5 * 16)]
    stp x12, x13, [sp, #(6 * 16)]
    stp x14, x15, [sp, #(7 * 16)]
    stp x16, x17, [sp, #(8 * 16)]
    stp x18, x19, [sp, #(9 * 16)]
    stp x20, x21, [sp, #(10 * 16)]
    stp x22, x23, [sp, #(11 * 16)]
    stp x24, x25, [sp, #(12 * 16)]
    stp x26, x27, [sp, #(13 * 16)]
    stp x28, x29, [sp, #(14 * 16)]
    stp x30, xzr, [sp, #(15 * 16)]

    // Call the sync exception handler.
    mov x0, sp
    mrs x1, ESR_EL1
    mrs x2, ELR_EL1
    mrs x3, SPSR_EL1
    bl handle_current_el_sp1_sync_exception

    // Restore the previous registers.
    ldp x0,  x1,  [sp, #(0 * 16)]
    ldp x2,  x3,  [sp, #(1 * 16)]
    ldp x4,  x5,  [sp, #(2 * 16)]
    ldp x6,  x7,  [sp, #(3 * 16)]
    ldp x8,  x9,  [sp, #(4 * 16)]
    ldp x10, x11, [sp, #(5 * 16)]
    ldp x12, x13, [sp, #(6 * 16)]
    ldp x14, x15, [sp, #(7 * 16)]
    ldp x16, x17, [sp, #(8 * 16)]
    ldp x18, x19, [sp, #(9 * 16)]
    ldp x20, x21, [sp, #(10 * 16)]
    ldp x22, x23, [sp, #(11 * 16)]
    ldp x24, x25, [sp, #(12 * 16)]
    ldp x26, x27, [sp, #(13 * 16)]
    ldp x28, x29, [sp, #(14 * 16)]
    ldr x30,      [sp, #(15 * 16)]
    add sp, sp, #(32 * 8)

    // Return to what the CPU was doing prior.
    eret

current_el_sp1_irq:
    eret

current_el_sp1_fiq:
    eret

current_el_sp1_serror:
    eret
