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
 *     offset 0x8:  type (0 = unsigned, 1 = signed ed25519); set by
 *                  tools/sign.c for signed images, 0 otherwise
 *     offset 0xA0: signer's ed25519 public key (signed images)
 *     offset 0xC0: ed25519 signature (signed images)
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
    /* Initialize sp to _estack (top of the SPRAM, see sections.lds) */
    la   sp, _estack

    /* Zero the firmware's writable regions, skipping the fwsiglib slot:
       the boot ROM does not know the firmware size, so the firmware
       finishes its own startup (same scheme as the init_ram the old XIP
       stub called). The fwsiglib library at [LIB_BASE, ...) is loaded
       by the boot ROM and may be called by the firmware, so it must not
       be zeroed. Zero [_edata, LIB_BASE) (bss + heap + gap) and
       [STACK_REGION_BASE, _eram) (stack region).
       */
    la   a0, _edata
    la   a1, LIB_BASE
1:
    sw   zero, 0(a0)
    addi a0, a0, 4
    blt  a0, a1, 1b
    la   a0, STACK_REGION_BASE
    la   a1, _eram
2:
    sw   zero, 0(a0)
    addi a0, a0, 4
    blt  a0, a1, 2b
    call main
3:
    j 3b
