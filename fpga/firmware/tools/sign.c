/*
 * sign - produce a signed Flashkeeper firmware image.
 *
 * Usage: sign firmware.bin firmware_signed.bin secret_key.key
 *
 * The signature placed at 0x0C0 is a signature of the 256-byte message:
 *     image[0x000..0x0C0)  ||  SHA-512(image[0x100..size))
 */
#include <stdio.h>
#include <stdint.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <errno.h>
#include <string.h>
#include "../../memmap.h"
#include "../tweetnacl.h"

#define MAX_FW_SIZE         128 * 1024 // 128 kB of SPRAM available for the firmware image including the header and stack
#define IMG_HEADER_SIZE     0x100
#define IMG_MAGIC           0x57464B46u /* "FKFW" little-endian */
#define IMG_MIN_SIZE        0x104u      /* header + at least the 4-byte entry */
#define IMG_UNSIGNED        0
#define IMG_SIGNED_ED25519  1

int main(int argc, char* argv[]){
    uint8_t in_image[MAX_FW_SIZE] = {0};
    uint8_t out_image[MAX_FW_SIZE] = {0};
    uint8_t key[crypto_sign_SECRETKEYBYTES] = {0};
    struct stat in_stbuf, key_stbuf;

    if(argc != 4){
        fprintf(stderr, "usage: sign firmware.bin firmware_signed.bin secret_key.key\n");
        return 1;
    }

    // Open all files
    int in_file = open(argv[1], O_RDONLY);
    if(in_file < 0){
        fprintf(stderr, "Could not open unsigned firmware image %s: %s\n", argv[1], strerror(errno));
        return 2;
    }

    /* O_TRUNC: a previous, larger signed image must not leave stale bytes
       past the new image size - the ROM only copies `size` bytes, but
       tools like makehex.py process the whole file. */
    int out_file = open(argv[2], O_WRONLY|O_CREAT|O_TRUNC, S_IRUSR|S_IWUSR|S_IRGRP|S_IROTH);
    if(out_file < 0){
        fprintf(stderr, "Could not create %s to write signed firmware image: %s\n", argv[2], strerror(errno));
        return 3;
    }

    int key_file = open(argv[3], O_RDONLY);
    if(key_file < 0){
        fprintf(stderr, "Could not open secret key file %s: %s\n", argv[3], strerror(errno));
        return 4;
    }

    // Get the input image size to confirm against the header
    if(fstat(in_file, &in_stbuf) != 0){
        fprintf(stderr, "Could not stat firmware image %s: %s\n", argv[1], strerror(errno));
        return 5;
    }
    
    // File must fit in the buffer and the ROM's size limit, and must be word-aligned
    // st_size is signed (off_t), so comparison is signed
    if(in_stbuf.st_size < (off_t) IMG_MIN_SIZE || in_stbuf.st_size > (off_t) FK_IMG_MAX ||
       (in_stbuf.st_size & 3)){
        fprintf(stderr, "Firmware image %s is %ld bytes; expected [0x%x, 0x%x] and word-aligned\n",
                argv[1], (long) in_stbuf.st_size, IMG_MIN_SIZE, FK_IMG_MAX);
        return 6;
    }
    size_t in_size = (size_t) in_stbuf.st_size;

    // Read the input image
    ssize_t in_read_status = read(in_file, in_image, MAX_FW_SIZE);
    if(in_read_status < 0 || (size_t) in_read_status != in_size){
        fprintf(stderr, "Could not read firmware image %s\n", argv[1]);
        return 7;
    }

    close(in_file);

    // Confirm the key is the right size (exact: seed || public key)
    if(fstat(key_file, &key_stbuf) != 0){
        fprintf(stderr, "Could not stat secret key file %s: %s\n", argv[3], strerror(errno));
        return 8;
    }

    if(key_stbuf.st_size != (off_t) crypto_sign_SECRETKEYBYTES){
        fprintf(stderr, "Key file %s is not the required size %i bytes. Is this the wrong key?\n", argv[3], crypto_sign_SECRETKEYBYTES);
        return 9;
    }

    // Load the key
    ssize_t key_read_status = read(key_file, key, crypto_sign_SECRETKEYBYTES);
    if(key_read_status != (ssize_t) crypto_sign_SECRETKEYBYTES){
        fprintf(stderr, "Could not read required %i bytes from %s\n", crypto_sign_SECRETKEYBYTES, argv[3]);
        return 10;
    }

    close(key_file);

    // Confirm magic and size in the header are consistent with the file
    if(((uint32_t*) in_image)[0] != IMG_MAGIC){
        fprintf(stderr, "Invalid magic in input firmware - is this a valid Flashkeeper image?\n");
        return 11;
    }

    if(((uint32_t*) in_image)[1] != in_size){
        fprintf(stderr, "Header size does not match file size. Is this header correct?\n");
        return 12;
    }

    if(((uint32_t*) in_image)[2] != IMG_UNSIGNED){
        fprintf(stderr, "Header is not marked as unsigned. Is this already a signed image?\n");
        return 13;
    }

    printf("Signing %lu byte firmware image %s as %s\n", in_size, argv[1], argv[2]);

    // Copy the whole image (header and payload) into the output; the
    // header is finalized below and the signature added at the end.
    memcpy(out_image, in_image, in_size);

    // Mark the image as signed (before signing: the type field is covered)
    ((uint32_t*) out_image)[2] = IMG_SIGNED_ED25519;

    // Add the public key used (0x20 and on in the secret key file) to the
    // image (before signing: the key is covered)
    memcpy(out_image + IMG_HEADER_SIZE - crypto_sign_BYTES - crypto_sign_PUBLICKEYBYTES,
           key + (crypto_sign_SECRETKEYBYTES - crypto_sign_PUBLICKEYBYTES),
           crypto_sign_PUBLICKEYBYTES);

    // Sign: message = header prefix [0x000, 0xC0) || SHA-512(payload)
    unsigned long long mlen;
    uint8_t digest[crypto_hash_BYTES];
    uint8_t message[IMG_HEADER_SIZE - crypto_sign_BYTES + crypto_hash_BYTES];
    uint8_t sm[crypto_sign_BYTES + sizeof(message)];
    crypto_hash(digest, in_image + IMG_HEADER_SIZE, in_size - IMG_HEADER_SIZE);
    memcpy(message, out_image, IMG_HEADER_SIZE - crypto_sign_BYTES);
    memcpy(message + IMG_HEADER_SIZE - crypto_sign_BYTES, digest, crypto_hash_BYTES);
    crypto_sign(sm, &mlen, message, sizeof(message), key);
    // sm = signature || message; only the signature goes into the image
    memcpy(out_image + IMG_HEADER_SIZE - crypto_sign_BYTES, sm, crypto_sign_BYTES);

    // Write out the result
    if(write(out_file, out_image, in_size) != (ssize_t) in_size){
        fprintf(stderr, "Could not write output firmware image %s.\n", argv[2]);
        return 14;
    }

    close(out_file);

    // Make sure crypto_sign_open correctly verifies the signature we just generated
    uint8_t verify_m[crypto_sign_BYTES + sizeof(message)];
    if (crypto_sign_open(verify_m, &mlen, sm, crypto_sign_BYTES + sizeof(message),
                            key + crypto_sign_SECRETKEYBYTES - crypto_sign_PUBLICKEYBYTES) != 0){
        fprintf(stderr, "Self-test failed - signed image could not be verified with its own key.\n");
        return 15;
    }
    printf("Self-test: Signature verified.\n");

    return 0;
}