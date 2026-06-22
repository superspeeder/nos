#pragma once
#include <stddef.h>
#include <stdint.h>
#define ALIGN_MEMORY_ADDRESS_UP(addr, alignment) ((void *)(((uint64_t)(addr) + ((uint64_t)(alignment) - 1ULL)) & (~((uint64_t)(alignment) - 1ULL))))
#define ALIGN_MEMORY_ADDRESS_DOWN(addr, alignment) ((void *)((uint64_t)(addr) & ~((uint64_t)(alignment) - 1ULL)))
#define CHECK_FLAG(value, bit) (((value) & (bit)) == (bit))

typedef struct linked_list_node {
    struct linked_list_node *next;
    struct linked_list_node *prev;
    void                    *value;
} linked_list_node_t;

typedef struct linked_list {
    linked_list_node_t *head, *tail;
} linked_list_t;

// for now, this simple linked list impl is basically just a deque, and doesn't support random access operations.

void                llpush_front(linked_list_t *ll, linked_list_node_t *node);
void                llpush_back(linked_list_t *ll, linked_list_node_t *node);
linked_list_node_t *llpop_front(linked_list_t *ll);
linked_list_node_t *llpop_back(linked_list_t *ll);

void *memset(void *dst, int c, size_t n);
