/* Minimal ISO9660 access to a raw (2352-byte sector) PS1 disc image. */
#pragma once
#include <stdint.h>
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Reads one sector's 2048 bytes of user data (Mode 1 or Mode 2 Form 1). */
int iso_read_sector(FILE *img, uint32_t lba, uint8_t out[2048]);

/* Finds a file in the root directory by name (case-sensitive, without ";1").
 * Returns 0 and fills lba/size on success. */
int iso_find_root_file(FILE *img, const char *name, uint32_t *lba, uint32_t *size);

typedef void (*IsoDirFn)(const char *name, uint32_t lba, uint32_t size, void *user);

/* Calls fn for each file (not subdirectory) in a directory given as a path from
 * the root ("" for the root, "XA", "MOVIE"). Returns 0 if the directory exists. */
int iso_list_dir(FILE *img, const char *path, IsoDirFn fn, void *user);

/* Reads a whole file (size bytes starting at lba) into a malloc'd buffer. */
uint8_t *iso_read_file(FILE *img, uint32_t lba, uint32_t size);

#ifdef __cplusplus
}
#endif
