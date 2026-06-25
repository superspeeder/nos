//
// Created by andy on 6/23/26.
//

#include "pmem.h"

#include "cpu/paging.h"
#include "kutil.h"

#define LOWMEM_END 0xa0000
#define EBDA_START 0x80000

static struct regionmap_entry *_regionmap_page = nullptr;

static inline uint64_t bsf64(uint64_t n) {
    uint64_t out = 0;
    asm volatile("bsf %0, %%rax" : "=a"(out) : "r"(n));
    return out;
}

static inline uint32_t bsf32(uint64_t n) {
    uint32_t out = 0;
    asm volatile("bsf %0, %%eax" : "=a"(out) : "r"(n));
    return out;
}

static uint32_t lowmem_bitmap[LOWMEM_END >> 17];

static void _pmem_mark_lowmem_page_used(uint64_t paddr) {
    size_t  index  = paddr >> 17;
    size_t  bitidx = (paddr >> 12) & 0x7;
    uint8_t mask   = 1 << bitidx;
    lowmem_bitmap[index] &= ~mask; // on bits represent available
}

static void _pmem_mark_lowmem_page_available(uint64_t paddr) {
    size_t   index  = paddr >> 17;
    size_t   bitidx = (paddr >> 12) & 0x7;
    uint32_t mask   = 1 << bitidx;
    lowmem_bitmap[index] |= mask; // on bits represent available
}

static void *_pmem_get_lowmem_page() {
    for (size_t i = 0; i < 5; i++) {
        if (lowmem_bitmap[i]) {
            uint32_t entry = lowmem_bitmap[i];
            uint32_t out   = bsf32(entry);
            uint64_t addr  = (i << 17) | (out << 12);
            lowmem_bitmap[i] &= ~(1 << out);
            return (void *)addr;
        }
    }
    return nullptr;
}

struct regionmap_entry {
    uint64_t  base_addr;
    size_t    size;
    uint64_t *bitmap;
    uint32_t  available_count;  // this value is maintained so that we can easily skip checking full regions.
    uint32_t  alloc_cursor;     // this is the bitmap entry containing the last allocated page.
} __attribute__((aligned(32))); // aligned so that we never have entries which are on different cache lines

enum {
    RMAP_ENTRY_MAX_PAGE_COUNT = 2147483648, // excessive but this way we can store a count (in case for some reason you manage to have 8 TiB of contiguous usable RAM, this would
                                            // have to go into multiple regions).
    RMAP_MAX_ENTRIES = 128,                 // enough to fit in a single page
};

static uint64_t rmap_rbitmap[RMAP_MAX_ENTRIES / 64] = {0, 0}; // by default, no available bitmaps
static size_t   num_regions                         = 0;

static void *_pmem_alloc_page_from_region(struct regionmap_entry *region) {
    // as long as the presence of pages doesn't unsync, we hopefully won't be wasting time with this.

    uint32_t cursor = region->alloc_cursor;
    for (uint32_t i = cursor; i < region->size; i++) {
        uint64_t bitmap_entry = region->bitmap[i];
        if (bitmap_entry == 0)
            continue;
        uint64_t bitidx = bsf64(bitmap_entry);
        uint64_t mask   = 1ULL << bitidx;
        region->bitmap[i] &= ~mask;
        region->available_count--;
        return (void *)(region->base_addr + (i * 64 + bitidx) * 0x1000);
    }

    for (uint32_t i = 0; i < cursor; i++) {
        uint64_t bitmap_entry = region->bitmap[i];
        if (bitmap_entry == 0)
            continue;
        uint64_t bitidx = bsf64(bitmap_entry);
        uint64_t mask   = 1ULL << bitidx;
        region->bitmap[i] &= ~mask;
        region->available_count--;
        return (void *)(region->base_addr + (i * 64 + bitidx) * 0x1000);
    }
    return nullptr; // no available pages.
}

static void *_pmem_alloc_page_from_rmap() {
    if (_regionmap_page == nullptr)
        return nullptr;
    for (size_t rbitmap_idx = 0; rbitmap_idx < RMAP_MAX_ENTRIES / 64; ++rbitmap_idx) {
        uint64_t rbent = rmap_rbitmap[rbitmap_idx];
        if (rbent == 0)
            continue;
        uint64_t                regidx = bsf64(rbent) + (rbitmap_idx << 6);
        struct regionmap_entry *region = &_regionmap_page[regidx];
        void                   *page   = _pmem_alloc_page_from_region(region);
        if (region->available_count == 0) {
            rmap_rbitmap[rbitmap_idx] &= ~(1ULL << regidx); // mark that region is unavailable.
        }
        if (page != nullptr) {
            return page;
        }
    }
    return nullptr; // no available pages in any regions.
}

static struct regionmap_entry *_pmem_find_region_containing(void *ptr) {
    const uint64_t addr = (uint64_t)ptr;
    for (size_t idx = 0; idx < num_regions; ++idx) {
        struct regionmap_entry *region = &_regionmap_page[idx];
        if (region->base_addr <= addr && region->base_addr + region->size > addr) {
            return region;
        }
    }
    return nullptr;
}

static void _pmem_free_page_region(struct regionmap_entry *region, void *ptr) {
    const uint64_t local_addr     = (uint64_t)ALIGN_MEMORY_ADDRESS_DOWN((uint64_t)ptr - region->base_addr, PGSZ_L1);
    const size_t   bitmap_idx     = local_addr >> 18;
    const size_t   bitmap_bit_idx = (local_addr >> 12) & 0x3f;
    const uint64_t mask           = 1 << bitmap_bit_idx;
    const uint64_t old_mask       = region->bitmap[bitmap_idx];
    region->bitmap[bitmap_idx] |= mask; // mark as available.
    if ((old_mask & mask) == 0)
        ++(region->available_count);
}

static void _pmem_free_page(void *ptr) {
    const uint64_t addr = (uint64_t)ptr;
    if (addr < LOWMEM_END) {
        _pmem_mark_lowmem_page_available(addr);
    } else {
        struct regionmap_entry *region = _pmem_find_region_containing(ptr);
        if (region) {
            _pmem_free_page_region(region, ptr);
        }
    }
}

static void *_pmem_get_page() {
    void *lowmem_page = _pmem_get_lowmem_page();
    if (lowmem_page == nullptr) {
        return _pmem_alloc_page_from_rmap();
    }
    return lowmem_page;
}

void pmem_init_lowmem_bitmap_used() {
    memset(lowmem_bitmap, 0xff, 20);                            // mark all as available
    _pmem_mark_lowmem_page_used(0x0000);                        // bda (runs into *some* usable but not enough to be worth caring about).
    _pmem_mark_lowmem_page_used(0x1000);                        // l4 pt
    _pmem_mark_lowmem_page_used(0x2000);                        // original l3 pt
    _pmem_mark_lowmem_page_used(0x3000);                        // original l2 pt
    for (size_t i = EBDA_START; i < LOWMEM_END; i += PGSZ_L1) { // ebda
        _pmem_mark_lowmem_page_used(i);
    }

    _regionmap_page = _pmem_get_lowmem_page();
}


void pmem_create_usable_region(uint64_t paddr, uint64_t size) {
    if (paddr < LOWMEM_END) {
        if (paddr + size <= LOWMEM_END) {
            return; // region fully exists in lowmem, we don't want to make a region for that.
        }
        size -= (LOWMEM_END - paddr); // cut anything before lowmem end
        paddr = LOWMEM_END;
    }

    uint64_t aligned_addr   = ALIGN_UP(paddr, PGSZ_L1); // round up to nearest page start.
    uint64_t corrected_size = size - (aligned_addr - paddr);
    uint64_t aligned_size   = ALIGN_DOWN(corrected_size, PGSZ_L1); // make sure we don't try to partially
    for (uint64_t i = 0; i < aligned_size; i += RMAP_ENTRY_MAX_PAGE_COUNT * 0x1000) {
        uint64_t ad    = aligned_addr + i;
        uint64_t npg   = (aligned_size < RMAP_ENTRY_MAX_PAGE_COUNT * 0x1000ULL ? aligned_size : RMAP_ENTRY_MAX_PAGE_COUNT * 0x1000) / 0x1000;
        uint64_t bms   = ALIGN_UP(npg, 0x40) / 0x40;
        uint64_t bmpgs = ALIGN_UP(bms, 0x200) / 0x200; // number of pages needed for bitmap.
        // TODO: write function for allocating multiple contiguous pages both for high and lowmem areas (the only reason we even distinguish between the two is because lowmem being
        // standardized means we can use it's available space to bootstrap space for region bitmaps from the memory map.
    }
}
