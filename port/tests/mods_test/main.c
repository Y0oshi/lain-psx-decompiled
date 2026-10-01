/* Tests for the mod support (port/client: napk.c, png.c, tim.c, mods.cpp).
 *
 *   mods_test                      unit tests (napk, PNG round trips, TIM)
 *   mods_test --pngs <dir>         decode the PNGs from gen_pngs.py and compare
 *   mods_test --reader <data dir>  apply the mods in <data dir>/mods and read every
 *                                  replaced PNG entry back through the CD drive, on
 *                                  each imported disc that has its archive
 * Exit status 0 when everything passes.
 */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "iso.h"
#include "lain_xa.h"
#include "mods.h"
#include "napk.h"
#include "png.h"
#include "psx_arena.h"
#include "settings.h"
#include "tim.h"

extern void PsyX_CDFS_Init(const char *imageFileName, int track, int sectorSize);
extern void PsyX_CDFS_SwapImage(const char *imageFileName);

static int failures;

#define CHECK(cond, ...)                                                                                               \
    do {                                                                                                               \
        if (!(cond)) {                                                                                                 \
            failures++;                                                                                                \
            printf("FAIL %s:%d: ", __FILE__, __LINE__);                                                                \
            printf(__VA_ARGS__);                                                                                       \
            printf("\n");                                                                                              \
        }                                                                                                              \
    } while (0)

static uint32_t rnd_state = 12345;
static uint32_t rnd(void) {
    rnd_state = rnd_state * 1103515245u + 12345u;
    return rnd_state >> 8;
}

/* The game's lz_decompress (src/game/800379C8.c), steps unchanged, into a buffer
 * sized from the header like the game's heap_alloc. */
static uint8_t *game_lz_decompress(const uint8_t *src, int32_t size, int32_t *out_size) {
    if (strncmp((const char *)src, "napk", 4) != 0) return NULL;
    src += 4;
    int32_t remaining = (int32_t)(src[0] | src[1] << 8 | src[2] << 16 | (uint32_t)src[3] << 24);
    src += 4;
    size -= 8;
    uint8_t *dst = malloc(remaining + 1);
    int32_t out = 0;
    *out_size = remaining;
    while (size != 0) {
        if (remaining == 0) break;
        size--;
        uint8_t flags = *src++;
        for (int bit = 0; bit < 8 && size > 0 && remaining > 0; bit++) {
            if (flags & (0x80 >> bit)) {
                int32_t back = *src++ + 1, len = *src++ + 3;
                if (out + len > *out_size || back > out) { /* the game would overrun its buffer */
                    free(dst);
                    return NULL;
                }
                for (int32_t i = 0; i < len; i++) (dst + out)[i] = (dst + out - back)[i];
                out += len;
                size -= 2;
                remaining -= len;
            } else {
                dst[out++] = *src++;
                size--;
                remaining--;
            }
        }
    }
    return remaining == 0 ? dst : (free(dst), NULL);
}

static void test_napk(void) {
    for (int t = 0; t < 300; t++) {
        size_t n = t < 5 ? (size_t)t : 1 + rnd() % 20000;
        uint8_t *d = malloc(n + 1);
        int kind = t % 4;
        for (size_t i = 0; i < n; i++) {
            d[i] = kind == 0 ? (uint8_t)rnd() : kind == 1 ? (uint8_t)(rnd() % 3) : kind == 2 ? 0 : (uint8_t)(i % 7);
        }
        size_t pn = 0, un = 0;
        uint8_t *p = napk_pack(d, n, &pn);
        CHECK(p != NULL, "napk_pack failed (n=%zu)", n);
        if (!p) continue;
        uint8_t *u = napk_unpack(p, pn, &un);
        CHECK(u && un == n && memcmp(u, d, n) == 0, "napk round trip (kind %d, n=%zu)", kind, n);
        int32_t gn = 0;
        uint8_t *g = game_lz_decompress(p, (int32_t)pn, &gn);
        CHECK(g && gn == (int32_t)n && memcmp(g, d, n) == 0, "game decompressor disagrees (kind %d, n=%zu)", kind, n);
        free(d);
        free(p);
        free(u);
        free(g);
    }
}

static void test_png_roundtrip(void) {
    const char *path = "mods_test_roundtrip.png";
    int sizes[][2] = {{1, 1}, {7, 3}, {256, 136}, {640, 480}};
    for (size_t s = 0; s < sizeof sizes / sizeof sizes[0]; s++) {
        int w = sizes[s][0], h = sizes[s][1];
        uint8_t *px = malloc((size_t)w * h * 4);
        for (int i = 0; i < w * h * 4; i++) px[i] = s == 2 ? (uint8_t)((i / 4) % 5 * 50) : (uint8_t)rnd();
        CHECK(png_save(path, px, w, h) == 0, "png_save %dx%d", w, h);
        int rw = 0, rh = 0;
        char err[128];
        uint8_t *back = png_load(path, &rw, &rh, err, sizeof err);
        CHECK(back && rw == w && rh == h && memcmp(back, px, (size_t)w * h * 4) == 0, "PNG round trip %dx%d (%s)",
              w, h, back ? "pixels differ" : err);
        free(px);
        free(back);
    }
    remove(path);
    int w, h;
    char err[128];
    uint8_t junk[64] = {0x89, 'P', 'N', 'G', '\r', '\n', 0x1A, '\n'};
    CHECK(png_decode(junk, sizeof junk, &w, &h, err, sizeof err) == NULL, "damaged PNG accepted");
}

/* A 4-bit TIM with two palettes (like the game's fonts), 8x2 pixels. */
static void test_tim(void) {
    uint8_t t[8 + 12 + 64 + 12 + 8];
    memset(t, 0, sizeof t);
    t[0] = 0x10;
    t[4] = 0x08; /* 4-bit, CLUT */
    t[8] = 12 + 64; /* CLUT block length */
    t[16] = 16;  /* 16 colors */
    t[18] = 2;   /* 2 palettes */
    for (int i = 0; i < 32; i++) {
        uint16_t c = i == 0 || i == 16 ? 0 : (uint16_t)(i * 997 % 0x7FFF) | 0x0001;
        if (i == 5) c = 7 * 997 % 0x7FFF | 1; /* palette 0 holds color 7 twice */
        t[20 + i * 2] = (uint8_t)c;
        t[21 + i * 2] = (uint8_t)(c >> 8);
    }
    size_t p = 8 + 12 + 64;
    t[p] = 12 + 8;
    t[p + 8] = 2; /* 2 halfwords = 8 pixels */
    t[p + 10] = 2;
    for (int i = 0; i < 8; i++) t[p + 12 + i] = (uint8_t)(i * 0x21 + 5 * (i == 3));
    TimInfo ti;
    CHECK(tim_parse(t, sizeof t, &ti) && ti.bpp == 4 && ti.w == 8 && ti.h == 2 && ti.clut_h == 2, "tim_parse");
    uint8_t *rgba = tim_to_rgba(t, sizeof t, &ti, 0);
    uint8_t copy[sizeof t];
    memcpy(copy, t, sizeof t);
    char msg[128];
    int r = tim_from_rgba(copy, sizeof copy, &ti, rgba, msg, sizeof msg);
    CHECK(r == 0 && memcmp(copy, t, sizeof t) == 0, "unchanged TIM changes on the way back (%d, %s)", r, msg);
    free(rgba);
}

static int test_pngs(const char *dir) {
    char list[1200];
    snprintf(list, sizeof list, "ls '%s'/*.png", dir);
    FILE *ls = popen(list, "r");
    char path[1024];
    int n = 0;
    while (ls && fgets(path, sizeof path, ls)) {
        path[strcspn(path, "\n")] = 0;
        int w = 0, h = 0;
        char err[128] = "";
        uint8_t *got = png_load(path, &w, &h, err, sizeof err);
        char exp_path[1100];
        snprintf(exp_path, sizeof exp_path, "%.*s.rgba", (int)(strlen(path) - 4), path);
        FILE *f = fopen(exp_path, "rb");
        uint32_t ew = 0, eh = 0;
        if (f) {
            fread(&ew, 4, 1, f);
            fread(&eh, 4, 1, f);
        }
        uint8_t *exp = malloc((size_t)ew * eh * 4 + 1);
        size_t en = f ? fread(exp, 1, (size_t)ew * eh * 4, f) : 0;
        if (f) fclose(f);
        CHECK(got && (uint32_t)w == ew && (uint32_t)h == eh && en == (size_t)w * h * 4 && !memcmp(got, exp, en),
              "%s: %s", path, got ? "pixels differ from Pillow" : err);
        free(got);
        free(exp);
        n++;
    }
    if (ls) pclose(ls);
    printf("%d PNGs decoded\n", n);
    return n;
}

/* Reads a whole entry through the drive: size from the (patched) table in the arena. */
static uint8_t *read_through_drive(uint32_t archive_lba, uint32_t table, int entry, uint32_t *size_out) {
    const uint8_t *e = PSX_PTR(table + (uint32_t)entry * 8);
    uint32_t sector = e[0] | e[1] << 8 | e[2] << 16 | (uint32_t)e[3] << 24;
    uint32_t size = e[4] | e[5] << 8 | e[6] << 16 | (uint32_t)e[7] << 24;
    uint8_t *out = malloc(size + 2048), raw[2352];
    for (uint32_t off = 0; off < size; off += 2048) {
        if (!LainCD_ReadRawSector((int)(archive_lba + sector + off / 2048), raw)) {
            free(out);
            return NULL;
        }
        memcpy(out + off, raw + 24, 2048);
    }
    *size_out = size;
    return out;
}

static void test_reader(const char *data_dir) {
    static const struct {
        const char *name;
        uint32_t table;
        int disc_mask; /* 1 = disc 1, 2 = disc 2 */
    } archives[] = {{"BIN.BIN", 0x80098EE8u, 3}, {"SITEA.BIN", 0x8009A460u, 1}, {"SITEB.BIN", 0x8009BD10u, 2}};
    char d[2][1100];
    snprintf(d[0], sizeof d[0], "%sdisc1.bin", data_dir);
    snprintf(d[1], sizeof d[1], "%sdisc2.bin", data_dir);
    Settings s;
    settings_load(&s);
    /* the tables come from the program on disc 1, as psx_mem_load does */
    FILE *f1 = fopen(d[0], "rb");
    uint32_t lba, size;
    if (!f1 || iso_find_root_file(f1, "SLPS_016.03", &lba, &size) != 0) {
        CHECK(0, "no disc 1 in %s", data_dir);
        if (f1) fclose(f1);
        return;
    }
    uint8_t *exe = iso_read_file(f1, lba, size);
    CHECK(exe != NULL, "can't read SLPS_016.03");
    if (!exe) {
        fclose(f1);
        return;
    }
    memcpy(PSX_PTR(0x80010000u), exe + 0x800, size - 0x800);
    free(exe);
    fclose(f1);
    int replaced = mods_apply(s.mods, d[0], d[1]);
    printf("%d entries replaced\n", replaced);
    int checked = 0;
    PsyX_CDFS_Init(d[0], 0, 0);
    for (int disc = 0; disc < 2; disc++) {
        if (disc == 1) PsyX_CDFS_SwapImage(d[1]);
        FILE *img = fopen(d[disc], "rb");
        if (!img) continue;
        for (size_t a = 0; a < sizeof archives / sizeof archives[0]; a++) {
            if (!(archives[a].disc_mask & (1 << disc))) continue;
            uint32_t alba, asize;
            if (iso_find_root_file(img, archives[a].name, &alba, &asize) != 0) continue;
            /* every PNG an enabled mod has for this archive */
            char cmd[1400];
            snprintf(cmd, sizeof cmd, "ls '%smods'/*/%s/*.png 2>/dev/null", data_dir, archives[a].name);
            FILE *ls = popen(cmd, "r");
            char path[1024];
            while (ls && fgets(path, sizeof path, ls)) {
                path[strcspn(path, "\n")] = 0;
                const char *base = strrchr(path, '/') + 1;
                int entry = atoi(base);
                uint32_t n = 0;
                uint8_t *stored = read_through_drive(alba, archives[a].table, entry, &n);
                size_t un = 0;
                uint8_t *u = NULL;
                if (stored && napk_is_packed(stored, n)) {
                    u = napk_unpack(stored, n, &un);
                } else if (stored) { /* stored unpacked */
                    u = malloc(n);
                    memcpy(u, stored, n);
                    un = n;
                }
                TimInfo ti;
                int w = 0, h = 0;
                char err[128];
                uint8_t *want = png_load(path, &w, &h, err, sizeof err);
                uint8_t *got = u && tim_parse(u, un, &ti) ? tim_to_rgba(u, un, &ti, 0) : NULL;
                /* compare as the PS1 stores colors (5 bits per channel, transparent or not) */
                int same = got && want && ti.w == w && ti.h == h;
                for (int i = 0; same && i < w * h; i++) {
                    int ta = want[i * 4 + 3] < 128, ga = got[i * 4 + 3] == 0;
                    same = ta == ga && (ta || ((want[i * 4] >> 3) == (got[i * 4] >> 3) &&
                                               (want[i * 4 + 1] >> 3) == (got[i * 4 + 1] >> 3) &&
                                               (want[i * 4 + 2] >> 3) == (got[i * 4 + 2] >> 3)));
                }
                printf("disc %d %s/%04d: %s\n", disc + 1, archives[a].name, entry, same ? "matches the mod" : "DIFFERENT");
                /* a file the Mods tab rejects is expected to read as the original */
                if (!same) printf("  (fine if --check-mods reports this file as not used)\n");
                checked += same;
                free(stored);
                free(u);
                free(want);
                free(got);
            }
            if (ls) pclose(ls);
        }
        fclose(img);
    }
    /* whole movie files: MOVIE/*.STR of the enabled mods, read back where the
     * game's disc 1 table now points */
    char cmd[1400];
    snprintf(cmd, sizeof cmd, "ls '%smods'/*/MOVIE/*.STR 2>/dev/null", data_dir);
    FILE *ls = popen(cmd, "r");
    char path[1024];
    PsyX_CDFS_SwapImage(d[0]);
    FILE *img = fopen(d[0], "rb");
    while (ls && img && fgets(path, sizeof path, ls)) {
        path[strcspn(path, "\n")] = 0;
        char rel[64];
        snprintf(rel, sizeof rel, "%s", strrchr(path, '/') + 1);
        /* the game's table entry now past the disc end, with this file's size */
        FILE *mf = fopen(path, "rb");
        if (!mf) continue;
        fseek(mf, 0, SEEK_END);
        long n = ftell(mf);
        fseek(mf, 0, SEEK_SET);
        uint8_t *want = malloc((size_t)n);
        fread(want, 1, (size_t)n, mf);
        fclose(mf);
        uint32_t sectors = (uint32_t)(n / 2352);
        int found = -1;
        for (int i = 0; i < 69; i++) {
            const uint8_t *e = PSX_PTR(0x80071678u + (uint32_t)i * 8);
            uint32_t pos = e[0] | e[1] << 8 | e[2] << 16 | (uint32_t)e[3] << 24;
            uint32_t size = e[4] | e[5] << 8 | e[6] << 16 | (uint32_t)e[7] << 24;
            if (pos >= 400000 && size == sectors * 2336) found = i;
        }
        CHECK(found >= 0, "%s: no table entry points past the disc end", rel);
        if (found < 0) {
            free(want);
            continue;
        }
        const uint8_t *e = PSX_PTR(0x80071678u + (uint32_t)found * 8);
        uint32_t pos = e[0] | e[1] << 8 | e[2] << 16 | (uint32_t)e[3] << 24;
        uint8_t raw[2352];
        uint32_t same = 0;
        for (uint32_t i = 0; i < sectors; i++) {
            if (LainCD_ReadRawSector((int)(pos + i), raw) && !memcmp(raw + 16, want + (size_t)i * 2352 + 16, 2336)) same++;
        }
        printf("disc 1 %s: table entry %d at LBA %u, %u of %u sectors match the mod\n", rel, found, pos, same, sectors);
        CHECK(same == sectors, "%s: sectors differ", rel);
        free(want);
    }
    if (ls) pclose(ls);
    if (img) fclose(img);
    CHECK(checked > 0, "no replaced picture read back");
}

int main(int argc, char **argv) {
    if (argc >= 3 && strcmp(argv[1], "--pngs") == 0) {
        CHECK(test_pngs(argv[2]) > 0, "no PNGs in %s", argv[2]);
    } else if (argc >= 3 && strcmp(argv[1], "--reader") == 0) {
        char dir[1024];
        snprintf(dir, sizeof dir, "%s/", argv[2]);
        setenv("LAIN_DATA_DIR", argv[2], 1);
        test_reader(dir);
    } else {
        test_napk();
        test_png_roundtrip();
        test_tim();
    }
    printf(failures ? "%d FAILED\n" : "all passed\n", failures);
    return failures ? 1 : 0;
}
