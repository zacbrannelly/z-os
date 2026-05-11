#include "address_space.h"
#include "../page_alloc.h"
#include "../mmap.h"

static uint64_t alloc_vmap_page(void) {
    uint64_t page;
    if (page_alloc_block(PAGE_ALLOC_ORDER_4KB, &page) < 0) {
        return 0;
    }
    return page;
}

static uint64_t vmap_physical_to_virtual(uint64_t physical_address) {
    uint64_t virtual_address;
    if (mmap_physical_to_virtual(physical_address, &virtual_address) < 0) {
        return 0;
    }
    return virtual_address;
}

int address_space_init(address_space_t *address_space) {
    if (vmap_init(&address_space->page_table, alloc_vmap_page, vmap_physical_to_virtual) < 0) {
        return -1;
    }
    return 0;
}

int address_space_destroy(address_space_t *address_space) {
    // TODO: Implement.
    return 0;
}
