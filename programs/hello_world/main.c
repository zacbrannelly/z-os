#include <stddef.h>
#include <libz/console.h>
#include <libz/syscall.h>
#include <libcompositor/window.h>

int main(void) {
    handle_t window_handle;
    if (window_create("Hello, Windows!", 480, 320, &window_handle) < 0) {
        console_write("hello_world: window_create failed\r\n");
        return 1;
    }
    console_write("hello_world: window_create succeeded\r\n");

    while (1) {
        syscall_yield();
    }

    return 0;
}
