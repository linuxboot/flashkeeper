#!/usr/bin/env python3
"""
Generate the fwsiglib build flags (rom/tools/make_lib_inc.py --print-defs)
from the compiled ed25519 verify library (build/libed25519.elf + .bin,
linked at the fixed address in rom/lib/lib.lds).

The flags define:
  FLIB_ADDR      fixed RAM address the ROM loads the library into
  FLIB_SIZE      library blob size in bytes (a multiple of 4)
  FLIB_HASH_INIT initializer for the 64-byte SHA-512 hash the ROM
                 compares the RAM copy against (see rom/boot.c)
  FK_FLIB_TARGET FLIB_ADDR + offset of fk_lib_verify: the absolute
                 address the trampoline (rom/libcall.S) jumps to

Changing the library binary changes FLIB_HASH_INIT, which is baked
into the ROM at build time, so a library change entails a ROM rebuild.

Usage: make_lib_inc.py <lib.elf> <lib.bin> <flib_base> --print-defs
"""
import hashlib
import subprocess
import sys

ENTRY = "fk_lib_verify"

def main(elf_path, bin_path, lib_base, print_defs):
    lib_base = int(lib_base, 16)

    with open(bin_path, 'rb') as f:
        blob = f.read()
    assert len(blob) > 0, f"{bin_path} is empty"
    assert len(blob) % 4 == 0, \
        f"{bin_path} is {len(blob)} bytes; the ROM copies it word-wise"

    # Entry point offset within the flat blob (LMA == VMA == lib_base).
    out = subprocess.run(
        ["riscv64-unknown-elf-nm", elf_path],
        capture_output=True, text=True, check=True).stdout
    entry = None
    for line in out.splitlines():
        p = line.split()
        if len(p) >= 3 and p[2] == ENTRY:
            entry = int(p[0], 16)
    assert entry is not None, \
        f"{ENTRY} not found in {elf_path}; is rom/lib/fk_verify.c in the build?"
    off = entry - lib_base
    assert 0 <= off < len(blob), \
        f"entry {entry:#x} not within the {lib_base:#x}+{len(blob)} blob"

    pin = hashlib.sha512(blob).digest()
    target = lib_base + off

    if print_defs:
        # One -D per value; the hash initializer is one word (no spaces)
        # so it survives shell word-splitting in the make recipe.
        init = "{" + ",".join("0x%02x" % b for b in pin) + "}"
        print("-DFLIB_ADDR=0x%xu -DFLIB_SIZE=0x%xu "
              "-DFLIB_HASH_INIT=%s -DFK_FLIB_TARGET=0x%x"
              % (lib_base, len(blob), init, target))
        return

    print("flib: %d bytes, entry %s at +0x%x (target 0x%x)"
          % (len(blob), ENTRY, off, target))

if __name__ == '__main__':
    assert len(sys.argv) == 5 and sys.argv[4] == '--print-defs', __doc__
    main(sys.argv[1], sys.argv[2], sys.argv[3], True)
