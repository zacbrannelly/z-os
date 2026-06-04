#include "comms.h"

#include <stddef.h>
#include <libz/console.h>
#include <libz/assert.h>
#include <libz/handle.h>
#include <libz/channel.h>
#include <libz/assert.h>
#include <libz/process.h>
#include "compositor_abi.h"

static handle_t g_comms_fd = -1;

static int ensure_channel_open(void) {
    if (g_comms_fd >= 0) return 0;
    return channel_open(COMPOSITOR_ABI_CHANNEL_PATH, &g_comms_fd, 0);
}

int comms_send_request(
    compositor_abi_payload_t *message,
    compositor_abi_payload_t *response
) {
    if (message == NULL || response == NULL) {
        return -1;
    }
    assert(ensure_channel_open() == 0);

    message->process_handle = process_get_handle();
    if (channel_send(g_comms_fd, message, sizeof(compositor_abi_payload_t)) < 0) {
        return -1;
    }

    response->process_handle = -1;
    while (response->process_handle != message->process_handle) {
        if (channel_recv(g_comms_fd, response, sizeof(compositor_abi_payload_t)) < 0) {
            return -1;
        }
    }

    return 0;
}
