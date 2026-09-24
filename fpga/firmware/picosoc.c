/*
 *  PicoSoC - A simple example SoC using PicoRV32
 *
 *  Copyright (C) 2017  Claire Xenia Wolf <claire@yosyshq.com>
 *
 *  Permission to use, copy, modify, and/or distribute this software for any
 *  purpose with or without fee is hereby granted, provided that the above
 *  copyright notice and this permission notice appear in all copies.
 *
 *  THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 *  WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 *  MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 *  ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 *  WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 *  ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 *  OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 */

#include <stdint.h>
#include <stdbool.h>
#include "picosoc.h"

// --------------------------------------------------------

extern void flashio_worker(uint8_t *data, int len, uint8_t wrencmd, uint32_t reg, uint32_t prebuf);

void flashio_call(uint8_t *data, int len, uint8_t wrencmd, uint32_t reg, uint32_t prebuf){
    flashio_worker(data, len, wrencmd, reg, prebuf);
}

/*
 * Enter MEMIO (memory-mapped, FSM-driven) mode for a spimemio instance.
 *
 * The flashio worker asserts manual mode on entry and leaves it there
 * (sticky), so anything that reads the memory-mapped window (0x01xxxxxx
 * for CSPI, 0x04xxxxxx for HSPI) must call this first. The FSM is
 * electrically quiet in MEMIO mode except during actual window reads.
 */
void spi_memio_enable(uint32_t reg){
    *((volatile uint8_t *)(reg + 3)) = 0x80; /* config_en = 1 */
}

void cspi_flashio(uint8_t *data, int len, uint8_t wrencmd){
    flashio_call(data, len, wrencmd, CSPI_CTL_ADDR, 0);
}

void hspi_flashio(uint8_t *data, int len, uint8_t wrencmd){
    flashio_call(data, len, wrencmd, HSPI_CTL_ADDR, 0);
}

void set_flash_qspi_flag()
{
    uint8_t buffer[8];

    // Read Configuration Registers (RDCR1 35h)
    buffer[0] = 0x35;
    buffer[1] = 0x00; // rdata
    cspi_flashio(buffer, 2, 0);
    uint8_t sr2 = buffer[1];

    // Write Enable Volatile (50h) + Write Status Register 2 (31h)
    buffer[0] = 0x31;
    buffer[1] = sr2 | 2; // Enable QSPI
    cspi_flashio(buffer, 2, 0x50);
}

void set_hspi_qspi_flag()
{
    uint8_t buffer[8];

    // Read Configuration Registers (RDCR1 35h)
    buffer[0] = 0x35;
    buffer[1] = 0x00; // rdata
    hspi_flashio(buffer, 2, 0);
    uint8_t sr2 = buffer[1];

    // Write Enable Volatile (50h) + Write Status Register 2 (31h)
    buffer[0] = 0x31;
    buffer[1] = sr2 | 2; // Enable QSPI
    hspi_flashio(buffer, 2, 0x50);
}

void set_flash_mode_spi(){
    reg_cspictrl = (reg_cspictrl & 0xFF80FFFF) | 0x00000000;
}

void set_flash_mode_dual(){
    reg_cspictrl = (reg_cspictrl & 0xFF80FFFF) | 0x00400000;
}

void set_flash_mode_quad(){
    reg_cspictrl = (reg_cspictrl & 0xFF80FFFF) | 0x00240000;
}

void set_flash_mode_qddr(){
    reg_cspictrl = (reg_cspictrl & 0xFF80FFFF) | 0x00670000;
}

void enable_flash_crm(){
    reg_cspictrl |= 0x00100000;
}

void set_hspi_mode_spi(){
    reg_hspictrl = (reg_hspictrl & 0xFF80FFFF) | 0x00000000;
}

void set_hspi_mode_dual(){
    reg_hspictrl = (reg_hspictrl & 0xFF80FFFF) | 0x00400000;
}

void set_hspi_mode_quad(){
    reg_hspictrl = (reg_hspictrl & 0xFF80FFFF) | 0x00240000;
}

void set_hspi_mode_qddr(){
    reg_hspictrl = (reg_hspictrl & 0xFF80FFFF) | 0x00670000;
}

void enable_hspi_crm(){
    reg_hspictrl |= 0x00100000;
}

// --------------------------------------------------------

void putchar_raw(char c)
{
    reg_uart_data = c;
}

void putchar(char c)
{
    if (c == '\n')
        putchar_raw('\r');
    putchar_raw(c);
}

void print(const char *p)
{
    while (*p)
        putchar(*(p++));
}

void print_hex(uint32_t v, int digits)
{
    for (int i = 7; i >= 0; i--) {
        char c = "0123456789abcdef"[(v >> (4*i)) & 15];
        if (c == '0' && i >= digits) continue;
        putchar(c);
        digits = i;
    }
}

void print_dec(int32_t v){
    if (v < 0) {
        putchar('-');
        v = -v;
    }

    if (v == 0){
        putchar('0');
        return;
    }

    if (v/10){
       print_dec(v/10);
    }

    putchar((char)(v % 10 + '0'));
}

char getchar()
{
    uint32_t c = 0xFFFFFFFFu; // reg_uart_data reads 0xFFFFFFFF while the RX FIFO is empty
    while (c == 0xFFFFFFFFu) {
        c = reg_uart_data;
    }

    return (char) c;
}

int getchar_tmo(uint32_t timeout_cycles)
{
    uint32_t start, now;

    __asm__ volatile ("rdcycle %0" : "=r"(start));
    for (;;) {
        uint32_t c = reg_uart_data;
        if (c != 0xFFFFFFFFu)
            return (int) c;
        __asm__ volatile ("rdcycle %0" : "=r"(now));
        if (now - start > timeout_cycles)
            return -1;
    }
}

void cmd_print_spi_state()
{
    print("SPI State:\n");

    print("  LATENCY ");
    print_dec((reg_cspictrl >> 16) & 15);
    print("\n");

    print("  DDR ");
    if ((reg_cspictrl & (1 << 22)) != 0)
        print("ON\n");
    else
        print("OFF\n");

    print("  QSPI ");
    if ((reg_cspictrl & (1 << 21)) != 0)
        print("ON\n");
    else
        print("OFF\n");

    print("  CRM ");
    if ((reg_cspictrl & (1 << 20)) != 0)
        print("ON\n");
    else
        print("OFF\n");
}

// --------------------------------------------------------

void cmd_read_flash_id()
{
    uint8_t buffer[17] = { 0x9F, /* zeros */ };
    cspi_flashio(buffer, 17, 0);

    for (int i = 1; i <= 16; i++) {
        putchar(' ');
        print_hex(buffer[i], 2);
    }
    putchar('\n');
}

// --------------------------------------------------------

uint8_t cmd_read_flash_reg(uint8_t cmd)
{
    uint8_t buffer[2] = {cmd, 0};
    cspi_flashio(buffer, 2, 0);
    return buffer[1];
}

void print_reg_bit(int val, const char *name)
{
    for (int i = 0; i < 12; i++) {
        if (*name == 0)
            putchar(' ');
        else
            putchar(*(name++));
    }

    putchar(val ? '1' : '0');
    putchar('\n');
}

uint32_t cmd_benchmark(uint8_t verbose, uint32_t *instns_p)
{
    uint8_t data[256];
    uint32_t *words = (void*)data;

    uint32_t x32 = 314159265;

    uint32_t cycles_begin, cycles_end;
    uint32_t instns_begin, instns_end;
    __asm__ volatile ("rdcycle %0" : "=r"(cycles_begin));
    __asm__ volatile ("rdinstret %0" : "=r"(instns_begin));

    for (int i = 0; i < 20; i++)
    {
        for (int k = 0; k < 256; k++)
        {
            x32 ^= x32 << 13;
            x32 ^= x32 >> 17;
            x32 ^= x32 << 5;
            data[k] = (uint8_t) x32;
        }

        for (int k = 0, p = 0; k < 256; k++)
        {
            if (data[k])
                data[p++] = (uint8_t) k;
        }

        for (int k = 0; k < 64; k++)
        {
            x32 = x32 ^ words[k];
        }
    }

    __asm__ volatile ("rdcycle %0" : "=r"(cycles_end));
    __asm__ volatile ("rdinstret %0" : "=r"(instns_end));

    if (verbose)
    {
        print("Cycles: 0x");
        print_hex(cycles_end - cycles_begin, 8);
        putchar('\n');

        print("Instns: 0x");
        print_hex(instns_end - instns_begin, 8);
        putchar('\n');

        print("Chksum: 0x");
        print_hex(x32, 8);
        putchar('\n');
    }

    if (instns_p)
        *instns_p = instns_end - instns_begin;

    return cycles_end - cycles_begin;
}