#include "compositor_abi.h"

#include <stddef.h>
#include <libz/memory.h>
#include <libz/channel.h>
#include <libz/console.h>
#include <libz/file.h>
#include <libcompositor/compositor_abi.h>

#include "window.h"
#include "../compositor.h"

int compositor_abi_init(compositor_t *compositor) {
    if (compositor == NULL) {
        return -1;
    }

    // Open channel for communication with other processes.
    if (channel_open("/compositor/channel/0", &compositor->channel_fd, O_NONBLOCK) < 0) {
        console_write("compositor_abi: channel_open failed\r\n");
        return -1;
    }
    console_write("compositor_abi: channel_open succeeded\r\n");

    return 0;
}

int compositor_abi_poll(compositor_t *compositor) {
    if (compositor == NULL) {
        return -1;
    }

    compositor_abi_payload_t message;
    memory_set(&message, 0, sizeof(compositor_abi_payload_t));

    if (channel_recv(compositor->channel_fd, &message, sizeof(compositor_abi_payload_t)) < 0) {
        // No message to process.
        return 0;
    }

    console_write("compositor_abi: received message\r\n");

    // Process message.
    compositor_abi_payload_t response;
    switch (message.type) {
        case COMPOSITOR_ABI_TYPE_CREATE_WINDOW_REQUEST:
            window_abi_create(compositor, &message, &response);
            break;
        default:
            console_write("compositor_abi: unknown payload type\r\n");
            return -1;
    }

    // Send response.
    if (channel_send(compositor->channel_fd, &response, sizeof(compositor_abi_payload_t)) < 0) {
        console_write("compositor_abi: channel_send failed\r\n");
        return -1;
    }
    console_write("compositor_abi: response sent\r\n");

    return 0;
}
