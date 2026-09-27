/* snd_test: exercises the lain_snd software SPU and libsnd offline.
 *
 * Loads SND.BIN from the user's extracted disc (VAB header at 0, VAB body at
 * the next 2 KiB sector, the two SEQs at later sectors: the layout the
 * game's file table for file 5 describes), sets up sound like the
 * game's snd_init, then renders several scenarios to WAV:
 *   notes.wav       SsUtKeyOn/Off of several programs (sound effects)
 *   seq0.wav        BGM sequence 0, then SsSeqSetDecrescendo fade-out
 *   seq1.wav        BGM sequence 1
 *   spu_voice.wav   libspu direct voice 23 (EncSPU + "SpuRead" upload), like the jingle
 *   cd_capture.wav  CD input mix + capture buffer / SPU IRQ double buffering (level meter)
 * and prints RMS / peak / duration for each.
 */
#include "libspu.h"
#include "libsnd.h"

#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#define RATE 44100

/* ---------------------------------------------------------------- utils */

static uint8_t *load_file(const char *path, long *size)
{
    FILE *f = fopen(path, "rb");
    uint8_t *buf;
    if (!f)
        return NULL;
    fseek(f, 0, SEEK_END);
    *size = ftell(f);
    fseek(f, 0, SEEK_SET);
    buf = (uint8_t *)malloc((size_t)*size);
    if (fread(buf, 1, (size_t)*size, f) != (size_t)*size) {
        fclose(f);
        free(buf);
        return NULL;
    }
    fclose(f);
    return buf;
}

typedef struct {
    int16_t *s;      /* interleaved stereo */
    size_t frames, cap;
} Pcm;

static void render(Pcm *p, double seconds)
{
    size_t n = (size_t)(seconds * RATE);
    if (p->frames + n > p->cap) {
        p->cap = (p->frames + n) * 2;
        p->s = (int16_t *)realloc(p->s, p->cap * 4);
    }
    LainSpu_Render(p->s + p->frames * 2, (int)n);
    p->frames += n;
}

static void put_u32(FILE *f, uint32_t v) { fputc(v & 255, f); fputc((v >> 8) & 255, f); fputc((v >> 16) & 255, f); fputc(v >> 24, f); }
static void put_u16(FILE *f, uint16_t v) { fputc(v & 255, f); fputc(v >> 8, f); }

static void write_wav(const char *name, const Pcm *p)
{
    char path[1024];
    FILE *f;
    uint32_t bytes = (uint32_t)(p->frames * 4);
    snprintf(path, sizeof(path), "%s/%s", SND_TEST_OUT, name);
    f = fopen(path, "wb");
    if (!f) {
        fprintf(stderr, "cannot write %s\n", path);
        return;
    }
    fwrite("RIFF", 1, 4, f); put_u32(f, 36 + bytes); fwrite("WAVE", 1, 4, f);
    fwrite("fmt ", 1, 4, f); put_u32(f, 16); put_u16(f, 1); put_u16(f, 2);
    put_u32(f, RATE); put_u32(f, RATE * 4); put_u16(f, 4); put_u16(f, 16);
    fwrite("data", 1, 4, f); put_u32(f, bytes);
    fwrite(p->s, 4, p->frames, f);
    fclose(f);
}

typedef struct { double rms_db, peak_db, active; double seconds; } Stats;

static Stats stats(const Pcm *p, size_t from, size_t to)
{
    Stats st;
    double sum = 0;
    int peak = 0;
    size_t i, win = RATE / 20, active = 0, wins = 0;
    if (to > p->frames) to = p->frames;
    for (i = from; i < to; i++) {
        int l = p->s[i * 2], r = p->s[i * 2 + 1];
        sum += (double)l * l + (double)r * r;
        if (abs(l) > peak) peak = abs(l);
        if (abs(r) > peak) peak = abs(r);
    }
    for (i = from; i + win <= to; i += win) {
        double w = 0;
        size_t k;
        for (k = i; k < i + win; k++)
            w += (double)p->s[k * 2] * p->s[k * 2] + (double)p->s[k * 2 + 1] * p->s[k * 2 + 1];
        w = sqrt(w / (2.0 * win)) / 32768.0;
        if (w > 0.001) active++;       /* > -60 dBFS */
        wins++;
    }
    st.seconds = (double)(to - from) / RATE;
    st.rms_db = to > from && sum > 0 ? 20 * log10(sqrt(sum / (2.0 * (to - from))) / 32768.0) : -999;
    st.peak_db = peak ? 20 * log10(peak / 32768.0) : -999;
    st.active = wins ? (double)active / wins : 0;
    return st;
}

static void report(const char *name, const Pcm *p, size_t from, size_t to)
{
    Stats s = stats(p, from, to);
    printf("  %-28s %7.2f s  RMS %7.2f dBFS  peak %7.2f dBFS  non-silent %5.1f%%\n",
           name, s.seconds, s.rms_db, s.peak_db, s.active * 100.0);
}

static int failures;
static void check(int cond, const char *what)
{
    printf("  [%s] %s\n", cond ? "ok" : "FAIL", what);
    if (!cond)
        failures++;
}

/* ------------------------------------------------------- CD / IRQ test */

static int16_t s_meter_buf[0x800];      /* SpuDecodedData-sized: 0x1000 bytes */
static unsigned s_irq_addr = 0x100;
static int s_irqs, s_xfers;
static double s_phase;

static int cd_sine(short *out, int frames, void *user)
{
    int i;
    (void)user;
    for (i = 0; i < frames; i++) {
        short v = (short)(16000 * sin(s_phase));
        s_phase += 2 * M_PI * 440.0 / RATE;
        out[i * 2] = v;
        out[i * 2 + 1] = v;
    }
    return frames;
}

/* mirrors spu_irq_read_decoded / spu_decoded_xfer_done */
static void irq_cb(void)
{
    s_irqs++;
    SpuSetIRQ(SPU_OFF);
    SpuReadDecodedData((SpuDecodedData *)s_meter_buf, SPU_CDONLY);
}

static void xfer_cb(void)
{
    s_xfers++;
    s_irq_addr = s_irq_addr == 0 ? 0x200 : 0;
    SpuSetIRQAddr(s_irq_addr);
    SpuSetIRQ(SPU_ON);
}

/* mirrors spu_decoded_peak_level */
static int meter_level(void)
{
    int i, start = s_irq_addr == 0 ? 0 : 0x100, end = start + 0x100, peak = 0;
    for (i = start; i < end - 50; i++) {
        int v = abs(s_meter_buf[i]);
        if (v > peak) peak = v;
    }
    return peak / 4681;
}

/* ----------------------------------------------------------------- main */

int main(int argc, char **argv)
{
    char path[1024];
    const char *disc = argc > 1 ? argv[1] : LAIN_REPO_ROOT "/extract/disc1";
    long size = 0;
    uint8_t *snd;
    uint32_t head_size, body_off;
    uint8_t *seq_ptr[2] = { NULL, NULL };
    long off;
    int nseq = 0;
    char table[SS_SEQ_TABSIZ * 2];
    short vab, seq0, seq1;
    Pcm pcm;
    int i;

    snprintf(path, sizeof(path), "%s/SND.BIN", disc);
    snd = load_file(path, &size);
    if (!snd) {
        fprintf(stderr, "cannot read %s (pass the extracted disc1 folder)\n", path);
        return 2;
    }
    mkdir(SND_TEST_OUT, 0755);

    /* layout: VH at 0, VB at the next sector, SEQs on later sectors */
    {
        const VabHdr *h = (const VabHdr *)snd;
        head_size = 0x20 + 0x800 + h->ps * 0x200 + 0x200;
        body_off = (head_size + 0x7FF) & ~0x7FFu;
        printf("SND.BIN: %ld bytes, VAB %u programs / %u tones / %u samples, head 0x%X, body @0x%X (fsize 0x%X)\n",
               size, h->ps, h->ts, h->vs, head_size, body_off, h->fsize);
    }
    for (off = (long)body_off; off + 4 <= size && nseq < 2; off += 0x800) {
        if (!memcmp(snd + off, "pQES", 4)) {
            printf("SEQ %d @0x%lX: resolution %d, tempo %d us/qn\n", nseq, off,
                   (snd[off + 8] << 8) | snd[off + 9],
                   (snd[off + 10] << 16) | (snd[off + 11] << 8) | snd[off + 12]);
            seq_ptr[nseq++] = snd + off;
        }
    }

    /* ---- the game's sound init (snd_init) ---- */
    LainSpu_SetOutputEnabled(0);
    SsInit();
    SsSetMVol(0, 0);
    SsSetReservedVoice(0x17);
    SsSetTableSize(table, 2, 1);
    SsSetTickMode(SS_TICK240);
    vab = SsVabOpenHead(snd, -1);
    check(vab >= 0, "SsVabOpenHead");
    check(SsVabTransBody(snd + body_off, vab) == vab, "SsVabTransBody");
    while (!SsVabTransCompleted(SS_IMMEDIATE))
        ;
    SsUtSetReverbType(SS_REV_TYPE_ROOM);
    SsUtReverbOn();
    SsUtSetReverbDepth(0x30, 0x30);
    seq0 = seq_ptr[0] ? SsSeqOpen(seq_ptr[0], vab) : -1;
    seq1 = seq_ptr[1] ? SsSeqOpen(seq_ptr[1], vab) : -1;
    check(seq0 >= 0 && seq1 >= 0, "SsSeqOpen x2");
    SsStart();
    SsSetMVol(0x7F, 0x7F);

    printf("\nrendering (44100 Hz stereo s16) to %s\n", SND_TEST_OUT);

    /* ---- 1: sound effects ---- */
    memset(&pcm, 0, sizeof(pcm));
    {
        static const short progs[] = { 0x18, 0, 2, 5, 12, 16, 25, 28, 30 };
        for (i = 0; i < (int)(sizeof(progs) / sizeof(progs[0])); i++) {
            short v = SsUtKeyOn(vab, progs[i], 0, 0x3C, 0, 0x7F, 0x7F);
            if (v < 0)
                printf("  SsUtKeyOn prog %d failed\n", progs[i]);
            render(&pcm, 0.7);
            SsUtKeyOffV(v);
            render(&pcm, 0.5);
        }
        /* a little scale on program 0 with panning */
        for (i = 0; i < 8; i++) {
            static const int scale[8] = { 48, 50, 52, 53, 55, 57, 59, 60 };
            short v = SsUtKeyOn(vab, 0, 0, scale[i], 0, 0x7F, (short)(i * 16));
            render(&pcm, 0.25);
            SsUtKeyOff(v, vab, 0, 0, scale[i]);
        }
        render(&pcm, 1.0);
    }
    write_wav("notes.wav", &pcm);
    report("notes.wav", &pcm, 0, pcm.frames);
    check(stats(&pcm, 0, pcm.frames).rms_db > -50, "sound effects are audible");
    free(pcm.s);

    /* ---- 2: BGM sequence 0 + fade out (bgm_play / bgm_fade_out) ---- */
    memset(&pcm, 0, sizeof(pcm));
    SsSeqSetVol(seq0, 0x7F, 0x7F);
    SsSeqPlay(seq0, SSPLAY_PLAY, 0);
    render(&pcm, 70.0);   /* the song loops at ~62.9 s */
    {
        size_t before = pcm.frames;
        SsSeqSetDecrescendo(seq0, 0x7F, 0xF0);
        render(&pcm, 1.5);
        SsSeqStop(seq0);
        render(&pcm, 1.0);
        write_wav("seq0.wav", &pcm);
        report("seq0.wav (play)", &pcm, 0, before);
        report("seq0.wav (last 0.5 s of fade)", &pcm, before + RATE / 2, before + RATE);
        report("seq0.wav (after stop)", &pcm, pcm.frames - RATE / 2, pcm.frames);
        check(stats(&pcm, 0, before).rms_db > -40, "sequence 0 plays");
        check(stats(&pcm, 0, before).active > 0.8, "sequence 0 keeps playing");
        report("seq0.wav (after loop point)", &pcm, (size_t)(63.5 * RATE), before);
        check(stats(&pcm, (size_t)(63.5 * RATE), before).active > 0.8, "sequence 0 loops (NRPN loop end)");
        check(stats(&pcm, before + RATE / 2, before + RATE).rms_db <
              stats(&pcm, before - RATE * 5, before).rms_db - 12, "decrescendo fades out");
    }
    free(pcm.s);

    /* ---- 3: BGM sequence 1 ---- */
    memset(&pcm, 0, sizeof(pcm));
    SsSeqSetVol(seq1, 0x7F, 0x7F);
    SsSeqPlay(seq1, SSPLAY_PLAY, 0);
    render(&pcm, 50.0);   /* loops at ~36.9 s */
    SsSeqStop(seq1);
    render(&pcm, 1.0);
    write_wav("seq1.wav", &pcm);
    report("seq1.wav", &pcm, 0, pcm.frames);
    check(stats(&pcm, 0, (size_t)(50.0 * RATE)).rms_db > -40, "sequence 1 plays");
    report("seq1.wav (after loop point)", &pcm, (size_t)(37.5 * RATE), (size_t)(50.0 * RATE));
    check(stats(&pcm, (size_t)(37.5 * RATE), (size_t)(50.0 * RATE)).active > 0.8, "sequence 1 loops");
    free(pcm.s);

    /* ---- 4: libspu voice 23, like the jingle in audio_node_play ---- */
    memset(&pcm, 0, sizeof(pcm));
    {
        int n = 11025, addr, enc_size;
        int16_t *raw = (int16_t *)malloc(n * 2);
        int16_t *enc = (int16_t *)malloc(n * 2);
        int16_t work[0x54];
        EncSPUEnv env;
        SpuVoiceAttr attr;
        uint8_t *check_ram;
        double err = 0, sig = 0;

        for (i = 0; i < n; i++) {
            double t = (double)i / n;
            raw[i] = (int16_t)(9000 * (sin(2 * M_PI * 440 * t) + sin(2 * M_PI * 554.4 * t) +
                                       sin(2 * M_PI * 659.3 * t)) * (1.0 - t));
        }
        memset(&env, 0, sizeof(env));
        env.src = raw;
        env.dest = enc;
        env.work = work;
        env.size = n * 2;
        env.loop = SPU_ENCSPU_NO_LOOP;
        enc_size = EncSPU(&env);
        addr = SpuMalloc(enc_size);
        check(enc_size > 0 && addr >= 0x1010, "EncSPU + SpuMalloc");
        SpuSetTransferStartAddr(addr);
        SpuRead((unsigned char *)enc, enc_size);   /* Lain's SpuRead uploads */

        /* round trip: decode what landed in SPU RAM and compare */
        check_ram = (uint8_t *)malloc(enc_size);
        LainSpu_ReadRam(addr, check_ram, enc_size);
        {
            static const int f0[5] = { 0, 60, 115, 98, 122 }, f1[5] = { 0, 0, -52, -55, -60 };
            int b, h1 = 0, h2 = 0, k = 0;
            for (b = 0; b < enc_size / 16; b++) {
                const uint8_t *blk = check_ram + b * 16;
                int sh = blk[0] & 15, fl = blk[0] >> 4, j;
                for (j = 0; j < 28 && k < n; j++, k++) {
                    int nib = (blk[2 + j / 2] >> ((j & 1) * 4)) & 15;
                    int s = ((int16_t)(uint16_t)(nib << 12)) >> sh;
                    s += (h1 * f0[fl] + h2 * f1[fl] + 32) >> 6;
                    if (s > 32767) s = 32767;
                    if (s < -32768) s = -32768;
                    h2 = h1; h1 = s;
                    err += (double)(s - raw[k]) * (s - raw[k]);
                    sig += (double)raw[k] * raw[k];
                }
            }
            printf("  EncSPU: %d bytes -> %d bytes, round-trip SNR %.1f dB\n", n * 2, enc_size,
                   10 * log10(sig / (err > 0 ? err : 1)));
            check(10 * log10(sig / (err > 0 ? err : 1)) > 20, "EncSPU round trip SNR > 20 dB");
        }
        free(check_ram);

        memset(&attr, 0, sizeof(attr));
        attr.mask = 0xFF93;
        attr.voice = 0x800000;
        attr.volume.left = 0x32C8;
        attr.volume.right = 0x32C8;
        attr.pitch = 0x400;                   /* 11025 Hz source */
        attr.a_mode = SPU_VOICE_LINEARIncN;
        attr.s_mode = SPU_VOICE_LINEARIncN;
        attr.r_mode = SPU_VOICE_LINEARDecN;
        attr.sl = 0xF;
        attr.addr = addr;
        SpuSetVoiceAttr(&attr);
        memset(&attr, 0, sizeof(attr));
        attr.mask = SPU_VOICE_PITCH;
        attr.voice = 0x800000;
        attr.pitch = 0x400;
        SpuSetKeyOnWithAttr(&attr);
        render(&pcm, 0.5);
        check(SpuGetKeyStatus(0x800000) == SPU_ON, "voice 23 keyed on");
        render(&pcm, 0.7);
        check(SpuGetKeyStatus(0x800000) == SPU_OFF, "one-shot sample ends (loop end + mute flag)");
        SpuSetKey(SPU_OFF, 0x800000);
        SpuFree(addr);
        write_wav("spu_voice.wav", &pcm);
        report("spu_voice.wav", &pcm, 0, (size_t)(0.9 * RATE));
        check(stats(&pcm, 0, (size_t)(0.9 * RATE)).rms_db > -30, "direct voice audible");
        free(raw);
        free(enc);
    }
    free(pcm.s);

    /* ---- 5: CD input + capture + IRQ double buffer (movie level meter) ---- */
    memset(&pcm, 0, sizeof(pcm));
    {
        SpuCommonAttr c;
        int levels[8], got = 0;
        memset(&c, 0, sizeof(c));
        c.mask = 0x2C3;                 /* spu_cd_audio_on: master + CD volume 0x3FFF */
        c.mvol.left = c.mvol.right = 0x3FFF;
        c.cd.volume.left = c.cd.volume.right = 0x3FFF;
        c.cd.mix = 1;
        SpuSetCommonAttr(&c);
        LainSpu_SetCdSource(cd_sine, NULL);
        SpuSetTransferCallback(xfer_cb);
        SpuSetIRQCallback(irq_cb);
        SpuSetIRQAddr(s_irq_addr = 0x100);
        SpuSetIRQ(SPU_ON);
        for (i = 0; i < 20; i++) {
            render(&pcm, 0.1);
            if (got < 8)
                levels[got++] = meter_level();
        }
        SpuSetIRQ(SPU_OFF);
        SpuSetIRQCallback(NULL);
        SpuSetTransferCallback(NULL);
        LainSpu_SetCdSource(NULL, NULL);
        write_wav("cd_capture.wav", &pcm);
        report("cd_capture.wav", &pcm, 0, pcm.frames);
        printf("  SPU IRQs: %d, transfer callbacks: %d (expected ~%d), meter levels:", s_irqs, s_xfers,
               (int)(2.0 * RATE / 256));
        for (i = 0; i < got; i++)
            printf(" %d", levels[i]);
        printf("\n");
        check(s_irqs > 300 && s_irqs < 400, "IRQ fires every 256 captured samples");
        check(levels[got - 1] == 3, "level meter reads the captured CD audio (16000/4681 = 3)");
        check(stats(&pcm, 0, pcm.frames).rms_db > -20, "CD input mixed at CD volume");
    }
    free(pcm.s);

    printf("\n%s (%d failure%s)\n", failures ? "FAILED" : "all checks passed", failures,
           failures == 1 ? "" : "s");
    free(snd);
    return failures ? 1 : 0;
}
