/* Build stub replacing sodium's private/common.h for the freestanding
 * target.
 *
 * CRYPTO_ALIGN is unused on this platform (there is no alignment requirement
 * for data in SPRAM).
 *
 * COMPILER_ASSERT is used to check the size of a table in ed25519_ref10.c
 * at build-time - _Static_assert can do this since C11 */
#ifndef CH
#define CH
#define CRYPTO_ALIGN(n)
#define COMPILER_ASSERT _Static_assert
#endif
