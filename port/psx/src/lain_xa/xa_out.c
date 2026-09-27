/* lain: XA audio output = the "CD input" of the SPU.
 *
 * On the PS1 the CD controller decodes XA-ADPCM, up-samples it to 44.1 kHz
 * and streams it into the SPU, which scales it by the CD volume
 * (SpuSetCommonAttr cd.volume / cd.mix), mixes it with the voices and copies
 * it to the capture area at SPU RAM 0x000-0x7FF (SpuReadDecodedData, SPU IRQ).
 *
 * Here decoded sectors are resampled to 44.1 kHz stereo into a ring buffer,
 * which the software SPU (src/lain_snd) pulls from its audio callback through
 * LainSpu_SetCdSource(). The SPU therefore applies the CD volume, the capture
 * buffers and the IRQ exactly as for any other CD input, and the SPU's output
 * clock paces consumption (no drift between two audio clocks).
 *
 * The CD attenuator (CdMix), CdlMute and the player's volume setting are
 * applied here, before the SPU, as on the real drive.
 */
#include "lain_xa.h"
#include "lain_xa_internal.h"

#include "psx/libspu.h"

#include <SDL.h>
#include <string.h>

#define XA_RING_FRAMES      (1 << 17)   /* ~3 s at 44.1 kHz */
#define XA_OUT_MAX          10240       /* > 4032 * 44100 / 18900 */

typedef struct {
    SDL_mutex *mu;
    int registered;

    /* decoder / resampler (drive thread or FeedSector caller) */
    LainXADecoder dec;
    LainXAResampler rs;
    int rsValid;

    /* ring of resampled 44.1 kHz stereo */
    short ring[XA_RING_FRAMES * 2];
    unsigned int rd, wr;

    int playing;            /* 0 = prebuffering */
    Uint64 firstData;       /* when data first arrived while prebuffering (0 = none) */
    int prebufMs;

    long long played;       /* frames handed to the SPU since the last flush */

    float userGain;
    int muted;
    int atv[4];             /* L->L, L->R, R->L, R->R; 0x80 = 1.0 */

    LainXAStats stats;
} XAOut;

static XAOut s_xa;
static short s_decPcm[LAIN_XA_MAX_FRAMES * 2];
static short s_rsPcm[XA_OUT_MAX * 2];

static int xa_pull(short *stereo, int frames, void *user);

static void xa_init(void)
{
    static SDL_SpinLock initLock = 0;

    if (s_xa.registered)
        return;

    SDL_AtomicLock(&initLock);
    if (!s_xa.registered)
    {
        s_xa.mu = SDL_CreateMutex();
        s_xa.userGain = 1.0f;
        s_xa.atv[0] = s_xa.atv[3] = 0x80;
        s_xa.atv[1] = s_xa.atv[2] = 0;
        s_xa.prebufMs = 60;
        LainXA_DecoderReset(&s_xa.dec);

        /* This is the SPU's CD input. (The STR streamer's default XA sink,
         * src/lain_press, calls LainXA_FeedSector itself.) */
        LainSpu_SetCdSource(xa_pull, NULL);

        s_xa.registered = 1;
    }
    SDL_AtomicUnlock(&initLock);
}

/* ------------------------------------------------------------------ */
/* SPU side (audio thread)                                              */
/* ------------------------------------------------------------------ */

/* lain: dub replacement source (see LainXA_SetReplacement). */
static LainXAReplaceFn s_repl_fn;
static void *s_repl_user;

void LainXA_SetReplacement(LainXAReplaceFn fn, void *user)
{
    SDL_LockMutex(s_xa.mu);
    s_repl_fn = fn;
    s_repl_user = user;
    SDL_UnlockMutex(s_xa.mu);
}

static int xa_pull(short *out, int frames, void *user)
{
    int n = 0, i;
    unsigned int avail;
    int g0, g1, g2, g3;
    float gain;

    (void)user;

    SDL_LockMutex(s_xa.mu);

    avail = s_xa.wr - s_xa.rd;

    if (!s_xa.playing)
    {
        /* Prebuffer: sectors arrive in bursts (one sector = up to 213 ms of
         * audio at once), so start a little after the first one. */
        if (avail > 0)
        {
            Uint64 now = SDL_GetPerformanceCounter();
            if (s_xa.firstData == 0)
                s_xa.firstData = now;
            if ((now - s_xa.firstData) * 1000 >= (Uint64)s_xa.prebufMs * SDL_GetPerformanceFrequency() ||
                avail >= (unsigned int)(LAIN_XA_OUT_RATE / 2))
            {
                s_xa.playing = 1;
                s_xa.firstData = 0;
            }
        }
    }

    if (s_xa.playing)
    {
        g0 = s_xa.atv[0]; g1 = s_xa.atv[1]; g2 = s_xa.atv[2]; g3 = s_xa.atv[3];
        gain = s_xa.muted ? 0.0f : s_xa.userGain;

        n = frames < (int)avail ? frames : (int)avail;

        for (i = 0; i < n; i++)
        {
            unsigned int r = (s_xa.rd + i) & (XA_RING_FRAMES - 1);
            int l = s_xa.ring[r * 2];
            int rr = s_xa.ring[r * 2 + 1];
            float ol = (float)((l * g0 + rr * g2) >> 7) * gain;
            float or_ = (float)((l * g1 + rr * g3) >> 7) * gain;

            if (ol > 32767.f) ol = 32767.f; else if (ol < -32768.f) ol = -32768.f;
            if (or_ > 32767.f) or_ = 32767.f; else if (or_ < -32768.f) or_ = -32768.f;
            out[i * 2] = (short)ol;
            out[i * 2 + 1] = (short)or_;
        }

        s_xa.rd += (unsigned int)n;
        s_xa.played += n;

        if (n < frames)
        {
            /* starved: the stream ended or the feeder is late */
            s_xa.playing = 0;
            s_xa.firstData = 0;
            s_xa.stats.underruns++;
        }
    }

    LainXAReplaceFn repl = s_repl_fn;
    void *repl_user = s_repl_user;
    int rg0 = s_xa.atv[0], rg1 = s_xa.atv[1], rg2 = s_xa.atv[2], rg3 = s_xa.atv[3];
    float rgain = s_xa.muted ? 0.0f : s_xa.userGain;
    SDL_UnlockMutex(s_xa.mu);

    /* lain: a dub replaces the disc audio sample for sample: the XA stream above
     * still paces playback (and the level meter), only the samples change. Frames
     * past the end of the dub are silent. Called without the lock held. */
    if (repl && n > 0)
    {
        short dub[2048];
        int done = 0;
        while (done < n)
        {
            int want = n - done > 1024 ? 1024 : n - done;
            int got = repl(dub, want, repl_user);
            for (i = 0; i < want; i++)
            {
                int l = i < got ? dub[i * 2] : 0, r = i < got ? dub[i * 2 + 1] : 0;
                float ol = (float)((l * rg0 + r * rg2) >> 7) * rgain;
                float or_ = (float)((l * rg1 + r * rg3) >> 7) * rgain;
                if (ol > 32767.f) ol = 32767.f; else if (ol < -32768.f) ol = -32768.f;
                if (or_ > 32767.f) or_ = 32767.f; else if (or_ < -32768.f) or_ = -32768.f;
                out[(done + i) * 2] = (short)ol;
                out[(done + i) * 2 + 1] = (short)or_;
            }
            done += want;
        }
    }

    /* the SPU fills the rest with silence */
    return n;
}

/* ------------------------------------------------------------------ */
/* Feeding                                                              */
/* ------------------------------------------------------------------ */

static int xa_feed(const u_char *raw)
{
    LainXAFormat fmt;
    int frames, outFrames, i;
    unsigned int space;

    if (!LainXA_IsAudioSector(raw))
        return 0;

    xa_init();

    SDL_LockMutex(s_xa.mu);

    frames = LainXA_DecodeSector(&s_xa.dec, raw, s_decPcm, &fmt);
    if (frames <= 0)
    {
        SDL_UnlockMutex(s_xa.mu);
        return 0;
    }

    if (!s_xa.rsValid || s_xa.rs.inRate != fmt.rate || s_xa.rs.channels != fmt.channels)
    {
        LainXA_ResamplerInit(&s_xa.rs, fmt.rate, LAIN_XA_OUT_RATE, fmt.channels);
        s_xa.rsValid = 1;
    }

    outFrames = LainXA_Resample(&s_xa.rs, s_decPcm, frames, s_rsPcm, XA_OUT_MAX);

    space = XA_RING_FRAMES - (s_xa.wr - s_xa.rd);
    if ((unsigned int)outFrames > space)
    {
        /* nobody is pulling (no SPU output) or the feeder runs ahead */
        s_xa.stats.sectorsDropped++;
        SDL_UnlockMutex(s_xa.mu);
        return 0;
    }

    for (i = 0; i < outFrames; i++)
    {
        unsigned int w = (s_xa.wr + i) & (XA_RING_FRAMES - 1);
        if (fmt.channels == 2)
        {
            s_xa.ring[w * 2] = s_rsPcm[i * 2];
            s_xa.ring[w * 2 + 1] = s_rsPcm[i * 2 + 1];
        }
        else
        {
            /* mono XA drives both CD inputs */
            s_xa.ring[w * 2] = s_xa.ring[w * 2 + 1] = s_rsPcm[i];
        }
    }
    s_xa.wr += (unsigned int)outFrames;

    s_xa.stats.sectorsDecoded++;
    s_xa.stats.framesQueued += outFrames;

    SDL_UnlockMutex(s_xa.mu);
    return outFrames;
}

int LainXA_FeedSector(const u_char *raw2352)
{
    return xa_feed(raw2352);
}

int LainXA_FeedSectorFiltered(const u_char *raw2352, int file, int chan)
{
    if (raw2352[16] != file || raw2352[17] != chan)
        return 0;
    return xa_feed(raw2352);
}

void LainXA_Flush(void)
{
    xa_init();
    SDL_LockMutex(s_xa.mu);
    s_xa.rd = s_xa.wr = 0;
    s_xa.playing = 0;
    s_xa.firstData = 0;
    s_xa.played = 0;
    s_xa.rsValid = 0;
    LainXA_DecoderReset(&s_xa.dec);
    SDL_UnlockMutex(s_xa.mu);
}

double LainXA_GetQueuedSeconds(void)
{
    unsigned int n;
    xa_init();
    SDL_LockMutex(s_xa.mu);
    n = s_xa.wr - s_xa.rd;
    SDL_UnlockMutex(s_xa.mu);
    return (double)n / LAIN_XA_OUT_RATE;
}

long long LainXA_GetPlayedFrames(void)
{
    long long p;
    xa_init();
    SDL_LockMutex(s_xa.mu);
    p = s_xa.played;
    SDL_UnlockMutex(s_xa.mu);
    return p;
}

int LainXA_IsPlaying(void)
{
    int p;
    xa_init();
    SDL_LockMutex(s_xa.mu);
    p = s_xa.playing;
    SDL_UnlockMutex(s_xa.mu);
    return p;
}

void LainXA_SetUserVolume(float gain)
{
    xa_init();
    SDL_LockMutex(s_xa.mu);
    s_xa.userGain = gain < 0.0f ? 0.0f : gain;
    SDL_UnlockMutex(s_xa.mu);
}

void LainXA_SetAttenuator(const CdlATV *atv)
{
    xa_init();
    SDL_LockMutex(s_xa.mu);
    s_xa.atv[0] = atv->val0;
    s_xa.atv[1] = atv->val1;
    s_xa.atv[2] = atv->val2;
    s_xa.atv[3] = atv->val3;
    SDL_UnlockMutex(s_xa.mu);
}

void LainXA_SetMute(int on)
{
    xa_init();
    SDL_LockMutex(s_xa.mu);
    s_xa.muted = on ? 1 : 0;
    SDL_UnlockMutex(s_xa.mu);
}

void LainXA_SetPrebufferMs(int ms)
{
    xa_init();
    SDL_LockMutex(s_xa.mu);
    s_xa.prebufMs = ms < 0 ? 0 : ms;
    SDL_UnlockMutex(s_xa.mu);
}

void LainXA_GetStats(LainXAStats *st)
{
    xa_init();
    SDL_LockMutex(s_xa.mu);
    *st = s_xa.stats;
    SDL_UnlockMutex(s_xa.mu);
}

int LainXA_Pull(short *stereo, int frames)
{
    int n;
    xa_init();
    n = xa_pull(stereo, frames, NULL);
    if (n < frames)
        memset(stereo + n * 2, 0, (size_t)(frames - n) * 4);
    return n;
}

void LainXA_Shutdown(void)
{
    if (!s_xa.registered)
        return;
    LainSpu_SetCdSource(NULL, NULL);
    LainXA_Flush();
}
