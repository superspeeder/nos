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

void parse_mbi(void (*tag_acceptor)(mbi_tag_header_t *tag));
