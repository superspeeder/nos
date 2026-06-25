#pragma once

#include <stddef.h>
#include <stdint.h>

typedef struct __attribute__((packed)) mbi_header {
    uint32_t total_size;
    uint32_t reserved;
} mbi_header_t;

typedef struct __attribute__((packed)) mbi_tag_header {
    uint32_t type;
    uint32_t size;
} mbi_tag_header_t;


typedef struct __attribute__((packed)) mbi_fb_color_descriptor {
    uint8_t red_value;
    uint8_t green_value;
    uint8_t blue_value;
} mbi_fb_color_descriptor_t;

typedef struct __attribute__((packed)) mbi_fb_color_info_indexed {
    uint32_t                  framebuffer_palette_num_colors;
    mbi_fb_color_descriptor_t framebuffer_palette[];
} mbi_fb_color_info_indexed_t;

typedef struct __attribute__((packed)) mbi_fb_color_info_direct {
    uint8_t framebuffer_red_field_position;
    uint8_t framebuffer_red_mask_size;
    uint8_t framebuffer_green_field_position;
    uint8_t framebuffer_green_mask_size;
    uint8_t framebuffer_blue_field_position;
    uint8_t framebuffer_blue_mask_size;
} mbi_fb_color_info_direct_t;

typedef struct __attribute__((packed)) mbi_fb_info_tag {
    mbi_tag_header_t header;
    uint64_t         framebuffer_addr;
    uint32_t         framebuffer_pitch;
    uint32_t         framebuffer_width;
    uint32_t         framebuffer_height;
    uint8_t          framebuffer_bpp;
    uint8_t          framebuffer_type;
    uint8_t          _reserved;
    union __attribute__((packed)) {
        mbi_fb_color_info_indexed_t indexed;
        mbi_fb_color_info_direct_t  direct;
    } color_info;
} mbi_fb_info_tag_t;

typedef struct __attribute__((packed)) mbi_basic_mem_info_tag {
    mbi_tag_header_t header;
    uint32_t mem_lower;
    uint32_t mem_higher;
} mbi_basic_mem_info_tag_t;

typedef struct __attribute__((packed)) mbi_bios_boot_dev_tag {
    mbi_tag_header_t header;
    uint32_t biosdev;
    uint32_t partition;
    uint32_t sub_partition;
} mbi_bios_boot_dev_tag_t;

typedef struct __attribute__((packed)) mbi_boot_command_line_tag {
    mbi_tag_header_t header;
    char string[];
} mbi_boot_command_line_tag_t;

typedef struct __attribute__((packed)) mbi_modules_tag {
    mbi_tag_header_t header;
    uint32_t mod_start;
    uint32_t mod_end;
    char string[];
} mbi_modules_tag_t;

typedef struct __attribute__((packed)) mbi_elf_symbols_tag {
    mbi_tag_header_t header;
    uint16_t num;
    uint16_t entsize;
    uint16_t shndx;
    uint16_t _reserved;
    uint8_t section_headers[]; // TODO: read about how to parse this data.
} mbi_elf_symbols_tag_t;

typedef struct __attribute__((packed)) mbi_memory_map_tag_entry {
    uint64_t base_addr;
    uint64_t length;
    uint32_t type;
    uint32_t reserved;
} mbi_memory_map_tag_entry_t;

typedef struct __attribute__((packed)) mbi_memory_map_tag {
    mbi_tag_header_t header;
    uint32_t entry_size;
    uint32_t entry_version;
    uint8_t entries[]; // Note: Parse this using mbi_parse_memory_map(tag, entry_acceptor) to ensure proper parsing.
} mbi_memory_map_tag_t;


typedef struct __attribute__((packed)) mbi_bootloader_name_tag {
    mbi_tag_header_t header;
    char string[];
} mbi_bootloader_name_tag_t;

typedef struct __attribute__((packed)) mbi_apm_table_tag {
    mbi_tag_header_t header;
    uint16_t version;
    uint16_t cseg;
    uint32_t offset;
    uint16_t cseg_16;
    uint16_t dseg;
    uint16_t flags;
    uint16_t cseg_len;
    uint16_t cseg_16_len;
    uint16_t dseg_len;
} mbi_apm_table_tag_t;

typedef struct __attribute__((packed)) mbi_vbe_info_tag {
    mbi_tag_header_t header;
    uint16_t vbe_mode;
    uint16_t vbe_interface_seg;
    uint16_t vbe_interface_off;
    uint16_t vbe_interface_len;
    uint8_t vbe_control_info[512];
    uint8_t vbe_mode_into[256];
} mbi_vbe_info_tag_t;

typedef struct __attribute__((packed)) mbi_efi_32bit_system_table_ptr_tag {
    mbi_tag_header_t header;
    uint32_t pointer;
} mbi_efi_32bit_system_table_ptr_tag_t;

typedef struct __attribute__((packed)) mbi_efi_64bit_system_table_ptr_tag {
    mbi_tag_header_t header;
    uint64_t pointer;
} mbi_efi_64bit_system_table_ptr_tag_t;

typedef struct __attribute__((packed)) mbi_smbios_tables_tag {
    mbi_tag_header_t header;
    uint8_t major;
    uint8_t minor;
    uint8_t _reserved[6];
    uint8_t smbios_tables[]; // TODO: look into parsing these.
} mbi_smbios_tables_tag_t;

typedef struct __attribute__((packed)) mbi_acpi_old_rsdp_tag {
    mbi_tag_header_t header;
    uint8_t rsdp[]; // TODO: replace type with actual struct for rsdp v1.
} mbi_acpi_old_rsdp_tag_t;

typedef struct __attribute__((packed)) mbi_acpi_new_rsdp_tag {
    mbi_tag_header_t header;
    uint8_t rsdp[]; // TODO: replace type with actual struct for rsdp v2.
} mbi_acpi_new_rsdp_tag_t;

typedef struct __attribute__((packed)) mbi_networking_information_tag {
    mbi_tag_header_t header;
    uint8_t dhcp_ack[]; // TODO: parsing
} mbi_networking_information_tag_t;

typedef struct __attribute__((packed)) mbi_efi_memory_map_tag {
    mbi_tag_header_t header;
    uint32_t descriptor_size;
    uint32_t descriptor_version;
    uint8_t efi_memory_map[]; // TODO: parsing
} mbi_efi_memory_map_tag_t;

typedef struct __attribute__((packed)) mbi_efi_32bit_image_handle_ptr_tag {
    mbi_tag_header_t header;
    uint32_t pointer;
} mbi_efi_32bit_image_handle_ptr_tag_t;

typedef struct __attribute__((packed)) mbi_efi_64bit_image_handle_ptr_tag {
    mbi_tag_header_t header;
    uint64_t pointer;
} mbi_efi_64bit_image_handle_ptr_tag_t;

typedef struct __attribute__((packed)) mbi_image_load_base_physical_address_tag {
    mbi_tag_header_t header;
    uint32_t pointer;
} mbi_image_load_base_physical_address_tag_t;


enum mbi_tag_type {
    MBI_TAG_BASIC_MEMORY_INFO = 4,
    MBI_TAG_BIOS_BOOT_DEVICE = 5,
    MBI_TAG_BOOT_COMMAND_LINE = 1,
    MBI_TAG_MODULES = 3,
    MBI_TAG_ELF_SYMBOLS = 9,
    MBI_TAG_MEMORY_MAP = 6,
    MBI_TAG_BOOTLOADER_NAME = 2,
    MBI_TAG_APM_TABLE = 10,
    MBI_TAG_VBE_INFO = 7,
    MBI_TAG_FRAMEBUFFER_INFO = 8,
    MBI_TAG_EFI_32BIT_SYSTEM_TABLE_POINTER = 11,
    MBI_TAG_EFI_64BIT_SYSTEM_TABLE_POINTER = 12,
    MBI_TAG_SMBIOS_TABLES = 13,
    MBI_TAG_ACPI_OLD_RSDP = 14,
    MBI_TAG_ACPI_NEW_RSDP = 15,
    MBI_TAG_NETWORKING_INFORMATION = 16,
    MBI_TAG_EFI_MEMORY_MAP = 17,
    MBI_TAG_EFI_BOOT_SERVICES_NOT_TERMINATED = 18,
    MBI_TAG_EFI_32BIT_IMAGE_HANDLE_POINTER = 19,
    MBI_TAG_EFI_64BIT_IMAGE_HANDLE_POINTER = 20,
    MBI_TAG_IMAGE_LOAD_BASE_PHYSICAL_ADDRESS = 21,
};

struct mbi_info_results {
    mbi_fb_info_tag_t* fbi;
};

extern struct mbi_info_results mbi_info_results;

// Helper function for parsing multiboot info, calls a function for each tag in the mbi struct.
void parse_mbi(void (*tag_acceptor)(mbi_tag_header_t *tag));

// Function to process the mbi struct. This uses parse_mbi internally and outputs to mbi_info_results
void process_mbi();

void mbi_parse_memory_map(mbi_memory_map_tag_t* tag, void(*entry_acceptor)(mbi_memory_map_tag_entry_t* entry));