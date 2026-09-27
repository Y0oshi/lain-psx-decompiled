/* Host versions of Lain's own movie/picture bitstream decoder.
 *
 * On the PS1 these are hand-written routines linked into the EXE's library
 * area (vlc_build_table builds the code table, vlc_decode_frame expands one frame
 * into an MDEC run-level command stream). Here they wrap the reference decoder
 * in port_vlc_core.c, which is verified on every frame of several movies.
 */
#include "common.h"
#include "lain_vlc.h"

/* Quantiser scales for the frame being decoded, set by the game beforehand. */
extern s16 g_vlc_qscale_chroma; /* chroma */
extern s16 g_vlc_qscale_luma; /* luma */

/* Builds the decode table from the game's 111-entry run/level table
 * (loaded with the EXE from the player's disc). */
void vlc_build_table(u8 *table) {
    lain_vlc_init((const unsigned short *)table);
}

/* `bs` points one word into the frame (at the halfword count); the word before
 * it is the frame header. The reference decoder wants the whole frame with the
 * scales in its header, so rebuild that word around the call. */
s32 vlc_decode_frame(u32 *bs, u32 *buf) {
    u32 saved = bs[-1];
    s32 count = (s32)bs[0];
    int words;

    if (count <= 0) {
        return -1;
    }
    bs[-1] = (u32)(u8)g_vlc_qscale_luma | (u32)(u8)g_vlc_qscale_chroma << 8 | 0x3800u << 16;
    /* Output: one header word plus the run-level halfwords, padded to 64. */
    words = ((count + 63) & ~63) / 2 + 2;
    /* Each code is at most 22 bits, so 3 bytes per halfword bounds the input. */
    words = lain_vlc_decode(bs - 1, 8 + count * 3, buf, words);
    bs[-1] = saved;
    return words < 0 ? -1 : 0;
}
