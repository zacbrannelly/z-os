#pragma once

#include <stdint.h>
#include <libz/handle.h>

#define SYSCALL_CONSOLE_WRITE 0x1
#define SYSCALL_YIELD         0x2
#define SYSCALL_EXIT          0x3
#define SYSCALL_MMAP          0x4
#define SYSCALL_MUNMAP        0x5
#define SYSCALL_SHM_OPEN      0x6
#define SYSCALL_SHM_UNLINK    0x7
#define SYSCALL_CHANNEL_OPEN  0x8
#define SYSCALL_CHANNEL_CLOSE 0x9
#define SYSCALL_CHANNEL_SEND  0xA
#define SYSCALL_CHANNEL_RECV  0xB

/**
* Calling conventions 
* x0: Return value
* x0-x5: Arguments
* x8: Syscall index
*/
static inline uint64_t syscall_call(uint64_t syscall_idx, uint64_t arg0, uint64_t arg1, uint64_t arg2, uint64_t arg3, uint64_t arg4, uint64_t arg5) {
    register uint64_t x0 asm("x0") = arg0;
    register uint64_t x1 asm("x1") = arg1;
    register uint64_t x2 asm("x2") = arg2;
    register uint64_t x3 asm("x3") = arg3;
    register uint64_t x4 asm("x4") = arg4;
    register uint64_t x5 asm("x5") = arg5;
    register uint64_t x8 asm("x8") = syscall_idx;

    __asm__ volatile (
        "svc #0"
        : "+r" (x0)
        : "r" (x1), "r" (x2), "r" (x3), "r" (x4), "r" (x5), "r" (x8)
        : "memory"
    );

    return x0;
}

static inline void syscall_console_write(const char *message) {
    syscall_call(SYSCALL_CONSOLE_WRITE, (uint64_t)message, 0, 0, 0, 0, 0);
}

static inline void syscall_yield(void) {
    syscall_call(SYSCALL_YIELD, 0, 0, 0, 0, 0, 0);
}

static inline void syscall_exit(void) {
    syscall_call(SYSCALL_EXIT, 0, 0, 0, 0, 0, 0);
}

static inline uint64_t syscall_mmap(uint64_t address, uint64_t length, uint64_t flags, handle_t fd) {
    return syscall_call(SYSCALL_MMAP, address, length, flags, (uint64_t)fd, 0, 0);
}

static inline void syscall_munmap(void *address, uint64_t length) {
    syscall_call(SYSCALL_MUNMAP, (uint64_t)address, length, 0, 0, 0, 0);
}

static inline uint64_t syscall_shm_open(const char *path, uint64_t size, handle_t *fd) {
    return syscall_call(SYSCALL_SHM_OPEN, (uint64_t)path, size, (uint64_t)fd, 0, 0, 0);
}

static inline uint64_t syscall_shm_unlink(const char *path) {
    return syscall_call(SYSCALL_SHM_UNLINK, (uint64_t)path, 0, 0, 0, 0, 0);
}

static inline uint64_t syscall_channel_open(const char *path, handle_t *fd) {
    return syscall_call(SYSCALL_CHANNEL_OPEN, (uint64_t)path, (uint64_t)fd, 0, 0, 0, 0);
}

static inline uint64_t syscall_channel_close(handle_t fd) {
    return syscall_call(SYSCALL_CHANNEL_CLOSE, (uint64_t)fd, 0, 0, 0, 0, 0);
}

static inline uint64_t syscall_channel_send(handle_t fd, const void *data, uint64_t size) {
    return syscall_call(SYSCALL_CHANNEL_SEND, (uint64_t)fd, (uint64_t)data, size, 0, 0, 0);
}

static inline uint64_t syscall_channel_recv(handle_t fd, void *data, uint64_t size) {
    return syscall_call(SYSCALL_CHANNEL_RECV, (uint64_t)fd, (uint64_t)data, size, 0, 0, 0);
}
