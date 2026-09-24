#include "rng.h"
#include "tweetnacl.h"

/*
 * rng.c - TRNG driver: raw-bit collection, NIST SP 800-90B health
 * tests, SHA-512 extraction. See rng.h for more information. See
 * fpga/rng.v for the entropy source.
 */

// Raw bit collection (samples one bit per register read)
static inline uint8_t rng_read_bit(void){
    return (uint8_t)((*(volatile uint32_t *)RNG_DATA) & 1u);
}

static uint8_t raw_batch[RNG_BATCH_BYTES];

// 4096 reads; each read returns a fresh raw bit (the register shifts
// in the next sample on the read).
static void rng_collect_batch(void){
    for (uint32_t b = 0; b < RNG_BATCH_BYTES; b++) {
        uint8_t x = 0;
        for (uint32_t k = 0; k < 8; k++)
            x |= (uint8_t)(rng_read_bit() << k);
        raw_batch[b] = x;
    }
}

// ---------------------------------------------------------------------
// Health tests (NIST SP 800-90B RCT + APT)
// ---------------------------------------------------------------------

static uint8_t  h_last;          // 0xFF = reset sentinel
static uint32_t h_run, h_rct_max;
static uint8_t  h_apt_a;
static uint32_t h_apt_count, h_apt_pos, h_apt_max;
static uint32_t h_bits_seen;

// Lifetime statistics (survive re-arms)
static uint32_t s_bits_seen, s_batches_ok, s_batches_fail;
static uint32_t s_rct_max, s_apt_max;

static void health_reset(void){
    h_last      = 0xFF;          // impossible value, first bit starts a run
    h_run       = 0;
    h_rct_max   = 0;
    h_apt_a     = 0;
    h_apt_count = 0;
    h_apt_pos   = RNG_APT_WINDOW; // force a fresh window on the first bit
    h_apt_max   = 0;
    h_bits_seen = 0;
}

static void health_reset_init(void){
    health_reset();
    s_bits_seen  = 0;
    s_batches_ok = 0;
    s_batches_fail = 0;
    s_rct_max    = 0;
    s_apt_max    = 0;
}

// Health-check one raw entropy batch. Returns 1 if the batch is valid (no 
// failure, and fully past warmup), 0 otherwise.
static int health_run_batch(void){
    uint32_t fed = 0;
    uint32_t batch_start = h_bits_seen;
    int failed = 0;

    for (uint32_t b = 0; b < RNG_BATCH_BYTES && !failed; b++) {
        for (uint32_t k = 0; k < 8 && !failed; k++) {
            uint8_t bit = (raw_batch[b] >> k) & 1u;

            // Repeated-count test
            if (bit == h_last)
                h_run++;
            else {
                h_last = bit;
                h_run  = 1;
            }
            if (h_run > h_rct_max)
                h_rct_max = h_run;
            if (h_run >= RNG_RCT_CUTOFF) {
                failed = 1;
                break;
            }

            // Adaptive proportions test
            if (h_apt_pos >= RNG_APT_WINDOW) {
                if (h_apt_count > h_apt_max)
                    h_apt_max = h_apt_count;
                h_apt_a     = bit;   // new window: reference = this bit
                h_apt_count = 1;
                h_apt_pos   = 1;
            } else {
                if (bit == h_apt_a)
                    h_apt_count++;
                h_apt_pos++;
                if (h_apt_count >= RNG_APT_CUTOFF) {
                    failed = 1;
                    break;
                }
            }

            h_bits_seen++;
            fed++;
        }
    }

    s_bits_seen += fed;
    if (h_rct_max > s_rct_max) s_rct_max = h_rct_max;
    if (h_apt_max > s_apt_max) s_apt_max = h_apt_max;

    if (failed) {
        s_batches_fail++;
        health_reset();            // re-arm: next batch starts warmup
        return 0;
    }
    if (batch_start < RNG_WARMUP_BITS)
        return 0;                  // overlapped warmup: discard conservatively
    s_batches_ok++;
    return 1;
}

// ---------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------

static int rng_initialized = 0;

// Blocking: fill dst with n conditioned random bytes. May take many
// milliseconds (each 64 bytes needs one healthy 4096-bit batch).
void randombytes(void *dst, size_t n){
    if (!rng_initialized) {
        health_reset_init();
        rng_initialized = 1;
    }
    uint8_t *d = (uint8_t *)dst;

    while (n > 0) {
        rng_collect_batch();
        if (health_run_batch()) {
            // Whitening: 512 raw bytes -> 64 conditioned bytes
            crypto_hash_sha512(d, raw_batch, RNG_BATCH_BYTES);
            size_t take = (n < RNG_COND_BYTES) ? n : RNG_COND_BYTES;
            d += take;
            n -= take;
        }
    }
}

void rng_get_stats(rng_stats_t *st){
    st->bits_seen    = s_bits_seen;
    st->batches_ok   = s_batches_ok;
    st->batches_fail = s_batches_fail;
    st->rct_max      = s_rct_max;
    st->apt_max      = s_apt_max;
}
