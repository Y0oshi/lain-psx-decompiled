/* PlayStation TIM images: RGBA conversion both ways (mods, exported originals). */
#pragma once
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    size_t start;      /* offset of the TIM in the data (entries may have a small header) */
    int bpp;           /* 4, 8, 16 or 24 */
    int w, h;          /* in pixels */
    int clut_w, clut_h; /* palette size (0 for 16/24-bit): clut_h palettes of clut_w colors */
    size_t clut_data;  /* offsets of the palette and pixel data */
    size_t pix_data;
    int pix_stride;    /* bytes per row of pixels */
} TimInfo;

/* Finds a TIM at offset 0, 4 or 8 of data. Returns 1 if found and consistent. */
int tim_parse(const uint8_t *data, size_t size, TimInfo *info);

/* RGBA8 (malloc'd, w*h*4) of the image with palette `clut_index`. Color 0x0000
 * is transparent (alpha 0), as the PS1 draws it. */
uint8_t *tim_to_rgba(const uint8_t *data, size_t size, const TimInfo *info, int clut_index);

/* Replaces the image in `data` (a copy of the original TIM data, same size)
 * with `rgba` (info->w x info->h). The layout, positions and palette count stay
 * as they are. Returns 0 on success, 1 if colors had to be approximated (note
 * in msg), -1 on failure (reason in msg). */
int tim_from_rgba(uint8_t *data, size_t size, const TimInfo *info, const uint8_t *rgba, char *msg, size_t msg_cap);

#ifdef __cplusplus
}
#endif
