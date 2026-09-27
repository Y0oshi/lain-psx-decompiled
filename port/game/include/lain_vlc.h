/* Reference decoder for Lain's movie bitstream (see lain_vlc.c). */
#ifndef LAIN_VLC_H
#define LAIN_VLC_H

/* Builds and self-checks the code table. `table` is the game's 111-entry
 * run/level table (g_movie_vlc_table: halfwords run << 10 | level). Returns 1 if OK. */
int lain_vlc_init(const unsigned short table[111]);

/* Decodes one frame (as returned by StGetNext) into an MDEC command stream
 * for DecDCTin. Returns the number of words written, or -1 on error. */
int lain_vlc_decode(const unsigned int *frame, int frame_bytes, unsigned int *out, int out_words);

#endif
