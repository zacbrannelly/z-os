#include "memory.h"

void memory_set(void *address, uint8_t value, uint64_t size) {
    for (uint64_t i = 0; i < size; i++) {
        *(uint8_t *)(address + i) = value;
    }
}

void memory_copy(void *destination, void *source, uint64_t size) {
    for (uint64_t i = 0; i < size; i++) {
        *(uint8_t *)(destination + i) = *(uint8_t *)(source + i);
    }
}
