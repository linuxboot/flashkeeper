// Minimal memset and memcpy for freestanding fe_25_5.h and ed25519_ref10.c

#ifndef STRING_STUB
#define STRING_STUB
#include <stddef.h>
static void *memset(void *d, int c, size_t n) { unsigned char *p=d; while(n--) *p++=(unsigned char)c; return d; }
static void *memcpy(void *d, const void *s, size_t n) { unsigned char *p=d; const unsigned char *q=s; while(n--) *p++=*q++; return d; }
#endif
