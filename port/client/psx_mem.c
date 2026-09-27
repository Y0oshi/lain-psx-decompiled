#include "psx_mem.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "iso.h"
#include "psx_arena.h"

#define EXE_HEADER 0x800
#define RETAIL_IMAGE_SIZE 0x96800 /* SLPS_016.03/04 text + data */

static uint32_t le32(const uint8_t *p) {
    return p[0] | p[1] << 8 | p[2] << 16 | (uint32_t)p[3] << 24;
}

int psx_mem_load(const char *disc_bin, char *err, int err_cap) {
    FILE *img = fopen(disc_bin, "rb");
    if (!img) {
        snprintf(err, err_cap, "Can't open %s", disc_bin);
        return -1;
    }
    uint32_t lba, size;
    /* The EXE has the same content on both discs, under the disc's serial. */
    if (iso_find_root_file(img, "SLPS_016.03", &lba, &size) != 0 &&
        iso_find_root_file(img, "SLPS_016.04", &lba, &size) != 0) {
        fclose(img);
        snprintf(err, err_cap, "Game executable not found on the disc image");
        return -1;
    }
    uint8_t *exe = iso_read_file(img, lba, size);
    fclose(img);
    if (!exe) {
        snprintf(err, err_cap, "Read error loading the game executable");
        return -1;
    }
    if (size < EXE_HEADER || memcmp(exe, "PS-X EXE", 8) != 0) {
        free(exe);
        snprintf(err, err_cap, "The game executable on the disc is not valid");
        return -1;
    }
    uint32_t t_addr = le32(exe + 0x18), t_size = le32(exe + 0x1C);
    if (t_addr < PSX_RAM_BASE || t_addr + t_size > PSX_RAM_BASE + PSX_RAM_SIZE ||
        EXE_HEADER + t_size > size) {
        free(exe);
        snprintf(err, err_cap, "Unexpected executable layout");
        return -1;
    }
    memset(psx_arena, 0, PSX_RAM_SIZE);
    /* The whole image (text, rodata, data) goes in at its load address: data
     * tables keep their original layout, and BSS past it starts zeroed. Patched
     * discs append code past the retail image; only the retail part is loaded. */
    if (t_size > RETAIL_IMAGE_SIZE) {
        t_size = RETAIL_IMAGE_SIZE;
    }
    memcpy(psx_arena + (t_addr - PSX_RAM_BASE), exe + EXE_HEADER, t_size);
    free(exe);
    return 0;
}
