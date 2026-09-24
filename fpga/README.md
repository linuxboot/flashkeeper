# Flashkeeper FPGA

Gateware and firmware for the iCE40UP5K FPGA on the Flashkeeper FPGA module, using PicoSoC and the PicoRV32 RISC-V core.

To build an image for the Flashkeeper SoM development kit, run `make som_image`. To flash it to the SoM using flashrom
and a standard CH341a SPI flash programmer, run `make prog_flash_som`. During flashing, this target will attempt to hold
the FPGA in reset using an ESP-PROG (which can be used as a USB serial interface for a standalone Flashkeeper SoM).

Targets and pin mappings are also provided for use with the official Lattice iCE40 UltraPlus Breakout Board (iCE40UP5K-B-EVN),
which has an onboard FT2232HL-based debug interface. To program this board using iceprog, run `make prog_flash_breakout`.

For KiCAD source for the Flashkeeper FPGA hardware, see hw/fpga

## Boot Process
The Flashkeeper FPGA SoM has its own onboard 4 Mbit (512 kB) SPI flash chip (CSPI), which holds its FPGA bitstream and firmware.
The RISC-V reset vector and early boot is in boot ROM (in the FPGA's EBR, preloaded as part of the FPGA bitstream) - it reads the
firmware image header (magic + size, written into the image's reserved 256-byte header by the firmware build), copies
the image from the SPI flash into the SPRAM, and jumps into it, so the running system never executes from the flash.
The ROM is firmware-independent (the image header in flash describes the image, so the ROM should not need to be rebuilt
for firmware changes).

## SoC Features
- PicoRV32 RV32IZmmul CPU - hardware multiply, software divide (picorv32.v)
- Modified PicoSoC (picosoc.v)
- 128 KiB RAM (iCE40UP5k SPRAM - ice40up5k_spram.v)
- EBR-based bitstream Boot ROM (can be OTP-programmed as part of NVCM - ebrrom.v)
- Configuration (CSPI) and Host (HSPI) SPI flash interfaces (spimemio.v)
- Buffered UART serial interface (uart.v)
- Entropy source for TRNG (rng.v)
- On-chip (HFOSC) clock source for crystal-less Flashkeeper FPGA SoM (top.v)

## Firmware Features
- Forth-like Embedded Debug Environment shell (ede.h)
- Diagnostics and flash device management via serial interface (type help at EDE)
- Boot-ROM-based firmware loading from flash (rom/)
- Cryptography provided by TweetNaCl (tweetnacl.c)
- Serial password authentication support (main.c - password_prompt, generate_password)
- flashrom-compatible serprog emulation mode (serprog.c)
- Software-based entropy extraction and RNG monitoring (rng.c)

## Setting a Password
If you are using a Flashkeeper FPGA in a deployed system, we strongly recommend setting a serial password - if you do not do so,
anyone able to connect to your FPGA's UART interface can read and write both Host (your computer's) and Configuration (your Flashkeeper's)
SPI flash devices.

By default, a password is required (SERIAL_REQUIRE_PASSWORD = 1), and the default password is `flashkeeper`.
If SERIAL_REQUIRE_PASSWORD is set to 0, the FPGA will enter EDE automatically at firmware boot (with a warning to set a password).

To set a password, run `genpw` at the Flashkeeper EDE shell (connected over serial), then copy-and-paste the resulting two lines into
firmware/flashkeeper_config.h, then rebuild and reflash your Flashkeeper firmware image (as described above).

## Build Dependencies
On Debian, install:
```
sudo apt install yosys nextpnr-ice40 fpga-icestorm gcc-riscv64-unknown-elf
```

## Licensing
Unless stated otherwise, developments of the Flashkeeper project are licensed under the GNU General Public License
Version 3.

Files originating with the PicoRV32 are under the ISC License. They were originally developed by Claire Xenia Wolf. picosoc.v,
spimemio.v, uart.v, and ice40up5k_spram.v have been modified as part of the Flashkeeper project.

memops.S, used for certain RISC-V memory operations, is under the 2-clause BSD license and was developed by Alexander Vysokovskikh.

tweetnacl.c and tweetnacl.h are a public-domain cryptography library, and were developed by Daniel J. Bernstein,
Bernard van Gastel, Wesley Janssen, Tanja Lange, Peter Schwabe, and Sjaak Smetsers.