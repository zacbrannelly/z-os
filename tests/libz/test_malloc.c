#include "test.h"
#include <stddef.h>
#include <libz/malloc.h>
#include <libz/assert.h>

TEST_CASE(test_malloc) {
    void *ptr = malloc(1024);
    assert(ptr != NULL);
    free(ptr);
}
