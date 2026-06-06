#pragma once

#include <stdint.h>
#include <libz/handle.h>

// Forward declarations.
typedef struct compositor_abi_payload_t compositor_abi_payload_t;

int comms_send_request(
    compositor_abi_payload_t *message,
    compositor_abi_payload_t *response
);

int comms_send_request_fd(
    handle_t fd_to_send,
    compositor_abi_payload_t *message,
    compositor_abi_payload_t *response
);
