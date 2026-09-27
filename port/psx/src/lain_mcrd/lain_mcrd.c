/*
 * lain: libmcrd (memory card) for the Lain native port, backed by raw card
 * image files (the common 128 KiB ".mcd"/".mcr" format: 16 blocks of 8 KiB;
 * block 0 holds the "MC" header frame and the 15 directory frames).
 *
 * Replaces PsyCross's LIBMCRD.C (incomplete: no writes, no create/format).
 * Written from public descriptions of the card format and of the libmcrd
 * interface. Every command completes immediately; MemCardSync() then reports
 * it as done with its result. The whole card is kept in memory and written back
 * to its file after every change.
 */
#include "psx/libmcrd.h"

#include <stdio.h>
#include <string.h>

#define CARD_SIZE   (128 * 1024)
#define BLOCK_SIZE  8192
#define FRAME_SIZE  128
#define NUM_BLOCKS  16

/* directory frame attributes */
#define ATTR_FREE   0xA0
#define ATTR_FIRST  0x51
#define ATTR_MIDDLE 0x52
#define ATTR_LAST   0x53

static struct {
    char path[1024];
    unsigned char data[CARD_SIZE];
    int loaded;    /* file read (or found missing) */
    int present;   /* a card is in the slot (a path was given) */
} s_card[2];

static int s_started;
static int s_cmds = -1, s_result = -1, s_done;
static int s_openChan = -1, s_openBlock = -1, s_openSize;
static MemCB s_callback;

void LainMcrd_SetCardPath(int chan, const char *path)
{
    if (chan < 0 || chan > 1)
        return;
    snprintf(s_card[chan].path, sizeof s_card[chan].path, "%s", path ? path : "");
    s_card[chan].present = path && path[0];
    s_card[chan].loaded = 0;
}

static unsigned char *frame(int chan, int i)
{
    return s_card[chan].data + i * FRAME_SIZE;
}

static void fix_checksum(unsigned char *f)
{
    unsigned char x = 0;
    for (int i = 0; i < FRAME_SIZE - 1; i++)
        x ^= f[i];
    f[FRAME_SIZE - 1] = x;
}

static int card_load(int chan)
{
    if (!s_card[chan].present)
        return 0;
    if (!s_card[chan].loaded)
    {
        FILE *fp = fopen(s_card[chan].path, "rb");
        memset(s_card[chan].data, 0, CARD_SIZE);
        if (fp)
        {
            size_t n = fread(s_card[chan].data, 1, CARD_SIZE, fp);
            (void)n;
            fclose(fp);
        }
        s_card[chan].loaded = 1;
    }
    return 1;
}

static int card_save(int chan)
{
    FILE *fp = fopen(s_card[chan].path, "wb");
    if (!fp)
    {
        fprintf(stderr, "lain_mcrd: cannot write %s\n", s_card[chan].path);
        return 0;
    }
    size_t n = fwrite(s_card[chan].data, 1, CARD_SIZE, fp);
    fclose(fp);
    return n == CARD_SIZE;
}

static int card_formatted(int chan)
{
    return s_card[chan].data[0] == 'M' && s_card[chan].data[1] == 'C';
}

static void finish(int cmds, int result)
{
    s_cmds = cmds;
    s_result = result;
    s_done = 1;
    if (s_callback)
        s_callback(cmds, result);
}

/* Directory frame (1..15) of the file `name`, or -1. */
static int find_file(int chan, const char *name)
{
    for (int i = 1; i < NUM_BLOCKS; i++)
    {
        unsigned char *f = frame(chan, i);
        if (f[0] == ATTR_FIRST && strncmp((const char *)f + 0x0A, name, 20) == 0)
            return i;
    }
    return -1;
}

/* libmcrd */

void MemCardInit(int val)
{
    (void)val;
    s_cmds = s_result = -1;
    s_done = 0;
}

void MemCardEnd(void) {}

void MemCardStart(void)
{
    s_started = 1;
}

void MemCardStop(void)
{
    s_started = 0;
}

static int accept_result(int chan)
{
    if (chan < 0 || chan > 1 || !card_load(chan))
        return McErrCardNotExist;
    return card_formatted(chan) ? McErrNone : McErrNotFormat;
}

int MemCardExist(int chan)
{
    int r = (chan >= 0 && chan <= 1 && s_card[chan].present) ? McErrNone : McErrCardNotExist;
    finish(McFuncExist, r);
    return 1;
}

int MemCardAccept(int chan)
{
    finish(McFuncAccept, accept_result(chan));
    return 1;
}

int MemCardOpen(int chan, char *file, int flag)
{
    int r = accept_result(chan);
    (void)flag;
    if (r != McErrNone)
        return r;
    int i = find_file(chan, file);
    if (i < 0)
        return McErrFileNotExist;
    s_openChan = chan;
    s_openBlock = i;
    s_openSize = (int)(frame(chan, i)[4] | frame(chan, i)[5] << 8 | frame(chan, i)[6] << 16 | frame(chan, i)[7] << 24);
    return McErrNone;
}

void MemCardClose(void)
{
    s_openChan = -1;
    s_openBlock = -1;
}

/* Byte offset in the card image of file offset `ofs` (following the block chain). */
static int file_offset(int ofs)
{
    int block = s_openBlock;
    while (ofs >= BLOCK_SIZE)
    {
        unsigned char *f = frame(s_openChan, block);
        int next = f[8] | f[9] << 8;
        if (next == 0xFFFF || next > 14)
            return -1;
        block = next + 1;
        ofs -= BLOCK_SIZE;
    }
    return block * BLOCK_SIZE + ofs;
}

static int transfer(unsigned int *adrs, int ofs, int bytes, int write)
{
    if (s_openChan < 0 || ofs < 0 || bytes < 0 || ofs + bytes > s_openSize)
        return McErrCardInvalid;
    unsigned char *mem = (unsigned char *)adrs;
    while (bytes > 0)
    {
        int at = file_offset(ofs);
        int n = BLOCK_SIZE - ofs % BLOCK_SIZE;
        if (at < 0)
            return McErrCardInvalid;
        if (n > bytes)
            n = bytes;
        if (write)
            memcpy(s_card[s_openChan].data + at, mem, n);
        else
            memcpy(mem, s_card[s_openChan].data + at, n);
        mem += n;
        ofs += n;
        bytes -= n;
    }
    if (write && !card_save(s_openChan))
        return McErrCardInvalid;
    return McErrNone;
}

int MemCardReadData(unsigned int *adrs, int ofs, int bytes)
{
    finish(McFuncReadData, transfer(adrs, ofs, bytes, 0));
    return 1;
}

int MemCardWriteData(unsigned int *adrs, int ofs, int bytes)
{
    finish(McFuncWriteData, transfer(adrs, ofs, bytes, 1));
    return 1;
}

int MemCardReadFile(int chan, char *file, unsigned int *adrs, int ofs, int bytes)
{
    int r = MemCardOpen(chan, file, 1);
    if (r == McErrNone)
    {
        r = transfer(adrs, ofs, bytes, 0);
        MemCardClose();
    }
    finish(McFuncReadFile, r);
    return 1;
}

int MemCardWriteFile(int chan, char *file, unsigned int *adrs, int ofs, int bytes)
{
    int r = MemCardOpen(chan, file, 2);
    if (r == McErrNone)
    {
        r = transfer(adrs, ofs, bytes, 1);
        MemCardClose();
    }
    finish(McFuncWriteFile, r);
    return 1;
}

int MemCardCreateFile(int chan, char *file, int blocks)
{
    int r = accept_result(chan);
    int free_frames[NUM_BLOCKS], nfree = 0;

    if (r != McErrNone)
        return r;
    if (find_file(chan, file) >= 0)
        return McErrAlreadyExist;
    for (int i = 1; i < NUM_BLOCKS; i++)
        if ((frame(chan, i)[0] & 0xF0) == 0xA0)
            free_frames[nfree++] = i;
    if (blocks < 1 || nfree < blocks)
        return McErrBlockFull;

    for (int k = 0; k < blocks; k++)
    {
        int i = free_frames[k];
        unsigned char *f = frame(chan, i);
        unsigned size = (unsigned)(k == 0 ? blocks * BLOCK_SIZE : 0);
        int next = k + 1 < blocks ? free_frames[k + 1] - 1 : 0xFFFF;

        memset(f, 0, FRAME_SIZE);
        f[0] = k == 0 ? ATTR_FIRST : (k + 1 < blocks ? ATTR_MIDDLE : ATTR_LAST);
        f[4] = size & 0xFF;
        f[5] = (size >> 8) & 0xFF;
        f[6] = (size >> 16) & 0xFF;
        f[7] = (size >> 24) & 0xFF;
        f[8] = next & 0xFF;
        f[9] = (next >> 8) & 0xFF;
        if (k == 0)
            strncpy((char *)f + 0x0A, file, 20);
        fix_checksum(f);
        memset(s_card[chan].data + i * BLOCK_SIZE, 0, BLOCK_SIZE);
    }
    return card_save(chan) ? McErrNone : McErrCardInvalid;
}

int MemCardDeleteFile(int chan, char *file)
{
    int r = accept_result(chan);
    if (r != McErrNone)
        return r;
    int i = find_file(chan, file);
    if (i < 0)
        return McErrFileNotExist;
    while (i >= 1 && i < NUM_BLOCKS)
    {
        unsigned char *f = frame(chan, i);
        int next = f[8] | f[9] << 8;
        f[0] = (f[0] & 0x0F) | ATTR_FREE;
        fix_checksum(f);
        i = next == 0xFFFF ? -1 : next + 1;
    }
    return card_save(chan) ? McErrNone : McErrCardInvalid;
}

int MemCardFormat(int chan)
{
    if (chan < 0 || chan > 1 || !card_load(chan))
        return McErrCardNotExist;
    unsigned char *d = s_card[chan].data;
    memset(d, 0, CARD_SIZE);
    d[0] = 'M';
    d[1] = 'C';
    fix_checksum(frame(chan, 0));
    for (int i = 1; i < NUM_BLOCKS; i++)
    {
        unsigned char *f = frame(chan, i);
        f[0] = ATTR_FREE;
        f[8] = f[9] = 0xFF;
        fix_checksum(f);
    }
    /* broken-sector list (frames 16..35): none */
    for (int i = 16; i < 36; i++)
    {
        unsigned char *f = frame(chan, i);
        f[0] = f[1] = f[2] = f[3] = 0xFF;
        f[8] = f[9] = 0xFF;
        fix_checksum(f);
    }
    frame(chan, 63)[0] = 'M';
    frame(chan, 63)[1] = 'C';
    fix_checksum(frame(chan, 63));
    return card_save(chan) ? McErrNone : McErrCardInvalid;
}

int MemCardUnformat(int chan)
{
    if (chan < 0 || chan > 1 || !card_load(chan))
        return McErrCardNotExist;
    memset(s_card[chan].data, 0, CARD_SIZE);
    return card_save(chan) ? McErrNone : McErrCardInvalid;
}

int MemCardSync(int mode, int *cmds, int *rslt)
{
    (void)mode;
    if (!s_done)
        return -1; /* nothing registered */
    if (cmds)
        *cmds = s_cmds;
    if (rslt)
        *rslt = s_result;
    return 1;
}

MemCB MemCardCallback(MemCB func)
{
    MemCB old = s_callback;
    s_callback = func;
    return old;
}

int MemCardGetDirentry(int chan, char *name, struct DIRENTRY *dir, int *files, int ofs, int max)
{
    int n = 0;
    (void)name;
    if (card_load(chan) && card_formatted(chan))
    {
        for (int i = 1; i < NUM_BLOCKS && n < max; i++)
        {
            unsigned char *f = frame(chan, i);
            if (f[0] != ATTR_FIRST)
                continue;
            if (ofs-- > 0)
                continue;
            memset(&dir[n], 0, sizeof dir[n]);
            memcpy(dir[n].name, f + 0x0A, 20);
            dir[n].size = (int)(f[4] | f[5] << 8 | f[6] << 16 | f[7] << 24);
            n++;
        }
    }
    *files = n;
    return 0;
}
