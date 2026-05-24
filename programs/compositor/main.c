#include <stddef.h>
#include <libz/syscall.h>
#include <libz/shm.h>
#include <libz/mmap.h>
#include <libz/channel.h>
#include <libz/memory.h>

int main(void) {
    // Create shared memory object for a window.
    handle_t fd;
    if (shm_open("compositor/window/0", 1024, &fd) != 0) {
        syscall_console_write("compositor: shm_open failed\r\n");
        return 1;
    }
    syscall_console_write("compositor: shm_open succeeded\r\n");

    // Map the shared memory object to the process's address space.
    void *address = (void *)mmap(0, 1024, MAP_SHARED | MAP_READ | MAP_WRITE, fd);
    if (address == NULL) {
        syscall_console_write("compositor: mmap failed\r\n");
        return 1;
    }
    syscall_console_write("compositor: mmap succeeded\r\n");

    // Read the message from the shared memory object.
    char *message = (char *)address;
    syscall_console_write("compositor: message: ");
    syscall_console_write(message);
    syscall_console_write("\r\n");

    handle_t channel_fd;
    if (channel_open("compositor/channel/0", &channel_fd) != 0) {
        syscall_console_write("compositor: channel_open failed\r\n");
        return 1;
    }
    syscall_console_write("compositor: channel_open succeeded\r\n");

    char channel_buffer[100];
    memory_set((void *)channel_buffer, 0, sizeof(channel_buffer));

    if (channel_recv(channel_fd, channel_buffer, sizeof(channel_buffer)) < 0) {
        syscall_console_write("compositor: channel_recv failed\r\n");
        return 1;
    }

    syscall_console_write("compositor: channel_recv succeeded\r\n");
    syscall_console_write("compositor: message: ");
    syscall_console_write(channel_buffer);
    syscall_console_write("\r\n");

    return 0;
}
