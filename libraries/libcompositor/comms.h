#pragma once

#include <stdint.h>

// Forward declarations.
typedef struct compositor_abi_payload_t compositor_abi_payload_t;

int comms_send_request(
    compositor_abi_payload_t *message,
    compositor_abi_payload_t *response
);
