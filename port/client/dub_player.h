/* Plays dub-pack audio in place of the disc's voice audio for the current track. */
#pragma once

#ifdef __cplusplus
extern "C" {
#endif

/* Sets the dub pack folder (NULL = original audio only). */
void dub_player_set_pack(const char *pack_dir);

/* Called once per frame with the playing track (NULL if none) and its position. */
void dub_player_update(const char *track, long ms);

/* Whether the pack has a dub named `name` (a disc track like LAIN01.XA.ch00, or a
 * node like Cou001), anywhere in its folder. */
int dub_player_has(const char *name);

#ifdef __cplusplus
}
#endif
