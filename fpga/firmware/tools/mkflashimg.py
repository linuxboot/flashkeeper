#!/usr/bin/env python3
"""
Build the flash image from the firmware ELF.

The firmware executes from the SPRAM, so the ELF's VMAs are in the RAM
region (0x00000000) and its load addresses (LMAs) in the flash XIP
region (0x00020000). The flash image (flash byte 128 kB) is the
concatenation of the loadable sections in VMA order, zero-filled between
them, from RAM 0x0 to _edata:

    0x000  .fw_header  256-byte header: magic + size, written below
    0x100  .text       code + rodata; entry point at the start (entry.s)
    ...    .data

The image is self-describing: the boot ROM (firmware-independent, see
fpga/rom/boot.c) reads the magic and size from the header to know how 
much to copy. The header format must stay in sync with boot.c.

The boot ROM loads the image with one flat copy from flash 0x20000 to
RAM 0x0, so it is essential that every loadable byte satisfies
LMA = VMA + 0x20000; this is checked below.
"""
import os
import re
import struct
import sys

# Extract the memory layout constants from memmap.h by pattern-matching #defines
MEMMAP_PATH = os.path.join(os.path.dirname(__file__), "..", "..", "memmap.h")

def memmap(name):
    with open(MEMMAP_PATH) as f:
        m = re.search(rf"^#define {name}[ \t]+(0x[0-9a-fA-F]+)", f.read(), re.M)
    assert m is not None, f"{name} not found in {MEMMAP_PATH}"
    return int(m.group(1), 16)

FLASH_BASE = memmap("FK_FLASH_IMAGE_OFF")
IMG_MAGIC = 0x57464B46  # "FKFW" little-endian; must match fpga/rom/boot.c
PROGBITS = 1
PT_LOAD = 1


def main(elfpath, outpath):
    with open(elfpath, 'rb') as f:
        data = f.read()
    assert data[:4] == b'\x7fELF', "not an ELF file"
    assert data[4] == 1, "expected ELF32"
    assert data[5] == 1, "expected little-endian"

    e_phoff, = struct.unpack_from('<I', data, 28)
    e_shoff, = struct.unpack_from('<I', data, 32)
    e_phentsize, = struct.unpack_from('<H', data, 42)
    e_phnum, = struct.unpack_from('<H', data, 44)
    e_shentsize, = struct.unpack_from('<H', data, 46)
    e_shnum, = struct.unpack_from('<H', data, 48)
    e_shstrndx, = struct.unpack_from('<H', data, 50)

    phdrs = []
    for i in range(e_phnum):
        off = e_phoff + i * e_phentsize
        p_type, p_offset, p_vaddr, p_paddr, p_filesz, p_memsz = \
            struct.unpack_from('<IIIIII', data, off)
        phdrs.append((p_type, p_offset, p_vaddr, p_paddr, p_filesz, p_memsz))

    shdrs = []
    for i in range(e_shnum):
        off = e_shoff + i * e_shentsize
        sh_name, sh_type, sh_flags, sh_addr, sh_offset, sh_size = \
            struct.unpack_from('<IIIIII', data, off)
        shdrs.append((sh_name, sh_type, sh_flags, sh_addr, sh_offset, sh_size))

    strtab = shdrs[e_shstrndx]

    def sh_name_str(idx):
        start = strtab[4] + idx
        end = data.index(b'\x00', start)
        return data[start:end].decode()

    # Image size = _edata (end of .data VMA) from the symbol table.
    symtab = None
    for i, (sh_name, sh_type, sh_flags, sh_addr, sh_offset, sh_size) in enumerate(shdrs):
        if sh_type == 2:  # SHT_SYMTAB
            symtab = i
    assert symtab is not None, "no symbol table"
    sh_name, sh_type, sh_flags, sh_addr, sh_offset, sh_size = shdrs[symtab]
    sh_link, = struct.unpack_from('<I', data, e_shoff + symtab * e_shentsize + 24)
    sh_entsize, = struct.unpack_from('<H', data, e_shoff + symtab * e_shentsize + 36)
    assert sh_entsize == 16, "expected 32-bit symbols"
    str_off = shdrs[sh_link][4]
    edata = None
    for i in range(sh_size // sh_entsize):
        off = sh_offset + i * sh_entsize
        st_name, st_value, st_size, st_info, st_other, st_shndx = \
            struct.unpack_from('<IIIBBH', data, off)
        name_start = str_off + st_name
        name = data[name_start:data.index(b'\x00', name_start)].decode()
        if name == '_edata':
            edata = st_value
    assert edata is not None, "_edata symbol not found"
    assert edata % 4 == 0, f"_edata {edata:#x} not word aligned"

    # Verify that the firmware image does not collide with the fwsiglib slot above it (from memmap.h)
    img_max = memmap("FK_IMG_MAX")
    assert 0 < edata <= img_max, f"_edata {edata:#x} exceeds the {img_max:#x} fwsiglib slot boundary"

    # Assemble the image.
    img = bytearray(edata)
    for p_type, p_offset, p_vaddr, p_paddr, p_filesz, p_memsz in phdrs:
        if p_type != PT_LOAD:
            continue
        assert p_paddr == p_vaddr + FLASH_BASE, \
            f"segment at VMA {p_vaddr:#x}: LMA {p_paddr:#x} != VMA + {FLASH_BASE:#x}; " \
            "the boot ROM's flat copy would be corrupted"
        assert p_vaddr >= 0 and p_vaddr + p_filesz <= edata, \
            f"segment at VMA {p_vaddr:#x}, size {p_filesz:#x} outside [0, {edata:#x})"
        img[p_vaddr:p_vaddr + p_filesz] = data[p_offset:p_offset + p_filesz]

    # .text (and the entry stub) must sit at VMA 0x100, right after the 256-byte reserved header.
    stext = None
    for i in range(sh_size // sh_entsize):
        off = sh_offset + i * sh_entsize
        st_name, st_value, st_size, st_info, st_other, st_shndx = \
            struct.unpack_from('<IIIBBH', data, off)
        name_start = str_off + st_name
        name = data[name_start:data.index(b'\x00', name_start)].decode()
        if name == '_stext':
            stext = st_value
    assert stext == 0x100, f"_stext is {stext:#x}, expected 0x100 (entry point from ROM)"

    # Write the image header the boot ROM reads: magic + total size.
    assert all(b == 0 for b in img[0:0x100]), \
        ".fw_header region must be empty (magic/size are written here)"
    struct.pack_into('<II', img, 0, IMG_MAGIC, edata)

    with open(outpath, 'wb') as f:
        f.write(img)
    print(f"flash image: {len(img)} bytes "
          f"({edata:#x} = _edata), header magic+size written, "
          f"segments checked for LMA = VMA + {FLASH_BASE:#x}")


if __name__ == '__main__':
    assert len(sys.argv) == 3, "usage: mkflashimg.py <firmware.elf> <image.bin>"
    main(sys.argv[1], sys.argv[2])
