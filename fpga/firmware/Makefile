# RISC-V configuration
RISCV_EXTENSIONS = izmmul

# Compiler Warning Options
GCC_WARNS  = -Wall -Wextra -Wshadow -Wundef -Wpointer-arith -Wcast-qual -Wcast-align -Wwrite-strings
GCC_WARNS += -Wredundant-decls -pedantic -Wconversion # -Wstrict-prototypes -Wmissing-prototypes -Werror

# Environment setup
PYTHON = python3
TOOLCHAIN_PREFIX = riscv64-unknown-elf-

# Firmware objects
FIRMWARE_OBJS = build/flashio_worker.o build/entry.o build/memops.o
FIRMWARE_OBJS += $(addprefix build/,$(patsubst %.c,%.o,$(wildcard *.c)))

# Determine the directory libgcc will come from for ilp32 rv32izmmul
LIBGCC_DIR := $(dir $(shell riscv64-unknown-elf-gcc -mabi=ilp32 -march=rv32izmmul -print-libgcc-file-name))
LDFLAGS += -L$(LIBGCC_DIR)

build/firmware.hex: build/firmware.bin tools/makehex.py
	$(PYTHON) tools/makehex.py $< 8192 > $@

build/firmware.bin: build/firmware.elf tools/mkflashimg.py
	$(PYTHON) tools/mkflashimg.py $< $@
	chmod -x $@

build/firmware.elf: $(FIRMWARE_OBJS) support/sections.lds
	# --no-warn-rwx-segments: the image is intentionally one flat RWX segment
	# (LMA = VMA + 0x20000, copied as a single block by the boot ROM; there is
	# no OS to enforce segment permissions - see support/sections.lds).
	$(TOOLCHAIN_PREFIX)gcc -Os -mabi=ilp32 -march=rv32$(RISCV_EXTENSIONS) -ffreestanding -nostdlib -o $@ -Wl,-Bstatic,-T,support/sections.lds,-Map,build/firmware.map,--strip-debug,--no-warn-rwx-segments $(FIRMWARE_OBJS) $(LDFLAGS) -lgcc 
	chmod -x $@

build/flashio_worker.o: flashio_worker.s
	$(TOOLCHAIN_PREFIX)gcc -c -mabi=ilp32 -march=rv32$(RISCV_EXTENSIONS) -o $@ $<

build/entry.o: entry.s
	$(TOOLCHAIN_PREFIX)gcc -c -mabi=ilp32 -march=rv32$(RISCV_EXTENSIONS) -o $@ $<

build/memops.o: memops.S
	$(TOOLCHAIN_PREFIX)gcc -c -mabi=ilp32 -march=rv32$(RISCV_EXTENSIONS) -o $@ $<

# tweetnacl.c is vendored upstream (public domain) and is subtle crypto code -
# don't edit it. Its macro-heavy code trips -Wconversion/-Wsign-compare with
# harmless int/uint conversions, so build it without those flags while keeping
# the rest of the sanity warnings.
build/tweetnacl.o: tweetnacl.c tweetnacl.h
	$(TOOLCHAIN_PREFIX)gcc -c -mabi=ilp32 -march=rv32$(RISCV_EXTENSIONS) -Os --std=c99 $(filter-out -Wconversion,$(GCC_WARNS)) -Wno-sign-compare -ffreestanding -nostdlib -o $@ $<

build/%.o: %.c *.h
	$(TOOLCHAIN_PREFIX)gcc -c -mabi=ilp32 -march=rv32$(RISCV_EXTENSIONS) -Os --std=c99 $(GCC_WARNS) -ffreestanding -nostdlib -o $@ $< 

clean_fw:
	rm build/*
