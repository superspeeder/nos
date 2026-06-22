#include "ports.h"

void  outb(uint16_t port, uint8_t value) {
    asm("out %%al, %%dx" ::"a"(value), "d"(port));
}

void outw(uint16_t port, uint16_t value) {
    asm("out %%ax, %%dx" ::"a"(value), "d"(port));
}

void outd(uint16_t port, uint32_t value) {
    asm("out %%eax, %%dx" ::"a"(value), "d"(port));
}

uint8_t inb(uint16_t port) {
    uint8_t value;
    asm volatile("in %%dx, %%al" : "=a"(value) : "d"(port));
    return value;
}

uint16_t inw(uint16_t port) {
    uint16_t value;
    asm volatile("in %%dx, %%ax" : "=a"(value) : "d"(port));
    return value;
}

uint32_t ind(uint16_t port) {
    uint32_t value;
    asm volatile("in %%dx, %%eax" : "=a"(value) : "d"(port));
    return value;
}
