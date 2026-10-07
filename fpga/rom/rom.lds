/*
 * Linker script for the Flashkeeper boot ROM.
 *
 * The Boot ROM is executed directly from the EBR ROM (0x06000000, 8 kB,
 * read-only). It sets up its own stack in SPRAM, but has no .data or .bss
 * section.
 */
MEMORY {
	ROM (rx) : ORIGIN = 0x06000000, LENGTH = 0x2000
}

SECTIONS {
    .text :
    {
        . = ALIGN(4);
        /* Reset vector first: this is what the core jumps to at PROGADDR_RESET. */
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
