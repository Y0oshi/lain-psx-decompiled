/* Subtitle packs: timed text for the game's voice tracks and movies.
 *
 * A pack is a folder <data dir>/lang/<code>/ with one SRT file per track in
 * subtitles/, named after the disc file and XA channel:
 *     lang/en/subtitles/LAIN01.XA.ch00.srt     (XA voice file, channel 0)
 *     lang/en/subtitles/F001.STR.srt           (movie)
 * plus an optional pack.txt ("name = English", "author = ..."). SRT is used so
 * translators can time lines with ordinary subtitle editors.
 */
#pragma once

#ifdef __cplusplus
extern "C" {
#endif

/* Loads every .srt in <pack_dir>/subtitles/ listed in <pack_dir>/subtitles/index.txt
 * (one file name per line; generated with the template kit). Returns the number
 * of tracks loaded, or -1 if the pack folder doesn't exist. */
int subtitles_load_pack(const char *pack_dir);
void subtitles_unload(void);

/* Text to show for `track` (e.g. "LAIN01.XA.ch00") at `ms` into the track,
 * or NULL when no cue is active. Lines are separated by '\n'. */
const char *subtitles_lookup(const char *track, long ms);

/* Whether the loaded pack has subtitles for `track` (a disc track or a node name). */
int subtitles_has_track(const char *track);

/* Parses one Advanced SubStation Alpha (.ass) buffer under `track`. */
int subtitles_add_ass(const char *track, const char *ass_text);

/* Parses one SRT buffer into the loaded set under `track`; exposed for tests. */
int subtitles_add_srt(const char *track, const char *srt_text);

#ifdef __cplusplus
}
#endif
