#include "paging.h"

#include "drivers/uart.h"
#include "sys/kutil.h"

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

bool pg_map_range_alloc(const uint64_t virtual_address_start, const uint64_t physical_address_start, const size_t size, const uint16_t page_flags, page_alloc_t *pgalloc) {
    LOGVAL_HEX(virtual_address_start);
    LOGVAL_HEX(physical_address_start);
    LOGVAL_HEX(size);
    uint64_t aligned_addr  = (uint64_t)ALIGN_MEMORY_ADDRESS_DOWN(virtual_address_start, PGSZ_L1);
    uint64_t aligned_paddr = (uint64_t)ALIGN_MEMORY_ADDRESS_DOWN(physical_address_start, PGSZ_L1);
    uint64_t n             = 0;
    for (uint64_t i = 0; i < size; i += PGSZ_L1) {
        if (!pg_map_alloc(aligned_addr + i, aligned_paddr + i, PT_PRESENT | PT_WRITABLE, pgalloc)) {
            return false;
        }
        n++;
    }
    return true;
}

bool pg_map_range_alloc_ident(const uint64_t virtual_address_start, const size_t size, const uint16_t page_flags, page_alloc_t *pgalloc) {
    return pg_map_range_alloc(virtual_address_start, virtual_address_start, size, page_flags, pgalloc);
}
