.global _start
.extern main

_start:
    // Initialize the malloc library.
    bl malloc_init

    // Call the main function.
    bl main

    // Exit the program (syscall_exit).
    mov x8, #3
    svc #0
