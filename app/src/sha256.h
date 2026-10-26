/* SHA-256 (FIPS 180-4), for checking downloads against a known digest
   (sha256.c). */
#ifndef XPPS_SHA256_H
#define XPPS_SHA256_H

#include <stddef.h>
#include <stdint.h>

typedef struct {
  uint32_t h[8];
  uint64_t bytes;      /* message length so far */
  unsigned char buf[64];
  size_t nbuf;
} Sha256;

void sha256_init(Sha256 *s);
void sha256_update(Sha256 *s, const void *data, size_t n);
/* The digest as 64 lower-case hex digits and a NUL. */
void sha256_hex(Sha256 *s, char out[65]);

#endif
