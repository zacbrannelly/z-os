#include "shm.h"
#include "syscall.h"

int shm_open(const char *path, uint64_t size, handle_t *fd) {
    return syscall_shm_open(path, size, fd);
}

int shm_unlink(const char *path) {
    return syscall_shm_unlink(path);
}
