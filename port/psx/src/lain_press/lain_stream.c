/*
 * lain: libcd data streaming (St* ring buffer + CdRead2) for STR movies.
 *
 * Own implementation of the documented behaviour. A "virtual drive" reads raw
 * 2352-byte sectors from a sector source (by default LainCD_ReadRawSector of
 * the CD drive emulation; see LainSt_SetSectorReader) and assembles the video sectors
 * of each STR frame into one contiguous buffer:
 *
 *   sector user data (2048 bytes) = 32-byte STR header + 2016 bytes payload
 *   header: u16 0x0160 magic, u16 type, u16 chunk index, u16 chunk count,
 *           u32 frame number, u32 frame size, u16 width, u16 height, ...
 *
 * StGetNext() hands out complete frames in arrival order: *addr = frame
 * payload (chunk i at offset i*2016), *header = the 32-byte StHEADER of the
 * frame's first chunk. The frame stays locked until StFreeRing(addr).
 *
 * The buffer passed to StSetRing() is not written to; its sector count only
 * sets how many sectors may be buffered (frames being assembled + frames not
 * yet freed). When that is full the virtual drive waits instead of dropping
 * data, so a slow consumer never loses frames.
 *
 * XA audio sectors (subheader submode bit 2) interleaved in the stream go,
 * raw, to the sink registered with LainSt_SetAudioSink(), or by default (when
 * CdRead2's mode has CdlModeRT, as on hardware) to LainXA_FeedSector().
 *
 * Pacing: by default sectors "arrive" in real time, 150/s at double speed
 * (CdlModeSpeed in the CdRead2 mode) or 75/s, so a movie plays at its authored
 * frame rate; StGetNext() sleeps (at most until the next sector is due) when
 * no frame is ready yet, so busy-wait loops in the game take real time.
 * LainSt_SetRealtime(0) makes sectors available instantly (tools/tests).
 *
 * Everything runs on the caller's thread: the drive is advanced from
 * StGetNext / StCdInterrupt / LainSt_Pump.
 */
#include "psx/types.h"
#include "psx/libcd.h"
#include "lain_xa.h"

#include <SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define RAW_SECTOR   2352
#define STR_PAYLOAD  2016
#define MAX_SLOTS    16

typedef struct {
    int      used;      /* slot holds a frame (assembling or complete) */
    int      complete;
    int      locked;    /* handed out by StGetNext, waiting for StFreeRing */
    u_int    seq;       /* completion order */
    u_int    frame;
    int      nsec, got;
    u_int    have[4];   /* bitmask of received chunks (up to 128) */
    int      lba;       /* lba of the first chunk seen */
    StHEADER hdr;       /* header of chunk 0 (32 bytes) */
    u_char  *data;
} Slot;

static Slot   s_slot[MAX_SLOTS];
static int    s_ring_sectors;   /* 0 = no ring set */
static int    s_slot_bytes;
static u_int  s_seq;

static int    s_active;         /* drive is reading */
static int    s_lba;            /* next sector to read */
static int    s_mode24;
static u_int  s_start_frame = 1, s_end_frame = 0xFFFFFFFFu;
static void (*s_func1)(void);
static void (*s_func2)(void);
static int    s_end_signalled;
static int    s_channel = -1;   /* StSetChannel: -1 = any */

static int    s_realtime = 1;
static double s_sector_period = 1.0 / 150.0;
static double s_next_due;       /* seconds */
static int    s_last_lba = -1;  /* lba of the newest completed frame */

static u_char s_pending[RAW_SECTOR];
static int    s_pending_valid;

static int    s_setloc_lba = -1;
static int    s_rt;             /* CdlModeRT: XA sectors go to the XA voice */

/* StCdIntrFlag: PsyQ sets this when a CD interrupt was deferred during a
 * 24-bit MDEC transfer, and the player then calls StCdInterrupt() itself
 * (Lain: D_801F96E4 in func_8001ABB0). Nothing is ever deferred here. */
int StCdIntrFlag = 0;

/* Sector source */

static LainStSectorReader s_reader;
static void *s_reader_user;
static LainStAudioSink s_audio_sink;
static void *s_audio_user;

/* Defaults go through the CD drive emulation's public API (lain_xa.h): its
 * sector reader (disc image or lain.pak) and its XA voice. */
static int default_reader(int lba, u_char *out, void *user)
{
    (void)user;
    return LainCD_ReadRawSector(lba, out);
}

static void default_audio_sink(const u_char *sector, int lba, void *user)
{
    (void)lba;
    (void)user;
    LainXA_FeedSector(sector);
}

void LainSt_SetSectorReader(LainStSectorReader reader, void *user)
{
    s_reader = reader;
    s_reader_user = user;
}

void LainSt_SetAudioSink(LainStAudioSink sink, void *user)
{
    s_audio_sink = sink;
    s_audio_user = user;
}

void LainSt_SetRealtime(int on)
{
    s_realtime = on;
}

void LainSt_NotifySetloc(int lba)
{
    s_setloc_lba = lba;
}

/* Timing */

static double now_s(void)
{
    static Uint64 freq;
    if (!freq)
        freq = SDL_GetPerformanceFrequency();
    return (double)SDL_GetPerformanceCounter() / (double)freq;
}

/* Ring */

static void free_slots(void)
{
    int i;
    for (i = 0; i < MAX_SLOTS; i++) {
        free(s_slot[i].data);
        memset(&s_slot[i], 0, sizeof(Slot));
    }
    s_slot_bytes = 0;
}

static void clear_slots(void)
{
    int i;
    for (i = 0; i < MAX_SLOTS; i++) {
        u_char *d = s_slot[i].data;
        memset(&s_slot[i], 0, sizeof(Slot));
        s_slot[i].data = d;
    }
}

static int sectors_in_use(void)
{
    int i, n = 0;
    for (i = 0; i < MAX_SLOTS; i++)
        if (s_slot[i].used)
            n += s_slot[i].nsec;
    return n;
}

static Slot *slot_for_frame(u_int frame, int nsec, int lba, int *full)
{
    int i;
    Slot *fr = NULL;
    *full = 0;
    for (i = 0; i < MAX_SLOTS; i++)
        if (s_slot[i].used && !s_slot[i].complete && s_slot[i].frame == frame)
            return &s_slot[i];
    /* a new frame: frames arrive in order, so drop older unfinished ones */
    for (i = 0; i < MAX_SLOTS; i++)
        if (s_slot[i].used && !s_slot[i].complete)
            s_slot[i].used = 0;
    if (sectors_in_use() + nsec > s_ring_sectors) {
        *full = 1;
        return NULL;
    }
    for (i = 0; i < MAX_SLOTS && !fr; i++)
        if (!s_slot[i].used)
            fr = &s_slot[i];
    if (!fr) {
        *full = 1;
        return NULL;
    }
    if (!fr->data)
        fr->data = (u_char *)calloc(1, (size_t)s_slot_bytes);
    if (!fr->data)
        return NULL;
    fr->used = 1;
    fr->complete = fr->locked = 0;
    fr->frame = frame;
    fr->nsec = nsec;
    fr->got = 0;
    fr->lba = lba;
    memset(fr->have, 0, sizeof(fr->have));
    return fr;
}

/* Handles one raw sector. Returns 0 if it must be retried later (ring full). */
static int process_sector(const u_char *raw, int lba)
{
    const u_char *sub = raw + 16;
    const u_char *d = raw + 24;
    u_short magic, chunk, nsec;
    u_int frame, fsize;
    int full;
    Slot *sl;

    if (sub[2] & 0x04) { /* XA audio (form 2): to the XA voice in RT mode */
        if (s_audio_sink)
            s_audio_sink(raw, lba, s_audio_user);
        else if (s_rt)
            default_audio_sink(raw, lba, NULL);
        return 1;
    }
    if (!(sub[2] & 0x08))
        return 1; /* not data */
    if (s_channel >= 0 && sub[1] != s_channel)
        return 1;

    magic = (u_short)(d[0] | d[1] << 8);
    if (magic != 0x0160)
        return 1;
    chunk = (u_short)(d[4] | d[5] << 8);
    nsec  = (u_short)(d[6] | d[7] << 8);
    frame = (u_int)(d[8] | d[9] << 8 | d[10] << 16 | (u_int)d[11] << 24);
    fsize = (u_int)(d[12] | d[13] << 8 | d[14] << 16 | (u_int)d[15] << 24);
    (void)fsize;
    if (nsec == 0 || chunk >= nsec || nsec > 128 || nsec > s_ring_sectors)
        return 1;

    if (frame < s_start_frame)
        return 1;
    if (frame > s_end_frame) {
        if (!s_end_signalled) {
            s_end_signalled = 1;
            if (s_func2)
                s_func2();
        }
        return 1;
    }

    sl = slot_for_frame(frame, nsec, lba, &full);
    if (!sl)
        return full ? 0 : 1;
    if (!(sl->have[chunk >> 5] & (1u << (chunk & 31)))) {
        sl->have[chunk >> 5] |= 1u << (chunk & 31);
        sl->got++;
        memcpy(sl->data + (size_t)chunk * STR_PAYLOAD, d + 32, STR_PAYLOAD);
        if (chunk == 0)
            memcpy(&sl->hdr, d, sizeof(StHEADER) < 32 ? sizeof(StHEADER) : 32);
    }
    if (sl->got == sl->nsec) {
        if (sl->hdr.id != 0x0160) /* chunk 0 header always arrives, but be safe */
            memcpy(&sl->hdr, d, sizeof(StHEADER) < 32 ? sizeof(StHEADER) : 32);
        sl->complete = 1;
        sl->seq = s_seq++;
        s_last_lba = sl->lba;
        if (s_func1)
            s_func1();
    }
    return 1;
}

/* Reads one sector into s_pending. Returns 0 at end of source. */
static int fetch_sector(void)
{
    LainStSectorReader rd = s_reader ? s_reader : default_reader;
    if (!rd(s_lba, s_pending, s_reader ? s_reader_user : NULL)) {
        s_active = 0;
        return 0;
    }
    s_pending_valid = 1;
    return 1;
}

/* Advances the virtual drive up to "now". Returns 1 if a frame completed. */
static int pump(void)
{
    u_int seq0 = s_seq;
    int budget = 4096;
    double now = s_realtime ? now_s() : 0.0;

    if (!s_ring_sectors)
        return 0;
    /* don't let a long stall turn into a huge burst */
    if (s_realtime && s_next_due < now - s_ring_sectors * s_sector_period)
        s_next_due = now - s_ring_sectors * s_sector_period;

    while (budget-- > 0) {
        if (!s_pending_valid) {
            if (!s_active)
                break;
            if (s_realtime && now < s_next_due)
                break;
            if (!fetch_sector())
                break;
            s_next_due += s_sector_period;
            s_lba++;
        }
        if (!process_sector(s_pending, s_lba - 1)) {
            /* ring full: the drive waits; its clock restarts when there is room */
            if (s_realtime && s_next_due < now)
                s_next_due = now;
            break;
        }
        s_pending_valid = 0;
        if (!s_realtime && s_seq != seq0)
            break; /* instant mode: stop after one frame */
    }
    return s_seq != seq0;
}

void LainSt_Pump(void)
{
    pump();
}

void LainSt_StartAt(int lba, int mode)
{
    s_lba = lba;
    s_active = 1;
    s_pending_valid = 0;
    s_end_signalled = 0;
    s_sector_period = (mode & CdlModeSpeed) ? 1.0 / 150.0 : 1.0 / 75.0;
    s_rt = (mode & CdlModeRT) != 0;
    s_next_due = s_realtime ? now_s() : 0.0;
    /* frames being assembled belong to the old position */
    {
        int i;
        for (i = 0; i < MAX_SLOTS; i++)
            if (s_slot[i].used && !s_slot[i].complete)
                s_slot[i].used = 0;
    }
}

void LainSt_Stop(void)
{
    s_active = 0;
    s_pending_valid = 0;
}

int LainSt_IsActive(void)
{
    return s_active;
}

/* lain: next sector the movie stream reads, or -1 when no stream is active.
 * Used to identify the movie and its playback time for subtitles/dubs. */
int LainSt_GetPosition(void)
{
    return s_active ? s_lba : -1;
}

/* PsyQ API */

void StSetRing(u_int *ring_addr, u_int ring_size)
{
    (void)ring_addr;
    free_slots();
    s_ring_sectors = (int)ring_size;
    s_slot_bytes = (int)ring_size * STR_PAYLOAD;
    s_seq = 0;
}

void StClearRing(void)
{
    clear_slots();
    s_pending_valid = 0;
}

void StUnSetRing(void)
{
    LainSt_Stop();
    free_slots();
    s_ring_sectors = 0;
}

void StSetStream(u_int mode, u_int start_frame, u_int end_frame,
                 void (*func1)(), void (*func2)())
{
    s_mode24 = (int)(mode & 1);
    s_start_frame = start_frame;
    s_end_frame = end_frame;
    s_func1 = (void (*)(void))func1;
    s_func2 = (void (*)(void))func2;
    s_end_signalled = 0;
    StCdIntrFlag = 0;
    clear_slots();
    s_pending_valid = 0;
}

void StSetEmulate(u_int *addr, u_int mode, u_int start_frame,
                  u_int end_frame, void (*func1)(), void (*func2)())
{
    /* memory-resident STR emulation: not needed by Lain */
    (void)addr;
    StSetStream(mode, start_frame, end_frame, func1, func2);
}

static Slot *next_ready(void)
{
    Slot *best = NULL;
    int i;
    for (i = 0; i < MAX_SLOTS; i++) {
        Slot *sl = &s_slot[i];
        if (sl->used && sl->complete && !sl->locked && (!best || sl->seq < best->seq))
            best = sl;
    }
    return best;
}

u_int StGetNext(u_int **addr, u_int **header)
{
    Slot *sl;

    if (!s_ring_sectors)
        return 1;
    pump();
    sl = next_ready();
    if (!sl && s_realtime && s_active) {
        /* nothing yet: wait (bounded) for the next sector, then look again */
        double wait = s_next_due - now_s();
        if (wait > 0.0) {
            Uint32 ms = (Uint32)(wait * 1000.0);
            SDL_Delay(ms < 1 ? 1 : ms > 20 ? 20 : ms);
        }
        pump();
        sl = next_ready();
    }
    if (!sl)
        return 1;
    sl->locked = 1;
    *addr = (u_int *)sl->data;
    *header = (u_int *)&sl->hdr;
    return 0;
}

u_int StGetNextS(u_int **addr, u_int **header)
{
    return StGetNext(addr, header);
}

u_short StNextStatus(u_int **addr, u_int **header)
{
    Slot *sl;
    pump();
    sl = next_ready();
    if (!sl)
        return StFREE;
    *addr = (u_int *)sl->data;
    *header = (u_int *)&sl->hdr;
    return StCOMPLETE;
}

u_int StFreeRing(u_int *base)
{
    int i;
    for (i = 0; i < MAX_SLOTS; i++)
        if (s_slot[i].used && s_slot[i].complete && (u_int *)s_slot[i].data == base) {
            s_slot[i].used = s_slot[i].complete = s_slot[i].locked = 0;
            return 0;
        }
    return 1;
}

void StRingStatus(short *free_sectors, short *over_sectors)
{
    int used = sectors_in_use();
    if (free_sectors)
        *free_sectors = (short)(s_ring_sectors - used);
    if (over_sectors)
        *over_sectors = 0;
}

void StSetMask(u_int mask, u_int start, u_int end)
{
    (void)mask;
    (void)start;
    (void)end;
}

void StCdInterrupt(void)
{
    StCdIntrFlag = 0;
    pump();
}

int StGetBackloc(CdlLOC *loc)
{
    if (s_last_lba < 0)
        return 0;
    CdIntToPos(s_last_lba, loc);
    return 1;
}

int StSetChannel(u_int channel)
{
    s_channel = (int)channel;
    return 1;
}

/* CdRead2: start streaming from the last CdlSetloc position, as reported by
 * the drive emulation (LainSt_NotifySetloc, else CdLastPos()). */
int CdRead2(int mode)
{
    int lba = s_setloc_lba >= 0 ? s_setloc_lba : CdPosToInt(CdLastPos());

    if (mode == 0) {
        LainSt_Stop();
        return 1;
    }
    if (lba < 0)
        return 0;
    LainSt_StartAt(lba, mode);
    return 1;
}
