#include "tracks.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "iso.h"
#include "lain_xa.h"
#include "libcd.h"

/* Lain's voice files and movies are mastered for double speed (150 sectors/s);
 * a voice file interleaves its channels, so each channel's playback time equals
 * the time spent reading the file: (sector - file start) / 150. */
#define SECTORS_PER_SECOND 150

typedef struct {
    char name[24];
    uint32_t lba, sectors;
    int is_xa;
} Extent;

static Extent *s_ext[2];
static int s_count[2];

typedef struct {
    int disc;
    int is_xa;
} IndexCtx;

static void add_extent(const char *name, uint32_t lba, uint32_t size, void *user) {
    IndexCtx *c = user;
    size_t n = strlen(name);
    int xa = n > 3 && strcmp(name + n - 3, ".XA") == 0;
    int str = n > 4 && strcmp(name + n - 4, ".STR") == 0;
    if (!xa && !str) {
        return;
    }
    s_ext[c->disc] = realloc(s_ext[c->disc], (s_count[c->disc] + 1) * sizeof(Extent));
    Extent *e = &s_ext[c->disc][s_count[c->disc]++];
    snprintf(e->name, sizeof e->name, "%s", name);
    e->lba = lba;
    e->sectors = (size + 2047) / 2048;
    e->is_xa = xa;
}

void tracks_init(const char *disc1_bin, const char *disc2_bin) {
    const char *bins[2] = {disc1_bin, disc2_bin};
    for (int d = 0; d < 2; d++) {
        free(s_ext[d]);
        s_ext[d] = NULL;
        s_count[d] = 0;
        FILE *img = bins[d] ? fopen(bins[d], "rb") : NULL;
        if (!img) {
            continue;
        }
        IndexCtx c = {d, 0};
        iso_list_dir(img, "XA", add_extent, &c);
        iso_list_dir(img, "MOVIE", add_extent, &c);
        iso_list_dir(img, "MOVIE2", add_extent, &c);
        fclose(img);
    }
}

static const Extent *find(int disc, int lba, int want_xa) {
    for (int i = 0; i < s_count[disc]; i++) {
        const Extent *e = &s_ext[disc][i];
        if (e->is_xa == want_xa && (uint32_t)lba >= e->lba && (uint32_t)lba < e->lba + e->sectors) {
            return e;
        }
    }
    return NULL;
}

int tracks_current(int disc, char *key, int key_cap, long *ms) {
    if (disc < 0 || disc > 1) {
        return 0;
    }
    /* A movie stream takes priority: its audio is interleaved in the STR file. */
    int lba = LainSt_GetPosition();
    const Extent *e = lba >= 0 ? find(disc, lba, 0) : NULL;
    if (e) {
        snprintf(key, key_cap, "%s", e->name);
        *ms = (long)(lba - (int)e->lba) * 1000 / SECTORS_PER_SECOND;
        return 1;
    }
    int chan = LainCD_GetXAChannel();
    if (chan < 0) {
        return 0;
    }
    lba = LainCD_GetPosition();
    e = find(disc, lba, 1);
    if (!e) {
        return 0;
    }
    snprintf(key, key_cap, "%s.ch%02d", e->name, chan);
    *ms = (long)(lba - (int)e->lba) * 1000 / SECTORS_PER_SECOND;
    return 1;
}
