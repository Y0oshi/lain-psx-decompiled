/* lain: software SPU mixer.
 *
 * 24 voices of SPU-ADPCM with loop flags, ADSR envelopes, linear
 * interpolation, per-voice volume, main volume, CD input, a simple
 * algorithmic reverb, the capture buffers at SPU RAM 0x000-0xFFF and the SPU
 * IRQ. Output: one 44.1 kHz stereo s16 stream (SDL audio), or pulled offline
 * with LainSpu_Render(). Behaviour follows the public psx-spx "Sound
 * Processing Unit" documentation.
 */
#include "spu_internal.h"
#include "psx/libspu.h"

#include <SDL.h>
#include <string.h>
#include <stdio.h>
#include <math.h>

LSpuState g_lspu;

static SDL_mutex *s_lock;
static SDL_mutex *s_cd_lock;
static SDL_AudioDeviceID s_dev;
static int s_output_enabled = 1;
static float s_gain = 1.0f;

SpuIRQCallbackProc      g_lspu_irq_cb;
SpuTransferCallbackProc g_lspu_transfer_cb;

static void (*s_tick_fn)(void);
static int s_tick_hz;
static int s_tick_acc;

/* Lock */

static void ensure_locks(void)
{
    if (!s_lock)
        s_lock = SDL_CreateMutex();      /* SDL mutexes are recursive */
    if (!s_cd_lock)
        s_cd_lock = SDL_CreateMutex();
}

void LSpu_Lock(void)
{
    ensure_locks();
    SDL_LockMutex(s_lock);
}

void LSpu_Unlock(void)
{
    SDL_UnlockMutex(s_lock);
}

void LSpu_SetTickHook(void (*fn)(void), int hz)
{
    LSpu_Lock();
    s_tick_fn = fn;
    s_tick_hz = hz > 0 ? hz : 0;
    s_tick_acc = 0;
    LSpu_Unlock();
}

/* IRQ */

static inline void irq_check8(uint32_t addr)
{
    if (g_lspu.irq_enabled && !g_lspu.irq_fired && ((addr ^ g_lspu.irq_addr) & 0x7FFF8u) == 0) {
        g_lspu.irq_fired = 1;
        g_lspu.irq_pending = 1;
    }
}

static inline void irq_check_block(uint32_t addr)
{
    if (g_lspu.irq_enabled && !g_lspu.irq_fired && ((addr ^ g_lspu.irq_addr) & 0x7FFF0u) == 0) {
        g_lspu.irq_fired = 1;
        g_lspu.irq_pending = 1;
    }
}

void LSpu_RamWrite(uint32_t addr, const void *src, uint32_t size)
{
    const uint8_t *s = (const uint8_t *)src;
    uint32_t i;

    addr &= LSPU_RAM_SIZE - 1;
    for (i = 0; i < size; i++) {
        g_lspu.ram[(addr + i) & (LSPU_RAM_SIZE - 1)] = s[i];
    }
    if (g_lspu.irq_enabled && !g_lspu.irq_fired && size) {
        uint32_t off = (g_lspu.irq_addr - addr) & (LSPU_RAM_SIZE - 1);
        if (off < size) {
            g_lspu.irq_fired = 1;
            g_lspu.irq_pending = 1;
        }
    }
}

void LSpu_DispatchCallbacks(void)
{
    int irq, xfer;

    LSpu_Lock();
    irq = g_lspu.irq_pending;
    g_lspu.irq_pending = 0;
    xfer = g_lspu.transfer_pending;
    g_lspu.transfer_pending = 0;
    LSpu_Unlock();

    if (irq && g_lspu_irq_cb)
        g_lspu_irq_cb();
    if (xfer && g_lspu_transfer_cb)
        g_lspu_transfer_cb();
}

/* ADPCM */

static const int s_f0[5] = { 0, 60, 115, 98, 122 };
static const int s_f1[5] = { 0, 0, -52, -55, -60 };

static void decode_block(LSpuVoice *v)
{
    const uint8_t *b = &g_lspu.ram[v->cur & 0x7FFF0];
    int shift = b[0] & 0x0F;
    int filt = (b[0] >> 4) & 0x07;
    int f0, f1, i;
    int32_t h1 = v->hist1, h2 = v->hist2;

    if (shift > 12)
        shift = 9;                 /* reserved shift values act like 9 */
    if (filt > 4)
        filt = 4;
    f0 = s_f0[filt];
    f1 = s_f1[filt];

    for (i = 0; i < 28; i++) {
        int nib = (b[2 + (i >> 1)] >> ((i & 1) * 4)) & 0x0F;
        int32_t s = (int16_t)(nib << 12);
        s >>= shift;
        s += (h1 * f0 + h2 * f1 + 32) >> 6;
        if (s > 32767) s = 32767;
        if (s < -32768) s = -32768;
        v->block[i] = (int16_t)s;
        h2 = h1;
        h1 = s;
    }
    v->hist1 = (int16_t)h1;
    v->hist2 = (int16_t)h2;

    if ((b[1] & 4) && !v->ignore_loop_flag)
        v->repeat = v->cur & 0x7FFF0;

    irq_check_block(v->cur);
}

static void end_block(LSpuVoice *v)
{
    uint8_t flags = g_lspu.ram[(v->cur & 0x7FFF0) + 1];

    if (flags & 1) {
        v->endx = 1;
        v->cur = v->repeat;
        if (!(flags & 2)) {
            /* loop end + mute: release with level 0 */
            v->env_phase = LSPU_ENV_OFF;
            v->env_level = 0;
        }
    } else {
        v->cur = (v->cur + 16) & 0x7FFF0;
    }
    decode_block(v);
}

/* ADSR */

static void env_tick(LSpuVoice *v)
{
    int rate, exp, dec;
    int shift, st;
    int32_t step;
    uint32_t cycles;

    switch (v->env_phase) {
    case LSPU_ENV_ATTACK:
        rate = (v->adsr1 >> 8) & 0x7F; exp = v->adsr1 >> 15; dec = 0;
        break;
    case LSPU_ENV_DECAY:
        rate = ((v->adsr1 >> 4) & 0x0F) << 2; exp = 1; dec = 1;
        break;
    case LSPU_ENV_SUSTAIN:
        rate = (v->adsr2 >> 6) & 0x7F; exp = v->adsr2 >> 15; dec = (v->adsr2 >> 14) & 1;
        break;
    case LSPU_ENV_RELEASE:
        rate = (v->adsr2 & 0x1F) << 2; exp = (v->adsr2 >> 5) & 1; dec = 1;
        break;
    default:
        return;
    }

    shift = rate >> 2;
    st = rate & 3;
    step = dec ? (-8 + st) : (7 - st);
    cycles = 1u << (shift > 11 ? shift - 11 : 0);
    if (shift < 11)
        step *= 1 << (11 - shift);
    if (exp && !dec && v->env_level > 0x6000)
        cycles <<= 2;
    if (exp && dec)
        step = (step * v->env_level) >> 15;

    if (++v->env_wait < cycles)
        return;
    v->env_wait = 0;

    v->env_level += step;
    if (v->env_level > 0x7FFF) v->env_level = 0x7FFF;
    if (v->env_level < 0) v->env_level = 0;

    switch (v->env_phase) {
    case LSPU_ENV_ATTACK:
        if (v->env_level >= 0x7FFF) {
            v->env_phase = LSPU_ENV_DECAY;
            v->env_wait = 0;
        }
        break;
    case LSPU_ENV_DECAY:
        if ((v->env_level >> 11) <= (v->adsr1 & 0x0F)) {
            v->env_phase = LSPU_ENV_SUSTAIN;
            v->env_wait = 0;
        }
        break;
    case LSPU_ENV_RELEASE:
        if (v->env_level <= 0)
            v->env_phase = LSPU_ENV_OFF;
        break;
    default:
        break;
    }
}

/* Keys */

void LSpu_KeyOn(uint32_t mask)
{
    int i;
    for (i = 0; i < LSPU_NUM_VOICES; i++) {
        LSpuVoice *v;
        if (!(mask & (1u << i)))
            continue;
        v = &g_lspu.v[i];
        v->cur = v->start & 0x7FFF0;
        v->repeat = v->cur;
        v->ignore_loop_flag = 0;
        v->hist1 = v->hist2 = 0;
        v->counter = 0;
        v->idx = 0;
        v->endx = 0;
        decode_block(v);
        v->prev = 0;
        v->curs = v->block[0];
        v->env_phase = LSPU_ENV_ATTACK;
        v->env_level = 0;
        v->env_wait = 0;
    }
}

void LSpu_KeyOff(uint32_t mask)
{
    int i;
    for (i = 0; i < LSPU_NUM_VOICES; i++) {
        LSpuVoice *v = &g_lspu.v[i];
        if ((mask & (1u << i)) && v->env_phase != LSPU_ENV_OFF) {
            v->env_phase = LSPU_ENV_RELEASE;
            v->env_wait = 0;
        }
    }
}

int LSpu_VoiceBusy(int i)
{
    return g_lspu.v[i].env_phase != LSPU_ENV_OFF;
}

/* Reverb */
/* A small Schroeder/Freeverb-style stereo reverb standing in for the SPU's
 * reverb unit; the mode picks size/decay, depth scales the wet output. */

#define RV_COMBS 4
#define RV_APS   2
#define RV_COMB_MAX 2600
#define RV_AP_MAX   800
#define RV_DELAY_MAX (LSPU_RATE * 3 / 4)

typedef struct {
    float buf[RV_COMB_MAX];
    int len, idx;
    float store;
} RvComb;

typedef struct {
    float buf[RV_AP_MAX];
    int len, idx;
} RvAp;

static struct {
    RvComb comb[2][RV_COMBS];
    RvAp ap[2][RV_APS];
    float feedback, damp, gain;
    int echo;                  /* 1: ECHO/DELAY delay line instead of the room */
    float dl[2][RV_DELAY_MAX];
    int dl_len, dl_idx;
    float dl_fb;
} s_rv;

static const int s_comb_len[RV_COMBS] = { 1116, 1277, 1422, 1617 };
static const int s_ap_len[RV_APS] = { 556, 341 };

typedef struct { float size, feedback, damp; } RvMode;

static const RvMode s_rv_modes[SPU_REV_MODE_MAX] = {
    { 0.0f, 0.0f, 0.0f },   /* OFF */
    { 0.45f, 0.72f, 0.40f }, /* ROOM */
    { 0.35f, 0.66f, 0.45f }, /* STUDIO_A */
    { 0.55f, 0.76f, 0.40f }, /* STUDIO_B */
    { 0.75f, 0.81f, 0.35f }, /* STUDIO_C */
    { 1.00f, 0.86f, 0.30f }, /* HALL */
    { 1.45f, 0.91f, 0.25f }, /* SPACE */
    { 0.0f, 0.0f, 0.0f },   /* ECHO  (delay line) */
    { 0.0f, 0.0f, 0.0f },   /* DELAY (delay line) */
    { 0.22f, 0.88f, 0.05f }, /* PIPE */
};

uint32_t LSpu_ReverbWorkSize(int mode)
{
    static const uint32_t sizes[SPU_REV_MODE_MAX] = {
        0x10, 0x26C0, 0x1F40, 0x4840, 0x6FE0, 0xADE0, 0xF6C0, 0x18040, 0x18040, 0x3C00
    };
    if (mode < 0 || mode >= SPU_REV_MODE_MAX)
        return 0;
    return sizes[mode];
}

void LSpu_ReverbClear(void)
{
    int c, i;
    for (c = 0; c < 2; c++) {
        for (i = 0; i < RV_COMBS; i++) {
            memset(s_rv.comb[c][i].buf, 0, sizeof(s_rv.comb[c][i].buf));
            s_rv.comb[c][i].store = 0;
        }
        for (i = 0; i < RV_APS; i++)
            memset(s_rv.ap[c][i].buf, 0, sizeof(s_rv.ap[c][i].buf));
        memset(s_rv.dl[c], 0, sizeof(s_rv.dl[c]));
    }
}

void LSpu_ReverbConfigure(void)
{
    int mode = g_lspu.reverb_mode;
    int c, i;

    if (mode < 0 || mode >= SPU_REV_MODE_MAX)
        mode = 0;

    if (mode == SPU_REV_MODE_ECHO || mode == SPU_REV_MODE_DELAY) {
        int len = (int)((g_lspu.reverb_delay & 0x7F) / 127.0f * (RV_DELAY_MAX - 1));
        s_rv.echo = 1;
        s_rv.dl_len = len < 1 ? 1 : len;
        s_rv.dl_idx %= s_rv.dl_len;
        s_rv.dl_fb = mode == SPU_REV_MODE_ECHO ? (g_lspu.reverb_feedback & 0x7F) / 127.0f * 0.9f : 0.0f;
        return;
    }

    s_rv.echo = 0;
    s_rv.feedback = s_rv_modes[mode].feedback;
    s_rv.damp = s_rv_modes[mode].damp;
    s_rv.gain = 0.25f * (1.0f - s_rv.feedback);
    for (c = 0; c < 2; c++) {
        for (i = 0; i < RV_COMBS; i++) {
            int len = (int)(s_comb_len[i] * s_rv_modes[mode].size) + c * 23;
            if (len < 16) len = 16;
            if (len > RV_COMB_MAX) len = RV_COMB_MAX;
            s_rv.comb[c][i].len = len;
            s_rv.comb[c][i].idx %= len;
        }
        for (i = 0; i < RV_APS; i++) {
            int len = s_ap_len[i] + c * 23;
            s_rv.ap[c][i].len = len;
            s_rv.ap[c][i].idx %= len;
        }
    }
}

static void reverb_process(int32_t in_l, int32_t in_r, float *out_l, float *out_r)
{
    float in[2], out[2];
    int c, i;

    in[0] = (float)in_l;
    in[1] = (float)in_r;

    if (s_rv.echo) {
        for (c = 0; c < 2; c++) {
            float d = s_rv.dl[c][s_rv.dl_idx];
            s_rv.dl[c][s_rv.dl_idx] = in[c] + d * s_rv.dl_fb;
            out[c] = d;
        }
        if (++s_rv.dl_idx >= s_rv.dl_len)
            s_rv.dl_idx = 0;
    } else {
        for (c = 0; c < 2; c++) {
            float x = in[c] * s_rv.gain;
            float acc = 0.0f;
            for (i = 0; i < RV_COMBS; i++) {
                RvComb *cb = &s_rv.comb[c][i];
                float y = cb->buf[cb->idx];
                cb->store = y * (1.0f - s_rv.damp) + cb->store * s_rv.damp;
                cb->buf[cb->idx] = x + cb->store * s_rv.feedback;
                if (++cb->idx >= cb->len) cb->idx = 0;
                acc += y;
            }
            for (i = 0; i < RV_APS; i++) {
                RvAp *ap = &s_rv.ap[c][i];
                float b = ap->buf[ap->idx];
                float y = b - acc;
                ap->buf[ap->idx] = acc + b * 0.5f;
                if (++ap->idx >= ap->len) ap->idx = 0;
                acc = y;
            }
            out[c] = acc;
        }
    }
    *out_l = out[0];
    *out_r = out[1];
}

/* Mixer */

static inline int32_t vol_reg(uint16_t reg)
{
    if (reg & 0x8000) {
        /* sweep mode: approximated by its end point */
        return (reg & 0x2000) ? 0 : 0x7FFF;
    }
    return (int16_t)(uint16_t)(reg << 1);
}

static inline int32_t clamp16(int32_t x)
{
    return x > 32767 ? 32767 : (x < -32768 ? -32768 : x);
}

static inline int16_t voice_sample(LSpuVoice *v)
{
    int32_t s, out;
    uint32_t step;

    if (v->env_phase == LSPU_ENV_OFF) {
        v->last_out = 0;
        return 0;
    }

    s = v->prev + (((v->curs - v->prev) * (int32_t)(v->counter & 0xFFF)) >> 12);
    env_tick(v);
    out = (s * v->env_level) >> 15;

    step = v->pitch;
    if (step > 0x4000)
        step = 0x4000;
    v->counter += step;
    while (v->counter >= 0x1000) {
        v->counter -= 0x1000;
        if (++v->idx >= 28) {
            v->idx = 0;
            end_block(v);
        }
        v->prev = v->curs;
        v->curs = v->block[v->idx];
    }

    v->last_out = (int16_t)out;
    return (int16_t)out;
}

static inline void cap_write(uint32_t base, int16_t s)
{
    uint32_t a = base + (uint32_t)g_lspu.capture_pos * 2;
    g_lspu.ram[a] = (uint8_t)(s & 0xFF);
    g_lspu.ram[a + 1] = (uint8_t)((uint16_t)s >> 8);
    irq_check8(a);
}

static void mix_frame(int16_t cd_l, int16_t cd_r, int16_t *out)
{
    int32_t dl = 0, dr = 0, rl = 0, rr = 0;
    int16_t v1 = 0, v3 = 0;
    int i;

    for (i = 0; i < LSPU_NUM_VOICES; i++) {
        LSpuVoice *v = &g_lspu.v[i];
        int32_t s, l, r;
        if (v->env_phase == LSPU_ENV_OFF) {
            v->last_out = 0;
            continue;
        }
        s = voice_sample(v);
        if (i == 1) v1 = (int16_t)s;
        if (i == 3) v3 = (int16_t)s;
        l = (s * vol_reg(v->vol_l)) >> 15;
        r = (s * vol_reg(v->vol_r)) >> 15;
        dl += l;
        dr += r;
        if (g_lspu.reverb_voices & (1u << i)) {
            rl += l;
            rr += r;
        }
    }

    if (g_lspu.cd_mix) {
        int32_t cl = ((int32_t)cd_l * g_lspu.cd_vol_l) >> 15;
        int32_t cr = ((int32_t)cd_r * g_lspu.cd_vol_r) >> 15;
        dl += cl;
        dr += cr;
        if (g_lspu.cd_reverb) {
            rl += cl;
            rr += cr;
        }
    }

    /* capture buffers (CD input as received, voices 1/3 after envelope) */
    cap_write(0x000, cd_l);
    cap_write(0x400, cd_r);
    cap_write(0x800, v1);
    cap_write(0xC00, v3);
    g_lspu.capture_pos = (g_lspu.capture_pos + 1) & (LSPU_CAPTURE_LEN - 1);

    if (g_lspu.reverb_on && g_lspu.reverb_mode > 0 && g_lspu.reverb_mode < SPU_REV_MODE_MAX) {
        float wl, wr;
        reverb_process(rl, rr, &wl, &wr);
        dl += (int32_t)(wl * g_lspu.reverb_depth_l / 32768.0f);
        dr += (int32_t)(wr * g_lspu.reverb_depth_r / 32768.0f);
    }

    dl = clamp16(dl);
    dr = clamp16(dr);
    dl = (dl * vol_reg(g_lspu.mvol_l)) >> 15;
    dr = (dr * vol_reg(g_lspu.mvol_r)) >> 15;
    if (g_lspu.mute) {
        dl = dr = 0;
    }
    out[0] = (int16_t)clamp16(dl);
    out[1] = (int16_t)clamp16(dr);
}

/* CD input */

#define CD_RING 65536   /* frames, power of two */
static int16_t s_cd_ring[CD_RING * 2];
static unsigned s_cd_rd, s_cd_wr;
static LainSpuCdSourceProc s_cd_src;
static void *s_cd_user;

void LainSpu_SetCdSource(LainSpuCdSourceProc fn, void *user)
{
    ensure_locks();
    SDL_LockMutex(s_cd_lock);
    s_cd_src = fn;
    s_cd_user = user;
    SDL_UnlockMutex(s_cd_lock);
}

int LainSpu_PushCd(const short *stereo, int frames)
{
    int n = 0;
    ensure_locks();
    SDL_LockMutex(s_cd_lock);
    while (n < frames && (s_cd_wr - s_cd_rd) < CD_RING) {
        unsigned w = s_cd_wr & (CD_RING - 1);
        s_cd_ring[w * 2] = stereo[n * 2];
        s_cd_ring[w * 2 + 1] = stereo[n * 2 + 1];
        s_cd_wr++;
        n++;
    }
    SDL_UnlockMutex(s_cd_lock);
    return n;
}

int LainSpu_CdQueued(void)
{
    int n;
    ensure_locks();
    SDL_LockMutex(s_cd_lock);
    n = (int)(s_cd_wr - s_cd_rd);
    SDL_UnlockMutex(s_cd_lock);
    return n;
}

void LainSpu_ClearCd(void)
{
    ensure_locks();
    SDL_LockMutex(s_cd_lock);
    s_cd_rd = s_cd_wr = 0;
    SDL_UnlockMutex(s_cd_lock);
}

static void fetch_cd(int16_t *buf, int frames)
{
    LainSpuCdSourceProc src;
    void *user;
    int got = 0;

    SDL_LockMutex(s_cd_lock);
    src = s_cd_src;
    user = s_cd_user;
    if (!src) {
        while (got < frames && s_cd_rd != s_cd_wr) {
            unsigned r = s_cd_rd & (CD_RING - 1);
            buf[got * 2] = s_cd_ring[r * 2];
            buf[got * 2 + 1] = s_cd_ring[r * 2 + 1];
            s_cd_rd++;
            got++;
        }
    }
    SDL_UnlockMutex(s_cd_lock);

    if (src) {
        got = src(buf, frames, user);
        if (got < 0) got = 0;
        if (got > frames) got = frames;
    }
    if (got < frames)
        memset(buf + got * 2, 0, (size_t)(frames - got) * 4);
}

/* Render */

#define CHUNK 32

void LainSpu_Render(short *stereo, int frames)
{
    int16_t cd[CHUNK * 2];

    ensure_locks();
    while (frames > 0) {
        int n = frames < CHUNK ? frames : CHUNK;
        int i;

        fetch_cd(cd, n);

        LSpu_Lock();
        for (i = 0; i < n; i++) {
            if (s_tick_hz > 0 && s_tick_fn) {
                s_tick_acc += s_tick_hz;
                while (s_tick_acc >= LSPU_RATE) {
                    s_tick_acc -= LSPU_RATE;
                    s_tick_fn();
                }
            }
            mix_frame(cd[i * 2], cd[i * 2 + 1], &stereo[i * 2]);
        }
        LSpu_Unlock();

        if (s_gain != 1.0f) {
            for (i = 0; i < n * 2; i++) {
                float x = stereo[i] * s_gain;
                stereo[i] = (short)(x > 32767.f ? 32767 : (x < -32768.f ? -32768 : x));
            }
        }

        LSpu_DispatchCallbacks();
        stereo += n * 2;
        frames -= n;
    }
}

void LainSpu_SetOutputGain(float gain)
{
    s_gain = gain < 0 ? 0 : gain;
}

void LainSpu_ReadRam(unsigned int addr, void *dst, unsigned int size)
{
    unsigned int i;
    uint8_t *d = (uint8_t *)dst;
    LSpu_Lock();
    for (i = 0; i < size; i++)
        d[i] = g_lspu.ram[(addr + i) & (LSPU_RAM_SIZE - 1)];
    LSpu_Unlock();
}

/* Reset */

void LSpu_Reset(void)
{
    static const uint8_t silent_loop[16] = { 0x00, 0x07 };  /* loop start+end+repeat */
    int i;

    LSpu_Lock();
    memset(g_lspu.ram, 0, sizeof(g_lspu.ram));
    memset(g_lspu.v, 0, sizeof(g_lspu.v));
    for (i = 0; i < LSPU_NUM_VOICES; i++) {
        g_lspu.v[i].start = 0x1000;
        g_lspu.v[i].repeat = 0x1000;
        g_lspu.v[i].env_phase = LSPU_ENV_OFF;
    }
    memcpy(&g_lspu.ram[0x1000], silent_loop, sizeof(silent_loop));
    g_lspu.mvol_l = g_lspu.mvol_r = 0;
    g_lspu.cd_vol_l = g_lspu.cd_vol_r = 0;
    g_lspu.cd_mix = 0;
    g_lspu.cd_reverb = 0;
    g_lspu.mute = 0;
    g_lspu.reverb_voices = 0;
    g_lspu.reverb_on = 0;
    g_lspu.reverb_mode = 0;
    g_lspu.reverb_depth_l = g_lspu.reverb_depth_r = 0;
    g_lspu.reverb_delay = g_lspu.reverb_feedback = 0;
    g_lspu.reverb_reserved = 0;
    g_lspu.transfer_addr = LSPU_MALLOC_BASE;
    g_lspu.irq_enabled = 0;
    g_lspu.irq_fired = 0;
    g_lspu.irq_addr = 0;
    g_lspu.irq_pending = 0;
    g_lspu.transfer_pending = 0;
    g_lspu.capture_pos = 0;
    LSpu_ReverbClear();
    LSpu_ReverbConfigure();
    LSpu_Unlock();
}

/* Output */

void LainSpu_SetOutputEnabled(int on)
{
    s_output_enabled = on;
}

static void SDLCALL audio_cb(void *user, Uint8 *stream, int len)
{
    (void)user;
    LainSpu_Render((short *)stream, len / 4);
}

void LSpu_OpenOutput(void)
{
    SDL_AudioSpec want, have;
    const char *env = SDL_getenv("LAIN_SPU_NOAUDIO");

    if (s_dev || !s_output_enabled || (env && *env && *env != '0'))
        return;

    if (!SDL_WasInit(SDL_INIT_AUDIO) && SDL_InitSubSystem(SDL_INIT_AUDIO) != 0) {
        fprintf(stderr, "lain_snd: SDL audio init failed: %s\n", SDL_GetError());
        return;
    }
    SDL_zero(want);
    want.freq = LSPU_RATE;
    want.format = AUDIO_S16SYS;
    want.channels = 2;
    want.samples = 512;
    want.callback = audio_cb;
    s_dev = SDL_OpenAudioDevice(NULL, 0, &want, &have, 0);
    if (!s_dev) {
        fprintf(stderr, "lain_snd: no audio device: %s\n", SDL_GetError());
        return;
    }
    SDL_PauseAudioDevice(s_dev, 0);
}

void LSpu_CloseOutput(void)
{
    if (s_dev) {
        SDL_CloseAudioDevice(s_dev);
        s_dev = 0;
    }
}
