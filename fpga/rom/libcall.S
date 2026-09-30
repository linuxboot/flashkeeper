/*
 * libcall.S - trampoline from the boot ROM (EBR) to the fwsiglib
 *
 * fk_lib_verify() is the library's single entry point (see
 * rom/lib/fk_verify.c). The ROM lives in EBR at 0x06000000, while
 * fwsiglib is at a fixed address in SPRAM.
 *
 * This file allows fk_lib_verify to be called from ROM C code.
 *
 * The trampoline uses t0, and 4 bytes of stack to save the return address.
 *
 * FLIB_TARGET is the absolute address of the library's entry point
 * (FLIB_ADDR + offset of fk_lib_verify), injected as -DFK_FLIB_TARGET
 * by the build (see the Makefile / rom/tools/make_lib_inc.py).
 */
.equ FLIB_TARGET, FK_FLIB_TARGET

    .section .text
    .globl fk_lib_verify
    .type fk_lib_verify, @function
fk_lib_verify:
    addi  sp, sp, -4
    sw    ra, 0(sp)
    lui   t0, %hi(FLIB_TARGET)
    addi  t0, t0, %lo(FLIB_TARGET)
    jalr  ra, 0(t0)          /* into the library; ra -> restore below */
    lw    ra, 0(sp)
    addi  sp, sp, 4
    ret
