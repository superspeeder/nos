//
// Created by andy on 6/23/26.
//

#pragma once

#include <stddef.h>
#include <stdint.h>
#include <cpu/paging.h>

void  pmem_init_lowmem_bitmap_used();
void  pmem_create_usable_region(uint64_t paddr, uint64_t size);
void *pmem_alloc_page();
void  pmem_free_page(void *page);
void *pmem_alloc_pages(size_t count);
// This only works with pages allocated using pmem_alloc_pages.
void pmem_free_pages(size_t count, void *pages);
uint64_t pmem_get_total_available();

extern page_alloc_t pmem_pgalloc;