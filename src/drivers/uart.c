#include "uart.h"
#include "cpu/ports.h"

uint16_t uart_active_port = COM1;

static void _uart_disable_interrupts(uint16_t port) {
    outb(port + 1, 0x00);
}

static void _uart_set_baud_rate_divisor(uint16_t port, uint16_t divisor) {
    outb(port + 3, 0x80);
    outb(port, divisor & 0xFF);
    outb(port + 1, (divisor >> 8) & 0xFF);
}

static void _uart_set_8n1(uint16_t port) {
    outb(port + 3, 0x03);
}

static void _uart_enable_fifo_14(uint16_t port) {
    outb(port + 2, 0xC7);
}

static void _uart_enable_irqs(uint16_t port) {
    outb(port + 4, 0x0B);
}

static void _uart_test(uint16_t port) {
    outb(port + 4, 0x1E);
    outb(port, 0xAE);
    inb(port);
    outb(port + 4, 0x0F);
}


void uart_init() {
    uart_pinit(uart_active_port);
}

int uart_recieved() {
    return uart_preceieved(uart_active_port);
}

char uart_read() {
    return uart_pread(uart_active_port);
}

int uart_transmit_empty() {
    return uart_ptransmit_empty(uart_active_port);
}

void uart_write(char c) {
    uart_pwrite(uart_active_port, c);
}

void uart_pinit(uint16_t port) {
    _uart_disable_interrupts(port);
    _uart_set_baud_rate_divisor(port, 3);
    _uart_set_8n1(port);
    _uart_enable_fifo_14(port);
    _uart_enable_irqs(port);
    _uart_test(port);
}

int uart_preceieved(uint16_t port) {
    return inb(port + 5) & 1;
}

char uart_pread(uint16_t port) {
    while (uart_preceieved(port) == 0)
        ;
    return inb(port);
}

int uart_ptransmit_empty(uint16_t port) {
    return inb(port + 5) & 0x20;
}

void uart_pwrite(uint16_t port, char c) {
    while (uart_ptransmit_empty(port) == 0)
        ;
    outb(port, c);
}

void uart_puts(const char *str) {
    uart_pputs(uart_active_port, str);
}

void uart_putsn(char *str, size_t max_string_size) {
    uart_pputsn(uart_active_port, str, max_string_size);
}

void uart_pputs(uint16_t port, const char *str) {
    while (*str) {
        uart_pwrite(port, *str);
        ++str;
    }
}

void uart_pputsn(uint16_t port, char *str, size_t max_string_size) {
    size_t l = 0;
    while (l < max_string_size && *str) {
        uart_pwrite(port, *str);
        ++str;
        ++l;
    }
}

void uart_putint(uint64_t i) {
    uart_pputint(uart_active_port, i);
}
void uart_pputint(uint16_t port, uint64_t i) {
    if (i > 0xF) {
        uart_pputint(port, i / 10);
    }
    uint8_t dig = i % 10;
    uart_pwrite(port, '0' + dig);
}

void uart_putint_hex(uint64_t i) {
    uart_pputint_hex(uart_active_port, i);
}
void uart_pputint_hex(uint16_t port, uint64_t i) {
    if (i > 0xF) {
        uart_pputint_hex(port, i >> 4);
    }
    uint8_t dig = i & 0xF;
    uart_pwrite(port, dig > 0x9 ? 'A' + (dig - 0xA) : '0' + dig);
}

void uart_putint_hexpad(uint64_t i, uint64_t pad) {
    uart_pputint_hexpad(uart_active_port, i, pad);
}

void uart_pputint_hexpad(uint16_t port, uint64_t i, uint64_t pad) {
    for (uint64_t n = pad; n > 0; n--) {
        uint8_t dig = (i >> ((n - 1) * 4)) & 0xF;
        uart_pwrite(port, dig > 0x9 ? 'A' + (dig - 0xA) : '0' + dig);
    }
}

void uart_putsvln(const char* pre, uint64_t i) {
    uart_puts(pre);
    uart_puts(": ");
    uart_putint(i);
    uart_write('\n');
}

void uart_putsvlnf(const char* fn, const char* pre, uint64_t i) {
    uart_puts(fn);
    uart_putsvln(pre, i);
}

void uart_putsvln_hex(const char* pre, uint64_t i) {
    uart_puts(pre);
    uart_puts(": 0x");
    uart_putint_hex(i);
    uart_write('\n');
}

void uart_putsvlnf_hex(const char* fn, const char* pre, uint64_t i) {
    uart_puts(fn);
    uart_putsvln_hex(pre, i);
}

void uart_log(const char *fn, const char *s) {
    uart_puts(fn);
    uart_puts(s);
    uart_write('\n');
}

void uart_putsvlnfs(const char *sfile, const char *fn, const char *pre, uint64_t i) {
    uart_puts(sfile);
    uart_write(':');
    uart_putsvlnf(fn, pre, i);
}
void uart_putsvlnfs_hex(const char *sfile, const char *fn, const char *pre, uint64_t i) {
    uart_puts(sfile);
    uart_write(':');
    uart_putsvlnf_hex(fn, pre, i);
}
void uart_logs(const char *sfile, const char *fn, const char *s) {
    uart_puts(sfile);
    uart_write(':');
    uart_log(fn, s);
}
