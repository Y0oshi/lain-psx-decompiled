#include "dub_player.h"

#include <SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <dirent.h>
#include <sys/stat.h>
#endif

#include "dub.h"
#include "lain_xa.h"

static char s_pack[1024];
static char s_track[64];
static DubStream *s_stream;
static SDL_mutex *s_mu;
static long long s_pulled; /* frames supplied for the current track (debug log) */

/* Every .ogg under the pack folder, any layout: name without extension -> path. */
typedef struct {
    char name[64];
    char *path;
} DubFile;
static DubFile *s_files;
static int s_file_count;

static void add_file(const char *path, const char *file_name) {
    size_t n = strlen(file_name);
    if (n < 5 || SDL_strcasecmp(file_name + n - 4, ".ogg") != 0) {
        return;
    }
    DubFile *grown = realloc(s_files, (s_file_count + 1) * sizeof *s_files);
    if (!grown) {
        return;
    }
    s_files = grown;
    DubFile *f = &s_files[s_file_count++];
    snprintf(f->name, sizeof f->name, "%.*s", (int)(n - 4), file_name);
    f->path = SDL_strdup(path);
}

static void scan(const char *dir, int depth) {
    char path[1024];
    if (depth > 4) {
        return;
    }
#ifdef _WIN32
    WIN32_FIND_DATAA fd;
    snprintf(path, sizeof path, "%s\\*", dir);
    HANDLE h = FindFirstFileA(path, &fd);
    if (h == INVALID_HANDLE_VALUE) {
        return;
    }
    do {
        if (fd.cFileName[0] == '.') continue;
        snprintf(path, sizeof path, "%s\\%s", dir, fd.cFileName);
        if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) scan(path, depth + 1);
        else add_file(path, fd.cFileName);
    } while (FindNextFileA(h, &fd));
    FindClose(h);
#else
    DIR *d = opendir(dir);
    if (!d) {
        return;
    }
    struct dirent *e;
    while ((e = readdir(d))) {
        struct stat st;
        if (e->d_name[0] == '.') continue;
        snprintf(path, sizeof path, "%s/%s", dir, e->d_name);
        if (stat(path, &st) != 0) continue;
        if (S_ISDIR(st.st_mode)) scan(path, depth + 1);
        else add_file(path, e->d_name);
    }
    closedir(d);
#endif
}

static const char *file_for(const char *name) {
    for (int i = 0; i < s_file_count; i++) {
        if (strcmp(s_files[i].name, name) == 0) {
            return s_files[i].path;
        }
    }
    return NULL;
}

int dub_player_has(const char *name) {
    return name && file_for(name) != NULL;
}

/* Audio thread: fills the replacement samples from the open dub. */
static int pull(short *stereo, int frames, void *user) {
    (void)user;
    int got = 0;
    SDL_LockMutex(s_mu);
    if (s_stream) {
        got = dub_read(s_stream, stereo, frames);
        s_pulled += got;
    }
    SDL_UnlockMutex(s_mu);
    return got;
}

static void log_at_exit(void) {
    if (s_stream) {
        SDL_Log("dub: %s still playing at exit, %.1f s of dub audio supplied", s_track,
                (double)s_pulled / 44100.0);
    }
}

void dub_player_set_pack(const char *pack_dir) {
    if (!s_mu) {
        s_mu = SDL_CreateMutex();
        atexit(log_at_exit);
    }
    SDL_strlcpy(s_pack, pack_dir ? pack_dir : "", sizeof s_pack);
    for (int i = 0; i < s_file_count; i++) {
        SDL_free(s_files[i].path);
    }
    free(s_files);
    s_files = NULL;
    s_file_count = 0;
    if (s_pack[0]) {
        scan(s_pack, 0);
    }
}

void dub_player_update(const char *track, long ms) {
    if (!s_mu || !s_pack[0]) {
        return;
    }
    const char *want = track ? track : "";
    if (strcmp(want, s_track) == 0) {
        return;
    }
    if (s_stream) {
        SDL_Log("dub: %s ended after %.1f s of dub audio", s_track, (double)s_pulled / 44100.0);
    }
    SDL_strlcpy(s_track, want, sizeof s_track);
    const char *file = track ? file_for(track) : NULL;
    DubStream *next = file ? dub_open_file(file) : NULL;
    if (next && ms > 0) {
        dub_seek_ms(next, ms);
    }
    if (next) {
        SDL_Log("dub: playing %s.ogg from %ld ms", track, ms);
    }
    SDL_LockMutex(s_mu);
    DubStream *old = s_stream;
    s_stream = next;
    s_pulled = 0;
    SDL_UnlockMutex(s_mu);
    dub_close(old);
    /* Only replace the disc audio while this track has a dub. */
    LainXA_SetReplacement(next ? pull : NULL, NULL);
}
