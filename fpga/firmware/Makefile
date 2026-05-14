# RISC-V configuration
RISCV_EXTENSIONS = im

# Compiler Warning Options
GCC_WARNS  = -Wall -Wextra -Wshadow -Wundef -Wpointer-arith -Wcast-qual -Wcast-align -Wwrite-strings
GCC_WARNS += -Wredundant-decls -pedantic -Wconversion # -Wstrict-prototypes -Wmissing-prototypes -Werror

# Environment setup
PYTHON = python3
TOOLCHAIN_PREFIX = riscv64-unknown-elf-

# Firmware objects
FIRMWARE_OBJS = build/start.o
FIRMWARE_OBJS += $(addprefix build/,$(patsubst %.c,%.o,$(wildcard *.c)))

build/firmware.hex: build/firmware.bin tools/makehex.py
	$(PYTHON) tools/makehex.py $< 2048 > $@

build/firmware.bin: build/firmware.elf
	$(TOOLCHAIN_PREFIX)objcopy -O binary $< $@
	chmod -x $@

build/firmware.elf: $(FIRMWARE_OBJS) support/sections.lds
	$(TOOLCHAIN_PREFIX)gcc -Os -mabi=ilp32 -march=rv32$(RISCV_EXTENSIONS) -ffreestanding -nostdlib -o $@ -Wl,-Bstatic,-T,support/sections.lds,-Map,build/firmware.map,--strip-debug $(FIRMWARE_OBJS) -lgcc
	chmod -x $@

build/start.o: start.s
	$(TOOLCHAIN_PREFIX)gcc -c -mabi=ilp32 -march=rv32$(RISCV_EXTENSIONS) -o $@ $<

build/%.o: %.c
	$(TOOLCHAIN_PREFIX)gcc -c -mabi=ilp32 -march=rv32$(RISCV_EXTENSIONS) -Os --std=c99 $(GCC_WARNS) -ffreestanding -nostdlib -o $@ $< 

clean_fw:
	rm build/*

prog_fw:
	iceprog -o 1M build/firmware.bin