#include "multiboot.h"
#include "drivers/uart.h"
#include "sys/kutil.h"
#include "sys/pmem.h"

extern void *mbi_data;

void parse_mbi(void (*tag_acceptor)(mbi_tag_header_t *tag)) {
    const mbi_header_t *header  = mbi_data;
    const uint64_t      tag_end = (uint64_t)mbi_data + header->total_size;
    auto                tag     = (mbi_tag_header_t *)((uint8_t *)mbi_data + sizeof(mbi_header_t));
    while ((uint64_t)tag < tag_end) {
        if (tag->type == 0)
            break;

        tag_acceptor(tag);
        tag = ALIGN_MEMORY_ADDRESS_UP((uint8_t *)tag + tag->size, 8);
    }
}

struct mbi_info_results mbi_info_results;

const char *mm_typenames[6] = {
    "reserved",
    "usable",
    "reserved",
    "acpi",
    "preserve",
    "defective",
};

static void mbi_log_mmentry(mbi_memory_map_tag_entry_t *entry) {
    uart_puts("base: 0x");
    uart_putint_hexpad(entry->base_addr, 16);
    uart_puts(" size: 0x");
    uart_putint_hexpad(entry->length, 16);
    uart_puts(" type: ");
    uart_puts(entry->type < 6 ? mm_typenames[entry->type] : "reserved");
    uart_write('\n');
}

uint64_t usable_memory_size = 0;

static void mbi_mmap_entry_acceptor(mbi_memory_map_tag_entry_t *entry) {
    mbi_log_mmentry(entry);
    if (entry->type == 1) {
        usable_memory_size += entry->length;
        pmem_create_usable_region(entry->base_addr, entry->length);
    }
}

static void mbi_tag_acceptor(mbi_tag_header_t *tag) {
    LOGVAL(tag->type);
    switch (tag->type) {
    case MBI_TAG_FRAMEBUFFER_INFO: {
        mbi_info_results.fbi = (mbi_fb_info_tag_t *)tag;
    } break;
    case MBI_TAG_MEMORY_MAP: {
        uart_puts("Memory map entries:\n");
        mbi_parse_memory_map((mbi_memory_map_tag_t *)tag, mbi_mmap_entry_acceptor);
        LOGVAL_HEX(usable_memory_size);
    } break;
    default:
        break;
    }
}

void process_mbi() {
    parse_mbi(mbi_tag_acceptor);
}

void mbi_parse_memory_map(mbi_memory_map_tag_t *tag, void (*entry_acceptor)(mbi_memory_map_tag_entry_t *entry)) {
    for (size_t i = offsetof(mbi_memory_map_tag_t, entries); i < tag->header.size; i += tag->entry_size) {
        const auto entry = (mbi_memory_map_tag_entry_t *)((uint8_t *)tag + i);
        entry_acceptor(entry);
    }
}
