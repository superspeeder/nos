#pragma once

#include <stdint.h>

void outb(uint16_t port, uint8_t value);
void outw(uint16_t port, uint16_t value);
void outd(uint16_t port, uint32_t value);

uint8_t inb(uint16_t port);
uint16_t inw(uint16_t port);
uint16_t intd(uint16_t port);