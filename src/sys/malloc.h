#pragma once

#include <stddef.h>

/**
 * @brief Basic kernel memory allocation.
 *
 * Returns nullptr if memory is not available.
 * Guaranteed alignment of at least 8-bytes.
 * Minimum allocation size is 8 bytes (as a result of alignment requirements).
 */
void *kmalloc(size_t size);

/**
 * @brief Allocate memory with a specified alignment
 *
 * Alignment must be a power of 2.
 * Memory allocations are always at minimum 8-byte aligned.
 * Max alignment is 4K. (due to page sizes and the physical memory allocator not currently supporting alignment requirements on pages. Theoretically, if enough memory is given to
 * the heap this could be different but that is unrealistic to rely upon).
 */
void *kmalloc_aligned(size_t size, size_t alignment);
