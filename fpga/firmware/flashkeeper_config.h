#include <stdint.h>

// 1 = require a password to access the serial console/EDE
// 0 = automatic login (insecure, for testing only)
#define SERIAL_REQUIRE_PASSWORD 1

// default hash: d8c1b45876447500e2cb33f8a1340bd77c90bd934605221966c9dca723bb68e2117eaf322b9f26793c4bde25473b51871add093fc638edb05574e1ef0abd55f2
// of password "flashkeeper" with salt 2cfe3cc5f7ca7e5c560355fac96b067f
// run "genpw" on the Flashkeeper serial console to generate the below two lines for your own password
static const uint32_t serial_password_salt[] = {0xc53cfe2c,0x5c7ecaf7,0xfa550356,0x7f066bc9};
static const uint32_t serial_password_hash[] = {0x58b4c1d8,0x00754476,0xf833cbe2,0xd70b34a1,0x93bd907c,0x19220546,0xa7dcc966,0xe268bb23,0x32af7e11,0x79269f2b,0x25de4b3c,0x87513b47,0x3f09dd1a,0xb0ed38c6,0xefe17455,0xf255bd0a};