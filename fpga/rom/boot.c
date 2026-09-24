/*
 * boot.c - second stage of the Flashkeeper boot ROM: load the firmware
 * image from the SPI flash into the SPRAM.
 *
 * The firmware image is self-describing. Its first 8 bytes (the start of
 * the 256-byte reserved header at image offset 0) carry:
 *
 *     offset 0x0:  magic, 0x57464B46 ("FKFW" little-endian)
 *     offset 0x4:  size, total image size in bytes (header included),
 *                  a multiple of 4
 *
 * The ROM copies exactly `size` bytes from the flash image window into
 * RAM and trusts nothing else about the image. It therefore has no
 * build-time dependency on the firmware: the same ROM boots any firmware
 * image that follows this header convention (written by
 * firmware/tools/mkflashimg.py). That matters for the end goal, where
 * the ROM is OTP-programmed into the FPGA and can no longer be updated
 * together with the firmware.
 *
 * The rest of the reserved header is the designated home for future
 * verification metadata (e.g. a hash or signature the ROM checks between
 * the copy and the jump in rom_start.S).
 */
#include <stdint.h>

#define FW_IMAGE_FLASH ((const volatile uint32_t *) 0x01020000)
#define FW_IMAGE_RAM   ((volatile uint32_t *) 0x00000000)
#define reg_uart_clkdiv (*(volatile uint32_t*)0x02000004)
#define reg_uart_data (*(volatile uint32_t*)0x02000008)

#define IMG_MAGIC    0x57464B46u /* "FKFW" little-endian */
#define IMG_MIN_SIZE 0x104u      /* header + at least the 4-byte entry */
#define IMG_MAX_SIZE 0x20000u    /* cannot exceed the SPRAM */

void print_hex(uint32_t v, int digits)
{
	for (int i = 7; i >= 0; i--) {
		char c = "0123456789abcdef"[(v >> (4*i)) & 15];
		if (c == '0' && i >= digits) continue;
		reg_uart_data = c;
		digits = i;
	}
}

void print(const char *p){
	while (*p)
		reg_uart_data = *(p++);
}

int boot_main(void){
	reg_uart_clkdiv = 10; // 1 Mbaud
	print("\r\nBootROM ");

	uint32_t magic = FW_IMAGE_FLASH[0];
	uint32_t size  = FW_IMAGE_FLASH[1];

	/* The flash is untrusted: refuse anything that is not a sane image
	   (a blank chip reads as all 0xFF, which fails the magic check). */
	if (magic != IMG_MAGIC){
		print("BadMagic ");
		print_hex(magic, 8);
		return 1;
	}
	if (size < IMG_MIN_SIZE || size > IMG_MAX_SIZE || (size & 3u)){
		print("BadSize");
		return 1;
	}
	
	/* Word-for-word copy of the image (header included) into RAM. */
	for (uint32_t i = 0; i < size / 4; i++)
		FW_IMAGE_RAM[i] = FW_IMAGE_FLASH[i];

	print("OK");
	return 0;
}
