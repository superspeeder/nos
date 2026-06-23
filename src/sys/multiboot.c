#include "multiboot.h"
#include "sys/kutil.h"
#include "drivers/uart.h"

extern void *mbi_data;

void parse_mbi(void (*tag_acceptor)(mbi_tag_header_t *tag)) {
    const mbi_header_t *header = mbi_data;
    const uint64_t      tag_end = (uint64_t)mbi_data + header->total_size;
    auto                tag    = (mbi_tag_header_t *)((uint8_t *)mbi_data + sizeof(mbi_header_t));
    while ((uint64_t)tag < tag_end) {
        if (tag->type == 0)
            break;

        tag_acceptor(tag);
        tag = ALIGN_MEMORY_ADDRESS_UP((uint8_t *)tag + tag->size, 8);
    }
}

struct mbi_info_results mbi_info_results;

void mbi_tag_acceptor(mbi_tag_header_t *tag) {
    LOGVAL(tag->type);
    switch (tag->type) {
    case 8: {
        mbi_info_results.fbi         = (mbi_fb_info_tag_t *)tag;
    } break;
    default:
        break;
    }
}

void process_mbi() {
    parse_mbi(mbi_tag_acceptor);
}
