#include "common.h"

/* Declarations carried over from 80018A38.c (same original headers). */

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
 * and music player) addresses them absolutely.
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
extern s16 g_polytan_parts NO_GP; /* absolute in this TU */
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
extern s16 g_current_site NO_GP; /* absolute in this TU */
extern s16 g_screensaver_count;
extern s16 g_site_cursor_on_node;
extern s16 g_pad_prev_command;
extern s16 g_site_sel_col NO_GP;
extern s16 g_site_action_request;
extern s16 g_pad_command NO_GP; /* absolute in this TU */
extern s16 g_site_sel_level NO_GP;
extern void *g_movie_ring_buf;
extern s32 g_xa_start_sector;
extern s32 g_xa_end_sector;
extern s32 g_xa_cur_sector;
extern s32 g_movie_frame_rl_size;
extern u8 g_movie_done_rectid;
extern FileEntry *g_site_file_tables[2];
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
extern s32 D_801F96E4 NO_GP; /* libcd's StCdIntrFlag (deferred CD interrupt); kept as D_ because
                                * the port's PsyQ layer defines StCdIntrFlag itself */

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
s32 DrawSync(s32 mode);
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


/* Matches (0 diffs) once 80018F48..8001D0C8 is split into its own TU, where g_current_site/g_pad_command/g_polytan_parts are declared absolute (NO_GP); in this file they are $gp-relative: 3 diffs. */
/* Plays movie `movieId`: sets up the display/SPU, runs the per-frame update until it ends. */
s32 media_play(s32 movieId, s16 *skipped, s16 *params) {
    RECT rect;

    movie_models_init();
    rect.x = 0;
    rect.y = 0;
    rect.w = 0x140;
    rect.h = 0x1E0;
    ClearImage(&rect, 0, 0, 0);
    DrawSync(0);
    g_site_file_tables[0] = g_site_a_file_table;
    D_800A689C = g_site_b_file_table;
    g_player_resume_pending = 0;
    g_ending_card_shown = 0;
    g_player_result = 0;
    g_slideshow_state = 0;
    if (*skipped != 0) {
        g_player_ending_mode = 1;
    } else {
        g_player_ending_mode = 0;
    }
    g_slideshow_file0[0] = params[0];
    g_slideshow_file1[0] = params[1];
    g_media_id = movieId;
    g_slideshow_file2[0] = params[2];
    spu_cd_audio_off();
    g_player_progress = 0;
    g_player_progress_step = 1;
    g_frame_buffer_index = GsGetActiveBuff();
    g_player_done = 0;
    g_player_ui_loaded = 0;
    g_player_ui_mode = 1;
    g_player_keyword_result = 0;
    *skipped = 0;
    g_media_xa_file = g_media_table[g_media_id].xa_file;
    if (g_media_id >= 0x206) {
        audio_node_play();
        return 2;
    }
    text_window_set_text(0x34, 0xAF, g_str_play, 1);
    while (g_player_done == 0) {
        media_player_run();
        *skipped = g_player_keyword_result;
        g_player_ui_loaded = 1;
    }
    if (g_player_ending_mode == 2) {
        if (g_media_xa_file == 0) {
            g_node_table[movieId].u.flags |= 0x4000000;
        }
        ending_movie_play();
        g_media_id = 0x2DD;
        audio_node_play();
        return 1;
    }
    text_window_set_text(0, 0, 0, 0);
    g_pad_command = 0;
    VSync(2);
    return g_player_result;
}

/* Declarations copied from src/game/80018F48.c (movie/music player TU). */




/* Slideshow image sprite (drawn by sprite_draw_rotated). */

/* libgs GsSPRITE. */

/* libgs GsLINE. */
typedef struct {
    u32 attribute;
    s16 x0, y0;
    s16 x1, y1;
    u8 r, g, b;
} GsLINE;

/* Archive file table entry: start sector and byte size. */




/* libgs ordering table header. */

/* Used by media_player_run. */

/* One column of the music player's level meter: 7 stacked bar sprites plus the
 * number of lit bars (allocated as a 0x100-byte block). */
typedef struct {
    GsSPRITE bar[7]; /* 0x00 */
    u8 level;        /* 0xFC */
} MeterColumn;

/* Point on the 30-step ellipse the three music-menu items rotate along. */
typedef struct {
    u8 x;
    u8 y;
} Pos8;

/* 5-byte label ("Play"/"Exit" with terminator), copied inline by strcpy. */
typedef struct {
    char s[5];
} Label5;

extern FileLoc g_disc_file_table[];
extern MovieInfo g_media_table[];
extern Pos8 g_player_menu_ellipse[];     /* 30 menu positions */
extern GsLINE g_player_cursor_lines[];   /* the two cursor cross-hair lines */
extern FileEntry g_bin_file_table[];
extern s16 g_movie_stream_state;         /* <0 draining, 0 paused, 1 playing */
extern s16 D_800A5F5E;
extern s16 g_player_ui_mode;         /* 0 idle, 1 paused("Play"), 2 menu focus, 3/4 rotating */
extern s16 g_player_done;
extern s32 g_media_id;
extern u8 g_media_xa_file;          /* 0 = movie, else music track */
extern s16 g_xa_playing;
extern u8 g_player_ui_loaded;
extern u8 g_slideshow_index;
extern u8 g_player_resume_pending;
extern u8 g_player_keyword_sel;          /* selected menu item */
extern u8 g_player_keyword_result;
extern u8 g_player_ending_mode;
extern s16 g_spu_decode_flag;
extern u8 g_slideshow_next_at;
extern u8 g_str_play[];        /* "Play" */
extern s16 g_player_cursor_frame;         /* cursor animation frame */
extern s16 g_player_keyword_pos[3];      /* menu item positions on the ellipse */
extern s16 g_player_cursor_pos;         /* cursor position on the ellipse */
extern u8 D_800A5FA8;
extern s32 g_player_hilite_w;
extern u16 g_player_ot_pri;         /* next OT priority */
extern u8 g_player_keyword_order[3];       /* menu items sorted by x; sized: cc1 then emits `la` (hi/lo in one reg) */
extern u8 g_str_exit[];        /* "Exit" */
extern s16 g_text_window_busy NO_GP;
extern s16 g_pad_command NO_GP;
extern s32 g_player_i;
extern s32 g_player_j;
extern s32 g_player_tmp;
extern s32 g_level_meter_clut_y;
extern u8 g_player_progress;
extern s32 g_player_progress_step;
extern u8 g_player_user_exit;
extern s16 g_slideshow_file2[1];
extern s32 g_spu_decode_buf;
extern u16 g_player_tpage;
extern s16 g_player_quit;
extern s32 g_spu_irq_addr;
extern s16 g_text_window_target_x NO_GP;
extern s16 g_text_window_target_y NO_GP;
extern s32 g_frame_buffer_index NO_GP;
extern GsSPRITE g_player_keyword_hilite[3];  /* menu item highlight */
extern GsSPRITE D_800D0E18;
extern MeterColumn *g_level_meter[15];
extern GsSPRITE g_player_progress_frame_sprite;
extern GsSPRITE D_800D0EE8;
extern GsSPRITE g_player_keyword_sprites[3];  /* menu item labels */
extern Size16 g_slideshow_sizes[];
extern GsIMAGE g_tim_info;
extern ImageSprite g_slideshow_sprite;
extern GsSPRITE g_player_progress_sprite;     /* progress bar */
extern GsSPRITE g_player_cursor_sprite;     /* animated cursor */
extern char g_text_window_next_name[];
extern u8 g_gs_packet_area[];
extern GsOT g_ot_2d[];
extern GsOT g_ot[];
extern u8 g_view[];

u16 GetTPage(s32 tp, s32 abr, s32 x, s32 y);
s32 CdRead2(s32 mode);
void SpuSetIRQ(s32 on_off);
void SpuSetIRQAddr(s32 addr);
void SpuSetIRQCallback(void (*func)(void));
void SpuSetTransferCallback(void (*func)(void));
s32 CdSync(s32 mode, u8 *result);
s32 CdControlF(u8 com, u8 *param);
s32 rand(void);
void GsSwapDispBuff(void);
void MoveImage(RECT *rect, s32 x, s32 y);
void GsSetRefView2(void *view);
void GsClearOt(s32 offset, s32 point, GsOT *ot);
void GsSortClear(s32 r, s32 g, s32 b, GsOT *ot);
void GsDrawOt(GsOT *ot);
GsOT *GsSortOt(GsOT *ot_src, GsOT *ot_dest);
void GsSortFastSprite(GsSPRITE *sp, GsOT *ot, u16 pri);
void GsSortSprite(GsSPRITE *sp, GsOT *ot, u16 pri);
void GsSortLine(GsLINE *lp, GsOT *ot, u16 pri);
void ClearImage(RECT *rect, s32 r, s32 g, s32 b);
s32 GsGetActiveBuff(void);
s32 VSync(s32 mode);
void GsGetTimInfo(u32 *tim, GsIMAGE *image);
void LoadImage(RECT *rect, u32 *p);

s32 cd_load_archive_entry(u16 arg0, s32 index, FileEntry *table, void *dest);
s32 cd_poll_load(s32 id);
void pad_read_command(void);
void xa_play_channel(s16 channel);
void movie_decode_frame(void);
void movie_start_stream(s32 pos);
void slideshow_load_images(void);
void slideshow_update(void);
void spu_irq_read_decoded(void);
void spu_decoded_xfer_done(void);
void movie_stop_stream(void);
void spu_cd_audio_off(void);
void spu_cd_audio_on(void);
s16 spu_decoded_peak_level(void);
void xa_seek_file(void);
/* Unprototyped in the original TU (all arguments passed as int). */
void gs_sprite_setup();
void text_window_run(s32 reset);
void snd_play_sfx(s32 id);
void spinner_draw(void);
void sprite_draw_rotated(ImageSprite *sprite, GsOT *ot, s32 pri);
void movie_models_update(s16 input);
void *lz_decompress(void *src, s32 size);
void *heap_alloc(s32 size);
void heap_free(void *ptr);
void GsSetWorkBase(u8 *base);

/* Inlined copy of player_load_ui_tim: loads the player UI TIM (file 0x15 of g_bin_file_table),
 * uploads its pixels to (x, y) and its clut to (0x1C0, clutY + 0x47). */
static inline void func_80019164_loadUiTim(s32 unused, s16 x, s16 y, s16 clutY) {
    RECT rect;
    void *buf;
    u32 *tim;
    s32 id;

    buf = heap_alloc(g_bin_file_table[0x15].size);
    id = cd_load_archive_entry(3, 0x15, g_bin_file_table, buf);
    while (cd_poll_load(id) == 0) {
    }
    tim = lz_decompress(buf, g_bin_file_table[0x15].size);
    heap_free(buf);
    tim++;
    GsGetTimInfo(tim, &g_tim_info);
    rect.x = x;
    rect.y = y;
    rect.w = g_tim_info.pw;
    rect.h = g_tim_info.ph;
    LoadImage(&rect, g_tim_info.pixel);
    DrawSync(0);
    if ((g_tim_info.pmode >> 3) & 1) {
        rect.x = 0x1C0;
        rect.y = clutY + 0x47;
        rect.w = g_tim_info.cw;
        rect.h = g_tim_info.ch;
        LoadImage(&rect, g_tim_info.clut);
        DrawSync(0);
    }
    heap_free(tim - 1);
}

/* Shows a label in the text box (inlined strcpy of a 5-byte literal). */
#define SHOW_LABEL(ypos, text)                          \
    {                                                   \
        g_text_window_target_x = 0x34;                              \
        g_text_window_target_y = ypos;                              \
        *(Label5 *)g_text_window_next_name = *(Label5 *)(text);      \
        text_window_run(1);                               \
    }

/* Matches (0 diffs on 2.8.0-psx and 2.8.1-psx). */
/*
 * Movie / music player main loop (called once per play from media_play and runs
 * until g_player_quit == 1).
 *
 * g_media_xa_file == 0: FMV playback. Each frame decodes one MDEC frame (movie_decode_frame),
 * draws the progress bar and player chrome, and handles Play/Pause/Exit.
 * g_media_xa_file != 0: XA music track. Draws the image slideshow, a 15-column level meter
 * driven by the decoded SPU audio peak, and a menu of three items rotating around an
 * ellipse that the cursor selects from.
 */
void media_player_run(void) {
    RECT rect;
    s16 buf;
    s16 prevMode;

    prevMode = 0;
    buf = g_frame_buffer_index;
    g_player_cursor_lines[0].y0 = g_player_cursor_lines[0].y1 = 0x46;
    g_player_cursor_lines[1].x0 = g_player_cursor_lines[1].x1 = 0x67;

    if (g_player_ui_loaded == 0) {
        /* First call: load the UI graphics and set up the player sprites. */
        func_80019164_loadUiTim(0, 0x1C0, 0, 1);
        g_player_tpage = GetTPage(0, 0, 0x1C0, 0);
        gs_sprite_setup(&g_player_progress_frame_sprite, 0xA1, 0x10, 0xA0, 0x10, g_player_tpage, 0x38, 0, 0x1C0, 0x4A, 0x58000020, 0, 0);
        gs_sprite_setup(&g_player_progress_sprite, 0xA8, 0x16, 8, 8, g_player_tpage, 0xB0, 0x28, 0x1C0, 0x48, 0, 0, 0);
        gs_sprite_setup(&D_800D0EE8, 0xF0, 0x20, 0x50, 0x10, g_player_tpage, 0x88, 0x10, 0x1C0, 0x4A, 0, 0, 0);
        gs_sprite_setup(&g_player_keyword_sprites[0], 0x28, 0x3F, 0x88, 0x10, g_player_tpage, 0, 0x10, 0x1C0, 0x48, 0x48000020, 0, 0);
        gs_sprite_setup(&g_player_keyword_sprites[1], 0x85, 0x9A, 0x88, 0x10, g_player_tpage, 0, 0x10, 0x1C0, 0x48, 0x48000020, 0, 0);
        gs_sprite_setup(&g_player_keyword_sprites[2], 0xA0, 0xBF, 0x88, 0x10, g_player_tpage, 0, 0x10, 0x1C0, 0x48, 0x48000020, 0, 0);
        g_player_hilite_w = 0x80;
        rect.x = 0x140;
        rect.w = 0x20;
        rect.y = 0x48;
        rect.h = 0x40;
        MoveImage(&rect, 0x1C0, 0x64);
        DrawSync(0);
        g_player_tpage = GetTPage(0, 0, 0x1C0, 0);
        gs_sprite_setup(&g_player_keyword_hilite[0], 0x2E, 0x42, g_player_hilite_w, 0x10, g_player_tpage, 0, 0x64, 0x10, 0x1E1, 0, 0, 0);
        gs_sprite_setup(&g_player_keyword_hilite[1], 0x8B, 0x9D, g_player_hilite_w, 0x10, g_player_tpage, 0, 0x74, 0x10, 0x1E1, 0, 0, 0);
        gs_sprite_setup(&g_player_keyword_hilite[2], 0xA6, 0xC2, g_player_hilite_w, 0x10, g_player_tpage, 0, 0x84, 0x10, 0x1E1, 0, 0, 0);
        gs_sprite_setup(&D_800D0E18, 0xFA, 0x24, 0x30, 0x10, g_player_tpage, 0, 0x94, 0, 0x1E1, 0, 0, 0);
    }

    g_player_quit = 0;
    g_player_user_exit = 0;
    if (g_media_xa_file == 0) {
        /* Movie: start the MDEC stream. */
        g_xa_playing = 0;
        D_800A5F5E = 0;
        g_movie_stream_state = -1;
        g_player_progress_step = g_media_table[g_media_id].size / 18;
        if (g_player_progress_step <= 0) {
            g_player_progress_step = 1;
        }
        movie_start_stream(g_disc_file_table[g_media_table[g_media_id].file].pos);
    } else {
        /* Music: set up SPU decoded-data capture for the level meter, the slideshow and
         * the meter sprites, then seek to the XA track. */
        g_slideshow_index = 1;
        g_spu_decode_buf = (s32)heap_alloc(0x1000);
        g_player_keyword_sel = 0;
        g_spu_decode_flag = 5;
        SpuSetTransferCallback(spu_decoded_xfer_done);
        SpuSetIRQCallback(spu_irq_read_decoded);
        SpuSetIRQAddr(g_spu_irq_addr = 0x100);
        g_player_cursor_pos = 1;
        g_player_keyword_pos[0] = -1;
        g_player_keyword_pos[1] = 9;
        g_movie_stream_state = 0;
        D_800A5F5E = 0;
        g_player_keyword_pos[2] = 0x13;
        slideshow_load_images();
        for (g_player_i = 0; g_player_i < 15; g_player_i++) {
            MeterColumn *col = heap_alloc(0x100);

            g_level_meter[g_player_i] = col;
            col->level = 0;
            for (g_player_j = 0; g_player_j < 7; g_player_j++) {
                gs_sprite_setup(&g_level_meter[g_player_i]->bar[g_player_j], g_player_j * 16, g_player_i * 16, 0x10,
                              0x10, GetTPage(0, 0, 0x1C0, 0), 0, 0, 0x1C0, 0x4A, 0x48000020, 0, 0);
            }
        }
        g_slideshow_next_at = 6;
        if (g_slideshow_file2[0] == -1) {
            g_slideshow_next_at = 9;
        }
        xa_seek_file();
    }
    g_movie_stream_state = -1;

    do {
        /* Background: movie frame or cleared screen. */
        if (g_media_xa_file == 0) {
            if (g_movie_stream_state != 0) {
                movie_decode_frame();
                if (g_movie_stream_state < 0 && ++g_movie_stream_state == 0) {
                    CdControlF(9, 0);
                    rect.x = 0x140;
                    rect.y = 0xF0;
                    rect.w = 0x140;
                    rect.h = 0xF0;
                    MoveImage(&rect, 0, buf * 0xF0);
                    DrawSync(0);
                }
            } else {
                /* paused: keep showing the last frame */
                rect.x = 0x140;
                rect.y = 0xF0;
                rect.w = 0x140;
                rect.h = 0xF0;
                MoveImage(&rect, 0, buf * 0xF0);
                DrawSync(0);
            }
        } else {
            rect.y = buf * 0xF0;
            rect.w = 0x140;
            rect.x = 0;
            rect.h = 0xF0;
            ClearImage(&rect, 0, 0, 0);
            DrawSync(0);
        }

        GsSetRefView2(g_view);
        GsSetWorkBase(g_gs_packet_area + buf * 36000);
        GsClearOt(0, 0, &g_ot[buf]);
        GsClearOt(0, 0, &g_ot_2d[buf]);
        DrawSync(0);
        g_frame_buffer_index = GsGetActiveBuff();
        text_window_run(0);
        D_800A5FA8 = 0;
        g_player_progress_sprite.v = 0x28;
        g_player_progress_sprite.u = 0xB0;
        if (g_slideshow_sizes[g_slideshow_index].w != -1) {
            slideshow_update();
        }

        /* Progress bar. */
        g_player_ot_pri = 0x13;
        if (g_xa_playing != 0 || g_movie_stream_state > 0) {
            if (g_player_progress < 0x13) {
                s32 len = g_player_progress * 8;

                g_player_progress_sprite.x = len + 0xAA;
                if (g_player_progress >= 6) {
                    g_player_progress_sprite.u = 0x88;
                    g_player_progress_sprite.w = 0x30;
                    g_player_progress_sprite.x = len + 0x7A;
                } else {
                    g_player_progress_sprite.u = 0xB0 - len;
                    g_player_progress_sprite.w = len;
                    g_player_progress_sprite.x -= len;
                }
            } else {
                g_player_progress_sprite.u = 0x88;
                g_player_progress_sprite.w = 0x30;
            }
            GsSortFastSprite(&g_player_progress_sprite, &g_ot_2d[buf], g_player_ot_pri++);
        }
        GsSortSprite(&g_player_progress_frame_sprite, &g_ot_2d[buf], g_player_ot_pri++);
        GsSortFastSprite(&D_800D0E18, &g_ot_2d[buf], g_player_ot_pri++);
        GsSortFastSprite(&D_800D0EE8, &g_ot_2d[buf], g_player_ot_pri++);

        if (g_media_xa_file != 0) {
            g_player_tmp = 0;
            if (g_xa_playing != 0) {
                /* Level meter: the center column shows the audio peak, the others
                 * fall off towards the edges (random flicker when out of range). */
                g_level_meter[7]->level = spu_decoded_peak_level();
                if (g_level_meter[7]->level >= 8) {
                    g_level_meter[7]->level = 7;
                }
                for (g_player_i = 0; g_player_i < 7; g_player_i++) {
                    if (g_level_meter[7]->level - g_player_i > 0) {
                        g_level_meter[6 - g_player_i]->level = g_level_meter[7]->level - g_player_i;
                        g_level_meter[g_player_i + 8]->level = g_level_meter[7]->level - g_player_i;
                    } else if (g_level_meter[7]->level != 0) {
                        g_level_meter[6 - g_player_i]->level = rand() / 8191;
                        g_level_meter[g_player_i + 8]->level = rand() / 8191;
                    } else {
                        g_level_meter[6 - g_player_i]->level = rand() / 16383;
                        g_level_meter[g_player_i + 8]->level = rand() / 16383;
                    }
                }
                for (g_player_i = 0; g_player_i < 15; g_player_i++) {
                    g_player_tmp = g_level_meter[g_player_i]->level >> 1;
                    for (g_player_j = 0; g_player_j < g_level_meter[g_player_i]->level; g_player_j++) {
                        if (g_player_tmp != 0) {
                            if (g_player_j >= g_player_tmp) {
                                g_level_meter_clut_y = 0x49;
                            } else {
                                g_level_meter_clut_y = 0x48;
                            }
                        } else {
                            g_level_meter_clut_y = 0x48;
                        }
                        g_level_meter[g_player_i]->bar[g_player_j].cy = g_level_meter_clut_y;
                        GsSortFastSprite(&g_level_meter[g_player_i]->bar[g_player_j], &g_ot_2d[buf],
                                         g_player_ot_pri++);
                    }
                }
            }

            /* Sort the three menu items by x (draw order). */
            for (g_player_i = 0; g_player_i < 2; g_player_i++) {
                for (g_player_j = g_player_i + 1; g_player_j < 3; g_player_j++) {
                    if (g_player_keyword_sprites[g_player_keyword_order[g_player_j]].x < g_player_keyword_sprites[g_player_keyword_order[g_player_i]].x) {
                        g_player_tmp = g_player_keyword_order[g_player_j];
                        g_player_keyword_order[g_player_j] = g_player_keyword_order[g_player_i];
                        g_player_keyword_order[g_player_i] = g_player_tmp;
                    }
                }
            }
            if (g_player_ending_mode == 0) {
                for (g_player_i = 0; g_player_i < 3; g_player_i++) {
                    GsSortFastSprite(&g_player_keyword_hilite[g_player_keyword_order[g_player_i]], &g_ot_2d[buf], g_player_ot_pri++);
                    GsSortFastSprite(&g_player_keyword_sprites[g_player_keyword_order[g_player_i]], &g_ot_2d[buf], g_player_ot_pri++);
                }
                GsSortLine(&g_player_cursor_lines[0], &g_ot_2d[buf], g_player_ot_pri++);
                GsSortLine(&g_player_cursor_lines[1], &g_ot_2d[buf], g_player_ot_pri++);
            }
            GsSortFastSprite(&g_player_cursor_sprite, &g_ot_2d[buf], g_player_ot_pri++);
            if (++g_player_cursor_frame >= 8) {
                g_player_cursor_frame = 0;
            }
            g_player_cursor_sprite.u = g_player_cursor_frame * 0x18;
            sprite_draw_rotated(&g_slideshow_sprite, &g_ot[buf], 0x15E);
        }

        /* "Play" confirmed while paused: resume the stream. */
        if (g_player_resume_pending == 1 && g_text_window_busy == 0) {
            g_movie_stream_state = 1;
            D_800A5F5E = 0;
            CdRead2(0x1E0);
            g_player_resume_pending = 0;
        }

        if (g_text_window_busy == 0) {
            pad_read_command();
            if (g_pad_command == 0x11) {
                /* Confirm button. */
                if (g_player_ui_mode == 1) {
                    spu_cd_audio_on();
                    if (g_player_ending_mode == 1) {
                        g_player_ending_mode = 2;
                    }
                    if (g_media_xa_file != 0 && g_xa_playing == 0) {
                        snd_play_sfx(0x1A);
                        xa_play_channel(g_media_table[g_media_id].file);
                    } else if (D_800A5F5E == 0 && g_movie_stream_state == 0 && g_media_xa_file == 0) {
                        snd_play_sfx(0x1A);
                        SHOW_LABEL(0xCD, g_str_exit);
                        g_player_ui_mode = 0;
                        g_player_resume_pending = 1;
                    }
                } else if (g_player_ui_mode == 0 || g_player_ui_mode == 2) {
                    if (g_player_ending_mode < 2) {
                        snd_play_sfx(0x1B);
                        g_player_done = 1;
                        g_player_quit = 1;
                        g_player_user_exit = 1;
                        if (g_player_ui_mode == 2) {
                            g_player_keyword_result = g_player_keyword_sel + 1;
                        }
                    }
                }
            }
            if (g_movie_stream_state != 1) {
                if (g_pad_command == 1) {
                    if (g_player_ui_mode <= 0) {
                        SHOW_LABEL(0xAF, g_str_play);
                        g_player_ui_mode = 1;
                    } else if (g_player_ui_mode == 2) {
                        for (g_player_i = 0; g_player_i < 3; g_player_i++) {
                            g_player_keyword_sprites[g_player_i].cy = 0x48;
                            g_player_keyword_hilite[g_player_i].cx = 0x10;
                        }
                        g_player_ui_mode = 3;
                        snd_play_sfx(1);
                    }
                }
                if (g_pad_command == 6) {
                    if (g_player_ui_mode < 2 && g_player_ui_mode != 0) {
                        SHOW_LABEL(0xCD, g_str_exit);
                        g_player_ui_mode = 0;
                    } else if (g_player_ui_mode == 2) {
                        for (g_player_i = 0; g_player_i < 3; g_player_i++) {
                            g_player_keyword_sprites[g_player_i].cy = 0x48;
                            g_player_keyword_hilite[g_player_i].cx = 0x10;
                        }
                        g_player_ui_mode = 4;
                        snd_play_sfx(1);
                    }
                }
            }
            if (g_media_xa_file != 0) {
                if (g_pad_command == 5 && g_player_ending_mode == 0) {
                    g_player_keyword_sprites[g_player_keyword_sel].cy = 0x49;
                    g_player_keyword_hilite[g_player_keyword_sel].cx = 0x20;
                    if (g_player_ui_mode < 2) {
                        prevMode = g_player_ui_mode;
                        g_player_ui_mode = 2;
                    }
                }
                if (g_pad_command == 4 && g_player_ui_mode >= 2) {
                    g_player_keyword_sprites[g_player_keyword_sel].cy = 0x48;
                    g_player_keyword_hilite[g_player_keyword_sel].cx = 0x10;
                    if (g_player_ui_mode == 2) {
                        g_player_ui_mode = prevMode;
                    }
                }
            }
        }

        if (g_media_xa_file != 0) {
            /* Rotate the menu items around the ellipse (mode 3: forward, 4: backward). */
            g_player_tpage = GetTPage(0, 0, 0x1C0, 0);
            if (g_player_ui_mode == 3) {
                for (g_player_i = 0; g_player_i < 3; g_player_i++) {
                    if (++g_player_keyword_pos[g_player_i] >= 30) {
                        g_player_keyword_pos[g_player_i] = 0;
                    }
                    g_player_keyword_hilite[g_player_i].x = g_player_menu_ellipse[g_player_keyword_pos[g_player_i]].x + 6;
                    g_player_keyword_hilite[g_player_i].y = g_player_menu_ellipse[g_player_keyword_pos[g_player_i]].y + 3;
                    gs_sprite_setup(&g_player_keyword_sprites[g_player_i], g_player_menu_ellipse[g_player_keyword_pos[g_player_i]].x,
                                  g_player_menu_ellipse[g_player_keyword_pos[g_player_i]].y, 0x88, 0x10, g_player_tpage, 0, 0x10, 0x1C0,
                                  0x48, 0, 0, 0);
                }
                if (--g_player_cursor_pos < 0) {
                    g_player_cursor_pos = 29;
                }
                g_player_cursor_lines[0].y0 = g_player_menu_ellipse[g_player_cursor_pos].y + 7;
                g_player_cursor_lines[0].y1 = g_player_menu_ellipse[g_player_cursor_pos].y + 7;
                g_player_cursor_lines[1].x0 = g_player_menu_ellipse[g_player_cursor_pos].x + 0x3F;
                g_player_cursor_lines[1].x1 = g_player_menu_ellipse[g_player_cursor_pos].x + 0x3F;
            }
            if (g_player_ui_mode == 4) {
                if (g_player_keyword_pos[0] == -1) {
                    g_player_keyword_pos[0] = 1;
                    g_player_keyword_pos[1] = 11;
                    g_player_cursor_pos = -1;
                    g_player_keyword_pos[2] = 0x15;
                }
                for (g_player_i = 2; g_player_i >= 0; g_player_i--) {
                    if (--g_player_keyword_pos[g_player_i] < 0) {
                        g_player_keyword_pos[g_player_i] = 29;
                    }
                    g_player_keyword_hilite[g_player_i].x = g_player_menu_ellipse[g_player_keyword_pos[g_player_i]].x + 6;
                    g_player_keyword_hilite[g_player_i].y = g_player_menu_ellipse[g_player_keyword_pos[g_player_i]].y + 3;
                    gs_sprite_setup(&g_player_keyword_sprites[g_player_i], g_player_menu_ellipse[g_player_keyword_pos[g_player_i]].x,
                                  g_player_menu_ellipse[g_player_keyword_pos[g_player_i]].y, 0x88, 0x10, g_player_tpage, 0, 0x10, 0x1C0,
                                  0x48, 0, 0, 0);
                }
                if (++g_player_cursor_pos >= 30) {
                    g_player_cursor_pos = 0;
                }
                g_player_cursor_lines[0].y0 = g_player_menu_ellipse[g_player_cursor_pos].y + 7;
                g_player_cursor_lines[0].y1 = g_player_menu_ellipse[g_player_cursor_pos].y + 7;
                g_player_cursor_lines[1].x0 = g_player_menu_ellipse[g_player_cursor_pos].x + 0x3F;
                g_player_cursor_lines[1].x1 = g_player_menu_ellipse[g_player_cursor_pos].x + 0x3F;
            }
            if (g_player_ui_mode >= 3) {
                /* Stop rotating once an item reaches the cursor. */
                for (g_player_i = 0; g_player_i < 3; g_player_i++) {
                    if (g_player_cursor_pos == g_player_keyword_pos[g_player_i]) {
                        if (g_player_ui_loaded != 0) {
                            g_player_ui_mode = 2;
                        }
                        g_player_ui_loaded = 1;
                        g_player_keyword_hilite[g_player_i].cx = 0x20;
                        g_player_keyword_sprites[g_player_i].cy = 0x49;
                        g_player_keyword_sel = g_player_i;
                        break;
                    }
                    g_player_keyword_hilite[g_player_i].cx = 0x10;
                }
            }
            spinner_draw();
        }

        movie_models_update(g_player_ui_mode);
        DrawSync(0);
        GsSortOt(&g_ot_2d[buf], &g_ot[buf]);
        if (g_media_xa_file != 0) {
            GsSortClear(0, 0, 0, &g_ot[buf]);
        }
        GsDrawOt(&g_ot[buf]);
        if (DrawSync(1) != 0) {
            DrawSync(0);
        }
        VSync((g_movie_stream_state != 0 && g_media_xa_file == 0) ? 0 : 5);
        GsSwapDispBuff();
        buf = GsGetActiveBuff();
        g_frame_buffer_index = buf;
    } while (g_player_quit != 1);

    g_player_resume_pending = 0;
    g_movie_stream_state = -1;
    g_player_progress = 0;
    spu_cd_audio_off();
    if (g_player_ending_mode == 2) {
        g_player_done = 1;
    }
    if (g_media_xa_file == 0) {
        if (g_player_quit == 1) {
            movie_stop_stream();
        }
    } else {
        if (CdSync(1, 0) == 0) {
            CdControlF(9, 0);
        }
        g_xa_playing = 0;
        for (g_player_i = 0; g_player_i < 15; g_player_i++) {
            heap_free(g_level_meter[g_player_i]);
        }
        heap_free((void *)g_spu_decode_buf);
        SpuSetIRQ(0);
        SpuSetIRQCallback(0);
        SpuSetTransferCallback(0);
    }
}

/* MDEC DecDCTout callback: uploads the decoded slice to VRAM and queues the next one. */
void movie_dct_out_callback(void) {
    RECT snap;
    s32 unused[2]; /* never used; reserves the 8-byte stack slot the original has */
    s32 id;

    if (g_movie_stream_state == 0) {
        return;
    }
    if (g_movie_rgb24 == 1 && D_801F96E4 != 0) {
        StCdInterrupt();
        D_801F96E4 = 0;
    }
    id = g_movie_decenv.imgid;
    snap = g_movie_decenv.slice;
    g_movie_decenv.imgid = g_movie_decenv.imgid ? 0 : 1;
    g_movie_decenv.slice.x += g_movie_decenv.slice.w;
    if (g_movie_decenv.slice.x < g_movie_decenv.rect[g_movie_decenv.rectid].x + g_movie_decenv.rect[g_movie_decenv.rectid].w) {
        DecDCTout(g_movie_decenv.imgbuf[g_movie_decenv.imgid], g_movie_decenv.slice.w * g_movie_decenv.slice.h / 2);
    } else {
        g_movie_done_rectid = g_movie_decenv.rectid;
        g_movie_decenv.isdone = 1;
        g_movie_decenv.rectid = g_movie_decenv.rectid == 0;
        g_movie_decenv.slice.x = g_movie_decenv.rect[g_movie_decenv.rectid].x;
        g_movie_decenv.slice.y = g_movie_decenv.rect[g_movie_decenv.rectid].y;
    }
    if (g_movie_stream_state < 0) {
        if (g_player_ui_loaded != 0) {
            return;
        }
        snap.y = 0xF0;
        snap.x += 0x140;
        LoadImage(&snap, g_movie_decenv.imgbuf[id]);
    } else {
        snap.y = g_frame_buffer_index * 0xF0;
        LoadImage(&snap, g_movie_decenv.imgbuf[id]);
    }
    DrawSync(0);
}

/* Fetches the next movie frame from the CD stream ring buffer (up to 2000 tries);
 * updates progress/subtitle state and returns the frame data (0 on timeout). */
u32 *movie_get_next_frame(DECENV *dec) {
    u32 *addr;
    StHEADER *header;
    s32 tries = 2000;

    while (StGetNext(&addr, &header) != 0) {
        if (--tries == 0) {
            return 0;
        }
    }
    g_movie_frame_no[dec->rectid] = ((u16 *)addr)[1];
    if (header->frameCount + 1 >= g_media_table[g_media_id].size - 4) {
        g_player_progress = 0x12;
        g_player_result = 2;
        g_player_quit = 1;
    }
    if (g_player_quit == 0 && g_movie_stream_state > 0) {
        g_player_progress = header->frameCount / g_player_progress_step;
    }
    if (g_player_ending_mode == 3) {
        if (g_ending_cues[g_ending_cue_index].end < g_movie_frame_no[dec->rectid]) {
            g_ending_card_shown = 0;
            if (g_ending_cue_index < 0x36) {
                g_ending_cue_index++;
            }
        }
        if (g_ending_cues[g_ending_cue_index].start <= g_movie_frame_no[dec->rectid] && g_ending_cues[g_ending_cue_index].show == 1) {
            g_ending_card_shown = 1;
        }
    }
    dec->rect[0].w = dec->rect[1].w = g_movie_width;
    dec->slice.h = dec->rect[0].h = dec->rect[1].h = g_movie_height;
    return addr;
}

/* Sets the XA channel filter and starts streaming the current movie's audio file,
 * computing its end sector from the file size. */
void xa_play_channel(s16 channel) {
    u8 filter[8];
    u8 loc[8];
    s32 unused[4]; /* never used; reserves the original stack slot */
    s32 start;
    s32 end;

    filter[1] = channel;
    filter[0] = 1;
    CdIntToPos(g_disc_file_table[g_xa_file_ids[g_media_xa_file - 1]].pos, loc);
    CdControl(0xD, filter, 0);
    start = CdPosToInt(loc);
    if (g_media_id >= 0x206) {
        end = start + (g_media_table[g_media_id].size / 2336) * 32;
    } else {
        end = start + ((g_media_table[g_media_id].size - 2.2 * 2336.0) / 2336.0) * 32.0;
    }
    g_player_progress_step = (end - start) / 18;
    if (g_player_progress_step <= 0) {
        g_player_progress_step = 1;
    }
    xa_play_range(start, end);
}

/* Per-frame XA audio monitor: on read error restarts, at end of the audio range stops
 * the CD and ends the movie, otherwise updates the progress counter from the CD position. */
void xa_update(void) {
    u8 result[8];
    s32 unused[2]; /* never used; reserves the 8-byte stack slot the original has */
    s32 status;
    s32 pos;
    s32 progress;

    memset(result, 0, 8);
    if (g_xa_playing == 0) {
        return;
    }
    if (VSync(-1) & 3) {
        return;
    }
    status = CdSync(1, result);
    if (status == 5) {
        xa_start_read(0x1B);
        return;
    }
    if (status != 2) {
        return;
    }
    if (g_xa_cur_sector > g_xa_end_sector || g_xa_cur_sector < g_xa_start_sector - 300) {
        SpuSetIRQ(0);
        spu_cd_audio_off();
        CdControlB(9, 0, 0);
        g_xa_playing = 0;
        g_player_progress = 0;
        g_slideshow_index = 1;
        if (g_player_ending_mode != 2) {
            xa_seek_file();
            slideshow_show_image(0);
        }
        g_slideshow_next_at = 6;
        if (g_slideshow_file2[0] == -1) {
            g_slideshow_next_at = 9;
        }
        g_player_result = 2;
        if (g_player_ending_mode == 2) {
            g_player_quit = 1;
        }
        return;
    }
    if (CdLastCom() == 0x11) {
        pos = CdPosToInt(&result[5]);
        if (pos > 0) {
            g_xa_cur_sector = pos;
        }
    }
    progress = (g_xa_cur_sector - g_xa_start_sector) / g_player_progress_step;
    if (g_player_progress < (u8)progress) {
        g_player_progress = progress;
    }
    CdControlF(0x11, 0);
}

/* Fetches the next frame's VLC data (retrying up to 2000 times) and decodes it into
 * the other VLC buffer; inlined into movie_decode_frame and movie_start_stream. */
static inline s32 func_8001B2A8_nextVlc(DECENV *dec) {
    s32 tries = 2000;
    u32 *next;

    while ((next = movie_get_next_frame(dec)) == 0) {
        if (--tries == 0) {
            return -1;
        }
    }
    dec->vlcid = dec->vlcid ? 0 : 1;
    g_vlc_qscale_luma[0] = ((u8 *)next)[0];
    g_vlc_qscale_chroma[0] = ((u8 *)next)[1];
    g_movie_frame_rl_size = next[1];
    if (g_ending_card_shown == 0) {
        g_vlc_decode_time = g_vsync_counter;
        vlc_decode_frame(next + 1, dec->vlcbuf[dec->vlcid]);
        g_vlc_decode_time = g_vsync_counter - g_vlc_decode_time;
    }
    StFreeRing(next);
    return 0;
}

/* Uploads a TIM image's pixels to `rect`. */
static inline void func_8001B2A8_showTim(u32 *tim, RECT rect) {
    GsGetTimInfo(tim, &g_tim_info);
    LoadImage(&rect, g_tim_info.pixel);
    DrawSync(0);
}

/* Waits for MDEC to finish the frame, forcing a buffer switch on timeout. */
static inline void func_8001B2A8_sync(DECENV *dec) {
    volatile s32 timeout = 0x800000;

    while (dec->isdone == 0) {
        if (--timeout == 0) {
            dec->isdone = 1;
            dec->rectid = dec->rectid ? 0 : 1;
            dec->slice.x = dec->rect[dec->rectid].x;
            dec->slice.y = dec->rect[dec->rectid].y;
        }
    }
    dec->isdone = 0;
}

/* Decodes one movie frame: starts MDEC on the current VLC buffer (or shows the still
 * image g_ending_card_tim), fetches/decodes the next frame's VLC data, then waits for MDEC. */
void movie_decode_frame(void) {
    RECT rect;

    if (g_ending_card_shown == 0) {
        DecDCTin(g_movie_decenv.vlcbuf[g_movie_decenv.vlcid], g_movie_dct_mode);
        DecDCTout(g_movie_decenv.imgbuf[g_movie_decenv.imgid], g_movie_decenv.slice.w * g_movie_decenv.slice.h / 2);
    } else {
        rect.x = 0;
        rect.y = g_frame_buffer_index * 0xF0;
        rect.w = 0x1E0;
        rect.h = 0xF0;
        func_8001B2A8_showTim(g_ending_card_tim + 4, rect);
        g_movie_done_rectid = g_movie_decenv.rectid;
        g_movie_decenv.isdone = 1;
        g_movie_decenv.rectid = g_movie_decenv.rectid ? 0 : 1;
    }
    func_8001B2A8_nextVlc(&g_movie_decenv);
    func_8001B2A8_sync(&g_movie_decenv);
}

/* Allocates the VLC/image buffers and sets up the decode rectangles. */
static inline void func_8001B528_initDecEnv(DECENV *dec) {
    dec->vlcbuf[0] = heap_alloc(0x28000);
    dec->vlcbuf[1] = heap_alloc(0x28000);
    dec->vlcid = 0;
    dec->imgbuf[0] = heap_alloc(g_movie_slice_bytes);
    dec->imgbuf[1] = heap_alloc(g_movie_slice_bytes);
    dec->imgid = 0;
    dec->rect[0].x = 0;
    dec->rect[0].y = 0;
    dec->rect[0].w = 0;
    dec->rect[0].h = 0;
    /* the original writes rect[0] twice; rect[1] is left untouched */
    dec->rect[0].x = 0;
    dec->rect[0].y = 0xF0;
    dec->rect[0].w = 0;
    dec->rect[0].h = 0;
    dec->rectid = 0;
    dec->slice.x = 0;
    dec->slice.y = 0;
    dec->slice.w = g_movie_slice_w;
    dec->slice.h = 0;
    dec->isdone = 0;
}

/* Resets MDEC and starts the CD stream at `loc`. */
static inline void func_8001B528_initStream(CdlLOC *loc, void (*callback)(void)) {
    DecDCTReset(0);
    DecDCToutCallback(callback);
    g_movie_ring_buf = heap_alloc(0x10000);
    StSetRing(g_movie_ring_buf, 0x20);
    StSetStream(g_movie_rgb24, 1, -1, 0, 0);
    movie_cd_seek_read((u8 *)loc);
}

/* Restarts the stream read at `loc` (inlined copy of movie_cd_seek_read). */
static inline void func_8001B528_kickCD(CdlLOC *loc) {
    g_movie_cd_loc = *loc;
    while (1) {
        if (CdControlB(0x16, (u8 *)&g_movie_cd_loc, 0) == 0) {
            continue;
        }
        CdControl(2, (u8 *)&g_movie_cd_loc, 0);
        if (CdRead2(0x1E0) != 0) {
            break;
        }
    }
}

/* Starts movie playback at CD sector `pos`: allocates the MDEC buffers, resets the
 * decoder, sets up the CD stream ring buffer and reads until the first frame decodes. */
void movie_start_stream(s32 pos) {
    CdlLOC *loc = g_movie_start_loc;

    CdIntToPos(pos, (u8 *)loc);
    func_8001B528_initDecEnv(&g_movie_decenv);
    func_8001B528_initStream(loc, movie_dct_out_callback);
    vlc_build_table(g_movie_vlc_table);
    while (func_8001B2A8_nextVlc(&g_movie_decenv) == -1) {
        func_8001B528_kickCD(g_movie_start_loc);
    }
}

/* Matches (0 diffs) once 80018F48..8001D0C8 is split into its own TU, where g_current_site/g_pad_command/g_polytan_parts are declared absolute (NO_GP); in this file they are $gp-relative: 34 diffs. */
/* Inlined copy of spu_cd_audio_on: master and CD volume 0x3FFF. */
static inline void func_8001B77C_spuFull(void) {
    SpuCommonAttr attr;

    attr.mask = 0x2C3;
    attr.mvol.left = 0x3FFF;
    attr.mvol.right = 0x3FFF;
    attr.mvolmode.left = 0x3FFF;
    attr.mvolmode.right = 0x3FFF;
    attr.cd.volume.left = 0x3FFF;
    attr.cd.volume.right = 0x3FFF;
    attr.cd.mix = 1;
    SpuSetCommonAttr(&attr);
}

/* Inlined copy of spu_cd_audio_off: master volume 0x4FFF, CD audio muted. */
static inline void func_8001B77C_spuCdMute(void) {
    SpuCommonAttr attr;

    attr.mask = 0x2C3;
    attr.mvol.left = 0x4FFF;
    attr.mvol.right = 0x4FFF;
    attr.mvolmode.left = 0x4FFF;
    attr.mvolmode.right = 0x4FFF;
    attr.cd.volume.left = 0;
    attr.cd.volume.right = 0;
    attr.cd.mix = 1;
    SpuSetCommonAttr(&attr);
}

/* Plays a randomly chosen movie (the pool depends on g_polytan_parts/g_current_site) until it
 * ends or the user presses the skip button. */
void idle_play_random_movie(void) {
    RECT rect;

    g_player_quit = 0;
    g_player_ending_mode = 0;
    g_ending_card_shown = 0;
    if (g_polytan_parts != 0x3F) {
        if (g_current_site != 0) {
            g_media_id = rand() / 4681 + 0x2D4;
        } else {
            g_media_id = rand() / 1489 + 0x2C5;
        }
    } else if (g_current_site == 1) {
        g_media_id = rand() / 4681 + 0x2D4;
        if (rand() / 10922 == 1) {
            g_media_id = rand() % 2 + 0x2DB;
        }
    } else {
        g_media_id = rand() / 1489 + 0x2C5;
    }
    if (g_media_id > g_current_site * 2 + 0x2DA) {
        g_media_id = g_current_site * 2 + 0x2DA;
    }
    srand(rand());
    g_frame_buffer_index = GsGetActiveBuff();
    g_player_progress = 0;
    g_player_progress_step = 1;
    g_movie_stream_state = 1;
    func_8001B77C_spuFull();
    movie_start_stream(g_disc_file_table[g_media_table[g_media_id].file].pos);
    do {
        movie_decode_frame();
        VSync(0);
        DrawSync(0);
        GsSwapDispBuff();
        g_frame_buffer_index = GsGetActiveBuff();
        pad_read_command();
        if (g_pad_command == 0x11) {
            g_player_quit = 1;
        }
    } while (g_player_quit != 1);
    movie_stop_stream();
    func_8001B77C_spuCdMute();
    VSync(0);
    rect.x = 0;
    rect.y = 0;
    rect.w = 0x140;
    rect.h = 0x1E0;
    ClearImage(&rect, 0, 0, 0);
    DrawSync(0);
}

/* Seeks the CD to the current track's XA audio file. */
static inline void func_8001BA60_seek(void) {
    CdlLOC loc;

    CdIntToPos(g_disc_file_table[g_xa_file_ids[g_media_xa_file - 1]].pos, (u8 *)&loc);
    CdControl(2, (u8 *)&loc, 0);
    CdControl(0x16, (u8 *)&loc, 0);
}

/* Plays a random music track (XA audio) with the image slideshow until it ends or the
 * user presses the skip button. */
void idle_play_random_voice(void) {
    RECT unused; /* never used; reserves the original stack slot */

    g_player_quit = 0;
    g_player_ending_mode = 2;
    g_xa_playing = 0;
    g_media_id = g_idle_voice_base[g_current_site] + rand() / 3640;
    srand(rand());
    g_slideshow_next_at = 6;
    g_slideshow_index = 1;
    g_media_xa_file = g_media_table[g_media_id].xa_file;
    g_frame_buffer_index = GsGetActiveBuff();
    D_800A5F7A = 0;
    g_player_progress = 0;
    g_player_progress_step = 1;
    func_8001B77C_spuFull();
    g_slideshow_file0[0] = g_idle_voice_slideshow[g_current_site][g_media_id - g_idle_voice_base[g_current_site]].a;
    g_slideshow_file1[0] = g_idle_voice_slideshow[g_current_site][g_media_id - g_idle_voice_base[g_current_site]].b;
    g_slideshow_file2[0] = g_idle_voice_slideshow[g_current_site][g_media_id - g_idle_voice_base[g_current_site]].c;
    slideshow_load_images();
    func_8001BA60_seek();
    xa_play_channel(g_media_table[g_media_id].file);
    g_player_user_exit = 0;
    do {
        GsSetRefView2(&g_view);
        GsSetWorkBase(g_gs_packet_area + g_frame_buffer_index * 36000);
        GsClearOt(0, 0, &g_ot[g_frame_buffer_index]);
        slideshow_update();
        spinner_draw();
        sprite_draw_rotated(&g_slideshow_sprite, &g_ot[g_frame_buffer_index], 0x15E);
        GsSortClear(0, 0, 0, &g_ot[g_frame_buffer_index]);
        GsDrawOt(&g_ot[g_frame_buffer_index]);
        VSync(5);
        GsSwapDispBuff();
        g_frame_buffer_index = GsGetActiveBuff();
        pad_read_command();
        if (g_pad_command == 0x11) {
            g_player_user_exit = 1;
            g_player_quit = 1;
        }
    } while (g_player_quit != 1);
    g_xa_playing = 0;
    g_player_ending_mode = 0;
    __asm__ volatile(""); /* FAKE MATCH: scheduling barrier keeping the two stores ahead of the SPU setup; find the real source shape */
    func_8001B77C_spuCdMute();
}

/* Inlined copy of site_load_file with language 1: loads file `index` of table D_800A689C to `dest`. */
static inline void func_8001BE20_loadFile(s32 index, void *dest) {
    u16 sizes[2] = { 6, 6 };
    s32 id;

    id = cd_load_archive_entry(sizes[1], index, D_800A689C, dest);
    while (cd_poll_load(id) == 0) {
    }
}

/* Inlined copy of movie_stop_stream: stops movie streaming and frees its buffers. */
static inline void func_8001BE20_stopMovie(void) {
    DecDCToutCallback(0);
    StUnSetRing();
    if (g_movie_stream_state == 1) {
        if (CdSync(1, 0) == 0) {
            CdControlF(9, 0);
        }
        g_movie_stream_state = -1;
    }
    heap_free(g_movie_ring_buf);
    heap_free(g_movie_decenv.vlcbuf[0]);
    heap_free(g_movie_decenv.vlcbuf[1]);
    heap_free(g_movie_decenv.imgbuf[0]);
    heap_free(g_movie_decenv.imgbuf[1]);
}

/* Plays the ending movie (file 0x2DF) in 24-bit mode over a still image, marks which
 * subtitle cues to show, then restores the normal 16-bit display mode. */
void ending_movie_play(void) {
    RECT rect;
    s32 i;
    s32 cue;

    VSync(0);
    rect.x = 0;
    rect.y = 0;
    rect.w = 0x1E0;
    rect.h = 0x1E0;
    ClearImage(&rect, 0, 0, 0);
    DrawSync(0);
    GsInitGraph2(0x140, 0xF0, 4, 0, 1);
    D_800A5F77 = 0;
    g_movie_done_rectid = 0;
    cue = 0;
    for (i = 0; i < 0x2CC; i++) {
        if (g_node_table[i].u.bits.type == 4) {
            if (g_node_table[i].u.bits.seen == 1) {
                g_ending_cues[cue].show = 0;
            } else {
                g_ending_cues[cue].show = 1;
            }
            cue++;
        }
    }
    g_movie_dct_mode = 3;
    g_movie_rgb24 = 1;
    g_player_ending_mode = 3;
    g_movie_slice_bytes = 0x2D00;
    g_movie_slice_w = 0x18;
    g_movie_width = g_movie_width * 3 / 2;
    g_ending_card_shown = 0;
    g_player_quit = 0;
    g_media_id = 0x2DF;
    g_frame_buffer_index = GsGetActiveBuff();
    g_player_progress = 0;
    g_player_progress_step = 1;
    g_movie_stream_state = 1;
    g_ending_cue_index = 0;
    g_xa_playing = 0;
    func_8001B77C_spuFull();
    D_800A68C8 = 0;
    while (g_frame_buffer_index != 0) {
        GsSwapDispBuff();
        g_frame_buffer_index = GsGetActiveBuff();
    }
    g_player_load_buf = heap_alloc(D_800A689C[0x22A].size);
    func_8001BE20_loadFile(0x22A, g_player_load_buf);
    g_ending_card_tim = lz_decompress(g_player_load_buf, D_800A689C[0x22A].size);
    heap_free(g_player_load_buf);
    movie_start_stream(g_disc_file_table[g_media_table[g_media_id].file].pos);
    do {
        movie_decode_frame();
        DrawSync(0);
        VSync(0);
        GsSwapDispBuff();
        g_frame_buffer_index = GsGetActiveBuff();
        D_800A5F77 = 0;
    } while (g_player_quit != 1);
    heap_free(g_ending_card_tim);
    g_movie_stream_state = 1;
    func_8001BE20_stopMovie();
    g_movie_width = 0x140;
    g_movie_dct_mode = 2;
    g_movie_slice_bytes = 0x1E00;
    g_ending_cue_index = 0;
    g_movie_rgb24 = 0;
    g_movie_slice_w = 0x10;
    GsInitGraph2(0x140, 0xF0, 4, 1, 0);
    DrawSync(0);
    VSync(0);
    GsSwapDispBuff();
    rect.x = 0;
    rect.y = 0;
    rect.w = 0x280;
    rect.h = 0x1E0;
    ClearImage(&rect, 0, 0, 0);
    DrawSync(0);
}

/* Centers image `index` (size table g_slideshow_sizes) on the 320x240 screen and picks its clut/tpage. */
void slideshow_show_image(s16 index) {
    ImageSprite *sprite = &g_slideshow_sprite;

    sprite->x = (0x140 - g_slideshow_sizes[index].w) / 2;
    sprite->y = (0xF0 - g_slideshow_sizes[index].h) / 2;
    sprite->clut = GetClut(0x140, index + 0xC9);
    sprite->v = 0;
    if (index == 0) {
        sprite->tpage = GetTPage(1, 0, 0x140, 0);
        sprite->u = 0;
    } else if (index == 2) {
        sprite->tpage = GetTPage(1, 0, 0x1C0, 0x100);
        sprite->u = 0;
    } else {
        sprite->tpage = GetTPage(1, 0, 0x140, 0x100);
        sprite->u = 0;
    }
    g_slideshow_sprite.w = g_slideshow_sizes[index].w;
    g_slideshow_sprite.h = g_slideshow_sizes[index].h;
    g_player_cursor_sprite.x = g_slideshow_sprite.x + 5;
    g_player_cursor_sprite.y = g_slideshow_sprite.y + 5;
}

/* Inlined copy of site_load_file: loads file `index` of the current language's table. */
static inline void func_8001C314_loadFile(s32 index, void *dest) {
    u16 sizes[2] = { 6, 6 };
    s32 id;

    id = cd_load_archive_entry(sizes[g_current_site], index, g_site_file_tables[g_current_site], dest);
    while (cd_poll_load(id) == 0) {
        loading_anim_draw(0);
    }
}

/* Uploads a TIM's pixels to (x, y) (then shifts it 3 pixels up-left) and its clut to (0x140, 0xC9 + n). */
static inline void func_8001C314_uploadTim(u32 *tim, s32 x, s32 y, s32 n) {
    RECT rect;

    GsGetTimInfo(tim, &g_tim_info);
    rect.x = x;
    rect.y = y;
    rect.w = g_tim_info.pw;
    rect.h = g_tim_info.ph;
    LoadImage(&rect, g_tim_info.pixel);
    DrawSync(0);
    rect.x = x + 3;
    rect.y = y + 3;
    rect.w = g_tim_info.pw - 3;
    rect.h = g_tim_info.ph - 3;
    MoveImage(&rect, x, y);
    DrawSync(0);
    if ((g_tim_info.pmode >> 3) & 1) {
        rect.x = 0x140;
        rect.y = 0xC9 + n;
        rect.w = g_tim_info.cw;
        rect.h = g_tim_info.ch;
        LoadImage(&rect, g_tim_info.clut);
        DrawSync(0);
    }
}

/* Loads the three slideshow images of the current track (file indices g_slideshow_file0/C2/C4,
 * -1 = none) into VRAM and sets up the image sprite and the text-cursor sprite. */
void slideshow_load_images(void) {
    s32 i;
    s32 none;
    u32 *tim;
    s32 x;
    s32 y;
    u16 tpage;
    u16 clut;
    s32 x2;
    s32 y2;

    none = -1;
    for (i = 2; i >= 0; i--) {
        g_slideshow_sizes[i].w = none;
    }

    g_player_load_buf = heap_alloc(g_site_file_tables[g_current_site][g_slideshow_file0[0]].size);
    func_8001C314_loadFile(g_slideshow_file0[0], g_player_load_buf);
    tim = lz_decompress(g_player_load_buf, g_site_file_tables[g_current_site][g_slideshow_file0[0]].size);
    tim++;
    func_8001C314_uploadTim(tim, 0x140, 0, 0);
    heap_free(tim - 1);
    heap_free(g_player_load_buf);
    g_slideshow_sizes[0].w = g_tim_info.pw * 2 - 0x11;
    x = (0x140 - g_slideshow_sizes[0].w) / 2;
    g_slideshow_sizes[0].h = g_tim_info.ph - 3;
    y = (0xF0 - g_slideshow_sizes[0].h) / 2;
    tpage = GetTPage(1, 0, 0x140, 0);
    clut = GetClut(0x140, 0xC9);
    g_slideshow_sprite.x = x;
    g_slideshow_sprite.y = y;
    g_slideshow_sprite.tpage = tpage;
    g_slideshow_sprite.u = 0;
    g_slideshow_sprite.v = 0;
    g_slideshow_sprite.clut = clut;
    g_slideshow_sprite.r = 0x80;
    g_slideshow_sprite.g = 0x80;
    g_slideshow_sprite.b = 0x80;
    g_slideshow_sprite.attribute = 0x1000040;
    g_slideshow_sprite.scaleX = 0x1000;
    g_slideshow_sprite.scaleY = 0x1000;
    g_slideshow_sprite.w = g_slideshow_sizes[0].w;
    g_slideshow_sprite.h = g_slideshow_sizes[0].h;
    x2 = (0x140 - g_slideshow_sizes[0].w) / 2 + 5;
    y2 = (0xF0 - g_slideshow_sizes[0].h) / 2 + 5;
    g_player_cursor_sprite.tpage = GetTPage(0, 0, 0x1C0, 0);
    g_player_cursor_sprite.x = x2;
    g_player_cursor_sprite.y = y2;
    g_player_cursor_sprite.w = 0x18;
    g_player_cursor_sprite.v = 0x30;
    g_player_cursor_sprite.cx = 0x1C0;
    g_player_cursor_sprite.h = 0x18;
    g_player_cursor_sprite.u = 0;
    g_player_cursor_sprite.cy = 0x4B;
    g_player_cursor_sprite.r = 0x80;
    g_player_cursor_sprite.g = 0x80;
    g_player_cursor_sprite.b = 0x80;
    g_player_cursor_sprite.mx = 0;
    g_player_cursor_sprite.my = 0;
    g_player_cursor_sprite.attribute = 0;

    if (g_slideshow_file1[0] != -1) {
        u32 *tim;

        g_player_load_buf = heap_alloc(g_site_file_tables[g_current_site][g_slideshow_file1[0]].size);
        func_8001C314_loadFile(g_slideshow_file1[0], g_player_load_buf);
        tim = lz_decompress(g_player_load_buf, g_site_file_tables[g_current_site][g_slideshow_file1[0]].size);
        tim++;
        func_8001C314_uploadTim(tim, 0x140, 0x100, 1);
        heap_free(tim - 1);
        g_slideshow_sizes[1].w = g_tim_info.pw * 2 - 0x11;
        g_slideshow_sizes[1].h = g_tim_info.ph - 3;
        heap_free(g_player_load_buf);
    }
    if (g_slideshow_file2[0] != -1) {
        u32 *tim;

        g_player_load_buf = heap_alloc(g_site_file_tables[g_current_site][g_slideshow_file2[0]].size);
        func_8001C314_loadFile(g_slideshow_file2[0], g_player_load_buf);
        tim = lz_decompress(g_player_load_buf, g_site_file_tables[g_current_site][g_slideshow_file2[0]].size);
        tim++;
        func_8001C314_uploadTim(tim, 0x1C0, 0x100, 2);
        heap_free(tim - 1);
        heap_free(g_player_load_buf);
        g_slideshow_sizes[2].w = g_tim_info.pw * 2 - 0x11;
        g_slideshow_sizes[2].h = g_tim_info.ph - 3;
    }
}

/* Image slideshow state machine: fade in, show next image, fade out. */
void slideshow_update(void) {
    if (g_player_progress < g_slideshow_next_at) {
        return;
    }
    switch (g_slideshow_state) {
    case 0:
        if (g_slideshow_sprite.scaleY != 0 && D_800A5F7A == 0) {
            g_slideshow_sprite.scaleY -= 0x400;
            return;
        }
        g_slideshow_state++;
        break;
    case 1:
        if (g_media_xa_file == 0) {
            return;
        }
        if ((u8)(g_slideshow_next_at / 3) == 4) {
            slideshow_show_image(2);
        } else {
            slideshow_show_image(1);
        }
        g_slideshow_state++;
        break;
    case 2:
        if (g_slideshow_sprite.scaleY < 0x1000 && D_800A5F7A == 0) {
            g_slideshow_sprite.scaleY += 0x400;
            return;
        }
        g_slideshow_state = 0;
        g_slideshow_next_at *= 2;
        g_slideshow_index++;
        break;
    }
}

/* Seeks to `loc` and starts a streaming read, retrying until both succeed. */
void movie_cd_seek_read(u8 *loc) {
    while (1) {
        if (CdControlB(0x16, loc, 0) == 0) {
            continue;
        }
        CdControl(2, loc, 0);
        if (CdRead2(0x1E0) != 0) {
            break;
        }
    }
}

void spu_irq_read_decoded(void) {
    SpuSetIRQ(0);
    SpuReadDecodedData(g_spu_decode_buf, g_spu_decode_flag);
}

/* Toggles the SPU IRQ address between 0 and 0x200 (decoded-data half buffers). */
void spu_decoded_xfer_done(void) {
    if (g_spu_irq_addr == 0) {
        g_spu_irq_addr = 0x200;
    } else {
        g_spu_irq_addr = 0;
    }
    SpuSetIRQAddr(g_spu_irq_addr);
    SpuSetIRQ(1);
}

/* Stops movie streaming and frees its buffers. */
void movie_stop_stream(void) {
    DecDCToutCallback(0);
    StUnSetRing();
    if (g_movie_stream_state == 1) {
        if (CdSync(1, 0) == 0) {
            CdControlF(9, 0);
        }
        g_movie_stream_state = -1;
    }
    heap_free(g_movie_ring_buf);
    heap_free(g_movie_decenv.vlcbuf[0]);
    heap_free(g_movie_decenv.vlcbuf[1]);
    heap_free(g_movie_decenv.imgbuf[0]);
    heap_free(g_movie_decenv.imgbuf[1]);
}

void idle_play_random_media(void) {
    SpuCommonAttr attr;

    g_site_file_tables[0] = g_site_a_file_table;
    D_800A689C = g_site_b_file_table;
    attr.mask = 0x2C3;
    attr.mvol.left = 0x4FFF;
    attr.mvol.right = 0x4FFF;
    attr.mvolmode.left = 0x4FFF;
    attr.mvolmode.right = 0x4FFF;
    attr.cd.volume.left = 0;
    attr.cd.volume.right = 0;
    attr.cd.mix = 1;
    SpuSetCommonAttr(&attr);
    if (!(rand() & 1)) {
        idle_play_random_movie();
    } else {
        spinner_init(g_player_empty_str, 0x15F);
        idle_play_random_voice();
        spinner_free();
    }
}

s32 xa_start_read(s32 unused) {
    u8 loc[4];

    CdIntToPos(g_xa_start_sector, loc);
    CdControl(0x1B, 0, 0);
    g_xa_playing = 1;
    g_xa_cur_sector = g_xa_start_sector;
    return 0;
}

/* Returns the byte size of file `index` in an archive file table. */
s32 file_table_get_size(FileEntry *table, s32 index) {
    return table[index].size;
}

/* Loads a TIM (file 0x15 of table g_bin_file_table), decompresses it and uploads pixels to (x, y) and its clut to (0x1C0, clutY + 0x47). */
void player_load_ui_tim(s32 unused, s16 x, s16 y, s16 clutY) {
    RECT rect;
    void *buf;
    u32 *tim;
    s32 id;

    buf = heap_alloc(g_bin_file_table[0x15].size);
    id = cd_load_archive_entry(3, 0x15, g_bin_file_table, buf);
    while (cd_poll_load(id) == 0) {
    }
    tim = lz_decompress(buf, g_bin_file_table[0x15].size);
    heap_free(buf);
    tim++;
    GsGetTimInfo(tim, &g_tim_info);
    rect.x = x;
    rect.y = y;
    rect.w = g_tim_info.pw;
    rect.h = g_tim_info.ph;
    LoadImage(&rect, g_tim_info.pixel);
    DrawSync(0);
    if ((g_tim_info.pmode >> 3) & 1) {
        rect.x = 0x1C0;
        rect.y = clutY + 0x47;
        rect.w = g_tim_info.cw;
        rect.h = g_tim_info.ch;
        LoadImage(&rect, g_tim_info.clut);
        DrawSync(0);
    }
    heap_free(tim - 1);
}

/* g_site_file_tables/D_800A689C are one 2-entry table (per-language file tables); indexing it as an
 * array (not (&g_site_file_tables)[i]) is what gets the index scaled before the address load. */
void site_load_file(s32 index, void *dest) {
    u16 sizes[2] = { 6, 6 };
    s32 id;

    id = cd_load_archive_entry(sizes[g_current_site], index, g_site_file_tables[g_current_site], dest);
    while (cd_poll_load(id) == 0) {
        loading_anim_draw(0);
    }
}

void site_b_load_file(s32 index, void *dest) {
    u16 sizes[2] = { 6, 6 };
    s32 id;

    id = cd_load_archive_entry(sizes[0], index, D_800A689C, dest);
    while (cd_poll_load(id) == 0) {
    }
}

s32 xa_play_range(s32 pos, s32 arg1) {
    u8 param[8];

    g_xa_start_sector = pos;
    g_xa_cur_sector = pos;
    g_xa_end_sector = arg1;
    param[0] = 0xC8;
    CdControlB(0xE, param, 0);
    xa_start_read(0x1B);
    if (g_media_id < 0x206) {
        SpuSetIRQ(1);
    }
    return 0;
}

/* Sets SPU master volume to 0x4FFF with CD audio muted. */
void spu_cd_audio_off(void) {
    SpuCommonAttr attr;

    attr.mask = 0x2C3;
    attr.mvol.left = 0x4FFF;
    attr.mvol.right = 0x4FFF;
    attr.mvolmode.left = 0x4FFF;
    attr.mvolmode.right = 0x4FFF;
    attr.cd.volume.left = 0;
    attr.cd.volume.right = 0;
    attr.cd.mix = 1;
    SpuSetCommonAttr(&attr);
}

/* Sets SPU master volume and CD audio volume to 0x3FFF. */
void spu_cd_audio_on(void) {
    SpuCommonAttr attr;

    attr.mask = 0x2C3;
    attr.mvol.left = 0x3FFF;
    attr.mvol.right = 0x3FFF;
    attr.mvolmode.left = 0x3FFF;
    attr.mvolmode.right = 0x3FFF;
    attr.cd.volume.left = 0x3FFF;
    attr.cd.volume.right = 0x3FFF;
    attr.cd.mix = 1;
    SpuSetCommonAttr(&attr);
}
