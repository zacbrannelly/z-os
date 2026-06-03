#include <stddef.h>
#include <libz/console.h>
#include "compositor.h"

int main(void) {
    if (compositor_init() < 0) {
        console_write("compositor: compositor_init failed\r\n");
        return 1;
    }
    console_write("compositor: compositor_init succeeded\r\n");

    return compositor_run();
}
