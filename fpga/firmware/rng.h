#ifndef RNG_H
#define RNG_H

#include <stddef.h>
#include <stdint.h>

/*
 * rng.h - TRNG driver.
 *
 * Per 4096-bit raw batch this driver:
 *   1. runs the NIST SP 800-90B RCT and APT continuous health tests
 *      (Repeated-Count Test, cutoff 32; Adaptive Proportions Test,
 *      1024-bit window, cutoff 820; 1024-bit warmup). These tests are
 *      simple runtime checks primarily intended to detect an outright-
 *      failing entropy source, and cannot detect every possible entropy
 *      source failure mode. For cryptographic randomness quality
 *      evaluation, they are not a replacement for a full randomness
 *      test suite.
 *   2. On a healthy batch fully past warmup, extracts 512 raw entropy
 *      bytes using SHA-512 (tweetnacl) into 64 output bytes.
 *   3. On a failed batch, discards it, re-arms the warmup, and counts
 *      the failure (auto-recovery).
 */

#define RNG_DATA      0x07000200u  // read: bit0 = raw bit (read-to-shift)

#define RNG_BATCH_BITS    4096
#define RNG_BATCH_BYTES   512
#define RNG_COND_BYTES    64

// Health-test cutoffs (NIST SP 800-90B / cryptoliteICE)
#define RNG_RCT_CUTOFF    32u
#define RNG_APT_WINDOW    1024u
#define RNG_APT_CUTOFF    820u
#define RNG_WARMUP_BITS   1024u

typedef struct {
    uint32_t bits_seen;    // raw bits consumed by health tests (lifetime)
    uint32_t batches_ok;   // healthy batches whitened (lifetime)
    uint32_t batches_fail; // batches discarded by health tests (lifetime)
    uint32_t rct_max;      // longest identical-bit run ever seen
    uint32_t apt_max;      // worst APT window count ever seen
} rng_stats_t;

// Lifetime statistics (for the `N rng` EDE word and the host-side
// test suite).
void rng_get_stats(rng_stats_t *st);

#endif
