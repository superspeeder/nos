#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>


// Bitshift values used to calculate page sizes, masks, and index values.
enum {
    PGL4_BITSHL = 39ULL,
    PGL3_BITSHL = 30ULL,
    PGL2_BITSHL = 21ULL,
    PGL1_BITSHL = 12ULL,
};

enum {
    PGTBL_ADDRESS_MASK = 0xfffffffffffff000,
    PGTBL_PROPS_MASK   = (1ULL << PGL1_BITSHL) - 1ULL,
    PGTBL_EIDX_MASK    = 0x1ffULL, // 512 entries per table so each index is between 0-511 (inclusive)
};

#define PGTBL_INDEX_L4(addr) ((((uint64_t)addr) >> PGL4_BITSHL) & PGTBL_EIDX_MASK)
#define PGTBL_INDEX_L3(addr) ((((uint64_t)addr) >> PGL3_BITSHL) & PGTBL_EIDX_MASK)
#define PGTBL_INDEX_L2(addr) ((((uint64_t)addr) >> PGL2_BITSHL) & PGTBL_EIDX_MASK)
#define PGTBL_INDEX_L1(addr) ((((uint64_t)addr) >> PGL1_BITSHL) & PGTBL_EIDX_MASK)
#define PGTBL_FROM_ENTRY(entry) ((page_table_t *)(((uint64_t)(entry)) & PGTBL_ADDRESS_MASK))
#define PGTBL_PROPS_FROM_ENTRY(entry) ((uint64_t)(entry)) & PGTBL_PROPS_MASK)


#define PML4T ((page_table_t *)0x1000)
// PDPT_0 is the pdpt at pml4t[0]
#define PDPT_0 ((page_table_t *)0x2000)
// PDT_0 is the pdt at PDPT_0[0]
#define PDT_0 ((page_table_t *)0x3000)


/**
 * @brief Basic page table struct.
 *
 * Struct matching the format for page tables that the cpu expects.
 * Is always aligned to the start of a page.
 */
typedef struct __attribute__((packed, aligned(4096))) page_table {
    uint64_t entries[512];
} page_table_t;

enum {
    PGSZ_L4 = (1ULL << PGL4_BITSHL),
    PGSZ_L3 = (1ULL << PGL3_BITSHL),
    PGSZ_L2 = (1ULL << PGL2_BITSHL),
    PGSZ_L1 = (1ULL << PGL1_BITSHL),
};

enum {
    PT_PRESENT       = 1 << 0,
    PT_WRITABLE      = 1 << 1,
    PT_USER          = 1 << 2,
    PT_WRITE_THROUGH = 1 << 3,
    PT_CACHE_DISABLE = 1 << 4,
    PT_ACCESSED      = 1 << 5,
    PT_DIRTY         = 1 << 6,
    PT_PAGE_SIZE     = 1 << 7,
    PT_PAT_4K        = 1 << 7,
    PT_GLOBAL        = 1 << 8,
    PT_PAT_HIGHER    = 1 << 12,
};

/**
 * @brief Get the physical address mapped for the provided virtual address (and optionally the flags of the page containing it).
 *
 * This function traverses page tables to try and find a mapping for the provided virtual address.
 * @param virtual_address the virtual address to check the mapping of
 * @param page_flags a pointer to where to write page flags. Flags will not be written if this is NULL.
 * @return If the provided address is mapped, returns the physical address it is mapped to. Otherwise, returns ~0ULL (and page_flags will not be written to).
 */
uint64_t pg_get_mapping_for(uint64_t virtual_address, uint16_t *page_flags);

/**
 * @brief Helper for determining if a page is present or not.
 *
 * This function just uses the pg_get_mapping_for function, so should not be used to try and avoid calling that function (the majority of the overhead of pg_get_mapping_for cannot
 * be avoided in the implementation of this function).
 */
#define PG_IS_MAPPED(virtual_address, page_flags) (pg_get_mapping_for((virtual_address), (page_flags)) != (~0ULL))

/**
 * @brief Try to map memory without allocating.
 *
 * Huge pages are not supported by this function.
 *
 * @param virtual_address The virtual address to map
 * @param physical_address The physical address map destination
 * @param page_flags The flags to map with.
 * @return mapping status (returns true if memory was successfully mapped). May return false if the mapping would require an allocation or if the virtual address falls into an
 * already mapped page. This does not check if the mapped page is mapped to the same destination.
 */
bool pg_map_noalloc(uint64_t virtual_address, uint64_t physical_address, uint16_t page_flags);

/**
 * @brief Basic allocator vtable which allows for allocating pages.
 *
 * Internally, these functions may make use of pg_map_noalloc and pg_unmap in order to make the new page tables usable or free them.
 */
typedef struct page_alloc {
    void *(*alloc)();
    void (*free)(void *);
} page_alloc_t;

bool pg_map_alloc(uint64_t virtual_address, uint64_t physical_address, uint16_t page_flags, page_alloc_t *pgalloc);

bool pg_map_range_alloc(uint64_t virtual_address_start, uint64_t physical_address_start, size_t size, uint16_t page_flags, page_alloc_t *pgalloc);
bool pg_map_range_alloc_ident(uint64_t virtual_address_start, size_t size, uint16_t page_flags, page_alloc_t *pgalloc);
