/*
 * rom_start.S - reset vector and tail of the Flashkeeper boot ROM.
 *
 * Executes from the EBR ROM window (0x06000000) immediately after FPGA
 * configuration: the CPU's reset vector (PROGADDR_RESET in top.v) points
 * here. The C half (boot.c: boot_main) reads the firmware image header
 * (magic + size, see boot.c) from the SPI flash and copies the image
 * into the SPRAM; this file does everything else:
 *
 *   reset:  set up the stack, defensive register init, call boot_main
 *   tail:   on success, jump to the firmware; on failure, halt
 *
 * The ROM knows nothing about the firmware except the image header
 * convention and the entry point below. The firmware finishes its own
 * startup (zeroing .bss/heap/stack, see entry.s) because only it knows
 * its own size.
 *
 * See entry.s in the firmware.
 */
#define FW_ENTRY 0x100

    .section .text.entry, "ax"
    .globl rom_reset
rom_reset:
    /* Top of the 128 kB SPRAM. PicoRV32 also sets sp here at reset
       (picosoc STACKADDR); set it explicitly so the ROM does not depend
       on the reset default. */
    li   sp, 0x20000

    /* Defensively zero the register file (PicoRV32 resets it; the old
       XIP boot stub did this too). x1 is hardwired zero, x2 is sp. */
    addi x3,  zero, 0
    addi x4,  zero, 0
    addi x5,  zero, 0
    addi x6,  zero, 0
    addi x7,  zero, 0
    addi x8,  zero, 0
    addi x9,  zero, 0
    addi x10, zero, 0
    addi x11, zero, 0
    addi x12, zero, 0
    addi x13, zero, 0
    addi x14, zero, 0
    addi x15, zero, 0
    addi x16, zero, 0
    addi x17, zero, 0
    addi x18, zero, 0
    addi x19, zero, 0
    addi x20, zero, 0
    addi x21, zero, 0
    addi x22, zero, 0
    addi x23, zero, 0
    addi x24, zero, 0
    addi x25, zero, 0
    addi x26, zero, 0
    addi x27, zero, 0
    addi x28, zero, 0
    addi x29, zero, 0
    addi x30, zero, 0
    addi x31, zero, 0

    /* Load the firmware image (flash -> SPRAM). boot_main returns 0 on
       success; perform image verification if applicable */
    call boot_main

    /* On failure (bad magic/size, e.g. blank flash) halt instead of
       jumping into untrusted RAM. */
    bnez a0, rom_loop

    /* Hand over to the firmware at its stable entry point. */
    li   a0, FW_ENTRY
    jalr zero, 0(a0)

rom_loop:
    j    rom_loop
