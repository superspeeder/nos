//
// Created by andy on 6/23/26.
//

#include "pmem.h"

#include "cpu/paging.h"
#include "drivers/uart.h"
#include "kutil.h"
#include <stdint.h>

#define LOWMEM_END 0xa0000
#define EBDA_START 0x80000

static struct regionmap_entry *_regionmap = nullptr;
extern void                   *mbi_data;
typedef struct __attribute__((packed)) _mbi_header {
    uint32_t total_size;
    uint32_t reserved;
} _mbi_header_t;


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


/**
 * @brief Find a contiguous region of memory in a memory bitmap. Note: Does not mark. This is so that the caller manages any other checks required for that mark to work.
 *
 * TODO: Potentially take in search cursor index instead of always starting at 0 (due to fragmentation might improve speeds).
 *
 * @param bitmap A pointer to the uint64 array that is the memory bitmap.
 * @param bitmap_length The number of uint64 values in the bitmap
 * @param required_contiguous_size The number of pages to be allocated.
 * @return The bit index of the page start in the bitmap. If no allocation could be made, returns ~0ULL.
 */
static uint64_t _alloc_contiguous_availability_bitmap(uint64_t *bitmap, size_t bitmap_length, size_t required_contiguous_size) {
    ///
    /// Plan for how to scan for regions:
    ///
    /// 1. Find an available page
    /// 2. Produce a bitmask for all the remaining bits (clamp the number of bits to set in this mask to the number of remaining bits in the entry so we don't check incorrectly,
    /// otherwise if we need less bits we should instead shift left so that the mask starts right after the found page).
    /// 3. Check the bitmask against the entry, if all the bits present in the bitmask are set in the entry, then this is valid (or we need to move on to the next entry, depending
    /// on the earlier clamping behavior). Note: it's probably fastest to make that clamp the only branch, but we should check because it might also screw with branch prediction
    /// and lookahead for all I know.
    /// 4. if the number of remaining pages in the entry is less than the required, but the entry is available for the remaining pages it represents, continue as follows:
    ///     a. Check the following entries using basic binary and operations and bitmasks to check if they fit the required region.
    ///     b. While doing this, continue to update `ecopy` and the entry index so that we don't attempt to cover extra ground if the region is not functional.
    ///     c. Also, this process requires that we make a copy of the required size and subtract the checked size so far in order to make sure that we don't incorrectly judge the
    ///     amount of memory required or available.
    /// 5. Once we have found a usable region, mask all the bits in the bitmap and update the region's page available counter to match.
    ///
    ///
    /// TODO: Support alignment > 4096. (Likely, we will support two alignment modes: 4 KB (page alignment) and 256 KB (bitmap entry alignment). This higher alignment only should
    /// be used sparingly, as it will likely cause severe memory fragmentation).
    ///

    size_t   current_entry_index = 0;
    uint64_t current_entry       = bitmap[current_entry_index];
    uint64_t current_start_bitindex;
    size_t   current_start_index;
    while (current_entry_index < bitmap_length) {
        if (current_entry == 0) {
            if (++current_entry_index >= bitmap_length) {
                break;
            }
            current_entry = bitmap[current_entry_index];
            continue; // no availability.
        }

        // 1.
        size_t remaining_size  = required_contiguous_size;
        current_start_index    = current_entry_index;
        current_start_bitindex = bsf64(current_entry);

        uint64_t size_available_in_entry = 0;

        // 2.
        uint64_t bits_in_entry = 64 - current_start_bitindex;
        uint64_t mask;
        if (bits_in_entry > remaining_size) {
            mask                    = ((1 << remaining_size) - 1);
            size_available_in_entry = remaining_size;
        } else {
            mask                    = (1 << bits_in_entry) - 1;
            size_available_in_entry = bits_in_entry;
        }

        mask <<= current_start_bitindex;

        // 3.
        if ((current_entry & mask) != mask) { // no fit.
            // Mask out open bits in entry copy
            uint64_t endscan   = ~(current_entry & ((1 << current_start_bitindex) - 1));
            uint64_t end       = bsf64(endscan);
            uint64_t range_len = (end - current_start_bitindex);
            uint64_t rmask     = ~(((1 << range_len) - 1) << current_start_bitindex);
            current_entry &= rmask;
            continue;
        }

        remaining_size -= size_available_in_entry;

        while (remaining_size && current_entry_index < bitmap_length) {
            current_entry_index += 1;
            current_entry   = bitmap[current_entry_index];
            uint64_t bmask2 = (1ULL << remaining_size) - 1;
            if ((current_entry & bmask2) == bmask2) {
                remaining_size -= remaining_size > 64 ? 64 : remaining_size;
            } else {
                break; // not available.
            }
        }

        if (remaining_size > 0) {
            continue;
        }

        return current_start_index * 64 + current_start_bitindex;
    }

    return ~0ULL;
}

#define PMEM_BITINDEX_MAP_INDEX(bi) (bi >> 6)
#define PMEM_BITINDEX_MAP_BITINDEX(bi) (bi & 0x3f)

static void _pmem_mark_contiguous_pages_used(uint64_t *bitmap, uint64_t bitindex, size_t size) {
    uint64_t eidx              = PMEM_BITINDEX_MAP_INDEX(bitindex);
    uint64_t ebidx             = PMEM_BITINDEX_MAP_BITINDEX(bitindex);
    uint64_t num_to_fill_start = 64 - ebidx;
    num_to_fill_start          = num_to_fill_start > size ? size : num_to_fill_start;
    uint64_t init_mask         = num_to_fill_start >= 64 ? ~0ULL : ((1ULL << num_to_fill_start) - 1) << ebidx;
    bitmap[eidx] &= ~init_mask;
    size -= num_to_fill_start;
    while (size > 0) {
        uint64_t num_to_fill = size > 64 ? 64 : size;
        uint64_t mask        = num_to_fill >= 64 ? ~0ULL : (1ULL << num_to_fill) - 1;
        bitmap[++eidx] &= ~mask;
        size -= num_to_fill;
    }
}

static void _pmem_mark_contiguous_pages_available(uint64_t *bitmap, uint64_t bitindex, size_t size) {
    uint64_t eidx              = PMEM_BITINDEX_MAP_INDEX(bitindex);
    uint64_t ebidx             = PMEM_BITINDEX_MAP_BITINDEX(bitindex);
    uint64_t num_to_fill_start = 64 - ebidx;
    num_to_fill_start          = num_to_fill_start > size ? size : num_to_fill_start;
    uint64_t init_mask         = num_to_fill_start >= 64 ? ~0ULL : ((1ULL << num_to_fill_start) - 1) << ebidx;
    bitmap[eidx] |= init_mask;
    size -= num_to_fill_start;
    while (size > 0) {
        uint64_t num_to_fill = size > 64 ? 64 : size;
        uint64_t mask        = num_to_fill >= 64 ? ~0ULL : (1ULL << num_to_fill) - 1;
        bitmap[++eidx] |= mask;
        size -= num_to_fill;
    }
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
static size_t   _num_regions                        = 0;

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
    if (_regionmap == nullptr)
        return nullptr;
    for (size_t rbitmap_idx = 0; rbitmap_idx < RMAP_MAX_ENTRIES / 64; ++rbitmap_idx) {
        uint64_t rbent = rmap_rbitmap[rbitmap_idx];
        if (rbent == 0)
            continue;
        uint64_t                regidx = bsf64(rbent) + (rbitmap_idx << 6);
        struct regionmap_entry *region = &_regionmap[regidx];
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
    for (size_t idx = 0; idx < _num_regions; ++idx) {
        struct regionmap_entry *region = &_regionmap[idx];
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

static bool _region_contains_address(struct regionmap_entry *region, void *ptr) {
    uint64_t a = (uint64_t)ptr;
    if (a < region->base_addr)
        return false;
    if (a >= (region->base_addr + (region->size >> 12)))
        return false;
    return true;
}

static void *_pmem_get_pages(uint64_t size) {
    for (uint64_t i = 0; i < _num_regions; i++) {
        if (_regionmap[i].available_count < size) {
            continue;
        }

        uint64_t bms = _alloc_contiguous_availability_bitmap(_regionmap[i].bitmap, _regionmap[i].size / 64, size);
        if (bms == ~0ULL) {
            continue;
        }

        _pmem_mark_contiguous_pages_used(_regionmap[i].bitmap, bms, size);
        _regionmap[i].available_count -= size;
        return (void *)(_regionmap[i].base_addr + (bms << 12));
    }
    return nullptr;
}

static void _pmem_free_pages(void *mem, uint64_t size) {
    struct regionmap_entry *region = _pmem_find_region_containing(mem);
    uint64_t                a      = (uint64_t)mem;
    a -= region->base_addr;
    _pmem_mark_contiguous_pages_available(region->bitmap, a >> 12, size);
    region->available_count += size;
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

    _mbi_header_t *mbih = mbi_data;

    if ((uint64_t)mbih < EBDA_START) {
        for (uint64_t a = ALIGN_DOWN((uint64_t)mbih, PGSZ_L1); a < (uint64_t)mbih + mbih->total_size; a += PGSZ_L1) {
            _pmem_mark_lowmem_page_used(a);
        }
        LOG("Marked multiboot data pages used");
    } else {
        for (size_t i = 0; i < mbih->total_size; i++) {
            uint64_t dest    = 0x4000 + i;
            uint64_t src     = ((uint64_t)mbih) + i;
            *(uint8_t *)dest = *(uint8_t *)src;
        }

        uint64_t pages = ALIGN_UP(mbih->total_size, PGSZ_L1) / PGSZ_L1;
        for (uint64_t i = 0; i < pages; i++) {
            _pmem_mark_lowmem_page_used(0x4000 + (0x1000 * i));
        }
        mbi_data = (void *)0x4000; // move mbi data into lowmem so we can protect it easier in this stage.
    }

    _regionmap = _pmem_get_lowmem_page();
}

static void _bmset(uint64_t *bitmap, uint64_t size) {
    uint64_t easyfill = size >> 3;
    uint64_t rem      = size - (easyfill << 3);
    memset(bitmap, 0xFF, easyfill);
    bitmap[easyfill] = (1 << rem) - 1; // set last entry to match.
}

void _pmem_insert_new_region(uint64_t addr, uint64_t size, void *bitmap) {
    _regionmap[_num_regions].base_addr       = addr;
    _regionmap[_num_regions].bitmap          = bitmap;
    _regionmap[_num_regions].alloc_cursor    = 0;
    _regionmap[_num_regions].available_count = size >> 12;
    _regionmap[_num_regions].size            = size >> 12;
    _num_regions++;

    // Mark regions as available.
    size_t s = (size >> 18) << 3;
    memset(bitmap, 0, s);
    _bmset(bitmap, size >> 12);

    LOG("Created new pmem region:");
    LOGVAL_HEX(addr);
    LOGVAL_HEX(size);
    LOGVAL_HEX(bitmap);
}

void pmem_create_usable_region(uint64_t paddr, uint64_t size) {
    LOGVAL_HEX(paddr);
    LOGVAL_HEX(size);
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

    if (aligned_size <= 0x1000000) { // needs only one page for a bitmap.
        void *page = _pmem_get_page();
        _pmem_insert_new_region(aligned_addr, aligned_size, page);
        return;
    }

    if (_num_regions == 0) {
        // first, we need a region to work with so we can allocate enough pages for the other bitmaps.
        // We can fit 64 uint64_ts into a page, or about 4096 pages (16 MiB).
        uint64_t sizing = 0x1000000;
        void    *page   = _pmem_get_page();
        _pmem_insert_new_region(aligned_addr, sizing, page);
        aligned_addr += sizing;
        aligned_size -= sizing;
    }

    for (uint64_t i = 0; i < aligned_size; i += RMAP_ENTRY_MAX_PAGE_COUNT * 0x1000ULL) {
        uint64_t ad    = aligned_addr + i;
        uint64_t npg   = (aligned_size < RMAP_ENTRY_MAX_PAGE_COUNT * 0x1000ULL ? aligned_size : RMAP_ENTRY_MAX_PAGE_COUNT * 0x1000) / 0x1000;
        uint64_t bms   = ALIGN_UP(npg, 0x40) / 0x40;
        uint64_t bmpgs = ALIGN_UP(bms, 0x40) / 0x40; // number of pages needed for bitmap.
        void    *pages = _pmem_get_pages(bmpgs);
        _pmem_insert_new_region(ad, npg << 12, pages);
    }

    LOGVAL(_num_regions);
}


void *pmem_alloc_page() {
    return _pmem_get_page();
}

void pmem_free_page(void *page) {
    _pmem_free_page(page);
}

void *pmem_alloc_pages(size_t count) {
    return _pmem_get_pages(count);
}

// This only works with pages allocated using pmem_alloc_pages.
void pmem_free_pages(size_t count, void *pages) {
    _pmem_free_pages(pages, count);
}

uint64_t pmem_get_total_available() {
    uint64_t total = 0;
    for (uint64_t i = 0; i < _num_regions; i++) {
        total += _regionmap[i].available_count;
    }

    return total << 12;
}

page_alloc_t pmem_pgalloc = {
    .alloc = pmem_alloc_page,
    .free  = pmem_free_page,
};
