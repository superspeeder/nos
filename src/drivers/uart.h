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

void uart_putint_hexpad(uint64_t i, uint64_t pad);
void uart_pputint_hexpad(uint16_t port, uint64_t i, uint64_t pad);

void uart_putsvln(const char* pre, uint64_t i);
void uart_putsvlnf(const char* fn, const char* pre, uint64_t i);
void uart_putsvln_hex(const char* pre, uint64_t i);
void uart_putsvlnf_hex(const char* fn, const char* pre, uint64_t i);

void uart_putsvlnfs(const char* sfile, const char* fn, const char* pre, uint64_t i);
void uart_putsvlnfs_hex(const char* sfile, const char* fn, const char* pre, uint64_t i);

void uart_log(const char* fn, const char* s);
void uart_logs(const char* sfile, const char* fn, const char* s);

#define S_(n) #n
#define S(n) S_(n)

#define LOGVAL(name) uart_putsvlnfs(__FILE_NAME__ , __PRETTY_FUNCTION__, "():" S(__LINE__) "::" #name , (uint64_t)((name)))
#define LOGVAL_HEX(name) uart_putsvlnfs_hex(__FILE_NAME__, __PRETTY_FUNCTION__, "():" S(__LINE__) "::" #name , (uint64_t)((name)))
#define LOG(s) uart_logs(__FILE_NAME__, __PRETTY_FUNCTION__, "():" S(__LINE__) ": " s)