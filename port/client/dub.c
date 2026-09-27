#include "dub.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define STB_VORBIS_HEADER_ONLY
#include "stb_vorbis.c"

#define OUT_RATE 44100
#define CHUNK 1024

struct DubStream {
    stb_vorbis *vorbis;
    int channels, rate;
    double length;
    float buf[CHUNK * 2];   /* decoded source frames, stereo interleaved */
    int buf_frames, buf_pos;
    float prev[2];     /* last source frame of the previous chunk */
    double frac;       /* fractional source position, 0..1 between prev and next */
    int eof;
};

DubStream *dub_open(const char *pack_dir, const char *track) {
    char path[1024];
    snprintf(path, sizeof path, "%s/%s.ogg", pack_dir, track);
    return dub_open_file(path);
}

DubStream *dub_open_file(const char *path) {
    int err = 0;
    stb_vorbis *v = stb_vorbis_open_filename(path, &err, NULL);
    if (!v) {
        return NULL;
    }
    DubStream *d = calloc(1, sizeof *d);
    stb_vorbis_info info = stb_vorbis_get_info(v);
    d->vorbis = v;
    d->channels = info.channels;
    d->rate = (int)info.sample_rate;
    d->length = stb_vorbis_stream_length_in_seconds(v);
    return d;
}

static int next_source_frame(DubStream *d, float fr[2]) {
    while (d->buf_pos >= d->buf_frames) {
        if (d->eof) {
            return 0;
        }
        int n = stb_vorbis_get_samples_float_interleaved(d->vorbis, 2, d->buf, CHUNK * 2);
        if (n <= 0) {
            d->eof = 1;
            return 0;
        }
        /* Asking for 2 interleaved channels makes stb duplicate mono sources. */
        d->buf_frames = n;
        d->buf_pos = 0;
    }
    fr[0] = d->buf[d->buf_pos * 2];
    fr[1] = d->buf[d->buf_pos * 2 + 1];
    d->buf_pos++;
    return 1;
}

static int16_t to_s16(float x) {
    x *= 32767.0f;
    if (x > 32767.0f) return 32767;
    if (x < -32768.0f) return -32768;
    return (int16_t)x;
}

int dub_read(DubStream *d, int16_t *out, int frames) {
    const double step = (double)d->rate / OUT_RATE;
    int written = 0;
    float next[2];
    if (d->rate == OUT_RATE) {
        while (written < frames && next_source_frame(d, next)) {
            out[written * 2] = to_s16(next[0]);
            out[written * 2 + 1] = to_s16(next[1]);
            written++;
        }
        return written;
    }
    /* Linear resampling between prev and the following source frame. */
    while (written < frames) {
        while (d->frac >= 1.0) {
            if (!next_source_frame(d, next)) {
                return written;
            }
            d->prev[0] = next[0];
            d->prev[1] = next[1];
            d->frac -= 1.0;
        }
        /* Peek the following frame without consuming it. */
        float peek[2];
        if (d->buf_pos < d->buf_frames) {
            peek[0] = d->buf[d->buf_pos * 2];
            peek[1] = d->buf[d->buf_pos * 2 + 1];
        } else if (next_source_frame(d, peek)) {
            d->buf_pos--; /* un-consume: the refilled chunk starts with it */
        } else {
            peek[0] = d->prev[0];
            peek[1] = d->prev[1];
        }
        float t = (float)d->frac;
        out[written * 2] = to_s16(d->prev[0] + (peek[0] - d->prev[0]) * t);
        out[written * 2 + 1] = to_s16(d->prev[1] + (peek[1] - d->prev[1]) * t);
        written++;
        d->frac += step;
    }
    return written;
}

void dub_seek_ms(DubStream *d, long ms) {
    unsigned int sample = ms > 0 ? (unsigned int)((double)ms * d->rate / 1000.0) : 0;
    stb_vorbis_seek(d->vorbis, sample);
    d->buf_frames = d->buf_pos = 0;
    d->frac = 0;
    d->prev[0] = d->prev[1] = 0;
    d->eof = 0;
}

double dub_length_seconds(const DubStream *d) {
    return d->length;
}

void dub_close(DubStream *d) {
    if (d) {
        stb_vorbis_close(d->vorbis);
        free(d);
    }
}
