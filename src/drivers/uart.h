#pragma once

#include <stddef.h>
#include <stdint.h>

#define COM1 0x3f8

extern uint16_t uart_active_port;

void uart_init();
int  uart_recieved();
char uart_read();
int  uart_transmit_empty();
void uart_write(char c);

void uart_pinit(uint16_t port);
int  uart_preceieved(uint16_t port);
char uart_pread(uint16_t port);
int  uart_ptransmit_empty(uint16_t port);
void uart_pwrite(uint16_t port, char c);

void uart_puts(const char *str);
void uart_putsn(char *str, size_t max_string_size);

void uart_pputs(uint16_t port, const char *str);
void uart_pputsn(uint16_t port, char *str, size_t max_string_size);

void uart_putint(uint64_t i);
void uart_pputint(uint16_t port, uint64_t i);

void uart_putint_hex(uint64_t i);
void uart_pputint_hex(uint16_t port, uint64_t i);
