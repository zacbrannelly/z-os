#pragma once

#include <stdint.h>
#include <libz/handle.h>

// Forward declarations.
typedef struct compositor_t compositor_t;
typedef struct compositor_abi_payload_t compositor_abi_payload_t;

int window_abi_create(
    compositor_t *compositor,
    compositor_abi_payload_t *message,
    compositor_abi_payload_t *response
);

int window_abi_attach_buffer(
    compositor_t *compositor,
    compositor_abi_payload_t *message,
    compositor_abi_payload_t *response
);

int window_abi_commit(
    compositor_t *compositor,
    compositor_abi_payload_t *message,
    compositor_abi_payload_t *response
);
