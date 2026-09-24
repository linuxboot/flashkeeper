#!/usr/bin/env python3
"""
Generate the EBR ROM init files for ebrrom.v (8 kB window, 2048 x 32-bit
words, one 16-bit hex word per line, low half and high half separately).

Usage: make_ebrrom_mem.py <lo.mem> <hi.mem> [source.bin]

Without a source file, writes the phase-0 test pattern. Each half is an
odd-multiple scramble of the word index, so no bit lane is constant
(yosys would otherwise strip constant lanes and the 2048x16 memory would
no longer match the SB_RAM40_4K width for EBR mapping):
    lo[w] = (w * 0x9E37) & 0xFFFF
    hi[w] = (w * 0x5511) & 0xFFFF
With a source file, the first 8192 bytes (little-endian 32-bit words,
zero-padded) are used.
"""
import sys

NWORDS = 2048  # 8 kB window


def main(lo_path, hi_path, src=None):
    if src is None:
        words = [((w * 0x5511 & 0xFFFF) << 16) | (w * 0x9E37 & 0xFFFF) for w in range(NWORDS)]
    else:
        with open(src, 'rb') as f:
            raw = f.read()
        raw = (raw + b'\x00' * (NWORDS * 4))[:NWORDS * 4]
        words = [int.from_bytes(raw[4 * w:4 * w + 4], 'little') for w in range(NWORDS)]

    with open(lo_path, 'w') as f:
        f.write(''.join(f'{w & 0xFFFF:04x}\n' for w in words))
    with open(hi_path, 'w') as f:
        f.write(''.join(f'{w >> 16:04x}\n' for w in words))
    print(f"ebrrom: wrote {lo_path} and {hi_path} ({NWORDS} words, "
          f"{'pattern' if src is None else src})")


if __name__ == '__main__':
    assert 3 <= len(sys.argv) <= 4, __doc__
    main(sys.argv[1], sys.argv[2], sys.argv[3] if len(sys.argv) == 4 else None)
