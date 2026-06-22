#include "cpu/paging.h"
#include "drivers/uart.h"
#include "kutil.h"
#include "multiboot.h"
#include <stddef.h>
#include <stdint.h>

__attribute__((noreturn)) static void hcf() {
    asm volatile("cli");
    while (1) {
        asm volatile("hlt");
    }
}

page_table_t __attribute__((aligned(4096))) initialization_page_tables[8];

linked_list_node_t initpgtbl_lln[8] = {
    {
        .next  = &initpgtbl_lln[1],
        .prev  = nullptr,
        .value = &initialization_page_tables[0],
    },
    {
        .next  = &initpgtbl_lln[2],
        .prev  = &initpgtbl_lln[0],
        .value = &initialization_page_tables[1],
    },
    {
        .next  = &initpgtbl_lln[3],
        .prev  = &initpgtbl_lln[1],
        .value = &initialization_page_tables[2],
    },
    {
        .next  = &initpgtbl_lln[4],
        .prev  = &initpgtbl_lln[2],
        .value = &initialization_page_tables[3],
    },
    {
        .next  = &initpgtbl_lln[5],
        .prev  = &initpgtbl_lln[3],
        .value = &initialization_page_tables[4],
    },
    {
        .next  = &initpgtbl_lln[6],
        .prev  = &initpgtbl_lln[4],
        .value = &initialization_page_tables[5],
    },
    {
        .next  = &initpgtbl_lln[7],
        .prev  = &initpgtbl_lln[5],
        .value = &initialization_page_tables[6],
    },
    {
        .next  = 0L,
        .prev  = &initpgtbl_lln[6],
        .value = &initialization_page_tables[7],
    },
};
linked_list_t initpgtbl_ll = {
    .head = &initpgtbl_lln[0],
    .tail = &initpgtbl_lln[7],
};

void *alloc_page() {
    linked_list_node_t *pgnode = llpop_front(&initpgtbl_ll);
    return pgnode->value;
}

void free_page(void *) {
    // no-op. does not support freeing.
}

page_alloc_t pgalloc = {
    .alloc = alloc_page,
    .free  = free_page,
};


uint32_t          *framebuffer;
mbi_fb_info_tag_t *fbi;

void mbi_tag_acceptor(mbi_tag_header_t *tag) {
    switch (tag->type) {
    case 8: {
        fbi         = (mbi_fb_info_tag_t *)tag;
        framebuffer = (uint32_t *)fbi->framebuffer_addr;
    } break;
    default:
        break;
    }
}

void map_framebuffer() {
    const size_t memsize = fbi->framebuffer_height * fbi->framebuffer_pitch;
    pg_map_range_alloc((uint64_t)framebuffer, memsize, PT_PRESENT | PT_WRITABLE, &pgalloc);
}


void kernel_main() {
    uart_active_port = COM1;
    uart_init();
    uart_puts("Hello!\r\n");

    parse_mbi(mbi_tag_acceptor);
    map_framebuffer();

    for (int i = 0; i < fbi->framebuffer_height; i++) {
        for (int j = 0; j < fbi->framebuffer_width; j++) {
            framebuffer[j + i * fbi->framebuffer_pitch / 4] = 0x0000ff | ((i * 255 / fbi->framebuffer_height) << 16) | ((j * 255 / fbi->framebuffer_width) << 8);
        }
    }

    /*
    parse_mbi();

    write_num_hex_ser((uint64_t)framebuffer);
    write_serial('\n');
    write_num_dec((uint64_t)fbi->framebuffer_width);
    write_serial('\t');
    write_num_dec((uint64_t)fbi->framebuffer_height);
    write_serial('\t');
    write_num_dec((uint64_t)fbi->framebuffer_bpp);
    write_serial('\n');

    map_framebuffer();

    // hcf();

    */
    hcf();
}
