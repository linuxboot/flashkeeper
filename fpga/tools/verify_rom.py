#!/usr/bin/env python3
"""Read back the EBR ROM window on the live SoM and compare with the built rom.bin.

Usage: verify_rom.py [port] [rom.bin] [baud] [--password PW] [--reset]

  --password PW  the serial console password (see flashkeeper_config.h).
                 When given, the script waits for the "Serial Password:" prompt,
                 logs in, and waits for the "Starting debug shell" banner.
  --reset        cycle the FPGA reset (DTR=0/RTS=1, then DTR=1) before
                 logging in, to guarantee a fresh boot. See hold_reset.py
                 for the reset wiring.
"""
import argparse
import os
import re
import sys
import time

import serial

DEFAULT_PORT = '/dev/serial/by-id/usb-FTDI_Dual_RS232-HS-if01-port0'  # ESP-PROG console (model-generic by-id, no chip serial)
# resolve relative to this script (fpga/tools/), so the repo can live anywhere
DEFAULT_ROM = os.path.normpath(os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'build', 'rom.bin'))
DEFAULT_BAUD = 1000000  # must match the firmware reg_uart_clkdiv (see main.c)
LOGIN_TIMEOUT = 15.0
BROM_SIZE = 8192  # the full EBR ROM window in bytes (2048 x 32-bit words)


def read_until(s, marker, timeout):
    """Read from the port until `marker` appears in the stream. Returns (data, seen)."""
    end = time.time() + timeout
    buf = b''
    while time.time() < end:
        n = s.in_waiting
        if n:
            buf += s.read(n)
            if marker in buf:
                return buf, True
            end = max(end, time.time() + 1.0)
        else:
            time.sleep(0.02)
    return buf, marker in buf


def main():
    ap = argparse.ArgumentParser(description=__doc__.split('\n')[1])
    ap.add_argument('port', nargs='?', default=DEFAULT_PORT)
    ap.add_argument('rom', nargs='?', default=DEFAULT_ROM)
    ap.add_argument('baud', nargs='?', type=int, default=DEFAULT_BAUD)
    ap.add_argument('--password', default=None,
                    help='serial console password (see flashkeeper_config.h)')
    ap.add_argument('--reset', action='store_true',
                    help='cycle the FPGA reset before logging in')
    args = ap.parse_args()

    rom = open(args.rom, 'rb').read()
    # The EBR is always the full window; the build zero-pads rom.bin to 8 kB
    # when generating the INIT data, so compare against the padded image.
    expected = rom.ljust(BROM_SIZE, b'\x00')

    s = serial.Serial(args.port, args.baud, timeout=1)
    s.reset_input_buffer()

    if args.reset:
        # assert reset (DTR=0, RTS=1), then release it (DTR=1) - see hold_reset.py
        s.dtr, s.rts = False, True
        time.sleep(0.2)
        s.dtr = True
        s.reset_input_buffer()

    if args.password is not None:
        buf, seen = read_until(s, b'Serial Password:', LOGIN_TIMEOUT)
        if not seen:
            sys.exit(f"FAIL: no 'Serial Password:' prompt within {LOGIN_TIMEOUT:.0f}s "
                     f"(is the password gate enabled? is the SoM responsive?)\n"
                     f"got: {buf.decode('ascii', 'replace')!r}")
        s.reset_input_buffer()
        s.write(args.password.encode() + b'\r')
        buf, seen = read_until(s, b'Starting debug shell', LOGIN_TIMEOUT)
        if not seen:
            sys.exit(f"FAIL: no 'Starting debug shell' banner after login (bad password?)\n"
                     f"got: {buf.decode('ascii', 'replace')!r}")
    else:
        # No password given: bail out with a clear error if the gate is active.
        buf, seen = read_until(s, b'Serial Password:', 3.0)
        if seen:
            sys.exit("FAIL: the console is asking for a password; rerun with --password")

    s.reset_input_buffer()
    s.write(b'\r')  # get a prompt
    time.sleep(0.2)
    s.reset_input_buffer()
    # romdump prints 2 hex digits per byte. EDE numeric literals are hex,
    # so 0x2000 = 8192 decimal = the whole ROM window.
    print("--- sending '0 2000 romdump' ---", flush=True)
    s.write(b'0 2000 romdump\r')

    end = time.time() + 15
    buf = b''
    m = None
    while time.time() < end:
        n = s.in_waiting
        if n:
            buf += s.read(n)
            end = max(end, time.time() + 2.0)
            m = re.search(rb'0 2000 romdump\r\n((?: [0-9a-fA-F]{2})+)\r\nok', buf)
        else:
            time.sleep(0.05)
        if m:
            break
    s.close()

    if not m:
        print("FAIL: no complete romdump received (timeout or shell error)")
        print(buf.decode('ascii', 'replace')[:2000])
        sys.exit(1)

    got = bytes(int(t, 16) for t in re.findall(rb'[0-9a-fA-F]{2}', m.group(1)))
    print(f"received {len(got)} bytes")
    if len(got) != BROM_SIZE:
        print(f"FAIL: expected {BROM_SIZE} bytes")
        sys.exit(1)

    bad = [i for i in range(BROM_SIZE) if got[i] != expected[i]]
    if bad:
        print(f"FAIL: {len(bad)} mismatched bytes, first at {bad[:8]}")
        for i in bad[:8]:
            print(f"  [0x{i:04x}] got {got[i]:02x} want {expected[i]:02x}")
        sys.exit(1)
    print(f"PASS: ROM window matches rom.bin ({len(rom)} bytes, zero-padded to {BROM_SIZE})")
    print(f"byte[0..3] = {got[0:4].hex(' ')}  (expect 37 01 02 00 = 'lui sp, 0x20')")


if __name__ == '__main__':
    main()
