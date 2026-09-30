// Build stub replacing libsodium's utils.c. The only use of this file from
// the ed25519 code we use is ed25519_ref10_fe_25_5.h's call to sodium_is_zero.
// This is the same constant-time implementation from libsodium's utils.c

#ifndef UH
#define UH
#include <stddef.h>
static int sodium_is_zero(const unsigned char *n, const size_t nlen)
{
    size_t                 i;
    volatile unsigned char d = 0U;

    for (i = 0U; i < nlen; i++) {
        d |= n[i];
    }
    return 1 & ((d - 1) >> 8);
}
#endif
