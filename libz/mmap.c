#include "mmap.h"
#include "syscall.h"

uint64_t mmap(uint64_t address, uint64_t length, uint64_t flags) {
    return syscall_mmap(address, length, flags);
}

void munmap(void *address, uint64_t length) {
    syscall_munmap(address, length);
}
