#pragma once

#include <stdint.h>

static inline int strcmp(const char *s1, const char *s2) {
    while (1) {
        if (*s1 != *s2) {
            return *s1 - *s2;
        }

        if (*s1 == '\0') {
            return 0;
        }

        s1++;
        s2++;
    }
}

static inline int strncmp(const char *s1, const char *s2, uint64_t count) {
    for (uint64_t i = 0; i < count; i++) {
        if (s1[i] != s2[i]) {
            return s1[i] - s2[i];
        }

        if (s1[i] == '\0') {
            break;
        }
    }

    return 0;
}

static inline int strlen(const char *s) {
    int length = 0;
    while (1) {
        if (*s == '\0') {
            break;
        }
        length++;
        s++;
    }
    return length;
}
