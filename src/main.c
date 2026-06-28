#include "cpu/acpi.h"
#include "cpu/paging.h"
#include "drivers/uart.h"
#include "sys/kutil.h"
#include "sys/multiboot.h"
#include "sys/pmem.h"
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
        .next  = nullptr,
        .prev  = &initpgtbl_lln[6],
        .value = &initialization_page_tables[7],
    },
};
linked_list_t initpgtbl_ll = {
    .head = &initpgtbl_lln[0],
    .tail = &initpgtbl_lln[7],
};

void *alloc_page() {
    const linked_list_node_t *pgnode = llpop_front(&initpgtbl_ll);
    return pgnode->value;
}

void free_page(void *) {
    // no-op. does not support freeing.
}

page_alloc_t pgalloc = {
    .alloc = alloc_page,
    .free  = free_page,
};

void map_framebuffer() {
    const size_t memsize = mbi_info_results.fbi->framebuffer_height * mbi_info_results.fbi->framebuffer_pitch;
    LOGVAL_HEX(memsize);
    pg_map_range_alloc_ident(mbi_info_results.fbi->framebuffer_addr, memsize, PT_PRESENT | PT_WRITABLE, &pmem_pgalloc);
}


void kernel_main() {
    uart_active_port = COM1;
    uart_init();
    uart_puts("Hello!\r\n");

    pmem_init_lowmem_bitmap_used();

    process_mbi();

    map_framebuffer();

    for (int i = 0; i < mbi_info_results.fbi->framebuffer_height; i++) {
        for (int j = 0; j < mbi_info_results.fbi->framebuffer_width; j++) {
            ((uint32_t *)mbi_info_results.fbi->framebuffer_addr)[j + i * mbi_info_results.fbi->framebuffer_pitch / 4] = 0x0000ff |
                (i * 255 / mbi_info_results.fbi->framebuffer_height) << 16 | (j * 255 / mbi_info_results.fbi->framebuffer_width) << 8;
        }
    }

    acpi_rsdp_t *rsdp = acpi_find_rsdp_table(&pmem_pgalloc);
    if (rsdp) {
        LOGVAL_HEX(rsdp);
    } else {
        LOG("Couldn't find RSDP table.");
    }

    if (acpi_validate_checksum(rsdp, sizeof(acpi_rsdp_t))) {
        LOG("RSDP is valid.");
    } else {
        LOG("RSDP is invalid.");
    }

    if (rsdp->revision == 2) {
        LOG("RSDP revision is 2. Verifying validity.");
        acpi_xsdp_t *xsdp = (acpi_xsdp_t *)rsdp;
        if (acpi_validate_checksum(xsdp, sizeof(acpi_xsdp_t))) {
            LOG("XSDP is valid.");
        } else {
            LOG("XSDP is invalid.");
        }
    }

    uint64_t pmem_avl = pmem_get_total_available();
    LOGVAL_HEX(pmem_avl);

    void *test_pages = pmem_alloc_pages(262144);
    LOGVAL_HEX(test_pages);
    void *test_pages2 = pmem_alloc_pages(16);
    LOGVAL_HEX(test_pages2);
    void *test_pages3 = pmem_alloc_pages(16);
    LOGVAL_HEX(test_pages3);
    void *test_pages4 = pmem_alloc_pages(262144);
    LOGVAL_HEX(test_pages4);
    void *test_pages5 = pmem_alloc_pages(16);
    LOGVAL_HEX(test_pages5);
    pmem_free_pages(262144, test_pages4);
    void *test_pages6 = pmem_alloc_pages(262144);
    LOGVAL_HEX(test_pages6);
    pmem_avl = pmem_get_total_available();
    LOGVAL_HEX(pmem_avl);

    hcf();
}
