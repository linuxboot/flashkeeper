MEMORY
{
    FLASH (rx) : ORIGIN = 0x00020000, LENGTH = 0x60000
    RAM (xrw) : ORIGIN = 0x00000000, LENGTH = 0x20000
}
SECTIONS {
    /* 256-byte image header (RAM 0x0, flash byte 128 kB): magic +
       total size, written by tools/mkflashimg.py, read by the boot ROM
       (see rom/boot.c); the rest reserved for future verification
       metadata. Part of the firmware image ABI for the boot ROM:
       the entry point is at image offset 0x100. See entry.s. */
    .fw_header :
    {
        . = ORIGIN(RAM);
        KEEP(*(.fw_header))
        . = ALIGN(4);
    } >RAM AT>FLASH

    /* Runtime code and constants execute from RAM: fetching an
       instruction word through the spimemio XIP window costs a full SPI
       read transaction (~196 cycles) while a RAM fetch is ~5 cycles.
       The entry point (entry.s) is kept first, at image offset 0x100.
       The whole image (this section and .data) is loaded by the boot
       ROM with one flat copy from flash 0x20000 to RAM 0x0; the
       invariant LMA = VMA + 0x20000 for every byte is checked by
       tools/mkflashimg.py at image build time. */
    .text :
    {
        . = ALIGN(4);
        _stext = .;
        KEEP(*(.text.entry))
        *(.text)
        *(.text*)
        *(.rodata)
        *(.rodata*)
        *(.srodata)
        *(.srodata*)
        . = ALIGN(4);
        _etext = .;
    } >RAM AT>FLASH

    .data :
    {
        . = ALIGN(4);
        _sdata = .;
        _ram_start = .;
        . = ALIGN(4);
        *(.data)
        *(.data*)
        *(.sdata)
        *(.sdata*)
        . = ALIGN(4);
        _edata = .;
    } >RAM AT>FLASH

    .bss :
    {
        . = ALIGN(4);
        _sbss = .;
        *(.bss)
        *(.bss*)
        *(.sbss)
        *(.sbss*)
        *(COMMON)
        . = ALIGN(4);
        _ebss = .;
    } >RAM

    .heap :
    {
        . = ALIGN(4);
        _heap_start = .;
    } >RAM

    _eram        = ORIGIN(RAM) + LENGTH(RAM);
}
