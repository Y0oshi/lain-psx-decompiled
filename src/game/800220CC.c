#include "common.h"

/* Small globals that live outside the $gp-addressed small-data area. */
#define NO_GP __attribute__((section(".data")))

/* 16-byte load-request slot (file id + state), see g_lain_idle_anims / g_lain_action_anims. */
typedef struct {
    s32 fileId;
    s16 id;
    s16 unk6;
    s16 state;
    s16 sfx;
    s32 playCount;
} LoadSlot;

/* 0x28-byte entry of the g_node_table table (0x2CC entries). */
typedef struct {
    u8 unk0[0x10];
    s16 tags[3];
    u8 unk16[8];
    union {
        u16 raw; /* bits 0-2: column, 3-14: cell (1-based), 15: flag */
        u8 bytes[2];
    } pos;
    u32 unk20_0 : 16;
    u32 level : 4;
    u32 kind : 4;
    u32 visible : 1;
    u32 flag25 : 1;
    u32 flag26 : 1;
    u32 flag27 : 1;
    u32 depth : 4;
    u16 parent;
    u8 unk26[2];
} Node;

/* 12-byte grid cell; g_site_grid is a [rows][24] grid of these. */
typedef struct {
    s16 node;
    s16 icon;
    u8 unk4[8];
} GridCell;

extern Node g_node_table[];
extern GridCell g_site_grid[][24];
extern u16 g_node_kind_icons[];
extern s16 g_current_site NO_GP;
extern s16 g_sskn_level NO_GP;
extern s16 g_media_played_count NO_GP;
extern s16 g_site_cursor_col NO_GP;
extern s16 g_site_cursor_row NO_GP;
extern s32 g_site_rotation;
extern s32 g_site_action_busy;
extern s32 g_site_level;

s32 site_get_level_count(void);
s32 node_is_displayable(s32);
void snd_play_sfx(s16);
void site_node_model_reload(s32, s32);
s32 rand(void);
void *memset(void *, s32, s32);

extern LoadSlot g_lain_idle_anims[];
extern LoadSlot g_lain_action_anims[];
extern s32 D_800D6760[];
extern s16 g_lain_anim_slot_order[4]; /* slot order table {3, 1, 2, 4} */
extern u8 g_lain_anim_frame[];

extern s32 g_lain_idle_loading;
extern s32 g_lain_action_anim_active;
extern s32 g_lain_anim_playing;
extern s32 g_lain_action_anim_index;
extern s16 g_lain_anim_sfx;
extern s16 g_snd_anim_sfx_voice;
extern s16 g_snd_sfx_voice;
extern s16 g_snd_vab_id;
extern u8 D_80073700[];

void lain_anim_poll_loads(void);
void lain_anim_load_random_idle(s32);
void lain_anim_play_idle(void);
void lain_anim_play_sequence(s32);
s32 cd_poll_load(s32);
void heap_free(s32);
void anim_open_archive(s32);
void anim_decode_frame(u8 *);
s32 anim_next_frame(void);
void FntPrint();
s16 SsUtKeyOn(s16, s16, s16, s16, s16, s16, s16);
s16 SsUtKeyOff(s16, s16, s16, s16, s16);
s16 SsUtKeyOffV(s16);

extern s32 g_snd_file_table[];
extern u8 g_snd_vab_header[];
extern u8 g_bgm_seq_main_data[];
extern u8 g_bgm_seq_credits_data[];
extern u8 g_snd_seq_table[];
extern s16 g_bgm_slot;
extern s16 g_bgm_seq_main;
extern s16 g_bgm_seq_credits;

s32 cd_load_archive_entry(s32, s32, s32 *, void *);
void loading_anim_draw(s32);
s32 heap_alloc(s32);
void SsUtReverbOn(void);
void SsInit(void);
void SsSetMVol(s16, s16);
void SsSetReservedVoice(s8);
void SsSetTableSize(u8 *, s16, s16);
void SsSetTickMode(s32);
s16 SsVabOpenHead(u8 *, s16);
s16 SsVabTransBody(u8 *, s16);
s16 SsVabTransCompleted(s16);
void SsUtSetReverbType(s16);
void SsUtSetReverbDepth(s16, s16);
s16 SsSeqOpen(u8 *, s16);
void SsStart(void);

void func_800220CC(void) {
}

void func_800220D4(void) {
}

void func_800220DC(void) {
}

/* Refreshes load state; if the spare slot is idle, kicks off a load into the first free slot. */
void lain_anim_idle_update(s32 arg0) {
    s32 freeSlot;
    s32 i;

    lain_anim_poll_loads();
    if (arg0 == 0 && D_800D6760[0] < 0) {
        freeSlot = -1;
        for (i = 0; i < 1; i++) {
            if (g_lain_idle_anims[i].state == 0) {
                freeSlot = i;
                break;
            }
        }
        if (freeSlot != -1) {
            lain_anim_load_random_idle(freeSlot);
        }
    }
    lain_anim_play_idle();
}

/* Loads the first 3 slots of g_lain_action_anims; returns 1 when all are loaded. */
s32 lain_anim_ready_move(void) {
    s32 i;

    for (i = 0; i < 3; i++) {
        if (g_lain_action_anims[i].state != 2) {
            if (g_lain_action_anims[i].state == -1) {
                FntPrint("MEMORY TOO FEW\n");
                return 0;
            }
            if (cd_poll_load(g_lain_action_anims[i].id) == 0) {
                return 0;
            }
            g_lain_action_anims[i].state = 2;
        }
    }
    return 1;
}

void lain_anim_play_move(void) {
    lain_anim_play_sequence(3);
}

/* Loads slot 0 of g_lain_action_anims; returns 1 when it is loaded. */
s32 lain_anim_ready_level_move(void) {
    s32 i;

    for (i = 0; i < 1; i++) {
        if (g_lain_action_anims[i].state != 2) {
            if (g_lain_action_anims[i].state == -1) {
                FntPrint("MEMORY TOO FEW\n");
                return 0;
            }
            if (cd_poll_load(g_lain_action_anims[i].id) == 0) {
                return 0;
            }
            g_lain_action_anims[i].state = 2;
        }
    }
    return 1;
}

void lain_anim_play_level_move(void) {
    lain_anim_play_sequence(1);
}

/* Loads the first 4 slots of g_lain_action_anims; returns 1 when all are loaded. */
s32 lain_anim_ready_select(void) {
    s32 i;

    for (i = 0; i < 4; i++) {
        if (g_lain_action_anims[i].state != 2) {
            if (g_lain_action_anims[i].state == -1) {
                FntPrint("MEMORY TOO FEW\n");
                return 0;
            }
            if (cd_poll_load(g_lain_action_anims[i].id) == 0) {
                return 0;
            }
            g_lain_action_anims[i].state = 2;
        }
    }
    return 1;
}

void lain_anim_play_select(void) {
    lain_anim_play_sequence(4);
}

/* Loads slot 0 of g_lain_action_anims; returns 1 when it is loaded. */
s32 lain_anim_ready_save(void) {
    s32 i;

    for (i = 0; i < 1; i++) {
        if (g_lain_action_anims[i].state != 2) {
            if (g_lain_action_anims[i].state == -1) {
                FntPrint("MEMORY TOO FEW\n");
                return 0;
            }
            if (cd_poll_load(g_lain_action_anims[i].id) == 0) {
                return 0;
            }
            g_lain_action_anims[i].state = 2;
        }
    }
    return 1;
}

void lain_anim_play_save(void) {
    lain_anim_play_sequence(1);
}

/* Loads slot 0 of g_lain_action_anims; returns 1 when it is loaded. */
s32 lain_anim_ready_site_enter(void) {
    s32 i;

    for (i = 0; i < 1; i++) {
        if (g_lain_action_anims[i].state != 2) {
            if (g_lain_action_anims[i].state == -1) {
                FntPrint("MEMORY TOO FEW\n");
                return 0;
            }
            if (cd_poll_load(g_lain_action_anims[i].id) == 0) {
                return 0;
            }
            g_lain_action_anims[i].state = 2;
        }
    }
    return 1;
}

void lain_anim_play_site_enter(void) {
    lain_anim_play_sequence(1);
}

/* Releases the buffer of every fully loaded slot in g_lain_idle_anims. */
void lain_anim_free_idle(void) {
    s32 i;

    if (g_lain_idle_loading == 0) {
        g_lain_anim_playing = 0;
        for (i = 0; i < 1; i++) {
            if (g_lain_idle_anims[i].state == 2) {
                g_lain_idle_anims[i].state = 0;
                heap_free(g_lain_idle_anims[i].fileId);
            }
        }
    }
}

/* Loads the first 6 slots of g_lain_action_anims; returns 1 when all are loaded. */
s32 lain_anim_ready_set6(void) {
    s32 i;

    for (i = 0; i < 6; i++) {
        if (g_lain_action_anims[i].state != 2) {
            if (g_lain_action_anims[i].state == -1) {
                FntPrint("MEMORY TOO FEW\n");
                return 0;
            }
            if (cd_poll_load(g_lain_action_anims[i].id) == 0) {
                return 0;
            }
            g_lain_action_anims[i].state = 2;
        }
    }
    return 1;
}

/* Streams the next queued file from g_lain_action_anims; returns 1 while busy, 0 when done, -1 if not started. */
s32 lain_anim_play_next(void) {
    if (g_lain_anim_playing == 0) {
        anim_open_archive(g_lain_action_anims[g_lain_action_anim_index].fileId);
        if (g_lain_action_anims[g_lain_action_anim_index].sfx != 0) {
            g_lain_anim_sfx = g_lain_action_anims[g_lain_action_anim_index].sfx;
        }
        g_lain_action_anim_index++;
        g_lain_action_anim_active = 1;
        g_lain_anim_playing = 1;
    }
    anim_decode_frame(g_lain_anim_frame);
    if (anim_next_frame() == 0) {
        if (g_lain_anim_sfx != 0) {
            g_lain_anim_sfx = 0;
        }
        g_lain_anim_playing = 0;
        g_lain_action_anim_index = 5;
        g_lain_anim_playing = 0;
        return 0;
    }
    if (g_lain_action_anim_active != 0) {
        return 1;
    }
    return -1;
}

/* Starts streaming the file for slot g_lain_anim_slot_order[index] (index clamped to >= 0). */
void lain_anim_open_slot(s32 index) {
    if (index < 0) {
        index = 0;
    }
    anim_open_archive(g_lain_action_anims[g_lain_anim_slot_order[index]].fileId);
    anim_decode_frame(g_lain_anim_frame);
}

/* True when no load is pending. */
s32 lain_anim_is_idle(void) {
    return g_lain_idle_loading == 0;
}

/* Whether node i may be shown on the grid (visible, right side, level and depth allowed). */
s32 node_is_displayable(s32 i) {
    if (g_node_table[i].visible) {
        if (g_current_site != 1 && (g_node_table[i].pos.bytes[1] >> 7)) {
            return 0;
        }
        if (g_node_table[i].kind == 7 && g_node_table[i].level < g_sskn_level) {
            return 0;
        }
        if (g_media_played_count != 0) {
            if (g_node_table[i].depth <= g_media_played_count + 1) {
                return 1;
            }
        } else if (g_node_table[i].depth <= g_media_played_count) {
            return 1;
        }
    }
    return 0;
}

/* Clears the node grid and places every displayable node into its cell. */
void node_grid_build(void) {
    s32 i;
    s32 j;
    s32 col;
    s32 cell;
    s32 row;
    s16 pos;

    g_site_rotation = 0;
    g_site_level = 0;
    g_site_action_busy = 0;
    for (i = 0; i < site_get_level_count(); i++) {
        memset(g_site_grid[i], 0, sizeof(g_site_grid[i]));
    }
    for (i = 0; i < site_get_level_count(); i++) {
        for (j = 0; j < 24; j++) {
            g_site_grid[i][j].node = -1;
        }
    }
    for (i = 0; i < 0x2CC; i++) {
        pos = g_node_table[i].pos.raw;
        col = pos & 7;
        cell = (pos >> 3) & 0xFFF;
        if (((pos >> 15) & 1) == g_current_site && cell > 0) {
            cell--;
            row = cell / 3;
            col += (cell % 3) * 8;
            if (node_is_displayable(i)) {
                g_site_grid[row][col].node = i;
                g_site_grid[row][col].icon = g_node_kind_icons[g_node_table[i].kind];
                if (g_node_table[i].flag26) {
                    g_site_grid[row][col].icon += 8;
                }
            }
        }
    }
}

/* Finds the grid cell holding a node; writes -1/-1 when it isn't placed. */
static inline void findGridCell(s32 nodeIdx, s16 *outCol, s16 *outRow) {
    s32 row;
    s32 col;

    for (row = 0; row < site_get_level_count(); row++) {
        for (col = 0; col < 24; col++) {
            if (g_site_grid[row][col].node == nodeIdx) {
                *outCol = col;
                *outRow = row;
                return;
            }
        }
    }
    *outCol = -1;
    *outRow = -1;
}

/* Searches forward (wrapping) from nodeIdx for another visible node sharing
 * tag `slot`, returning its grid position; 1 if found. */
s32 node_find_by_keyword(s16 nodeIdx, s16 slot, s16 *outCol, s16 *outRow) {
    s32 tag = g_node_table[nodeIdx].tags[slot];
    s32 i;
    s32 k;

    for (i = nodeIdx + 1; i < 0x2CC; i++) {
        if (node_is_displayable(i)) {
            for (k = 0; k < 3; k++) {
                if (g_node_table[i].tags[k] == tag) {
                    findGridCell(i, outCol, outRow);
                    if (*outCol != -1) {
                        return 1;
                    }
                }
            }
        }
    }
    for (i = 0; i < nodeIdx; i++) {
        if (node_is_displayable(i)) {
            for (k = 0; k < 3; k++) {
                if (g_node_table[i].tags[k] == tag) {
                    findGridCell(i, outCol, outRow);
                    if (*outCol != -1) {
                        return 1;
                    }
                }
            }
        }
    }
    return 0;
}

/* Unlocks the children of nodeIdx, placing them on the grid (plays a sound once unless quiet). */
void node_unlock_children(s32 nodeIdx, s32 quiet) {
    s32 i;
    s32 col;
    s32 cell;
    s32 row;
    s32 flag;
    s16 pos;

    if (g_node_table[nodeIdx].flag26) {
        return;
    }
    g_node_table[nodeIdx].flag26 = 1;
    for (i = 0; i < 0x2CC; i++) {
        if (g_node_table[i].parent == nodeIdx) {
            pos = g_node_table[i].pos.raw;
            col = pos & 7;
            cell = (pos >> 3) & 0xFFF;
            flag = (pos >> 15) & 1;
            if (cell > 0) {
                g_node_table[i].visible = 1;
                if (flag == g_current_site) {
                    cell--;
                    row = cell / 3;
                    col += (cell % 3) * 8;
                    if (g_site_grid[row][col].node < 0 && node_is_displayable(i)) {
                        if (!quiet) {
                            snd_play_sfx(0x1F);
                            quiet = 1;
                        }
                        g_site_grid[row][col].node = i;
                        g_site_grid[row][col].icon = g_node_kind_icons[g_node_table[i].kind];
                        site_node_model_reload(row, col);
                    }
                }
            }
        }
    }
    if (g_node_table[nodeIdx].kind >= 7 && g_node_table[nodeIdx].kind <= 9) {
        g_node_table[nodeIdx].visible = 0;
    }
}

/* Random reaction for the node under the cursor; -1 if the cell is empty. */
s32 node_pick_select_anim(void) {
    s16 row = g_site_cursor_row / 3;
    s16 sub = g_site_cursor_row % 3;
    s32 idx;

    idx = g_site_cursor_col + sub * 8;
    idx = g_site_grid[row][idx].node;
    if (idx >= 0) {
        if (g_node_table[idx].depth <= g_media_played_count && g_node_table[idx].level <= g_sskn_level) {
            return rand() % 2;
        }
        return rand() % 3 + 2;
    }
    return -1;
}

/* Whether the node under the cursor has kind < 7. */
s32 node_cursor_is_media(void) {
    s16 row = g_site_cursor_row / 3;
    s16 sub = g_site_cursor_row % 3;
    s32 idx;

    idx = g_site_cursor_col + sub * 8;
    idx = g_site_grid[row][idx].node;
    return g_node_table[idx].kind < 7;
}

/* Resets every node: clears flag26 and restores visibility from flag25. */
void node_reset_all(void) {
    s32 i;

    for (i = 0; i < 0x2CC; i++) {
        g_node_table[i].flag26 = 0;
        g_node_table[i].visible = g_node_table[i].flag25;
    }
}

/* Sound init: loads the VAB header/body and two sequences from file 5,
 * opens them and (unless `quiet`) starts playback at full volume. */
void snd_init(s32 quiet) {
    s32 request;
    u8 *vabBody;
    s32 i;

    if (quiet == 0) {
        SsInit();
        SsSetMVol(0, 0);
        SsSetReservedVoice(0x17);
        SsSetTableSize(g_snd_seq_table, 2, 1);
        SsSetTickMode(2);
        request = cd_load_archive_entry(5, 0, g_snd_file_table, g_snd_vab_header);
        while (!cd_poll_load(request)) {
            loading_anim_draw(0);
        }
        vabBody = (u8 *)heap_alloc(g_snd_file_table[3]);
        request = cd_load_archive_entry(5, 1, g_snd_file_table, vabBody);
        while (!cd_poll_load(request)) {
            loading_anim_draw(0);
        }
        request = cd_load_archive_entry(5, 2, g_snd_file_table, g_bgm_seq_main_data);
        while (!cd_poll_load(request)) {
            loading_anim_draw(0);
        }
        request = cd_load_archive_entry(5, 3, g_snd_file_table, g_bgm_seq_credits_data);
        while (!cd_poll_load(request)) {
            loading_anim_draw(0);
        }
        g_snd_vab_id = SsVabOpenHead(g_snd_vab_header, -1);
        if (g_snd_vab_id != -1 && SsVabTransBody(vabBody, g_snd_vab_id) == g_snd_vab_id) {
            while (!SsVabTransCompleted(0)) {
                loading_anim_draw(0);
            }
            heap_free((s32)vabBody);
            SsUtSetReverbType(1);
            SsUtReverbOn();
            SsUtSetReverbDepth(0x30, 0x30);
            g_bgm_seq_main = SsSeqOpen(g_bgm_seq_main_data, g_snd_vab_id);
            g_bgm_seq_credits = SsSeqOpen(g_bgm_seq_credits_data, g_snd_vab_id);
            g_bgm_slot = 0;
            if (quiet == 0) {
                SsStart();
                for (i = 0; i < 10; i++) {
                    loading_anim_draw(0);
                }
                SsSetMVol(0x7F, 0x7F);
            }
        }
    }
}

/* Silences all voices. */
void snd_all_keys_off(void) {
    SsUtKeyOff(0, 0, 0, 0, 0x40);
}

void snd_play_sfx_18(void) {
    SsUtKeyOn(g_snd_vab_id, 0x18, 0, 0x3C, 0, 0x7F, 0x7F);
}

/* Empty in this build; called every vblank from game_vsync_callback. */
void snd_vsync_update(void) {
}

/* Plays sound-effect program `prog` (voice kept in g_snd_sfx_voice). */
void snd_play_sfx(s16 prog) {
    D_80073700[0] = 0x78;
    g_snd_sfx_voice = SsUtKeyOn(g_snd_vab_id, prog, 0, 0x3C, 0, 0x7F, 0x7F);
}

/* Plays sound-effect program `prog` (voice kept in g_snd_anim_sfx_voice). */
void snd_play_anim_sfx(s16 prog) {
    g_snd_anim_sfx_voice = SsUtKeyOn(g_snd_vab_id, prog, 0, 0x3C, 0, 0x7F, 0x7F);
}

void snd_stop_anim_sfx(void) {
    SsUtKeyOffV(g_snd_anim_sfx_voice);
}


