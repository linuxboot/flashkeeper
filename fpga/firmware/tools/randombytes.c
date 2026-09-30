#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <sys/random.h>

void randombytes(uint8_t *dst, uint64_t n){
    unsigned char *p = (unsigned char *)dst;
    while (n > 0) {
        /* getrandom() may return a short read, so loop until n bytes are
         * generated. Failure is fatal: never fall back to or continue with
         * partial/uninitialized data for key material. */
        ssize_t r = getrandom(p, n, 0);
        if (r <= 0) {
            perror("getrandom");
            exit(1);
        }
        p += (size_t)r;
        n -= (size_t)r;
    }
}