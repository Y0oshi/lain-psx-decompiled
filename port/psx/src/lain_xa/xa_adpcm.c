/* lain: CD-XA ADPCM decoder and band-limited resampler.
 *
 * Written from the psx-spx "CDROM XA Audio ADPCM Compression" description:
 *   - a Form 2 audio sector carries 18 sound groups of 128 bytes after the
 *     24-byte sync/header/subheader;
 *   - each group has a 16-byte header (sound parameters: shift in bits 0-3,
 *     filter in bits 4-5; the 8 unit parameters sit at header bytes 4..11)
 *     and 28 32-bit words of sample data;
 *   - 4-bit mode: 8 sound units of 28 samples (unit n = nibble n of each
 *     word); 8-bit mode: 4 units (unit n = byte n of each word);
 *   - stereo: even units are left, odd units are right;
 *   - sample = (raw << 12 or << 8) >> shift, plus the prediction
 *     (old*K0 + older*K1 + 32) >> 6 with K0 = 0,60,115,98 and K1 = 0,0,-52,-55,
 *     clamped to 16 bits.
 *
 * The real SPU up-samples 37.8/18.9 kHz to 44.1 kHz with a 7-phase 29-tap
 * "zigzag" FIR. This uses an equivalent windowed-sinc polyphase interpolator
 * (Blackman window, 32 taps, cutoff just below the lower Nyquist).
 */
#include "lain_xa.h"

#include <math.h>
#include <string.h>

static const int s_K0[4] = { 0, 60, 115, 98 };
static const int s_K1[4] = { 0, 0, -52, -55 };

int LainXA_IsAudioSector(const u_char *raw)
{
    u_char submode = raw[18];
    /* bit 2 = audio, bit 5 = form 2 */
    return (submode & 0x04) && (submode & 0x20);
}

int LainXA_GetFormat(const u_char *raw, LainXAFormat *fmt)
{
    u_char coding;

    if (!LainXA_IsAudioSector(raw))
        return 0;

    coding = raw[19];
    fmt->file = raw[16];
    fmt->chan = raw[17];
    fmt->submode = raw[18];
    fmt->eof = (raw[18] & 0x80) != 0;
    fmt->channels = ((coding & 3) == 1) ? 2 : 1;
    fmt->rate = ((coding >> 2) & 3) == 1 ? 18900 : 37800;
    fmt->bits = ((coding >> 4) & 3) == 1 ? 8 : 4;
    return 1;
}

void LainXA_DecoderReset(LainXADecoder *d)
{
    memset(d, 0, sizeof(*d));
    d->lastCoding = -1;
}

int LainXA_DecodeSector(LainXADecoder *d, const u_char *raw, short *pcm, LainXAFormat *fmtOut)
{
    LainXAFormat fmt;
    const u_char *data;
    int units, g, u, j, frames;

    if (!LainXA_GetFormat(raw, &fmt))
        return 0;

    if (fmtOut)
        *fmtOut = fmt;

    /* A format change means a different stream: start from clean history. */
    if (d->lastCoding != raw[19])
    {
        memset(d->hist, 0, sizeof(d->hist));
        d->lastCoding = raw[19];
    }

    data = raw + 24;
    units = (fmt.bits == 8) ? 4 : 8;

    for (g = 0; g < 18; g++)
    {
        const u_char *grp = data + g * 128;

        for (u = 0; u < units; u++)
        {
            int param = grp[4 + u];
            int shift = param & 0x0F;
            int filter = (param >> 4) & 0x03;
            int ch, base, stride;
            int k0, k1;
            int *h;

            if (shift > 12)
                shift = 9;  /* reserved shift values behave like 9 on hardware */

            if (fmt.channels == 2)
            {
                ch = u & 1;
                base = (g * (units / 2) + (u >> 1)) * 28 * 2 + ch;
                stride = 2;
            }
            else
            {
                ch = 0;
                base = (g * units + u) * 28;
                stride = 1;
            }

            h = d->hist[ch];
            k0 = s_K0[filter];
            k1 = s_K1[filter];

            for (j = 0; j < 28; j++)
            {
                int s, v;

                if (fmt.bits == 8)
                {
                    s = (int)(signed char)grp[16 + j * 4 + u];
                    s = (s * 256) >> shift;         /* (byte << 8) >> shift */
                }
                else
                {
                    int b = grp[16 + j * 4 + (u >> 1)];
                    int nib = (u & 1) ? (b >> 4) : (b & 0x0F);
                    if (nib & 8)
                        nib -= 16;
                    s = (nib * 4096) >> shift;      /* (nibble << 12) >> shift */
                }

                v = s + ((h[0] * k0 + h[1] * k1 + 32) >> 6);
                if (v > 32767) v = 32767;
                if (v < -32768) v = -32768;

                h[1] = h[0];
                h[0] = v;
                pcm[base + j * stride] = (short)v;
            }
        }
    }

    frames = 18 * units * 28 / fmt.channels;
    return frames;
}

/* ------------------------------------------------------------------ */
/* Resampler                                                            */
/* ------------------------------------------------------------------ */

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

void LainXA_ResamplerInit(LainXAResampler *r, int inRate, int outRate, int channels)
{
    int p, k;
    double fc;
    const int half = LAIN_XA_RS_TAPS / 2;

    r->inRate = inRate;
    r->outRate = outRate;
    r->channels = channels;
    r->step = (double)inRate / (double)outRate;

    /* Cutoff relative to the input rate: just below the lower of the two
     * Nyquist frequencies, leaving room for the window's transition band. */
    fc = 0.5 * (outRate < inRate ? (double)outRate / inRate : 1.0) * 0.90;

    for (p = 0; p <= LAIN_XA_RS_PHASES; p++)
    {
        double f = (double)p / LAIN_XA_RS_PHASES;
        double sum = 0.0;

        for (k = 0; k < LAIN_XA_RS_TAPS; k++)
        {
            /* tap k sits at input index floor(t) - half + 1 + k; x = its distance to t */
            double x = (double)(k - half + 1) - f;
            double w, s, n;

            n = (x + half) / (double)LAIN_XA_RS_TAPS;   /* 0..1 across the window */
            if (n < 0.0 || n > 1.0)
                w = 0.0;
            else
                w = 0.42 - 0.5 * cos(2.0 * M_PI * n) + 0.08 * cos(4.0 * M_PI * n);

            if (fabs(x) < 1e-9)
                s = 2.0 * fc;
            else
                s = sin(2.0 * M_PI * fc * x) / (M_PI * x);

            r->table[p][k] = (float)(s * w);
            sum += s * w;
        }

        /* unity DC gain for every phase */
        for (k = 0; k < LAIN_XA_RS_TAPS; k++)
            r->table[p][k] = (float)(r->table[p][k] / sum);
    }

    memset(r->buf, 0, sizeof(r->buf));
    /* Prime with half a window of silence so the first real sample can be centred. */
    r->nbuf = half;
    r->pos = (double)half;
}

int LainXA_Resample(LainXAResampler *r, const short *in, int inFrames, short *out, int maxOut)
{
    const int half = LAIN_XA_RS_TAPS / 2;
    int ch, i, n = 0;
    int consumed;

    if (inFrames > LAIN_XA_MAX_FRAMES)
        inFrames = LAIN_XA_MAX_FRAMES;

    /* append input */
    for (i = 0; i < inFrames; i++)
        for (ch = 0; ch < r->channels; ch++)
            r->buf[ch][r->nbuf + i] = in[i * r->channels + ch];
    r->nbuf += inFrames;

    while (n < maxOut)
    {
        int i0 = (int)r->pos;
        double f = r->pos - i0;
        int start = i0 - half + 1;
        double pf;
        int p;
        float a;
        const float *t0, *t1;

        if (i0 + half >= r->nbuf)
            break;

        pf = f * LAIN_XA_RS_PHASES;
        p = (int)pf;
        a = (float)(pf - p);
        t0 = r->table[p];
        t1 = r->table[p + 1];

        for (ch = 0; ch < r->channels; ch++)
        {
            const float *x = &r->buf[ch][start];
            float acc = 0.0f;
            int k;

            for (k = 0; k < LAIN_XA_RS_TAPS; k++)
                acc += x[k] * (t0[k] + a * (t1[k] - t0[k]));

            if (acc > 32767.0f) acc = 32767.0f;
            if (acc < -32768.0f) acc = -32768.0f;
            out[n * r->channels + ch] = (short)lrintf(acc);
        }

        n++;
        r->pos += r->step;
    }

    /* drop input no longer needed by the window */
    consumed = (int)r->pos - half + 1;
    if (consumed > 0)
    {
        if (consumed > r->nbuf)
            consumed = r->nbuf;
        for (ch = 0; ch < r->channels; ch++)
            memmove(r->buf[ch], r->buf[ch] + consumed, (size_t)(r->nbuf - consumed) * sizeof(float));
        r->nbuf -= consumed;
        r->pos -= consumed;
    }

    return n;
}
