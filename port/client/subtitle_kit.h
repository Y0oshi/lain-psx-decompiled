/* Subtitle pack template kit: lists every voice track and movie on the player's
 * discs and writes blank, correctly named SRT files for translators. Only names
 * and durations are written, never game content. */
#pragma once

#ifdef __cplusplus
extern "C" {
#endif

/* Creates <data dir>/lang/<code>/ with pack.txt, subtitles/index.txt and one
 * template .srt per XA channel and STR movie found on the given disc images.
 * Existing .srt files are kept. Returns the number of tracks, or -1. */
int subkit_write_templates(const char *const *disc_bins, int disc_count, const char *pack_dir,
                           const char *language_name);

#ifdef __cplusplus
}
#endif
