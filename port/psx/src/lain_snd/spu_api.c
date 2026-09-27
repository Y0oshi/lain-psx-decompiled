/* lain: libspu API on top of the software SPU (spu_core.c).
 *
 * Replaces PsyCross's src/psx/LIBSPU.C (compiled out). Implemented from
 * the public libspu reference behaviour; register semantics per psx-spx.
 *
 * NOTE (Lain-specific): the function the game links as "SpuRead"
 * (0x80067CAC) is really libspu's *write* routine: it calls the internal
 * main-RAM -> SPU transfer and clamps to 0x7EFF0 bytes, and libsnd's
 * SsVabTransBody calls it to upload the VAB body. So SpuRead here uploads
 * `size` bytes from `addr` to SPU RAM at the transfer start address, exactly
 * like SpuWrite. Use LainSpu_ReadRam() for a real SPU -> main RAM read.
 */
#include "spu_internal.h"
#include "psx/libspu.h"

#include <string.h>
#include <math.h>

extern SpuIRQCallbackProc      g_lspu_irq_cb;
extern SpuTransferCallbackProc g_lspu_transfer_cb;

static int s_transfer_mode = SPU_TRANSFER_BY_DMA;
static uint16_t s_sample_note[LSPU_NUM_VOICES];
static uint16_t s_note[LSPU_NUM_VOICES];
static uint32_t s_noise_voices, s_lfo_voices;
static int s_noise_clock;
static int s_initialised;

/* Malloc */

#define MALLOC_MAX 256
static struct { uint32_t addr, size; } s_blk[MALLOC_MAX];
static int s_nblk;
static int s_maxblk = 32;

void LSpu_MallocInit(int num)
{
    if (num <= 0) num = 32;
    if (num > MALLOC_MAX) num = MALLOC_MAX;
    s_maxblk = num;
    s_nblk = 0;
}

static uint32_t malloc_limit(void)
{
    if (g_lspu.reverb_reserved)
        return LSPU_RAM_SIZE - LSpu_ReverbWorkSize(g_lspu.reverb_mode);
    return LSPU_RAM_SIZE;
}

static int malloc_insert(uint32_t addr, uint32_t size)
{
    int i, j;
    if (s_nblk >= s_maxblk)
        return -1;
    for (i = 0; i < s_nblk && s_blk[i].addr < addr; i++)
        ;
    for (j = s_nblk; j > i; j--)
        s_blk[j] = s_blk[j - 1];
    s_blk[i].addr = addr;
    s_blk[i].size = size;
    s_nblk++;
    return (int)addr;
}

int SpuInitMalloc(int num, char *top)
{
    (void)top;
    LSpu_Lock();
    LSpu_MallocInit(num);
    LSpu_Unlock();
    return s_maxblk;
}

int SpuMalloc(int size)
{
    uint32_t need, pos, limit;
    int i, r = -1;

    if (size <= 0)
        return -1;
    need = ((uint32_t)size + 15u) & ~15u;

    LSpu_Lock();
    limit = malloc_limit();
    pos = LSPU_MALLOC_BASE;
    for (i = 0; i <= s_nblk; i++) {
        uint32_t end = i < s_nblk ? s_blk[i].addr : limit;
        if (end >= pos && end - pos >= need) {
            r = malloc_insert(pos, need);
            break;
        }
        if (i < s_nblk)
            pos = s_blk[i].addr + s_blk[i].size;
    }
    LSpu_Unlock();
    return r;
}

int SpuMallocWithStartAddr(unsigned int addr, int size)
{
    uint32_t need, a = addr & ~15u;
    int i, r = -1;

    if (size <= 0 || a < LSPU_MALLOC_BASE)
        return -1;
    need = ((uint32_t)size + 15u) & ~15u;

    LSpu_Lock();
    if (a + need <= malloc_limit()) {
        for (i = 0; i < s_nblk; i++) {
            if (a < s_blk[i].addr + s_blk[i].size && s_blk[i].addr < a + need)
                break;
        }
        if (i == s_nblk)
            r = malloc_insert(a, need);
    }
    LSpu_Unlock();
    return r;
}

void SpuFree(unsigned int addr)
{
    int i;
    LSpu_Lock();
    for (i = 0; i < s_nblk; i++) {
        if (s_blk[i].addr == addr) {
            for (; i < s_nblk - 1; i++)
                s_blk[i] = s_blk[i + 1];
            s_nblk--;
            break;
        }
    }
    LSpu_Unlock();
}

/* Init/quit */

void SpuInit(void)
{
    LSpu_Reset();
    LSpu_Lock();
    LSpu_MallocInit(32);
    /* audible defaults: main volume and CD input on */
    g_lspu.mvol_l = g_lspu.mvol_r = 0x3FFF;
    g_lspu.cd_vol_l = g_lspu.cd_vol_r = 0x3FFF;
    g_lspu.cd_mix = 1;
    memset(s_sample_note, 0, sizeof(s_sample_note));
    memset(s_note, 0, sizeof(s_note));
    s_noise_voices = s_lfo_voices = 0;
    LSpu_Unlock();
    g_lspu_irq_cb = NULL;
    g_lspu_transfer_cb = NULL;
    s_initialised = 1;
    LSpu_OpenOutput();
}

void SpuInitHot(void)
{
    if (!s_initialised)
        SpuInit();
}

void SpuStart(void)
{
    LSpu_OpenOutput();
}

void SpuQuit(void)
{
    LSpu_CloseOutput();
    LSpu_Lock();
    LSpu_KeyOff(0xFFFFFF);
    LSpu_Unlock();
    s_initialised = 0;
}

int SpuSetMute(int on_off)
{
    LSpu_Lock();
    g_lspu.mute = on_off == SPU_ON;
    LSpu_Unlock();
    return on_off;
}

int SpuGetMute(void)
{
    return g_lspu.mute ? SPU_ON : SPU_OFF;
}

void SpuSetEnv(SpuEnv *env)
{
    (void)env;
}

unsigned int SpuFlush(unsigned int ev)
{
    (void)ev;
    return 0;
}

/* Transfer */

unsigned int SpuSetTransferStartAddr(unsigned int addr)
{
    if (addr > LSPU_RAM_SIZE - 8)
        return 0;
    LSpu_Lock();
    g_lspu.transfer_addr = addr & ~7u;
    LSpu_Unlock();
    return addr;
}

unsigned int SpuGetTransferStartAddr(void)
{
    return g_lspu.transfer_addr;
}

int SpuSetTransferMode(int mode)
{
    s_transfer_mode = mode == SPU_TRANSFER_BY_DMA ? SPU_TRANSFER_BY_DMA : SPU_TRANSFER_BY_IO;
    return s_transfer_mode;
}

int SpuGetTransferMode(void)
{
    return s_transfer_mode;
}

static unsigned int do_write(const unsigned char *addr, unsigned int size, int advance)
{
    if (size > 0x7EFF0)
        size = 0x7EFF0;
    LSpu_Lock();
    if (addr)
        LSpu_RamWrite(g_lspu.transfer_addr, addr, size);
    else {
        static const uint8_t zero[256];
        unsigned int done = 0;
        while (done < size) {
            unsigned int n = size - done > sizeof(zero) ? sizeof(zero) : size - done;
            LSpu_RamWrite(g_lspu.transfer_addr + done, zero, n);
            done += n;
        }
    }
    if (advance)
        g_lspu.transfer_addr = (g_lspu.transfer_addr + size) & (LSPU_RAM_SIZE - 1);
    LSpu_Unlock();

    /* transfers complete immediately; DMA-complete callback */
    LSpu_DispatchCallbacks();
    if (g_lspu_transfer_cb)
        g_lspu_transfer_cb();
    return size;
}

unsigned int SpuWrite(unsigned char *addr, unsigned int size)
{
    return do_write(addr, size, 0);
}

unsigned int SpuWrite0(unsigned int size)
{
    return do_write(NULL, size, 0);
}

unsigned int SpuWritePartly(unsigned char *addr, unsigned int size)
{
    return do_write(addr, size, 1);
}

/* See the note at the top of the file: Lain's "SpuRead" uploads. */
unsigned int SpuRead(unsigned char *addr, unsigned int size)
{
    return do_write(addr, size, 0);
}

int SpuIsTransferCompleted(int flag)
{
    (void)flag;
    return 1;
}

SpuTransferCallbackProc SpuSetTransferCallback(SpuTransferCallbackProc func)
{
    SpuTransferCallbackProc old = g_lspu_transfer_cb;
    g_lspu_transfer_cb = func;
    return old;
}

/* Capture/IRQ */

int SpuReadDecodedData(SpuDecodedData *d_data, int flag)
{
    int half;
    LSpu_Lock();
    if (flag == SPU_CDONLY)
        memcpy(d_data, &g_lspu.ram[0x000], 0x800);
    else if (flag == SPU_VOICEONLY)
        memcpy((uint8_t *)d_data + 0x800, &g_lspu.ram[0x800], 0x800);
    else
        memcpy(d_data, &g_lspu.ram[0x000], 0x1000);
    half = g_lspu.capture_pos >= LSPU_CAPTURE_LEN / 2 ? SPU_DECODED_SECONDHALF : SPU_DECODED_FIRSTHALF;
    LSpu_Unlock();

    if (g_lspu_transfer_cb)
        g_lspu_transfer_cb();
    return half;
}

int SpuSetIRQ(int on_off)
{
    LSpu_Lock();
    switch (on_off) {
    case SPU_OFF:
        g_lspu.irq_enabled = 0;
        g_lspu.irq_fired = 0;
        break;
    case SPU_ON:
        if (!g_lspu.irq_enabled)
            g_lspu.irq_fired = 0;
        g_lspu.irq_enabled = 1;
        break;
    case SPU_RESET:
        g_lspu.irq_fired = 0;
        g_lspu.irq_enabled = 1;
        break;
    default:
        break;
    }
    LSpu_Unlock();
    return on_off;
}

int SpuGetIRQ(void)
{
    return g_lspu.irq_enabled ? SPU_ON : SPU_OFF;
}

unsigned int SpuSetIRQAddr(unsigned int addr)
{
    if (addr > LSPU_RAM_SIZE - 8)
        return 0;
    LSpu_Lock();
    g_lspu.irq_addr = addr & 0x7FFF8;
    LSpu_Unlock();
    return addr;
}

unsigned int SpuGetIRQAddr(void)
{
    return g_lspu.irq_addr;
}

SpuIRQCallbackProc SpuSetIRQCallback(SpuIRQCallbackProc func)
{
    SpuIRQCallbackProc old = g_lspu_irq_cb;
    g_lspu_irq_cb = func;
    return old;
}

/* Voices */

static uint16_t encode_vol(short vol, short mode, int use_mode)
{
    if (!use_mode || mode == SPU_VOICE_DIRECT16)
        return (uint16_t)vol & 0x7FFF;
    switch (mode) {
    case SPU_VOICE_LINEARIncN: return 0x8000 | (vol & 0x7F);
    case SPU_VOICE_LINEARIncR: return 0x9000 | (vol & 0x7F);
    case SPU_VOICE_LINEARDecN: return 0xA000 | (vol & 0x7F);
    case SPU_VOICE_LINEARDecR: return 0xB000 | (vol & 0x7F);
    case SPU_VOICE_EXPIncN:    return 0xC000 | (vol & 0x7F);
    case SPU_VOICE_EXPIncR:    return 0xD000 | (vol & 0x7F);
    default:                   return 0xE000 | (vol & 0x7F);
    }
}

static uint16_t note_to_pitch(uint16_t note, uint16_t sample_note)
{
    double n = (note >> 8) + (note & 0xFF) / 128.0;
    double s = (sample_note >> 8) + (sample_note & 0xFF) / 128.0;
    double p = 4096.0 * pow(2.0, (n - s) / 12.0);
    if (p > 0x3FFF) p = 0x3FFF;
    return (uint16_t)p;
}

static void set_voice(int i, const SpuVoiceAttr *a)
{
    LSpuVoice *v = &g_lspu.v[i];
    unsigned m = a->mask;

    if (m == 0)
        m = 0xFFFFFFFFu & ~(SPU_VOICE_ADSR_ADSR1 | SPU_VOICE_ADSR_ADSR2);  /* mask 0 = all */

    if (m & SPU_VOICE_VOLL)
        v->vol_l = encode_vol(a->volume.left, a->volmode.left, m & SPU_VOICE_VOLMODEL);
    if (m & SPU_VOICE_VOLR)
        v->vol_r = encode_vol(a->volume.right, a->volmode.right, m & SPU_VOICE_VOLMODER);
    if (m & SPU_VOICE_SAMPLE_NOTE)
        s_sample_note[i] = a->sample_note;
    if (m & SPU_VOICE_NOTE) {
        s_note[i] = a->note;
        v->pitch = note_to_pitch(a->note, s_sample_note[i]);
    }
    if (m & SPU_VOICE_PITCH)
        v->pitch = a->pitch;
    if (m & SPU_VOICE_WDSA)
        v->start = a->addr & 0x7FFF8;
    if (m & SPU_VOICE_LSAX) {
        v->repeat = a->loop_addr & 0x7FFF8;
        v->ignore_loop_flag = 1;
    }
    if (m & SPU_VOICE_ADSR_ADSR1)
        v->adsr1 = a->adsr1;
    if (m & SPU_VOICE_ADSR_ADSR2)
        v->adsr2 = a->adsr2;

    if (m & SPU_VOICE_ADSR_AMODE)
        v->adsr1 = (v->adsr1 & 0x7FFF) | (a->a_mode == SPU_VOICE_EXPIncN ? 0x8000 : 0);
    if (m & SPU_VOICE_ADSR_AR)
        v->adsr1 = (v->adsr1 & ~0x7F00) | ((a->ar & 0x7F) << 8);
    if (m & SPU_VOICE_ADSR_DR)
        v->adsr1 = (v->adsr1 & ~0x00F0) | ((a->dr & 0x0F) << 4);
    if (m & SPU_VOICE_ADSR_SL)
        v->adsr1 = (v->adsr1 & ~0x000F) | (a->sl & 0x0F);
    if (m & SPU_VOICE_ADSR_SMODE) {
        uint16_t bits;
        switch (a->s_mode) {
        case SPU_VOICE_LINEARDecN: bits = 0x4000; break;
        case SPU_VOICE_EXPIncN:    bits = 0x8000; break;
        case SPU_VOICE_EXPDec:     bits = 0xC000; break;
        default:                   bits = 0x0000; break;   /* linear increase */
        }
        v->adsr2 = (v->adsr2 & 0x3FFF) | bits;
    }
    if (m & SPU_VOICE_ADSR_SR)
        v->adsr2 = (v->adsr2 & ~0x1FC0) | ((a->sr & 0x7F) << 6);
    if (m & SPU_VOICE_ADSR_RMODE)
        v->adsr2 = (v->adsr2 & ~0x0020) | (a->r_mode == SPU_VOICE_EXPDec ? 0x20 : 0);
    if (m & SPU_VOICE_ADSR_RR)
        v->adsr2 = (v->adsr2 & ~0x001F) | (a->rr & 0x1F);
}

void SpuSetVoiceAttr(SpuVoiceAttr *arg)
{
    int i;
    LSpu_Lock();
    for (i = 0; i < LSPU_NUM_VOICES; i++)
        if (arg->voice & (1u << i))
            set_voice(i, arg);
    LSpu_Unlock();
}

void SpuNSetVoiceAttr(int vNum, SpuVoiceAttr *arg)
{
    if (vNum < 0 || vNum >= LSPU_NUM_VOICES)
        return;
    LSpu_Lock();
    set_voice(vNum, arg);
    LSpu_Unlock();
}

int SpuRSetVoiceAttr(int min_, int max_, SpuVoiceAttr *arg)
{
    int i;
    if (min_ < 0) min_ = 0;
    if (max_ >= LSPU_NUM_VOICES) max_ = LSPU_NUM_VOICES - 1;
    LSpu_Lock();
    for (i = min_; i <= max_; i++)
        if (arg->voice & (1u << i))
            set_voice(i, arg);
    LSpu_Unlock();
    return SPU_SUCCESS;
}

void SpuLSetVoiceAttr(int num, SpuLVoiceAttr *argList)
{
    int i;
    for (i = 0; i < num; i++)
        SpuNSetVoiceAttr(argList[i].voiceNum, &argList[i].attr);
}

static void get_voice(int i, SpuVoiceAttr *a)
{
    LSpuVoice *v = &g_lspu.v[i];
    a->volume.left = v->vol_l & 0x7FFF;
    a->volume.right = v->vol_r & 0x7FFF;
    a->volmode.left = a->volmode.right = 0;
    a->volumex.left = (short)((v->last_out * (int32_t)(int16_t)(v->vol_l << 1)) >> 15);
    a->volumex.right = (short)((v->last_out * (int32_t)(int16_t)(v->vol_r << 1)) >> 15);
    a->pitch = v->pitch;
    a->note = s_note[i];
    a->sample_note = s_sample_note[i];
    a->envx = (short)v->env_level;
    a->addr = v->start;
    a->loop_addr = v->repeat;
    a->a_mode = (v->adsr1 & 0x8000) ? SPU_VOICE_EXPIncN : SPU_VOICE_LINEARIncN;
    switch (v->adsr2 & 0xC000) {
    case 0x0000: a->s_mode = SPU_VOICE_LINEARIncN; break;
    case 0x4000: a->s_mode = SPU_VOICE_LINEARDecN; break;
    case 0x8000: a->s_mode = SPU_VOICE_EXPIncN; break;
    default:     a->s_mode = SPU_VOICE_EXPDec; break;
    }
    a->r_mode = (v->adsr2 & 0x20) ? SPU_VOICE_EXPDec : SPU_VOICE_LINEARDecN;
    a->ar = (v->adsr1 >> 8) & 0x7F;
    a->dr = (v->adsr1 >> 4) & 0x0F;
    a->sl = v->adsr1 & 0x0F;
    a->sr = (v->adsr2 >> 6) & 0x7F;
    a->rr = v->adsr2 & 0x1F;
    a->adsr1 = v->adsr1;
    a->adsr2 = v->adsr2;
}

void SpuGetVoiceAttr(SpuVoiceAttr *arg)
{
    int i;
    LSpu_Lock();
    for (i = 0; i < LSPU_NUM_VOICES; i++) {
        if (arg->voice & (1u << i)) {
            get_voice(i, arg);
            break;
        }
    }
    LSpu_Unlock();
}

void SpuNGetVoiceAttr(int vNum, SpuVoiceAttr *arg)
{
    if (vNum < 0 || vNum >= LSPU_NUM_VOICES)
        return;
    LSpu_Lock();
    get_voice(vNum, arg);
    LSpu_Unlock();
}

void SpuSetKey(int on_off, unsigned int voice_bit)
{
    LSpu_Lock();
    if (on_off == SPU_ON || on_off == SPU_ON_ENV_OFF)
        LSpu_KeyOn(voice_bit & 0xFFFFFF);
    else
        LSpu_KeyOff(voice_bit & 0xFFFFFF);
    LSpu_Unlock();
}

void SpuSetKeyOnWithAttr(SpuVoiceAttr *attr)
{
    LSpu_Lock();
    SpuSetVoiceAttr(attr);
    LSpu_KeyOn(attr->voice & 0xFFFFFF);
    LSpu_Unlock();
}

static int key_status(int i)
{
    LSpuVoice *v = &g_lspu.v[i];
    switch (v->env_phase) {
    case LSPU_ENV_ATTACK:
    case LSPU_ENV_DECAY:
    case LSPU_ENV_SUSTAIN:
        return v->env_level ? SPU_ON : SPU_ON_ENV_OFF;
    case LSPU_ENV_RELEASE:
        return SPU_OFF_ENV_ON;
    default:
        return SPU_OFF;
    }
}

int SpuGetKeyStatus(unsigned int voice_bit)
{
    int i, r = SPU_ERROR;
    LSpu_Lock();
    for (i = 0; i < LSPU_NUM_VOICES; i++) {
        if (voice_bit & (1u << i)) {
            r = key_status(i);
            break;
        }
    }
    LSpu_Unlock();
    return r;
}

void SpuGetAllKeysStatus(char *status)
{
    int i;
    LSpu_Lock();
    for (i = 0; i < LSPU_NUM_VOICES; i++)
        status[i] = (char)key_status(i);
    LSpu_Unlock();
}

int SpuRGetAllKeysStatus(int min_, int max_, char *status)
{
    int i;
    if (min_ < 0) min_ = 0;
    if (max_ >= LSPU_NUM_VOICES) max_ = LSPU_NUM_VOICES - 1;
    LSpu_Lock();
    for (i = min_; i <= max_; i++)
        status[i - min_] = (char)key_status(i);
    LSpu_Unlock();
    return SPU_SUCCESS;
}

void SpuGetVoiceEnvelopeAttr(int vNum, int *keyStat, short *envx)
{
    LSpu_Lock();
    *keyStat = key_status(vNum);
    *envx = (short)g_lspu.v[vNum].env_level;
    LSpu_Unlock();
}

void SpuGetVoiceEnvelope(int vNum, short *envx)
{
    *envx = (short)g_lspu.v[vNum].env_level;
}

unsigned int SpuSetNoiseVoice(int on_off, unsigned int voice_bit)
{
    if (on_off == SPU_ON) s_noise_voices |= voice_bit;
    else if (on_off == SPU_OFF) s_noise_voices &= ~voice_bit;
    else if (on_off == SPU_BIT) s_noise_voices = voice_bit;
    return s_noise_voices;
}

unsigned int SpuGetNoiseVoice(void) { return s_noise_voices; }
int SpuSetNoiseClock(int n_clock) { s_noise_clock = n_clock & 0x3F; return s_noise_clock; }
int SpuGetNoiseClock(void) { return s_noise_clock; }

unsigned int SpuSetPitchLFOVoice(int on_off, unsigned int voice_bit)
{
    if (on_off == SPU_ON) s_lfo_voices |= voice_bit;
    else if (on_off == SPU_OFF) s_lfo_voices &= ~voice_bit;
    else if (on_off == SPU_BIT) s_lfo_voices = voice_bit;
    return s_lfo_voices;
}

unsigned int SpuGetPitchLFOVoice(void) { return s_lfo_voices; }

/* small per-field helpers */

#define VOICE_SETTER(flag, field, value)   \
    SpuVoiceAttr attr;                     \
    memset(&attr, 0, sizeof(attr));        \
    attr.voice = SPU_VOICECH(vNum);        \
    attr.mask = (flag);                    \
    attr.field = (value);                  \
    SpuSetVoiceAttr(&attr)

void SpuSetVoiceVolume(int vNum, short volL, short volR)
{
    SpuVoiceAttr attr;
    memset(&attr, 0, sizeof(attr));
    attr.voice = SPU_VOICECH(vNum);
    attr.mask = SPU_VOICE_VOLL | SPU_VOICE_VOLR;
    attr.volume.left = volL;
    attr.volume.right = volR;
    SpuSetVoiceAttr(&attr);
}

void SpuSetVoiceVolumeAttr(int vNum, short volL, short volR, short volModeL, short volModeR)
{
    SpuVoiceAttr attr;
    memset(&attr, 0, sizeof(attr));
    attr.voice = SPU_VOICECH(vNum);
    attr.mask = SPU_VOICE_VOLL | SPU_VOICE_VOLR | SPU_VOICE_VOLMODEL | SPU_VOICE_VOLMODER;
    attr.volume.left = volL;
    attr.volume.right = volR;
    attr.volmode.left = volModeL;
    attr.volmode.right = volModeR;
    SpuSetVoiceAttr(&attr);
}

void SpuSetVoicePitch(int vNum, unsigned short pitch) { VOICE_SETTER(SPU_VOICE_PITCH, pitch, pitch); }
void SpuSetVoiceNote(int vNum, unsigned short note) { VOICE_SETTER(SPU_VOICE_NOTE, note, note); }
void SpuSetVoiceSampleNote(int vNum, unsigned short sampleNote) { VOICE_SETTER(SPU_VOICE_SAMPLE_NOTE, sample_note, sampleNote); }
void SpuSetVoiceStartAddr(int vNum, unsigned int startAddr) { VOICE_SETTER(SPU_VOICE_WDSA, addr, startAddr); }
void SpuSetVoiceLoopStartAddr(int vNum, unsigned int lsa) { VOICE_SETTER(SPU_VOICE_LSAX, loop_addr, lsa); }
void SpuSetVoiceAR(int vNum, unsigned short AR) { VOICE_SETTER(SPU_VOICE_ADSR_AR, ar, AR); }
void SpuSetVoiceDR(int vNum, unsigned short DR) { VOICE_SETTER(SPU_VOICE_ADSR_DR, dr, DR); }
void SpuSetVoiceSR(int vNum, unsigned short SR) { VOICE_SETTER(SPU_VOICE_ADSR_SR, sr, SR); }
void SpuSetVoiceRR(int vNum, unsigned short RR) { VOICE_SETTER(SPU_VOICE_ADSR_RR, rr, RR); }
void SpuSetVoiceSL(int vNum, unsigned short SL) { VOICE_SETTER(SPU_VOICE_ADSR_SL, sl, SL); }

void SpuSetVoiceARAttr(int vNum, unsigned short AR, int ARmode)
{
    SpuVoiceAttr attr;
    memset(&attr, 0, sizeof(attr));
    attr.voice = SPU_VOICECH(vNum);
    attr.mask = SPU_VOICE_ADSR_AR | SPU_VOICE_ADSR_AMODE;
    attr.ar = AR;
    attr.a_mode = ARmode;
    SpuSetVoiceAttr(&attr);
}

void SpuSetVoiceSRAttr(int vNum, unsigned short SR, int SRmode)
{
    SpuVoiceAttr attr;
    memset(&attr, 0, sizeof(attr));
    attr.voice = SPU_VOICECH(vNum);
    attr.mask = SPU_VOICE_ADSR_SR | SPU_VOICE_ADSR_SMODE;
    attr.sr = SR;
    attr.s_mode = SRmode;
    SpuSetVoiceAttr(&attr);
}

void SpuSetVoiceRRAttr(int vNum, unsigned short RR, int RRmode)
{
    SpuVoiceAttr attr;
    memset(&attr, 0, sizeof(attr));
    attr.voice = SPU_VOICECH(vNum);
    attr.mask = SPU_VOICE_ADSR_RR | SPU_VOICE_ADSR_RMODE;
    attr.rr = RR;
    attr.r_mode = RRmode;
    SpuSetVoiceAttr(&attr);
}

void SpuSetVoiceADSR(int vNum, unsigned short AR, unsigned short DR,
                     unsigned short SR, unsigned short RR, unsigned short SL)
{
    SpuVoiceAttr attr;
    memset(&attr, 0, sizeof(attr));
    attr.voice = SPU_VOICECH(vNum);
    attr.mask = SPU_VOICE_ADSR_AR | SPU_VOICE_ADSR_DR | SPU_VOICE_ADSR_SR |
                SPU_VOICE_ADSR_RR | SPU_VOICE_ADSR_SL;
    attr.ar = AR; attr.dr = DR; attr.sr = SR; attr.rr = RR; attr.sl = SL;
    SpuSetVoiceAttr(&attr);
}

void SpuSetVoiceADSRAttr(int vNum, unsigned short AR, unsigned short DR,
                         unsigned short SR, unsigned short RR, unsigned short SL,
                         int ARmode, int SRmode, int RRmode)
{
    SpuVoiceAttr attr;
    memset(&attr, 0, sizeof(attr));
    attr.voice = SPU_VOICECH(vNum);
    attr.mask = SPU_VOICE_ADSR_AR | SPU_VOICE_ADSR_DR | SPU_VOICE_ADSR_SR |
                SPU_VOICE_ADSR_RR | SPU_VOICE_ADSR_SL |
                SPU_VOICE_ADSR_AMODE | SPU_VOICE_ADSR_SMODE | SPU_VOICE_ADSR_RMODE;
    attr.ar = AR; attr.dr = DR; attr.sr = SR; attr.rr = RR; attr.sl = SL;
    attr.a_mode = ARmode; attr.s_mode = SRmode; attr.r_mode = RRmode;
    SpuSetVoiceAttr(&attr);
}

void SpuGetVoiceVolume(int vNum, short *volL, short *volR)
{
    *volL = (short)(g_lspu.v[vNum].vol_l & 0x7FFF);
    *volR = (short)(g_lspu.v[vNum].vol_r & 0x7FFF);
}

void SpuGetVoiceVolumeX(int vNum, short *volXL, short *volXR)
{
    SpuVoiceAttr a;
    SpuNGetVoiceAttr(vNum, &a);
    *volXL = a.volumex.left;
    *volXR = a.volumex.right;
}

void SpuGetVoicePitch(int vNum, unsigned short *pitch) { *pitch = g_lspu.v[vNum].pitch; }
void SpuGetVoiceNote(int vNum, unsigned short *note) { *note = s_note[vNum]; }
void SpuGetVoiceSampleNote(int vNum, unsigned short *sampleNote) { *sampleNote = s_sample_note[vNum]; }
void SpuGetVoiceStartAddr(int vNum, unsigned int *startAddr) { *startAddr = g_lspu.v[vNum].start; }
void SpuGetVoiceLoopStartAddr(int vNum, unsigned int *lsa) { *lsa = g_lspu.v[vNum].repeat; }

/* Common */

void SpuSetCommonAttr(SpuCommonAttr *attr)
{
    unsigned m = attr->mask;
    if (m == 0)
        m = 0xFFFFFFFFu;
    LSpu_Lock();
    if (m & SPU_COMMON_MVOLL)
        g_lspu.mvol_l = encode_vol(attr->mvol.left, attr->mvolmode.left, m & SPU_COMMON_MVOLMODEL);
    if (m & SPU_COMMON_MVOLR)
        g_lspu.mvol_r = encode_vol(attr->mvol.right, attr->mvolmode.right, m & SPU_COMMON_MVOLMODER);
    if (m & SPU_COMMON_CDVOLL)
        g_lspu.cd_vol_l = attr->cd.volume.left;
    if (m & SPU_COMMON_CDVOLR)
        g_lspu.cd_vol_r = attr->cd.volume.right;
    if (m & SPU_COMMON_CDREV)
        g_lspu.cd_reverb = attr->cd.reverb != 0;
    if (m & SPU_COMMON_CDMIX)
        g_lspu.cd_mix = attr->cd.mix != 0;
    LSpu_Unlock();
}

void SpuGetCommonAttr(SpuCommonAttr *attr)
{
    LSpu_Lock();
    attr->mvol.left = (short)(g_lspu.mvol_l & 0x7FFF);
    attr->mvol.right = (short)(g_lspu.mvol_r & 0x7FFF);
    attr->mvolmode.left = attr->mvolmode.right = 0;
    attr->mvolx.left = attr->mvol.left;
    attr->mvolx.right = attr->mvol.right;
    attr->cd.volume.left = g_lspu.cd_vol_l;
    attr->cd.volume.right = g_lspu.cd_vol_r;
    attr->cd.reverb = g_lspu.cd_reverb;
    attr->cd.mix = g_lspu.cd_mix;
    attr->ext.volume.left = attr->ext.volume.right = 0;
    attr->ext.reverb = attr->ext.mix = 0;
    LSpu_Unlock();
}

void SpuSetCommonMasterVolume(short mvol_left, short mvol_right)
{
    LSpu_Lock();
    g_lspu.mvol_l = (uint16_t)mvol_left & 0x7FFF;
    g_lspu.mvol_r = (uint16_t)mvol_right & 0x7FFF;
    LSpu_Unlock();
}

void SpuSetCommonMasterVolumeAttr(short mvol_left, short mvol_right, short mvolmode_left, short mvolmode_right)
{
    LSpu_Lock();
    g_lspu.mvol_l = encode_vol(mvol_left, mvolmode_left, 1);
    g_lspu.mvol_r = encode_vol(mvol_right, mvolmode_right, 1);
    LSpu_Unlock();
}

void SpuGetCommonMasterVolume(short *l, short *r) { *l = (short)(g_lspu.mvol_l & 0x7FFF); *r = (short)(g_lspu.mvol_r & 0x7FFF); }
void SpuGetCommonMasterVolumeX(short *l, short *r) { SpuGetCommonMasterVolume(l, r); }
void SpuGetCommonMasterVolumeAttr(short *l, short *r, short *ml, short *mr) { SpuGetCommonMasterVolume(l, r); *ml = *mr = 0; }

void SpuSetCommonCDMix(int cd_mix) { LSpu_Lock(); g_lspu.cd_mix = cd_mix == SPU_ON; LSpu_Unlock(); }
void SpuSetCommonCDVolume(short cd_left, short cd_right) { LSpu_Lock(); g_lspu.cd_vol_l = cd_left; g_lspu.cd_vol_r = cd_right; LSpu_Unlock(); }
void SpuSetCommonCDReverb(int cd_reverb) { LSpu_Lock(); g_lspu.cd_reverb = cd_reverb == SPU_ON; LSpu_Unlock(); }
void SpuGetCommonCDMix(int *cd_mix) { *cd_mix = g_lspu.cd_mix ? SPU_ON : SPU_OFF; }
void SpuGetCommonCDVolume(short *l, short *r) { *l = g_lspu.cd_vol_l; *r = g_lspu.cd_vol_r; }
void SpuGetCommonCDReverb(int *cd_reverb) { *cd_reverb = g_lspu.cd_reverb ? SPU_ON : SPU_OFF; }

/* Reverb */

int SpuSetReverb(int on_off)
{
    LSpu_Lock();
    if (on_off == SPU_ON || on_off == SPU_OFF)
        g_lspu.reverb_on = on_off == SPU_ON;
    LSpu_Unlock();
    return g_lspu.reverb_on ? SPU_ON : SPU_OFF;
}

int SpuGetReverb(void)
{
    return g_lspu.reverb_on ? SPU_ON : SPU_OFF;
}

int SpuSetReverbModeParam(SpuReverbAttr *attr)
{
    unsigned m = attr->mask;
    if (m == 0)
        m = 0xFFFFFFFFu;
    LSpu_Lock();
    if (m & SPU_REV_MODE) {
        int mode = attr->mode & ~SPU_REV_MODE_CLEAR_WA;
        if (mode < 0 || mode >= SPU_REV_MODE_MAX) {
            LSpu_Unlock();
            return SPU_ERROR;
        }
        if (mode != g_lspu.reverb_mode || (attr->mode & SPU_REV_MODE_CLEAR_WA))
            LSpu_ReverbClear();
        g_lspu.reverb_mode = mode;
        /* a mode change silences the wet output until depth is set again */
        if (!(m & (SPU_REV_DEPTHL | SPU_REV_DEPTHR)))
            g_lspu.reverb_depth_l = g_lspu.reverb_depth_r = 0;
    }
    if (m & SPU_REV_DEPTHL) g_lspu.reverb_depth_l = attr->depth.left;
    if (m & SPU_REV_DEPTHR) g_lspu.reverb_depth_r = attr->depth.right;
    if (m & SPU_REV_DELAYTIME) g_lspu.reverb_delay = attr->delay;
    if (m & SPU_REV_FEEDBACK) g_lspu.reverb_feedback = attr->feedback;
    LSpu_ReverbConfigure();
    LSpu_Unlock();
    return SPU_SUCCESS;
}

void SpuGetReverbModeParam(SpuReverbAttr *attr)
{
    attr->mode = g_lspu.reverb_mode;
    attr->depth.left = g_lspu.reverb_depth_l;
    attr->depth.right = g_lspu.reverb_depth_r;
    attr->delay = g_lspu.reverb_delay;
    attr->feedback = g_lspu.reverb_feedback;
}

int SpuSetReverbDepth(SpuReverbAttr *attr)
{
    unsigned m = attr->mask;
    if (m == 0)
        m = SPU_REV_DEPTHL | SPU_REV_DEPTHR;
    LSpu_Lock();
    if (m & SPU_REV_DEPTHL) g_lspu.reverb_depth_l = attr->depth.left;
    if (m & SPU_REV_DEPTHR) g_lspu.reverb_depth_r = attr->depth.right;
    LSpu_Unlock();
    return SPU_SUCCESS;
}

int SpuReserveReverbWorkArea(int on_off)
{
    if (on_off == SPU_ON || on_off == SPU_OFF)
        g_lspu.reverb_reserved = on_off == SPU_ON;
    return g_lspu.reverb_reserved ? SPU_ON : SPU_OFF;
}

int SpuIsReverbWorkAreaReserved(int on_off)
{
    (void)on_off;
    return g_lspu.reverb_reserved ? SPU_ON : SPU_OFF;
}

unsigned int SpuSetReverbVoice(int on_off, unsigned int voice_bit)
{
    LSpu_Lock();
    if (on_off == SPU_ON) g_lspu.reverb_voices |= voice_bit & 0xFFFFFF;
    else if (on_off == SPU_OFF) g_lspu.reverb_voices &= ~voice_bit;
    else if (on_off == SPU_BIT) g_lspu.reverb_voices = voice_bit & 0xFFFFFF;
    LSpu_Unlock();
    return g_lspu.reverb_voices;
}

unsigned int SpuGetReverbVoice(void)
{
    return g_lspu.reverb_voices;
}

int SpuClearReverbWorkArea(int mode)
{
    (void)mode;
    LSpu_Lock();
    LSpu_ReverbClear();
    LSpu_Unlock();
    return SPU_SUCCESS;
}

int SpuSetReverbModeType(int mode)
{
    SpuReverbAttr a;
    memset(&a, 0, sizeof(a));
    a.mask = SPU_REV_MODE;
    a.mode = mode;
    return SpuSetReverbModeParam(&a);
}

void SpuSetReverbModeDepth(short depth_left, short depth_right)
{
    LSpu_Lock();
    g_lspu.reverb_depth_l = depth_left;
    g_lspu.reverb_depth_r = depth_right;
    LSpu_Unlock();
}

void SpuSetReverbModeDelayTime(int delay)
{
    LSpu_Lock();
    g_lspu.reverb_delay = delay;
    LSpu_ReverbConfigure();
    LSpu_Unlock();
}

void SpuSetReverbModeFeedback(int feedback)
{
    LSpu_Lock();
    g_lspu.reverb_feedback = feedback;
    LSpu_ReverbConfigure();
    LSpu_Unlock();
}

void SpuGetReverbModeType(int *mode) { *mode = g_lspu.reverb_mode; }
void SpuGetReverbModeDepth(short *l, short *r) { *l = g_lspu.reverb_depth_l; *r = g_lspu.reverb_depth_r; }
void SpuGetReverbModeDelayTime(int *delay) { *delay = g_lspu.reverb_delay; }
void SpuGetReverbModeFeedback(int *feedback) { *feedback = g_lspu.reverb_feedback; }

/* ADSR / volume-mode getters */

void SpuGetVoiceAR(int vNum, unsigned short *AR) { *AR = (g_lspu.v[vNum].adsr1 >> 8) & 0x7F; }
void SpuGetVoiceDR(int vNum, unsigned short *DR) { *DR = (g_lspu.v[vNum].adsr1 >> 4) & 0x0F; }
void SpuGetVoiceSL(int vNum, unsigned short *SL) { *SL = g_lspu.v[vNum].adsr1 & 0x0F; }
void SpuGetVoiceSR(int vNum, unsigned short *SR) { *SR = (g_lspu.v[vNum].adsr2 >> 6) & 0x7F; }
void SpuGetVoiceRR(int vNum, unsigned short *RR) { *RR = g_lspu.v[vNum].adsr2 & 0x1F; }

void SpuGetVoiceARAttr(int vNum, unsigned short *AR, int *ARmode)
{
    SpuVoiceAttr a;
    SpuNGetVoiceAttr(vNum, &a);
    *AR = a.ar;
    *ARmode = a.a_mode;
}

void SpuGetVoiceSRAttr(int vNum, unsigned short *SR, int *SRmode)
{
    SpuVoiceAttr a;
    SpuNGetVoiceAttr(vNum, &a);
    *SR = a.sr;
    *SRmode = a.s_mode;
}

void SpuGetVoiceRRAttr(int vNum, unsigned short *RR, int *RRmode)
{
    SpuVoiceAttr a;
    SpuNGetVoiceAttr(vNum, &a);
    *RR = a.rr;
    *RRmode = a.r_mode;
}

void SpuGetVoiceADSR(int vNum, unsigned short *AR, unsigned short *DR,
                     unsigned short *SR, unsigned short *RR, unsigned short *SL)
{
    SpuVoiceAttr a;
    SpuNGetVoiceAttr(vNum, &a);
    *AR = a.ar; *DR = a.dr; *SR = a.sr; *RR = a.rr; *SL = a.sl;
}

void SpuGetVoiceADSRAttr(int vNum, unsigned short *AR, unsigned short *DR,
                         unsigned short *SR, unsigned short *RR, unsigned short *SL,
                         int *ARmode, int *SRmode, int *RRmode)
{
    SpuVoiceAttr a;
    SpuNGetVoiceAttr(vNum, &a);
    *AR = a.ar; *DR = a.dr; *SR = a.sr; *RR = a.rr; *SL = a.sl;
    *ARmode = a.a_mode; *SRmode = a.s_mode; *RRmode = a.r_mode;
}

void SpuGetVoiceVolumeAttr(int vNum, short *volL, short *volR, short *volModeL, short *volModeR)
{
    SpuGetVoiceVolume(vNum, volL, volR);
    *volModeL = *volModeR = 0;
}
