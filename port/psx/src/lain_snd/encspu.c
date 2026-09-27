/* lain: EncSPU, 16-bit PCM to SPU-ADPCM encoder (libspu).
 *
 * Exhaustive search over the 5 prediction filters and 13 shift values per
 * 28-sample block, tracking the decoder's own history so the result decodes
 * exactly as encoded. Returns the encoded size in bytes, or -1.
 */
#include "psx/libspu.h"

#include <stdint.h>
#include <string.h>

static const int s_f0[5] = { 0, 60, 115, 98, 122 };
static const int s_f1[5] = { 0, 0, -52, -55, -60 };

static int16_t src_sample(const EncSPUEnv *env, int i)
{
    const uint8_t *p = (const uint8_t *)env->src + i * 2;
    if (env->byte_swap)
        return (int16_t)((p[0] << 8) | p[1]);
    return (int16_t)(p[0] | (p[1] << 8));
}

/* Encode 28 samples with a given filter/shift; returns squared error. */
static int64_t try_block(const int16_t *in, int filt, int shift, int32_t h1, int32_t h2,
                         uint8_t *nib, int32_t *oh1, int32_t *oh2)
{
    int64_t err = 0;
    int i;
    for (i = 0; i < 28; i++) {
        int32_t pred = (h1 * s_f0[filt] + h2 * s_f1[filt] + 32) >> 6;
        int32_t diff = in[i] - pred;
        /* quantise: value << 12 >> shift ~= diff  ->  value = diff * 2^shift / 4096 */
        int32_t q;
        int32_t s;
        int64_t scaled = (int64_t)diff * (1 << shift);
        q = (int32_t)((scaled + (scaled >= 0 ? 2048 : -2048)) / 4096);
        if (q > 7) q = 7;
        if (q < -8) q = -8;
        s = ((int16_t)(uint16_t)((q & 0x0F) << 12)) >> shift;
        s += pred;
        if (s > 32767) s = 32767;
        if (s < -32768) s = -32768;
        err += (int64_t)(in[i] - s) * (in[i] - s);
        if (nib)
            nib[i] = (uint8_t)(q & 0x0F);
        h2 = h1;
        h1 = s;
    }
    *oh1 = h1;
    *oh2 = h2;
    return err;
}

int EncSPU(EncSPUEnv *env)
{
    int nsamp, nblocks, b, i;
    int32_t h1 = 0, h2 = 0;
    uint8_t *out;
    int loop_block;

    if (!env || !env->src || !env->dest || env->size <= 0)
        return -1;

    nsamp = env->size / 2;
    nblocks = (nsamp + 27) / 28;
    out = (uint8_t *)env->dest;
    loop_block = env->loop ? (env->loop_start / 2) / 28 : -1;

    for (b = 0; b < nblocks; b++) {
        int16_t in[28];
        uint8_t nib[28];
        int best_f = 0, best_s = 0;
        int64_t best = -1;
        int32_t n1, n2;
        int f, s;
        uint8_t flags = 0;

        for (i = 0; i < 28; i++) {
            int k = b * 28 + i;
            in[i] = k < nsamp ? src_sample(env, k) : 0;
        }
        for (f = 0; f < 5; f++) {
            for (s = 0; s <= 12; s++) {
                int64_t e = try_block(in, f, s, h1, h2, NULL, &n1, &n2);
                if (best < 0 || e < best) {
                    best = e;
                    best_f = f;
                    best_s = s;
                }
            }
        }
        try_block(in, best_f, best_s, h1, h2, nib, &h1, &h2);

        if (b == loop_block)
            flags |= 4;
        if (b == nblocks - 1)
            flags |= env->loop ? 3 : 1;

        out[0] = (uint8_t)((best_f << 4) | best_s);
        out[1] = flags;
        for (i = 0; i < 14; i++)
            out[2 + i] = (uint8_t)(nib[i * 2] | (nib[i * 2 + 1] << 4));
        out += 16;
    }
    return nblocks * 16;
}
