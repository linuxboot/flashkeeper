/* 
 * memmap.h - Flashkeeper SPRAM/CSPI flash memory map
 *
 * The consistency of the values in this file is checked when building boot.c
 * This file does not cover the EBR ROMs or memory-mapped I/O devices.
 *
 * This file must only contain #define lines and comments - it is used in C code,
 * linker scripts (using the C preprocessor), Makefiles (extracting values with sed),
 * and Python scripts (which parse #defines directly).
 */
#ifndef FK_MEMMAP_H
#define FK_MEMMAP_H

/* These values define the SPRAM memory map */
#define FK_SPRAM_SIZE        0x20000 /* SPRAM available to the RISC-V core - can be reduced if hardware peripherals need SPRAM */
#define FK_MIN_STACK_SPACE   0x2000  /* Size reserved for the stack above firmware and fwsiglib */
#define FK_FLIB_SLOT_SIZE    0x6800  /* Size of the SPRAM slot reserved for fwsiglib */

/* Derived SPRAM addresses */
#define FK_STACK_TOP         0x20000 /* = FK_SPRAM_SIZE */
#define FK_STACK_REGION_BASE 0x1E000 /* = FK_STACK_TOP - FK_MIN_STACK_SPACE */
#define FK_LIB_BASE          0x17800 /* = FK_STACK_REGION_BASE - FK_FLIB_SLOT_SIZE */
#define FK_IMG_MAX           0x17800 /* = FK_LIB_BASE, the firmware image (header+code+data+bss) must end before the fwsiglib slot */

/* These values define the CSPI flash memory map */
#define FK_FLASH_SIZE        0x80000 /* 512 kB AT25DF041B */
#define FK_FLASH_IMAGE_OFF   0x20000 /* signed firmware image */
#define FK_FLASH_LIB_OFF     0x40000 /* fwsiglib binary */

/* Derived CSPI flash addresses */
#define FK_FLASH_IMAGE_LEN   0x60000 /* = FK_FLASH_SIZE - FK_FLASH_IMAGE_OFF */

#endif
