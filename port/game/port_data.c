/* Initial values of the host-typed globals (game/PORT_TYPES.md).
 *
 * These globals hold pointers, so they can't live in the byte arena with their
 * PS1 layout. After the arena is loaded from the player's EXE, their original
 * 32-bit words are read from it and turned into host pointers here.
 */
#include <stddef.h>
#include <string.h>

#include "common.h"
#include "psx_arena.h"

/* Declared with their real types in the game files; only the pointer slots matter here. */
extern void *g_node_text_strings[417]; /* node/text strings */
extern void *g_node_label_strings[51];  /* option strings */
extern void *g_site_node_tmds[16];  /* grid TMDs */
extern void *g_site_node_tims[16];  /* grid TIMs */
extern void *g_site_open_shard_tmds[4];   /* TMDs */
extern void *g_ring_burst_tmds[32];  /* TMDs */
extern void *g_gate_model_tmds[5];   /* g_gate_model_tmds (4 words) + D_800955C0 (1 word) */
extern void *g_sjis_digit_strings[10];  /* digit strings */
extern void *g_movie_model_tmds[2];   /* TMDs */
extern void *g_movie_model_tmds_hi[2];   /* TMDs */
extern void *g_lz_magic;      /* "napk" magic */

/* GsRVIEW2 camera presets: 7 words of data, then a `super` pointer that is 0. */
extern u8 g_view_preset_map[], g_view_preset_menu[], g_view_preset_intro[];

static const struct {
    u32 psx_addr;
    void **host;
    int count;
} POINTER_TABLES[] = {
    {0x80073848, g_node_text_strings, 417}, {0x80073ECC, g_node_label_strings, 51}, {0x800950C8, g_site_node_tmds, 16},
    {0x80095108, g_site_node_tims, 16},  {0x80095148, g_site_open_shard_tmds, 4},  {0x80095158, g_ring_burst_tmds, 32},
    {0x800955B0, g_gate_model_tmds, 5},   {0x8009D468, g_sjis_digit_strings, 10}, {0x800A66B4, g_movie_model_tmds, 2},
    {0x800A66BC, g_movie_model_tmds_hi, 2},   {0x800A6764, &g_lz_magic, 1},
};

static void *relocate(u32 word) {
    if (word == 0) {
        return NULL;
    }
    if (word >= PSX_RAM_BASE && word < PSX_RAM_BASE + PSX_RAM_SIZE) {
        return PSX_PTR(word);
    }
    return NULL; /* not a RAM address: no pointer this table could hold */
}

/* Returns the number of words that weren't valid RAM addresses (should be 0). */
int port_data_relocate(void) {
    int bad = 0;
    for (size_t t = 0; t < sizeof POINTER_TABLES / sizeof POINTER_TABLES[0]; t++) {
        const u32 *words = PSX_PTR(POINTER_TABLES[t].psx_addr);
        for (int i = 0; i < POINTER_TABLES[t].count; i++) {
            void *p = relocate(words[i]);
            bad += words[i] != 0 && p == NULL;
            POINTER_TABLES[t].host[i] = p;
        }
    }
    /* Copy the 7 data words of each camera preset; `super` stays NULL. */
    memcpy(g_view_preset_map, PSX_PTR(0x800951D8), 7 * 4);
    memcpy(g_view_preset_menu, PSX_PTR(0x800951F8), 7 * 4);
    memcpy(g_view_preset_intro, PSX_PTR(0x80095238), 7 * 4);
    return bad;
}
