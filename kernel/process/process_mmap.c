#include "process_mmap.h"
#include "process.h"
#include "address_space.h"
#include "../page_alloc.h"
#include "../mmap.h"
#include "../assert.h"

#define HEAP_VIRTUAL_BASE 0x0000400000000000ULL
#define HEAP_SIZE (1ULL << 34) // 16GB

// Map process memory.
uint64_t process_mmap(
    process_t *process,
    uint64_t address,
    uint64_t length,
    uint64_t flags
) {
    // This gives the userland process the heap virtual base address.
    if (address == 0) {
        return HEAP_VIRTUAL_BASE;
    }

    assert(address >= HEAP_VIRTUAL_BASE);
    assert(address + length <= HEAP_VIRTUAL_BASE + HEAP_SIZE);

    uint64_t num_pages = length / PAGE_SIZE;
    if (length % PAGE_SIZE != 0) {
        num_pages++;
    }

    for (uint64_t i = 0; i < num_pages; i++) {
        uint64_t page_address = 0;
        // TODO: Handle error cases properly.
        assert(page_alloc_block(PAGE_ALLOC_ORDER_4KB, &page_address) == 0);
        assert(vmap_map_page(&process->address_space.page_table, page_address, address + i * PAGE_SIZE, flags) == 0);
    }

    return address;
}

// Unmap process memory.
void process_munmap(
    process_t *process,
    void *address, 
    uint64_t length
) {
    // TODO: Implement.
    assert(0);
}
