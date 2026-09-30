MEMORY
{
    FLASH (rx) : ORIGIN = 0x00020000, LENGTH = 0x60000
    RAM (xrw) : ORIGIN = 0x00000000, LENGTH = 0x20000
}
ENTRY(fw_entry)
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

    /* Minimum stack space to guarantee: the region [_ebss, _estack) must
       never shrink below this as image+bss grow. SP starts at _estack
       (entry.s) and may use the whole region; this is a floor, not a cap.
       (ASSERT is a top-level command in this binutils: it must sit
       outside the SECTIONS block, hence below it.) */
    MIN_STACK_SPACE = 8192;
    _estack = ORIGIN(RAM) + LENGTH(RAM);

    /* fwsiglib is loaded and verified at [LIB_BASE, ...) at boot. The image
       proper (code + data + bss, i.e. everything up to the heap start) must
       end before LIB_BASE; heap growth is untracked and must stay below it
       too or it corrupts the library. */
    LIB_BASE = 0x17800;
}
ASSERT(_estack - _ebss >= MIN_STACK_SPACE,
       "stack space below minimum: image+bss too large");
ASSERT(_heap_start <= LIB_BASE,
       "image (code+data+bss) collides with the fwsiglib slot at 0x17800");
