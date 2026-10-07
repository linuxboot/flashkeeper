// Build stub: ed25519_ref10.c includes this but it's never called from the verify path

#ifndef EVH
#define EVH
#include <stddef.h>
int crypto_verify_32(const unsigned char *,const unsigned char *,size_t);
#endif
