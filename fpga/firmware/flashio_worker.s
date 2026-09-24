/*
 * flashio_worker.s - SPI bit-bang worker.
 *
 * Called directly by flashio_call() in picosoc.c. In upstream
 * PicoSoC, the SPI worker gets copied onto the stack and executed
 * there, because when the firmware is XIP from CSPI flash, fetching
 * instructions from flash while bit-banging that same flash is a
 * recipe for disaster. We now execute from SPRAM, so this copy
 * at runtime is unnecessary.
 *
 * On entry, the worker puts the SPI flash into manual mode
 * (config_en = 0) and leaves it there - manual mode is "sticky".
 * Entering MEMIO (memory-mapped) mode again is the caller's explicit
 * job (spi_memio_enable()). Unless actively bitbanged, spimemio FSM
 * is electrically quiet in manual mode (held in softreset: CS high,
 * SCK low, outputs off), so holding manual mode between worker calls
 * does nothing to the bus.
 *
 * a0 ... data pointer
 * a1 ... data length
 * a2 ... optional WREN cmd (0 = disable)
 * a3 ... SPI control register address
 * a4 ... 32-bit prebuffer (0 = disable)
 */

.section .text

.global flashio_worker

.balign 4

flashio_worker:
# Set CS high, IO0 is output
li   t1, 0x120
sh   t1, 0(a3)

# Assert Manual SPI Ctrl (sticky - caller re-enables MEMIO explicitly)
sb   zero, 3(a3)

# Send optional WREN cmd
beqz a2, flashio_worker_L1
li   t5, 8
andi t2, a2, 0xff
flashio_worker_L4:
srli t4, t2, 7
sb   t4, 0(a3)
ori  t4, t4, 0x10
sb   t4, 0(a3)
slli t2, t2, 1
andi t2, t2, 0xff
addi t5, t5, -1
bnez t5, flashio_worker_L4
sb   t1, 0(a3)

# If the prebuffer in a4 is not zero, transmit the 32 bits in the prebuffer
# before transmitting the remaining 8*a1 bits in the a1-long buffer at a0,
# but do so in the same low CS pulse as the remainder of the buffer (not in
# the one the WREN command may have generated first). The prebuffer is used
# for the command and address in a write operation - together these are 32
# bits. When it is used, the buffer at a0 will be the payload only. The
# prebuffer is a single 32-bit value in a single register, or it is 0
# signifying not to send a prebuffer before the buffer array.

# Prebuffer transmission (if a4 != 0)
# CS will be pulled low here and stay low through the data transfer
flashio_worker_L1:
beqz a4, flashio_worker_L1b
# Pull CS low, IO0 output enabled
li   t0, 0x10
sb   t0, 0(a3)
# Transmit 32 bits from a4, MSB first
li   t5, 32
flashio_worker_L4b:
srli t4, a4, 31
sb   t4, 0(a3)
ori  t4, t4, 0x10
sb   t4, 0(a3)
slli a4, a4, 1
addi t5, t5, -1
bnez t5, flashio_worker_L4b
j    flashio_worker_L2

flashio_worker_L1b:
# For data transfer without prebuffer, Pull CS low, IO0 output enabled
li   t0, 0x10
sb   t0, 0(a3)

# SPI transfer
flashio_worker_L2:
beqz a1, flashio_worker_L3
li   t5, 8
lbu  t2, 0(a0)
flashio_worker_L2b:
srli t4, t2, 7
sb   t4, 0(a3)
ori  t4, t4, 0x10
sb   t4, 0(a3)
lbu  t4, 0(a3)
andi t4, t4, 2
srli t4, t4, 1
slli t2, t2, 1
or   t2, t2, t4
andi t2, t2, 0xff
addi t5, t5, -1
bnez t5, flashio_worker_L2b
sb   t2, 0(a0)
addi a0, a0, 1
addi a1, a1, -1
j    flashio_worker_L2

flashio_worker_L3:
# Pull CS high
sb   t1, 0(a3)

# config_en is not reset and remains 0. Further flashio calls can be made
# without returning to memio mode in between, or you can spi_memio_enable().

ret

.balign 4
