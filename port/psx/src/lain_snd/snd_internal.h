/* lain: libsnd internals (VAB banks, voice manager, sequencer). All state is
 * protected by the SPU lock (LSpu_Lock) because the sequencer tick runs on
 * the audio thread inside the mixer. */
#ifndef LAIN_SND_INTERNAL_H
#define LAIN_SND_INTERNAL_H

#include <stdint.h>
#include "psx/libsnd.h"
#include "spu_internal.h"

#define SND_OWNER_NONE  (-1)
#define SND_OWNER_UT    1000     /* SsUtKeyOn */

typedef struct {
    int       used;
    uint8_t  *hdr;               /* private copy of the VH */
    uint32_t  hdr_size;
    int       ps, ts, vs;
    uint8_t   mvol, pan;
    int       prog_block[128];   /* program -> tone block (-1 = none) */
    uint32_t  vag_addr[256];     /* SPU address of each sample (1..vs) */
    uint32_t  vag_size[256];
    int32_t   spu_base;          /* SpuMalloc result, -1 if none */
    uint32_t  body_size;
    int       body_loaded;
} SndVab;

typedef struct {
    int      owner;              /* seq index, SND_OWNER_UT or NONE */
    int      chan, note, vab, prog, tone;
    int      vel;
    int      fine;
    int      prior;
    uint32_t age;
    int      keyed;              /* key is down (not released) */
    int      held;               /* released while the damper pedal is down */
    int      ut_vol, ut_pan;     /* SsUtKeyOn volume/pan (0..127) */
} SndVoice;

typedef struct {
    uint8_t prog, vol, pan, expr, sustain;
    uint8_t nrpn_msb, nrpn_lsb;
    uint8_t rpn_msb, rpn_lsb;    /* 127/127 = none */
    int     bend;                /* 0..16383 */
} SndChan;

enum { SEQ_STOPPED, SEQ_PLAYING, SEQ_PAUSED };

typedef struct {
    int             used;
    int             vab;
    const uint8_t  *data;
    const uint8_t  *start;       /* first delta time */
    const uint8_t  *pos;
    int             resolution;
    uint32_t        tempo0, tempo;   /* microseconds per quarter note */
    uint8_t         running;
    double          wait;        /* MIDI ticks until the next event */
    int             state;
    int             l_count;     /* plays left (0 = infinite) */
    int             eos;
    int             next;        /* SsSeqSetNext target, -1 = none */
    float           voll, volr;  /* 0..127 */
    float           fade_step;   /* per tick */
    int             fade_ticks;
    SndChan         ch[16];
    /* NRPN loop */
    const uint8_t  *loop_pos;
    uint8_t         loop_running;
    int             loop_count;  /* -1 = infinite */
    int             in_loop;
    int             loop_count_pending;
} SndSeq;

typedef struct {
    int      init;
    int      started;
    int      tick_mode;
    int      tick_hz;
    int      reserved;           /* voices 0..reserved-1 belong to libsnd */
    short    mvol_l, mvol_r;
    int      seq_max;
    uint32_t age;
    int      rev_type;
    SndVab   vab[SS_MAX_VAB];
    SndVoice voice[LSPU_NUM_VOICES];
    SndSeq   seq[SS_MAX_SEQ];
} SndState;

extern SndState g_snd;

/* snd_vab.c */
const ProgAtr *Snd_Prog(const SndVab *vab, int prog);
const VagAtr  *Snd_Tone(const SndVab *vab, int prog, int tone);

/* snd_voice.c */
int  Snd_AllocVoice(int prior);
void Snd_StartVoice(int v, int owner, int chan, int vab, int prog, int tone,
                    int note, int fine, int vel);
void Snd_UpdateVoiceVolume(int v);
void Snd_UpdateVoicePitch(int v);
void Snd_KeyOffVoice(int v);
void Snd_KillOwner(int owner);
uint16_t Snd_Pitch(int note, int fine, int center, int shift);

/* snd_seq.c */
void Snd_SeqTick(void);

#endif
