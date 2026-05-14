#!/bin/bash
# Run this to disassemble a built firmware binary and inspect the generated assembly code.

cd "$(dirname "$0")"
riscv64-unknown-elf-objdump --disassemble ../build/firmware.elf 