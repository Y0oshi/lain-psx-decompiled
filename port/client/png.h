/* PNG reading and writing, no dependencies (mods and their exported originals). */
#pragma once
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Decodes a PNG file in memory into 8-bit RGBA (malloc'd, w*h*4 bytes).
 * Handles every color type, bit depths 1-16, palettes, tRNS and Adam7
 * interlacing. Returns NULL and writes a reason to err on failure. */
uint8_t *png_decode(const uint8_t *data, size_t size, int *w, int *h, char *err, size_t err_cap);

/* Same, from a file. */
uint8_t *png_load(const char *path, int *w, int *h, char *err, size_t err_cap);

/* Writes 8-bit RGBA as a compressed PNG. Returns 0 on success. */
int png_save(const char *path, const uint8_t *rgba, int w, int h);

#ifdef __cplusplus
}
#endif
