#include "subtitles.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <dirent.h>
#include <sys/stat.h>
#endif

typedef struct {
    long start_ms, end_ms;
    char *text;
    int order; /* position in the file: keeps cues that start together in file order */
} Cue;

typedef struct {
    char *name;
    Cue *cues;
    int count;
} Track;

static Track *s_tracks;
static int s_track_count;

/* Copies n bytes, turning typographic quotes and dashes (U+2018/2019/201C/201D,
 * U+2013/2014, U+2026) into ASCII: the fallback UI font has no glyphs for them. */
static char *dup_range(const char *s, size_t n) {
    char *d = malloc(n + 1), *o = d;
    for (size_t i = 0; i < n; i++) {
        const unsigned char *u = (const unsigned char *)s + i;
        if (i + 2 < n && u[0] == 0xE2 && u[1] == 0x80) {
            const char *rep = NULL;
            switch (u[2]) {
                case 0x98: case 0x99: rep = "'"; break;
                case 0x9C: case 0x9D: rep = "\""; break;
                case 0x93: case 0x94: rep = "-"; break;
                case 0xA6: rep = "..."; break;
            }
            if (rep) {
                size_t len = strlen(rep);
                memcpy(o, rep, len); /* never longer than the 3 bytes it replaces */
                o += len;
                i += 2;
                continue;
            }
        }
        *o++ = s[i];
    }
    *o = 0;
    return d;
}

/* "hh:mm:ss,mmm" (',' or '.') -> milliseconds; returns -1 on a malformed stamp. */
static long parse_stamp(const char *p, const char **end) {
    int h, m, s, ms;
    char sep;
    int n = 0;
    if (sscanf(p, "%d:%d:%d%c%d%n", &h, &m, &s, &sep, &ms, &n) != 5 || (sep != ',' && sep != '.')) {
        return -1;
    }
    *end = p + n;
    return ((h * 60L + m) * 60L + s) * 1000L + ms;
}

static int cue_cmp(const void *a, const void *b) {
    const Cue *x = a, *y = b;
    if (x->start_ms != y->start_ms) {
        return x->start_ms < y->start_ms ? -1 : 1;
    }
    return x->order - y->order;
}

int subtitles_add_srt(const char *track, const char *srt) {
    Cue *cues = NULL;
    int count = 0, cap = 0;
    const char *p = srt;
    if ((unsigned char)p[0] == 0xEF && (unsigned char)p[1] == 0xBB && (unsigned char)p[2] == 0xBF) {
        p += 3; /* UTF-8 BOM */
    }
    while (*p) {
        /* Find the next timing line "start --> end". */
        const char *arrow = strstr(p, "-->");
        if (!arrow) {
            break;
        }
        const char *line = arrow;
        while (line > p && line[-1] != '\n') {
            line--;
        }
        const char *after;
        long start = parse_stamp(line, &after);
        const char *q = arrow + 3;
        while (*q == ' ') q++;
        long end = start >= 0 ? parse_stamp(q, &after) : -1;
        const char *eol = strchr(arrow, '\n');
        if (!eol) {
            break;
        }
        /* Text runs until a blank line. */
        const char *text = eol + 1, *t = text;
        while (*t) {
            const char *nl = strchr(t, '\n');
            const char *next = nl ? nl + 1 : t + strlen(t);
            int blank = 1;
            for (const char *c = t; c < next; c++) {
                if (!isspace((unsigned char)*c)) blank = 0;
            }
            if (blank) {
                break;
            }
            t = next;
        }
        /* Template placeholders ("(... replace with timed lines)") are skipped, so
         * a partly translated pack shows nothing for untranslated tracks. */
        int placeholder = (size_t)(t - text) < 512 && strstr(text, "replace with timed lines)") != NULL &&
                          strstr(text, "replace with timed lines)") < t;
        if (start >= 0 && end > start && !placeholder) {
            size_t n = (size_t)(t - text);
            while (n && (text[n - 1] == '\n' || text[n - 1] == '\r')) n--;
            if (count == cap) {
                cap = cap ? cap * 2 : 16;
                cues = realloc(cues, cap * sizeof *cues);
            }
            char *s = dup_range(text, n);
            /* Normalise CRLF to LF. */
            char *w = s;
            for (char *r = s; *r; r++) {
                if (*r != '\r') *w++ = *r;
            }
            *w = 0;
            cues[count] = (Cue){start, end, s, count};
            count++;
        }
        p = *t ? t : t;
        if (p == text) {
            p = eol + 1;
        }
    }
    if (count == 0) {
        free(cues);
        return 0;
    }
    qsort(cues, count, sizeof *cues, cue_cmp);
    s_tracks = realloc(s_tracks, (s_track_count + 1) * sizeof *s_tracks);
    s_tracks[s_track_count++] = (Track){dup_range(track, strlen(track)), cues, count};
    return count;
}

static char *read_file(const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) {
        return NULL;
    }
    fseek(f, 0, SEEK_END);
    long n = ftell(f);
    fseek(f, 0, SEEK_SET);
    char *buf = malloc(n + 1);
    if (fread(buf, 1, n, f) != (size_t)n) {
        free(buf);
        fclose(f);
        return NULL;
    }
    buf[n] = 0;
    fclose(f);
    return buf;
}

/* "h:mm:ss.cc" (ASS) -> milliseconds; -1 if malformed. */
static long parse_ass_time(const char *p) {
    int h, m, s, cs, n = 0;
    if (sscanf(p, " %d:%d:%d.%d%n", &h, &m, &s, &cs, &n) != 4) {
        return -1;
    }
    /* Two digits are centiseconds; tolerate other precisions. */
    const char *dot = strchr(p, '.');
    int digits = 0;
    while (dot && isdigit((unsigned char)dot[1 + digits])) digits++;
    long frac = digits == 1 ? cs * 100L : digits == 2 ? cs * 10L : digits >= 3 ? cs : 0;
    return ((h * 60L + m) * 60L + s) * 1000L + frac;
}

/* Advanced SubStation Alpha (.ass/.ssa, as made with Aegisub): the Dialogue lines of
 * [Events], in the column order given by its Format line. Style tags ({\...}) are
 * dropped, \N and \n become line breaks. Converted to SRT text and parsed as such. */
int subtitles_add_ass(const char *track, const char *ass) {
    int ncols = 0, col_start = -1, col_end = -1, col_text = -1;
    size_t cap = strlen(ass) + 64, len = 0;
    char *srt = malloc(cap * 2 + 64);
    int cue = 0;
    const char *line = ass;
    srt[0] = 0;
    while (*line) {
        const char *eol = strchr(line, '\n');
        size_t n = eol ? (size_t)(eol - line) : strlen(line);
        if (n >= 7 && strncmp(line, "Format:", 7) == 0 && col_text < 0) {
            /* e.g. "Layer, Start, End, Style, Name, MarginL, MarginR, MarginV, Effect, Text" */
            const char *f = line + 7;
            for (int c = 0; f < line + n; c++) {
                while (*f == ' ') f++;
                const char *e = f;
                while (e < line + n && *e != ',') e++;
                size_t w = (size_t)(e - f);
                while (w && (f[w - 1] == ' ' || f[w - 1] == '\r')) w--;
                if (w == 5 && strncmp(f, "Start", 5) == 0) col_start = c;
                if (w == 3 && strncmp(f, "End", 3) == 0) col_end = c;
                if (w == 4 && strncmp(f, "Text", 4) == 0) col_text = c;
                ncols = c + 1;
                f = e < line + n ? e + 1 : e;
            }
        } else if (n >= 9 && strncmp(line, "Dialogue:", 9) == 0 && col_text == ncols - 1 && col_start >= 0 &&
                   col_end >= 0) {
            /* Split into ncols fields; the last (Text) keeps its commas. */
            const char *fields[32];
            const char *f = line + 9;
            int c = 0;
            while (c < ncols - 1 && c < 31) {
                fields[c++] = f;
                const char *comma = memchr(f, ',', (size_t)(line + n - f));
                if (!comma) break;
                f = comma + 1;
            }
            if (c == ncols - 1) {
                long a = parse_ass_time(fields[col_start]), b = parse_ass_time(fields[col_end]);
                if (a >= 0 && b > a) {
                    /* text: drop {...} tags, \N \n -> newline, \h -> space */
                    char text[2048];
                    size_t t = 0;
                    for (const char *q = f; q < line + n && t + 2 < sizeof text; q++) {
                        if (*q == '{') {
                            const char *close = memchr(q, '}', (size_t)(line + n - q));
                            if (close) { q = close; continue; }
                        }
                        if (*q == '\\' && q + 1 < line + n && (q[1] == 'N' || q[1] == 'n')) {
                            /* no leading or doubled line breaks: a blank line ends an SRT cue */
                            if (t && text[t - 1] != '\n') text[t++] = '\n';
                            q++;
                            continue;
                        }
                        if (*q == '\\' && q + 1 < line + n && q[1] == 'h') { text[t++] = ' '; q++; continue; }
                        if (*q == '\r') continue;
                        text[t++] = *q;
                    }
                    while (t && (text[t - 1] == ' ' || text[t - 1] == '\n')) t--;
                    text[t] = 0;
                    if (t) {
                        char stamp[64];
                        snprintf(stamp, sizeof stamp, "%d\n%02ld:%02ld:%02ld,%03ld --> %02ld:%02ld:%02ld,%03ld\n", ++cue,
                                 a / 3600000, a / 60000 % 60, a / 1000 % 60, a % 1000,
                                 b / 3600000, b / 60000 % 60, b / 1000 % 60, b % 1000);
                        size_t need = len + strlen(stamp) + t + 4;
                        if (need > cap * 2 + 60) {
                            cap = need;
                            srt = realloc(srt, cap * 2 + 64);
                        }
                        len += (size_t)sprintf(srt + len, "%s%s\n\n", stamp, text);
                    }
                }
            }
        }
        line += n;
        if (*line == '\n') line++;
    }
    int count = subtitles_add_srt(track, srt);
    free(srt);
    return count;
}

static int ext_is(const char *ext, const char *want) {
    for (; *ext && *want; ext++, want++) {
        if (tolower((unsigned char)*ext) != *want) return 0;
    }
    return *ext == 0 && *want == 0;
}

/* Adds one subtitle file; the track key is its name without the extension. */
static int add_file(const char *path, const char *file_name) {
    char key[256];
    const char *dot = strrchr(file_name, '.');
    if (!dot) {
        return 0;
    }
    int ass = ext_is(dot, ".ass") || ext_is(dot, ".ssa");
    if (!ass && !ext_is(dot, ".srt")) {
        return 0;
    }
    snprintf(key, sizeof key, "%.*s", (int)(dot - file_name), file_name);
    if (subtitles_has_track(key)) {
        return 0; /* first file wins */
    }
    char *text = read_file(path);
    if (!text) {
        return 0;
    }
    const char *body = text;
    if ((unsigned char)body[0] == 0xEF && (unsigned char)body[1] == 0xBB && (unsigned char)body[2] == 0xBF) {
        body += 3;
    }
    int n = ass ? subtitles_add_ass(key, body) : subtitles_add_srt(key, body);
    free(text);
    return n > 0;
}

/* Loads every .srt/.ass file under dir (any depth). */
static int scan_folder(const char *dir, int depth) {
    int tracks = 0;
    char path[1024];
    if (depth > 4) {
        return 0;
    }
#ifdef _WIN32
    WIN32_FIND_DATAA fd;
    snprintf(path, sizeof path, "%s\\*", dir);
    HANDLE h = FindFirstFileA(path, &fd);
    if (h == INVALID_HANDLE_VALUE) {
        return 0;
    }
    do {
        if (fd.cFileName[0] == '.') continue;
        snprintf(path, sizeof path, "%s\\%s", dir, fd.cFileName);
        if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            tracks += scan_folder(path, depth + 1);
        } else {
            tracks += add_file(path, fd.cFileName);
        }
    } while (FindNextFileA(h, &fd));
    FindClose(h);
#else
    DIR *d = opendir(dir);
    if (!d) {
        return 0;
    }
    struct dirent *e;
    while ((e = readdir(d))) {
        if (e->d_name[0] == '.') continue;
        struct stat st;
        snprintf(path, sizeof path, "%s/%s", dir, e->d_name);
        if (stat(path, &st) != 0) continue;
        if (S_ISDIR(st.st_mode)) {
            tracks += scan_folder(path, depth + 1);
        } else {
            tracks += add_file(path, e->d_name);
        }
    }
    closedir(d);
#endif
    return tracks;
}

int subtitles_load_pack(const char *pack_dir) {
    char path[1024], name[256];
    subtitles_unload();
    snprintf(path, sizeof path, "%s/subtitles/index.txt", pack_dir);
    FILE *idx = fopen(path, "r");
    if (!idx) {
        /* No index: a plain folder of subtitle files (e.g. timed .ass files named
         * by node, Cou001.ass, as fan subtitle projects publish them). */
        return scan_folder(pack_dir, 0);
    }
    int tracks = 0;
    while (fgets(name, sizeof name, idx)) {
        name[strcspn(name, "\r\n")] = 0;
        if (!name[0] || name[0] == '#') {
            continue;
        }
        snprintf(path, sizeof path, "%s/subtitles/%s", pack_dir, name);
        char *srt = read_file(path);
        if (!srt) {
            continue;
        }
        /* Track key is the file name without ".srt". */
        size_t n = strlen(name);
        if (n > 4 && strcmp(name + n - 4, ".srt") == 0) {
            name[n - 4] = 0;
        }
        tracks += subtitles_add_srt(name, srt) > 0;
        free(srt);
    }
    fclose(idx);
    return tracks;
}

void subtitles_unload(void) {
    for (int i = 0; i < s_track_count; i++) {
        for (int c = 0; c < s_tracks[i].count; c++) {
            free(s_tracks[i].cues[c].text);
        }
        free(s_tracks[i].cues);
        free(s_tracks[i].name);
    }
    free(s_tracks);
    s_tracks = NULL;
    s_track_count = 0;
}

int subtitles_has_track(const char *track) {
    for (int i = 0; i < s_track_count; i++) {
        if (strcmp(s_tracks[i].name, track) == 0) {
            return 1;
        }
    }
    return 0;
}

const char *subtitles_lookup(const char *track, long ms) {
    static char joined[2048];
    for (int i = 0; i < s_track_count; i++) {
        if (strcmp(s_tracks[i].name, track) != 0) {
            continue;
        }
        /* Binary search for the last cue starting at or before ms. */
        const Cue *cues = s_tracks[i].cues;
        int lo = 0, hi = s_tracks[i].count - 1, best = -1;
        while (lo <= hi) {
            int mid = (lo + hi) / 2;
            if (cues[mid].start_ms <= ms) {
                best = mid;
                lo = mid + 1;
            } else {
                hi = mid - 1;
            }
        }
        /* Every cue that has started and not ended (lines can overlap or start
         * together), oldest first, one per line. Looks back a bounded distance. */
        int first = best, found = 0;
        for (int c = best; c >= 0 && c > best - 32; c--) {
            if (ms < cues[c].end_ms) {
                first = c;
                found++;
            }
        }
        if (!found) {
            return NULL;
        }
        size_t len = 0;
        joined[0] = 0;
        for (int c = first; c <= best; c++) {
            if (ms >= cues[c].end_ms) continue;
            int w = snprintf(joined + len, sizeof joined - len, "%s%s", len ? "\n" : "", cues[c].text);
            if (w < 0 || (size_t)w >= sizeof joined - len) break;
            len += (size_t)w;
        }
        return joined;
    }
    return NULL;
}
