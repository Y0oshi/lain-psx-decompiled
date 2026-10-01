/* lain: CD-XA ADPCM audio and CD drive emulation for the Lain native port.
 *
 * PsyCross has no XA audio. This adds:
 *   - an XA-ADPCM decoder (4/8-bit, mono/stereo, 37.8/18.9 kHz) plus a
 *     windowed-sinc resampler to the 44.1 kHz output rate;
 *   - the XA "voice" of the CD controller, feeding the software SPU's CD
 *     input (src/lain_snd), which applies the CD volume, mixes it and fills
 *     the capture area (SpuReadDecodedData / SPU IRQ, used by Lain's level
 *     meter); CdMix attenuator and CdlMute are applied here;
 *   - a paced CD drive (libcd CdControl* / CdSync / CdReadyCallback /
 *     CdSyncCallback / CdGetSector) that honours CdlSetmode (speed, RT, SF,
 *     sector size), CdlSetfilter, CdlReadN/ReadS/Pause/Stop/Seek/GetlocP/L,
 *     sends matching XA sectors to the decoder and data sectors to the game.
 *
 * Everything is written from public hardware documentation (psx-spx).
 */
#ifndef LAIN_XA_H
#define LAIN_XA_H

#include "psx/types.h"
#include "psx/libcd.h"

#if defined(__cplusplus)
extern "C" {
#endif

/* ------------------------------------------------------------------ */
/* Sector layout helpers (raw 2352-byte Mode 2 sectors)                */
/* ------------------------------------------------------------------ */

#define LAIN_XA_RAW_SECTOR      2352
#define LAIN_XA_OUT_RATE        44100
/* Largest decode of one sector: 4-bit mono = 18 groups * 8 units * 28 samples. */
#define LAIN_XA_MAX_FRAMES      4032

typedef struct {
    int channels;   /* 1 or 2 */
    int rate;       /* 37800 or 18900 */
    int bits;       /* 4 or 8 */
    int file;       /* subheader file number */
    int chan;       /* subheader channel number */
    int submode;    /* subheader submode byte */
    int eof;        /* submode EOF bit */
} LainXAFormat;

/* Nonzero if the raw sector is a Form 2 XA-ADPCM audio sector (submode audio + form2). */
int LainXA_IsAudioSector(const u_char *raw2352);

/* Parse the subheader of an audio sector. Returns 0 if it is not an audio sector. */
int LainXA_GetFormat(const u_char *raw2352, LainXAFormat *fmt);

/* ------------------------------------------------------------------ */
/* Pure decoder (no audio device needed)                               */
/* ------------------------------------------------------------------ */

typedef struct {
    int hist[2][2];     /* ADPCM history per channel: [ch][0]=s(n-1), [1]=s(n-2) */
    int lastCoding;     /* coding byte of the previous sector (-1: none) */
} LainXADecoder;

void LainXA_DecoderReset(LainXADecoder *d);

/* Decodes one raw audio sector to interleaved s16 PCM at the sector's native
 * rate. `pcm` must hold LAIN_XA_MAX_FRAMES * 2 shorts. Returns the number of
 * frames written (0 if the sector is not audio). `fmt` may be NULL. */
int LainXA_DecodeSector(LainXADecoder *d, const u_char *raw2352, short *pcm, LainXAFormat *fmt);

/* Band-limited resampler (windowed sinc, 32 taps, polyphase). */
#define LAIN_XA_RS_TAPS    32
#define LAIN_XA_RS_PHASES  256
#define LAIN_XA_RS_MAXIN   (LAIN_XA_MAX_FRAMES + LAIN_XA_RS_TAPS * 2)

typedef struct {
    int inRate, outRate, channels;
    double step;        /* input samples per output sample */
    double pos;         /* position in buf (input samples) */
    int nbuf;           /* valid samples in buf */
    float buf[2][LAIN_XA_RS_MAXIN];
    float table[LAIN_XA_RS_PHASES + 1][LAIN_XA_RS_TAPS];
} LainXAResampler;

void LainXA_ResamplerInit(LainXAResampler *r, int inRate, int outRate, int channels);
/* Feeds `inFrames` interleaved frames; writes up to `maxOut` interleaved output
 * frames and returns their count. inFrames must be <= LAIN_XA_MAX_FRAMES. */
int LainXA_Resample(LainXAResampler *r, const short *in, int inFrames, short *out, int maxOut);

/* ------------------------------------------------------------------ */
/* Playback: the XA voice of the CD controller -> SPU CD input          */
/* ------------------------------------------------------------------ */
/* Output goes to the software SPU (src/lain_snd) as its CD input, pulled
 * by the SPU audio callback: the SPU applies the CD volume and cd.mix
 * (SpuSetCommonAttr), mixes, and fills the capture area / SPU IRQ.
 * The STR streamer (src/lain_press, CdRead2) feeds its XA sectors here
 * through LainXA_FeedSector by default, so movies play their audio. */

/* Decodes and queues one raw sector for playback, exactly as the drive does
 * with RT mode on and no channel filter. For STR movies: pass every sector
 * (non-audio ones are ignored) or only the audio ones, in disc order.
 * Returns the number of 44.1 kHz frames queued (0 = not audio / dropped).
 * Thread-safe; call from any thread. */
int LainXA_FeedSector(const u_char *raw2352);

/* Same, but only plays the sector if its subheader file/channel match
 * (like CdlModeSF + CdlSetfilter). */
int LainXA_FeedSectorFiltered(const u_char *raw2352, int file, int chan);

/* Drops all queued audio and resets the decoder (seek / new stream). */
void LainXA_Flush(void);

/* Seconds of audio queued but not yet played. */
double LainXA_GetQueuedSeconds(void);

/* 44.1 kHz frames played since the last flush: the audio clock for A/V sync. */
long long LainXA_GetPlayedFrames(void);

/* Nonzero while the output is playing (not starved / not idle). */
int LainXA_IsPlaying(void);

/* Player-facing XA volume (settings / voice slider), 0.0 .. 1.0+; default 1. */
void LainXA_SetUserVolume(float gain);

/* CD attenuator matrix (CdMix), 0x80 = 100%. */
void LainXA_SetAttenuator(const CdlATV *atv);

/* CdlMute / CdlDemute state. */
void LainXA_SetMute(int on);

/* Delay between the first queued sector and the start of output (default
 * 60 ms): absorbs the burstiness of sector arrival. */
void LainXA_SetPrebufferMs(int ms);

/* Pulls up to `frames` frames of 44.1 kHz stereo output (what the SPU's CD
 * input receives), zero-filling the rest; returns frames of real audio.
 * Normally the software SPU (src/lain_snd) calls this itself through
 * LainSpu_SetCdSource(); use it only for offline rendering / tests. */
int LainXA_Pull(short *stereo, int frames);

/* Counters for diagnostics. */
typedef struct {
    long long sectorsDecoded;
    long long sectorsDropped;   /* no free output buffer */
    long long underruns;        /* output starved (includes each stream's end) */
    long long framesQueued;
} LainXAStats;
void LainXA_GetStats(LainXAStats *st);

/* Detaches from the SPU CD input and drops queued audio (tests / shutdown). */
void LainXA_Shutdown(void);

/* lain: replacement audio (dub packs). While set, fn supplies the samples that
 * play in place of the decoded XA audio (44.1 kHz stereo s16), frame for frame,
 * so playback stays paced by the disc stream. fn runs on the audio thread and
 * returns the frames it wrote (fewer = the rest is silent). NULL restores XA. */
typedef int (*LainXAReplaceFn)(short *stereo, int frames, void *user);
void LainXA_SetReplacement(LainXAReplaceFn fn, void *user);

/* ------------------------------------------------------------------ */
/* CD drive                                                             */
/* ------------------------------------------------------------------ */

/* Supplies raw 2352-byte sectors by LBA (LBA 0 = MSF 00:02:00, i.e.
 * CdPosToInt()). Return nonzero on success. Default: the PsyCross disc image
 * given to PsyX_CDFS_Init (read through its own file handle). The client can
 * point this at lain.pak. Pass NULL to restore the default. */
typedef int (*LainCD_SectorReader)(int lba, u_char *raw2352, void *user);
void LainCD_SetSectorReader(LainCD_SectorReader reader, void *user);

/* Reads one raw sector through the current reader (any thread). */
int LainCD_ReadRawSector(int lba, u_char *raw2352);

/* lain: reads one raw sector straight from the disc image, bypassing the
 * reader (for a reader that changes what the disc holds, such as mods). */
int LainCD_ReadImageSector(int lba, u_char *raw2352);

/* Data-only reads (RT off) may run faster than real time: 1 = real speed
 * (default), N = N times faster, 0 = as fast as the game consumes them.
 * Reads with CdlModeRT set always run at the real 75/150 sectors per second. */
void LainCD_SetDataTurbo(int multiplier);

/* "Interrupt" lock. The drive and SPU-IRQ threads hold it while running game
 * callbacks, so EnterCriticalSection()/ExitCriticalSection() should call these
 * to keep callbacks out, as disabling interrupts does on the PS1. Recursive. */
void LainCD_EnterIRQ(void);
void LainCD_ExitIRQ(void);

/* Current drive position (LBA of the last sector read) and read state. */
int LainCD_GetPosition(void);
int LainCD_IsReading(void);
/* XA channel being streamed as real-time audio (RT + SF modes), or -1. */
int LainCD_GetXAChannel(void);

/* Disc swap: the shell (lid) reads as open for `ms` milliseconds, then closed
 * again, the way the game's lid watchdog expects (func_8003B438). Swap the
 * image (PsyX_CDFS_SwapImage) at the same time. */
void LainCD_OpenShell(int ms);

/* Stops the drive thread (tests / shutdown). */
void LainCD_Shutdown(void);

/* libcd functions PsyCross lacks, implemented by the drive. */
int CdMix(CdlATV *vol);
int CdMode(void);
int CdStatus(void);
CdlLOC *CdLastPos(void);
int CdReady(int mode, u_char *result);
int CdReset(int mode);
void CdFlush(void);
int CdDataSync(int mode);

#if defined(__cplusplus)
}
#endif

#endif /* LAIN_XA_H */
