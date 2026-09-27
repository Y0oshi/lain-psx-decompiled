/* lain: libsnd sequencer: Sony SEQ ("pQES") playback.
 *
 * SEQ layout: "pQES", u32 version (BE), u16 resolution (ticks per quarter,
 * BE), u24 tempo (us per quarter, BE), u16 rhythm, then one MIDI-style event
 * stream (delta times, running status). Loops are marked with NRPN
 * controller 99: value 20 = loop start (followed by data entry = count,
 * 0/127 = infinite), value 30 = loop end. The tick (SsSetTickMode rate) is
 * run from the mixer thread via LSpu_SetTickHook, or by SsSeqCalledTbyT.
 */
#include "snd_internal.h"

#include <string.h>

static uint32_t read_varlen(const uint8_t **pp)
{
    uint32_t v = 0;
    int n = 0;
    uint8_t b;
    do {
        b = *(*pp)++;
        v = (v << 7) | (b & 0x7F);
    } while ((b & 0x80) && ++n < 4);
    return v;
}

static SndSeq *get_seq(short acc)
{
    if (acc < 0 || acc >= SS_MAX_SEQ || !g_snd.seq[acc].used)
        return NULL;
    return &g_snd.seq[acc];
}

static void reset_channels(SndSeq *s)
{
    int i;
    for (i = 0; i < 16; i++) {
        SndChan *c = &s->ch[i];
        c->prog = (uint8_t)i;
        c->vol = 127;
        c->pan = 64;
        c->expr = 127;
        c->sustain = 0;
        c->nrpn_msb = c->nrpn_lsb = 127;
        c->rpn_msb = c->rpn_lsb = 127;
        c->bend = 8192;
    }
}

static void seq_rewind(SndSeq *s)
{
    s->pos = s->start;
    s->running = 0;
    s->tempo = s->tempo0;
    s->wait = read_varlen(&s->pos);
    s->eos = 0;
    s->loop_pos = NULL;
    s->in_loop = 0;
    s->loop_count = -1;
    s->loop_count_pending = 0;
    reset_channels(s);
}

static void update_owner_volume(int idx, int chan)
{
    int i;
    for (i = 0; i < LSPU_NUM_VOICES; i++) {
        SndVoice *sv = &g_snd.voice[i];
        if (sv->owner == idx && (chan < 0 || sv->chan == chan) && LSpu_VoiceBusy(i))
            Snd_UpdateVoiceVolume(i);
    }
}

/* Events */

static void note_on(SndSeq *s, int idx, int ch, int note, int vel)
{
    const SndVab *vab;
    const ProgAtr *p;
    int prog = s->ch[ch].prog;
    int t;

    if (s->vab < 0 || s->vab >= SS_MAX_VAB || !g_snd.vab[s->vab].used)
        return;
    vab = &g_snd.vab[s->vab];
    p = Snd_Prog(vab, prog);
    if (!p || p->tones == 0 || vab->prog_block[prog] < 0)
        return;

    for (t = 0; t < p->tones && t < 16; t++) {
        const VagAtr *tone = Snd_Tone(vab, prog, t);
        int v;
        if (!tone || tone->vag <= 0 || tone->vag > vab->vs)
            continue;
        if (note < tone->min || note > tone->max)
            continue;
        v = Snd_AllocVoice(tone->prior);
        if (v < 0)
            continue;
        Snd_StartVoice(v, idx, ch, s->vab, prog, t, note, 0, vel);
    }
}

static void note_off(SndSeq *s, int idx, int ch, int note)
{
    int i;
    for (i = 0; i < LSPU_NUM_VOICES; i++) {
        SndVoice *sv = &g_snd.voice[i];
        if (sv->owner != idx || sv->chan != ch || sv->note != note || !sv->keyed)
            continue;
        if (s->ch[ch].sustain)
            sv->held = 1;
        else
            Snd_KeyOffVoice(i);
    }
}

static void all_notes_off(int idx, int ch)
{
    int i;
    for (i = 0; i < LSPU_NUM_VOICES; i++) {
        SndVoice *sv = &g_snd.voice[i];
        if (sv->owner == idx && (ch < 0 || sv->chan == ch) && sv->keyed)
            Snd_KeyOffVoice(i);
    }
}

static void control(SndSeq *s, int idx, int ch, int cc, int val)
{
    SndChan *c = &s->ch[ch];
    int i;

    switch (cc) {
    case 7:
        c->vol = (uint8_t)val;
        update_owner_volume(idx, ch);
        break;
    case 10:
        c->pan = (uint8_t)val;
        update_owner_volume(idx, ch);
        break;
    case 11:
        c->expr = (uint8_t)val;
        update_owner_volume(idx, ch);
        break;
    case 64:
        c->sustain = val >= 64;
        if (!c->sustain) {
            for (i = 0; i < LSPU_NUM_VOICES; i++) {
                SndVoice *sv = &g_snd.voice[i];
                if (sv->owner == idx && sv->chan == ch && sv->held)
                    Snd_KeyOffVoice(i);
            }
        }
        break;
    case 6:   /* data entry */
        if (s->loop_count_pending) {
            s->loop_count = (val == 0 || val == 127) ? -1 : val;
            s->loop_count_pending = 0;
        }
        break;
    case 98:
        c->nrpn_lsb = (uint8_t)val;
        break;
    case 99:
        c->nrpn_msb = (uint8_t)val;
        if (val == 20) {
            if (!s->in_loop) {
                s->loop_pos = s->pos;
                s->loop_running = s->running;
                s->loop_count = -1;
                s->loop_count_pending = 1;
                s->in_loop = 1;
            }
        } else if (val == 30) {
            if (s->loop_pos) {
                int again = s->loop_count < 0 || --s->loop_count > 0;
                if (again) {
                    s->pos = s->loop_pos;
                    s->running = s->loop_running;
                } else {
                    s->loop_pos = NULL;
                    s->in_loop = 0;
                }
            }
        }
        break;
    case 100:
        c->rpn_lsb = (uint8_t)val;
        break;
    case 101:
        c->rpn_msb = (uint8_t)val;
        break;
    case 120:
    case 123:
        all_notes_off(idx, ch);
        break;
    case 121:
        c->vol = 127;
        c->expr = 127;
        c->pan = 64;
        c->sustain = 0;
        c->bend = 8192;
        update_owner_volume(idx, ch);
        break;
    default:
        break;
    }
}

static void seq_stop_internal(SndSeq *s, int idx)
{
    all_notes_off(idx, -1);
    s->state = SEQ_STOPPED;
}

static void start_play(SndSeq *s, int l_count)
{
    seq_rewind(s);
    s->l_count = l_count;
    s->state = SEQ_PLAYING;
}

/* Handles end of track; returns 0 if the sequence stopped. */
static int end_of_track(SndSeq *s, int idx)
{
    if (s->l_count == 0 || --s->l_count > 0) {
        s->pos = s->start;
        s->running = 0;
        s->loop_pos = NULL;
        s->in_loop = 0;
        s->loop_count_pending = 0;
        return 1;
    }
    seq_stop_internal(s, idx);
    s->eos = 1;
    if (s->next >= 0 && s->next < SS_MAX_SEQ && g_snd.seq[s->next].used) {
        SndSeq *n = &g_snd.seq[s->next];
        start_play(n, 1);
    }
    return 0;
}

/* Executes one event at s->pos. Returns 0 when playback stopped. */
static int seq_event(SndSeq *s, int idx)
{
    uint8_t st = *s->pos;
    int ch;

    if (st & 0x80) {
        s->pos++;
        if (st < 0xF0)
            s->running = st;
    } else {
        st = s->running;
        if (!st) {
            seq_stop_internal(s, idx);     /* corrupt stream */
            return 0;
        }
    }
    ch = st & 15;

    switch (st & 0xF0) {
    case 0x80: {
        int n = s->pos[0];
        s->pos += 2;
        note_off(s, idx, ch, n);
        break;
    }
    case 0x90: {
        int n = s->pos[0], v = s->pos[1];
        s->pos += 2;
        if (v)
            note_on(s, idx, ch, n, v);
        else
            note_off(s, idx, ch, n);
        break;
    }
    case 0xA0:
        s->pos += 2;
        break;
    case 0xB0: {
        int c = s->pos[0], v = s->pos[1];
        s->pos += 2;
        control(s, idx, ch, c, v);
        break;
    }
    case 0xC0:
        s->ch[ch].prog = s->pos[0] & 0x7F;
        s->pos += 1;
        break;
    case 0xD0:
        s->pos += 1;
        break;
    case 0xE0: {
        int i;
        s->ch[ch].bend = (s->pos[0] & 0x7F) | ((s->pos[1] & 0x7F) << 7);
        s->pos += 2;
        for (i = 0; i < LSPU_NUM_VOICES; i++) {
            SndVoice *sv = &g_snd.voice[i];
            if (sv->owner == idx && sv->chan == ch && LSpu_VoiceBusy(i))
                Snd_UpdateVoicePitch(i);
        }
        break;
    }
    default:
        if (st == 0xFF) {
            uint8_t type = *s->pos++;
            if (type == 0x2F) {
                s->pos++;                  /* length byte (0) */
                return end_of_track(s, idx);
            } else if (type == 0x51) {
                /* SEQ tempo: three bytes, no length byte */
                uint32_t t = ((uint32_t)s->pos[0] << 16) | (s->pos[1] << 8) | s->pos[2];
                s->pos += 3;
                if (t)
                    s->tempo = t;
            } else {
                uint32_t len = read_varlen(&s->pos);
                s->pos += len;
            }
        } else if (st == 0xF0 || st == 0xF7) {
            uint32_t len = read_varlen(&s->pos);
            s->pos += len;
        }
        break;
    }
    return 1;
}

/* Tick */

void Snd_SeqTick(void)
{
    int i;
    for (i = 0; i < SS_MAX_SEQ; i++) {
        SndSeq *s = &g_snd.seq[i];
        if (!s->used)
            continue;

        if (s->fade_ticks > 0) {
            s->voll += s->fade_step;
            s->volr += s->fade_step;
            if (s->voll < 0) s->voll = 0;
            if (s->voll > 127) s->voll = 127;
            if (s->volr < 0) s->volr = 0;
            if (s->volr > 127) s->volr = 127;
            s->fade_ticks--;
            update_owner_volume(i, -1);
        }

        if (s->state == SEQ_PLAYING) {
            int guard = 0;
            s->wait -= (double)s->resolution * 1000000.0 / (double)s->tempo / (double)g_snd.tick_hz;
            while (s->wait <= 0 && s->state == SEQ_PLAYING && guard++ < 4096) {
                if (!seq_event(s, i))
                    break;
                s->wait += read_varlen(&s->pos);
            }
        }
    }
}

/* API */

short SsSeqOpen(unsigned char *addr, short vab_id)
{
    int i;
    SndSeq *s;

    if (!addr || addr[0] != 'p' || addr[1] != 'Q' || addr[2] != 'E' || addr[3] != 'S')
        return -1;

    LSpu_Lock();
    for (i = 0; i < g_snd.seq_max && g_snd.seq[i].used; i++)
        ;
    if (i >= g_snd.seq_max) {
        LSpu_Unlock();
        return -1;
    }
    s = &g_snd.seq[i];
    memset(s, 0, sizeof(*s));
    s->used = 1;
    s->vab = vab_id;
    s->data = addr;
    s->resolution = (addr[8] << 8) | addr[9];
    if (s->resolution <= 0)
        s->resolution = 480;
    s->tempo0 = ((uint32_t)addr[10] << 16) | (addr[11] << 8) | addr[12];
    if (!s->tempo0)
        s->tempo0 = 500000;
    s->start = addr + 15;
    s->voll = s->volr = 127;
    s->next = -1;
    s->state = SEQ_STOPPED;
    seq_rewind(s);
    LSpu_Unlock();
    return (short)i;
}

void SsSeqClose(short acc)
{
    SndSeq *s;
    LSpu_Lock();
    s = get_seq(acc);
    if (s) {
        seq_stop_internal(s, acc);
        s->used = 0;
    }
    LSpu_Unlock();
}

void SsSeqPlay(short acc, char play_mode, short l_count)
{
    SndSeq *s;
    LSpu_Lock();
    s = get_seq(acc);
    if (s) {
        if (s->state == SEQ_STOPPED)
            start_play(s, l_count);
        else
            s->l_count = l_count;
        s->state = play_mode == SSPLAY_PLAY ? SEQ_PLAYING : SEQ_PAUSED;
    }
    LSpu_Unlock();
}

void SsSeqPause(short acc)
{
    SndSeq *s;
    LSpu_Lock();
    s = get_seq(acc);
    if (s && s->state == SEQ_PLAYING) {
        all_notes_off(acc, -1);
        s->state = SEQ_PAUSED;
    }
    LSpu_Unlock();
}

void SsSeqReplay(short acc)
{
    SndSeq *s;
    LSpu_Lock();
    s = get_seq(acc);
    if (s && s->state == SEQ_PAUSED)
        s->state = SEQ_PLAYING;
    LSpu_Unlock();
}

void SsSeqStop(short acc)
{
    SndSeq *s;
    LSpu_Lock();
    s = get_seq(acc);
    if (s) {
        seq_stop_internal(s, acc);
        s->fade_ticks = 0;
    }
    LSpu_Unlock();
}

void SsSeqSetVol(short acc, short voll, short volr)
{
    SndSeq *s;
    LSpu_Lock();
    s = get_seq(acc);
    if (s) {
        s->voll = voll < 0 ? 0 : (voll > 127 ? 127 : voll);
        s->volr = volr < 0 ? 0 : (volr > 127 ? 127 : volr);
        s->fade_ticks = 0;
        update_owner_volume(acc, -1);
    }
    LSpu_Unlock();
}

void SsSeqGetVol(short acc, short *voll, short *volr)
{
    SndSeq *s = get_seq(acc);
    *voll = s ? (short)s->voll : 0;
    *volr = s ? (short)s->volr : 0;
}

static void set_fade(short acc, int delta, int v_time)
{
    SndSeq *s;
    LSpu_Lock();
    s = get_seq(acc);
    if (s) {
        if (v_time <= 0) {
            s->voll += delta;
            s->volr += delta;
            if (s->voll < 0) s->voll = 0;
            if (s->voll > 127) s->voll = 127;
            if (s->volr < 0) s->volr = 0;
            if (s->volr > 127) s->volr = 127;
            s->fade_ticks = 0;
            update_owner_volume(acc, -1);
        } else {
            s->fade_step = (float)delta / (float)v_time;
            s->fade_ticks = v_time;
        }
    }
    LSpu_Unlock();
}

void SsSeqSetCrescendo(short acc, short vol, int v_time)
{
    set_fade(acc, vol, v_time);
}

void SsSeqSetDecrescendo(short acc, short vol, int v_time)
{
    set_fade(acc, -vol, v_time);
}

void SsSeqSetNext(short acc1, short acc2)
{
    SndSeq *s;
    LSpu_Lock();
    s = get_seq(acc1);
    if (s)
        s->next = acc2;
    LSpu_Unlock();
}

short SsIsEos(short acc, short seq_num)
{
    SndSeq *s;
    (void)seq_num;
    s = get_seq(acc);
    return s && s->state == SEQ_PLAYING ? 1 : 0;
}
