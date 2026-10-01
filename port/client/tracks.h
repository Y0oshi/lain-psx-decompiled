/* Which voice track or movie is playing, for subtitles and dubs.
 *
 * Track keys match the subtitle/dub pack file names: "LAIN01.XA.ch05" for an
 * XA voice channel, "F001.STR" for a movie.
 */
#pragma once

#ifdef __cplusplus
extern "C" {
#endif

/* Indexes the XA and movie files on both disc images (either may be NULL). */
void tracks_init(const char *disc1_bin, const char *disc2_bin);

/* A voice or movie file a mod moved to another place on the disc (client/mods.cpp):
 * where it is found from now on. May be called before or after tracks_init. */
void tracks_move(int disc, const char *name, unsigned lba, unsigned sectors);

/* Current track for the inserted disc (0 = disc 1, 1 = disc 2). Returns 1 and
 * fills key (at least 32 bytes) and ms if a voice track or movie is playing. */
int tracks_current(int disc, char *key, int key_cap, long *ms);

#ifdef __cplusplus
}
#endif
