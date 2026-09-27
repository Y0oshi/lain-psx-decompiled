/* Dub packs: replacement voice audio for XA voice tracks and movies.
 *
 * A pack is a folder <data dir>/dub/<code>/ holding Ogg Vorbis files named like
 * the subtitle tracks: LAIN01.XA.ch00.ogg, F001.STR.ogg, ... When a dub pack is
 * active, the client plays the file for a track in place of the disc's audio.
 * Any sample rate and mono/stereo work; audio is delivered as 44.1 kHz stereo.
 */
#pragma once
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct DubStream DubStream;

/* Opens <pack_dir>/<track>.ogg; NULL if the pack has no dub for this track. */
DubStream *dub_open(const char *pack_dir, const char *track);

/* Opens an Ogg Vorbis file by path; NULL if it can't be read. */
DubStream *dub_open_file(const char *path);

/* Fills up to `frames` stereo frames of 44.1 kHz s16 audio; returns frames
 * written (fewer at the end of the track, 0 once finished). */
int dub_read(DubStream *d, int16_t *stereo_out, int frames);

/* Jumps to `ms` into the track (e.g. when a voice track resumes mid-way). */
void dub_seek_ms(DubStream *d, long ms);

/* Seconds of audio in the track (for sync and the template kit). */
double dub_length_seconds(const DubStream *d);

void dub_close(DubStream *d);

#ifdef __cplusplus
}
#endif
