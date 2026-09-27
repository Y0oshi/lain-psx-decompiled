/* Minimal SHA-1 (FIPS 180-1), used only to identify the player's disc dumps. */
#pragma once
#include <stddef.h>
#include <stdint.h>

typedef struct {
    uint32_t h[5];
    uint64_t length;      /* bytes processed */
    uint8_t block[64];
    size_t used;          /* bytes in block */
} Sha1;

void sha1_init(Sha1 *s);
void sha1_update(Sha1 *s, const void *data, size_t len);
void sha1_final(Sha1 *s, uint8_t digest[20]);
/* Lower-case hex of a digest; out must hold 41 bytes. */
void sha1_hex(const uint8_t digest[20], char out[41]);
