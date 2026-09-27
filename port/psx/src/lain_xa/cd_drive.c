/* lain: CD drive emulation for PsyCross' libcd.
 *
 * Models the PS1 CD controller the way libcd sees it (psx-spx "CDROM
 * Controller Command Summary" / "CDROM XA Audio"):
 *   - commands take effect when issued; their completion (the INT2/INT3 the
 *     real drive raises) is delivered a moment later on the drive thread,
 *     through CdSync() status and the CdSyncCallback;
 *   - CdlReadN/CdlReadS read one sector per 1/75 s (1/150 s with
 *     CdlModeSpeed). With CdlModeRT, Form 2 audio sectors go to the XA
 *     decoder (only those matching CdlSetfilter when CdlModeSF is set) and
 *     never reach the CPU; every other sector is a data sector: it is
 *     buffered (2048 / 2328 / 2340 bytes per CdlModeSize0/1) and signalled
 *     with CdReadyCallback(CdlDataReady); CdGetSector() reads the buffer
 *     sequentially, as from the drive's FIFO; CdDataCallback runs after;
 *   - CdlGetlocP/CdlGetlocL report the sector being read.
 * Game callbacks run on the drive thread while holding the "IRQ" lock
 * (LainCD_EnterIRQ), which EnterCriticalSection() should also take.
 */
#include "lain_xa.h"
#include "lain_xa_internal.h"

#include "../PsyX_main.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_PENDING 64

typedef struct {
    u_char status;
    u_char result[8];
    Uint64 due;
    unsigned int seq;
} Pending;

typedef struct {
    int started;
    SDL_mutex *mu;
    SDL_mutex *irq;             /* SDL mutexes are recursive */
    SDL_Thread *thread;
    SDL_threadID threadId;
    volatile int quit;

    u_char mode;
    u_char filterFile, filterChan;
    int targetLBA, setlocPending;
    int nextLBA, curLBA;
    int reading;
    int motor;
    unsigned int gen;           /* bumped whenever the read stream is restarted/stopped */
    Uint64 nextTime;
    int turbo;

    u_char lastCom;
    int lastStatus;
    u_char lastResult[8];
    Pending pend[MAX_PENDING];
    int pHead, pCount;
    unsigned int issuedSeq, completedSeq;
    Uint64 lastDue;

    CdlCB readyCb, syncCb;
    void (*dataCb)(void);

    /* data sector buffer (CdGetSector) */
    u_char raw[LAIN_XA_RAW_SECTOR];
    int dataOfs, dataLen, dataRd;
    int getSectorCalled;
    int dataReady;
    u_char lastHeader[8];
    CdlLOC lastPos;

    LainCD_SectorReader reader;
    void *readerUser;
    SDL_mutex *rdMu;
    FILE *fp;
    char fpName[2048];
} CdDrive;

static CdDrive s_cd = { 0 };

static int cd_thread(void *arg);

/* ------------------------------------------------------------------ */

static void cd_early_init(void)
{
    /* first use happens on the game's main thread, before the drive thread exists */
    if (s_cd.mu)
        return;
    s_cd.mu = SDL_CreateMutex();
    s_cd.irq = SDL_CreateMutex();
    s_cd.rdMu = SDL_CreateMutex();
    s_cd.turbo = 1;
    s_cd.lastStatus = CdlComplete;
    s_cd.motor = 1;
}

static void cd_start(void)
{
    cd_early_init();
    if (s_cd.started)
        return;
    SDL_LockMutex(s_cd.mu);
    if (!s_cd.started)
    {
        s_cd.started = 1;
        s_cd.quit = 0;
        s_cd.thread = SDL_CreateThread(cd_thread, "LainCD", NULL);
    }
    SDL_UnlockMutex(s_cd.mu);
}

void LainCD_EnterIRQ(void)
{
    cd_early_init();
    SDL_LockMutex(s_cd.irq);
}

void LainCD_ExitIRQ(void)
{
    SDL_UnlockMutex(s_cd.irq);
}

/* Shell (lid) emulation for disc swaps: open until this performance-counter
 * time. While open, the drive reports CdlStatShellOpen and no spindle. */
static Uint64 s_shellOpenUntil;

void LainCD_OpenShell(int ms)
{
    cd_early_init();
    SDL_LockMutex(s_cd.mu);
    s_shellOpenUntil = SDL_GetPerformanceCounter() + SDL_GetPerformanceFrequency() * (Uint64)ms / 1000;
    SDL_UnlockMutex(s_cd.mu);
}

static u_char cd_stat_locked(void)
{
    u_char st = 0;
    if (s_shellOpenUntil && SDL_GetPerformanceCounter() < s_shellOpenUntil)
        return CdlStatShellOpen;
    if (s_cd.motor)
        st |= CdlStatStandby;
    if (s_cd.reading)
        st |= CdlStatRead;
    return st;
}

static Uint64 cd_period_locked(void)
{
    Uint64 freq = SDL_GetPerformanceFrequency();
    Uint64 p = freq / ((s_cd.mode & CdlModeSpeed) ? 150 : 75);

    if (!(s_cd.mode & CdlModeRT))
    {
        if (s_cd.turbo == 0)
            return 0;
        if (s_cd.turbo > 1)
            p /= (Uint64)s_cd.turbo;
    }
    return p;
}

static void cd_push_completion_locked(u_char status, const u_char *result, int delayUs)
{
    Uint64 now = SDL_GetPerformanceCounter();
    Uint64 due = now + SDL_GetPerformanceFrequency() * (Uint64)delayUs / 1000000u;
    Pending *p;

    /* completions arrive in order */
    if (due < s_cd.lastDue)
        due = s_cd.lastDue;
    s_cd.lastDue = due;

    s_cd.issuedSeq++;

    if (s_cd.pCount == MAX_PENDING)
    {
        /* overflow: drop the oldest (should not happen) */
        s_cd.pHead = (s_cd.pHead + 1) % MAX_PENDING;
        s_cd.pCount--;
    }

    p = &s_cd.pend[(s_cd.pHead + s_cd.pCount) % MAX_PENDING];
    p->status = status;
    memcpy(p->result, result, 8);
    p->due = due;
    p->seq = s_cd.issuedSeq;
    s_cd.pCount++;
}

static void lba_to_bcd_msf(int lba, u_char *out3)
{
    CdlLOC loc;
    CdIntToPos(lba, &loc);
    out3[0] = loc.minute;
    out3[1] = loc.second;
    out3[2] = loc.sector;
}

static void cd_stop_stream_locked(void)
{
    s_cd.reading = 0;
    s_cd.gen++;
}

/* The STR streamer (CdRead2, src/lain_press) is the same physical drive:
 * any command that moves or stops the head ends its stream. */
static void cd_stop_st_stream(void)
{
    if (LainSt_IsActive())
        LainSt_Stop();
}

/* Position change for a new read: drop the XA audio of the old stream. */
static void cd_seek_locked(int lba, int *flushAudio)
{
    if (lba != s_cd.nextLBA)
        *flushAudio = 1;
    s_cd.nextLBA = lba;
    s_cd.curLBA = lba;
}

int LainCD_Control(u_char com, u_char *param, u_char *result, int kind)
{
    u_char res[8];
    int delayUs = 1000;
    int flushAudio = 0;
    unsigned int mySeq;
    int onDriveThread;
    int stopSt = 0;

    cd_start();

    memset(res, 0, sizeof(res));

    SDL_LockMutex(s_cd.mu);

    s_cd.lastCom = com;

    /* libcd sends a Setloc first when these commands are given a position */
    if (param && (com == CdlReadN || com == CdlReadS || com == CdlSeekL || com == CdlSeekP ||
                  com == CdlPlay))
    {
        s_cd.targetLBA = CdPosToInt((CdlLOC *)param);
        s_cd.setlocPending = 1;
    }

    switch (com)
    {
    case CdlNop:
        break;

    case CdlSetloc:
        if (param)
        {
            s_cd.targetLBA = CdPosToInt((CdlLOC *)param);
            s_cd.setlocPending = 1;
            s_cd.lastPos = *(CdlLOC *)param;
            LainSt_NotifySetloc(s_cd.targetLBA);    /* CdRead2 starts here */
        }
        break;

    case CdlReadN:
    case CdlReadS:
        if (s_cd.setlocPending)
        {
            cd_seek_locked(s_cd.targetLBA, &flushAudio);
            s_cd.setlocPending = 0;
        }
        {
            /* lain: LAIN_LOG_READS=1 logs where each read starts (tests: which files load) */
            static int log_reads = -1;
            if (log_reads < 0)
                log_reads = getenv("LAIN_LOG_READS") != NULL;
            if (log_reads)
                printf("[cd] read from lba %d\n", s_cd.curLBA);
        }
        s_cd.gen++;
        s_cd.reading = 1;
        s_cd.motor = 1;
        s_cd.nextTime = SDL_GetPerformanceCounter() + cd_period_locked();
        break;

    case CdlSeekL:
    case CdlSeekP:
        cd_stop_stream_locked();
        if (s_cd.setlocPending)
        {
            cd_seek_locked(s_cd.targetLBA, &flushAudio);
            s_cd.setlocPending = 0;
        }
        s_cd.motor = 1;
        delayUs = 4000;
        break;

    case CdlPause:
        /* stop reading; XA audio already sent to the SPU plays out */
        cd_stop_stream_locked();
        delayUs = 2000;
        break;

    case CdlStop:
        cd_stop_stream_locked();
        s_cd.motor = 0;
        flushAudio = 1;
        delayUs = 2000;
        break;

    case CdlStandby:
        s_cd.motor = 1;
        break;

    case CdlSetmode:
        if (param)
            s_cd.mode = param[0];
        break;

    case CdlSetfilter:
        if (param)
        {
            s_cd.filterFile = param[0];
            s_cd.filterChan = param[1];
        }
        break;

    case CdlMute:
        LainXA_SetMute(1);
        break;

    case CdlDemute:
        LainXA_SetMute(0);
        break;

    case CdlGetparam:
        res[1] = s_cd.mode;
        res[2] = 0;
        res[3] = s_cd.filterFile;
        res[4] = s_cd.filterChan;
        break;

    case CdlGetlocL:
        memcpy(res, s_cd.lastHeader, 8);
        break;

    case CdlGetlocP:
    {
        int lba = s_cd.curLBA;
        CdlLOC rel;
        /* track 1 starts at LBA 0; relative MSF has no 2-second lead-in */
        rel.sector = ENCODE_BCD(lba % 75);
        rel.second = ENCODE_BCD((lba / 75) % 60);
        rel.minute = ENCODE_BCD(lba / 75 / 60);
        res[0] = 0x01;          /* track */
        res[1] = 0x01;          /* index */
        res[2] = rel.minute;
        res[3] = rel.second;
        res[4] = rel.sector;
        lba_to_bcd_msf(lba, &res[5]);
        break;
    }

    case CdlGetTN:
        res[1] = 0x01;
        res[2] = 0x01;
        break;

    case CdlGetTD:
        res[1] = 0x00;
        res[2] = 0x02;
        break;

    case CdlPlay:
    case CdlForward:
    case CdlBackward:
        /* CD-DA is not used by Lain */
        break;

    default:
        eprintwarn("lain_cd: unhandled CD command 0x%02X\n", com);
        break;
    }

    if (com == CdlReadN || com == CdlReadS || com == CdlSeekL || com == CdlSeekP ||
        com == CdlPause || com == CdlStop)
        stopSt = 1;

    /* commands that carry a status byte first */
    if (com != CdlGetlocL && com != CdlGetlocP)
        res[0] = cd_stat_locked();

    cd_push_completion_locked(CdlComplete, res, delayUs);
    mySeq = s_cd.issuedSeq;

    onDriveThread = s_cd.thread && SDL_ThreadID() == s_cd.threadId;

    SDL_UnlockMutex(s_cd.mu);

    if (stopSt)
        cd_stop_st_stream();

    if (flushAudio)
        LainXA_Flush();

    if (result)
        memcpy(result, res, (com == CdlGetlocL || com == CdlGetlocP) ? 8 : 1);

    if (kind == LAINCD_CTL_B && !onDriveThread)
    {
        /* wait for the completion */
        for (;;)
        {
            unsigned int done;
            SDL_LockMutex(s_cd.mu);
            done = s_cd.completedSeq;
            SDL_UnlockMutex(s_cd.mu);
            if ((int)(done - mySeq) >= 0)
                break;
            SDL_Delay(1);
        }
    }

    return 1;
}

int LainCD_Sync(int mode, u_char *result)
{
    int status;

    cd_start();

    if (mode == 0 && !(s_cd.thread && SDL_ThreadID() == s_cd.threadId))
    {
        for (;;)
        {
            int n;
            SDL_LockMutex(s_cd.mu);
            n = s_cd.pCount;
            SDL_UnlockMutex(s_cd.mu);
            if (n == 0)
                break;
            SDL_Delay(1);
        }
    }

    SDL_LockMutex(s_cd.mu);
    if (s_cd.pCount > 0)
        status = CdlNoIntr;
    else
    {
        status = s_cd.lastStatus;
        if (result)
            memcpy(result, s_cd.lastResult, 8);
    }
    SDL_UnlockMutex(s_cd.mu);

    return status;
}

CdlCB LainCD_SetReadyCallback(CdlCB cb)
{
    CdlCB old;
    cd_early_init();
    SDL_LockMutex(s_cd.mu);
    old = s_cd.readyCb;
    s_cd.readyCb = cb;
    SDL_UnlockMutex(s_cd.mu);
    return old;
}

CdlCB LainCD_SetSyncCallback(CdlCB cb)
{
    CdlCB old;
    cd_early_init();
    SDL_LockMutex(s_cd.mu);
    old = s_cd.syncCb;
    s_cd.syncCb = cb;
    SDL_UnlockMutex(s_cd.mu);
    return old;
}

void *LainCD_SetDataCallback(void (*cb)(void))
{
    void (*old)(void);
    cd_early_init();
    SDL_LockMutex(s_cd.mu);
    old = s_cd.dataCb;
    s_cd.dataCb = cb;
    SDL_UnlockMutex(s_cd.mu);
    return (void *)old;
}

int LainCD_GetSector(void *madr, int words)
{
    int bytes = words * 4;
    int avail = s_cd.dataLen - s_cd.dataRd;

    if (bytes > avail)
        bytes = avail > 0 ? avail : 0;

    if (bytes > 0)
        memcpy(madr, s_cd.raw + s_cd.dataOfs + s_cd.dataRd, (size_t)bytes);
    if (words * 4 > bytes)
        memset((u_char *)madr + bytes, 0, (size_t)(words * 4 - bytes));

    s_cd.dataRd += bytes;
    s_cd.getSectorCalled = 1;
    s_cd.dataReady = 0;
    return 1;
}

int LainCD_LastCom(void)
{
    return s_cd.lastCom;
}

void LainCD_Reset(void)
{
    cd_start();

    SDL_LockMutex(s_cd.mu);
    cd_stop_stream_locked();
    s_cd.mode = 0;
    s_cd.filterFile = s_cd.filterChan = 0;
    s_cd.setlocPending = 0;
    s_cd.pCount = 0;
    s_cd.completedSeq = s_cd.issuedSeq;
    s_cd.lastStatus = CdlComplete;
    s_cd.readyCb = NULL;
    s_cd.syncCb = NULL;
    s_cd.dataCb = NULL;
    s_cd.dataLen = s_cd.dataRd = 0;
    s_cd.dataReady = 0;
    s_cd.motor = 1;
    SDL_UnlockMutex(s_cd.mu);

    cd_stop_st_stream();
    LainXA_Flush();
    LainXA_SetMute(0);
}

/* ------------------------------------------------------------------ */
/* Sector source                                                        */
/* ------------------------------------------------------------------ */

static void make_fake_header(int lba, u_char *raw)
{
    memset(raw, 0, 24);
    memset(raw + 1, 0xFF, 10);
    lba_to_bcd_msf(lba, raw + 12);
    raw[15] = 2;
}

static int default_reader(int lba, u_char *raw, void *user)
{
    const char *name = NULL;
    const u_char *mem = NULL;
    int memSize = 0, secSize = 0;
    int ok = 0;

    (void)user;

    if (lba < 0)
        return 0;

    if (!PsyX_CD_GetImageInfo(&name, &mem, &memSize, &secSize) || secSize <= 0)
        return 0;

    if (mem)
    {
        long long ofs = (long long)lba * secSize;
        if (ofs + secSize > memSize)
            return 0;
        if (secSize == LAIN_XA_RAW_SECTOR)
            memcpy(raw, mem + ofs, LAIN_XA_RAW_SECTOR);
        else
        {
            make_fake_header(lba, raw);
            memcpy(raw + 24, mem + ofs, secSize > 2048 ? 2048 : secSize);
        }
        return 1;
    }

    if (!name || !name[0])
        return 0;

    SDL_LockMutex(s_cd.rdMu);

    if (!s_cd.fp || strcmp(s_cd.fpName, name) != 0)
    {
        if (s_cd.fp)
            fclose(s_cd.fp);
        s_cd.fp = fopen(name, "rb");
        strncpy(s_cd.fpName, name, sizeof(s_cd.fpName) - 1);
    }

    if (s_cd.fp && fseek(s_cd.fp, (long)((long long)lba * secSize), SEEK_SET) == 0)
    {
        if (secSize == LAIN_XA_RAW_SECTOR)
            ok = fread(raw, LAIN_XA_RAW_SECTOR, 1, s_cd.fp) == 1;
        else
        {
            make_fake_header(lba, raw);
            ok = fread(raw + 24, secSize > 2048 ? 2048 : secSize, 1, s_cd.fp) == 1;
        }
    }

    SDL_UnlockMutex(s_cd.rdMu);
    return ok;
}

void LainCD_SetSectorReader(LainCD_SectorReader reader, void *user)
{
    cd_early_init();
    SDL_LockMutex(s_cd.mu);
    s_cd.reader = reader;
    s_cd.readerUser = user;
    SDL_UnlockMutex(s_cd.mu);
}

int LainCD_ReadRawSector(int lba, u_char *raw2352)
{
    LainCD_SectorReader r;
    void *u;

    cd_early_init();
    SDL_LockMutex(s_cd.mu);
    r = s_cd.reader;
    u = s_cd.readerUser;
    SDL_UnlockMutex(s_cd.mu);

    if (!r)
        return default_reader(lba, raw2352, NULL);
    return r(lba, raw2352, u);
}

void LainCD_SetDataTurbo(int multiplier)
{
    cd_early_init();
    SDL_LockMutex(s_cd.mu);
    s_cd.turbo = multiplier < 0 ? 1 : multiplier;
    SDL_UnlockMutex(s_cd.mu);
}

int LainCD_GetPosition(void)
{
    return s_cd.curLBA;
}

int LainCD_IsReading(void)
{
    return s_cd.reading;
}

/* lain: XA channel being streamed to the audio path (real-time ADPCM with the
 * channel filter on), or -1. Used to identify the voice track for subtitles/dubs. */
int LainCD_GetXAChannel(void)
{
    int chan = -1;
    cd_early_init();
    SDL_LockMutex(s_cd.mu);
    if (s_cd.reading && (s_cd.mode & CdlModeRT) && (s_cd.mode & CdlModeSF))
        chan = s_cd.filterChan;
    SDL_UnlockMutex(s_cd.mu);
    return chan;
}

void LainCD_Shutdown(void)
{
    if (!s_cd.started)
        return;
    s_cd.quit = 1;
    SDL_WaitThread(s_cd.thread, NULL);
    s_cd.thread = NULL;
    s_cd.started = 0;
    if (s_cd.fp)
    {
        fclose(s_cd.fp);
        s_cd.fp = NULL;
    }
}

/* ------------------------------------------------------------------ */
/* Drive thread                                                         */
/* ------------------------------------------------------------------ */

static int cd_thread(void *arg)
{
    static u_char sector[LAIN_XA_RAW_SECTOR];
    (void)arg;

    s_cd.threadId = SDL_ThreadID();

    while (!s_cd.quit)
    {
        Uint64 now = SDL_GetPerformanceCounter();
        int haveSector = 0, lba = 0, burst = 0;
        unsigned int gen = 0;

        /* 1. command completions -> CdSync status + CdSyncCallback */
        for (;;)
        {
            Pending p;
            CdlCB cb;

            SDL_LockMutex(s_cd.mu);
            if (s_cd.pCount == 0 || s_cd.pend[s_cd.pHead].due > now)
            {
                SDL_UnlockMutex(s_cd.mu);
                break;
            }
            p = s_cd.pend[s_cd.pHead];
            s_cd.pHead = (s_cd.pHead + 1) % MAX_PENDING;
            s_cd.pCount--;
            s_cd.lastStatus = p.status;
            memcpy(s_cd.lastResult, p.result, 8);
            s_cd.completedSeq = p.seq;
            cb = s_cd.syncCb;
            SDL_UnlockMutex(s_cd.mu);

            if (cb)
            {
                LainCD_EnterIRQ();
                cb(p.status, p.result);
                LainCD_ExitIRQ();
            }
        }

        /* 2. sectors due */
        do
        {
            Uint64 period;
            int ok, feedAudio = 0;
            u_char event = 0, res[8];
            CdlCB readyCb;
            void (*dataCb)(void);

            haveSector = 0;

            SDL_LockMutex(s_cd.mu);
            period = cd_period_locked();
            if (s_cd.reading && (period == 0 || now >= s_cd.nextTime))
            {
                lba = s_cd.nextLBA++;
                s_cd.curLBA = lba;
                gen = s_cd.gen;
                haveSector = 1;
                s_cd.nextTime += period;
                /* fell far behind (debugger, stall): don't burst to catch up */
                if (period && now > s_cd.nextTime + 8 * period)
                    s_cd.nextTime = now + period;
            }
            SDL_UnlockMutex(s_cd.mu);

            if (!haveSector)
                break;

            ok = LainCD_ReadRawSector(lba, sector);

            SDL_LockMutex(s_cd.mu);
            if (gen != s_cd.gen)
            {
                /* stream was stopped/restarted during the read */
                SDL_UnlockMutex(s_cd.mu);
                break;
            }

            if (!ok)
            {
                cd_stop_stream_locked();
                event = CdlDataEnd;
            }
            else
            {
                memcpy(s_cd.lastHeader, sector + 12, 8);

                if ((s_cd.mode & CdlModeRT) && LainXA_IsAudioSector(sector))
                {
                    if (!(s_cd.mode & CdlModeSF) ||
                        (sector[16] == s_cd.filterFile && sector[17] == s_cd.filterChan))
                        feedAudio = 1;
                }
                else
                {
                    memcpy(s_cd.raw, sector, LAIN_XA_RAW_SECTOR);
                    if (s_cd.mode & CdlModeSize1)
                    {
                        s_cd.dataOfs = 12;
                        s_cd.dataLen = 2340;
                    }
                    else if (s_cd.mode & CdlModeSize0)
                    {
                        s_cd.dataOfs = 24;
                        s_cd.dataLen = 2328;
                    }
                    else
                    {
                        s_cd.dataOfs = 24;
                        s_cd.dataLen = 2048;
                    }
                    s_cd.dataRd = 0;
                    s_cd.dataReady = 1;
                    event = CdlDataReady;
                }
            }

            memset(res, 0, sizeof(res));
            res[0] = cd_stat_locked();
            readyCb = s_cd.readyCb;
            dataCb = s_cd.dataCb;
            SDL_UnlockMutex(s_cd.mu);

            if (feedAudio)
                LainXA_FeedSector(sector);

            if (event && readyCb)
            {
                LainCD_EnterIRQ();
                s_cd.getSectorCalled = 0;
                readyCb(event, res);
                if (s_cd.getSectorCalled && dataCb)
                    dataCb();
                LainCD_ExitIRQ();
            }

            /* "as fast as possible" mode: bounded bursts */
        } while (haveSector && ++burst < 64);

        SDL_Delay(1);
    }

    return 0;
}

/* ------------------------------------------------------------------ */
/* libcd functions PsyCross lacks                                      */
/* ------------------------------------------------------------------ */

int CdMix(CdlATV *vol)
{
    if (vol)
        LainXA_SetAttenuator(vol);
    return 1;
}

int CdMode(void)
{
    return s_cd.mode;
}

int CdStatus(void)
{
    int st;
    cd_early_init();
    SDL_LockMutex(s_cd.mu);
    st = cd_stat_locked();
    SDL_UnlockMutex(s_cd.mu);
    return st;
}

CdlLOC *CdLastPos(void)
{
    return &s_cd.lastPos;
}

int CdReady(int mode, u_char *result)
{
    if (mode == 0)
    {
        while (!s_cd.dataReady && s_cd.reading)
            SDL_Delay(1);
    }
    if (result)
        result[0] = (u_char)CdStatus();
    return s_cd.dataReady ? CdlDataReady : CdlNoIntr;
}

int CdReset(int mode)
{
    (void)mode;
    LainCD_Reset();
    return 1;
}

void CdFlush(void)
{
    cd_early_init();
    SDL_LockMutex(s_cd.mu);
    s_cd.pCount = 0;
    s_cd.completedSeq = s_cd.issuedSeq;
    SDL_UnlockMutex(s_cd.mu);
}

int CdDataSync(int mode)
{
    (void)mode;
    return 0;   /* CdGetSector copies synchronously: never busy */
}
