#!/usr/bin/env python3
"""
Hold the Flashkeeper SoM FPGA in reset while a command runs (typically
flashrom programming over the CH341a), then release it.

On the development breakout harness, the ESP-PROG FTDI modem lines are
wired so that DTR=0, RTS=1 pulls ESP_EN low, holding the iCE40 in reset.
In reset the FPGA's SPI pins tristate, freeing the onboard flash bus for
the CH341a programmer. DTR=1 releases reset (RTS value is irrelevant);
the FPGA then reboots from its SPI flash.

Usage:
    hold_reset.py [--port /dev/serial/by-id/...] -- <command> [args...]

Exits with the command's exit status. The port is held open for the
duration of the command so the reset state cannot drift.
"""
import argparse
import subprocess
import sys
import time

import serial

DEFAULT_PORT = '/dev/serial/by-id/usb-FTDI_Dual_RS232-HS-if01-port0'
BAUD = 1000000  # must match the firmware reg_uart_clkdiv (see main.c)


def set_lines(s, dtr, rts, label):
    if rts != s.rts:
        s.rts = bool(rts)
    if dtr != s.dtr:
        s.dtr = bool(dtr)
    print(f"[hold_reset] {label} (DTR={int(dtr)}, RTS={int(rts)})", flush=True)
    time.sleep(0.1)  # let the lines settle


def main():
    ap = argparse.ArgumentParser(description=__doc__.split('\n')[1])
    ap.add_argument('--port', default=DEFAULT_PORT,
                    help=f'FTDI console port (default: {DEFAULT_PORT})')
    ap.add_argument('command', nargs=argparse.REMAINDER,
                    help='command to run while the FPGA is held in reset')
    args = ap.parse_args()

    cmd = args.command
    if cmd and cmd[0] == '--':
        cmd = cmd[1:]
    if not cmd:
        ap.error("no command given after '--'")

    try:
        s = serial.Serial(args.port, BAUD, timeout=1)
    except serial.SerialException as e:
        sys.exit(f"[hold_reset] cannot open {args.port}: {e}")

    rc = 1
    try:
        # assert reset: DTR=0, RTS=1
        set_lines(s, 0, 1, 'FPGA held in reset')
        rc = subprocess.call(cmd)
    finally:
        # release reset: DTR=1
        set_lines(s, 1, 1, 'reset released, FPGA rebooting')
        end = time.time() + 3.0
        while time.time() < end:
            n = s.in_waiting
            if n:
                sys.stdout.write(s.read(n).decode('ascii', 'replace'))
                sys.stdout.flush()
                end = max(end, time.time() + 0.5)
            else:
                time.sleep(0.02)
        s.close()
    sys.exit(rc)


if __name__ == '__main__':
    main()
