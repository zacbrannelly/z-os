#pragma once

#include <stdint.h>

// Binaries that link to this library must provide these functions.
void *gfx_alloc(uint64_t size);
void gfx_free(void *address);
