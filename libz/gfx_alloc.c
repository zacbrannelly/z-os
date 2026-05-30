#include "malloc.h"

void *gfx_alloc(uint64_t size) {
    return malloc(size);
}

void gfx_free(void *address) {
    free(address);
}
