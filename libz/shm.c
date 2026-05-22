#include "shm.h"
#include "syscall.h"

uint64_t shm_open(const char *path, uint64_t size, handle_t *fd) {
    return syscall_shm_open(path, size, fd);
}

uint64_t shm_unlink(const char *path) {
    return syscall_shm_unlink(path);
}
