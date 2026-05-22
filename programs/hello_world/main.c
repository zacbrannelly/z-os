#include <stddef.h>
#include <libz/syscall.h>
#include <libz/malloc.h>
#include <libz/shm.h>
#include <libz/mmap.h>

int main(void) {
    // Open shared memory object for a window.
    handle_t fd;
    if (shm_open("compositor/window/0", 1024, &fd) != 0) {
        syscall_console_write("compositor: shm_open failed\r\n");
        return 1;
    }
    syscall_console_write("hello_world: shm_open succeeded\r\n");

    // Map the shared memory object to the process's address space.
    void *address = (void *)mmap(0, 1024, MAP_SHARED | MAP_READ | MAP_WRITE, fd);
    if (address == NULL) {
        syscall_console_write("hello_world: mmap failed\r\n");
        return 1;
    }
    syscall_console_write("hello_world: mmap succeeded\r\n");

    // Write a message into the shared memory object.
    *(char *)address = 'H';
    *(char *)(address + 1) = 'e';
    *(char *)(address + 2) = 'l';
    *(char *)(address + 3) = 'l';
    *(char *)(address + 4) = 'o';
    *(char *)(address + 5) = ',';
    *(char *)(address + 6) = ' ';
    *(char *)(address + 7) = 's';
    *(char *)(address + 8) = 'h';
    *(char *)(address + 9) = 'a';
    *(char *)(address + 10) = 'r';
    *(char *)(address + 11) = 'e';
    *(char *)(address + 12) = 'd';
    *(char *)(address + 13) = ' ';
    *(char *)(address + 14) = 'm';
    *(char *)(address + 15) = 'e';
    *(char *)(address + 16) = 'm';
    *(char *)(address + 17) = 'o';
    *(char *)(address + 18) = 'r';
    *(char *)(address + 19) = 'y';
    *(char *)(address + 20) = ' ';
    *(char *)(address + 21) = 'o';
    *(char *)(address + 22) = 'b';
    *(char *)(address + 23) = 'j';
    *(char *)(address + 24) = 'e';
    *(char *)(address + 25) = 'c';
    *(char *)(address + 26) = 't';
    *(char *)(address + 29) = '\0';
    syscall_console_write("hello_world: message written to shared memory object\r\n");

    return 0;
}
