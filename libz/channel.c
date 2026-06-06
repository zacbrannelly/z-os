#include "channel.h"
#include "syscall.h"

int channel_open(const char *path, handle_t *fd, int flags) {
    return syscall_channel_open(path, fd, flags);
}

int channel_close(handle_t fd) {
    return syscall_channel_close(fd);
}

int channel_send(handle_t fd, const void *data, uint64_t size) {
    return syscall_channel_send(fd, data, size);
}

int channel_recv(handle_t fd, void *data, uint64_t size) {
    return syscall_channel_recv(fd, data, size);
}

int channel_send_fd(handle_t channel_fd, handle_t fd, const void *data, uint64_t size) {
    return syscall_channel_send_fd(channel_fd, fd, data, size);
}

int channel_recv_fd(handle_t channel_fd, handle_t* fd, void *data, uint64_t size) {
    return syscall_channel_recv_fd(channel_fd, fd, data, size);
}
