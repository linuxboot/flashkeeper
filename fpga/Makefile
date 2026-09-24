buildpath = build/
filename = flashkeeper
files = top.v spimemio.v uart.v picosoc.v picorv32.v ice40up5k_spram.v ebrrom.v rng.v
# Both overridable: make prog_flash_som flashrom=/path/to/flashrom  (or FLASHROM=... in the environment)
# The default assumes flashrom is built in ~/Flashkeeper/flashrom (see the flashrom repo).
flashrom ?= $(HOME)/Flashkeeper/flashrom/builddir/flashrom -p ch341a_spi -c AT25DF041B
# ESP-PROG console (model-generic by-id, no chip serial), used to hold the FPGA in reset during flashing
SOM_SERIAL ?= /dev/serial/by-id/usb-FTDI_Dual_RS232-HS-if01-port0
TOOLCHAIN_PREFIX = riscv64-unknown-elf-

# Boot ROM ISA. The core is built with ENABLE_FAST_MUL(1) (hardware
# multiply) and ENABLE_DIV(0) (no hardware divide) - see top.v. So use
# Zmmul (multiply only), NOT the full M extension: under rv32im GCC emits
# div/rem for C '/' and '%', which trap on a core with no divide unit.
# Under rv32izmmul GCC emits 'mul' (hw) and calls libgcc's software
# __divsi3/__modsi3 for division.
ROM_MARCH = rv32izmmul

# Synthesis passes: the standard `synth_ice40` flow, with only the
# `memory_libmap` step redone here without `-no-auto-huge`: the EBR boot ROM
# (ebrrom.v) is a 2048x16 memory spanning 8 SB_RAM40_4K blocks, and the
# one-shot `synth_ice40` hardcodes `memory_libmap ... -no-auto-huge` (which
# refuses to map one memory across multiple blocks; memory_libmap -help:
# "Disables automatic mapping of given kind of RAMs", auto-huge = multi-block),
# so the ROM would otherwise fall back to LUT RAM. The stock script is run
# up to just before the `map_ram` label (all front-end/opt/memory-collect
# passes, unchanged), the libmap + brams_map.v techmap + ice40_braminit
# steps are repeated verbatim except for the dropped flag, and the stock
# script resumes at the `map_ffram` label (abc9 etc. all unchanged).
SYNTH = synth_ice40 -dsp -top top -run :map_ram; memory_libmap -lib +/ice40/brams.txt -lib +/ice40/spram.txt; techmap -map +/ice40/brams_map.v -map +/ice40/spram_map.v; ice40_braminit; synth_ice40 -dsp -top top -run map_ffram:

# Boot ROM: lives in the EBR (window 0x06000000, 8 kB, see ebrrom.v and
# rom/rom.lds) and is the FPGA's reset vector. It is firmware-independent:
# the firmware image is self-describing (magic + size in its header, see
# rom/boot.c and firmware/tools/mkflashimg.py), so the ROM never needs a
# rebuild because the firmware changed, and the bitstream build below has
# no dependency on the firmware either.
# It must not use any SPRAM (no .data/.bss, checked below): its only RAM
# footprint is the stack at the top of the SPRAM, which the firmware
# image uses too.
build/rom.elf: rom/rom_start.S rom/boot.c rom/rom.lds
	$(TOOLCHAIN_PREFIX)gcc -c -mabi=ilp32 -march=$(ROM_MARCH) -o build/rom_start.o rom/rom_start.S; \
	$(TOOLCHAIN_PREFIX)gcc -c -Os --std=c99 -Wall -Wextra -ffreestanding -nostdlib -mabi=ilp32 -march=$(ROM_MARCH) -o build/boot.o rom/boot.c; \
	$(TOOLCHAIN_PREFIX)gcc -Os -mabi=ilp32 -march=$(ROM_MARCH) -ffreestanding -nostdlib -o $@ -Wl,-Bstatic,-T,rom/rom.lds,--strip-debug build/rom_start.o build/boot.o -lgcc
build/rom.bin: build/rom.elf
	$(TOOLCHAIN_PREFIX)objcopy -O binary $< $@
	@set -- $(shell $(TOOLCHAIN_PREFIX)size -B $< | awk 'NR==2 {print $$1, $$2, $$3}'); if [ $$3 -ne 0 ]; then echo "error: boot ROM has BSS ($$3 bytes); it must not use SPRAM" >&2; exit 1; fi; if [ $$1 -gt 8192 ]; then echo "error: boot ROM is $$1 bytes; the EBR window is 8 kB" >&2; exit 1; fi; echo "boot ROM: text=$$1 data=$$2 bss=$$3 (<= 8192 bytes, no BSS)"

# EBR ROM init data (build/ebrrom_{lo,hi}.mem, read by ebrrom.v at synthesis
# time), generated from the boot ROM binary (see tools/make_ebrrom_mem.py).
build/ebrrom_lo.mem build/ebrrom_hi.mem: tools/make_ebrrom_mem.py build/rom.bin
	python3 tools/make_ebrrom_mem.py build/ebrrom_lo.mem build/ebrrom_hi.mem build/rom.bin

.PHONY: prog_breakout prog_fw_breakout prog_flash_breakout som_image prog_flash_som firmware clean

$(buildpath)$(filename)_breakout.bin: $(files) io_breakout.pcf build/ebrrom_lo.mem build/ebrrom_hi.mem
	yosys -p "$(SYNTH); write_json $(buildpath)$(filename)_breakout.json" $(files)
	nextpnr-ice40 --freq 13 --package sg48 --up5k --ignore-loops --json $(buildpath)$(filename)_breakout.json --pcf io_breakout.pcf --asc $(buildpath)$(filename)_breakout.asc
	icepack $(buildpath)$(filename)_breakout.asc $(buildpath)$(filename)_breakout.bin

$(buildpath)$(filename)_som.bin: $(files) io_som.pcf build/ebrrom_lo.mem build/ebrrom_hi.mem
	yosys -p "$(SYNTH); write_json $(buildpath)$(filename)_som.json" $(files)
	nextpnr-ice40 --freq 13 --package uwg30 --up5k --ignore-loops --json $(buildpath)$(filename)_som.json --pcf io_som.pcf --asc $(buildpath)$(filename)_som.asc
	icepack $(buildpath)$(filename)_som.asc $(buildpath)$(filename)_som.bin

prog_breakout: $(buildpath)$(filename)_breakout.bin #for sram
	iceprog -S $(buildpath)$(filename)_breakout.bin

# Program the breakout's SPI flash (Lattice iCE40 UltraPlus Breakout Board,
# iCE40UP5K-B-EVN, FPGA-UG-02001; onboard flash is a 4 MB N25Q032A1) with
# both halves of the image, one iceprog pass per half (iceprog writes one
# file at one flash offset per run; -o sets the flash start address, per
# `iceprog --help`). The J6 shunt must be in the PROG FLASH position so the
# FTDI's SPI reaches the flash:
#   1. firmware image -> flash offset 128 kB: the boot ROM's image window
#      (FW_IMAGE_FLASH = 0x01020000, see rom/boot.c)
#   2. bitstream -> flash offset 0
# The default erase mode wipes 64 kB-aligned chunks, so the two passes never
# touch each other's region (the bitstream is < 128 kB). iceprog programs
# the flash through the FTDI and does not reconfigure the FPGA: power-cycle
# the board afterwards to boot the new image (bitstream at 0 -> boot ROM ->
# firmware at 128 kB). Best effort: this board hasn't been exercised
# recently - re-verify it on the bench before relying on it.
prog_fw_breakout: firmware
	iceprog -o 128k firmware/build/firmware.bin

prog_flash_breakout: $(buildpath)$(filename)_breakout.bin prog_fw_breakout
	iceprog $(buildpath)$(filename)_breakout.bin

som_image: $(buildpath)$(filename)_som.bin firmware
	dd if=/dev/zero of=$(buildpath)$(filename)_som_image.bin bs=512k count=1  # create a blank 512 kB flash image
	dd if=$(buildpath)$(filename)_som.bin of=$(buildpath)$(filename)_som_image.bin conv=notrunc  # copy the bitstream to the beginning
	dd if=firmware/build/firmware.bin of=$(buildpath)$(filename)_som_image.bin bs=1 seek=128k conv=notrunc  # copy the firmware beginning at 128 kB

prog_flash_som: som_image
	python3 tools/hold_reset.py --port $(SOM_SERIAL) -- $(flashrom) -w $(buildpath)$(filename)_som_image.bin

firmware:
	$(MAKE) --directory=firmware/

clean:
	rm -rf $(buildpath)/*
	rm -rf firmware/build/*