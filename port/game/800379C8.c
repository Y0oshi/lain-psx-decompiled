#include "common.h"
/* port: PsyQ types, macros and prototypes come from psx_sdk.h (port/psx headers). */

/* PsyQ library types */














/* PsyQ library / libc functions */






/* Game types */

/* Accesses to some globals are absolute (lui/%lo) rather than $gp-relative in
 * this part of the original code; declaring them in .data reproduces that. */
#define NO_GP 

#define HEAP_FREE 0x46726565 /* 'Free' */
#define HEAP_USED 0x55736521 /* 'Use!' */

/* Heap block header; the user pointer follows it. */
typedef struct HeapBlock {
    u32 magic;
    s32 size;
    struct HeapBlock *next;
    struct HeapBlock *prev;
} HeapBlock;

/* File table entry. */
typedef struct {
    s32 sector;
    s32 size;
} FileEntry;

typedef struct {
    u8 xa_file;
    u8 pad1;
    s16 file;
    s32 size;
} MovieInfo;

typedef struct {
    s32 x;
    s32 y;
} Point;

/* A 2D sprite drawn as a textured quad (see sprite_draw). */
typedef struct {
    POLY_FT4 poly; /* 0x00 */
    s32 attr;      /* 0x28: 0x40000000 semi-trans, 0x800000 flip U, 0x400000 flip V */
    s16 tpage;     /* 0x2C */
    s16 clut;      /* 0x2E */
    s16 scaleX;    /* 0x30 */
    s16 scaleY;    /* 0x32 */
    s16 x;         /* 0x34 */
    s16 y;         /* 0x36 */
    s16 w;         /* 0x38 */
    s16 h;         /* 0x3A */
    s16 rotX;     /* 0x3C */
    s16 rotY;     /* 0x3E */
    s16 rotZ;     /* 0x40 */
    s16 unk42;     /* 0x42 */
    u8 u;          /* 0x44 */
    u8 v;          /* 0x45 */
    u8 r;          /* 0x46 */
    u8 g;          /* 0x47 */
    u8 b;          /* 0x48 */
    u8 pad49[3];
} SpriteEntry; /* size 0x4C */

/* Sprite layout entry (g_sskn_sprite_defs). */
typedef struct {
    s16 x, y;
    u16 w, h;
    u16 tpage;
    u8 u, v;
    s16 cx, cy;
} SpriteDef;

/* Game globals */

/* Heap */
extern HeapBlock *g_heap_start;     /* heap start */
extern s32 g_heap_size;            /* heap size */
extern s32 g_heap_peak;            /* peak bytes in use */
extern s32 g_heap_used;            /* bytes in use */
extern HeapBlock *g_heap_rover;     /* first free block (rover) */
char *g_lz_magic; /* port: defined here (host-typed) */          /* compressed-file magic */

/* Files / graphics */
extern FileEntry g_bin_file_table[];
extern FileEntry g_site_a_file_table[];
extern FileEntry g_site_b_file_table[];
extern s32 g_frame_buffer_index NO_GP;      /* current double-buffer index */
extern GsOT g_ot_2d[2];
extern GsOT g_ot[];

/* Background curves (16 animated gouraud lines) */
Point *g_bg_curve_history[16]; /* port: defined here (host-typed) */     /* per-curve point history */
GsGLINE *g_bg_curve_lines[16]; /* port: defined here (host-typed) */   /* per-curve line primitives */
extern s32 g_bg_curve_ctrl[][4];       /* per-curve control points */
extern u16 g_bg_curve_t[16];        /* curve parameter t (0..255) */
extern u16 g_bg_curve_mirror[16];        /* mirror flag */
extern u16 g_bg_curve_head[16];        /* history write index */
extern u16 g_bg_curve_len[16];        /* history length (saturates at 255) */
extern u16 g_bg_curve_origin[16][2];     /* curve origin */
extern Point *g_bg_curve_polyline;         /* scratch polyline */
extern u16 g_bg_curve_polyline_len;            /* scratch polyline length */

/* Title screen / dialogs */
extern GsSPRITE g_gate_sprites[];     /* title logo sprites; [5] = "press ANY button" */
extern u32 g_gate_clut[];
extern SpriteDef g_sskn_sprite_defs[];    /* yes/no dialog sprite layout */
extern u16 g_gate_obj_screen_x;            /* logo centre x */
extern u16 g_gate_obj_screen_y;            /* logo centre y */
extern s16 g_gate_fly_pending;
extern s16 g_gate_level NO_GP;
extern s16 g_pad_command NO_GP;      /* pad command */

/* Movie/intro sequence */
/* VRAM blit rect (x, y, w, h) shared by the loading-icon code; the original
 * accesses the fields through $gp by their own symbols. */
extern RECT g_vram_move_rect;
extern s16 g_vram_move_rect_y;
extern s16 g_vram_move_rect_w;
extern s16 g_vram_move_rect_h;
#define ICON_RECT (&g_vram_move_rect)
extern u8 g_voice_icon_frames[7];
extern u8 g_audio_node_is_ending;
extern s16 g_audio_node_timer;
extern s16 g_audio_node_icon_step;
extern u8 g_voice_mora_count;
extern s16 g_audio_node_state;            /* movie/intro sequence state */
extern u32 g_voice_spu_addr;            /* SPU address of the jingle */
extern u8 g_voice_name_frames;
extern s16 g_audio_level;
extern u32 *g_audio_node_file_buf;           /* scratch file buffer */
extern s32 g_voice_adpcm_size;            /* encoded jingle size */
extern u8 g_audio_icon_frames[];
extern s32 g_lain_anim_frame[];
extern MovieInfo g_media_table[];
extern GsSPRITE g_slideshow_sprite; /* port: the host object is 80018F48.c's ImageSprite; this GsSPRITE view overlays its start, as on the PS1 */
extern u8 g_player_name[];
extern s16 g_current_site[1] NO_GP;   /* language (0/1) */
extern s32 g_media_id NO_GP;      /* current movie index */
extern u8 g_media_xa_file NO_GP;
extern u8 g_player_ending_mode NO_GP;
extern s16 g_spu_decode_flag NO_GP;
extern volatile s16 g_player_quit NO_GP;
extern void *g_spu_decode_buf NO_GP;
extern volatile s32 g_spu_irq_addr NO_GP;
extern u8 g_ring_models_done NO_GP;

/* CD drive watchdog */
extern s16 g_cd_watch_inited;
extern s16 g_cd_last_shell_open;
extern s16 g_cd_idle_timeout;
extern s16 g_cd_error_timeout;

/* Game functions */

extern s32 cd_load_archive_entry(s32 mode, s32 index, FileEntry *table, void *buf);
/* port: exact prototype of the definition (8001D114.c), not all-s32 */
extern void gs_sprite_setup(GsSPRITE *sprite, s16 x, s16 y, u16 w, u16 h, u16 tpage, u8 u, u8 v,
                          s16 cx, s16 cy, u32 attribute, s16 mx, s16 my);
extern void sprite_draw(SpriteEntry *entry, GsOT *ot, u16 pri);
extern void sprite_draw_rotated(SpriteEntry *entry, GsOT *ot, u16 pri);


/* Load language file `name` (directory 6) into `buf` and wait for it. */
static inline void LoadLangFile(u32 *buf, s32 name) {
    u16 dirs[2] = { 6, 6 };
    FileEntry *tables[2] = { g_site_a_file_table, g_site_b_file_table };
    s32 id;

    g_audio_node_file_buf = buf;
    id = cd_load_archive_entry(dirs[g_current_site[0]], name, tables[g_current_site[0]], buf);
    while (cd_poll_load(id) == 0) {
    }
}

/* Upload the TIM in g_audio_node_file_buf to VRAM at (x, y) and free the buffer. */
static inline void UploadTim(s32 x, s32 y) {
    RECT rect;
    GsIMAGE image;

    GsGetTimInfo(g_audio_node_file_buf + 1, &image);
    rect.x = x;
    rect.y = y;
    rect.w = image.pw;
    rect.h = image.ph;
    LoadImage(&rect, image.pixel);
    DrawSync(0);
    heap_free(g_audio_node_file_buf);
}

/* Movie/intro sequence: encode and upload the jingle to SPU RAM, load the
 * language-specific "now loading" graphics, then run audio_node_update each frame
 * until the sequence finishes, and restore VRAM. */
void audio_node_play(void) {
    EncSPUEnv enc;
    s16 work[0x54];
    SpuVoiceAttr attr;
    u16 files[5][2] = { { 0x312, 0x227 }, { 0x315, 0x22B }, { 0x314, 0x229 }, { 0x13, 0x0D }, { 0x311, 0x226 } };
    s32 size;
    u8 *raw;
    u8 *encoded;

    g_audio_node_is_ending = 0;
    if (g_media_id == 0x2DD) {
        ring_models_init();
        g_audio_node_is_ending = 1;
    }
    if (g_media_id < 0x27F || g_audio_node_is_ending == 1) {
        raw = voice_synth_romaji(g_player_name, &size, &g_voice_mora_count);
        g_voice_name_frames = g_voice_mora_count * 18;
        if (g_voice_mora_count >= 6) {
            g_voice_mora_count = 6;
        }
        encoded = heap_alloc(size);
        if (encoded == NULL) {
            return;
        }
        enc.src = (s16 *)raw;
        enc.dest = (s16 *)encoded;
        enc.loop = 0;
        enc.byte_swap = 0;
        enc.proceed = 0;
        enc.work = work;
        enc.size = size;
        g_voice_adpcm_size = EncSPU(&enc);
        if (g_voice_adpcm_size == -1) {
            return;
        }
        heap_free(raw);
        g_voice_spu_addr = SpuMalloc(g_voice_adpcm_size);
        SpuSetTransferStartAddr(g_voice_spu_addr);
        SpuRead(encoded, g_voice_adpcm_size);
        heap_free(encoded);
        attr.mask = 0xFF93;
        attr.voice = 0x800000;
        attr.volume.left = 0x32C8;
        attr.volume.right = 0x32C8;
        attr.pitch = 0x2DF;
        attr.a_mode = 1;
        attr.s_mode = 1;
        attr.r_mode = 3;
        attr.ar = 0;
        attr.dr = 0;
        attr.sr = 0;
        attr.rr = 0;
        attr.sl = 0xF;
        attr.addr = g_voice_spu_addr;
        SpuSetVoiceAttr(&attr);
    }
    g_vram_move_rect.x = 0;
    g_vram_move_rect_y = 0;
    g_vram_move_rect_w = 0x280;
    g_vram_move_rect_h = 0x1E0;
    ClearImage(ICON_RECT, 0, 0, 0);
    DrawSync(0);
    g_vram_move_rect.x = 0x140;
    g_vram_move_rect_y = 0x1E0;
    g_vram_move_rect_w = 0x80;
    g_vram_move_rect_h = 0x20;
    MoveImage(ICON_RECT, 0x280, 0x100);
    DrawSync(0);
    g_vram_move_rect.x = 0x1C0;
    g_vram_move_rect_y = 0x1E0;
    g_vram_move_rect_w = 0x48;
    g_vram_move_rect_h = 0x20;
    MoveImage(ICON_RECT, 0x280, 0x120);
    DrawSync(0);

    LoadLangFile(heap_alloc(0x1E014), files[0][g_current_site[0]]);
    UploadTim(0x140, 0x100);
    LoadLangFile(heap_alloc(0x154), files[1][g_current_site[0]]);
    UploadTim(0x240, 0x100);
    LoadLangFile(heap_alloc(0x174), files[2][g_current_site[0]]);
    UploadTim(0x254, 0x100);
    LoadLangFile(heap_alloc(0x20C), files[3][g_current_site[0]]);
    UploadTim(0x26A, 0x100);
    LoadLangFile(heap_alloc(0x2D4), files[4][g_current_site[0]]);
    UploadTim(0x240, 0x108);
    DrawSync(0);

    gs_sprite_setup(&g_slideshow_sprite, 0x20, 0, 0x100, 0xF0, GetTPage(2, 1, 0x140, 0x100), 0, 0, 0, 0, 0x5A000020, 0, 0);
    g_vram_move_rect.x = 0x240;
    g_vram_move_rect_y = 0x100;
    g_vram_move_rect_w = 0x14;
    g_vram_move_rect_h = 8;
    MoveImage(ICON_RECT, 0x1BB, 0x19C);
    g_spu_decode_buf = heap_alloc(0x1000);
    SpuSetTransferCallback(spu_decoded_xfer_done);
    g_spu_decode_flag = 5;
    SpuSetIRQCallback(spu_irq_read_decoded);
    SpuSetIRQAddr(g_spu_irq_addr = 0x100);
    g_audio_node_state = 0;
    lain_anim_load_set6();
    while (lain_anim_ready_set6() == 0) {
    }
    do {
        gfx_frame_begin();
        if (g_audio_node_state == 0 || g_audio_node_state == 4) {
            if (g_audio_node_is_ending == 1) {
                ring_models_spin(0);
            }
            site_draw_background();
        } else {
            if (g_audio_node_is_ending == 1) {
                ring_models_spin(3);
            }
            GsSortFastSprite(&g_slideshow_sprite, &g_ot[g_frame_buffer_index], 0x15E);
            DrawSync(0);
        }
        site_end_frame();
        if (g_audio_node_is_ending == 0 && g_audio_node_state == 1) {
            VSync(2);
        }
    } while (audio_node_update() != 1);
    if (g_audio_node_is_ending == 1) {
        do {
            gfx_frame_begin();
            ring_models_collapse();
            site_end_frame();
        } while (g_ring_models_done == 0);
    }
    SpuSetIRQ(0);
    SpuSetIRQCallback(NULL);
    SpuSetTransferCallback(NULL);
    heap_free(g_spu_decode_buf);
    SpuFree(g_voice_spu_addr);
    g_vram_move_rect.x = 0x280;
    g_vram_move_rect_y = 0x100;
    g_vram_move_rect_w = 0x80;
    g_vram_move_rect_h = 0x20;
    MoveImage(ICON_RECT, 0x140, 0x1E0);
    DrawSync(0);
    g_vram_move_rect.x = 0x280;
    g_vram_move_rect_y = 0x120;
    g_vram_move_rect_w = 0x48;
    g_vram_move_rect_h = 0x20;
    MoveImage(ICON_RECT, 0x1C0, 0x1E0);
    DrawSync(0);
    loading_anim_reload();
}

/* Blit frame `k` of the animated loading icon from the sheet at (0x240,0x100). */
#define SHOW_ICON_FRAME(k)                                           \
    g_vram_move_rect.x = src_x[k] + 0x240;                                 \
    g_vram_move_rect_y = 0x100;                                            \
    g_vram_move_rect_w = size[k][0];                                       \
    g_vram_move_rect_h = size[k][1];                                       \
    MoveImage(ICON_RECT, pos[k][0] + 0x120, pos[k][1] | 0x100)

/* Per-frame driver of the movie/intro sequence (state in g_audio_node_state): wait,
 * play the jingle with a pulsing icon, stream the movie, and clean up.
 * Returns 1 once the sequence has finished. */
u8 audio_node_update(void) {
    u8 pos[3][2] = { { 0x9B, 0x9C }, { 0x9A, 0x9D }, { 0x9D, 0x9C } };
    u8 size[3][2] = { { 0x14, 0x08 }, { 0x16, 0x08 }, { 0x12, 0x0E } };
    u8 src_x[3] = { 0x00, 0x14, 0x2A };
    SpuVoiceAttr attr;
    SpuVoiceAttr *va;

    switch (g_audio_node_state) {
        case 0:
            if (lain_anim_play_next() == 0) {
                if (g_media_id < 0x27F) {
                    g_audio_node_state += 1;
                } else {
                    g_audio_node_state += 2;
                }
                g_audio_node_icon_step = 0;
                g_audio_node_timer = 0;
            }
            g_lain_anim_frame[2] = 150;
            g_lain_anim_frame[3] = 300;
            break;
        case 1:
            g_vram_move_rect.x = 0x240;
            g_vram_move_rect_y = 0x108;
            g_vram_move_rect_w = 0x16;
            g_vram_move_rect_h = 0x10;
            MoveImage(ICON_RECT, 0x1BA, 0x19B);
            SHOW_ICON_FRAME(g_audio_icon_frames[0]);
            DrawSync(0);
            if (g_audio_node_timer == 0) {
                g_vram_move_rect.x = 0x240;
                g_vram_move_rect_y = 0x100;
                g_vram_move_rect_w = 0x14;
                g_vram_move_rect_h = 8;
                MoveImage(ICON_RECT, 0x1BB, 0x19C);
                DrawSync(0);
                va = &attr;
                va->mask = 0x10;
                attr.voice = 0x800000;
                attr.pitch = 0x85F;
                SpuSetKeyOnWithAttr(va);
            }
            if (g_audio_node_timer < g_voice_name_frames) {
                if (g_audio_node_icon_step < 6 && g_voice_mora_count != 0) {
                    g_audio_node_icon_step++;
                } else {
                    g_audio_node_icon_step = 0;
                    if (g_voice_mora_count != 0) {
                        g_voice_mora_count--;
                    }
                }
                SHOW_ICON_FRAME(g_voice_icon_frames[g_audio_node_icon_step]);
                g_audio_node_timer++;
            } else {
                SpuSetKey(0, 0x800000);
                SpuFree(g_voice_spu_addr);
                g_audio_node_timer = 0;
                g_audio_node_state++;
            }
            break;
        case 2:
            if (g_audio_node_timer == 0) {
                g_vram_move_rect.x = 0x240;
                g_vram_move_rect_y = 0x100;
                g_vram_move_rect_w = 0x14;
                g_vram_move_rect_h = 8;
                MoveImage(ICON_RECT, 0x1BB, 0x19C);
                SHOW_ICON_FRAME(g_audio_icon_frames[0]);
                DrawSync(0);
                g_player_ending_mode = 2;
                g_player_quit = 0;
                g_media_xa_file = g_media_table[g_media_id].xa_file;
                spu_cd_audio_on();
                if (CdSync(1, NULL) == 0) {
                    CdControlF(9, NULL);
                }
                xa_seek_file();
                SpuSetIRQ(1);
                xa_play_channel(g_media_table[g_media_id].file);
                g_audio_level = spu_decoded_peak_level();
            }
            if (player_get_quit() == 0) {
                g_vram_move_rect.x = 0x240;
                g_vram_move_rect_y = 0x108;
                g_vram_move_rect_w = 0x16;
                g_vram_move_rect_h = 0x10;
                MoveImage(ICON_RECT, 0x1BA, 0x19B);
                DrawSync(0);
                if (g_audio_level > 0) {
                    if (g_audio_node_icon_step < 8) {
                        g_audio_node_icon_step++;
                    } else {
                        g_audio_node_icon_step = 0;
                        g_audio_level = spu_decoded_peak_level();
                    }
                    SHOW_ICON_FRAME(g_audio_icon_frames[g_audio_node_icon_step]);
                } else {
                    SHOW_ICON_FRAME(g_audio_icon_frames[0]);
                    DrawSync(0);
                    g_audio_level = spu_decoded_peak_level();
                }
                g_audio_node_timer = 1;
            } else {
                if (g_audio_node_is_ending == 0) {
                    g_audio_node_state += 2;
                } else {
                    g_audio_node_state += 1;
                }
                g_audio_node_timer = 0;
                g_audio_node_icon_step = 0;
                spu_cd_audio_off();
                SpuSetIRQ(0);
                SpuSetIRQCallback(NULL);
                SpuSetTransferCallback(NULL);
            }
            break;
        case 3:
            g_vram_move_rect.x = 0x240;
            g_vram_move_rect_y = 0x108;
            g_vram_move_rect_w = 0x16;
            g_vram_move_rect_h = 0x10;
            MoveImage(ICON_RECT, 0x1BA, 0x19B);
            SHOW_ICON_FRAME(g_audio_icon_frames[0]);
            DrawSync(0);
            if (g_audio_node_timer == 0) {
                g_vram_move_rect.x = 0x240;
                g_vram_move_rect_y = 0x100;
                g_vram_move_rect_w = 0x14;
                g_vram_move_rect_h = 8;
                MoveImage(ICON_RECT, 0x1BB, 0x19C);
                DrawSync(0);
                va = &attr;
                va->mask = 0x10;
                attr.voice = 0x800000;
                attr.pitch = 0x85F;
                SpuSetKeyOnWithAttr(va);
            }
            if (g_audio_node_timer < g_voice_name_frames) {
                if (g_audio_node_icon_step < 6 && g_voice_mora_count != 0) {
                    g_audio_node_icon_step++;
                } else {
                    g_audio_node_icon_step = 0;
                    if (g_voice_mora_count != 0) {
                        g_voice_mora_count--;
                    }
                }
                SHOW_ICON_FRAME(g_voice_icon_frames[g_audio_node_icon_step]);
                g_audio_node_timer++;
            } else {
                SpuSetKey(0, 0x800000);
                SpuFree(g_voice_spu_addr);
                g_audio_node_timer = 0;
                g_audio_node_state++;
            }
            break;
        case 4:
            if (g_audio_node_timer == 0 && g_audio_node_is_ending == 1) {
                g_media_id = 0x2DE;
                g_player_ending_mode = 2;
                g_player_quit = 0;
                g_audio_node_icon_step = 0;
                g_media_xa_file = g_media_table[0x2DE].xa_file;
                spu_cd_audio_on();
                if (CdSync(1, NULL) == 0) {
                    CdControlF(9, NULL);
                }
                xa_seek_file();
                xa_play_channel(g_media_table[g_media_id].file);
                g_audio_node_timer = 1;
            }
            if (lain_anim_play_set6_last() == 0) {
                g_audio_node_icon_step = 0;
                g_audio_node_timer = 0;
                g_audio_node_state++;
                spu_cd_audio_off();
                if (g_audio_node_is_ending == 0) {
                    g_vram_move_rect.x = 0;
                    g_vram_move_rect_y = 0;
                    g_vram_move_rect_w = 0x140;
                    g_vram_move_rect_h = 0x1E0;
                    ClearImage(ICON_RECT, 0, 0, 0);
                    DrawSync(0);
                }
            }
            g_lain_anim_frame[2] = 150;
            g_lain_anim_frame[3] = 300;
            break;
    }
    if (g_audio_node_state >= 5) {
        g_player_ending_mode = 0;
        return 1;
    }
    return 0;
}

/* Key on a voice with a pitch derived from its index. */
void snd_voice_key_on_pitched(s32 voice) {
    SpuVoiceAttr attr;

    attr.mask = 0x10;
    attr.voice = 1 << voice;
    attr.pitch = voice * 735 / 12 + 735;
    SpuSetKeyOnWithAttr(&attr);
}

void snd_voice_key_off(s32 voice) {
    SpuSetKey(0, 1 << voice);
}

/* Draw curve `index`'s scratch polyline as gouraud lines fading to black
 * towards the tail. */
void bg_curve_draw(s32 index) {
    volatile s32 j;
    u16 head;
    u16 tail;

    for (j = 0; j < g_bg_curve_polyline_len - 1; j++) {
        head = (0x80 - j) * 100 / 128;
        tail = (0x7F - j) * 100 / 128;
        g_bg_curve_lines[index][j].attribute = 0;
        g_bg_curve_lines[index][j].x0 = g_bg_curve_polyline[j + 1].x;
        g_bg_curve_lines[index][j].y0 = g_bg_curve_polyline[j + 1].y;
        g_bg_curve_lines[index][j].x1 = g_bg_curve_polyline[j].x;
        g_bg_curve_lines[index][j].y1 = g_bg_curve_polyline[j].y;
        g_bg_curve_lines[index][j].r0 = 0;
        g_bg_curve_lines[index][j].g0 = 0;
        g_bg_curve_lines[index][j].b0 = head;
        g_bg_curve_lines[index][j].r1 = 0;
        g_bg_curve_lines[index][j].g1 = 0;
        g_bg_curve_lines[index][j].b1 = tail;
        GsSortGLine(&g_bg_curve_lines[index][j], &g_ot_2d[g_frame_buffer_index], 4);
    }
}

/* Randomise slot `index`: two points within a 480x360 area. */
void bg_curve_randomize(s32 index, s32 mirror) {
    g_bg_curve_ctrl[index][0] = rand() % 480;
    g_bg_curve_ctrl[index][1] = rand() % 360;
    g_bg_curve_ctrl[index][2] = rand() % 480;
    g_bg_curve_ctrl[index][3] = -(rand() % 360);
    if (mirror) {
        g_bg_curve_ctrl[index][0] = -g_bg_curve_ctrl[index][0];
    }
}

#ifdef NON_MATCHING
/* 16 diffs: t and b swap registers ($a2/$v1) */
/* Evaluate curve `index` at its current t (cubic Bernstein terms
 * t^2(1-t) and t(1-t)^2) and append the point to its history. */
void bg_curve_step(s32 index) {
    s32 t;
    s32 a;
    s32 b;
    s32 x;
    s32 y;

    t = g_bg_curve_t[index];
    a = ((t * t) >> 8) * (0x100 - t);
    b = (((0x100 - t) * (0x100 - t)) >> 8) * t;
    a >>= 8;
    b >>= 8;
    x = a * g_bg_curve_ctrl[index][0] + b * g_bg_curve_ctrl[index][2];
    y = a * g_bg_curve_ctrl[index][1] + b * g_bg_curve_ctrl[index][3];
    x = (x >> 8) + g_bg_curve_origin[index][0];
    y = (y >> 8) + g_bg_curve_origin[index][1];
    g_bg_curve_history[index][g_bg_curve_head[index]].x = x;
    g_bg_curve_history[index][g_bg_curve_head[index]].y = y;
    g_bg_curve_head[index] = (g_bg_curve_head[index] + 1) & 0xFF;
    if (g_bg_curve_len[index] < 0xFF) {
        g_bg_curve_len[index]++;
    }
}
#else
INCLUDE_ASM("asm/nonmatchings/game/800379C8", bg_curve_step);
#endif

/* Allocate buffers and randomise the 16 curves. */
void bg_curves_init(void) {
    s32 i;
    s32 phase;

    g_bg_curve_polyline = heap_alloc(0x800);
    phase = 0;
    for (i = 0; i < 16; i++) {
        g_bg_curve_lines[i] = heap_alloc(0xA00);
        g_bg_curve_history[i] = heap_alloc(0x800);
        g_bg_curve_head[i] = 0;
        g_bg_curve_len[i] = 0;
        g_bg_curve_mirror[i] = i & 1;
        g_bg_curve_origin[i][0] = rand() % 20 + 150;
        g_bg_curve_origin[i][1] = rand() % 20 + 90;
        g_bg_curve_t[i] = phase & 0xFF;
        bg_curve_randomize(i, g_bg_curve_mirror[i]);
        phase += 16;
    }
}

/* Advance all 16 curves four steps, then copy the newest (up to 128) points of
 * each curve's history into the scratch polyline and draw it. */
void bg_curves_update(void) {
    u8 unused[0x58];
    s32 step;
    s32 i;
    s32 j;
    s32 count;
    s32 pos;
    Point *dst;
    Point *src;
    s32 prev;

    for (step = 0; step < 4; step++) {
        for (i = 0; i < 16; i++) {
            if (g_bg_curve_t[i] == 0) {
                if (g_bg_curve_mirror[i] != 0) {
                    g_bg_curve_mirror[i] = 0;
                } else {
                    g_bg_curve_mirror[i] = 1;
                }
                bg_curve_randomize(i, g_bg_curve_mirror[i]);
            }
            bg_curve_step(i);
            g_bg_curve_t[i]++;
            if (g_bg_curve_t[i] >= 0x100) {
                g_bg_curve_t[i] = 0;
            }
        }
    }
    for (i = 0; i < 16; i++) {
        count = 0x80;
        if (g_bg_curve_len[i] < 0x80) {
            count = g_bg_curve_len[i];
        }
        pos = g_bg_curve_head[i];
        g_bg_curve_polyline_len = 0;
        for (j = 0; j < count; j++) {
            prev = pos - 1;
            if (prev < 0) {
                prev = 0xFF;
            }
            pos = prev;
            /* Byte-offset arithmetic with the offset first: `&g_bg_curve_history[i][pos]`
             * expands base-first and ties the sum to the base register. */
            src = (Point *)(pos * sizeof(Point) + (uintptr_t)g_bg_curve_history[i]); /* port: uintptr_t, not u32 (64-bit pointers) */
            g_bg_curve_polyline[g_bg_curve_polyline_len].x = src->x;
            g_bg_curve_polyline[g_bg_curve_polyline_len].y = src->y;
            g_bg_curve_polyline_len++;
        }
        bg_curve_draw(i);
    }
}

void bg_curves_free(void) {
    s32 i;

    for (i = 0; i < 16; i++) {
        heap_free(g_bg_curve_lines[i]);
        heap_free(g_bg_curve_history[i]);
    }
    heap_free(g_bg_curve_polyline);
}

/* Stubbed-out debug print. */
void debug_printf(const char *fmt, ...) {
}

/* port: takes the log file name like the callers pass (the PS1 stub ignored it). */
void debug_log_open(char *logName) {
}

void func_800395E8(void) {
}

void func_800395F0(void) {
}

/* Decompress an LZ-packed buffer (magic g_lz_magic, u32 unpacked size, then
 * flag-byte-driven literals / back-references) into a newly allocated one. */
void *lz_decompress(u8 *src, s32 size) { /* port: void * (u8 * on the PS1); callers store it as u32 * etc. */
    u8 *dst;
    s32 out;
    s32 remaining;
    s32 bit;
    u8 flags;

    if (strncmp((const char *)src, g_lz_magic, 4) != 0) {
        return NULL;
    }
    src += 4;
    remaining = *(s32 *)src;
    src += 4;
    size -= 8;
    dst = heap_alloc(remaining);
    if (dst == NULL) {
        return NULL;
    }
    out = 0;
    while (size != 0) {
        if (remaining == 0) {
            break;
        }
        size--;
        flags = *src++;
        for (bit = 0; bit < 8 && size > 0 && remaining > 0; bit++) {
            if (flags & (0x80 >> bit)) {
                s32 back, len, i;
                back = *src++ + 1;
                len = *src++;
                len += 3;
                for (i = 0; i < len; i++) {
                    (dst + out)[i] = (dst + out - back)[i];
                }
                out += len;
                size -= 2;
                remaining -= len;
            } else {
                u8 *d = &dst[out];
                out++;
                *d = *src++;
                size--;
                remaining--;
            }
        }
    }
    return dst;
}
