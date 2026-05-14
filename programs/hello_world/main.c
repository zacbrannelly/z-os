#include <libz/syscall.h>
#include <libz/malloc.h>

int main(void) {
    // Allocate memory for a message
    char *msg = (char *)malloc(32);
    if (msg == 0) {
        syscall_console_write("malloc failed\r\n");
        return 1;
    }

    // Write a message into the allocated memory
    msg[0] = 'H';
    msg[1] = 'e';
    msg[2] = 'l';
    msg[3] = 'l';
    msg[4] = 'o';
    msg[5] = ',';
    msg[6] = ' ';
    msg[7] = 'm';
    msg[8] = 'a';
    msg[9] = 'l';
    msg[10] = 'l';
    msg[11] = 'o';
    msg[12] = 'c';
    msg[13] = '!';
    msg[14] = '\r';
    msg[15] = '\n';
    msg[16] = 0;

    syscall_console_write(msg);

    // Free the memory
    free(msg);

    return 0;
}
