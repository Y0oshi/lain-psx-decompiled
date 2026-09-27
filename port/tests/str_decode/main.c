/*
 * str_decode: plays a Lain STR movie through the port's St* streaming and
 * software MDEC exactly the way the game's player (src/game/80018F48.c,
 * movie_start_stream / movie_decode_frame / movie_dct_out_callback) drives them, into a fake
 * VRAM, and writes some frames as PNG.
 *
 *   str_decode [file.STR] [out_dir] [--realtime] [--exe SLPS_016.03]
 *
 * Defaults: <repo>/extract/disc1/MOVIE/F001.STR, <repo>/extract/disc1/SLPS_016.03
 * (for the VLC run/level table), <build>/out. These are the user's own
 * extracted disc files; nothing from them is kept in the repository.
 */
#include "psx/types.h"
#include "psx/libcd.h"
#include "psx/libpress.h"

#include "lain_vlc.h"
#include "png.h"

#include <SDL.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#ifdef _WIN32
#include <direct.h>
#define mkdir(p, m) _mkdir(p)
#endif

#define VRAM_W 1024
#define VRAM_H 512

static FILE *g_str;
static int g_audio_sectors;

static int str_reader(int lba, u_char *out, void *user)
{
    (void)user;
    if (lba < 0 || fseek(g_str, (long)lba * 2352, SEEK_SET) != 0)
        return 0;
    return fread(out, 1, 2352, g_str) == 2352;
}

static void audio_sink(const u_char *sector, int lba, void *user)
{
    (void)sector; (void)lba; (void)user;
    g_audio_sectors++;
}

/* ---- the game's decode environment (DECENV) and slice callback ---------- */

typedef struct { short x, y, w, h; } Rect16;

static struct {
    u_int *vlcbuf[2];
    int vlcid;
    u_int *imgbuf[2];
    int imgid;
    Rect16 rect[2];
    int rectid;
    Rect16 slice;
    volatile int isdone;
} dec;

static u_short g_vram[VRAM_H][VRAM_W];
static int g_uploads;

static void load_image(const Rect16 *r, const u_int *p)
{
    const u_short *src = (const u_short *)p;
    int y;
    for (y = 0; y < r->h; y++)
        memcpy(&g_vram[r->y + y][r->x], src + y * r->w, (size_t)r->w * 2);
    g_uploads++;
}

/* movie_dct_out_callback: DecDCTout callback. */
static void out_callback(void)
{
    Rect16 snap = dec.slice;
    int id = dec.imgid;

    dec.imgid = dec.imgid ? 0 : 1;
    dec.slice.x += dec.slice.w;
    if (dec.slice.x < dec.rect[dec.rectid].x + dec.rect[dec.rectid].w) {
        DecDCTout(dec.imgbuf[dec.imgid], dec.slice.w * dec.slice.h / 2);
    } else {
        dec.isdone = 1;
        dec.rectid = dec.rectid == 0;
        dec.slice.x = dec.rect[dec.rectid].x;
        dec.slice.y = dec.rect[dec.rectid].y;
    }
    load_image(&snap, dec.imgbuf[id]);
}

/* ---- helpers ------------------------------------------------------------- */

static void vram_to_rgb(int mode24, int x0, int y0, int w, int h, u_char *rgb)
{
    int x, y;
    for (y = 0; y < h; y++)
        for (x = 0; x < w; x++) {
            u_char *d = rgb + ((size_t)y * w + x) * 3;
            if (mode24) {
                const u_char *row = (const u_char *)&g_vram[y0 + y][x0];
                d[0] = row[x * 3 + 0];
                d[1] = row[x * 3 + 1];
                d[2] = row[x * 3 + 2];
            } else {
                u_short p = g_vram[y0 + y][x0 + x];
                d[0] = (u_char)((p & 31) << 3 | (p & 31) >> 2);
                d[1] = (u_char)(((p >> 5) & 31) << 3 | ((p >> 5) & 31) >> 2);
                d[2] = (u_char)(((p >> 10) & 31) << 3 | ((p >> 10) & 31) >> 2);
            }
        }
}

/* Mean absolute difference between neighbouring pixels: noise is ~80+,
 * a real picture is far smoother. */
static double roughness(const u_char *rgb, int w, int h)
{
    double s = 0;
    long n = 0;
    int x, y, c;
    for (y = 0; y < h; y++)
        for (x = 1; x < w; x++)
            for (c = 0; c < 3; c++, n++)
                s += abs(rgb[(y * w + x) * 3 + c] - rgb[(y * w + x - 1) * 3 + c]);
    return n ? s / n : 0;
}

static int is_wanted(u_int frame)
{
    return frame == 1 || frame == 10 || frame == 30 || frame == 45 || frame == 59;
}

/* ---- the game's run/level table ------------------------------------------ */

/* Lain's VLC decoder takes its (run, level) values from a 111-entry table in
 * the executable (g_movie_vlc_table). It is game data, so it is read from the user's
 * extracted SLPS_016.03 at run time. */
static unsigned short g_rl_table[111];

static int load_vlc_table(const char *exe_path)
{
    unsigned char hdr[0x800], buf[222];
    unsigned int text;
    long off;
    int i;
    FILE *f = fopen(exe_path, "rb");
    if (!f)
        return 0;
    if (fread(hdr, 1, sizeof(hdr), f) != sizeof(hdr) || memcmp(hdr, "PS-X EXE", 8) != 0) {
        fclose(f);
        return 0;
    }
    text = hdr[0x18] | hdr[0x19] << 8 | hdr[0x1A] << 16 | (unsigned int)hdr[0x1B] << 24;
    off = (long)(0x800A5CE4u - text) + 0x800;
    if (fseek(f, off, SEEK_SET) != 0 || fread(buf, 1, sizeof(buf), f) != sizeof(buf)) {
        fclose(f);
        return 0;
    }
    fclose(f);
    for (i = 0; i < 111; i++)
        g_rl_table[i] = (unsigned short)(buf[i * 2] | buf[i * 2 + 1] << 8);
    return 1;
}

/* ---- one playback pass --------------------------------------------------- */

static int play(const char *out_dir, int mode24, int realtime)
{
    static u_int ring[0x10000 / 4];
    const int width = 320, height = 240;
    int frames = 0, errors = 0, written = 0, idle = 0;
    u_int *addr, *hdrp;
    double t0 = (double)SDL_GetPerformanceCounter() / SDL_GetPerformanceFrequency(), t1;

    /* movie_start_stream: buffers and rectangles */
    dec.vlcbuf[0] = malloc(0x28000);
    dec.vlcbuf[1] = malloc(0x28000);
    dec.imgbuf[0] = malloc(mode24 ? 0x2D00 * 4 : 0x1E00 * 4);
    dec.imgbuf[1] = malloc(mode24 ? 0x2D00 * 4 : 0x1E00 * 4);
    dec.vlcid = dec.imgid = dec.rectid = 0;
    dec.rect[0].x = 0; dec.rect[0].y = 0;
    dec.rect[1].x = 0; dec.rect[1].y = 240;
    dec.rect[0].w = dec.rect[1].w = (short)(mode24 ? width * 3 / 2 : width);
    dec.rect[0].h = dec.rect[1].h = (short)height;
    dec.slice.x = 0; dec.slice.y = 0;
    dec.slice.w = (short)(mode24 ? 24 : 16);
    dec.slice.h = (short)height;
    dec.isdone = 0;

    DecDCTReset(0);
    DecDCToutCallback(out_callback);
    StSetRing(ring, 0x20);
    StSetStream(mode24 ? 1 : 0, 1, 0xFFFFFFFFu, 0, 0);
    LainSt_SetRealtime(realtime);
    LainSt_SetSectorReader(str_reader, NULL);
    LainSt_SetAudioSink(audio_sink, NULL);
    g_audio_sectors = 0;
    LainSt_StartAt(0, CdlModeStream2 | CdlModeSpeed | CdlModeRT);

    for (;;) {
        StHEADER *h;
        int words;
        u_int frame;

        if (StGetNext(&addr, &hdrp) != 0) {
            if (!LainSt_IsActive() || ++idle > 100000)
                break; /* end of file (or stuck) */
            continue;
        }
        idle = 0;
        h = (StHEADER *)hdrp;
        frame = h->frameCount;

        /* func_8001B2A8_nextVlc: VLC into the other buffer */
        dec.vlcid ^= 1;
        words = lain_vlc_decode(addr, (int)h->nSectors * 2016, dec.vlcbuf[dec.vlcid], 0x28000 / 4);
        StFreeRing(addr);
        if (words < 0) {
            fprintf(stderr, "frame %u: VLC decode failed\n", frame);
            errors++;
            continue;
        }
        if (h->width != width || h->height != height) {
            fprintf(stderr, "frame %u: unexpected size %dx%d\n", frame, h->width, h->height);
            errors++;
            continue;
        }

        /* movie_decode_frame: MDEC the frame; the callback walks the slices */
        dec.isdone = 0;
        g_uploads = 0;
        DecDCTin(dec.vlcbuf[dec.vlcid], mode24 ? 3 : 2);
        DecDCTout(dec.imgbuf[dec.imgid], dec.slice.w * dec.slice.h / 2);
        if (!dec.isdone || g_uploads != width / 16) {
            fprintf(stderr, "frame %u: callback chain incomplete (%d slices)\n", frame, g_uploads);
            errors++;
        }
        frames++;

        if (is_wanted(frame)) {
            static u_char rgb[320 * 240 * 3];
            char path[1024];
            int shown = dec.rectid ^ 1; /* the rect just completed */
            double r;
            vram_to_rgb(mode24, 0, dec.rect[shown].y, width, height, rgb);
            r = roughness(rgb, width, height);
            snprintf(path, sizeof(path), "%s/F001_%s_frame%03u.png", out_dir, mode24 ? "24" : "16", frame);
            if (write_png_rgb(path, rgb, width, height)) {
                printf("  wrote %s (neighbour diff %.1f)\n", path, r);
                written++;
            }
            if (r > 40.0) {
                fprintf(stderr, "frame %u looks like noise\n", frame);
                errors++;
            }
        }
    }
    t1 = (double)SDL_GetPerformanceCounter() / SDL_GetPerformanceFrequency();

    StUnSetRing();
    DecDCToutCallback(0);
    free(dec.vlcbuf[0]); free(dec.vlcbuf[1]);
    free(dec.imgbuf[0]); free(dec.imgbuf[1]);

    printf("%s: %d frames, %d XA audio sectors, %d errors, %d images, %.2fs%s\n",
           mode24 ? "24bpp" : "15bpp", frames, g_audio_sectors, errors, written, t1 - t0,
           realtime ? " (real time)" : "");
    return errors == 0 && frames > 0 ? 0 : 1;
}

int main(int argc, char **argv)
{
    const char *path = LAIN_REPO_ROOT "/extract/disc1/MOVIE/F001.STR";
    const char *out = STR_DECODE_OUT;
    const char *exe = LAIN_REPO_ROOT "/extract/disc1/SLPS_016.03";
    int realtime = 0, i, rc = 0, npos = 0;

    for (i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--realtime"))
            realtime = 1;
        else if (!strcmp(argv[i], "--exe") && i + 1 < argc)
            exe = argv[++i];
        else if (npos++ == 0)
            path = argv[i];
        else
            out = argv[i];
    }
    if (!load_vlc_table(exe)) {
        fprintf(stderr, "cannot load the run/level table from %s\n", exe);
        return 1;
    }
    if (!lain_vlc_init(g_rl_table)) {
        fprintf(stderr, "VLC table self-check failed\n");
        return 1;
    }
    g_str = fopen(path, "rb");
    if (!g_str) {
        fprintf(stderr, "cannot open %s (extract the disc first)\n", path);
        return 1;
    }
    mkdir(out, 0755);
    printf("decoding %s\n", path);
    rc |= play(out, 0, realtime);
    rc |= play(out, 1, realtime);
    fclose(g_str);
    printf(rc ? "FAILED\n" : "OK\n");
    return rc;
}
