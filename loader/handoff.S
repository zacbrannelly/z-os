// void handoff(uint64_t kernel_stack_top, uint64_t kernel_entry_point, boot_info_t *boot_info)
.section .text
.align 2
.global handoff
.type handoff, %function
handoff:
    mov sp, x0
    mov x0, x2
    br x1
