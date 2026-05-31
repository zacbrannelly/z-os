#include "file.h"
#include "syscall.h"

int open(const char *path, handle_t *fd) {
    return syscall_open(path, fd);
}

int read(handle_t fd, void *buffer, uint64_t size) {
    return syscall_read(fd, buffer, size);
}

int write(handle_t fd, const void *buffer, uint64_t size) {
    return syscall_write(fd, buffer, size);
}

int close(handle_t fd) {
    return syscall_close(fd);
}
