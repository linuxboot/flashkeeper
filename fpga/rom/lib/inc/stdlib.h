// ed25519_ref10.c only calls abort() inside ge25519_elligator2, which is gc-sectioned
// out of the verify-only build). Stub implementation in ref10_stub_impl.c. 

#ifndef STDLIB_STUB
#define STDLIB_STUB
void abort(void);
#endif
