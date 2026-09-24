/*
 * entry.s - firmware entry point and reserved image header.
 *
 * ABI for Flashkeeper boot ROM (see fpga/rom/rom_start.S):
 *
 *   The firmware image (byte 128 kB of the onboard flash, RAM 0x0)
 *   begins with a 256-byte header:
 *
 *     offset 0x0:  magic 0x57464B46 ("FKFW"), written by mkflashimg.py
 *     offset 0x4:  total image size in bytes, written by mkflashimg.py
 *     offset 0x8:  reserved (future verification metadata)
 *
 *   The image entry point is at image offset 0x100 (RAM 0x100).
 *
 *   The boot ROM is stored in the FPGA bitstream (possibly NVCM OTP) and
 *   is not typically updated alongside firmware, so 0x100 is a long-term
 *   constant baked into the ROM, and the ROM itself must not depend on any
 *   other firmware build detail: it learns the image size from the
 *   header and leaves the rest of firmware image startup to this file.
 */

/* The reserved header: real loadable section so the flash image carries
   the 256-byte header at its start. mkflashimg.py writes the magic and
   size into it after linking. */
    .section .fw_header, "a"
    .skip 256

    /* Must be the first item of .text (sections.lds keeps it there), so
       that it sits at image offset 0x100. */
    .section .text.entry, "ax"
    .balign 4
    .globl fw_entry
fw_entry:
    /* Zero the .bss/heap/stack region [_edata, _eram): the boot ROM
       does not know the firmware size, so the firmware finishes its
       own startup (same scheme as the init_ram the old XIP stub
       called). */
    la   a0, _edata
    la   a1, _eram
1:
    sw   zero, 0(a0)
    addi a0, a0, 4
    blt  a0, a1, 1b
    call main
2:
    j 2b
