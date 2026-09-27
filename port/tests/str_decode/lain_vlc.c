/*
 * Reference decoder for Lain's movie bitstream, modelled on the game's
 * hand-written vlc_decode_frame (see asm/psyq/unknown_8007038C.s). Lain does not
 * use Sony's STR v2/v3 format; each frame's data is:
 *
 *   u8  luma qscale            (game: g_vlc_qscale_luma)
 *   u8  chroma qscale          (game: g_vlc_qscale_chroma)
 *   u16 0x3800
 *   u32 number of run-level halfwords the frame decodes to
 *   ... bitstream, read MSB first in byte order
 *
 * Per macroblock, 6 blocks (Cr, Cb, Y0..Y3); per block a raw 10-bit DC value,
 * then AC codes shaped like MPEG-1's (ISO 11172-2 "dct_coeff_next") with a
 * sign bit (Lain's own run/level assignment, see below), MPEG-1 escape
 * (000001 + 6-bit run + 8/16-bit level) and EOB (10).
 * The output is an MDEC command stream: header word 0x3800xxxx (xxxx = words
 * that follow), run-level halfwords, padded with 0xFE00 to 64 halfwords.
 */
#include "lain_vlc.h"

#include <string.h>

/*
 * The AC codes have the bit patterns of MPEG-1's dct_coeff_next table, but
 * Lain assigns its own (run, level) to each code: vlc_decode_frame maps a code
 * to an index 0..110 into a 111-entry table of (run << 10 | level) halfwords
 * held in the game's data (g_movie_vlc_table, expanded by vlc_build_table into
 * positive/negative copies at D_801FD2D8). That table is game data, so it is
 * not in this file: the caller passes it to lain_vlc_init() (the test reads
 * it from the user's own SLPS_016.03).
 *
 * Code groups as the game's decoder walks them (sign bit follows the code):
 *   prefix         suffix bits  first index
 */
static const struct { const char *prefix; int bits, base; } k_groups[] = {
    {"11",           0,  0},
    {"011",          0,  1},
    {"010",          1,  2},   /* 0100, 0101 */
    {"001",          2,  3},   /* 00101..00111 -> 4..6 (suffix 00 is the next group) */
    {"0001",         2,  7},
    {"00001",        2, 11},
    {"00100",        3, 15},
    {"0000001",      3, 23},
    {"00000001",     4, 31},
    {"000000001",    4, 47},
    {"0000000001",   4, 63},
    {"00000000001",  4, 79},
    {"000000000001", 4, 95},
};

#define K_EOB 0xFE
#define K_ESC 0xFF

typedef struct { unsigned char len, idx; } Entry; /* len 0 = invalid */
static Entry s_tab[65536];
static unsigned short s_rl[111];
static int s_tab_ok;

static int add_code(int v, int len, int idx)
{
    int i, n = 1 << (16 - len);
    for (i = 0; i < n; i++) {
        Entry *e = &s_tab[(v << (16 - len)) | i];
        if (e->len)
            return 0; /* overlapping codes */
        e->len = (unsigned char)len;
        e->idx = (unsigned char)idx;
    }
    return 1;
}

static int bits_of(const char *s, int *len)
{
    int v = 0;
    *len = (int)strlen(s);
    for (; *s; s++)
        v = (v << 1) | (*s == '1');
    return v;
}

int lain_vlc_init(const unsigned short table[111])
{
    size_t g;
    int ok = 1, len, v, i, used = 0;
    long covered = 0;
    static unsigned char seen[111];

    memset(s_tab, 0, sizeof(s_tab));
    memset(seen, 0, sizeof(seen));
    v = bits_of("10", &len);
    ok &= add_code(v, len, K_EOB);
    v = bits_of("000001", &len);
    ok &= add_code(v, len, K_ESC);
    for (g = 0; g < sizeof(k_groups) / sizeof(k_groups[0]); g++) {
        int n = 1 << k_groups[g].bits;
        v = bits_of(k_groups[g].prefix, &len);
        for (i = 0; i < n; i++) {
            int idx = k_groups[g].base + i;
            if (g == 3 && i == 0)
                continue; /* "00100" belongs to the 8-bit group */
            ok &= add_code((v << k_groups[g].bits) | i, len + k_groups[g].bits, idx);
            ok &= idx < 111 && !seen[idx];
            if (idx < 111)
                seen[idx] = 1;
            used++;
        }
    }
    /* the code set is complete except for the unused 0000 0000 0000 xxxx */
    for (i = 0; i < 65536; i++)
        covered += s_tab[i].len != 0;
    ok &= used == 111 && covered == 65536 - 16;
    for (i = 0; i < 111; i++) {
        s_rl[i] = table[i];
        ok &= (table[i] & 0x3FF) != 0; /* every entry is a nonzero level */
    }
    s_tab_ok = ok;
    return ok;
}

typedef struct {
    const unsigned char *p, *end;
    unsigned int acc;  /* bits, left aligned */
    int n;             /* valid bits in acc */
} Bits;

static void fill(Bits *b)
{
    while (b->n <= 24) {
        unsigned int byte = b->p < b->end ? *b->p++ : 0;
        b->acc |= byte << (24 - b->n);
        b->n += 8;
    }
}

static unsigned int peek(Bits *b, int n)
{
    fill(b);
    return b->acc >> (32 - n);
}

static void skip(Bits *b, int n)
{
    b->acc <<= n;
    b->n -= n;
}

static unsigned int get(Bits *b, int n)
{
    unsigned int v = peek(b, n);
    skip(b, n);
    return v;
}

int lain_vlc_decode(const unsigned int *frame, int frame_bytes, unsigned int *out, int out_words)
{
    const unsigned char *hdr = (const unsigned char *)frame;
    unsigned short *o = (unsigned short *)(out + 1);
    unsigned short *o_end = (unsigned short *)(out + out_words);
    int qy = hdr[0], qc = hdr[1];
    int count = (int)frame[1], rounded, remaining, blk, i;
    Bits b;

    if (!s_tab_ok || (hdr[2] | hdr[3] << 8) != 0x3800 || count <= 0)
        return -1;
    rounded = (count + 63) & ~63;
    if (rounded + 2 > out_words * 2)
        return -1;
    out[0] = 0x38000000u | (unsigned int)(rounded / 2);

    b.p = hdr + 8;
    b.end = hdr + frame_bytes;
    b.acc = 0;
    b.n = 0;

    remaining = count;
    while (remaining > 0) {
        for (blk = 0; blk < 6; blk++) {
            int q = blk < 2 ? qc : qy;
            *o++ = (unsigned short)((q << 10) | get(&b, 10));
            remaining--;
            for (;;) {
                Entry e = s_tab[peek(&b, 16)];
                int run, level;
                if (o >= o_end || !e.len)
                    return -1; /* bad code / overflow */
                skip(&b, e.len);
                remaining--;
                if (e.idx == K_EOB) {
                    *o++ = 0xFE00;
                    break;
                }
                if (e.idx == K_ESC) {
                    run = (int)get(&b, 6);
                    level = (int)get(&b, 8);
                    if (level == 0)
                        level = (int)get(&b, 8);
                    else if (level == 0x80)
                        level = (int)get(&b, 8) - 256;
                    else if (level & 0x80)
                        level -= 256;
                } else {
                    run = s_rl[e.idx] >> 10;
                    level = s_rl[e.idx] & 0x3FF;
                    if (get(&b, 1))
                        level = -level;
                }
                *o++ = (unsigned short)((run << 10) | (level & 0x3FF));
            }
        }
    }
    if (remaining != 0)
        return -1; /* stream and header disagree */
    for (i = count; i < rounded; i++)
        *o++ = 0xFE00;
    return 1 + rounded / 2;
}
