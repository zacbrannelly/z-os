#include <stddef.h>
#include <libz/console.h>
#include <libz/shm.h>
#include <libz/mmap.h>
#include <libz/channel.h>
#include <libz/memory.h>
#include <libz/syscall.h>
#include <libinput/input.h>

#include <libgfx/bitmap.h>
#include <libgfx/paint.h>
#include <libgfx/colors.h>
#include <libgfx/font.h>

#include "gfx.h"
#include "ui/cursor.h"
#include "ui/text_input.h"

// TODO: Expose these from the kernel instead via file info API or something.
#define FRAMEBUFFER_WIDTH 800
#define FRAMEBUFFER_HEIGHT 600
#define FRAMEBUFFER_SIZE (FRAMEBUFFER_WIDTH * FRAMEBUFFER_HEIGHT * sizeof(uint32_t))

int main(void) {
    console_write("compositor: starting\r\n");

    // Open the input device.
    handle_t mouse_input_fd;
    if (input_open_nonblock("/dev/mouse0", &mouse_input_fd) < 0) {
        console_write("compositor: input_open failed\r\n");
        return 1;
    }

    handle_t keyboard_input_fd;
    if (input_open_nonblock("/dev/keyboard0", &keyboard_input_fd) < 0) {
        console_write("compositor: input_open failed\r\n");
        return 1;
    }
    console_write("compositor: input_open succeeded\r\n");

    // Open the shared memory object for the framebuffer.
    handle_t fd;
    if (shm_open("/dev/fb0", FRAMEBUFFER_SIZE, &fd) != 0) {
        console_write("compositor: shm_open failed\r\n");
        return 1;
    }
    console_write("compositor: shm_open succeeded\r\n");

    // Map the framebuffer to the process's address space.
    uint32_t *framebuffer = (uint32_t *)mmap(0, FRAMEBUFFER_SIZE, MAP_SHARED | MAP_READ | MAP_WRITE, fd);
    if (framebuffer == NULL) {
        console_write("compositor: mmap failed\r\n");
        return 1;
    }
    console_write("compositor: framebuffer mmap succeeded\r\n");

    if (gfx_init(framebuffer, FRAMEBUFFER_WIDTH, FRAMEBUFFER_HEIGHT) < 0) {
        console_write("compositor: gfx_init failed\r\n");
        return 1;
    }
    console_write("compositor: gfx_init succeeded\r\n");

    // Initialize the font system.
    if (font_init() < 0) {
        console_write("compositor: font_init failed\r\n");
        return 1;
    }
    console_write("compositor: font_init succeeded\r\n");

    if (cursor_init(FRAMEBUFFER_WIDTH, FRAMEBUFFER_HEIGHT) < 0) {
        console_write("compositor: cursor_init failed\r\n");
        return 1;
    }
    console_write("compositor: cursor_init succeeded\r\n");

    text_input_t text_input;
    if (text_input_alloc(&text_input) < 0) {
        console_write("compositor: text_input_alloc failed\r\n");
        return 1;
    }
    console_write("compositor: text_input_alloc succeeded\r\n");

    // Create shared memory object for a window.
    handle_t window_fd;
    if (shm_open("compositor/window/0", 1024, &window_fd) != 0) {
        console_write("compositor: shm_open failed\r\n");
        return 1;
    }
    console_write("compositor: shm_open succeeded\r\n");

    // Map the shared memory object to the process's address space.
    void *address = (void *)mmap(0, 1024, MAP_SHARED | MAP_READ | MAP_WRITE, window_fd);
    if (address == NULL) {
        console_write("compositor: mmap failed\r\n");
        return 1;
    }
    console_write("compositor: mmap succeeded\r\n");

    // Read the message from the shared memory object.
    char *message = (char *)address;
    console_write("compositor: message: ");
    console_write(message);
    console_write("\r\n");

    handle_t channel_fd;
    if (channel_open("compositor/channel/0", &channel_fd) != 0) {
        console_write("compositor: channel_open failed\r\n");
        return 1;
    }
    console_write("compositor: channel_open succeeded\r\n");

    char channel_buffer[100];
    memory_set((void *)channel_buffer, 0, sizeof(channel_buffer));

    if (channel_recv(channel_fd, channel_buffer, sizeof(channel_buffer)) < 0) {
        console_write("compositor: channel_recv failed\r\n");
        return 1;
    }

    console_write("compositor: channel_recv succeeded\r\n");
    console_write("compositor: message: ");
    console_write(channel_buffer);
    console_write("\r\n");

    bitmap_t *back_framebuffer = gfx_get_back_framebuffer();

    while (1) {
        // Update cursor.
        cursor_update();

        // Clear screen.
        paint_clear(back_framebuffer, RGB_COLOR_BLACK);

        // Render text input.
        text_input_draw(&text_input);

        // Render cursor.
        cursor_draw();

        input_device_event_t event;
        if (input_read(keyboard_input_fd, &event) > 0) {
            console_write("compositor: input_read succeeded\r\n");

            if (
                event.type == INPUT_DEVICE_EVENT_TYPE_KEY_DOWN_EVENT ||
                event.type == INPUT_DEVICE_EVENT_TYPE_KEY_UP_EVENT
            ) {
                console_write("compositor: keyboard event\r\n");
                console_write("compositor: key: ");
                console_write_hex(event.key_event.keycode);
                console_write("\r\n");
            }
        }
        
        gfx_swap_buffers();
        syscall_yield();
    }

    return 0;
}
