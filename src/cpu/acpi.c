//
// Created by andy on 6/23/26.
//

#include "acpi.h"

#include <stddef.h>
#include <stdbool.h>

#define EBDA_POINTER 0x40E
#define EBDA_SIZE 0x400
#define ACPITABLES_START 0x000E0000
#define ACPITABLES_END   0x000FFFFF

static const char RSDP_SIG[8] = "RSD PTR ";

static bool _check_signature(uint8_t* p, const char sig[8]) {
    for (size_t i = 0 ; i < 8 ; i++) {
        if (*(p + i) != sig[i]) {
            return false;
        }
    }
    return true;
}

acpi_rsdp_t *acpi_find_rsdp_table(struct page_alloc* pgalloc) {
    uint8_t* ebda = (uint8_t*)(uint64_t)*(uint16_t *)EBDA_POINTER;
    pg_map_range_alloc((uint64_t)ebda, EBDA_SIZE, PT_PRESENT | PT_WRITABLE, pgalloc);
    for (size_t i = 0 ; i < EBDA_SIZE ; ++i) {
        if (_check_signature(ebda + i, RSDP_SIG)) {
            return (acpi_rsdp_t*)(ebda + i);
        }
    }

    pg_map_range_alloc(ACPITABLES_START, ACPITABLES_END - ACPITABLES_START, PT_PRESENT | PT_WRITABLE, pgalloc);
    uint8_t* acpi_tables = (uint8_t*)ACPITABLES_START;
    uint8_t* acpi_tables_end = (uint8_t*)ACPITABLES_END;

    for (;acpi_tables < acpi_tables_end; acpi_tables++) {
        if (_check_signature(acpi_tables, RSDP_SIG)) {
            return (acpi_rsdp_t*)acpi_tables;
        }
    }

    return nullptr;
}

bool acpi_validate_checksum(const void *data, const size_t size) {
    const uint8_t* data_u = data;
    uint8_t sum = 0;
    for (size_t i = 0 ; i < size ; ++i) {
        sum += data_u[i];
    }
    return sum == 0;
}
