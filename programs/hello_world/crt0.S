.global _start
.extern main

_start:
    // Call the main function.
    bl main

    // Exit the program (syscall_exit).
    mov x8, #3
    svc #0
