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

int8_t memory_compare(void *a, void *b, uint64_t size) {
    for (uint64_t i = 0; i < size; i++) {
        if (*(uint8_t *)(a + i) < *(uint8_t *)(b + i)) {
            return -1;
        }
        if (*(uint8_t *)(a + i) > *(uint8_t *)(b + i)) {
            return 1;
        }
    }
    return 0;
}
