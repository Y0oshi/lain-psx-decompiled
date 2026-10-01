/* lain: HD texture replacement (texture packs, port/MODDING.md).
 *
 * Every picture uploaded into VRAM is identified by a hash of its pixels; a
 * textured primitive that samples one is matched, together with the hash of the
 * palette it uses, against the replacements the client provides. */
#ifndef LAIN_HD_H
#define LAIN_HD_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    uint64_t image;      /* hash of the uploaded picture */
    uint64_t palette;    /* hash of the palette used (4/8-bit), 0 for 16-bit */
    int bpp;             /* 4, 8 or 16 */
    int x, y, w, h;      /* the picture in VRAM, in 16-bit words */
    int width;           /* in texels: w * 4, * 2 or * 1 */
    int clut_x, clut_y;  /* the palette in VRAM (4/8-bit) */
    const uint16_t *vram; /* 1024 x 512 words */
} LainHDKey;

/* Returns the replacement's texture for a picture seen for the first time with
 * this palette, or 0 for none. Called once per key, on the render thread. */
typedef unsigned int (*LainHDProvider)(const LainHDKey *key);
void LainHD_SetProvider(LainHDProvider provider);

/* VRAM writes (render): an upload is recorded, any write drops the pictures it overlaps. */
void LainHD_NoteUpload(int x, int y, int w, int h);
void LainHD_NoteWrite(int x, int y, int w, int h);

typedef struct {
    unsigned int texture;       /* 0: none */
    float origin_x, origin_y;   /* the picture's top left, in texels of the page */
    float inv_w, inv_h;         /* 1 / its size in texels */
} LainHDMatch;

/* The replacement for a textured primitive: tpage and clut words and the box of
 * texels it samples. */
void LainHD_Match(int tpage, int clut, int umin, int vmin, int umax, int vmax, LainHDMatch *out);

/* A texture for an HD picture (RGBA8), with mipmaps. */
unsigned int GR_CreateHDTexture(int width, int height, const unsigned char *rgba);

#ifdef __cplusplus
}
#endif

#endif
