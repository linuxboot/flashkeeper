/*
 * vtest - host-side verification test for signed Flashkeeper firmware images.
 *
 * Verifies a signed firmware image as the Boot ROM will (rom/boot.c): pk check,
 * SHA-512(payload), build sig||prefix||digest message, crypto_sign_open. Also
 * tests cases which should fail: wrong signer, tampered payload, tampered size
 * field, tampered signature, unsigned type.
 *
 * Usage: vtest <signed_image.bin> <pk.key>
 * Build: make tools/vtest   (in fpga/firmware/)
 */
#include "../../memmap.h"
#include "../tweetnacl.h"
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define IMG_MAGIC 0x57464B46u
#define IMG_TYPE_SIGNED_ED25519 1u
#define HDR_TYPE 0x08
#define HDR_PK   0xA0
#define HDR_SIG  0xC0
#define IMG_PAYLOAD 0x100

// Refuse unsigned images, as the ROM (by default) does
static int rom_check_unsigned(const uint8_t *img){
	uint32_t type;
	memcpy(&type, img + HDR_TYPE, 4);
	return (type == IMG_TYPE_SIGNED_ED25519) ? 0 : 1; // 0 = proceed to verify
}

/* Should match ROM boot_main() from the pk check onward. The ROM reads the
   size from the (so-far-unverified) header and hashes that range. */
static int rom_verify(const uint8_t *img, const uint8_t *pk){
	uint32_t size;
	memcpy(&size, img + 4, 4);

	uint8_t digest[crypto_hash_BYTES];
	uint8_t sm[crypto_sign_BYTES + HDR_SIG + crypto_hash_BYTES];
	uint8_t m[sizeof(sm)];
	unsigned long long mlen;

	if (crypto_verify_32(img + HDR_PK, pk)) return 1;

	crypto_hash(digest, img + IMG_PAYLOAD, size - IMG_PAYLOAD);

	memcpy(sm, img + HDR_SIG, crypto_sign_BYTES);
	memcpy(sm + crypto_sign_BYTES, img, HDR_SIG);
	memcpy(sm + crypto_sign_BYTES + HDR_SIG, digest, crypto_hash_BYTES);

	if (crypto_sign_open(m, &mlen, sm, sizeof(sm), pk)) return 2;
	return 0;
}

// For additional tests: build a signed image the way tools/sign.c does (minus the files).
static void make_signed_image(uint8_t *img, size_t size, const uint8_t *sk, uint32_t type){
	uint32_t magic = IMG_MAGIC;

	memset(img, 0, size);
	memcpy(img + 0, &magic, 4);
	memcpy(img + 4, &size, 4);
	memcpy(img + HDR_TYPE, &type, 4);
	memcpy(img + HDR_PK, sk + 32, 32);

	for (size_t i = IMG_PAYLOAD; i < size; i++)
		img[i] = (uint8_t)(i * 31 + 7);

	uint8_t digest[crypto_hash_BYTES];
	uint8_t message[HDR_SIG + crypto_hash_BYTES];
	uint8_t sm[crypto_sign_BYTES + sizeof(message)];
	unsigned long long mlen;

	crypto_hash(digest, img + IMG_PAYLOAD, size - IMG_PAYLOAD);

	memcpy(message, img, HDR_SIG);
	memcpy(message + HDR_SIG, digest, crypto_hash_BYTES);
	crypto_sign(sm, &mlen, message, sizeof(message), sk);
	
	memcpy(img + HDR_SIG, sm, crypto_sign_BYTES);
}

static int failures = 0;
static void check(const char *what, int got, int want){
	if (got == want)
		printf("  PASS  %s (rc=%d)\n", what, got);
	else {
		printf("  FAIL  %s: got rc=%d, want rc=%d\n", what, got, want);
		failures++;
	}
}

int main(int argc, char **argv){
	if (argc != 3){
		fprintf(stderr, "usage: %s signed_image.bin pk.key\n", argv[0]);
		return 1;
	}

	// Attempt to load the public key file
	uint8_t pk[32];
	FILE *f = fopen(argv[2], "rb");
	if (!f || fread(pk, 1, 32, f) != 32){
		fprintf(stderr, "cannot read 32-byte pk from %s\n", argv[2]);
		return 1;
	}
	fclose(f);

	// Attempt to open the signed image
	f = fopen(argv[1], "rb");
	if (!f){
		fprintf(stderr, "cannot open %s\n", argv[1]);
		return 1;
	}

	// Determine image file size
	long flen = 0;
	fseek(f, 0, SEEK_END);
	flen = ftell(f);
	fseek(f, 0, SEEK_SET);
	if (flen < 0x104 || flen > (long) FK_IMG_MAX){
		fprintf(stderr, "bad image size %ld\n", flen);
		return 1;
	}

	// Read the image
	uint8_t *img = malloc(flen);
	if (fread(img, 1, flen, f) != (size_t) flen){
		fprintf(stderr, "read error\n");
		return 1;
	}
	fclose(f);

	// Extract header values
	uint32_t magic, size, type;
	memcpy(&magic, img, 4);
	memcpy(&size, img + 4, 4);
	memcpy(&type, img + HDR_TYPE, 4);

	printf("image: %ld bytes, magic=0x%08x size=%u type=%u\n", flen, magic, size, type);

	// Verify magic and type as the ROM will
	if (magic != IMG_MAGIC || size != (uint32_t) flen){
		fprintf(stderr, "FAIL  image header inconsistent\n");
		return 1;
	}

	if (type != IMG_TYPE_SIGNED_ED25519){
		fprintf(stderr, "FAIL  image is not marked signed\n");
		return 1;
	}

	printf("Testing verification of real image\n");
	check("valid signed image", rom_verify(img, pk), 0);

	// Additional verification test cases (fresh keypair as "the ROM's key").
	uint8_t sk[crypto_sign_SECRETKEYBYTES], other_sk[crypto_sign_SECRETKEYBYTES];
	uint8_t other_pk[32];
	for (int i = 0; i < 32; i++){
		sk[i] = (uint8_t)(i + 1);
		other_sk[i] = (uint8_t)(127 - i);
	}
	crypto_sign_keypair(pk, sk);
	crypto_sign_keypair(other_pk, other_sk);

	size_t s = 0x7000; /* 28 kB, approximate size of real firmware image */
	uint8_t buf[0x10000];

	printf("Testing additional cases\n");

	// Confirm a valid key works
	make_signed_image(buf, s, sk, IMG_TYPE_SIGNED_ED25519);
	check("signed by the ROM key", rom_verify(buf, pk), 0);

	// Confirm a bad private key is rejected (BadPk)
	make_signed_image(buf, s, other_sk, IMG_TYPE_SIGNED_ED25519);
	check("signed by a different key", rom_verify(buf, pk), 1); /* BadPk */

	// Comfirm a tampered payload is rejected (BadSig)
	make_signed_image(buf, s, sk, IMG_TYPE_SIGNED_ED25519);
	buf[s - 1] ^= 0x01;
	check("tampered payload byte", rom_verify(buf, pk), 2); /* BadSig */

	// Confirm a tampered size field is rejected (BadSig)
	make_signed_image(buf, s, sk, IMG_TYPE_SIGNED_ED25519);
	memcpy(buf + 4, &s, 4); buf[4] ^= 4; /* size field lies: +4 bytes */
	check("tampered size field", rom_verify(buf, pk), 2);     /* BadSig */

	// Confirm a tampered signature is rejected (BadSig)
	make_signed_image(buf, s, sk, IMG_TYPE_SIGNED_ED25519);
	buf[HDR_SIG] ^= 0x80;
	check("tampered signature byte", rom_verify(buf, pk), 2); /* BadSig */

	// Confirm an unsigned image is rejected
	make_signed_image(buf, s, sk, IMG_TYPE_SIGNED_ED25519);
	uint32_t zero = 0;
	memcpy(buf + HDR_TYPE, &zero, 4);
	check("unsigned type (ROM refuses before verify)", rom_check_unsigned(buf), 1);

	free(img);
	printf(failures ? "RESULT: %d FAILURE(S)\n" : "RESULT: all pass\n", failures);
	return failures != 0;
}
