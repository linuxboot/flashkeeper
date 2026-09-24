/*
 * Linker script for the Flashkeeper boot ROM.
 *
 * The ROM lives in the EBR window (0x06000000, 8 kB, read-only) and
 * executes there from reset. It allocates no SPRAM of its own: the stack
 * is the top of the SPRAM (0x20000), set up by the reset vector before
 * boot_main() runs. Hence there is no .data and no .bss section - if
 * either ever appears, the build must fail rather than silently
 * reserving RAM that the firmware image also uses.
 */
MEMORY {
	ROM (rx) : ORIGIN = 0x06000000, LENGTH = 0x2000
}

SECTIONS {
    .text :
    {
        . = ALIGN(4);
        /* Reset vector first: this is what the FPGA jumps to at
           PROGADDR_RESET. */
        KEEP(*(.text.entry))
        *(.text)
        *(.text*)
        *(.rodata)
        *(.rodata*)
        *(.srodata)
        *(.srodata*)
        . = ALIGN(4);
        _erom = .;
    } >ROM
}
