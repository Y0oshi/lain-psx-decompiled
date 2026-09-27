/* lain: libsnd voice manager and SsUt* utility functions. */
#include "snd_internal.h"
#include "psx/libspu.h"

#include <math.h>
#include <string.h>

/* Pitch register for `note` played on a tone rooted at center/shift.
 * fine and shift are in 1/128 semitone; 0x1000 = the sample's own rate. */
uint16_t Snd_Pitch(int note, int fine, int center, int shift)
{
    double semis = (note - center) + (fine + shift) / 128.0;
    double p = 4096.0 * pow(2.0, semis / 12.0);
    if (p > 0x3FFF) p = 0x3FFF;
    if (p < 0) p = 0;
    return (uint16_t)(p + 0.5);
}

short SsPitchFromNote(short note, short fine, unsigned char center, unsigned char shift)
{
    return (short)Snd_Pitch(note, fine, center, shift);
}

int Snd_AllocVoice(int prior)
{
    int i, best = -1;
    uint32_t best_age = 0;
    int best_prior = 0x7FFFFFFF;

    /* 1: a silent voice */
    for (i = 0; i < g_snd.reserved; i++) {
        if (!LSpu_VoiceBusy(i))
            return i;
    }
    /* 2: a released voice, lowest priority then oldest */
    for (i = 0; i < g_snd.reserved; i++) {
        SndVoice *sv = &g_snd.voice[i];
        if (sv->keyed)
            continue;
        if (best < 0 || sv->prior < best_prior || (sv->prior == best_prior && sv->age < best_age)) {
            best = i;
            best_prior = sv->prior;
            best_age = sv->age;
        }
    }
    if (best >= 0)
        return best;
    /* 3: steal a sounding voice of lower or equal priority, oldest first */
    for (i = 0; i < g_snd.reserved; i++) {
        SndVoice *sv = &g_snd.voice[i];
        if (sv->prior > prior)
            continue;
        if (best < 0 || sv->prior < best_prior || (sv->prior == best_prior && sv->age < best_age)) {
            best = i;
            best_prior = sv->prior;
            best_age = sv->age;
        }
    }
    return best;
}

static const SndVab *voice_vab(const SndVoice *sv)
{
    if (sv->vab < 0 || sv->vab >= SS_MAX_VAB || !g_snd.vab[sv->vab].used)
        return NULL;
    return &g_snd.vab[sv->vab];
}

void Snd_UpdateVoiceVolume(int v)
{
    SndVoice *sv = &g_snd.voice[v];
    const SndVab *vab = voice_vab(sv);
    const ProgAtr *p;
    const VagAtr *t;
    float vol, l, r, lv = 1.0f, rv = 1.0f;
    int pan, pan_ex;

    if (!vab)
        return;
    p = Snd_Prog(vab, sv->prog);
    t = Snd_Tone(vab, sv->prog, sv->tone);
    if (!p || !t)
        return;

    if (sv->owner == SND_OWNER_UT || sv->owner < 0 || sv->owner >= SS_MAX_SEQ) {
        vol = (float)sv->ut_vol;
        pan_ex = sv->ut_pan;
    } else {
        const SndSeq *s = &g_snd.seq[sv->owner];
        const SndChan *ch = &s->ch[sv->chan & 15];
        vol = sv->vel * (ch->vol / 127.0f) * (ch->expr / 127.0f);
        pan_ex = ch->pan;
        lv = s->voll / 127.0f;
        rv = s->volr / 127.0f;
    }

    vol = (vol / 127.0f) * (t->vol / 127.0f) * (p->mvol / 127.0f) * (vab->mvol / 127.0f);
    pan = t->pan + (p->mpan - 64) + (pan_ex - 64) + (vab->pan - 64);
    if (pan < 0) pan = 0;
    if (pan > 127) pan = 127;
    l = r = vol;
    if (pan < 64)
        r *= pan / 64.0f;
    else
        l *= (127 - pan) / 63.0f;
    l *= lv;
    r *= rv;
    if (l > 1.0f) l = 1.0f;
    if (r > 1.0f) r = 1.0f;

    g_lspu.v[v].vol_l = (uint16_t)(l * 0x3FFF);
    g_lspu.v[v].vol_r = (uint16_t)(r * 0x3FFF);
}

void Snd_UpdateVoicePitch(int v)
{
    SndVoice *sv = &g_snd.voice[v];
    const SndVab *vab = voice_vab(sv);
    const VagAtr *t;
    int fine = sv->fine;

    if (!vab)
        return;
    t = Snd_Tone(vab, sv->prog, sv->tone);
    if (!t)
        return;
    if (sv->owner >= 0 && sv->owner < SS_MAX_SEQ) {
        int b = g_snd.seq[sv->owner].ch[sv->chan & 15].bend - 8192;
        int range = b >= 0 ? t->pbmax : t->pbmin;
        fine += b * range * 128 / 8192;
    }
    g_lspu.v[v].pitch = Snd_Pitch(sv->note, fine, t->center, t->shift);
}

void Snd_StartVoice(int v, int owner, int chan, int vabid, int prog, int tone,
                    int note, int fine, int vel)
{
    SndVoice *sv = &g_snd.voice[v];
    const SndVab *vab = &g_snd.vab[vabid];
    const VagAtr *t = Snd_Tone(vab, prog, tone);
    LSpuVoice *hw = &g_lspu.v[v];

    if (!t)
        return;

    sv->owner = owner;
    sv->chan = chan;
    sv->note = note;
    sv->vab = vabid;
    sv->prog = prog;
    sv->tone = tone;
    sv->vel = vel;
    sv->fine = fine;
    sv->prior = t->prior;
    sv->age = ++g_snd.age;
    sv->keyed = 1;
    sv->held = 0;

    hw->start = t->vag > 0 && t->vag <= vab->vs ? vab->vag_addr[t->vag] : 0x1000;
    hw->adsr1 = t->adsr1;
    hw->adsr2 = t->adsr2;
    if (t->mode & 4)
        g_lspu.reverb_voices |= 1u << v;
    else
        g_lspu.reverb_voices &= ~(1u << v);

    Snd_UpdateVoiceVolume(v);
    Snd_UpdateVoicePitch(v);
    LSpu_KeyOn(1u << v);
}

void Snd_KeyOffVoice(int v)
{
    if (v < 0 || v >= LSPU_NUM_VOICES)
        return;
    LSpu_KeyOff(1u << v);
    g_snd.voice[v].keyed = 0;
    g_snd.voice[v].held = 0;
}

void Snd_KillOwner(int owner)
{
    int i;
    for (i = 0; i < LSPU_NUM_VOICES; i++) {
        if (g_snd.voice[i].owner == owner && g_snd.voice[i].keyed)
            Snd_KeyOffVoice(i);
    }
}

/* SsUt */

static void ut_vol_pan(short voll, short volr, int *vol, int *pan)
{
    if (voll == volr) {
        *vol = voll;
        *pan = 64;
    } else if (volr < voll) {
        *vol = voll;
        *pan = voll ? volr * 64 / voll : 64;
    } else {
        *vol = volr;
        *pan = 127 - (volr ? voll * 64 / volr : 64);
    }
    if (*vol > 127) *vol = 127;
    if (*vol < 0) *vol = 0;
}

static int ut_valid(short vab_id, short prog, short tone)
{
    const VagAtr *t;
    if (vab_id < 0 || vab_id >= SS_MAX_VAB || !g_snd.vab[vab_id].used)
        return 0;
    t = Snd_Tone(&g_snd.vab[vab_id], prog, tone);
    return t && t->vag > 0 && t->vag <= g_snd.vab[vab_id].vs;
}

static short ut_start(int v, short vab_id, short prog, short tone, short note, short fine,
                      short voll, short volr)
{
    SndVoice *sv = &g_snd.voice[v];
    int vol, pan;
    ut_vol_pan(voll, volr, &vol, &pan);
    sv->ut_vol = vol;
    sv->ut_pan = pan;
    Snd_StartVoice(v, SND_OWNER_UT, 0, vab_id, prog, tone, note, fine, 127);
    return (short)v;
}

short SsUtKeyOn(short vab_id, short prog, short tone, short note, short fine, short voll, short volr)
{
    int v;
    short r = -1;
    LSpu_Lock();
    if (ut_valid(vab_id, prog, tone)) {
        const VagAtr *t = Snd_Tone(&g_snd.vab[vab_id], prog, tone);
        v = Snd_AllocVoice(t->prior);
        if (v >= 0)
            r = ut_start(v, vab_id, prog, tone, note, fine, voll, volr);
    }
    LSpu_Unlock();
    return r;
}

short SsUtKeyOnV(short voice, short vab_id, short prog, short tone, short note, short fine,
                 short voll, short volr)
{
    short r = -1;
    if (voice < 0 || voice >= LSPU_NUM_VOICES)
        return -1;
    LSpu_Lock();
    if (ut_valid(vab_id, prog, tone))
        r = ut_start(voice, vab_id, prog, tone, note, fine, voll, volr);
    LSpu_Unlock();
    return r;
}

/* Like the library, keys the voice off whether or not the other arguments
 * match what it is playing. */
short SsUtKeyOff(short voice, short vab_id, short prog, short tone, short note)
{
    (void)vab_id; (void)prog; (void)tone; (void)note;
    if (voice < 0 || voice >= LSPU_NUM_VOICES)
        return -1;
    LSpu_Lock();
    Snd_KeyOffVoice(voice);
    LSpu_Unlock();
    return 0;
}

short SsUtKeyOffV(short voice)
{
    if (voice < 0 || voice >= LSPU_NUM_VOICES)
        return -1;
    LSpu_Lock();
    Snd_KeyOffVoice(voice);
    LSpu_Unlock();
    return 0;
}

void SsUtAllKeyOff(short mode)
{
    int i;
    (void)mode;
    LSpu_Lock();
    for (i = 0; i < g_snd.reserved; i++)
        Snd_KeyOffVoice(i);
    LSpu_Unlock();
}

short SsUtSetVVol(short voice, short voll, short volr)
{
    if (voice < 0 || voice >= LSPU_NUM_VOICES)
        return -1;
    LSpu_Lock();
    g_lspu.v[voice].vol_l = (uint16_t)((voll & 0x7F) * 0x3FFF / 127);
    g_lspu.v[voice].vol_r = (uint16_t)((volr & 0x7F) * 0x3FFF / 127);
    LSpu_Unlock();
    return 0;
}

short SsUtGetVVol(short voice, short *voll, short *volr)
{
    if (voice < 0 || voice >= LSPU_NUM_VOICES)
        return -1;
    *voll = (short)((g_lspu.v[voice].vol_l & 0x3FFF) * 127 / 0x3FFF);
    *volr = (short)((g_lspu.v[voice].vol_r & 0x3FFF) * 127 / 0x3FFF);
    return 0;
}

short SsUtChangePitch(short voice, short vab_id, short prog, short old_note, short old_fine,
                      short new_note, short new_fine)
{
    (void)vab_id; (void)prog; (void)old_note; (void)old_fine;
    if (voice < 0 || voice >= LSPU_NUM_VOICES)
        return -1;
    LSpu_Lock();
    g_snd.voice[voice].note = new_note;
    g_snd.voice[voice].fine = new_fine;
    Snd_UpdateVoicePitch(voice);
    LSpu_Unlock();
    return 0;
}
