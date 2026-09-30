#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <sys/stat.h>
#include "../tweetnacl.h"

int main(int argc, char* argv[]){
    unsigned char pk[crypto_sign_PUBLICKEYBYTES];
    unsigned char sk[crypto_sign_SECRETKEYBYTES];
    
    if(argc != 3){
        fprintf(stderr, "usage: genkey pk.key sk.key\n");
        return 1;
    }

    printf("Generating keypair %s (public) and %s (secret)\n", argv[1], argv[2]);

    // O_EXCL: refuse to clobber an existing key file rather than silently overwriting a previously generated keypair.
    int pk_file = open(argv[1], O_WRONLY|O_CREAT|O_EXCL, S_IRUSR|S_IWUSR|S_IRGRP|S_IROTH);
    if(pk_file < 0){
        fprintf(stderr, "Could not create %s to write public key: %s\n", argv[1], strerror(errno));
        return 2;
    }

    int sk_file = open(argv[2], O_WRONLY|O_CREAT|O_EXCL, S_IRUSR|S_IWUSR);
    if(sk_file < 0){
        fprintf(stderr, "Could not create %s to write secret key: %s\n", argv[2], strerror(errno));
        return 3;
    }

    crypto_sign_keypair(pk, sk);

    if(write(pk_file, pk, crypto_sign_PUBLICKEYBYTES) != crypto_sign_PUBLICKEYBYTES){
        fprintf(stderr, "Could not write public key file at %s.\n", argv[1]);
        return 4;
    }

    close(pk_file);

    if(write(sk_file, sk, crypto_sign_SECRETKEYBYTES) != crypto_sign_SECRETKEYBYTES){
        fprintf(stderr, "Could not write secret key file at %s.\n", argv[2]);
        return 5;
    }

    close(sk_file);

    return 0;
}