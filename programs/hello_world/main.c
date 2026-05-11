#include "syscall.h"

int main(void) {
    syscall_console_write("Hello, world!\r\n");
    return 0;
}
