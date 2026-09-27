/* lain: software SPU (replaces PsyCross's per-voice OpenAL backend).
 *
 * Private interface shared by the libspu API (spu_api.c), the mixer
 * (spu_core.c) and libsnd (snd_*.c). Everything here is called with the SPU
 * lock held unless noted. Written from the public psx-spx SPU documentation.
 */
#ifndef LAIN_SPU_INTERNAL_H
#define LAIN_SPU_INTERNAL_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define LSPU_RAM_SIZE     0x80000   /* 512 KiB of sound RAM */
#define LSPU_NUM_VOICES   24
#define LSPU_RATE         44100
#define LSPU_CAPTURE_LEN  0x200     /* samples per capture ring (cd L/R, voice 1/3) */
#define LSPU_MALLOC_BASE  0x1010    /* after capture buffers (0x0000-0x0FFF) + silent block */

enum { LSPU_ENV_OFF, LSPU_ENV_ATTACK, LSPU_ENV_DECAY, LSPU_ENV_SUSTAIN, LSPU_ENV_RELEASE };

typedef struct {
    /* registers */
    uint16_t vol_l, vol_r;      /* raw volume registers (bit15=0: fixed, 15-bit signed /2) */
    uint16_t pitch;             /* 0x1000 = 44.1 kHz */
    uint32_t start;             /* byte address */
    uint32_t repeat;            /* byte address */
    uint16_t adsr1, adsr2;
    /* state */
    uint32_t cur;               /* address of the block being played */
    uint32_t counter;           /* 12-bit fraction of the sample position */
    int      idx;               /* sample index in the decoded block (0..27) */
    int16_t  block[28];
    int16_t  hist1, hist2;      /* ADPCM filter history */
    int16_t  prev, curs;        /* interpolation pair */
    int      env_phase;
    int32_t  env_level;         /* 0..0x7FFF */
    uint32_t env_wait;
    int      endx;
    int      ignore_loop_flag;  /* repeat address written after key-on */
    int16_t  last_out;          /* post-envelope output (for SpuGetVoiceEnvelope/capture) */
} LSpuVoice;

typedef struct {
    uint8_t  ram[LSPU_RAM_SIZE];
    LSpuVoice v[LSPU_NUM_VOICES];

    uint16_t mvol_l, mvol_r;    /* raw main volume registers */
    int16_t  cd_vol_l, cd_vol_r;
    int      cd_mix, cd_reverb;
    int      mute;

    uint32_t reverb_voices;     /* voice bitmask */
    int      reverb_on;
    int      reverb_mode;
    int16_t  reverb_depth_l, reverb_depth_r;
    int      reverb_delay, reverb_feedback;
    int      reverb_reserved;

    uint32_t transfer_addr;

    int      irq_enabled;
    int      irq_fired;         /* latched until SpuSetIRQ(OFF/RESET) */
    uint32_t irq_addr;          /* byte address */
    int      irq_pending;       /* callback to dispatch outside the lock */

    int      capture_pos;       /* 0..0x1FF */

    int      transfer_pending;  /* transfer-complete callback to dispatch */
} LSpuState;

extern LSpuState g_lspu;

void LSpu_Lock(void);
void LSpu_Unlock(void);

/* core */
void LSpu_Reset(void);
void LSpu_KeyOn(uint32_t mask);
void LSpu_KeyOff(uint32_t mask);
void LSpu_RamWrite(uint32_t addr, const void *src, uint32_t size);   /* checks IRQ */
int  LSpu_VoiceBusy(int v);           /* envelope not finished */
void LSpu_ReverbConfigure(void);      /* after mode/delay/feedback change */
void LSpu_ReverbClear(void);
uint32_t LSpu_ReverbWorkSize(int mode);

/* Dispatch pending IRQ / transfer callbacks (call WITHOUT the lock). */
void LSpu_DispatchCallbacks(void);

/* Periodic hook run inside the mixer (with the lock held), `hz` times per
 * second of rendered audio. Used by libsnd's sequencer tick. hz<=0 disables. */
void LSpu_SetTickHook(void (*fn)(void), int hz);

/* Opens the host audio device on first SpuInit unless disabled. */
void LSpu_OpenOutput(void);
void LSpu_CloseOutput(void);

/* malloc */
void LSpu_MallocInit(int num);

#ifdef __cplusplus
}
#endif

#endif
