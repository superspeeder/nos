#include "paging.h"
#include "drivers/uart.h"
#include "kutil.h"

static void uart_putsvln(const char* pre, uint64_t i) {
    uart_puts(pre);
    uart_puts(": 0x");
    uart_putint_hex(i);
    uart_write('\n');
}

static void uart_putsvlnf(const char* fn, const char* pre, uint64_t i) {
    uart_puts(fn);
    uart_putsvln(pre, i);
}

#define STRF(m) m

#define LOGVAL(name) uart_putsvlnf(__PRETTY_FUNCTION__, "()::" #name , (uint64_t)((name)))

__attribute__((noreturn)) static void hcf() {
    asm volatile("cli");
    while (1) {
        asm volatile("hlt");
    }
}

static inline uint64_t _get_pg_entry(page_table_t *tbl, size_t index) {
    return tbl->entries[index];
}

static inline uint64_t _get_l4_entry(size_t index) {
    return _get_pg_entry(PML4T, index);
}

uint64_t pg_get_mapping_for(uint64_t virtual_address, uint16_t *page_flags) {
    uint64_t l4_idx   = PGTBL_INDEX_L4(virtual_address);
    uint64_t l4_entry = _get_l4_entry(l4_idx);

    if (!CHECK_FLAG(l4_entry, PT_PRESENT))
        return ~0ULL;

    if (CHECK_FLAG(l4_entry, PT_PAGE_SIZE)) {
        uint64_t aligned_vaddr = (uint64_t)ALIGN_MEMORY_ADDRESS_DOWN(virtual_address, PGSZ_L4);
        uint64_t offset        = virtual_address - aligned_vaddr;
        uint64_t physaddr_al   = l4_entry & PGTBL_ADDRESS_MASK;
        if (page_flags)
            *page_flags = (uint16_t)(l4_entry & PGTBL_PROPS_MASK);
        uint64_t added = physaddr_al + offset;
        return added;
    }

    page_table_t *pt_l3    = PGTBL_FROM_ENTRY(l4_entry);
    uint64_t      l3_idx   = PGTBL_INDEX_L3(virtual_address);
    uint64_t      l3_entry = _get_pg_entry(pt_l3, l3_idx);

    if (!CHECK_FLAG(l3_entry, PT_PRESENT))
        return ~0ULL;

    if (CHECK_FLAG(l3_entry, PT_PAGE_SIZE)) {
        uint64_t aligned_vaddr = (uint64_t)ALIGN_MEMORY_ADDRESS_DOWN(virtual_address, PGSZ_L3);
        uint64_t offset        = virtual_address - aligned_vaddr;
        uint64_t physaddr_al   = l3_entry & PGTBL_ADDRESS_MASK;
        if (page_flags)
            *page_flags = (uint16_t)(l3_entry & PGTBL_PROPS_MASK);
        uint64_t added = physaddr_al + offset;
        return added;
    }

    page_table_t *pt_l2    = PGTBL_FROM_ENTRY(l3_entry);
    uint64_t      l2_idx   = PGTBL_INDEX_L2(virtual_address);
    uint64_t      l2_entry = _get_pg_entry(pt_l2, l2_idx);

    if (!CHECK_FLAG(l2_entry, PT_PRESENT))
        return ~0ULL;

    if (CHECK_FLAG(l2_entry, PT_PAGE_SIZE)) {
        uint64_t aligned_vaddr = (uint64_t)ALIGN_MEMORY_ADDRESS_DOWN(virtual_address, PGSZ_L2);
        uint64_t offset        = virtual_address - aligned_vaddr;
        uint64_t physaddr_al   = l2_entry & PGTBL_ADDRESS_MASK;
        if (page_flags)
            *page_flags = (uint16_t)(l2_entry & PGTBL_PROPS_MASK);
        uint64_t added = physaddr_al + offset;
        return added;
    }

    page_table_t *pt_l1    = PGTBL_FROM_ENTRY(l2_entry);
    uint64_t      l1_idx   = PGTBL_INDEX_L1(virtual_address);
    uint64_t      l1_entry = _get_pg_entry(pt_l1, l1_idx);

    if (!CHECK_FLAG(l1_entry, PT_PRESENT))
        return ~0ULL;

    uint64_t aligned_vaddr = (uint64_t)ALIGN_MEMORY_ADDRESS_DOWN(virtual_address, PGSZ_L1);
    uint64_t offset        = virtual_address - aligned_vaddr;
    uint64_t physaddr_al   = l1_entry & PGTBL_ADDRESS_MASK;
    if (page_flags)
        *page_flags = (uint16_t)(l1_entry & PGTBL_PROPS_MASK);

    uint64_t added = physaddr_al + offset;
    return added;
}

bool pg_map_noalloc(uint64_t virtual_address, uint64_t physical_address, uint16_t page_flags) {
    page_flags &= PGTBL_PROPS_MASK; // invalid flags may cause serious issues, so avoid that.

    // the alignment of both addresses is ensured as well. Do note that using unaligned addresses in this function call may result in what seems to be improperly mapped memory.
    virtual_address &= PGTBL_ADDRESS_MASK;
    physical_address &= PGTBL_ADDRESS_MASK;

    uint64_t l4_idx = PGTBL_INDEX_L4(virtual_address);
    uint64_t l3_idx = PGTBL_INDEX_L3(virtual_address);
    uint64_t l2_idx = PGTBL_INDEX_L2(virtual_address);
    uint64_t l1_idx = PGTBL_INDEX_L1(virtual_address);

    uint64_t l4_entry = _get_l4_entry(l4_idx);
    if (!CHECK_FLAG(l4_entry, PT_PRESENT) || CHECK_FLAG(l4_entry, PT_PAGE_SIZE)) {
        return false;
    }
    page_table_t *pt_l3 = PGTBL_FROM_ENTRY(l4_entry);

    uint64_t l3_entry = _get_pg_entry(pt_l3, l3_idx);
    if (!CHECK_FLAG(l3_entry, PT_PRESENT) || CHECK_FLAG(l3_entry, PT_PAGE_SIZE)) {
        return false;
    }
    page_table_t *pt_l2 = PGTBL_FROM_ENTRY(l3_entry);

    uint64_t l2_entry = _get_pg_entry(pt_l2, l2_idx);
    if (!CHECK_FLAG(l2_entry, PT_PRESENT) || CHECK_FLAG(l2_entry, PT_PAGE_SIZE)) {
        return false;
    }
    page_table_t *pt_l1 = PGTBL_FROM_ENTRY(l2_entry);

    uint64_t l1_entry = _get_pg_entry(pt_l1, l1_idx);
    if (CHECK_FLAG(l1_entry, PT_PRESENT)) {
        return false; // l1 entry already exists.
    }

    pt_l1->entries[l1_idx] = physical_address | page_flags;
    return true;
}

bool pg_map_alloc(uint64_t virtual_address, uint64_t physical_address, uint16_t page_flags, page_alloc_t *pgalloc) {
    page_flags &= PGTBL_PROPS_MASK; // invalid flags may cause serious issues, so avoid that.

    // the alignment of both addresses is ensured as well. Do note that using unaligned addresses in this function call may result in what seems to be improperly mapped memory.
    virtual_address &= PGTBL_ADDRESS_MASK;
    physical_address &= PGTBL_ADDRESS_MASK;

    uint64_t l4_idx = PGTBL_INDEX_L4(virtual_address);
    uint64_t l3_idx = PGTBL_INDEX_L3(virtual_address);
    uint64_t l2_idx = PGTBL_INDEX_L2(virtual_address);
    uint64_t l1_idx = PGTBL_INDEX_L1(virtual_address);

    uint64_t      l4_entry = _get_l4_entry(l4_idx);
    page_table_t *pt_l3;
    if (!CHECK_FLAG(l4_entry, PT_PRESENT)) {
        void *newpg = pgalloc->alloc();
        if (newpg == nullptr) {
            return false; // couldn't alloc page
        }
        memset(newpg, 0, sizeof(page_table_t));
        pt_l3                  = newpg;
        PML4T->entries[l4_idx] = pg_get_mapping_for((uint64_t)newpg, nullptr) | PT_PRESENT | PT_WRITABLE;
    } else if (CHECK_FLAG(l4_entry, PT_PAGE_SIZE)) {
        return false;
    } else {
        pt_l3 = PGTBL_FROM_ENTRY(l4_entry);
    }

    uint64_t      l3_entry = _get_pg_entry(pt_l3, l3_idx);
    page_table_t *pt_l2;
    if (!CHECK_FLAG(l3_entry, PT_PRESENT)) {
        void *newpg = pgalloc->alloc();
        if (newpg == nullptr) {
            return false; // couldn't alloc page
        }
        memset(newpg, 0, sizeof(page_table_t));
        pt_l2                  = newpg;
        pt_l3->entries[l3_idx] = pg_get_mapping_for((uint64_t)newpg, nullptr) | PT_PRESENT | PT_WRITABLE;
    } else if (CHECK_FLAG(l3_entry, PT_PAGE_SIZE)) {
        return false;
    } else {
        pt_l2 = PGTBL_FROM_ENTRY(l3_entry);
    }

    uint64_t      l2_entry = _get_pg_entry(pt_l2, l2_idx);
    page_table_t *pt_l1;
    if (!CHECK_FLAG(l2_entry, PT_PRESENT)) {
        void *newpg = pgalloc->alloc();
        if (newpg == nullptr) {
            return false; // couldn't alloc page
        }
        memset(newpg, 0, sizeof(page_table_t));
        pt_l1                  = newpg;
        pt_l2->entries[l2_idx] = pg_get_mapping_for((uint64_t)newpg, nullptr) | PT_PRESENT | PT_WRITABLE;
    } else if (CHECK_FLAG(l2_entry, PT_PAGE_SIZE)) {
        return false;
    } else {
        pt_l1 = PGTBL_FROM_ENTRY(l2_entry);
    }

    uint64_t l1_entry = _get_pg_entry(pt_l1, l1_idx);
    if (CHECK_FLAG(l1_entry, PT_PRESENT)) {
        return false; // l1 entry already exists.
    }

    pt_l1->entries[l1_idx] = physical_address | page_flags;
    return true;
}

bool pg_map_range_alloc(const uint64_t virtual_address_start, const size_t size, const uint16_t page_flags, page_alloc_t *pgalloc) {
    uint64_t aligned_addr = (uint64_t)ALIGN_MEMORY_ADDRESS_DOWN(virtual_address_start, PGSZ_L1);
    uint64_t end_addr     = virtual_address_start + size;
    uint64_t n            = 0;
    for (uint64_t i = aligned_addr; i < end_addr; i += PGSZ_L1) {
        if (!pg_map_alloc(i, i, PT_PRESENT | PT_WRITABLE, pgalloc)) {
            return false;
        }
        n++;
    }
    return true;
}
