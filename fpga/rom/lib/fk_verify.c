/* fk_verify.c - entry point for the fwsiglib ed25519 library
 *
 * This is a direct adaptation of _crypto_sign_ed25519_verify_detached
 * from libsodium 1.0.22. ed25519 curve operations are implemented in
 * ed25519_ref10.c, which is unmodified. In order,
 *
 *     sc25519_is_canonical(s)                  reject non-canonical S
 *     ge25519_has_small_order(r)               reject small-order R
 *     ge25519_is_canonical(pk)                 reject non-canonical A
 *     ge25519_has_small_order(pk)              reject small-order A
 *     ge25519_frombytes_negate_vartime(&A, pk) decode A, negate it
 *     sc25519_reduce(h)                        h = h mod L
 *     ge25519_double_scalarmult_vartime        R' = [h](-A) + [S]B
 *     ge25519_tobytes(rout)                    canonical pack of R'
 *
 * There are two differences between this function and the upstream
 * _crypto_sign_ed25519_verify_detached, both due to how fwsiglib is
 * is used by the Flashkeeper Boot ROM:
 *
 *   - the hash h = SHA-512(R || A || M) is computed by the ROM code 
 *     (see rom/boot.c), rather than this function. SHA-512 is provided
 *     by TweetNaCl in the boot ROM (where it has already been used to
 *     verify a hash of this library before calling into it). This
 *     function therefore takes a precomputed h, rather than providing
 *     its own SHA-512.
 *
 *   - the final compare R' == R is also done by the ROM, using
 *     TweetNaCl's crypto_verify_32. Since 931ce38, upstream libsodium
 *     checks this using BOTH crypto_verify_32 and sodium_memcmp, and
 *     also confirms sig != rcheck (i.e. not pointer aliasing). The
 *     latter case is not possible in our Boot ROM (rout is on the ROM's
 *     stack at the top of SPRAM, sig is in the image well below LIB_BASE).
 *     Upstream's second compare is apparently intended as redundancy
 *     against any undetected bug in crypto_verify_32, since libsodium
 *     already has both crypto_verify_32 and sodium_memcmp available.
 *     Our Boot ROM does not have sodium_memcmp, and already relies on 
 *     TweetNaCl's crypto_verify_* and vn before calling this library.
 *     Given this, the code size of including a redundant comparison
 *     implementation is likely not worthwhile here.
 */
#include "private/ed25519_ref10.h"

/*
 * fk_lib_verify computes the ed25519 verification point:
 *
 *     R' = [h](-A) + [S]B
 *
 * from parts the boot ROM has already at hand:
 *
 *   h    - SHA-512(R || A || M) as computed by the ROM (64 bytes,
 *          scratch: reduced in place below
 *   pk   - the public key A (32 bytes, canonical encoding)
 *   r    - the signature's R part (32 bytes, canonical encoding)
 *   s    - the signature's S part (32 bytes)
 *   rout - output: R' in canonical byte form (32 bytes)
 *
 * Returns 0 on success (rout holds R'), -1 if any pre-check or the
 * decode fails.
 */
int fk_lib_verify(const unsigned char h[64],
                  const unsigned char pk[32],
                  const unsigned char r[32],
                  const unsigned char s[32],
                  unsigned char rout[32])
{
    ge25519_p3 A;
    ge25519_p2 R;
    unsigned char hh[64];
    int i;

    for (i = 0; i < 64; i++) hh[i] = h[i];

    if (sc25519_is_canonical(s) == 0) return -1;
    if (ge25519_has_small_order(r) != 0) return -1;
    if (ge25519_is_canonical(pk) == 0) return -1;
    if (ge25519_has_small_order(pk) != 0) return -1;
    if (ge25519_frombytes_negate_vartime(&A, pk) != 0) return -1;

    sc25519_reduce(hh); /* h = h mod L (destroys hh) */
    ge25519_double_scalarmult_vartime(&R, hh, &A, s);
    ge25519_tobytes(rout, &R);
    return 0;
}
