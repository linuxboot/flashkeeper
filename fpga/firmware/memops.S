/*
 * Copyright 2023 (C) Alexander Vysokovskikh
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY,
 * OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT
 * OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
 * BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
 * OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN
 * IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#define HAVE_RISCV_MEMSET

#define HAVE_RISCV_MEMCPY
#define HAVE_RISCV_MEMCPY_UNROLL_LOOP
#define HAVE_RISCV_MEMCPY_UNALIGNED

#define HAVE_RISCV_MEMMOVE
#define HAVE_RISCV_MEMMOVE_UNROLL_LOOP
#define HAVE_RISCV_MEMMOVE_UNALIGNED

/* The following defines rename the exported labels at preprocess time
 * (this is a .S file, so the preprocessor runs over it): the object
 * actually exports memcpy/memset/memmove without a leading underscore,
 * which is what GCC emits for implicit calls (e.g. array `= {0}`
 * initializations) even under -ffreestanding. Do not "fix" the
 * #define'd names - the labels depend on them. See memops.h. */
#define _memcpy memcpy
#define _memset memset
#define _memmove memmove

#if __riscv_xlen == 32
# define REG_S          sw
# define REG_L          lw
# define SZREG          4
#elif __riscv_xlen == 64
# define REG_S          sd
# define REG_L          ld
# define SZREG          8
#else
# error unsupported __riscv_xlen
#endif

#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
# define SHL_H          srl
# define SHL_L          sll
#else
# define SHL_H          sll
# define SHL_L          srl
#endif

    /* code is optimized for in-order
    // multiple issue (superscalar) CPUs */

    /* we want to reduce the size of the code, so in order
    // to comply with the compressed ISA specification,
    // we will use x0-x15 registers. */

#ifdef HAVE_RISCV_MEMSET
.section .text._memset, "ax", @progbits
.globl _memset
.type _memset, @function
    /*
    // void *_memset(void *dst, int ch, size_t sz)
    //
    // Function fills the first sz bytes of the memory
    // area pointed to by dst with the constant byte ch.
    // Function returns a pointer to the memory area dst.
    // Uses stores operations of XLEN (register) size.
    // ---
    // Minimal supported ISA: rv32e
    // Clobbered registers: a1, a2, a3, a4, a5, t0
    // Code size (bytes): rv64ic: 178, rv64i: 280, rv32ic: 172, rv32i: 268
    */
.align 2
_memset:
    li    a4, 2*SZREG           // threshold for bytewise operations
    mv    a5, a0                // preserve a0 as return value, further a5 is 'dst'
    bltu  a2, a4, 4f            // jump to bytewise memset if 'sz' is less than threshold
    andi  a3, a0, SZREG-1       // a3 is dst misalignment
    andi  a1, a1, 255           // clamp 'ch' to one byte
    beqz  a3, 1f                // skip align of the headroom

    /* align the headroom */
    addi  a3, a3, -SZREG        // bytes to set: SZREG - misalignment, negative value
    add   a2, a2, a3            // decrement 'sz' for subsequent calculations
    sub   a3, a5, a3            // a3 is last address for the next loop
0:  sb    a1, 0(a5)             // store 'ch' to the address in 'dst'
    addi  a5, a5, 1             // increment 'dst'
    bltu  a5, a3, 0b            // repeat until the last address (a3) is reached

    /* propagate 'ch' (a1) to whole register
    // and calculate last address (a4) for the main loop */
1:  slli  a3, a1, 8             // tmp = a1 << 8
    add   a4, a5, a2            // a4 is last address for the main loop
    or    a1, a1, a3            // a1 = a1 | tmp (a1 << 8)
    andi  a4, a4, -SZREG        // round a4 (last address) to the register size
    slli  a3, a1, 16            // repeat... tmp = a1 << 16, a1 = a1 | tmp, ...
    or    a1, a1, a3
#if __riscv_xlen == 64
    slli  a3, a1, 32
    or    a1, a1, a3
#endif
    /* the main loop is an unrolled loop of 32 stores of the register size,
    // so we use the Duff's device to handle the non-integer amount of 32*SZREG */
    andi  a3, a2, 31*SZREG      // multiple register size remainder of 32*SZREG
    andi  a2, a2, SZREG-1       // decrement sz for subsequent calculations
    beqz  a3, 2f                // integer number of the main loop cycles
    /* calculate jump address (t0) for Duff's device: */
0:  auipc t0, %pcrel_hi(3f)     // (1), auipc(1) + addi(2) == lla t0, 3f
    add   a5, a5, a3            // calculate dst immediate offset compensation (a5)
    addi  t0, t0, %pcrel_lo(0b) // (2), now t0 is absolute address of label 3
    /* take in account how many bytes stored by one store instruction
    // (4/8 bytes of data per 2/4 byte instruction) */
#if __riscv_xlen == 64 && defined(__riscv_compressed)
    srli  a3, a3, 2             // 2-bytes 'c.sd' stores a 8 bytes => x4
#elif __riscv_xlen > 32 || defined(__riscv_compressed)
    srli  a3, a3, 1             // 4-bytes 'sd' stores a 8 bytes => x2
#endif
    sub   t0, t0, a3            // t0 is address of certain store instruction
    addi  a5, a5, -32*SZREG     // compensate store instructions immediate offset
    jr    t0                    // jump inside the main loop

    /* set 32*SZREG bytes per loop cycle */
2:  REG_S a1, 0*SZREG(a5)
    REG_S a1, 1*SZREG(a5)
    REG_S a1, 2*SZREG(a5)
    REG_S a1, 3*SZREG(a5)
    REG_S a1, 4*SZREG(a5)
    REG_S a1, 5*SZREG(a5)
    REG_S a1, 6*SZREG(a5)
    REG_S a1, 7*SZREG(a5)
    REG_S a1, 8*SZREG(a5)
    REG_S a1, 9*SZREG(a5)
    REG_S a1, 10*SZREG(a5)
    REG_S a1, 11*SZREG(a5)
    REG_S a1, 12*SZREG(a5)
    REG_S a1, 13*SZREG(a5)
    REG_S a1, 14*SZREG(a5)
    REG_S a1, 15*SZREG(a5)
    REG_S a1, 16*SZREG(a5)
    REG_S a1, 17*SZREG(a5)
    REG_S a1, 18*SZREG(a5)
    REG_S a1, 19*SZREG(a5)
    REG_S a1, 20*SZREG(a5)
    REG_S a1, 21*SZREG(a5)
    REG_S a1, 22*SZREG(a5)
    REG_S a1, 23*SZREG(a5)
    REG_S a1, 24*SZREG(a5)
    REG_S a1, 25*SZREG(a5)
    REG_S a1, 26*SZREG(a5)
    REG_S a1, 27*SZREG(a5)
    REG_S a1, 28*SZREG(a5)
    REG_S a1, 29*SZREG(a5)
    REG_S a1, 30*SZREG(a5)
    REG_S a1, 31*SZREG(a5)
3:  addi  a5, a5, 32*SZREG      // increment 'dst' by 32*SZREG
    bltu  a5, a4, 2b            // repeat until the last address is reached

    /* handle tail misalignment, bytewise memset */
4:  beqz  a2, 5f                // quit if 'sz' == 0
    add   a2, a2, a5            // a2 is last address now
0:  sb    a1, 0(a5)             // store 'ch' to the address in 'dst'
    addi  a5, a5, 1             // increment 'dst'
    bltu  a5, a2, 0b            // repeat until the last address is reached
5:  ret                         // return a0 (initial 'dst')
    .size _memset, . - _memset
#endif /* HAVE_RISCV_MEMSET */

#ifdef HAVE_RISCV_MEMCPY
.section .text._memcpy, "ax", @progbits
.globl _memcpy
.type _memcpy, @function
    /*
    // void *_memcpy(void *dst, void *src, size_t sz)
    //
    // Copies sz bytes from memory area src to memory area dst.
    // Function returns a pointer to dst.
    // The memory areas must not overlap. Uses load/stores of XLEN.
    // For mutual misaligned buffers does byte-by-byte coping
    // if HAVE_RISCV_MEMCPY_UNALIGNED is not defined.
    */
.align 2
_memcpy:
#ifdef HAVE_RISCV_MEMCPY_UNALIGNED
    li    a4, 3*SZREG           // threshold for byte-by-byte copying
#else
    li    a4, 2*SZREG
#endif
    mv    t2, a0                // save initial dst value
    bltu  a2, a4, .Lmemcpy_bops // jump to byte-by-byte copying if sz is less than threshold

    andi  a3, a0, SZREG-1       // if dst is aligned to register size...
    beqz  a3, 1f                // jump to the fast copying

    /* handle dst head misalignment with bytewise copying */
    addi  a3, a3, -SZREG        // a3 = -(SZREG - misaligment)
    add   a2, a2, a3            // size = size - a3
    sub   a3, a0, a3            // the last address = src + a3
0:  lbu   a5, 0(a1)             // load byte from src
    sb    a5, 0(a0)             // store byte to dst
    addi  a1, a1, 1             // increment src
    addi  a0, a0, 1             // increment dst
    bltu  a0, a3, 0b            // repeat until the last address is reached

    /* now dst (a0) is aligned to register size
    // but is source aligned to register size? */
1:  andi  t3, a1, SZREG-1       // t3 is src misaligment
#ifdef HAVE_RISCV_MEMCPY_UNALIGNED
    mv    t0, s0                // save clobbered registers
    mv    t1, s1                // regarding calling convention
    mv    a3, a0
    bnez  t3, .Lmemcpy_unaligned
#else
    /* dst and src are mutual misaligned,
       do the byte-by-byte copying */
    bnez  t3, .Lmemcpy_bops
    mv    t0, s0
    mv    t1, s1
    mv    a3, a0
#endif

#ifdef HAVE_RISCV_MEMCPY_UNROLL_LOOP
    li    a7, 16*SZREG
    bltu  a2, a7, 2f

    /* calculate last address, decrease size (a2) */
    add   a3, a3, a2
    andi  a2, a2, 16*SZREG-1    // decrement sz to later processing
    sub   a3, a3, a2            // a3 is last address for main loop

.align 3
    /* copy 16*SZREG per loop */
1:  REG_L a4, 0*SZREG(a1)
    REG_L a5, 1*SZREG(a1)
    REG_L s0, 2*SZREG(a1)
    REG_L s1, 3*SZREG(a1)
    REG_S a4, 0*SZREG(a0)
    REG_S a5, 1*SZREG(a0)
    REG_S s0, 2*SZREG(a0)
    REG_S s1, 3*SZREG(a0)
    REG_L a4, 4*SZREG(a1)
    REG_L a5, 5*SZREG(a1)
    REG_L s0, 6*SZREG(a1)
    REG_L s1, 7*SZREG(a1)
    REG_S a4, 4*SZREG(a0)
    REG_S a5, 5*SZREG(a0)
    REG_S s0, 6*SZREG(a0)
    REG_S s1, 7*SZREG(a0)
    REG_L a4, 8*SZREG(a1)
    REG_L a5, 9*SZREG(a1)
    REG_L s0, 10*SZREG(a1)
    REG_L s1, 11*SZREG(a1)
    REG_S a4, 8*SZREG(a0)
    REG_S a5, 9*SZREG(a0)
    REG_S s0, 10*SZREG(a0)
    REG_S s1, 11*SZREG(a0)
    REG_L a4, 12*SZREG(a1)
    REG_L a5, 13*SZREG(a1)
    REG_L s0, 14*SZREG(a1)
    REG_L s1, 15*SZREG(a1)
    REG_S a4, 12*SZREG(a0)
    REG_S a5, 13*SZREG(a0)
    REG_S s0, 14*SZREG(a0)
    REG_S s1, 15*SZREG(a0)
    add   a0, a0, a7
    add   a1, a1, a7
    bltu  a0, a3, 1b
#endif
    /* calculate last address, decrease size (a2) */
2:  add   a3, a3, a2
    andi  a3, a3, -SZREG
    beq   a3, a0, .Lmemcpy_bytes

    andi  a2, a2, SZREG-1
    /* copy SZREG per loop */
3:  REG_L a4, 0(a1)
    addi  a1, a1, SZREG
    REG_S a4, 0(a0)
    addi  a0, a0, SZREG
    bltu  a0, a3, 3b

.Lmemcpy_bytes:
#if defined(HAVE_RISCV_MEMCPY_UNROLL_LOOP) || defined(HAVE_RISCV_MEMCPY_UNALIGNED)
    /* restore s0, s1 */
    mv    s1, t1
    mv    s0, t0
#endif

.Lmemcpy_bops:
    beqz  a2, 1f
    /* finish off the rest by byte-by-byte copying */
    add   a2, a2, a0
0:  lbu   a4, 0(a1)
    addi  a1, a1, 1
    sb    a4, 0(a0)
    addi  a0, a0, 1
    bltu  a0, a2, 0b
    /* return initial a0 */
1:  mv    a0, t2
    ret
#ifdef HAVE_RISCV_MEMCPY_UNALIGNED
.Lmemcpy_unaligned:
    /* memcpy for mutual misaligned src and dst buffers
    // ---
    // at this point:
    // a0 (dst) is already aligned to register size
    // a1 (src) is not aligned
    // t3 is source misaligment (1..SZREG-1)
    // a2 (sz) is number bytes to copy are left */
    andi  a3, a2, -2*SZREG      // rounded to 2*SZREG sz value
    andi  a1, a1, -SZREG        // align source pointer to register size
    slli  a6, t3, 3             // a6 = misalignment bytes * 8 (bits per byte)
    andi  a2, a2, 2*SZREG-1     // a2 = remaining bytes to copy byte by byte
    REG_L a5, 0(a1)             // preload first dword/qword
    add   a3, a3, a0            // a3 = last address for the loop quit condition
    sub   a7, zero, a6          // a7 is opposite to a6 shift value
    /* uses load/stores of register size with logical
    // shifting of loaded values, combining them and then store
    // a6, a7 are shift value pair here
    // a5, a4 loaded dwords/qwords
    // s0, s1 are values to store */
    .align 3

#if 0 // 20 cycles
0:  REG_L a4, SZREG(a1)         // load [dq]word
    SHL_H s1, a5, a6            // s0 = logical shifted prev. loaded [dq]word
    addi  a1, a1, 2*SZREG       // increment src pointer
    SHL_L s0, a4, a7            // s1 = logical ashifted [dq]word in a4
    REG_L a5, -SZREG(a1)       // load next [dq]word
    or    s0, s0, s1            // combine previous shifted values to store value
    SHL_H s1, a4, a6            // 3rd logical shift
    REG_S s0, 0(a0)             // store s0 to dst
    SHL_L s0, a5, a7            // 4th logical shift
    addi  a0, a0, 2*SZREG       // increment dst pointer
    or    s1, s1, s0            // combine shifted values to s1
    REG_S s1, -SZREG(a0)         // store s1
    bltu  a0, a3, 0b            // ... while (dst < last)
#elif 0 // 16 cycles
//    REG_L a5, 0(a1)
0:  REG_L a4, SZREG(a1)
    SHL_H s1, a5, a6
    REG_L a5, 2*SZREG(a1)
    SHL_L s0, a4, a7
    or    s0, s0, s1
    SHL_H s1, a4, a6
    REG_S s0, 0(a0)
    SHL_L s0, a5, a7
    addi  a1, a1, 2*SZREG
    or    s1, s1, s0
    REG_S s1, SZREG(a0)
    addi  a0, a0, 2*SZREG
    bltu  a0, a3, 0b
#elif 0 // 14 cycles
    /* dual issue processor optimized */
//    REG_L a5, 0(a1)
    REG_L a4, SZREG(a1)
0:
    SHL_H s1, a5, a6
    REG_L a5, 2*SZREG(a1)
    SHL_L s0, a4, a7
    or    s0, s0, s1
    SHL_H s1, a4, a6
    REG_S s0, 0(a0)
    SHL_L s0, a5, a7
    addi  a1, a1, 2*SZREG
    or    s1, s1, s0
    REG_S s1, SZREG(a0)
    addi  a0, a0, 2*SZREG
    REG_L a4, SZREG(a1)
    bltu  a0, a3, 0b
#else   // 7 cycles on dual-issue
    REG_L a4, SZREG(a1)
0:
    SHL_H s1, a5, a6
    REG_L a5, 2*SZREG(a1)
    SHL_L s0, a4, a7
    or    s0, s0, s1
    SHL_H s1, a4, a6
    REG_S s0, 0(a0)
    SHL_L s0, a5, a7
    or    s1, s1, s0
    addi  a1, a1, 2*SZREG
    REG_S s1, SZREG(a0)
    addi  a0, a0, 2*SZREG
    REG_L a4, SZREG(a1)
    bltu  a0, a3, 0b
#endif

    add   a1, a1, t3            // return initial source misalignment
    j .Lmemcpy_bytes
#endif
    .size _memcpy, . - _memcpy

.globl _memcpy128, _memcpy64, _memcpy32, _memcpy24, _memcpy16, _memcpy8
.type _memcpy64, @function
_memcpy128:
    REG_L a2, 8*SZREG(a1)
    REG_L a3, 9*SZREG(a0)
    REG_L a4, 10*SZREG(a1)
    REG_L a5, 11*SZREG(a0)
    REG_S a2, 8*SZREG(a1)
    REG_S a3, 9*SZREG(a0)
    REG_S a4, 10*SZREG(a1)
    REG_S a5, 11*SZREG(a0)
    REG_L a2, 12*SZREG(a1)
    REG_L a3, 13*SZREG(a0)
    REG_L a4, 14*SZREG(a1)
    REG_L a5, 15*SZREG(a0)
    REG_S a2, 12*SZREG(a1)
    REG_S a3, 13*SZREG(a0)
    REG_S a4, 14*SZREG(a1)
    REG_S a5, 15*SZREG(a0)
_memcpy64:
    REG_L a2, 4*SZREG(a1)
    REG_L a3, 4*SZREG(a0)
    REG_L a4, 5*SZREG(a1)
    REG_L a5, 5*SZREG(a0)
    REG_S a2, 6*SZREG(a1)
    REG_S a3, 6*SZREG(a0)
    REG_S a4, 7*SZREG(a1)
    REG_S a5, 7*SZREG(a0)
_memcpy32:
#if __riscv_xlen == 32
    REG_L a5, 3*SZREG(a1)
    REG_S a5, 3*SZREG(a0)
#else
    REG_L a5, 3*SZREG(a1)
    REG_S a5, 3*SZREG(a0)
#endif
_memcpy24:
#if __riscv_xlen == 32
    REG_L a2, 4*SZREG(a1)
    REG_L a3, 5*SZREG(a1)
    REG_S a2, 4*SZREG(a0)
    REG_S a3, 5*SZREG(a0)
#else
    REG_L a4, 2*SZREG(a1)
    REG_S a4, 2*SZREG(a0)
#endif
_memcpy16:
#if __riscv_xlen == 32
    REG_L a2, 2*SZREG(a1)
    REG_L a3, 3*SZREG(a1)
    REG_S a2, 2*SZREG(a0)
    REG_S a3, 3*SZREG(a0)
#else
    REG_L a3, SZREG(a1)
    REG_S a3, SZREG(a0)
#endif
_memcpy8:
#if __riscv_xlen == 32
    REG_L a2, 0(a1)
    REG_L a3, 0(a1)
    REG_S a2, 1*SZREG(a0)
    REG_S a3, 1*SZREG(a0)
#else
    REG_L a2, 0(a1)
    REG_S a2, 0(a0)
#endif
    ret
    .size _memcpy8, . - _memcpy8
    .size _memcpy16, . - _memcpy16
    .size _memcpy24, . - _memcpy24
    .size _memcpy32, . - _memcpy32
    .size _memcpy64, . - _memcpy64
    .size _memcpy128, . - _memcpy128

#endif /* HAVE_RISCV_MEMCPY */

#ifdef HAVE_RISCV_MEMMOVE
.section .text._memmove, "ax", @progbits
.globl _memmove, _memcpy_r
.type _memcpy_r, @function
.type _memmove, @function
    /*
    // void *_memmove(void *dst, void *src, size_t sz)
    //
    // Function copies sz bytes from memory area src to memory area dst.
    // The memory areas may overlap.
    */
_memmove:
    addi a5, a5, 0
    addi a5, gp, 0
    /* use memcpy for straight order */
    bltu  a1, a0, _memcpy_r
    tail  _memcpy
_memcpy_r:
    /* threshold for byte-by-byte copying */
#ifdef HAVE_RISCV_MEMMOVE_UNALIGNED
    li    a4, 3*SZREG
#else
    li    a4, 2*SZREG
#endif
    /* reverse order: start from the end */
    add   a0, a0, a2
    add   a1, a1, a2
    bltu  a2, a4, .Lmemmove_bops

    /* is dest aligned to register size? */
    andi  a3, a0, SZREG-1
    beqz  a3, 1f

    /* handle dst head misalignment */
    sub   a2, a2, a3
    xor   a3, a3, a0
0:  addi  a1, a1, -1
    addi  a0, a0, -1
    lbu   a4, 0(a1)
    sb    a4, 0(a0)
    bgtu  a0, a3, 0b

    /* now dest (a0) is aligned to register size
    // but is source aligned to register size? */
1:  andi  t2, a1, SZREG-1
#ifdef HAVE_RISCV_MEMMOVE_UNALIGNED
    mv    t0, s0
    mv    t1, s1
    bnez  t2, .Lmemmove_unaligned
#else
    bnez  t2, .Lmemmove_bops
# ifdef HAVE_RISCV_MEMMOVE_UNROLL_LOOP
    mv    t0, s0
    mv    t1, s1
# endif
#endif
#ifdef HAVE_RISCV_MEMMOVE_UNROLL_LOOP
    li    a3, 16*SZREG
    bltu  a2, a3, 2f

    /* calculate last address, decrease size (a2) */
    sub   a7, a0, a2
    andi  a2, a2, 16*SZREG-1
    add   a7, a7, a2

    /* copy 16*SZREG per loop */
1:  sub   a1, a1, a3
    sub   a0, a0, a3

    REG_L a4, 12*SZREG(a1)
    REG_L a5, 13*SZREG(a1)
    REG_L s0, 14*SZREG(a1)
    REG_L s1, 15*SZREG(a1)

    REG_S a4, 12*SZREG(a0)
    REG_S a5, 13*SZREG(a0)
    REG_S s0, 14*SZREG(a0)
    REG_S s1, 15*SZREG(a0)

    REG_L a4, 8*SZREG(a1)
    REG_L a5, 9*SZREG(a1)
    REG_L s0, 10*SZREG(a1)
    REG_L s1, 11*SZREG(a1)

    REG_S a4, 8*SZREG(a0)
    REG_S a5, 9*SZREG(a0)
    REG_S s0, 10*SZREG(a0)
    REG_S s1, 11*SZREG(a0)

    REG_L a4, 4*SZREG(a1)
    REG_L a5, 5*SZREG(a1)
    REG_L s0, 6*SZREG(a1)
    REG_L s1, 7*SZREG(a1)

    REG_S a4, 4*SZREG(a0)
    REG_S a5, 5*SZREG(a0)
    REG_S s0, 6*SZREG(a0)
    REG_S s1, 7*SZREG(a0)

    REG_L a4, 0*SZREG(a1)
    REG_L a5, 1*SZREG(a1)
    REG_L s0, 2*SZREG(a1)
    REG_L s1, 3*SZREG(a1)

    REG_S a4, 0*SZREG(a0)
    REG_S a5, 1*SZREG(a0)
    REG_S s0, 2*SZREG(a0)
    REG_S s1, 3*SZREG(a0)

    bgtu  a0, a7, 1b
#endif
2:
    /* copy SZREG per loop */
    /* calculate last address, decrease size (a2) */
    sub   a3, a0, a2
    andi  a2, a2, SZREG-1
    add   a3, a3, a2
    beq   a0, a3, .Lmemmove_bytes
3:  addi  a0, a0, -SZREG
    addi  a1, a1, -SZREG
    REG_L a4, 0(a1)
    REG_S a4, 0(a0)
    bgtu  a0, a3, 3b

.Lmemmove_bytes:
#if defined(HAVE_RISCV_MEMMOVE_UNROLL_LOOP) || defined(HAVE_RISCV_MEMMOVE_UNALIGNED)
    /* restore s0, s1 */
    mv    s1, t1
    mv    s0, t0
#endif
    /* finish off the rest by byte-by-byte copying */
.Lmemmove_bops:
    beqz  a2, 1f
    sub   a3, a0, a2
0:  addi  a1, a1, -1
    addi  a0, a0, -1
    lbu   a4, 0(a1)
    sb    a4, 0(a0)
    bgtu  a0, a3, 0b
1:  ret

#ifdef HAVE_RISCV_MEMMOVE_UNALIGNED
.Lmemmove_unaligned:
    /* unaligned memcpy uses load/stores of SZREG size
    // with shifting copied data */

    /* a0 = (dest) is already aligned to register size
    // a1 = (source) is not aligned here
    // t2 = source's misaligned bytes (1..SZREG-1)
    // a2 = bytes to copy are left */
    /* align source to SZREG size */

    /* a6, a7 are shift values */
    slli  a6, t2, 3
    sub   a3, a0, a2
    andi  a1, a1, -SZREG
    andi  a2, a2, 2*SZREG-1
    sub   a7, zero, a6
    REG_L a5, 0(a1)
    addi  a1, a1, -2*SZREG
    add   a3, a3, a2            // last address

.align 3
0:  REG_L a4, SZREG(a1)         // load 1
    SHL_L s0, a5, a7
    REG_L a5, 0(a1)             // load 2
    addi  a0, a0, -2*SZREG
    SHL_H s1, a4, a6
    or    s0, s0, s1
    SHL_L s1, a4, a7
    REG_S s0, SZREG(a0)         // store 1 shifted
    SHL_H s0, a5, a6
    addi  a1, a1, -2*SZREG      // decrement src (X)
    or    s1, s1, s0
    REG_S s1, 0(a0)             // store 2 shifted
    bgtu  a0, a3, 0b            // repeat until the last address is reached

    addi  a1, a1, 2*SZREG       // remove the decrement that was there before (X)
    add   a1, a1, t2            // return initial source misalignment
    j .Lmemmove_bytes
#endif
    .size _memmove, . - _memmove
    .size _memcpy_r, . - _memcpy_r
#endif /* HAVE_RISCV_MEMMOVE */