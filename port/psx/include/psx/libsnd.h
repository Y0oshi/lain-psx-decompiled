/* lain: libsnd (PlayStation "basic sound library") for the native port.
 *
 * Implemented in src/lain_snd/ on top of the software SPU. Covers the subset
 * Serial Experiments Lain uses plus close relatives: VAB sound banks
 * (VH header + VB body), SEQ sequences, the sequencer tick, utility
 * key-on/off and reverb. Written from public documentation of the formats
 * and API behaviour; not derived from Sony's headers.
 *
 * Addresses of game data are taken as byte pointers (the game passes u8*),
 * and 32-bit PsyQ "long" parameters are plain int, so the API is LP64-safe.
 */
#ifndef LAIN_LIBSND_H
#define LAIN_LIBSND_H

#if defined(_LANGUAGE_C_PLUS_PLUS) || defined(__cplusplus) || defined(c_plusplus)
extern "C" {
#endif

/* SsSetTickMode */
#define SS_NOTICK        0x1000   /* flag: caller drives the tick with SsSeqCalledTbyT */
#define SS_NOTICK0       0
#define SS_TICK60        1
#define SS_TICK240       2
#define SS_TICK120       3
#define SS_TICK50        4
#define SS_TICKVSYNC     5
#define SS_TICKMODE_MAX  6

/* SsSeqPlay */
#define SSPLAY_PAUSE     0
#define SSPLAY_PLAY      1
#define SSPLAY_INFINITY  0

/* SsVabTransCompleted */
#define SS_IMEDIATE        0
#define SS_IMMEDIATE       0
#define SS_WAIT_COMPLETED  1

/* SsUtSetReverbType */
#define SS_REV_TYPE_OFF      0
#define SS_REV_TYPE_ROOM     1
#define SS_REV_TYPE_STUDIO_A 2
#define SS_REV_TYPE_STUDIO_B 3
#define SS_REV_TYPE_STUDIO_C 4
#define SS_REV_TYPE_HALL     5
#define SS_REV_TYPE_SPACE    6
#define SS_REV_TYPE_ECHO     7
#define SS_REV_TYPE_DELAY    8
#define SS_REV_TYPE_PIPE     9

/* SsSetSerialAttr / SsSetSerialVol */
#define SS_SERIAL_A  0     /* CD input */
#define SS_SERIAL_B  1     /* external input */
#define SS_MIX       0
#define SS_REV       1
#define SS_SOFF      0
#define SS_SON       1

/* Size of one per-sequence work entry for SsSetTableSize (kept for source
 * compatibility; this implementation keeps its own state). */
#define SS_SEQ_TABSIZ 176

#define SS_MAX_VAB 16
#define SS_MAX_SEQ 32

/* ---- file formats (little-endian on disc) ---- */

/* VAB header ("VH"): 32 bytes, then 128 program records, then 16 tone
 * records per program, then a 256-entry table of sample sizes / 8. */
typedef struct {
    int            form;      /* 'VABp' */
    int            ver;
    int            id;
    unsigned int   fsize;     /* header + body */
    unsigned short reserved0;
    unsigned short ps;        /* number of programs */
    unsigned short ts;        /* number of tones */
    unsigned short vs;        /* number of samples */
    unsigned char  mvol, pan, attr1, attr2;
    unsigned int   reserved1;
} VabHdr;

typedef struct {
    unsigned char  tones, mvol, prior, mode, mpan, reserved0;
    short          attr;
    unsigned int   reserved1, reserved2;
} ProgAtr;

typedef struct {
    unsigned char  prior, mode, vol, pan;
    unsigned char  center, shift;       /* root note and fine tune (1/128 semitone) */
    unsigned char  min, max;            /* key range */
    unsigned char  vibW, vibT, porW, porT;
    unsigned char  pbmin, pbmax;        /* pitch bend range (semitones) */
    unsigned char  reserved1, reserved2;
    unsigned short adsr1, adsr2;
    short          prog, vag;           /* owning program, sample number (1..) */
    short          reserved[4];
} VagAtr;

/* ---- API ---- */

void  SsInit(void);
void  SsInitHot(void);
void  SsStart(void);
void  SsStart2(void);
void  SsEnd(void);
void  SsQuit(void);
void  SsSetTableSize(char *table, short s_max, short t_max);
int   SsSetTickMode(int tick_mode);
void  SsSeqCalledTbyT(void);
char  SsSetReservedVoice(char voices);
void  SsSetMVol(short voll, short volr);
void  SsGetMVol(short *voll, short *volr);
void  SsSetSerialAttr(char s_num, char attr, char mode);
void  SsSetSerialVol(char s_num, short voll, short volr);

short SsVabOpenHead(unsigned char *addr, short vab_id);
short SsVabTransBody(unsigned char *addr, short vab_id);
short SsVabTransCompleted(short immediate_flag);
short SsVabOpen(unsigned char *addr, short vab_id);   /* header + body in one buffer */
void  SsVabClose(short vab_id);

short SsSeqOpen(unsigned char *addr, short vab_id);
void  SsSeqClose(short seq_access_num);
void  SsSeqPlay(short seq_access_num, char play_mode, short l_count);
void  SsSeqPause(short seq_access_num);
void  SsSeqReplay(short seq_access_num);
void  SsSeqStop(short seq_access_num);
void  SsSeqSetVol(short seq_access_num, short voll, short volr);
void  SsSeqGetVol(short seq_access_num, short *voll, short *volr);
void  SsSeqSetCrescendo(short seq_access_num, short vol, int v_time);
void  SsSeqSetDecrescendo(short seq_access_num, short vol, int v_time);
void  SsSeqSetNext(short seq_access_num1, short seq_access_num2);
short SsIsEos(short access_num, short seq_num);

short SsUtKeyOn(short vab_id, short prog, short tone, short note, short fine, short voll, short volr);
short SsUtKeyOff(short voice, short vab_id, short prog, short tone, short note);
short SsUtKeyOnV(short voice, short vab_id, short prog, short tone, short note, short fine, short voll, short volr);
short SsUtKeyOffV(short voice);
void  SsUtAllKeyOff(short mode);
short SsUtSetVVol(short voice, short voll, short volr);
short SsUtGetVVol(short voice, short *voll, short *volr);
short SsUtChangePitch(short voice, short vab_id, short prog, short old_note, short old_fine, short new_note, short new_fine);
short SsUtSetReverbType(short type);
short SsUtGetReverbType(void);
void  SsUtSetReverbDepth(short ldepth, short rdepth);
void  SsUtSetReverbDelay(short delay);
void  SsUtSetReverbFeedback(short feedback);
void  SsUtReverbOn(void);    /* the game's func_8006179C */
void  SsUtReverbOff(void);   /* the game's func_8006177C */
short SsPitchFromNote(short note, short fine, unsigned char center, unsigned char shift);

#if defined(_LANGUAGE_C_PLUS_PLUS) || defined(__cplusplus) || defined(c_plusplus)
}
#endif

#endif /* LAIN_LIBSND_H */
