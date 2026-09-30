buildpath = build/
filename = flashkeeper
files = top.v spimemio.v uart.v picosoc.v picorv32.v ice40up5k_spram.v ebrrom.v rng.v
# Both overridable: make prog_flash_som flashrom=/path/to/flashrom  (or FLASHROM=... in the environment)
# The default assumes flashrom is built in ~/Flashkeeper/flashrom (see the flashrom repo).
flashrom ?= $(HOME)/Flashkeeper/flashrom/builddir/flashrom -p ch341a_spi -c AT25DF041B
# ESP-PROG console (model-generic by-id, no chip serial), used to hold the FPGA in reset during flashing
SOM_SERIAL ?= /dev/serial/by-id/usb-FTDI_Dual_RS232-HS-if01-port0
TOOLCHAIN_PREFIX = riscv64-unknown-elf-

# SIGN=1 (default): Build a signed firmware image. Requires a secret key at
# firmware/keys/sk.key - generate your own with firmware/tools/genkey.
# If you build an unsigned image with SIGN=0, you will also need to build
# your boot ROM with ROM_ALLOW_UNSIGNED=1, otherwise the Boot ROM will reject
# the image ("BadType").
SIGN ?= 1

# ROM_ALLOW_UNSIGNED: If 1, build a BootROM that will allow loading any firmware
# image, signed or not. ROM_ALLOW_UNSIGNED ROMs will still attempt to verify a signature
# if one is present, but will also load Type 0 (IMG_UNSIGNED) images without verification.
# The boot ROM in this configuration provides no protection against firmware tampering.
# Use for development or non-secure applications only.
ifneq ($(ROM_ALLOW_UNSIGNED),1)
ROM_UNSIGNED_FLAG =
else
ROM_UNSIGNED_FLAG = -DROM_ALLOW_UNSIGNED
$(warning ROM_ALLOW_UNSIGNED=1: This BootROM will load unsigned firmware images! Use for development only.)
endif

ifeq ($(SIGN),1)
FW_IMG = firmware/build/firmware_signed.bin
else
FW_IMG = firmware/build/firmware.bin
endif

# Boot ROM ISA. The core is built with ENABLE_FAST_MUL(1) (hardware
# multiply) and ENABLE_DIV(0) (no hardware divide) - see top.v. So use
# Zmmul (multiply only), NOT the full M extension: under rv32im GCC emits
# div/rem for C '/' and '%', which trap on a core with no divide unit.
# Under rv32izmmul GCC emits 'mul' (hw) and calls libgcc's software
# __divsi3/__modsi3 for division.
ROM_MARCH = rv32izmmul

# ROM optimization level (default -Os - the EBR ROM is small).
OPT ?= -Os

# Synthesis passes: the standard `synth_ice40` flow, with only the
# `memory_libmap` step redone here without `-no-auto-huge`: the EBR boot ROM
# (ebrrom.v) is two 2048x16 memories (rom_lo/rom_hi) spanning 16
# SB_RAM40_4K blocks (8 kB), and the
# one-shot `synth_ice40` hardcodes `memory_libmap ... -no-auto-huge` (which
# refuses to map one memory across multiple blocks; memory_libmap -help:
# "Disables automatic mapping of given kind of RAMs", auto-huge = multi-block),
# so the ROM would otherwise fall back to LUT RAM. The stock script is run
# up to just before the `map_ram` label (all front-end/opt/memory-collect
# passes, unchanged), the libmap + brams_map.v techmap + ice40_braminit
# steps are repeated verbatim except for the dropped flag, and the stock
# script resumes at the `map_ffram` label (abc9 etc. all unchanged).
SYNTH = synth_ice40 -dsp -top top -run :map_ram; memory_libmap -lib +/ice40/brams.txt -lib +/ice40/spram.txt; techmap -map +/ice40/brams_map.v -map +/ice40/spram_map.v; ice40_braminit; synth_ice40 -dsp -top top -run map_ffram:

# Boot ROM: synthesized into EBR ROM (window 0x06000000, 8 kB, see ebrrom.v and
# rom/rom.lds). Contains the core's reset vector. The boot ROM loads the firmware
# image from the CSPI flash into the SPRAM, verifies its ed25519 signature (the
# signer key is built in from firmware/keys/pk.key via build/fwpk.inc), and
# jumps to it. It must not use .bss, as there is no guarantee that memory has
# been zeroed.
#
# The Boot ROM uses firmware/tweetnacl.c for some cryptographic operations
# (SHA-512 hashing and crypto_verify_* comparison), but, for performance
# reasons, does not use TweetNaCl's ed25519. TweetNaCl is compiled with
# -fdata-sections and linked with --gc-sections, because it includes a lot
# of code which is not used in the Boot ROM and which would not fit in EBR.
build/rom.elf: build/flag_config rom/rom_start.S rom/boot.c rom/libcall.S rom/rom.lds build/fwpk.inc build/libed25519.elf build/libed25519.bin rom/tools/make_lib_inc.py firmware/tweetnacl.c firmware/tweetnacl.h
	set -e; \
	FDEFS=$$(python3 rom/tools/make_lib_inc.py build/libed25519.elf build/libed25519.bin $(FLIB_ADDR) --print-defs) || exit 1; \
	test -n "$$FDEFS" || { echo "error: make_lib_inc.py produced no output" >&2; exit 1; }; \
	$(TOOLCHAIN_PREFIX)gcc -c -mabi=ilp32 -march=$(ROM_MARCH) -o build/rom_start.o rom/rom_start.S; \
	$(TOOLCHAIN_PREFIX)gcc -c $(OPT) --std=c99 -Wall -Wextra -ffreestanding -nostdlib -mabi=ilp32 -march=$(ROM_MARCH) -ffunction-sections $$FDEFS $(ROM_UNSIGNED_FLAG) -I build -I firmware -o build/boot.o rom/boot.c; \
	$(TOOLCHAIN_PREFIX)gcc -c -mabi=ilp32 -march=$(ROM_MARCH) -o build/libcall.o rom/libcall.S $$FDEFS; \
	$(TOOLCHAIN_PREFIX)gcc -c $(OPT) --std=c99 -Wall -Wextra -Wno-sign-compare -ffreestanding -nostdlib -mabi=ilp32 -march=$(ROM_MARCH) -ffunction-sections -fdata-sections -I firmware -o build/rom_tweetnacl.o firmware/tweetnacl.c; \
	$(TOOLCHAIN_PREFIX)gcc $(OPT) -mabi=ilp32 -march=$(ROM_MARCH) -ffreestanding -nostdlib -o $@ -Wl,-Bstatic,-T,rom/rom.lds,--strip-debug,--gc-sections build/rom_start.o build/boot.o build/libcall.o build/rom_tweetnacl.o -lgcc
build/rom.bin: build/rom.elf
	$(TOOLCHAIN_PREFIX)objcopy -O binary $< $@
	@set -- $(shell $(TOOLCHAIN_PREFIX)size -B $< | awk 'NR==2 {print $$1, $$2, $$3}'); if [ $$3 -ne 0 ]; then echo "error: boot ROM has BSS ($$3 bytes); it must not use SPRAM" >&2; exit 1; fi; if [ $$(($$1 + $$2)) -gt 8192 ]; then echo "error: boot ROM is $$(($$1 + $$2)) bytes (text+data); the EBR window is 8 kB" >&2; exit 1; fi; echo "boot ROM: text=$$1 data=$$2 bss=$$3 (text+data <= 8192 bytes, no BSS)"

# Save flags (e.g. ROM_ALLOW_UNSIGNED) in build/flag_config, so that changes to
# these flags force a rebuild when necessary.
build/flag_config: FORCE
	@printf '%s\n' "$(ROM_UNSIGNED_FLAG) | $(OPT)" | cmp -s $@ 2>/dev/null || printf '%s\n' "$(ROM_UNSIGNED_FLAG) | $(OPT)" > $@

# fwsiglib (see rom/lib/) is a minimal wrapper for libsodium's ed25519
# implementation, which the boot ROM loads from flash into SPRAM, verifies
# by hash, then uses to verify the firmware image signature. It is linked
# at a fixed address (LMA == VMA, flat blob), and must fit in between the
# firmware image and the stack [0x17800, 0x1E000). Cannot use .bss for the
# same reason the Boot ROM cannot.
FLIB_ADDR = 0x17800
LIB_CFLAGS = -Os --std=c99 -Wall -Wextra -Wno-sign-compare -ffreestanding -nostdlib -mabi=ilp32 -march=rv32izmmul -ffunction-sections -fdata-sections -Irom/lib/inc
build/libed25519.elf: build/flag_config rom/lib/ed25519_ref10.c rom/lib/ref10_stub_impl.c rom/lib/fk_verify.c $(wildcard rom/lib/inc/*.h) $(wildcard rom/lib/inc/private/*.h) rom/lib/fe_25_5/fe.h rom/lib/fe_25_5/constants.h rom/lib/fe_25_5/base.h rom/lib/fe_25_5/base2.h rom/lib/lib.lds
	set -e; \
	$(TOOLCHAIN_PREFIX)gcc -c $(LIB_CFLAGS) -o build/lib_ref10.o rom/lib/ed25519_ref10.c; \
	$(TOOLCHAIN_PREFIX)gcc -c $(LIB_CFLAGS) -o build/lib_fkverify.o rom/lib/fk_verify.c; \
	$(TOOLCHAIN_PREFIX)gcc -c $(LIB_CFLAGS) -o build/lib_stub.o rom/lib/ref10_stub_impl.c; \
	$(TOOLCHAIN_PREFIX)gcc $(OPT) -mabi=ilp32 -march=rv32izmmul -ffreestanding -nostdlib -o $@ -Wl,-Bstatic,-T,rom/lib/lib.lds,--strip-debug,--gc-sections build/lib_ref10.o build/lib_fkverify.o build/lib_stub.o -lgcc
build/libed25519.bin: build/libed25519.elf
	$(TOOLCHAIN_PREFIX)objcopy -O binary $< $@
	@set -- $(shell $(TOOLCHAIN_PREFIX)size -B $< | awk 'NR==2 {print $$1, $$2, $$3}'); if [ $$3 -ne 0 ]; then echo "error: fwsiglib has BSS ($$3 bytes); nothing zeroes it" >&2; exit 1; fi; if [ $$(($$1 + $$2)) -gt $$((0x6800)) ]; then echo "error: fwsiglib is $$(($$1 + $$2)) bytes; available slot is 26 kB ([0x17800, 0x1E000))" >&2; exit 1; fi; if [ $$(($$1 + $$2)) % 4 -ne 0 ]; then echo "error: fwsiglib size is not a multiple of 4" >&2; exit 1; fi; echo "fwsiglib: text=$$1 data=$$2 bss=$$3 (slot [0x17800, 0x1E000))"

# The ROM's signer public key (build/fwpk.inc): generated from the user's public
# key. A rule with only a recipe, so it only runs when the key is missing.
firmware/keys/pk.key:
	@echo "error: firmware/keys/pk.key not found - generate a keypair first:" >&2
	@echo "       make -C firmware tools/genkey && firmware/tools/genkey firmware/keys/pk.key firmware/keys/sk.key" >&2
	@exit 1
build/fwpk.inc: rom/tools/make_pk_inc.py firmware/keys/pk.key
	python3 rom/tools/make_pk_inc.py firmware/keys/pk.key $@

# EBR ROM init data (build/ebrrom_{lo,hi}.mem, read by ebrrom.v at synthesis
# time), generated from the boot ROM binary (see tools/make_ebrrom_mem.py).
build/ebrrom_lo.mem build/ebrrom_hi.mem: tools/make_ebrrom_mem.py build/rom.bin
	python3 tools/make_ebrrom_mem.py build/ebrrom_lo.mem build/ebrrom_hi.mem build/rom.bin

.PHONY: prog_breakout prog_fw_breakout prog_flash_breakout som_image prog_flash_som firmware clean FORCE

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
	iceprog -o 128k $(FW_IMG)

prog_flash_breakout: $(buildpath)$(filename)_breakout.bin prog_fw_breakout
	iceprog $(buildpath)$(filename)_breakout.bin

som_image: $(buildpath)$(filename)_som.bin firmware build/libed25519.bin
	dd if=/dev/zero of=$(buildpath)$(filename)_som_image.bin bs=512k count=1  # create a blank 512 kB flash image
	dd if=$(buildpath)$(filename)_som.bin of=$(buildpath)$(filename)_som_image.bin conv=notrunc  # copy the bitstream to the beginning
	dd if=$(FW_IMG) of=$(buildpath)$(filename)_som_image.bin bs=1 seek=128k conv=notrunc  # copy the firmware beginning at 128 kB
	dd if=build/libed25519.bin of=$(buildpath)$(filename)_som_image.bin bs=1 seek=256k conv=notrunc  # copy fwsiglib at 256 kB

prog_flash_som: som_image
	python3 tools/hold_reset.py --port $(SOM_SERIAL) -- $(flashrom) -w $(buildpath)$(filename)_som_image.bin

ifeq ($(SIGN),1)
firmware:
	$(MAKE) --directory=firmware/ build/firmware_signed.bin
else
firmware:
	$(MAKE) --directory=firmware/
endif

clean:
	rm -rf $(buildpath)/*
	rm -rf firmware/build/*