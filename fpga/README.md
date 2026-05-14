# Flashkeeper FPGA Gateware

Gateware and firmware for the iCE40UP5K FPGA on the Flashkeeper FPGA module, using PicoSoC and the PicoRV32 RISC-V core.

The Flashkeeper FPGA SoM has its own onboard 4 Mbit (512 kB) SPI flash chip, from which it loads its FPGA bitstream and firmware.
To build an image for this chip, run `make som_image`. To flash it to the SoM using flashrom and a standard CH341a SPI flash
programmer, run `make prog_flash_som`. The Flashkeeper SoM does not have a USB interface and must be flashed over SPI directly.

Targets and pin mappings are also provided for use with the official Lattice iCE40 UltraPlus Breakout Board (iCE40UP5K-B-EVN),
which has an onboard FT2232HL-based debug interface. To program this board using iceprog, run `make prog_flash_breakout`.

For KiCAD source for the Flashkeeper FPGA hardware, see hw/fpga