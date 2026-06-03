#pragma once

#include <stdint.h>
#include <libz/handle.h>

// Forward declarations.
typedef struct compositor_t compositor_t;

int compositor_abi_init(compositor_t *compositor);
int compositor_abi_poll(compositor_t *compositor);
