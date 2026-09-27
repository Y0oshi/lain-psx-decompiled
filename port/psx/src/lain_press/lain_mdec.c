/*
 * lain: software MDEC (PlayStation motion decoder) behind the libpress
 * DecDCT* API.
 *
 * Written from the public hardware description (psx-spx "MDEC"), not from
 * Sony code. The input is the MDEC "decode macroblock" command stream: one
 * command word followed by run-level halfwords (the output of a VLC decoder
 * such as DecDCTvlc or Lain's own func_8007038C):
 *
 *   command word  bits 31-29 = 1 (decode), 28-27 = depth (0 4bpp, 1 8bpp,
 *                 2 24bpp, 3 15bpp), 26 = signed output, 25 = set bit 15,
 *                 15-0 = number of 32-bit parameter words that follow
 *   per block     DC halfword  (qscale << 10) | dc10
 *                 AC halfwords (run << 10) | level10, ended by 0xFE00 (EOB);
 *                 0xFE00 at the start of a block is padding and is skipped
 *   colour MB     6 blocks: Cr, Cb, Y0 (top-left), Y1 (top-right),
 *                 Y2 (bottom-left), Y3 (bottom-right)
 *
 * Output is one macroblock after another, each in raster order: 16x16 pixels
 * (15bpp: 512 bytes, 24bpp: 768 bytes) for colour, 8x8 for monochrome. An STR
 * frame stores its macroblocks column by column, so a DecDCTout() of one
 * macroblock column (16 x height pixels) is a plain 16-pixel-wide strip ready
 * for LoadImage(), which is what the PsyQ movie player (and Lain) does.
 *
 * Timing model: decoding is synchronous. The DecDCTout() callback is not run
 * from inside DecDCTout() itself (the real MDEC finishes later, and players
 * start the next slice from the callback, which would otherwise recurse and
 * overwrite a buffer before it is uploaded). Instead it runs from a trampoline
 * at the outermost DecDCTin/DecDCTout call: a DecDCTout() issued from inside
 * the callback is decoded immediately and its callback runs once the current
 * one returns. The net effect for a player is "the whole frame is decoded and
 * all callbacks have fired by the time the first DecDCTout() returns".
 */
#include "psx/types.h"
#include "psx/libpress.h"

#include <math.h>
#include <string.h>

/* MPEG-1 default intra quantiser matrix, natural (row-major) order, with the
 * DC entry set to 2 as the PlayStation uses it. DecDCTReset() uploads this
 * for both luma and chroma. */
static const u_char k_default_qt[64] = {
     2, 16, 19, 22, 26, 27, 29, 34,
    16, 16, 22, 24, 27, 29, 34, 37,
    19, 22, 26, 27, 29, 34, 34, 38,
    22, 22, 26, 27, 29, 34, 37, 40,
    22, 26, 27, 29, 32, 35, 40, 48,
    26, 27, 29, 32, 35, 40, 48, 58,
    26, 27, 29, 34, 38, 46, 56, 69,
    27, 29, 35, 38, 46, 56, 69, 83,
};

static int   s_ready;
static u_char s_zigzag[64];      /* scan position -> natural index */
static float  s_idct[8][8];      /* [pixel][freq] = C(freq)/2 * cos(...) */
static int    s_qt_y[64], s_qt_c[64]; /* in scan order */

/* input */
static const u_short *s_in, *s_in_end;
static int s_depth = 3, s_signed, s_stp;

/* partially consumed output macroblock */
static u_char s_mb[16 * 16 * 3];
static int    s_mb_bytes, s_mb_pos;

/* callbacks / trampoline */
static void (*s_out_cb)(void);
static void (*s_in_cb)(void);
static int  s_cb_depth;
static int  s_out_pending, s_in_pending;

static void mdec_init_tables(void)
{
    int k = 0, x, u, s;

    /* zigzag: walk the anti-diagonals, alternating direction */
    for (s = 0; s < 15; s++) {
        int lo = s < 8 ? 0 : s - 7, hi = s < 8 ? s : 7, i;
        for (i = lo; i <= hi; i++) {
            int r = (s & 1) ? i : s - i; /* odd diagonals go down-left */
            s_zigzag[k++] = (u_char)(r * 8 + (s - r));
        }
    }
    for (x = 0; x < 8; x++)
        for (u = 0; u < 8; u++)
            s_idct[x][u] = (float)((u == 0 ? sqrt(0.5) : 1.0) * 0.5 *
                                   cos((2 * x + 1) * u * 3.14159265358979323846 / 16.0));
    for (k = 0; k < 64; k++)
        s_qt_y[k] = s_qt_c[k] = k_default_qt[s_zigzag[k]];
    s_ready = 1;
}

static int sext10(int v)
{
    v &= 0x3FF;
    return v >= 0x200 ? v - 0x400 : v;
}

static int clampi(int v, int lo, int hi)
{
    return v < lo ? lo : v > hi ? hi : v;
}

/* Decodes one 8x8 block into out[64] (signed samples, -128..127).
 * Returns 0 when the input ran out before the block started. */
static int mdec_block(int out[64], const int *qt)
{
    float coef[64], tmp[64];
    u_short n;
    int k, qs, val, x, y, u;

    do {
        if (s_in >= s_in_end)
            return 0;
        n = *s_in++;
    } while (n == 0xFE00);

    memset(coef, 0, sizeof(coef));
    qs = n >> 10;
    k = 0;
    val = qs ? sext10(n) * qt[0] : sext10(n) * 2;
    for (;;) {
        val = clampi(val, -0x400, 0x3FF);
        coef[qs ? s_zigzag[k] : k] = (float)val;
        if (s_in >= s_in_end)
            break;
        n = *s_in++;
        k += (n >> 10) + 1;
        if (k > 63)
            break;
        val = qs ? (sext10(n) * qt[k] * qs + 4) >> 3 : sext10(n) * 2;
    }

    /* separable IDCT: rows, then columns */
    for (y = 0; y < 8; y++)
        for (x = 0; x < 8; x++) {
            float acc = 0;
            for (u = 0; u < 8; u++)
                acc += s_idct[x][u] * coef[y * 8 + u];
            tmp[y * 8 + x] = acc;
        }
    for (x = 0; x < 8; x++)
        for (y = 0; y < 8; y++) {
            float acc = 0;
            for (u = 0; u < 8; u++)
                acc += s_idct[y][u] * tmp[u * 8 + x];
            out[y * 8 + x] = clampi((int)lrintf(acc), -128, 127);
        }
    return 1;
}

static void put_pixel(u_char *dst, int i, int r, int g, int b)
{
    if (!s_signed) {
        r += 128;
        g += 128;
        b += 128;
    } else {
        r &= 0xFF;
        g &= 0xFF;
        b &= 0xFF;
    }
    if (s_depth == 2) {
        dst[i * 3 + 0] = (u_char)r;
        dst[i * 3 + 1] = (u_char)g;
        dst[i * 3 + 2] = (u_char)b;
    } else {
        u_short p = (u_short)((r >> 3) | ((g >> 3) << 5) | ((b >> 3) << 10) | (s_stp ? 0x8000 : 0));
        dst[i * 2 + 0] = (u_char)p;
        dst[i * 2 + 1] = (u_char)(p >> 8);
    }
}

/* Decodes the next macroblock into s_mb. Out of input -> zero-filled. */
static void mdec_macroblock(void)
{
    int blk[6][64];
    int ok = 1, i, x, y;

    if (s_depth <= 1) { /* monochrome: one Y block, 8x8 */
        s_mb_bytes = s_depth == 1 ? 64 : 32;
        if (!mdec_block(blk[0], s_qt_y))
            memset(blk[0], 0, sizeof(blk[0]));
        for (i = 0; i < 64; i++) {
            int v = s_signed ? (blk[0][i] & 0xFF) : blk[0][i] + 128;
            if (s_depth == 1)
                s_mb[i] = (u_char)v;
            else if (i & 1)
                s_mb[i >> 1] |= (u_char)((v >> 4) << 4);
            else
                s_mb[i >> 1] = (u_char)(v >> 4);
        }
        s_mb_pos = 0;
        return;
    }

    s_mb_bytes = s_depth == 2 ? 16 * 16 * 3 : 16 * 16 * 2;
    for (i = 0; i < 6 && ok; i++)
        ok = mdec_block(blk[i], i < 2 ? s_qt_c : s_qt_y);
    if (!ok)
        memset(blk, 0, sizeof(blk));

    for (y = 0; y < 16; y++)
        for (x = 0; x < 16; x++) {
            int cr = blk[0][(y >> 1) * 8 + (x >> 1)];
            int cb = blk[1][(y >> 1) * 8 + (x >> 1)];
            int yy = blk[2 + (y >> 3) * 2 + (x >> 3)][(y & 7) * 8 + (x & 7)];
            int r = clampi(yy + (int)lrintf(1.402f * cr), -128, 127);
            int g = clampi(yy + (int)lrintf(-0.3437f * cb - 0.7143f * cr), -128, 127);
            int b = clampi(yy + (int)lrintf(1.772f * cb), -128, 127);
            put_pixel(s_mb, y * 16 + x, r, g, b);
        }
    s_mb_pos = 0;
}

/* Runs pending callbacks when called at the outermost level. */
static void mdec_trampoline(void)
{
    if (s_cb_depth)
        return;
    while (s_in_pending || s_out_pending) {
        void (*cb)(void);
        if (s_in_pending) {
            s_in_pending = 0;
            cb = s_in_cb;
        } else {
            s_out_pending = 0;
            cb = s_out_cb;
        }
        if (cb) {
            s_cb_depth++;
            cb();
            s_cb_depth--;
        }
    }
}

void DecDCTReset(int mode)
{
    if (!s_ready)
        mdec_init_tables();
    s_in = s_in_end = NULL;
    s_mb_pos = s_mb_bytes = 0;
    s_in_pending = s_out_pending = 0;
    if (mode == 0) { /* full reset: default tables, 15bpp */
        int k;
        for (k = 0; k < 64; k++)
            s_qt_y[k] = s_qt_c[k] = k_default_qt[s_zigzag[k]];
        s_depth = 3;
        s_signed = s_stp = 0;
    }
}

void DecDCTin(u_int *buf, int mode)
{
    u_int cmd;
    int depth;

    if (!s_ready)
        mdec_init_tables();
    cmd = buf[0];
    depth = (cmd >> 27) & 3;
    /* mode bit 0: 24bpp output (clears depth bit 27), else 15bpp;
     * mode bit 1: set bit 15 (STP) of every 15bpp pixel. */
    if (mode & 1)
        depth &= ~1;
    else
        depth |= 1;
    s_depth = depth;
    s_signed = (cmd >> 26) & 1;
    s_stp = (mode & 2) ? 1 : (int)((cmd >> 25) & 1);
    s_in = (const u_short *)(buf + 1);
    s_in_end = s_in + (cmd & 0xFFFF) * 2;
    s_mb_pos = s_mb_bytes = 0;

    s_in_pending = 1;
    mdec_trampoline();
}

void DecDCTout(u_int *buf, int size)
{
    u_char *dst = (u_char *)buf;
    int want = size * 4;

    if (!s_ready)
        mdec_init_tables();
    while (want > 0) {
        int n;
        if (s_mb_pos >= s_mb_bytes)
            mdec_macroblock();
        n = s_mb_bytes - s_mb_pos;
        if (n > want)
            n = want;
        memcpy(dst, s_mb + s_mb_pos, (size_t)n);
        s_mb_pos += n;
        dst += n;
        want -= n;
    }

    s_out_pending = 1;
    mdec_trampoline();
}

int DecDCTinSync(int mode)
{
    (void)mode;
    return 0; /* never busy */
}

int DecDCToutSync(int mode)
{
    (void)mode;
    return 0;
}

void DecDCTinCallback(void (*func)(void))
{
    s_in_cb = func;
}

void DecDCToutCallback(void (*func)(void))
{
    s_out_cb = func;
}
