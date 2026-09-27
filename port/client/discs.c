#include "discs.h"

#include <SDL.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "sha1.h"

#define RAW_SECTOR 2352
#define COPY_CHUNK (RAW_SECTOR * 256)

/* SHA-1 of the Redump-format data track of each retail disc (Japan, SLPS-01603/01604).
 * Only these hashes ship with the client; no disc content does. */
static const struct {
    int disc;
    const char *sha1;
} KNOWN_DUMPS[] = {
    {DISC_1, "6426fbdb45f27e089af650cddb8c41929c7890de"},
    {DISC_2, "409668af454a0e0a33a71c1d22918d765e718608"},
};

static void set_err(char *err, int cap, const char *msg) {
    if (err && cap > 0) {
        snprintf(err, cap, "%s", msg);
    }
}

static int ends_with_ci(const char *s, const char *suffix) {
    size_t n = strlen(s), m = strlen(suffix);
    if (n < m) {
        return 0;
    }
    for (size_t i = 0; i < m; i++) {
        if (tolower((unsigned char)s[n - m + i]) != tolower((unsigned char)suffix[i])) {
            return 0;
        }
    }
    return 1;
}

int discs_resolve_track(const char *path, char *bin_out, int bin_cap, char *err, int err_cap) {
    if (ends_with_ci(path, ".bin") || ends_with_ci(path, ".img")) {
        snprintf(bin_out, bin_cap, "%s", path);
        return 0;
    }
    if (ends_with_ci(path, ".chd")) {
        set_err(err, err_cap, "CHD images aren't supported yet; convert with: chdman extractcd -i disc.chd -o disc.cue");
        return -1;
    }
    if (!ends_with_ci(path, ".cue")) {
        set_err(err, err_cap, "Choose the disc's .cue (or .bin) file");
        return -1;
    }
    FILE *f = fopen(path, "r");
    if (!f) {
        set_err(err, err_cap, "Can't open the .cue file");
        return -1;
    }
    char line[1024], track_file[1024] = "";
    while (fgets(line, sizeof line, f)) {
        char *q1 = strchr(line, '"'), *q2 = q1 ? strrchr(line, '"') : NULL;
        if (strncmp(line, "FILE", 4) == 0 && q1 && q2 > q1 && !track_file[0]) {
            *q2 = 0;
            snprintf(track_file, sizeof track_file, "%s", q1 + 1);
        }
    }
    fclose(f);
    if (!track_file[0]) {
        set_err(err, err_cap, "The .cue file lists no track");
        return -1;
    }
    /* Track file names are relative to the .cue's folder. */
    const char *slash = strrchr(path, '/');
#ifdef _WIN32
    const char *bslash = strrchr(path, '\\');
    if (!slash || (bslash && bslash > slash)) {
        slash = bslash;
    }
#endif
    int dir_len = slash ? (int)(slash - path + 1) : 0;
    snprintf(bin_out, bin_cap, "%.*s%s", dir_len, path, track_file);
    return 0;
}

/* Minimal ISO9660 over raw Mode 2 Form 1 sectors (user data at offset 24). */

static int read_user_sector(FILE *f, uint32_t lba, uint8_t out[2048]) {
    uint8_t raw[RAW_SECTOR];
    if (fseek(f, (long)lba * RAW_SECTOR, SEEK_SET) != 0 || fread(raw, 1, RAW_SECTOR, f) != RAW_SECTOR) {
        return -1;
    }
    memcpy(out, raw + (raw[15] == 1 ? 16 : 24), 2048);
    return 0;
}

static uint32_t le32(const uint8_t *p) {
    return p[0] | p[1] << 8 | p[2] << 16 | (uint32_t)p[3] << 24;
}

int discs_identify_by_contents(const char *bin_path) {
    FILE *f = fopen(bin_path, "rb");
    if (!f) {
        return DISC_UNKNOWN;
    }
    uint8_t sec[2048];
    int disc = DISC_UNKNOWN;
    if (read_user_sector(f, 16, sec) == 0 && memcmp(sec + 1, "CD001", 5) == 0) {
        uint32_t root_lba = le32(sec + 156 + 2), root_size = le32(sec + 156 + 10);
        for (uint32_t s = 0; s < (root_size + 2047) / 2048 && disc == DISC_UNKNOWN; s++) {
            if (read_user_sector(f, root_lba + s, sec) != 0) {
                break;
            }
            for (int pos = 0; pos < 2048 && sec[pos];) {
                const uint8_t *rec = sec + pos;
                int name_len = rec[32];
                if (name_len >= 10 && memcmp(rec + 33, "LAIN_1.INF", 10) == 0) {
                    disc = DISC_1;
                } else if (name_len >= 10 && memcmp(rec + 33, "LAIN_2.INF", 10) == 0) {
                    disc = DISC_2;
                }
                pos += rec[0];
            }
        }
    }
    fclose(f);
    return disc;
}

/* Import: copy + hash in one pass on a worker thread. */

typedef struct {
    ImportJob *job;
    char src[1024];
    char data_dir[1024];
} ImportArgs;

static int import_thread(void *p) {
    ImportArgs *a = p;
    ImportJob *job = a->job;
    char bin[1024], tmp[1100], dst[1100];

    if (discs_resolve_track(a->src, bin, sizeof bin, job->message, sizeof job->message) != 0) {
        job->state = IMPORT_FAILED;
        free(a);
        return 0;
    }
    FILE *in = fopen(bin, "rb");
    if (!in) {
        snprintf(job->message, sizeof job->message, "Can't open %s", bin);
        job->state = IMPORT_FAILED;
        free(a);
        return 0;
    }
    fseek(in, 0, SEEK_END);
    long total = ftell(in);
    fseek(in, 0, SEEK_SET);
    if (total <= 0 || total % RAW_SECTOR != 0) {
        snprintf(job->message, sizeof job->message,
                 "Not a raw (2352-byte sector) image. Dump in Redump format (BIN/CUE).");
        fclose(in);
        job->state = IMPORT_FAILED;
        free(a);
        return 0;
    }
    snprintf(tmp, sizeof tmp, "%s/import.tmp", a->data_dir);
    FILE *out = fopen(tmp, "wb");
    if (!out) {
        snprintf(job->message, sizeof job->message, "Can't write to %s", a->data_dir);
        fclose(in);
        job->state = IMPORT_FAILED;
        free(a);
        return 0;
    }

    Sha1 sha;
    sha1_init(&sha);
    uint8_t *buf = malloc(COPY_CHUNK);
    long done = 0;
    size_t n;
    while ((n = fread(buf, 1, COPY_CHUNK, in)) > 0) {
        sha1_update(&sha, buf, n);
        if (fwrite(buf, 1, n, out) != n) {
            snprintf(job->message, sizeof job->message, "Disk full while copying");
            break;
        }
        done += (long)n;
        job->progress = (float)done / (float)total;
    }
    free(buf);
    fclose(in);
    fclose(out);
    if (done != total) {
        remove(tmp);
        if (!job->message[0]) {
            snprintf(job->message, sizeof job->message, "Read error while copying");
        }
        job->state = IMPORT_FAILED;
        free(a);
        return 0;
    }

    uint8_t digest[20];
    sha1_final(&sha, digest);
    sha1_hex(digest, job->sha1);
    job->disc = DISC_UNKNOWN;
    job->verified = 0;
    for (size_t i = 0; i < sizeof KNOWN_DUMPS / sizeof KNOWN_DUMPS[0]; i++) {
        if (strcmp(job->sha1, KNOWN_DUMPS[i].sha1) == 0) {
            job->disc = KNOWN_DUMPS[i].disc;
            job->verified = 1;
        }
    }
    if (job->disc == DISC_UNKNOWN) {
        job->disc = discs_identify_by_contents(tmp);
    }
    if (job->disc == DISC_UNKNOWN) {
        remove(tmp);
        snprintf(job->message, sizeof job->message, "This isn't a Serial Experiments Lain disc");
        job->state = IMPORT_FAILED;
        free(a);
        return 0;
    }
    snprintf(dst, sizeof dst, "%sdisc%d.bin", a->data_dir, job->disc);
    remove(dst);
    if (rename(tmp, dst) != 0) {
        remove(tmp);
        snprintf(job->message, sizeof job->message, "Can't save %s", dst);
        job->state = IMPORT_FAILED;
        free(a);
        return 0;
    }
    snprintf(job->message, sizeof job->message, job->verified
                 ? "Disc %d imported (verified retail dump)"
                 : "Disc %d imported (unverified dump: it may be modified or a different release)",
             job->disc);
    job->progress = 1.0f;
    job->state = IMPORT_DONE;
    free(a);
    return 0;
}

void discs_import_start(ImportJob *job, const char *path, const char *data_dir) {
    memset(job, 0, sizeof *job);
    job->state = IMPORT_RUNNING;
    ImportArgs *a = calloc(1, sizeof *a);
    a->job = job;
    snprintf(a->src, sizeof a->src, "%s", path);
    snprintf(a->data_dir, sizeof a->data_dir, "%s", data_dir);
    SDL_Thread *t = SDL_CreateThread(import_thread, "disc import", a);
    SDL_DetachThread(t);
}

int discs_imported(const char *data_dir, int disc, char *out, int cap) {
    snprintf(out, cap, "%sdisc%d.bin", data_dir, disc);
    FILE *f = fopen(out, "rb");
    if (!f) {
        return 0;
    }
    fclose(f);
    return 1;
}
