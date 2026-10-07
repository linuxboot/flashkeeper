/*
 * romapi.h - the firmware's call-in API to the boot ROM.
 *
 * The Boot ROM must already include TweetNaCl's SHA-512 implementation,
 * because it uses it to verify the fwsiglib binary. To avoid needing a
 * separate copy of this code in the firmware, this file provides an API
 * to call boot ROM code from the firmware.
 *
 * These functions are tweetnacl's crypto_hash_sha512 /
 * crypto_hashblocks_sha512, under explicit fk_rom_ names. tweetnacl.h
 * macro-maps the plain names onto tweetnacl.c's local _tweet symbols,
 * so this avoids name collisions if both tweetnacl.h and the romapi.h
 * are used.
 */
#ifndef ROMAPI_H
#define ROMAPI_H

#include <stdint.h>

int fk_rom_hash_sha512(uint8_t *out, const uint8_t *in, uint64_t inlen);
int fk_rom_hashblocks_sha512(uint64_t *state, const uint8_t *in,
                             uint64_t inlen);

#endif
