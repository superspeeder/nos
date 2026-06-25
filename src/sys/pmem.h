//
// Created by andy on 6/23/26.
//

#pragma once

#include <stdint.h>
#include <stddef.h>

void pmem_init_lowmem_bitmap_used();
void pmem_create_usable_region(uint64_t paddr, uint64_t size);
