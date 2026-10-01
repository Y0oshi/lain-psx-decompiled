#include "tim.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static uint32_t rd32(const uint8_t *p) {
    return p[0] | p[1] << 8 | p[2] << 16 | (uint32_t)p[3] << 24;
}

static uint16_t rd16(const uint8_t *p) {
    return (uint16_t)(p[0] | p[1] << 8);
}

static void wr16(uint8_t *p, uint16_t v) {
    p[0] = (uint8_t)v;
    p[1] = (uint8_t)(v >> 8);
}

int tim_parse(const uint8_t *data, size_t size, TimInfo *info) {
    static const size_t starts[3] = {0, 4, 8};
    for (int s = 0; s < 3; s++) {
        size_t t = starts[s];
        if (t + 8 > size || rd32(data + t) != 0x10) {
            continue;
        }
        uint32_t flag = rd32(data + t + 4);
        int mode = flag & 7;
        if (mode > 3 || (flag & ~0xFu)) {
            continue;
        }
        TimInfo in;
        memset(&in, 0, sizeof in);
        in.start = t;
        in.bpp = mode == 0 ? 4 : mode == 1 ? 8 : mode == 2 ? 16 : 24;
        size_t p = t + 8;
        if (flag & 8) {
            if (p + 12 > size) continue;
            uint32_t len = rd32(data + p);
            in.clut_w = rd16(data + p + 8);
            in.clut_h = rd16(data + p + 10);
            in.clut_data = p + 12;
            if (len < 12 || p + len > size || (size_t)in.clut_w * in.clut_h * 2 + 12 > len || in.clut_w == 0 ||
                in.clut_h == 0) {
                continue;
            }
            p += len;
        }
        if (in.bpp <= 8 && in.clut_w == 0) {
            continue; /* paletted without a palette */
        }
        if (p + 12 > size) continue;
        uint32_t len = rd32(data + p);
        int pw = rd16(data + p + 8), ph = rd16(data + p + 10);
        in.pix_data = p + 12;
        in.pix_stride = pw * 2;
        in.w = in.bpp == 4 ? pw * 4 : in.bpp == 8 ? pw * 2 : in.bpp == 16 ? pw : pw * 2 / 3;
        in.h = ph;
        if (len < 12 || p + len > size || in.w == 0 || in.h == 0 || in.w > 1024 || in.h > 512 ||
            in.pix_data + (size_t)in.pix_stride * ph > size) {
            continue;
        }
        *info = in;
        return 1;
    }
    return 0;
}

static void rgba_of(uint16_t v, uint8_t *out) {
    out[0] = (uint8_t)((v & 31) << 3);
    out[1] = (uint8_t)(((v >> 5) & 31) << 3);
    out[2] = (uint8_t)(((v >> 10) & 31) << 3);
    out[3] = v == 0 ? 0 : 255;
}

static uint16_t clut_color(const uint8_t *data, const TimInfo *in, int clut_index, int i) {
    if (clut_index >= in->clut_h || i >= in->clut_w) return 0;
    return rd16(data + in->clut_data + ((size_t)clut_index * in->clut_w + i) * 2);
}

uint8_t *tim_to_rgba(const uint8_t *data, size_t size, const TimInfo *in, int clut_index) {
    (void)size;
    uint8_t *out = (uint8_t *)malloc((size_t)in->w * in->h * 4);
    if (!out) return NULL;
    for (int y = 0; y < in->h; y++) {
        const uint8_t *row = data + in->pix_data + (size_t)y * in->pix_stride;
        for (int x = 0; x < in->w; x++) {
            uint8_t *px = out + ((size_t)y * in->w + x) * 4;
            if (in->bpp == 24) {
                memcpy(px, row + x * 3, 3);
                px[3] = 255;
            } else if (in->bpp == 16) {
                rgba_of(rd16(row + x * 2), px);
            } else if (in->bpp == 8) {
                rgba_of(clut_color(data, in, clut_index, row[x]), px);
            } else {
                uint8_t b = row[x / 2];
                rgba_of(clut_color(data, in, clut_index, (x & 1) ? b >> 4 : b & 15), px);
            }
        }
    }
    return out;
}

/* A PNG pixel as a PS1 color without the semi-transparency bit; transparent -> 0. */
static uint16_t color15(const uint8_t *px) {
    if (px[3] < 128) return 0;
    return (uint16_t)((px[0] >> 3) | (px[1] >> 3) << 5 | (px[2] >> 3) << 10);
}

/* Opaque pure black must not be 0x0000 (transparent): the PS1 draws 0x8000 as black. */
static uint16_t opaque(uint16_t c, int stp) {
    return c == 0 ? 0x8000 : (uint16_t)(c | (stp ? 0x8000 : 0));
}

typedef struct {
    uint16_t c;   /* RGB555 */
    uint32_t n;   /* pixels */
} Color;

static int cmp_channel;
static int ch(uint16_t c, int k) {
    return (c >> (5 * k)) & 31;
}
static int by_channel(const void *a, const void *b) {
    return ch(((const Color *)a)->c, cmp_channel) - ch(((const Color *)b)->c, cmp_channel);
}

/* Median cut: splits `colors` into at most `want` boxes; writes one color per box. */
static int median_cut(Color *colors, int n, int want, uint16_t *out) {
    typedef struct {
        int lo, hi;
    } Box;
    Box boxes[256];
    int nb = 1;
    boxes[0].lo = 0;
    boxes[0].hi = n;
    while (nb < want) {
        int pick = -1, pick_ch = 0, pick_range = 0;
        for (int b = 0; b < nb; b++) {
            if (boxes[b].hi - boxes[b].lo < 2) continue;
            for (int k = 0; k < 3; k++) {
                int mn = 31, mx = 0;
                for (int i = boxes[b].lo; i < boxes[b].hi; i++) {
                    int v = ch(colors[i].c, k);
                    if (v < mn) mn = v;
                    if (v > mx) mx = v;
                }
                if (mx - mn > pick_range) {
                    pick_range = mx - mn;
                    pick = b;
                    pick_ch = k;
                }
            }
        }
        if (pick < 0) break;
        cmp_channel = pick_ch;
        qsort(colors + boxes[pick].lo, (size_t)(boxes[pick].hi - boxes[pick].lo), sizeof(Color), by_channel);
        /* split at the median pixel count */
        uint64_t total = 0, acc = 0;
        for (int i = boxes[pick].lo; i < boxes[pick].hi; i++) total += colors[i].n;
        int mid = boxes[pick].lo + 1;
        for (int i = boxes[pick].lo; i < boxes[pick].hi - 1; i++) {
            acc += colors[i].n;
            mid = i + 1;
            if (acc * 2 >= total) break;
        }
        boxes[nb].lo = mid;
        boxes[nb].hi = boxes[pick].hi;
        boxes[pick].hi = mid;
        nb++;
    }
    for (int b = 0; b < nb; b++) {
        uint64_t s[3] = {0, 0, 0}, w = 0;
        for (int i = boxes[b].lo; i < boxes[b].hi; i++) {
            for (int k = 0; k < 3; k++) s[k] += (uint64_t)ch(colors[i].c, k) * colors[i].n;
            w += colors[i].n;
        }
        if (!w) w = 1;
        out[b] = (uint16_t)((s[0] + w / 2) / w | ((s[1] + w / 2) / w) << 5 | ((s[2] + w / 2) / w) << 10);
    }
    return nb;
}

static int nearest(const uint16_t *pal, int n, uint16_t c, int skip_transparent) {
    int best = -1;
    long bd = 0;
    for (int i = 0; i < n; i++) {
        uint16_t p = pal[i] & 0x7FFF;
        if (skip_transparent && pal[i] == 0) continue;
        long d = 0;
        for (int k = 0; k < 3; k++) {
            long e = ch(p, k) - ch(c, k);
            d += e * e;
        }
        if (best < 0 || d < bd) {
            best = i;
            bd = d;
        }
    }
    return best < 0 ? 0 : best;
}

static void set_msg(char *msg, size_t cap, const char *text) {
    if (msg && cap) snprintf(msg, cap, "%s", text);
}

int tim_from_rgba(uint8_t *data, size_t size, const TimInfo *in, const uint8_t *rgba, char *msg, size_t msg_cap) {
    (void)size;
    const size_t npx = (size_t)in->w * in->h;
    set_msg(msg, msg_cap, "");

    if (in->bpp == 24) {
        for (int y = 0; y < in->h; y++) {
            uint8_t *row = data + in->pix_data + (size_t)y * in->pix_stride;
            for (int x = 0; x < in->w; x++) memcpy(row + x * 3, rgba + ((size_t)y * in->w + x) * 4, 3);
        }
        return 0;
    }
    if (in->bpp == 16) {
        for (int y = 0; y < in->h; y++) {
            uint8_t *row = data + in->pix_data + (size_t)y * in->pix_stride;
            for (int x = 0; x < in->w; x++) {
                uint16_t old = rd16(row + x * 2);
                uint16_t c = color15(rgba + ((size_t)y * in->w + x) * 4);
                wr16(row + x * 2, rgba[((size_t)y * in->w + x) * 4 + 3] < 128 ? 0 : opaque(c, old & 0x8000));
            }
        }
        return 0;
    }

    /* Paletted: 16 or 256 colors per palette (fewer if the palette is narrower). */
    const int pal_n = in->clut_w < (in->bpp == 4 ? 16 : 256) ? in->clut_w : (in->bpp == 4 ? 16 : 256);
    uint16_t pal[256];
    for (int i = 0; i < pal_n; i++) pal[i] = clut_color(data, in, 0, i);
    int stp_votes = 0, opaque_n = 0;
    for (int i = 0; i < pal_n; i++) {
        if (pal[i] != 0) {
            opaque_n++;
            stp_votes += (pal[i] & 0x8000) != 0;
        }
    }
    const int stp = opaque_n > 0 && stp_votes * 2 > opaque_n;

    uint8_t *index = (uint8_t *)malloc(npx);
    if (!index) {
        set_msg(msg, msg_cap, "out of memory");
        return -1;
    }
    /* 1. Every pixel's color is already in palette 0: keep the palette. A pixel
     * keeps its original index when that still has its color, since a palette
     * can hold a color twice and the other palettes may differ there. */
    int exact = 1;
    for (size_t i = 0; i < npx && exact; i++) {
        uint16_t c = color15(rgba + i * 4);
        int transparent = rgba[i * 4 + 3] < 128, found = -1;
        const uint8_t *row = data + in->pix_data + (i / in->w) * in->pix_stride;
        size_t x = i % in->w;
        int old = in->bpp == 8 ? row[x] : (x & 1) ? row[x / 2] >> 4 : row[x / 2] & 15;
        if (old < pal_n && (transparent ? pal[old] == 0 : (pal[old] != 0 && (pal[old] & 0x7FFF) == c))) found = old;
        for (int k = 0; k < pal_n && found < 0; k++) {
            if (transparent ? pal[k] == 0 : (pal[k] != 0 && (pal[k] & 0x7FFF) == c)) found = k;
        }
        exact = found >= 0;
        index[i] = (uint8_t)(found < 0 ? 0 : found);
    }
    int result = 0;
    if (!exact && in->clut_h == 1) {
        /* 2. One palette: build a new one from the image. */
        Color *colors = (Color *)calloc(32768, sizeof(Color));
        int transparent = 0;
        if (!colors) {
            free(index);
            set_msg(msg, msg_cap, "out of memory");
            return -1;
        }
        for (size_t i = 0; i < npx; i++) {
            if (rgba[i * 4 + 3] < 128) transparent = 1;
            else colors[color15(rgba + i * 4)].n++;
        }
        int n = 0;
        for (int c = 0; c < 32768; c++) {
            if (colors[c].n) {
                colors[n].c = (uint16_t)c;
                colors[n].n = colors[c].n;
                n++;
            }
        }
        const int slots = pal_n - transparent;
        int first = transparent ? 1 : 0;
        uint16_t chosen[256];
        int used;
        if (n <= slots) {
            for (int i = 0; i < n; i++) chosen[i] = colors[i].c;
            used = n;
        } else {
            used = median_cut(colors, n, slots, chosen);
            char t[160];
            snprintf(t, sizeof t, "%d colors reduced to the %d the image format holds", n, slots);
            set_msg(msg, msg_cap, t);
            result = 1;
        }
        free(colors);
        if (transparent) pal[0] = 0;
        for (int i = 0; i < pal_n - first; i++) pal[first + i] = i < used ? opaque(chosen[i], stp) : opaque(0, stp);
        for (int i = 0; i < pal_n; i++) wr16(data + in->clut_data + (size_t)i * 2, pal[i]);
        for (size_t i = 0; i < npx; i++) {
            index[i] = rgba[i * 4 + 3] < 128 ? 0 : (uint8_t)nearest(pal, pal_n, color15(rgba + i * 4), 1);
        }
    } else if (!exact) {
        /* 3. Several palettes share the pixels: map to the nearest color of palette 0. */
        int has_transparent = 0;
        for (int k = 0; k < pal_n; k++) has_transparent |= pal[k] == 0;
        for (size_t i = 0; i < npx; i++) {
            if (rgba[i * 4 + 3] < 128 && has_transparent) {
                int k = 0;
                while (pal[k] != 0) k++;
                index[i] = (uint8_t)k;
            } else {
                index[i] = (uint8_t)nearest(pal, pal_n, color15(rgba + i * 4), 1);
            }
        }
        set_msg(msg, msg_cap, "colors matched to the original palette (the picture has several palettes)");
        result = 1;
    }
    for (int y = 0; y < in->h; y++) {
        uint8_t *row = data + in->pix_data + (size_t)y * in->pix_stride;
        for (int x = 0; x < in->w; x++) {
            uint8_t v = index[(size_t)y * in->w + x];
            if (in->bpp == 8) {
                row[x] = v;
            } else if (x & 1) {
                row[x / 2] = (uint8_t)((row[x / 2] & 0x0F) | (v & 15) << 4);
            } else {
                row[x / 2] = (uint8_t)((row[x / 2] & 0xF0) | (v & 15));
            }
        }
    }
    free(index);
    return result;
}
