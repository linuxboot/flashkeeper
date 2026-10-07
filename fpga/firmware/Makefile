# RISC-V configuration
RISCV_EXTENSIONS = izmmul

# Compiler Warning Options
GCC_WARNS  = -Wall -Wextra -Wshadow -Wundef -Wpointer-arith -Wcast-qual -Wcast-align -Wwrite-strings
GCC_WARNS += -Wredundant-decls -pedantic -Wconversion # -Wstrict-prototypes -Wmissing-prototypes -Werror

# Environment setup
PYTHON = python3
HOSTCC = gcc
TOOLCHAIN_PREFIX = riscv64-unknown-elf-

# Firmware objects.
# tweetnacl.c is compiled separately, with separate flags.
FIRMWARE_OBJS = build/flashio_worker.o build/entry.o build/memops.o build/romcalls.o
FIRMWARE_OBJS += $(addprefix build/,$(patsubst %.c,%.o,$(wildcard *.c)))

# Determine the directory libgcc will come from for ilp32 rv32izmmul
LIBGCC_DIR := $(dir $(shell riscv64-unknown-elf-gcc -mabi=ilp32 -march=rv32izmmul -print-libgcc-file-name))
LDFLAGS += -L$(LIBGCC_DIR)

build/firmware.hex: build/firmware.bin tools/makehex.py
	$(PYTHON) tools/makehex.py $< 8192 > $@

build/firmware_signed.hex: build/firmware_signed.bin tools/makehex.py
	$(PYTHON) tools/makehex.py $< 8192 > $@

build/firmware.bin: build/firmware.elf tools/mkflashimg.py
	$(PYTHON) tools/mkflashimg.py $< $@
	chmod -x $@

build/firmware_signed.bin: build/firmware.bin tools/sign keys/sk.key
	tools/sign build/firmware.bin build/firmware_signed.bin keys/sk.key
	chmod -x $@

# Error when the signed image is requested but no keypair exists
# yet (rule with only a recipe, so it only runs when the key is missing).
keys/sk.key:
	@echo "error: keys/sk.key not found - generate a keypair first:" >&2
	@echo "       make tools/genkey && ./tools/genkey keys/pk.key keys/sk.key" >&2
	@exit 1

# Error when the public key is missing (see the keys/sk.key rule above).
keys/pk.key:
	@echo "error: keys/pk.key not found - generate a keypair first:" >&2
	@echo "       make tools/genkey && ./tools/genkey keys/pk.key keys/sk.key" >&2
	@exit 1

check: build/firmware_signed.bin tools/vtest keys/pk.key
	tools/vtest build/firmware_signed.bin keys/pk.key

# The linker script is preprocessed (cc -E -P -x c): ld's own preprocessor
# does not support #define (see the note in support/sections.lds).
build/sections.lds.pp: support/sections.lds ../memmap.h
	cc -E -P -x c support/sections.lds > $@

# --no-warn-rwx-segments: the image is intentionally one flat RWX segment
# (LMA = VMA + the flash image offset, see support/sections.lds and memmap.h).
build/firmware.elf: $(FIRMWARE_OBJS) build/sections.lds.pp
	$(TOOLCHAIN_PREFIX)gcc -Os -mabi=ilp32 -march=rv32$(RISCV_EXTENSIONS) -ffreestanding -nostdlib -o $@ -Wl,-Bstatic,-T,build/sections.lds.pp,-Map,build/firmware.map,--strip-debug,--no-warn-rwx-segments,--gc-sections $(FIRMWARE_OBJS) $(LDFLAGS) -lgcc 
	chmod -x $@

build/flashio_worker.o: flashio_worker.s
	$(TOOLCHAIN_PREFIX)gcc -c -mabi=ilp32 -march=rv32$(RISCV_EXTENSIONS) -o $@ $<

build/entry.o: entry.s
	$(TOOLCHAIN_PREFIX)gcc -c -mabi=ilp32 -march=rv32$(RISCV_EXTENSIONS) -o $@ $<

build/memops.o: memops.S
	$(TOOLCHAIN_PREFIX)gcc -c -mabi=ilp32 -march=rv32$(RISCV_EXTENSIONS) -o $@ $<

# tweetnacl.c: similar flags to firmware, minus -Wconversion (NaCl-style macros generate a lot), plus
# section granularity so --gc-sections at the link keeps only the functions the firmware references.
build/tweetnacl.o: tweetnacl.c tweetnacl.h
	$(TOOLCHAIN_PREFIX)gcc -c -mabi=ilp32 -march=rv32$(RISCV_EXTENSIONS) -Os --std=c99 -Wall -Wextra -Wno-sign-compare -ffreestanding -nostdlib -ffunction-sections -fdata-sections -o $@ $<

# Trampolines into the ROM (see romcalls.s): the EBR addresses are generated from the
# linked ROM (../build/rom.elf) by ../rom/tools/make_romapi.py and injected as -D flags
build/romcalls.o: romcalls.S romapi.h ../build/rom.elf ../rom/tools/make_romapi.py
	RDEFS=$$(python3 ../rom/tools/make_romapi.py ../build/rom.elf --print-defs) || exit 1; \
	test -n "$$RDEFS" || { echo "error: make_romapi.py produced no output" >&2; exit 1; }; \
	$(TOOLCHAIN_PREFIX)gcc -c $$RDEFS -mabi=ilp32 -march=rv32$(RISCV_EXTENSIONS) -o $@ $<

build/%.o: %.c *.h
	$(TOOLCHAIN_PREFIX)gcc -c -mabi=ilp32 -march=rv32$(RISCV_EXTENSIONS) -Os --std=c99 $(GCC_WARNS) -ffreestanding -nostdlib -o $@ $< 

# Firmware image signing tool
tools/sign: tools/sign.c tools/randombytes.c tweetnacl.c tweetnacl.h ../memmap.h
	$(HOSTCC) tools/sign.c tools/randombytes.c tweetnacl.c -o tools/sign

# Firmware signing keypair generation tool
tools/genkey: tools/genkey.c tools/randombytes.c tweetnacl.c tweetnacl.h
	$(HOSTCC) tools/genkey.c tools/randombytes.c tweetnacl.c -o tools/genkey

# Image verification test: verifies a signed image to ensure the Boot ROM will accept it. Run by `make check`.
tools/vtest: tools/vtest.c tools/randombytes.c tweetnacl.c tweetnacl.h ../memmap.h
	cc -O2 -Wall -Wextra -Wno-sign-compare -o $@ tools/vtest.c tools/randombytes.c tweetnacl.c

clean_fw:
	rm build/*
