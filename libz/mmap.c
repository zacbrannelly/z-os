#include "mmap.h"
#include "syscall.h"

uint64_t mmap(uint64_t address, uint64_t length, uint64_t flags, handle_t fd) {
    return syscall_mmap(address, length, flags, fd);
}

void munmap(void *address, uint64_t length) {
    syscall_munmap(address, length);
}
