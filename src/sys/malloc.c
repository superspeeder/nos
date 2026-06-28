#include "sys/malloc.h"
#include "sys/kutil.h"

///
/// Internally, kmalloc uses a buddy allocator to manage kernel heap memory.
///
///

typedef struct buddy_allocator {
    linked_list_t blocks;
} buddy_allocator_t;

enum buddy_allocator_flags {
    BA_ALLOCATED = 0b1,

    // This indicates that a block is allocated as part of a different allocation (i.e. this will be freed when another address is freed).
    // Non-primary allocations cannot be freed using _ba_free() and must instead be freed via _ba_set_free() which does not touch.
    //
    // Non-primary allocations are also considered part of the most recently seen allocated node which is found via breadth-first-search.
    // This flags lack of presence acts as a terminator to an allocation.
    // The node which starts a large block must set BA_LARGE or it will leak memory (see BA_LARGE).
    //
    BA_NONPRIMARY = 0b10,

    // This indicates that this block is contiguous with the previous block.
    // This is used for allocations.
    // All child blocks *should* set this value.
    // Toplevel parent blocks must *never* set this if the next block in the linked list is not contiguous (that linked list must remain sorted).
    // This is required for toplevel so that the caller of kmalloc can allocate large chunks of memory without issue.
    BA_CONTIGUOUS = 0b100,

    // This indicates that a node is toplevel. This changes the behavior of _ba_free().
    // When not present, _ba_free() works as normal (use _ba_set_free() then return this node to the free pool).
    // When present, _ba_free() will instead just make this node unallocated (but it stays in the blocks list).
    // To free the underlying physical memory, you need to use _ba_release() which only works on blocks with BA_TOPLEVEL set and BA_NONPRIMARY not set.
    BA_TOPLEVEL = 0b1000,

    // Indicates that this block is part of an allocation larger than this block.
    // When present, _ba_free() will step forward in the
    BA_LARGE = 0b10000,
};


// ALigned to 64-bytes for cache friendliness.
typedef struct __attribute__((aligned(64))) ba_node {
    uint64_t base_addr;
    // Node children.
    struct ba_node *left;
    struct ba_node *right;
    // Parent node. If toplevel this is actually the linked list node which holds this block (allowing for traversal of the toplevel linked list starting from a node, important for
    // large allocation management).
    struct ba_node *parent;

    uint16_t flags;
    uint16_t
        order; // Blocks are always sized by powers of two. this represents the power of two this block is (this could be a uint8_t but is uint16_t because of alignment reasons).
} ba_node_t;

static buddy_allocator_t _buddy_allocator = {.blocks = {
                                                 .head = nullptr,
                                                 .tail = nullptr,
                                             }};


// These are kinda weird in structure, but they use the left and right pointers as next and prev for the linked list.
typedef struct ba_node_list {
    ba_node_t *head;
    ba_node_t *tail;
} ba_node_list_t;

// This is populated by _ba_alloc_nodes(); which allocates 64 nodes at a time (1 page of memory).
static ba_node_list_t _ba_node_pool = {
    .head = nullptr,
    .tail = nullptr,
};

static void _ba_create_tl_block(uint64_t base_addr, size_t size) {
    ///
    /// This function is slightly misleading in name since it may create several blocks .
    ///
}
