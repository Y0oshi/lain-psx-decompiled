#include "common.h"

/* These are addressed absolutely (lui/%lo via $at) even though they are 2 bytes:
 * keep cc1 treating them as small but stop the .extern size hint. */
#define NO_GP __attribute__((section(".data")))

typedef struct {
    s16 left;
    s16 right;
} SpuVolume;

typedef struct {
    SpuVolume volume;
    s32 reverb;
    s32 mix;
} SpuExtAttr;

typedef struct {
    u32 mask;
    SpuVolume mvol;
    SpuVolume mvolmode;
    SpuVolume mvolx;
    SpuExtAttr cd;
    SpuExtAttr ext;
} SpuCommonAttr;

typedef struct {
    s16 x, y, w, h;
} RECT;

typedef struct {
    u8 minute;
    u8 second;
    u8 sector;
    u8 track;
} CdlLOC;

/* MDEC decode environment (same layout as the PsyQ movie sample's DECENV). */
typedef struct {
    u32 *vlcbuf[2];  /* 0x00 */
    s32 vlcid;       /* 0x08 */
    u16 *imgbuf[2];  /* 0x0C */
    s32 imgid;       /* 0x14 */
    RECT rect[2];    /* 0x18 */
    s32 rectid;      /* 0x28 */
    RECT slice;      /* 0x2C */
    s32 isdone;      /* 0x34 */
} DECENV;

typedef struct {
    s16 w;
    s16 h;
} Size16;

/* Slideshow image sprite (drawn by sprite_draw_rotated). */
typedef struct {
    u8 pad0[0x28];
    u32 attribute; /* 0x28 */
    u16 tpage;     /* 0x2C */
    u16 clut;      /* 0x2E */
    u16 scaleX;    /* 0x30 */
    u16 scaleY;    /* 0x32 */
    s16 x;         /* 0x34 */
    s16 y;         /* 0x36 */
    s16 w;         /* 0x38 */
    s16 h;         /* 0x3A */
    u8 pad3C[8];
    u8 u;          /* 0x44 */
    u8 v;          /* 0x45 */
    u8 r;          /* 0x46 */
    u8 g;          /* 0x47 */
    u8 b;          /* 0x48 */
} ImageSprite;

/* libgs GsSPRITE. */
typedef struct {
    u32 attribute;
    s16 x, y;
    u16 w, h;
    u16 tpage;
    u8 u, v;
    s16 cx, cy;
    u8 r, g, b;
    s16 mx, my;
    s16 scalex, scaley;
    s32 rotate;
} GsSPRITE;

/* Archive file table entry: start sector and byte size. */
typedef struct {
    s32 sector;
    s32 size;
} FileEntry;

typedef struct {
    u32 pmode;
    s16 px, py;
    u16 pw, ph;
    u32 *pixel;
    s16 cx, cy;
    u16 cw, ch;
    u32 *clut;
} GsIMAGE;

typedef struct {
    u8 xa_file;
    u8 pad1;
    s16 file;
    s32 size;
} MovieInfo;

typedef struct {
    s32 pos;
    s32 size;
} FileLoc;

/* Per-track slideshow parameters (copied to g_slideshow_file0/C2/C4). */
typedef struct {
    u16 a;
    u16 b;
    u16 c;
} TrackParams;

/* libgs ordering table header. */
typedef struct {
    u32 length;
    u32 *org;
    u32 offset;
    u32 point;
    u32 *tag;
} GsOT;

/* CD stream sector header (libcd StHEADER). */
typedef struct {
    u16 id;
    u16 type;
    u16 secCount;
    u16 nSectors;
    u32 frameCount;
    u32 frameSize;
    u16 width;
    u16 height;
} StHEADER;

/* Subtitle timing entry: shown while start <= frame <= end. */
typedef struct {
    u8 show;
    u8 pad1;
    s16 start;
    s16 end;
} SubtitleCue;

typedef struct {
    u8 pad0[0x20];
    union {
        u32 flags;
        struct {
            u32 unk0 : 20;
            u32 type : 4;
            u32 unk24 : 2;
            u32 seen : 1;
            u32 unk27 : 5;
        } bits;
    } u;
    u8 pad24[4];
} MovieState;

/*
 * This file holds two original translation units. 80018A38..80018F0C (pad input and
 * menu cursor) addresses g_current_site/g_pad_command/g_polytan_parts via $gp; 80018F48..8001D0C8 (movie
 * and music player) addresses them absolutely. Functions of the second part that touch them
 * are under NON_MATCHING until the file is split.
 *
 * Globals declared as one-element arrays (g_slideshow_file0/C2/C4, g_vlc_qscale_chroma/5E) are accessed
 * "in struct" by GCC's alias analysis, which keeps their stores ordered against pointer loads.
 */

extern FileLoc g_disc_file_table[];
extern MovieInfo g_media_table[];
extern s32 g_xa_file_ids[];
extern TrackParams g_idle_voice_slideshow[][10];
extern SubtitleCue g_ending_cues[];
extern MovieState g_node_table[];
extern FileEntry g_bin_file_table[];
extern FileEntry g_site_a_file_table[];
extern FileEntry g_site_b_file_table[];
extern u8 g_movie_vlc_table[];
extern s16 g_gate_level;
extern s16 g_polytan_parts;
extern s16 g_sskn_level;
extern s16 g_media_played_count;
extern s16 g_movie_stream_state;
extern s16 g_player_ui_mode;
extern s16 g_player_done;
extern s32 g_media_id;
extern u8 g_media_xa_file;
extern s16 g_xa_playing;
extern u8 g_player_ui_loaded;
extern s16 g_movie_width;
extern u16 g_movie_height;
extern u8 g_slideshow_state;
extern u8 D_800A5F77;
extern u8 g_slideshow_index;
extern u8 g_player_resume_pending;
extern u8 D_800A5F7A;
extern u16 g_idle_voice_base[3];
extern u8 g_player_keyword_result;
extern u8 g_player_ending_mode;
extern u8 g_player_result;
extern s16 g_spu_decode_flag;
extern u8 g_slideshow_next_at;
extern s16 g_movie_slice_bytes;
extern s16 g_movie_dct_mode;
extern s16 g_movie_rgb24;
extern s16 g_movie_slice_w;
extern u8 g_str_play[];
extern u8 g_player_empty_str[];
extern u16 g_vsync_counter NO_GP;
extern s16 g_site_cursor_col;
extern s16 g_site_cursor_row;
extern s16 g_other_site_cursor_col;
extern s16 g_other_site_cursor_row;
extern s16 g_stat_gakkuri;
extern s16 g_stat_harumage;
extern s16 g_stat_tokimeki;
extern s32 g_save_count;
extern s16 g_current_site;
extern s16 g_screensaver_count;
extern s16 g_site_cursor_on_node;
extern s16 g_pad_prev_command;
extern s16 g_site_sel_col NO_GP;
extern s16 g_site_action_request;
extern s16 g_pad_command;
extern s16 g_site_sel_level NO_GP;
extern void *g_movie_ring_buf;
extern s32 g_xa_start_sector;
extern s32 g_xa_end_sector;
extern s32 g_xa_cur_sector;
extern s32 g_movie_frame_rl_size;
extern u8 g_movie_done_rectid;
extern FileEntry *g_site_file_tables;
extern FileEntry *D_800A689C; /* = g_site_file_tables[1] (site B) */
extern s16 g_movie_frame_no[2];
extern s16 g_ending_cue_index;
extern CdlLOC g_movie_cd_loc;
extern u8 g_player_progress;
extern s32 g_player_progress_step;
extern u16 g_vlc_decode_time;
extern u8 g_ending_card_shown;
extern u8 g_player_user_exit;
extern s16 g_slideshow_file0[1];
extern s16 g_slideshow_file1[1];
extern s16 g_slideshow_file2[1];
extern s32 D_800A68C8;
extern s32 g_spu_decode_buf;
extern s16 g_player_quit;
extern s32 g_spu_irq_addr;
extern void *g_player_load_buf;
extern u32 *g_ending_card_tim;
extern s32 g_frame_buffer_index NO_GP;
extern s16 g_vlc_qscale_chroma[1] NO_GP;
extern s16 g_vlc_qscale_luma[1] NO_GP;
extern u8 g_pad_buf[];
extern u8 g_pad1_buttons[];
extern DECENV g_movie_decenv;
extern CdlLOC g_movie_start_loc[];
extern Size16 g_slideshow_sizes[];
extern GsIMAGE g_tim_info;
extern ImageSprite g_slideshow_sprite;
extern GsSPRITE g_player_cursor_sprite;
extern u8 g_gs_packet_area[];
extern GsOT g_ot[];
extern u8 g_view[];
extern s32 D_801F96E4 NO_GP;

u32 PadGetState(s32 port);
void PadInitDirect(u8 *pad1, u8 *pad2);
void PadStartCom(void);
u16 GetClut(s32 x, s32 y);
u16 GetTPage(s32 tp, s32 abr, s32 x, s32 y);
s32 CdControlB(u8 com, u8 *param, u8 *result);
s32 CdControl(u8 com, u8 *param, u8 *result);
s32 CdRead2(s32 mode);
void SpuSetIRQ(s32 on_off);
void SpuReadDecodedData(s32 buf, s32 flag);
void SpuSetIRQAddr(s32 addr);
void StCdInterrupt(void);
void DecDCTout(u16 *buf, s32 size);
void DecDCToutCallback(void (*func)());
void StUnSetRing(void);
s32 CdSync(s32 mode, u8 *result);
s32 CdControlF(u8 com, u8 *param);
s32 rand(void);
s32 StGetNext(u32 **addr, StHEADER **header);
void *memset(void *s, s32 c, s32 n);
s32 CdLastCom(void);
void DecDCTin(u32 *buf, s32 mode);
void StFreeRing(u32 *base);
void DecDCTReset(s32 mode);
void StSetRing(void *ring, s32 size);
void StSetStream(s32 mode, s32 start, s32 end, void (*func1)(), void (*func2)());
void srand(u32 seed);
void GsSwapDispBuff(void);
void MoveImage(RECT *rect, s32 x, s32 y);
void GsInitGraph2(s32 w, s32 h, s32 intl, s32 dither, s32 vrammode);
void GsSetRefView2(void *view);
void GsClearOt(s32 offset, s32 point, GsOT *ot);
void GsSortClear(s32 r, s32 g, s32 b, GsOT *ot);
void GsDrawOt(GsOT *ot);
s32 CdPosToInt(u8 *p);
void ClearImage(RECT *rect, s32 r, s32 g, s32 b);
s32 GsGetActiveBuff(void);
s32 VSync(s32 mode);
void CdIntToPos(s32 i, u8 *p);
void GsGetTimInfo(u32 *tim, GsIMAGE *image);
void LoadImage(RECT *rect, u32 *p);
void DrawSync(s32 mode);
void SpuSetCommonAttr(SpuCommonAttr *attr);

s32 cd_load_archive_entry(u16 arg0, s32 index, FileEntry *table, void *dest);
s32 cd_poll_load(s32 id);
void pad_read_command(void);
void media_player_run(void);
void movie_dct_out_callback(void);
u32 *movie_get_next_frame(DECENV *dec);
void movie_decode_frame(void);
void movie_start_stream(s32 pos);
void idle_play_random_movie(void);
void idle_play_random_voice(void);
void ending_movie_play(void);
void slideshow_show_image(s16 index);
void slideshow_load_images(void);
void slideshow_update(void);
void movie_cd_seek_read(u8 *loc);
void movie_stop_stream(void);
s32 xa_start_read(s32 unused);
s32 xa_play_range(s32 pos, s32 end);
void xa_seek_file(void);
void text_window_set_text(s32 a0, s32 a1, u8 *a2, s32 a3);
void node_grid_build(void);
void spinner_init(u8 *arg0, s32 arg1);
void spinner_draw(void);
void spinner_free(void);
void sprite_draw_rotated(ImageSprite *sprite, GsOT *ot, s32 pri);
void site_scene_reset_to_node(s32 a, s32 b);
void movie_models_init(void);
void audio_node_play(void);
void *lz_decompress(void *src, s32 size);
void *heap_alloc(s32 size);
void heap_free(void *ptr);
void loading_anim_draw(s32 arg0);
void GsSetWorkBase(u8 *base);
void vlc_decode_frame(u32 *bs, u32 *buf);
void vlc_build_table(u8 *arg0);


s32 site_get_level_count(void) {
    if (g_current_site == 0) {
        return 0x16;
    }
    return 0xD;
}

void site_init_new_game(void) {
    s32 unused[2]; /* never used; reserves the 8-byte stack slot the original has */
    s32 col;

    g_site_cursor_col = 6;
    g_site_cursor_row = 10;
    g_other_site_cursor_col = 5;
    g_site_sel_level = 3;
    g_current_site = 0;
    g_gate_level = 0;
    g_polytan_parts = 0;
    g_sskn_level = 0;
    g_media_played_count = 0;
    g_other_site_cursor_row = 0;
    g_stat_tokimeki = 0;
    g_screensaver_count = 0;
    g_stat_harumage = 0;
    g_stat_gakkuri = 0;
    col = 14;
    g_site_sel_col = col;
    site_scene_reset_to_node(3, col);
    g_site_cursor_on_node = 1;
    g_save_count = 0;
}

void site_reload(void) {
    s32 unused[2]; /* never used; reserves the 8-byte stack slot the original has */
    s32 row;
    s32 col;

    g_stat_tokimeki = 0;
    g_screensaver_count = 0;
    g_stat_harumage = 0;
    g_stat_gakkuri = 0;
    row = (s16)(g_site_cursor_row / 3);
    col = g_site_cursor_col + (s16)(g_site_cursor_row % 3) * 8;
    g_site_sel_level = row;
    g_site_sel_col = col;
    node_grid_build();
    site_scene_reset_to_node(row, col);
}

/* Maps the (active-low) d-pad bits of pad 1 to one of 8 directions in g_pad_command. */
void pad_read_dpad(void) {
    u8 buttons = g_pad1_buttons[0];

    if (!(buttons & 0x10)) {
        if (!(buttons & 0x80)) {
            g_pad_command = 2;
        } else if (!(buttons & 0x20)) {
            g_pad_command = 3;
        } else {
            g_pad_command = 1;
        }
    } else if (!(buttons & 0x40)) {
        if (!(buttons & 0x80)) {
            g_pad_command = 7;
        } else if (!(buttons & 0x20)) {
            g_pad_command = 8;
        } else {
            g_pad_command = 6;
        }
    } else if (!(buttons & 0x80)) {
        g_pad_command = 4;
    } else if (!(buttons & 0x20)) {
        g_pad_command = 5;
    }
}

/* Reads pad 1 (digital or analog) into a command code in g_pad_command; a code equal to
 * the previous frame's (g_pad_prev_command) is suppressed so holding a button fires once. */
void pad_read_command(void) {
    s32 type;
    u8 buttons;
    u8 buttons2;

    g_pad_command = 0;
    if (PadGetState(0) >= 2) {
        type = g_pad_buf[1] >> 4;
        if (g_pad_buf[0] == 0 && (type == 4 || type == 7)) {
            buttons = g_pad_buf[3];
            if (!(buttons & 0x20)) {
                g_pad_command = 0x11;
            } else if (!(buttons & 0x40)) {
                g_pad_command = 0x12;
            } else {
                buttons2 = g_pad_buf[2];
                if (!(buttons2 & 0x8)) {
                    g_pad_command = 0x14;
                } else if (!(buttons & 0x10) || !(buttons2 & 0x1)) {
                    g_pad_command = 0x13;
                } else if (!(buttons & 0x80)) {
                    g_pad_command = 0x15;
                } else if (!(buttons & 0x4)) {
                    g_pad_command = 0xC;
                } else if (!(buttons & 0x1)) {
                    g_pad_command = 0xE;
                } else if (!(buttons & 0x8)) {
                    g_pad_command = 0xD;
                } else if (!(buttons & 0x2)) {
                    g_pad_command = 9;
                } else if (type == 7) {
                    if (g_pad_buf[4] < 0x50) {
                        g_pad_command = 0xC;
                    } else if (g_pad_buf[4] > 0xB0) {
                        g_pad_command = 0xD;
                    } else if (g_pad_buf[5] < 0x50) {
                        g_pad_command = 0xA;
                    } else if (g_pad_buf[5] > 0xB0) {
                        g_pad_command = 0xB;
                    } else if (g_pad_buf[7] < 0x50) {
                        if (g_pad_buf[6] < 0x50) {
                            g_pad_command = 2;
                        } else if (g_pad_buf[6] > 0xB0) {
                            g_pad_command = 3;
                        } else {
                            g_pad_command = 1;
                        }
                    } else if (g_pad_buf[7] > 0xB0) {
                        if (g_pad_buf[6] < 0x50) {
                            g_pad_command = 7;
                        } else if (g_pad_buf[6] > 0xB0) {
                            g_pad_command = 8;
                        } else {
                            g_pad_command = 6;
                        }
                    } else if (g_pad_buf[6] < 0x50) {
                        g_pad_command = 4;
                    } else if (g_pad_buf[6] > 0xB0) {
                        g_pad_command = 5;
                    } else {
                        pad_read_dpad();
                    }
                } else {
                    pad_read_dpad();
                }
            }
        }
    }
    if (g_pad_prev_command != 0 && g_pad_prev_command == g_pad_command) {
        g_pad_command = 0;
    } else {
        g_pad_prev_command = g_pad_command;
    }
}

/* Initializes the controller ports (direct pad buffers at g_pad_buf). */
void pad_init(void) {
    g_pad_command = 0;
    g_pad_prev_command = 0;
    g_site_action_request = 0;
    PadInitDirect(g_pad_buf, g_pad_buf + 0x24);
    PadStartCom();
}
