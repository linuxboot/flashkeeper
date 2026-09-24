#ifndef _MEMOPS_H_
#define _MEMOPS_H_

/*
 * Minimal declarations for the memory operations implemented in memops.S.
 *
 * The firmware is built with -ffreestanding -nostdlib, but GCC emits calls
 * to these functions on its own (e.g. for zero-initializing local arrays
 * with `= {0}`), so memops.S provides these implementations. This header
 * allows calls to these from C without requiring external (system) headers.
 *
 * Note: memops.S `#define _memcpy memcpy` etc. rename the exported labels
 * at preprocess time, so the object actually exports memcpy / memset /
 * memmove (without a leading underscore).
 */

extern void *memcpy(void *dst, const void *src, unsigned int n);
extern void *memset(void *s, int c, unsigned int n);

/* memmove is also exported by memops.S (for GCC's implicit calls, which
 * ignore its return value), but is intentionally not declared here: its
 * backwards-copy path returns a clobbered destination pointer. */

#endif
