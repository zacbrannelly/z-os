#include "format.h"

void format_hex(char* buffer, uint64_t buffer_size, uint64_t value) {
    if (value == 0) {
        buffer[0] = '0';
        buffer[1] = '\0';
        return;
    }
    
    int current_pos = 0;
    for (int i = 0; i < 16; i++) {
        int value_at_idx = (value >> (4 * (15 - i))) & 0xf;
        if (value_at_idx == 0 && current_pos == 0) {
            continue;
        }
        buffer[current_pos++] = "0123456789abcdef"[(value >> (4 * (15 - i))) & 0xf];
    }
    buffer[current_pos] = '\0';
}
