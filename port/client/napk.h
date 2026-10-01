/* "napk": the LZSS packing of the game's archive entries (lz_decompress in the game).
 *
 * "napk", then the unpacked size (32-bit little-endian), then groups of a flag
 * byte and 8 items, flags read from the top bit down: 0 = a literal byte,
 * 1 = a 2-byte back reference (distance - 1, length - 3). */
#pragma once
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* 1 if data starts with the napk header. */
int napk_is_packed(const uint8_t *data, size_t size);

/* Unpacks into a malloc'd buffer; NULL if the data is damaged. */
uint8_t *napk_unpack(const uint8_t *data, size_t size, size_t *out_size);

/* Packs data (malloc'd result, header included), choosing the shortest
 * encoding the format allows. */
uint8_t *napk_pack(const uint8_t *data, size_t size, size_t *out_size);

#ifdef __cplusplus
}
#endif
