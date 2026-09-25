#include "picosoc.h"
#include "serprog.h"
#include <stdint.h>
#include "memops.h"
#include "tweetnacl.h"
#include "rng.h"

#define _EDE_IMPLEMENTATION_
#include "ede.h"

#include "flashkeeper_config.h"

#define SERIAL_PASSWORD_SALT_BYTES 16

extern void randombytes(uint8_t *, uint64_t);

void hspi_read_flash_id(){
	uint8_t buffer[4] = { 0x9F, };
	hspi_flashio(buffer, 4, 0);

	for (int i = 1; i <= 3; i++) {
		putchar(' ');
		print_hex(buffer[i], 2);
	}
	putchar('\n');
}

void hspi_print_spi_state(){
	print("SPI State:\n");

	print("  LATENCY ");
	print_dec((reg_hspictrl >> 16) & 15);
	print("\n");

	print("  DDR ");
	if ((reg_hspictrl & (1 << 22)) != 0)
		print("ON\n");
	else
		print("OFF\n");

	print("  QSPI ");
	if ((reg_hspictrl & (1 << 21)) != 0)
		print("ON\n");
	else
		print("OFF\n");

	print("  CRM ");
	if ((reg_hspictrl & (1 << 20)) != 0)
		print("ON\n");
	else
		print("OFF\n");
}

// (value -- )
void config_spi(edei32 *s, edeu16 *sp, volatile uint32_t* reg){
    if(*sp < 1){
        print("Must provide parameters\n");
        return;
    }
    *reg = (uint32_t) s[*sp - 1];
    *sp = *sp - 1;
}

void config_hspi(edei32 *s, edeu16 *sp){
    config_spi(s, sp, &reg_hspictrl);
}

void config_cspi(edei32 *s, edeu16 *sp){
    config_spi(s, sp, &reg_cspictrl);
}

// (addr len -- )
void dump_mem(edei32 *s, edeu16 *sp, volatile uint8_t* base, uint32_t window_size){
    if(*sp < 2){
        print("Must provide parameters\n");
        return;
    }
    uint32_t addr = (uint32_t) s[*sp - 2];
    uint32_t len  = (uint32_t) s[*sp - 1];
    *sp = *sp - 2;
    // Reading past the end of the memory-mapped window hits unmapped space
    // and hangs the core, so bound-check before touching `base`.
    if(addr >= window_size || len > window_size - addr){
        print("Out of window\n");
        return;
    }
    for (uint32_t i = addr; i < addr + len; i++) {
        putchar(' ');
        print_hex(base[i], 2);
    }
    putchar('\n');
}

void dump_hspi(edei32 *s, edeu16 *sp){
    // The window only works in MEMIO mode; manual mode is sticky after
    // any flashio worker call, so enable it explicitly here.
    spi_memio_enable(HSPI_CTL_ADDR);
    dump_mem(s, sp, (volatile uint8_t*) HSPI_BASE_ADDR, FLASH_WINDOW_SIZE);
}

void dump_cspi(edei32 *s, edeu16 *sp){
    spi_memio_enable(CSPI_CTL_ADDR);
    dump_mem(s, sp, (volatile uint8_t*) CSPI_BASE_ADDR, FLASH_WINDOW_SIZE);
}

void dump_brom(edei32 *s, edeu16 *sp){
    dump_mem(s, sp, (volatile uint8_t*) BROM_BASE_ADDR, BROM_WINDOW_SIZE);
}

uint8_t spi_sr(uint8_t reg, uint32_t reg_addr){
    uint8_t buffer[2] = {0};
    
    switch(reg){
        case 1: buffer[0] = 0x05; break; // Read Status Register-1
        case 2: buffer[0] = 0x35; break; // Read Status Register-2
        case 3: buffer[0] = 0x15; break; // Read Status Register-3
    }

    flashio_call(buffer, 2, 0, reg_addr, 0);

    return buffer[1];
}

void spi_print_sr(uint32_t reg_addr){
    print("SR1: ");
    print_hex(spi_sr(1, reg_addr), 2);
    print("\nSR2: ");
    print_hex(spi_sr(2, reg_addr), 2);
    print("\nSR3: ");
    print_hex(spi_sr(3, reg_addr), 2);
    print("\n");
}

void hspi_print_sr(){
    spi_print_sr(HSPI_CTL_ADDR);
}

void cspi_print_sr(){
    spi_print_sr(CSPI_CTL_ADDR);
}

void spi_wait_busy(uint32_t reg_addr){
    while(spi_sr(1, reg_addr) & 1); // loop until BUSY goes zero
}

void hspi_chip_erase(){
    uint8_t buffer[1] = {SPI_CMD_CHIP_ERASE};

    hspi_flashio(buffer, 1, SPI_CMD_WE);
    spi_wait_busy(HSPI_CTL_ADDR);
}

// ( d0 ... dn addr len -- )
void write_spi(edei32 *s, edeu16 *sp, uint32_t reg_addr){
    uint32_t flash_addr; // address to write at
    uint16_t len; // length of the write in bytes (1-256)
    uint8_t cells;

    if(*sp < 2){
        print("Must provide parameters\n");
        return;
    }

    flash_addr = s[*sp - 2] & 0x00FFFFFF;
    len = (uint16_t) s[*sp - 1];
    cells = (uint8_t)((len + 3) / 4);

    *sp = *sp - 2;

    if(len < 1){
        print("Length must be positive\n");
        return;
    }
    if((0x100 - (flash_addr & 0xFF)) < len){
        print("Write must fit within page\n");
        return;
    }
    if(cells > *sp){
        print("Insufficient data present on stack.\n");
        return;
    }
    uint32_t cmd = ((uint32_t) SPI_CMD_PP << 24) | flash_addr; // assemble a 32-bit write command (02h + 24-bit address)

    // Call flashio_call directly to use the prebuffer
    flashio_call((uint8_t*) (s + (*sp - cells)), len, SPI_CMD_WE, reg_addr, cmd);

    spi_wait_busy(reg_addr);

    *sp = *sp - cells;
}

void write_hspi(edei32 *s, edeu16 *sp){
    write_spi(s, sp, HSPI_CTL_ADDR);
}

void write_cspi(edei32 *s, edeu16 *sp){
    write_spi(s, sp, CSPI_CTL_ADDR);
}

void cmd_rng(edei32 *s, edeu16 *sp){
    if(*sp < 1){
        print("Must provide parameters\n");
        return;
    }
    int count = s[*sp - 1];
    if (count < 0 || count > 256) count = 256;

    static uint8_t buf[256];
    randombytes(buf, (size_t) count);
    for (int i = 0; i < count; i++) {
        putchar(' ');
        print_hex(buf[i], 2);
    }
    putchar('\n');

    rng_stats_t st;
    rng_get_stats(&st);

    print("rng: bits ");
    print_dec((int32_t) st.bits_seen);
    print(" ok ");
    print_dec((int32_t) st.batches_ok);
    print(" fail ");
    print_dec((int32_t) st.batches_fail);
    print(" rct_max ");
    print_dec((int32_t) st.rct_max);
    print(" apt_max ");
    print_dec((int32_t) st.apt_max);
    putchar('\n');

    *sp = *sp - 1;
}

// UART RX framing-error counter (uart status register bits [15:8]).
// Nonzero after a serprog session means the RX dropped bit-slipped frames;
// serprog NAKs any SPIOP whose window saw an increase.
void cmd_ferr(){
    print("uart ferr: ");
    print_dec((reg_uart_sts >> 8) & 0xFF);
    putchar('\n');
}

// Generate a password configuration for flashkeeper_config.h
void generate_password(){
    uint8_t hash_buffer[64];
    uint8_t password_buffer[65 + SERIAL_PASSWORD_SALT_BYTES]; // +1 for the NUL that ede_getstring_placeholder writes at offset maxlen
    uint16_t entered_len;

    // Initialize the password buffer with the salt
    randombytes(password_buffer, SERIAL_PASSWORD_SALT_BYTES);

    print("New Password: ");
    entered_len = ede_getstring_placeholder(getchar, putchar_raw, password_buffer + SERIAL_PASSWORD_SALT_BYTES, 64, '*');
    crypto_hash_sha512(hash_buffer, password_buffer, entered_len + SERIAL_PASSWORD_SALT_BYTES);

    print("static const uint32_t serial_password_salt[] = {");
    for(int i=0; i < SERIAL_PASSWORD_SALT_BYTES; i+=4){
        uint32_t w = (uint32_t) password_buffer[i] | ((uint32_t) password_buffer[i + 1] << 8) | ((uint32_t) password_buffer[i + 2] << 16) | ((uint32_t) password_buffer[i + 3] << 24);
        print("0x");
        ede_puthex(putchar, w, 32);
        if(i < SERIAL_PASSWORD_SALT_BYTES - 4) print(",");
    }
    print("};\n");

    print("static const uint32_t serial_password_hash[] = {");
    for(int i=0; i < 64; i += 4){
        uint32_t w = (uint32_t) hash_buffer[i] | ((uint32_t) hash_buffer[i + 1] << 8) | ((uint32_t) hash_buffer[i + 2] << 16) | ((uint32_t) hash_buffer[i + 3] << 24);
        print("0x");
        ede_puthex(putchar, w, 32);
        if(i < 60) print(",");
    }
    print("};\n");
}

void password_prompt(){
    uint8_t hash_buffer[64];
    uint8_t password_buffer[65 + SERIAL_PASSWORD_SALT_BYTES]; // +1 for the NUL that ede_getstring_placeholder writes at offset maxlen
    uint16_t entered_len;

    while(1){
        // Initialize the password buffer with the salt
        memcpy(password_buffer, serial_password_salt, SERIAL_PASSWORD_SALT_BYTES);

        print("Serial Password: ");

        // Have the user enter the password into the buffer, after the salt
        entered_len = ede_getstring_placeholder(getchar, putchar_raw, password_buffer + SERIAL_PASSWORD_SALT_BYTES, 64, '*');

        // Hash the buffer, including the salt, up to the end of the password the user typed
        crypto_hash_sha512(hash_buffer, password_buffer, entered_len + SERIAL_PASSWORD_SALT_BYTES);
        if(crypto_verify_64(hash_buffer, (const uint8_t *) serial_password_hash) == 0) break;
    }
}

// (addr len -- )
void hash_mem(edei32 *s, edeu16 *sp, volatile uint8_t* base, uint32_t window_size){
    if(*sp < 2){
        print("Must provide parameters\n");
        return;
    }
    uint32_t addr = (uint32_t) s[*sp - 2];
    uint32_t len  = (uint32_t) s[*sp - 1];
    *sp = *sp - 2;
    // Reading past the end of the memory-mapped window hits unmapped space
    // and hangs the core, so bound-check before touching `base`.
    if(addr >= window_size || len > window_size - addr){
        print("Out of window\n");
        return;
    }
    uint8_t hash_buffer[64];
    volatile uint8_t* start = base + addr;

    // crypto_hash_sha512 takes a non-volatile pointer. The cast is safe: the
    // hash reads each byte of the region exactly once, in order, and nothing
    // writes to the region meanwhile.
    crypto_hash_sha512(hash_buffer, (const uint8_t *) start, (uint64_t) len);

    for(int i=0; i<64; i++){
        print_hex(hash_buffer[i], 2);
    }

    print(" ");
}

void hash_hspi(edei32 *s, edeu16 *sp){
    // The window only works in MEMIO mode; manual mode is sticky after
    // any flashio worker call, so enable it explicitly here.
    spi_memio_enable(HSPI_CTL_ADDR);
    hash_mem(s, sp, (volatile uint8_t*) HSPI_BASE_ADDR, FLASH_WINDOW_SIZE);
}

void hash_cspi(edei32 *s, edeu16 *sp){
    // The window only works in MEMIO mode; manual mode is sticky after
    // any flashio worker call, so enable it explicitly here.
    spi_memio_enable(CSPI_CTL_ADDR);
    hash_mem(s, sp, (volatile uint8_t*) CSPI_BASE_ADDR, FLASH_WINDOW_SIZE);
}

void picosoc_benchmark_wrapper(){
    cmd_benchmark(1, 0);
}

void help(edei32 *s, edeu16 *sp){
    (void) s; // help takes no stack parameters
    (void) sp;
	print("help - print this help\n"
          "cfstat - print Configuration Flash controller status\n"
          "cfid - print Configuration Flash IDs\n"
          "cfcfg - set Configuration Flash controller config register (value -- )\n"
          "cfdump - dump Configuration Flash data (addr len -- )\n"
          "cfwrite - write to Configuration Flash ( d0 ... dn addr len -- )\n"
          "cfsr - print Configuration Flash Status Registers \n"
          "cfhash - SHA-512 hash Configuration Flash region (addr len -- )\n"
          "hfstat - print Host Flash controller status\n"
          "hfid - print Host Flash IDs\n"
          "hfcfg - set Host Flash controller config register (value -- )\n"
          "hfdump - dump Host Flash region (addr len -- )\n"
          "hfwrite - write to Host Flash ( d0 ... dn addr len -- )\n"
          "hfsr - print Host Flash Status Registers\n"
          "hfchiperase - erase Host Flash (danger!)\n"
          "hfhash - SHA-512 hash Host Flash region (addr len -- )\n"
          "romdump - dump Boot ROM (addr len -- )\n"
          "rng - print up to 256 whitened random bytes plus TRNG stats (n -- )\n"
          "ferr - dump UART Frame Error detection counter\n"
          "genpw - generate new password and salt configuration for flashkeeper_config.h\n"
          "benchmark - Run the PicoSoC CPU benchmark\n"
          "serprog - enter serprog mode\n"
          "\nThis shell has a Forth-like parameter stack - type .s to show its contents.\n"
          "Stack manipulation words (drop, swap, dup, over, rot), arithmetic words\n"
          "(+, -, *, /, /mod, mod), and basic output (., emit) are supported.\n");
}

void start_shell(){
    reg_uart_clkdiv = 10; // 1 Mbaud

	print("\n\nWelcome to Flashkeeper\n");

    ede_registerfn("help", help);

    // Configuration Flash controller commands
    ede_registerfn("cfstat", cmd_print_spi_state);
    ede_registerfn("cfid", cmd_read_flash_id);
    ede_registerfn("cfcfg", config_cspi);
    ede_registerfn("cfdump", dump_cspi);
    ede_registerfn("cfwrite", write_cspi);
    ede_registerfn("cfsr", cspi_print_sr);
    // cfchiperase is deliberately ommitted, as it would very directly entail "please brick immediately"
    ede_registerfn("cfhash", hash_cspi);

    // Host Flash controller commands
    ede_registerfn("hfstat", hspi_print_spi_state);
    ede_registerfn("hfid", hspi_read_flash_id);
    ede_registerfn("hfcfg", config_hspi);
    ede_registerfn("hfdump", dump_hspi);
    ede_registerfn("hfwrite", write_hspi);
    ede_registerfn("hfsr", hspi_print_sr);
    ede_registerfn("hfchiperase", hspi_chip_erase);
    ede_registerfn("hfhash", hash_hspi);

    // Boot ROM commands
    ede_registerfn("romdump", dump_brom);

    // System test commands
    ede_registerfn("rng", cmd_rng);
    ede_registerfn("ferr", cmd_ferr);
    ede_registerfn("benchmark", picosoc_benchmark_wrapper);

    // Configuration helpers
    ede_registerfn("genpw", generate_password);

    // Non-EDE modes
    ede_registerfn("serprog", serprog_command_loop);

    while(1){
        if(SERIAL_REQUIRE_PASSWORD){
            password_prompt();
        }else{
            print("WARNING! Serial password is disabled. Anyone can reconfigure this Flashkeeper.\n");
        }
        print("\n");

        print("Starting debug shell (Forth-like syntax - type help for help)...\n");

        ede_shell(getchar, putchar_raw);
    }
}

void main(){
    // for now, just start the shell at boot
    start_shell();
}