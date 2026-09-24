#include <stdint.h>

#ifndef _PICOSOC_H_

#define MEM_TOTAL 0x20000

#define CSPI_BASE_ADDR      0x01000000
#define CSPI_CTL_ADDR       0x02000000
#define HSPI_BASE_ADDR      0x04000000
#define HSPI_CTL_ADDR       0x05000000
#define BROM_BASE_ADDR      0x06000000
#define FLASH_WINDOW_SIZE   0x01000000  // CSPI/HSPI MEMIO windows are each 16 MB
#define BROM_WINDOW_SIZE    0x00002000  // EBR ROM window: 2048 x 32-bit words = 8 kB

#define SPI_CMD_CHIP_ERASE  0xC7 // Chip Erase
#define SPI_CMD_WE          0x06 // Write Enable
#define SPI_CMD_WEVSR       0x50 // Write Enable Volatile Status Register
#define SPI_CMD_PP          0x02 // Page Program

#define reg_cspictrl (*(volatile uint32_t*)CSPI_CTL_ADDR)
#define reg_hspictrl (*(volatile uint32_t*)HSPI_CTL_ADDR)
#define reg_uart_clkdiv (*(volatile uint32_t*)0x02000004)
#define reg_uart_data (*(volatile uint32_t*)0x02000008)
#define reg_uart_sts  (*(volatile uint32_t*)0x0200000C) /* bit0=notempty, bit1=ovr, [15:8]=ferr */
#define reg_leds (*(volatile uint32_t*)0x08000000)

// --------------------------------------------------------

void set_flash_qspi_flag();
void set_hspi_qspi_flag();
void set_flash_mode_spi();
void set_flash_mode_dual();
void set_flash_mode_quad();
void set_flash_mode_qddr();
void enable_flash_crm();
void set_hspi_mode_spi();
void set_hspi_mode_dual();
void set_hspi_mode_quad();
void set_hspi_mode_qddr();
void enable_hspi_crm();
void putchar(char c);
void putchar_raw(char c);
void print(const char *p);
void print_hex(uint32_t v, int digits);
void print_dec(int32_t v);
char getchar();
int getchar_tmo(uint32_t timeout_cycles); /* Like getchar(), but returns -1 if no byte arrives within timeout_cycles. */
void cmd_print_spi_state();
void cmd_read_flash_id();
void flashio_call(uint8_t *data, int len, uint8_t wrencmd, uint32_t reg, uint32_t prebuf);
void hspi_flashio(uint8_t *data, int len, uint8_t wrencmd);
void cspi_flashio(uint8_t *data, int len, uint8_t wrencmd);
void spi_memio_enable(uint32_t reg);
uint32_t cmd_benchmark(uint8_t verbose, uint32_t *instns_p);

#endif