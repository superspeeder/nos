//
// Created by andy on 6/23/26.
//

#pragma once

#include <stdint.h>
#include "cpu/paging.h"

typedef struct __attribute__((packed)) acpi_rsdp {
    char     signature[8];
    uint8_t  checksum;
    char     oemid[6];
    uint8_t  revision;
    uint32_t rsdt_address;
} acpi_rsdp_t;

typedef struct __attribute__((packed)) acpi_xsdp {
    char     signature[8];
    uint8_t  checksum;
    char     oemid[6];
    uint8_t  revision;
    uint32_t rsdt_address; // deprecated since version 2.0

    uint32_t length;
    uint64_t xsdt_address;
    uint8_t  extended_checksum;
    uint8_t  reserved[3];
} acpi_xsdp_t;

acpi_rsdp_t* acpi_find_rsdp_table(struct page_alloc* pgalloc);
bool acpi_validate_checksum(const void* data, size_t size);