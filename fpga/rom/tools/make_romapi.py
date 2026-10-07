#!/usr/bin/env python3
"""
Generate the firmware's EBR call-in flags (rom/tools/make_romapi.py --print-defs)
from the linked boot ROM (build/rom.elf). The firmware calls the SHA-512
code in ROM rather than needing to provide its own copy.

The flags (a single line, no generated files - the build injects them
into the romcalls.s compile, see firmware/Makefile) define:

  FK_ROM_SHA512         absolute address of crypto_hash_sha512_tweet
  FK_ROM_SHA512_BLOCKS  absolute address of
                        crypto_hashblocks_sha512_tweet

Usage: make_romapi.py <rom.elf> --print-defs
"""
import subprocess
import sys

SYMS = {
    "crypto_hash_sha512_tweet": "FK_ROM_SHA512",
    "crypto_hashblocks_sha512_tweet": "FK_ROM_SHA512_BLOCKS",
}

def main(elf_path, print_defs):
    out = subprocess.run(
        ["riscv64-unknown-elf-nm", elf_path],
        capture_output=True, text=True, check=True).stdout
    addrs = {}
    for line in out.splitlines():
        p = line.split()
        if len(p) >= 3 and p[2] in SYMS and p[1] in ("T", "t"):
            addrs[p[2]] = int(p[0], 16)
    missing = [s for s in SYMS if s not in addrs]
    assert not missing, f"missing in {elf_path}: {missing} (gc-sections?)"

    if print_defs:
        print(" ".join("-D%s=0x%08xu" % (macro, addrs[sym])
                       for sym, macro in SYMS.items()))

if __name__ == '__main__':
    assert len(sys.argv) == 3 and sys.argv[2] == '--print-defs', __doc__
    main(sys.argv[1], True)
