/* lain: libsnd initialisation, tick, master volume and VAB sound banks. */
#include "snd_internal.h"
#include "psx/libspu.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

SndState g_snd;

#define VAB_MAGIC 0x56414270u   /* "pBAV" on disc */

/* Init */

static void reset_state(void)
{
    int i;
    for (i = 0; i < SS_MAX_VAB; i++) {
        free(g_snd.vab[i].hdr);
    }
    memset(&g_snd, 0, sizeof(g_snd));
    for (i = 0; i < LSPU_NUM_VOICES; i++)
        g_snd.voice[i].owner = SND_OWNER_NONE;
    g_snd.reserved = LSPU_NUM_VOICES;
    g_snd.tick_mode = SS_TICK60;
    g_snd.tick_hz = 60;
    g_snd.seq_max = SS_MAX_SEQ;
    g_snd.mvol_l = g_snd.mvol_r = 127;
}

void SsInit(void)
{
    LSpu_SetTickHook(NULL, 0);
    SpuInit();
    LSpu_Lock();
    reset_state();
    g_snd.init = 1;
    LSpu_MallocInit(64);
    /* main volume full, CD input mixed at full volume */
    g_lspu.mvol_l = g_lspu.mvol_r = 0x3FFF;
    g_lspu.cd_vol_l = g_lspu.cd_vol_r = 0x3FFF;
    g_lspu.cd_mix = 1;
    g_lspu.reverb_reserved = 1;
    LSpu_Unlock();
}

void SsInitHot(void)
{
    SsInit();
}

static int tick_hz_for(int mode)
{
    switch (mode & ~SS_NOTICK) {
    case SS_TICK240: return 240;
    case SS_TICK120: return 120;
    case SS_TICK50:  return 50;
    case SS_TICK60:
    case SS_TICKVSYNC:
    default:         return 60;
    }
}

static int auto_tick(void)
{
    return !(g_snd.tick_mode & SS_NOTICK) && g_snd.tick_mode != SS_NOTICK0;
}

int SsSetTickMode(int tick_mode)
{
    LSpu_Lock();
    g_snd.tick_mode = tick_mode;
    g_snd.tick_hz = tick_hz_for(tick_mode);
    if (g_snd.started)
        LSpu_SetTickHook(auto_tick() ? Snd_SeqTick : NULL, g_snd.tick_hz);
    LSpu_Unlock();
    return tick_mode;
}

void SsStart(void)
{
    LSpu_Lock();
    g_snd.started = 1;
    LSpu_SetTickHook(auto_tick() ? Snd_SeqTick : NULL, g_snd.tick_hz);
    LSpu_Unlock();
}

void SsStart2(void)
{
    SsStart();
}

void SsSeqCalledTbyT(void)
{
    LSpu_Lock();
    Snd_SeqTick();
    LSpu_Unlock();
}

void SsEnd(void)
{
    LSpu_SetTickHook(NULL, 0);
    LSpu_Lock();
    g_snd.started = 0;
    LSpu_Unlock();
}

void SsQuit(void)
{
    int i;
    SsEnd();
    LSpu_Lock();
    for (i = 0; i < g_snd.reserved; i++)
        Snd_KeyOffVoice(i);
    LSpu_Unlock();
    SpuQuit();
}

void SsSetTableSize(char *table, short s_max, short t_max)
{
    (void)table;
    (void)t_max;
    LSpu_Lock();
    g_snd.seq_max = s_max > 0 && s_max < SS_MAX_SEQ ? s_max : SS_MAX_SEQ;
    LSpu_Unlock();
}

char SsSetReservedVoice(char voices)
{
    LSpu_Lock();
    if (voices >= 1 && voices <= LSPU_NUM_VOICES)
        g_snd.reserved = voices;
    LSpu_Unlock();
    return (char)g_snd.reserved;
}

void SsSetMVol(short voll, short volr)
{
    if (voll < 0) voll = 0;
    if (voll > 127) voll = 127;
    if (volr < 0) volr = 0;
    if (volr > 127) volr = 127;
    LSpu_Lock();
    g_snd.mvol_l = voll;
    g_snd.mvol_r = volr;
    g_lspu.mvol_l = (uint16_t)(voll * 0x3FFF / 127);
    g_lspu.mvol_r = (uint16_t)(volr * 0x3FFF / 127);
    LSpu_Unlock();
}

void SsGetMVol(short *voll, short *volr)
{
    *voll = g_snd.mvol_l;
    *volr = g_snd.mvol_r;
}

void SsSetSerialAttr(char s_num, char attr, char mode)
{
    if (s_num != SS_SERIAL_A)
        return;
    LSpu_Lock();
    if (attr == SS_MIX)
        g_lspu.cd_mix = mode == SS_SON;
    else if (attr == SS_REV)
        g_lspu.cd_reverb = mode == SS_SON;
    LSpu_Unlock();
}

void SsSetSerialVol(char s_num, short voll, short volr)
{
    if (s_num != SS_SERIAL_A)
        return;
    if (voll < 0) voll = 0;
    if (voll > 127) voll = 127;
    if (volr < 0) volr = 0;
    if (volr > 127) volr = 127;
    LSpu_Lock();
    g_lspu.cd_vol_l = (int16_t)(voll * 0x7FFF / 127);
    g_lspu.cd_vol_r = (int16_t)(volr * 0x7FFF / 127);
    LSpu_Unlock();
}

/* VAB */

const ProgAtr *Snd_Prog(const SndVab *vab, int prog)
{
    if (prog < 0 || prog >= 128)
        return NULL;
    return (const ProgAtr *)(vab->hdr + 0x20 + prog * 16);
}

const VagAtr *Snd_Tone(const SndVab *vab, int prog, int tone)
{
    int blk;
    if (prog < 0 || prog >= 128 || tone < 0 || tone >= 16)
        return NULL;
    blk = vab->prog_block[prog];
    if (blk < 0)
        return NULL;
    return (const VagAtr *)(vab->hdr + 0x820 + (blk * 16 + tone) * 32);
}

short SsVabOpenHead(unsigned char *addr, short vab_id)
{
    const VabHdr *h = (const VabHdr *)addr;
    uint32_t size, total;
    int id, i, blk;
    SndVab *vab;
    const uint16_t *vt;

    if (!addr || (uint32_t)h->form != VAB_MAGIC)
        return -1;
    if (h->ps == 0 || h->ps > 128 || h->vs > 254)
        return -1;

    LSpu_Lock();
    if (vab_id >= 0 && vab_id < SS_MAX_VAB) {
        id = vab_id;
        if (g_snd.vab[id].used) {
            LSpu_Unlock();
            return -1;
        }
    } else {
        for (id = 0; id < SS_MAX_VAB && g_snd.vab[id].used; id++)
            ;
        if (id == SS_MAX_VAB) {
            LSpu_Unlock();
            return -1;
        }
    }

    vab = &g_snd.vab[id];
    memset(vab, 0, sizeof(*vab));
    size = 0x20 + 0x800 + h->ps * 0x200 + 0x200;
    vab->hdr = (uint8_t *)malloc(size);
    if (!vab->hdr) {
        LSpu_Unlock();
        return -1;
    }
    memcpy(vab->hdr, addr, size);
    vab->hdr_size = size;
    vab->ps = h->ps;
    vab->ts = h->ts;
    vab->vs = h->vs;
    vab->mvol = h->mvol;
    vab->pan = h->pan;

    /* programs with tones take the tone blocks in order */
    blk = 0;
    for (i = 0; i < 128; i++) {
        const ProgAtr *p = (const ProgAtr *)(vab->hdr + 0x20 + i * 16);
        if (p->tones > 0 && blk < vab->ps)
            vab->prog_block[i] = blk++;
        else
            vab->prog_block[i] = -1;
    }

    vt = (const uint16_t *)(vab->hdr + 0x820 + vab->ps * 0x200);
    total = 0;
    for (i = 1; i <= vab->vs; i++) {
        vab->vag_size[i] = (uint32_t)vt[i] << 3;
        total += vab->vag_size[i];
    }
    vab->body_size = total;
    vab->spu_base = total ? SpuMalloc((int)total) : -1;
    if (total && vab->spu_base < 0) {
        free(vab->hdr);
        memset(vab, 0, sizeof(*vab));
        LSpu_Unlock();
        return -1;
    }
    total = 0;
    for (i = 1; i <= vab->vs; i++) {
        vab->vag_addr[i] = (uint32_t)vab->spu_base + total;
        total += vab->vag_size[i];
    }
    vab->used = 1;
    LSpu_Unlock();
    return (short)id;
}

short SsVabTransBody(unsigned char *addr, short vab_id)
{
    SndVab *vab;
    if (vab_id < 0 || vab_id >= SS_MAX_VAB || !addr)
        return -1;
    LSpu_Lock();
    vab = &g_snd.vab[vab_id];
    if (!vab->used) {
        LSpu_Unlock();
        return -1;
    }
    if (vab->body_size)
        LSpu_RamWrite((uint32_t)vab->spu_base, addr, vab->body_size);
    vab->body_loaded = 1;
    LSpu_Unlock();
    LSpu_DispatchCallbacks();
    return vab_id;
}

short SsVabTransCompleted(short immediate_flag)
{
    (void)immediate_flag;
    return 1;
}

short SsVabOpen(unsigned char *addr, short vab_id)
{
    const VabHdr *h = (const VabHdr *)addr;
    short id = SsVabOpenHead(addr, vab_id);
    if (id < 0)
        return id;
    return SsVabTransBody(addr + 0x20 + 0x800 + h->ps * 0x200 + 0x200, id);
}

void SsVabClose(short vab_id)
{
    SndVab *vab;
    int i;
    if (vab_id < 0 || vab_id >= SS_MAX_VAB)
        return;
    LSpu_Lock();
    vab = &g_snd.vab[vab_id];
    if (vab->used) {
        for (i = 0; i < LSPU_NUM_VOICES; i++) {
            if (g_snd.voice[i].owner != SND_OWNER_NONE && g_snd.voice[i].vab == vab_id) {
                LSpu_KeyOff(1u << i);
                g_lspu.v[i].env_phase = LSPU_ENV_OFF;
                g_lspu.v[i].env_level = 0;
                g_snd.voice[i].owner = SND_OWNER_NONE;
            }
        }
        if (vab->spu_base >= 0)
            SpuFree((unsigned int)vab->spu_base);
        free(vab->hdr);
        memset(vab, 0, sizeof(*vab));
    }
    LSpu_Unlock();
}

/* Reverb */

short SsUtSetReverbType(short type)
{
    SpuReverbAttr a;
    if (type < 0 || type >= SPU_REV_MODE_MAX)
        return -1;
    memset(&a, 0, sizeof(a));
    a.mask = SPU_REV_MODE;
    a.mode = type;
    SpuSetReverbModeParam(&a);
    SpuReserveReverbWorkArea(SPU_ON);
    g_snd.rev_type = type;
    return type;
}

short SsUtGetReverbType(void)
{
    return (short)g_snd.rev_type;
}

void SsUtSetReverbDepth(short ldepth, short rdepth)
{
    SpuReverbAttr a;
    if (ldepth < 0) ldepth = 0;
    if (ldepth > 127) ldepth = 127;
    if (rdepth < 0) rdepth = 0;
    if (rdepth > 127) rdepth = 127;
    memset(&a, 0, sizeof(a));
    a.mask = SPU_REV_DEPTHL | SPU_REV_DEPTHR;
    a.depth.left = (short)(ldepth * 0x7FFF / 127);
    a.depth.right = (short)(rdepth * 0x7FFF / 127);
    SpuSetReverbDepth(&a);
}

void SsUtSetReverbDelay(short delay)
{
    SpuSetReverbModeDelayTime(delay);
}

void SsUtSetReverbFeedback(short feedback)
{
    SpuSetReverbModeFeedback(feedback);
}

void SsUtReverbOn(void)
{
    SpuSetReverb(SPU_ON);
}

void SsUtReverbOff(void)
{
    SpuSetReverb(SPU_OFF);
}
