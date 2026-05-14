#include "process_mmap.h"
#include "process.h"
#include "address_space.h"
#include "../page_alloc.h"
#include "../mmap.h"
#include "../assert.h"
#include "../utils/linked_list.h"
#include "../console.h"

#include <stddef.h>

#define HEAP_VIRTUAL_BASE 0x0000400000000000ULL
#define HEAP_SIZE (1ULL << 34) // 16GB

static uint64_t find_free_address_space(process_t *process, uint64_t length) {
    linked_list_node_t *node = process->mmap_entries.head;
    uint64_t free_address_space = HEAP_VIRTUAL_BASE;

    while (node != NULL) {
        mmap_entry_t *entry = (mmap_entry_t *)node->data;
        uint64_t end_address = entry->virtual_address + entry->num_pages * PAGE_SIZE;
        if (end_address >= free_address_space) {
            free_address_space = end_address;
        }
        node = node->next;
    }

    if (free_address_space == HEAP_VIRTUAL_BASE && process->mmap_entries.head != NULL) {
        // No free address space found.
        assert(0);
    }

    assert(free_address_space % PAGE_SIZE == 0);
    return free_address_space;
}

// Map process memory.
uint64_t process_mmap(
    process_t *process,
    uint64_t address,
    uint64_t length,
    uint64_t flags
) {
    // The virtual address must be aligned to 4kb.
    assert(address % PAGE_SIZE == 0);

    if (address == 0) {
        address = find_free_address_space(process, length);
    }

    assert(address >= HEAP_VIRTUAL_BASE);
    assert(address + length <= HEAP_VIRTUAL_BASE + HEAP_SIZE);

    uint64_t num_pages = length / PAGE_SIZE;
    if (length % PAGE_SIZE != 0) {
        num_pages++;
    }

    uint64_t page_flags = PAGE_FLAG_NORMAL_MEMORY | PAGE_FLAG_INNER_SHARABLE;
    if ((flags & MAP_READ) && (flags & MAP_WRITE)) {
        page_flags |= PAGE_FLAG_EL0_EL1_RW;
    } else if (flags & MAP_READ) {
        page_flags |= PAGE_FLAG_EL0_EL1_RO;
    } else if (flags & MAP_WRITE) {
        page_flags |= PAGE_FLAG_EL0_EL1_RW;
    }

    // TODO: Add entries to the mmap_entries linked list.
    for (uint64_t i = 0; i < num_pages; i++) {
        uint64_t page_address = 0;
        // TODO: Handle error cases properly.
        assert(page_alloc_block(PAGE_ALLOC_ORDER_4KB, &page_address) == 0);
        assert(vmap_map_page(&process->address_space.page_table, page_address, address + i * PAGE_SIZE, page_flags) == 0);
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
