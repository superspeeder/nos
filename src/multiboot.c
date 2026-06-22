#include "multiboot.h"
#include "kutil.h"

extern void *mbi_data;

void parse_mbi(void (*tag_acceptor)(mbi_tag_header_t *tag)) {
    mbi_header_t     *header = mbi_data;
    uint64_t          tagend = (uint64_t)mbi_data + header->total_size;
    mbi_tag_header_t *tag    = (mbi_tag_header_t *)((uint8_t *)mbi_data + sizeof(mbi_header_t));
    while ((uint64_t)tag < tagend) {
        if (tag->type == 0)
            break;

        tag_acceptor(tag);
        tag = ALIGN_MEMORY_ADDRESS_UP((uint8_t *)tag + tag->size, 8);
    }
}
