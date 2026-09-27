/* Standalone test for the port's CD-XA ADPCM audio (port/psx/src/lain_xa).
 *
 *   xa_decode [--xa FILE] [--chan N] [--out DIR]
 *       Decodes channel N (default 0) of an XA file (default
 *       extract/disc1/XA/LAIN01.XA) straight through the decoder, writes the
 *       native-rate WAV and the 44.1 kHz resampled WAV, prints stats and a
 *       plausibility verdict (level, clipping, silence, noise-likeness).
 *   xa_decode --drive [SECONDS]  (default 6)
 *       Plays the same channel through libcd the way Lain does (Setloc +
 *       SeekP, Setfilter, Setmode 0xC8, ReadS, GetlocP polling, Pause), with
 *       the XA file as the "disc". Audio is pulled offline at 44.1 kHz in real
 *       time; checks pacing (~150 sectors/s), that data sectors reach the
 *       ready callback, and that the output is bit-exact with the direct decode.
 *   xa_decode --live [SECONDS]  (default 8)
 *       Same libcd sequence, played audibly through the software SPU, with
 *       the game's SPU-IRQ level meter (SpuReadDecodedData).
 *
 * Reads the user's own disc dump at run time; writes under the build dir.
 */
#include "lain_xa.h"
#include "psx/libcd.h"
#include "psx/libspu.h"

#include <SDL.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#ifndef LAIN_REPO_ROOT
#define LAIN_REPO_ROOT "."
#endif
#ifndef XA_DECODE_OUT
#define XA_DECODE_OUT "out"
#endif

static u_char *g_file;
static long g_nsec;
static int g_base;          /* disc LBA of the file's first sector (from its header) */

/* ------------------------------------------------------------------ */

static int load_file(const char *path)
{
    FILE *f = fopen(path, "rb");
    long sz;

    if (!f)
    {
        fprintf(stderr, "cannot open %s (needs the extracted disc: extract/disc1/XA)\n", path);
        return 0;
    }
    fseek(f, 0, SEEK_END);
    sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    g_file = (u_char *)malloc((size_t)sz);
    if (!g_file || fread(g_file, 1, (size_t)sz, f) != (size_t)sz)
    {
        fprintf(stderr, "read failed: %s\n", path);
        fclose(f);
        return 0;
    }
    fclose(f);
    g_nsec = sz / LAIN_XA_RAW_SECTOR;
    /* the sectors keep their disc address: serve them at their real LBA */
    g_base = g_nsec ? CdPosToInt((CdlLOC *)(g_file + 12)) : 0;
    return 1;
}

static int file_reader(int lba, u_char *raw, void *user)
{
    (void)user;
    lba -= g_base;
    if (lba < 0 || lba >= g_nsec)
        return 0;
    memcpy(raw, g_file + (size_t)lba * LAIN_XA_RAW_SECTOR, LAIN_XA_RAW_SECTOR);
    return 1;
}

static void put32(FILE *f, unsigned v) { fputc(v & 255, f); fputc((v >> 8) & 255, f); fputc((v >> 16) & 255, f); fputc(v >> 24, f); }
static void put16(FILE *f, unsigned v) { fputc(v & 255, f); fputc((v >> 8) & 255, f); }

static int write_wav(const char *path, const short *pcm, long frames, int channels, int rate)
{
    FILE *f = fopen(path, "wb");
    unsigned bytes = (unsigned)(frames * channels * 2);
    long i;

    if (!f)
    {
        fprintf(stderr, "cannot write %s\n", path);
        return 0;
    }
    fwrite("RIFF", 1, 4, f); put32(f, 36 + bytes);
    fwrite("WAVEfmt ", 1, 8, f); put32(f, 16); put16(f, 1); put16(f, (unsigned)channels);
    put32(f, (unsigned)rate); put32(f, (unsigned)(rate * channels * 2)); put16(f, (unsigned)(channels * 2)); put16(f, 16);
    fwrite("data", 1, 4, f); put32(f, bytes);
    for (i = 0; i < frames * channels; i++)
        put16(f, (unsigned short)pcm[i]);
    fclose(f);
    return 1;
}

typedef struct {
    double rms_db, peak_db, silent_pct, clip_pct, diff_ratio, zcr;
} Stats;

/* stats on channel 0 of interleaved PCM */
static Stats analyze(const short *pcm, long frames, int channels, int rate)
{
    Stats st;
    double sum = 0, dsum = 0;
    long i, clip = 0, zc = 0, win, silentWins = 0, nWins = 0;
    int peak = 0;
    int w = rate / 50;  /* 20 ms windows */

    memset(&st, 0, sizeof(st));
    if (frames < 2)
        return st;

    for (i = 0; i < frames; i++)
    {
        int s = pcm[i * channels];
        int a = s < 0 ? -s : s;
        sum += (double)s * s;
        if (a > peak) peak = a;
        if (a >= 32767) clip++;
        if (i > 0)
        {
            int p = pcm[(i - 1) * channels];
            dsum += (double)(s - p) * (s - p);
            if ((s >= 0) != (p >= 0)) zc++;
        }
    }

    for (win = 0; win + w <= frames; win += w)
    {
        double e = 0;
        for (i = win; i < win + w; i++)
            e += (double)pcm[i * channels] * pcm[i * channels];
        e = sqrt(e / w);
        if (e < 32768.0 * 0.001)    /* below -60 dBFS */
            silentWins++;
        nWins++;
    }

    st.rms_db = 20.0 * log10(sqrt(sum / frames) / 32768.0 + 1e-12);
    st.peak_db = 20.0 * log10(peak / 32768.0 + 1e-12);
    st.clip_pct = 100.0 * clip / frames;
    st.silent_pct = nWins ? 100.0 * silentWins / nWins : 0;
    st.diff_ratio = sum > 0 ? sqrt(dsum / sum) : 0;   /* white noise ~1.41, speech << 1 */
    st.zcr = (double)zc / frames * rate;              /* zero crossings per second */
    return st;
}

static void print_stats(const char *label, const Stats *st)
{
    printf("  %-9s RMS %6.1f dBFS  peak %6.1f dBFS  clipped %.3f%%  silent %4.1f%%  diff/RMS %.3f  ZCR %6.0f/s\n",
           label, st->rms_db, st->peak_db, st->clip_pct, st->silent_pct, st->diff_ratio, st->zcr);
}

static int plausible(const Stats *st)
{
    return st->rms_db > -45.0 && st->rms_db < -3.0 &&
           st->clip_pct < 0.1 &&
           st->silent_pct < 90.0 &&
           st->diff_ratio < 1.0;
}

/* ------------------------------------------------------------------ */

static short *g_ref;         /* direct-decode reference at 44.1 kHz (mono -> stereo) */
static long g_refFrames;

static int decode_channel(int chan, const char *outDir, const char *base)
{
    static LainXADecoder dec;
    static LainXAResampler rs;
    static short pcm[LAIN_XA_MAX_FRAMES * 2];
    static short rsOut[10240 * 2];
    short *nat = NULL, *res = NULL;
    long natFrames = 0, resFrames = 0, natCap = 0, resCap = 0;
    long i, nAudio = 0, nOther = 0, nEof = 0;
    LainXAFormat fmt0;
    int haveFmt = 0;
    char path[1024];
    Stats sn, sr;
    int ok;

    LainXA_DecoderReset(&dec);

    for (i = 0; i < g_nsec; i++)
    {
        const u_char *raw = g_file + (size_t)i * LAIN_XA_RAW_SECTOR;
        LainXAFormat fmt;
        int n, m;

        if (!LainXA_GetFormat(raw, &fmt))
        {
            nOther++;
            continue;
        }
        if (fmt.file != 1 || fmt.chan != chan)
            continue;

        if (!haveFmt)
        {
            fmt0 = fmt;
            haveFmt = 1;
            LainXA_ResamplerInit(&rs, fmt.rate, LAIN_XA_OUT_RATE, fmt.channels);
        }
        if (fmt.eof)
            nEof++;

        n = LainXA_DecodeSector(&dec, raw, pcm, &fmt);
        nAudio++;

        if (natFrames + n > natCap)
        {
            natCap = (natFrames + n) * 2;
            nat = (short *)realloc(nat, (size_t)natCap * fmt0.channels * 2);
        }
        memcpy(nat + natFrames * fmt0.channels, pcm, (size_t)n * fmt0.channels * 2);
        natFrames += n;

        m = LainXA_Resample(&rs, pcm, n, rsOut, 10240);
        if (resFrames + m > resCap)
        {
            resCap = (resFrames + m) * 2;
            res = (short *)realloc(res, (size_t)resCap * fmt0.channels * 2);
        }
        memcpy(res + resFrames * fmt0.channels, rsOut, (size_t)m * fmt0.channels * 2);
        resFrames += m;
    }

    if (!haveFmt)
    {
        fprintf(stderr, "channel %d: no audio sectors\n", chan);
        return 0;
    }

    printf("channel %d: %ld audio sectors (%ld with EOF), %ld non-audio sectors in file of %ld\n",
           chan, nAudio, nEof, nOther, g_nsec);
    printf("  format    %d-bit %s, %d Hz\n", fmt0.bits, fmt0.channels == 2 ? "stereo" : "mono", fmt0.rate);
    printf("  duration  %.2f s  (%ld frames @ %d Hz; %ld frames @ 44100 Hz)\n",
           (double)natFrames / fmt0.rate, natFrames, fmt0.rate, resFrames);

    sn = analyze(nat, natFrames, fmt0.channels, fmt0.rate);
    sr = analyze(res, resFrames, fmt0.channels, LAIN_XA_OUT_RATE);
    print_stats("native", &sn);
    print_stats("44.1k", &sr);

    mkdir(outDir, 0755);
    snprintf(path, sizeof(path), "%s/%s_ch%02d_%d.wav", outDir, base, chan, fmt0.rate);
    write_wav(path, nat, natFrames, fmt0.channels, fmt0.rate);
    printf("  wrote     %s\n", path);
    snprintf(path, sizeof(path), "%s/%s_ch%02d_44100.wav", outDir, base, chan);
    write_wav(path, res, resFrames, fmt0.channels, LAIN_XA_OUT_RATE);
    printf("  wrote     %s\n", path);

    /* keep a stereo reference for the drive test */
    g_ref = (short *)malloc((size_t)resFrames * 4);
    for (i = 0; i < resFrames; i++)
    {
        if (fmt0.channels == 2)
        {
            g_ref[i * 2] = res[i * 2];
            g_ref[i * 2 + 1] = res[i * 2 + 1];
        }
        else
            g_ref[i * 2] = g_ref[i * 2 + 1] = res[i];
    }
    g_refFrames = resFrames;

    ok = plausible(&sn) && plausible(&sr);
    printf("  verdict   %s\n", ok ? "PLAUSIBLE (speech-like level/spectrum, not silence, not noise)"
                                 : "NOT PLAUSIBLE");
    free(nat);
    free(res);
    return ok;
}

/* ------------------------------------------------------------------ */
/* libcd path                                                           */
/* ------------------------------------------------------------------ */

static volatile int g_dataSectors, g_dataBadHeader, g_syncCallbacks;

static void ready_cb(u_char intr, u_char *result)
{
    u_int hdr[3];
    (void)result;
    if (intr != CdlDataReady)
        return;
    /* Lain's loader reads the 12-byte header+subheader first (Size1 mode);
     * in Size-less mode the buffer starts at the user data. Only counted here. */
    CdGetSector(hdr, 3);
    g_dataSectors++;
}

static void sync_cb(u_char intr, u_char *result)
{
    (void)intr;
    (void)result;
    g_syncCallbacks++;
}

/* SPU IRQ level meter, as Lain's spu_irq_read_decoded / spu_decoded_xfer_done / spu_decoded_peak_level6 */
static SpuDecodedData g_dec;
static volatile unsigned g_irqAddr = 0x100;
static volatile int g_irqCount;
static void meter_irq(void)
{
    SpuSetIRQ(SPU_OFF);
    SpuReadDecodedData(&g_dec, SPU_CDONLY);
    g_irqCount++;
}
static void meter_transfer(void)
{
    g_irqAddr = g_irqAddr == 0 ? 0x200 : 0;
    SpuSetIRQAddr(g_irqAddr);
    SpuSetIRQ(SPU_ON);
}
static int meter_peak(void)
{
    int i, start = g_irqAddr == 0 ? 0 : 0x100, peak = 0;
    for (i = start; i < start + 0x100 - 50; i++)
    {
        int v = g_dec.cd_left[i];
        if (v < 0) v = -v;
        if (v > peak) peak = v;
    }
    return peak;
}

static int run_drive(int chan, double seconds, int live, const char *outDir, const char *base)
{
    CdlLOC loc;
    u_char filter[4], param[4], result[8];
    Uint64 t0, freq = SDL_GetPerformanceFrequency();
    double lastPoll = -1, lastPrint = -1;
    int firstPos = -1, lastPos = -1, polls = 0, ok = 1;
    double firstT = 0, lastT = 0;
    short *got = NULL;
    long gotFrames = 0, gotCap = 0;
    char path[1024];

    LainCD_SetSectorReader(file_reader, NULL);
    CdInit();

    if (live)
    {
        SpuCommonAttr attr;
        SpuInit();
        /* spu_cd_audio_on: master and CD volume 0x3FFF, CD mix on */
        memset(&attr, 0, sizeof(attr));
        attr.mask = SPU_COMMON_MVOLL | SPU_COMMON_MVOLR | SPU_COMMON_CDVOLL | SPU_COMMON_CDVOLR | SPU_COMMON_CDMIX;
        attr.mvol.left = attr.mvol.right = 0x3FFF;
        attr.cd.volume.left = attr.cd.volume.right = 0x3FFF;
        attr.cd.mix = SPU_ON;
        SpuSetCommonAttr(&attr);
        SpuSetTransferCallback(meter_transfer);
        SpuSetIRQCallback(meter_irq);
        SpuSetIRQAddr(g_irqAddr);
        SpuSetIRQ(SPU_ON);
    }

    CdReadyCallback(ready_cb);
    CdSyncCallback(sync_cb);

    /* xa_seek_file: Setloc + SeekP to the file start */
    CdIntToPos(g_base, &loc);
    CdControl(CdlSetloc, (u_char *)&loc, 0);
    CdControl(CdlSeekP, (u_char *)&loc, 0);
    /* xa_play_channel: filter (file 1, channel) */
    filter[0] = 1;
    filter[1] = (u_char)chan;
    CdControl(CdlSetfilter, filter, 0);
    /* xa_play_range: Setmode double speed | RT | SF, then ReadS */
    param[0] = CdlModeSpeed | CdlModeRT | CdlModeSF;
    CdControlB(CdlSetmode, param, 0);
    CdControl(CdlReadS, 0, 0);

    t0 = SDL_GetPerformanceCounter();
    for (;;)
    {
        double t = (double)(SDL_GetPerformanceCounter() - t0) / freq;
        if (t >= seconds)
            break;

        if (!live)
        {
            /* consume like the SPU would: 44.1 kHz in real time */
            static long pulled = 0;
            long want = (long)(t * LAIN_XA_OUT_RATE) - pulled;
            if (want > 0)
            {
                short tmp[4096 * 2];
                int n;
                if (want > 4096) want = 4096;
                n = LainXA_Pull(tmp, (int)want);
                pulled += want;
                if (n > 0)
                {
                    if (gotFrames + n > gotCap)
                    {
                        gotCap = (gotFrames + n) * 2 + 65536;
                        got = (short *)realloc(got, (size_t)gotCap * 4);
                    }
                    memcpy(got + gotFrames * 2, tmp, (size_t)n * 4);
                    gotFrames += n;
                }
            }
        }

        /* xa_update: every 4 vsyncs, CdSync(1) and GetlocP */
        if (t - lastPoll >= 4.0 / 60.0)
        {
            int st;
            lastPoll = t;
            memset(result, 0, sizeof(result));
            st = CdSync(1, result);
            if (st == CdlComplete)
            {
                if (CdLastCom() == CdlGetlocP)
                {
                    int pos = CdPosToInt((CdlLOC *)&result[5]);
                    if (firstPos < 0) { firstPos = pos; firstT = t; }
                    lastPos = pos;
                    lastT = t;
                    polls++;
                }
                CdControlF(CdlGetlocP, 0);
            }
        }

        if (t - lastPrint >= 0.5)
        {
            lastPrint = t;
            if (live)
                printf("  t=%4.1fs  pos %5d  queued %.2fs  meter peak %5d (irq %d)\n",
                       t, lastPos, LainXA_GetQueuedSeconds(), meter_peak(), g_irqCount);
            else
                printf("  t=%4.1fs  pos %5d  queued %.2fs  pulled %ld frames\n",
                       t, lastPos, LainXA_GetQueuedSeconds(), gotFrames);
            fflush(stdout);
        }

        SDL_Delay(2);
    }

    CdControlB(CdlPause, 0, 0);

    {
        LainXAStats xs;
        double rate = lastT > firstT ? (lastPos - firstPos) / (lastT - firstT) : 0;
        LainXA_GetStats(&xs);
        printf("drive: GetlocP went %d..%d in %.2f s = %.1f sectors/s (expect 150 at 2x)\n",
               firstPos, lastPos, lastT - firstT, rate);
        printf("drive: %d GetlocP polls, %d sync callbacks, %d data sectors to the ready callback\n",
               polls, g_syncCallbacks, g_dataSectors);
        printf("xa:    %lld sectors decoded, %lld dropped, %lld starvations, %lld frames queued\n",
               xs.sectorsDecoded, xs.sectorsDropped, xs.underruns, xs.framesQueued);
        if (rate < 140 || rate > 160)
            ok = 0;
        if (xs.sectorsDecoded < (long long)(seconds * 150 / 32) - 2)
            ok = 0;
    }

    if (!live)
    {
        long cmp = gotFrames < g_refFrames ? gotFrames : g_refFrames, i, diff = 0;
        for (i = 0; i < cmp * 2; i++)
            if (got[i] != g_ref[i])
                diff++;
        printf("drive: pulled %ld frames (%.2f s of audio in %.1f s); %ld/%ld samples differ from direct decode\n",
               gotFrames, (double)gotFrames / LAIN_XA_OUT_RATE, seconds, diff, cmp * 2);
        if (diff != 0 || gotFrames < (long)((seconds - 0.5) * LAIN_XA_OUT_RATE))
            ok = 0;
        snprintf(path, sizeof(path), "%s/%s_ch%02d_drive.wav", outDir, base, chan);
        write_wav(path, got, gotFrames, 2, LAIN_XA_OUT_RATE);
        printf("  wrote     %s\n", path);
    }
    else
    {
        SDL_Delay(400);  /* let the tail play */
        if (g_irqCount == 0)
            printf("live:  warning: SPU IRQ meter never fired\n");
    }

    printf("  verdict   %s\n", ok ? "OK" : "FAILED");
    free(got);
    return ok;
}

/* ------------------------------------------------------------------ */
/* Lain's file loader (cd_sync_callback / cd_ready_callback) against the drive */
/* ------------------------------------------------------------------ */

static volatile int g_ldState, g_ldDone, g_ldError, g_ldPrev, g_ldFirst, g_ldRemain;
static u_char *g_ldDest;

static void loader_ready(u_char intr, u_char *result)
{
    u_int header[3];
    int sector;
    (void)result;

    if (intr != CdlDataReady)
    {
        g_ldError = 1;
        CdReadyCallback(NULL);
        CdControlF(CdlPause, NULL);
        return;
    }
    CdGetSector(header, 3);     /* Size1: header + subheader first */
    sector = CdPosToInt((CdlLOC *)header);
    if (g_ldPrev < 0 ? sector != g_ldFirst : sector != g_ldPrev + 1)
        g_ldError = 1;
    g_ldPrev = sector;
    CdGetSector(g_ldDest, 0x200);
    g_ldDest += 0x800;
    if (--g_ldRemain == 0 || g_ldError)
    {
        CdReadyCallback(NULL);
        CdControlF(CdlPause, NULL);
    }
}

static void loader_sync(u_char intr, u_char *result)
{
    u_char param[4];
    (void)result;

    if (intr != CdlComplete)
    {
        g_ldError = 1;
        return;
    }
    switch (g_ldState)
    {
    case 1:
        param[0] = CdlModeSpeed | CdlModeSize1;     /* 0xA0 */
        CdControlF(CdlSetmode, param);
        g_ldState = 2;
        break;
    case 2:
        CdReadyCallback(loader_ready);
        CdControlF(CdlReadN, NULL);
        g_ldState = 3;
        break;
    case 3:
        g_ldState = 4;
        break;
    case 4:
        /* the Pause issued by the ready callback completed */
        g_ldDone = 1;
        CdSyncCallback(NULL);
        g_ldState = 0;
        break;
    }
}

static int run_loader(int startIdx, int count, int turbo)
{
    CdlLOC loc;
    u_char *buf = (u_char *)malloc((size_t)count * 0x800);
    Uint64 t0 = SDL_GetPerformanceCounter();
    double t;
    int i, bad = 0;

    LainCD_SetDataTurbo(turbo);
    g_ldDest = buf;
    g_ldRemain = count;
    g_ldPrev = -1;
    g_ldFirst = g_base + startIdx;
    g_ldDone = g_ldError = 0;
    g_ldState = 1;

    CdSyncCallback(loader_sync);
    CdIntToPos(g_ldFirst, &loc);
    CdControlF(CdlSetloc, (u_char *)&loc);

    while (!g_ldDone && !g_ldError)
    {
        if ((double)(SDL_GetPerformanceCounter() - t0) / SDL_GetPerformanceFrequency() > 30.0)
            break;
        SDL_Delay(1);
    }
    t = (double)(SDL_GetPerformanceCounter() - t0) / SDL_GetPerformanceFrequency();

    for (i = 0; i < count; i++)
        if (memcmp(buf + (size_t)i * 0x800, g_file + (size_t)(startIdx + i) * LAIN_XA_RAW_SECTOR + 24, 0x800))
            bad++;

    printf("loader: %d sectors from LBA %d, turbo %d: %s in %.2f s (%.0f sectors/s), %d mismatched, error %d\n",
           count, g_ldFirst, turbo, g_ldDone ? "done" : "NOT DONE", t, count / t, bad, g_ldError);
    free(buf);
    LainCD_SetDataTurbo(1);
    return g_ldDone && !g_ldError && bad == 0;
}

/* RT + SF over a stretch with data sectors between the audio: those reach the ready callback. */
static int run_mixed(int chan, int startIdx, double seconds)
{
    CdlLOC loc;
    u_char filter[4], param[4], result[8];
    int endLba, i, expectData = 0, ok;

    g_dataSectors = 0;
    CdReadyCallback(ready_cb);
    CdSyncCallback(NULL);
    CdIntToPos(g_base + startIdx, &loc);
    CdControl(CdlSetloc, (u_char *)&loc, 0);
    CdControl(CdlSeekP, (u_char *)&loc, 0);
    filter[0] = 1;
    filter[1] = (u_char)chan;
    CdControl(CdlSetfilter, filter, 0);
    param[0] = CdlModeSpeed | CdlModeRT | CdlModeSF;
    CdControlB(CdlSetmode, param, 0);
    CdControl(CdlReadS, 0, 0);
    SDL_Delay((Uint32)(seconds * 1000));
    CdControlB(CdlPause, 0, 0);
    CdControlB(CdlGetlocP, 0, result);
    endLba = CdPosToInt((CdlLOC *)&result[5]);
    CdReadyCallback(NULL);

    for (i = startIdx; i <= endLba - g_base && i < g_nsec; i++)
        if (!LainXA_IsAudioSector(g_file + (size_t)i * LAIN_XA_RAW_SECTOR))
            expectData++;
    ok = expectData > 0 && g_dataSectors == expectData;
    printf("mixed:  RT+SF read LBA %d..%d: %d data sectors to the ready callback (expected %d) -> %s\n",
           g_base + startIdx, endLba, g_dataSectors, expectData, ok ? "OK" : "FAILED");
    return ok;
}

int main(int argc, char **argv)
{
    char xaPath[1024];
    const char *outDir = XA_DECODE_OUT;
    const char *base;
    int chan = 0, mode = 0, i, ok;
    double seconds = -1;

    snprintf(xaPath, sizeof(xaPath), "%s/extract/disc1/XA/LAIN01.XA", LAIN_REPO_ROOT);

    for (i = 1; i < argc; i++)
    {
        if (!strcmp(argv[i], "--xa") && i + 1 < argc)
            snprintf(xaPath, sizeof(xaPath), "%s", argv[++i]);
        else if (!strcmp(argv[i], "--chan") && i + 1 < argc)
            chan = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--out") && i + 1 < argc)
            outDir = argv[++i];
        else if (!strcmp(argv[i], "--drive"))
            mode = 1;
        else if (!strcmp(argv[i], "--live"))
            mode = 2;
        else if (argv[i][0] != '-' && mode != 0)
            seconds = atof(argv[i]);
        else
        {
            fprintf(stderr, "usage: %s [--xa FILE] [--chan N] [--out DIR] [--drive [SECONDS] | --live [SECONDS]]\n", argv[0]);
            return 2;
        }
    }
    if (seconds <= 0)
        seconds = mode == 2 ? 8.0 : 6.0;

    base = strrchr(xaPath, '/');
    base = base ? base + 1 : xaPath;
    {
        static char b[256];
        char *dot;
        snprintf(b, sizeof(b), "%s", base);
        dot = strchr(b, '.');
        if (dot) *dot = 0;
        for (dot = b; *dot; dot++)
            if (*dot >= 'A' && *dot <= 'Z') *dot += 32;
        base = b;
    }

    if (!load_file(xaPath))
        return 1;

    printf("%s: %ld sectors\n", xaPath, g_nsec);
    ok = decode_channel(chan, outDir, base);

    if (ok && mode)
    {
        if (SDL_Init(mode == 2 ? SDL_INIT_AUDIO : 0) != 0)
            fprintf(stderr, "SDL_Init: %s\n", SDL_GetError());
        printf("\n%s: channel %d through libcd for %.1f s\n", mode == 2 ? "live" : "drive", chan, seconds);
        ok = run_drive(chan, seconds, mode == 2, outDir, base);
        if (mode == 1)
        {
            printf("\n");
            ok &= run_loader(1000, 300, 1);
            ok &= run_loader(20000, 2000, 0);
            ok &= run_mixed(chan, 8000, 3.0);
        }
        LainCD_Shutdown();
        SDL_Quit();
    }

    free(g_file);
    return ok ? 0 : 1;
}
