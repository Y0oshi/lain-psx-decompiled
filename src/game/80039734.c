#include "common.h"

/* Declarations carried over from 800379C8.c (same original headers). */

/* PsyQ library types */

typedef struct {
    s16 x, y;
    s16 w, h;
} RECT;

typedef struct {
    u32 mode;
    RECT *crect;
    u32 *caddr;
    RECT *prect;
    u32 *paddr;
} TIM_IMAGE;

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
    u32 length;
    void *org;
    u32 offset;
    u32 point;
    void *tag;
} GsOT;

typedef struct {
    u32 attribute;
    s16 x, y;
    u16 w, h;
    u16 tpage;
    u8 u, v;
    s16 cx, cy;
    u8 r, g, b;
    u8 pad17;
    s16 mx, my;
    s16 scalex, scaley;
    s32 rotate;
} GsSPRITE;

typedef struct {
    u32 attribute;
    s16 x0, y0;
    s16 x1, y1;
    u8 r0, g0, b0;
    u8 r1, g1, b1;
} GsGLINE;

typedef struct {
    u8 addr[3];
    u8 len;
    u8 r0, g0, b0, code;
    s16 x0, y0;
    u8 u0, v0;
    u16 clut;
    s16 x1, y1;
    u8 u1, v1;
    u16 tpage;
    s16 x2, y2;
    u8 u2, v2;
    u16 pad1;
    s16 x3, y3;
    u8 u3, v3;
    u16 pad2;
} POLY_FT4;

typedef struct {
    unsigned addr : 24;
    unsigned len : 8;
    u8 r0, g0, b0, code;
} P_TAG;

#define setlen(p, _len) (((P_TAG *)(p))->len = (u8)(_len))
#define setcode(p, _code) (((P_TAG *)(p))->code = (u8)(_code))
#define getcode(p) (u8)(((P_TAG *)(p))->code)
#define setPolyFT4(p) setlen(p, 9), setcode(p, 0x2C)
#define setSemiTrans(p, abe) ((abe) ? setcode(p, getcode(p) | 0x02) : setcode(p, getcode(p) & ~0x02))

typedef struct {
    s16 left;
    s16 right;
} SpuVolume;

typedef struct {
    u32 voice;
    u32 mask;
    SpuVolume volume;
    SpuVolume volmode;
    SpuVolume volumex;
    u16 pitch;
    u16 note;
    u16 sample_note;
    s16 envx;
    u32 addr;
    u32 loop_addr;
    s32 a_mode;
    s32 s_mode;
    s32 r_mode;
    u16 ar;
    u16 dr;
    u16 sr;
    u16 rr;
    u16 sl;
    u16 adsr1;
    u16 adsr2;
} SpuVoiceAttr;

typedef struct {
    s16 *src;
    s16 *dest;
    s16 *work;
    s32 size;
    s32 loop_start;
    s8 loop;
    s8 byte_swap;
    s8 proceed;
    s8 quality;
} EncSPUEnv;

typedef struct {
    u8 pos[4];
    u32 size;
    char name[16];
} CdlFILE;

/* PsyQ library / libc functions */

extern s32 printf(const char *fmt, ...);
extern s32 strncmp(const char *a, const char *b, s32 n);
extern void *memset(void *s, s32 c, s32 n);
extern s32 rand(void);

extern s32 ClearImage(RECT *rect, s32 r, s32 g, s32 b);
extern s32 LoadImage(RECT *rect, u32 *p);
extern s32 MoveImage(RECT *rect, s32 x, s32 y);
extern u16 LoadClut(u32 *clut, s32 x, s32 y);
extern u16 GetTPage(s32 tp, s32 abr, s32 x, s32 y);
extern s32 DrawSync(s32 mode);
extern s32 VSync(s32 mode);
extern s32 ResetGraph(s32 mode);
extern s32 OpenTIM(u32 *addr);
extern TIM_IMAGE *ReadTIM(TIM_IMAGE *timg);
extern void GsGetTimInfo(u32 *tim, GsIMAGE *image);
extern void GsSwapDispBuff(void);
extern void GsSortClear(u8 r, u8 g, u8 b, GsOT *otp);
extern void GsDrawOt(GsOT *otp);
extern void GsSortPoly(void *prim, GsOT *otp, u16 pri);
extern void GsSortGLine(GsGLINE *line, GsOT *ot, u16 pri);
extern void GsSortFastSprite(GsSPRITE *sp, GsOT *ot, u16 pri);

extern void SpuSetKey(s32 on_off, u32 voice_bit);
extern void SpuSetKeyOnWithAttr(SpuVoiceAttr *attr);
extern void SpuSetVoiceAttr(SpuVoiceAttr *attr);
extern u32 SpuMalloc(s32 size);
extern void SpuFree(u32 addr);
extern u32 SpuSetTransferStartAddr(u32 addr);
extern u32 SpuRead(u8 *addr, u32 size);
extern void SpuSetIRQ(s32 on_off);
extern void SpuSetIRQAddr(u32 addr);
extern void SpuSetIRQCallback(void (*func)(void));
extern void SpuSetTransferCallback(void (*func)(void));
extern s32 EncSPU(EncSPUEnv *env);

extern s32 CdControlB(u8 com, u8 *param, u8 *result);
extern s32 CdControlF(u8 com, u8 *param);
extern s32 CdSync(s32 mode, u8 *result);
extern CdlFILE *CdSearchFile(CdlFILE *fp, char *name);

extern s32 MemCardInit(s32 val);
extern void MemCardStart(void);

/* Game types */

/* Accesses to some globals are absolute (lui/%lo) rather than $gp-relative in
 * this part of the original code; declaring them in .data reproduces that. */
#define NO_GP __attribute__((section(".data")))

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
extern HeapBlock *g_heap_start;
extern s32 g_heap_size;
extern s32 g_heap_peak;            /* peak bytes in use */
extern s32 g_heap_used;            /* bytes in use */
extern HeapBlock *g_heap_rover;     /* first free block (rover) */
extern char *g_lz_magic;          /* compressed-file magic */

/* Files / graphics */
extern FileEntry g_bin_file_table[];
extern FileEntry g_site_a_file_table[];
extern FileEntry g_site_b_file_table[];
extern s32 g_frame_buffer_index NO_GP;      /* current double-buffer index */
extern GsOT g_ot_2d[2];
extern GsOT g_ot[];

/* Background curves (16 animated gouraud lines) */
extern Point *g_bg_curve_history[16];     /* per-curve point history */
extern GsGLINE *g_bg_curve_lines[16];   /* per-curve line primitives */
extern s32 g_bg_curve_ctrl[][4];       /* per-curve control points */
extern u16 g_bg_curve_t[16];        /* curve parameter t (0..255) */
extern u16 g_bg_curve_mirror[16];        /* mirror flag */
extern u16 g_bg_curve_head[16];        /* history write index */
extern u16 g_bg_curve_len[16];        /* history length (saturates at 255) */
extern u16 g_bg_curve_origin[16][2];
extern Point *g_bg_curve_polyline;         /* scratch polyline */
extern u16 g_bg_curve_polyline_len;

/* Title screen / dialogs */
extern GsSPRITE g_gate_sprites[];     /* title logo sprites; [5] = "press ANY button" */
extern u32 g_gate_clut[];
extern SpriteDef g_sskn_sprite_defs[];    /* yes/no dialog sprite layout */
extern u16 g_gate_obj_screen_x;            /* logo centre x */
extern u16 g_gate_obj_screen_y;            /* logo centre y */
extern s16 g_gate_fly_pending;
extern s16 g_gate_level NO_GP;
extern s16 g_pad_command NO_GP;

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
extern GsSPRITE g_slideshow_sprite;
extern u8 g_player_name[];
extern s16 g_current_site[1] NO_GP;   /* language (0/1) */
extern s32 g_media_id NO_GP;      /* current movie index */
extern u8 g_media_xa_file NO_GP;
extern u8 g_player_ending_mode NO_GP;
extern s16 g_spu_decode_flag NO_GP;
extern s16 g_player_quit NO_GP;
extern void *g_spu_decode_buf NO_GP;
extern s32 g_spu_irq_addr NO_GP;
extern u8 g_ring_models_done NO_GP;

/* CD drive watchdog */
extern s16 g_cd_watch_inited;
extern s16 g_cd_last_shell_open;
extern s16 g_cd_idle_timeout;
extern s16 g_cd_error_timeout;

/* Game functions */

extern s32 cd_load_archive_entry(s32 mode, s32 index, FileEntry *table, void *buf);
extern s32 cd_poll_load(s32 id);
extern void site_end_frame(void);
extern void pad_read_command(void);
extern void xa_play_channel(s16 file);
extern void spu_irq_read_decoded(void);
extern void spu_decoded_xfer_done(void);
extern void spu_cd_audio_off(void);
extern void spu_cd_audio_on(void);
extern s16 spu_decoded_peak_level(void);
extern void xa_seek_file(void);

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

/* Blit frame `k` of the animated loading icon from the sheet at (0x240,0x100). */
#define SHOW_ICON_FRAME(k)                                           \
    g_vram_move_rect.x = src_x[k] + 0x240;                                 \
    g_vram_move_rect_y = 0x100;                                            \
    g_vram_move_rect_w = size[k][0];                                       \
    g_vram_move_rect_h = size[k][1];                                       \
    MoveImage(ICON_RECT, pos[k][0] + 0x120, pos[k][1] | 0x100)


#ifdef NON_MATCHING
/* 101 diffs: register allocation in the TIM loaders and the countdown loops
 * (the original keeps the sprite offset as a separate induction variable).
 * decomp-permuter (20 min) only found 97 with a split `spr[k].x = 0x87;
 * spr[k].x += (count - j) * 8`, not applied.
 * Also needs its own .rodata (switch jump tables). */
/* Yes/No confirmation dialog: fades in, lets the player pick with the d-pad
 * and confirm, runs a short countdown on "yes" and fades out. Returns 1 when
 * "yes" was chosen. */
s32 sskn_scene_run(void) {
    TIM_IMAGE tim;
    RECT rect;
    GsSPRITE spr[19];
    SpriteEntry icons[2];
    s32 tick;
    s32 choice;
    s32 result;
    void *packed;
    u8 *data;
    s32 id;
    s32 i;
    s32 j;
    s32 n;
    s32 k;
    s32 rot;
    s32 state;
    s32 fade;
    s32 count;

    rect.x = 0;
    rect.y = 0;
    rect.w = 0x140;
    rect.h = 0x1E0;
    ClearImage(&rect, 0, 0, 0);
    packed = heap_alloc(g_bin_file_table[10].size);
    if (packed != NULL) {
        id = cd_load_archive_entry(3, 10, g_bin_file_table, packed);
        while (cd_poll_load(id) == 0) {
        }
        data = lz_decompress(packed, g_bin_file_table[10].size);
        heap_free(packed);
        OpenTIM((u32 *)data);
        ReadTIM(&tim);
        LoadClut(tim.caddr, 0x140, 0xC1);
        rect.x = 0x140;
        rect.y = 0;
        rect.w = 0x40;
        rect.h = 0xC0;
        LoadImage(&rect, tim.paddr);
        DrawSync(0);
        heap_free(data);

        packed = heap_alloc(g_bin_file_table[11].size);
        id = cd_load_archive_entry(3, 11, g_bin_file_table, packed);
        while (cd_poll_load(id) == 0) {
        }
        data = lz_decompress(packed, g_bin_file_table[11].size);
        heap_free(packed);
        OpenTIM((u32 *)data);
        ReadTIM(&tim);
        LoadClut(tim.caddr, 0x140, 0xC2);
        rect.x = 0x180;
        rect.y = 0;
        rect.w = 0x26;
        rect.h = 0xC0;
        LoadImage(&rect, tim.paddr);
        DrawSync(0);
        heap_free(data);

        packed = heap_alloc(g_bin_file_table[12].size);
        id = cd_load_archive_entry(3, 12, g_bin_file_table, packed);
        while (cd_poll_load(id) == 0) {
        }
        data = lz_decompress(packed, g_bin_file_table[12].size);
        heap_free(packed);
        OpenTIM((u32 *)data);
        ReadTIM(&tim);
        LoadClut(tim.caddr, 0x140, 0xC3);
        rect.x = 0x1C0;
        rect.y = 0;
        rect.w = 0x30;
        rect.h = 0xA0;
        LoadImage(&rect, tim.paddr);
        DrawSync(0);
        heap_free(data);
    }

    for (i = 0; i < 19; i++) {
        spr[i].attribute = 0;
        spr[i].x = g_sskn_sprite_defs[i].x;
        spr[i].y = g_sskn_sprite_defs[i].y;
        spr[i].w = g_sskn_sprite_defs[i].w;
        spr[i].h = g_sskn_sprite_defs[i].h;
        spr[i].tpage = g_sskn_sprite_defs[i].tpage;
        spr[i].u = g_sskn_sprite_defs[i].u;
        spr[i].v = g_sskn_sprite_defs[i].v;
        spr[i].cx = g_sskn_sprite_defs[i].cx;
        spr[i].cy = g_sskn_sprite_defs[i].cy;
        spr[i].r = 0;
        spr[i].g = 0;
        spr[i].b = 0;
        spr[i].scalex = 0x1000;
        spr[i].scaley = 0x1000;
        spr[i].rotate = 0;
    }
    spr[6].attribute = 0x40000000;
    spr[7].attribute = 0x40000000;
    for (i = 0; i < 2; i++) {
        icons[i].attr = 0;
        icons[i].x = g_sskn_sprite_defs[i + 19].x;
        icons[i].y = g_sskn_sprite_defs[i + 19].y;
        icons[i].w = g_sskn_sprite_defs[i + 19].w;
        icons[i].h = g_sskn_sprite_defs[i + 19].h;
        icons[i].tpage = g_sskn_sprite_defs[i + 19].tpage;
        icons[i].u = g_sskn_sprite_defs[i + 19].u;
        icons[i].v = g_sskn_sprite_defs[i + 19].v;
        icons[i].r = 0;
        icons[i].g = 0;
        icons[i].b = 0;
        icons[i].scaleX = 0x1000;
        icons[i].scaleY = 0x1000;
        icons[i].rotX = 0;
        icons[i].rotY = 0;
        icons[i].rotZ = 0;
        icons[i].clut = (g_sskn_sprite_defs[i + 19].cy << 6) | ((g_sskn_sprite_defs[i + 19].cx >> 4) & 0x3F);
    }
    icons[1].attr = 0x40000000;
    rot = 0;
    state = 0;
    fade = 0;
    choice = 0;
    result = 0;

    while (1) {
        DrawSync(0);
        gfx_frame_begin();
        switch (state) {
            case 0:
                for (i = 1; i < 19; i++) {
                    spr[i].r = fade * 4;
                    spr[i].g = fade * 4;
                    spr[i].b = fade * 4;
                }
                for (i = 0; i < 2; i++) {
                    icons[i].r = fade * 4;
                    icons[i].g = fade * 4;
                    icons[i].b = fade * 4;
                }
                icons[1].r = fade * 2;
                icons[1].g = fade * 2;
                icons[1].b = fade * 2;
                fade++;
                if (fade >= 33) {
                    state++;
                }
                for (i = 1; i < 12; i++) {
                    if (i == 11) {
                        GsSortFastSprite(&spr[11], &g_ot_2d[g_frame_buffer_index], 4);
                    } else {
                        GsSortFastSprite(&spr[i], &g_ot_2d[g_frame_buffer_index], 1);
                    }
                }
                break;
            case 1:
                pad_read_command();
                switch (g_pad_command) {
                    case 1:
                    case 2:
                    case 3:
                        choice = 0;
                        break;
                    case 6:
                    case 7:
                    case 8:
                        choice = 1;
                        break;
                    case 17:
                        fade = 0;
                        count = 0;
                        tick = 0;
                        if (choice == 0) {
                            result = 1;
                            state += 1;
                        } else {
                            state += 2;
                        }
                        break;
                }
                if (choice == 0) {
                    spr[1].cx = 0x140;
                    spr[2].cx = 0x150;
                    spr[9].cx = 0x150;
                    spr[10].cx = 0x140;
                } else {
                    spr[2].cx = 0x140;
                    spr[1].cx = 0x150;
                    spr[10].cx = 0x150;
                    spr[9].cx = 0x140;
                }
                for (i = 1; i < 12; i++) {
                    if (i == 11) {
                        GsSortFastSprite(&spr[11], &g_ot_2d[g_frame_buffer_index], 4);
                    } else {
                        GsSortFastSprite(&spr[i], &g_ot_2d[g_frame_buffer_index], 1);
                    }
                }
                break;
            case 2:
                for (i = 1; i < 13; i++) {
                    switch (i) {
                        case 1:
                        case 2:
                        case 8:
                        case 9:
                        case 10:
                            break;
                        case 11:
                            GsSortFastSprite(&spr[i], &g_ot_2d[g_frame_buffer_index], 4);
                            break;
                        case 12:
                            GsSortFastSprite(&spr[i], &g_ot_2d[g_frame_buffer_index], 1);
                            break;
                        default:
                            GsSortFastSprite(&spr[i], &g_ot_2d[g_frame_buffer_index], 2);
                            break;
                    }
                }
                if (count < 20) {
                    spr[fade + 13].x = count * 8 + 0x8F;
                    spr[fade + 13].y = 0xC4;
                    GsSortFastSprite(&spr[fade + 13], &g_ot_2d[g_frame_buffer_index], 0);
                }
                for (j = 0, n = 5, k = 18; n > 0; j++, n--, k--) {
                    if (j >= count) {
                        break;
                    }
                    spr[k].x = (count - j) * 8 + 0x87;
                    spr[k].y = 0xC4;
                    GsSortFastSprite(&spr[k], &g_ot_2d[g_frame_buffer_index], 0);
                }
                if (tick == 0) {
                    if (count == 20) {
                        fade = 0;
                        state++;
                    }
                    if (fade == 5) {
                        fade = 0;
                        count++;
                    } else {
                        fade++;
                    }
                }
                tick++;
                if (tick == 3) {
                    tick = 0;
                }
                break;
            case 3:
                for (i = 1; i < 19; i++) {
                    spr[i].r = 0x80 - fade * 4;
                    spr[i].g = 0x80 - fade * 4;
                    spr[i].b = 0x80 - fade * 4;
                }
                for (i = 0; i < 2; i++) {
                    icons[i].r = 0x80 - fade * 4;
                    icons[i].g = 0x80 - fade * 4;
                    icons[i].b = 0x80 - fade * 4;
                }
                icons[1].r = 0x40 - fade * 2;
                icons[1].g = 0x40 - fade * 2;
                icons[1].b = 0x40 - fade * 2;
                fade++;
                if (fade >= 33) {
                    state++;
                }
                for (i = 1; i < 13; i++) {
                    switch (i) {
                        case 1:
                        case 2:
                        case 8:
                        case 9:
                        case 10:
                            break;
                        case 11:
                            GsSortFastSprite(&spr[i], &g_ot_2d[g_frame_buffer_index], 4);
                            break;
                        case 12:
                            GsSortFastSprite(&spr[i], &g_ot_2d[g_frame_buffer_index], 1);
                            break;
                        default:
                            GsSortFastSprite(&spr[i], &g_ot_2d[g_frame_buffer_index], 2);
                            break;
                    }
                }
                for (j = 0, n = 5, k = 18; n > 0; j++, n--, k--) {
                    if (j >= count) {
                        break;
                    }
                    spr[k].x = (count - j) * 8 + 0x87;
                    spr[k].y = 0xC4;
                    GsSortFastSprite(&spr[k], &g_ot_2d[g_frame_buffer_index], 0);
                }
                break;
            case 4:
                goto end;
        }
        icons[0].rotY = rot;
        icons[1].rotY = rot;
        sprite_draw_rotated(&icons[0], &g_ot_2d[g_frame_buffer_index], 3);
        sprite_draw_rotated(&icons[1], &g_ot_2d[g_frame_buffer_index], 3);
        VSync(0);
        ResetGraph(1);
        rot += 16;
        GsSwapDispBuff();
        GsSortClear(0, 0, 0, &g_ot_2d[g_frame_buffer_index]);
        GsDrawOt(&g_ot_2d[g_frame_buffer_index]);
        rot %= 4096;
    }
end:
    DrawSync(0);
    VSync(0);
    ResetGraph(1);
    GsSwapDispBuff();
    rect.x = 0x140;
    rect.y = 0;
    rect.w = 0x140;
    rect.h = 0x1E0;
    ClearImage(&rect, 0, 0, 0);
    pad_read_command();
    return result;
}
#else
INCLUDE_ASM("asm/nonmatchings/game/80039734", sskn_scene_run);
#endif

/* malloc: first-fit search from the free-block rover, splitting large blocks. */
void *heap_alloc(u32 size) {
    HeapBlock *block;
    HeapBlock *split;
    HeapBlock *next;

    if (g_heap_start == NULL) {
        printf("allocate fault");
        return NULL;
    }
    block = g_heap_rover;
    if (size == 0) {
        return NULL;
    }
    size += 3;
    size &= ~3;
    while (block != NULL) {
        if (block->magic == HEAP_USED) {
            block = block->next;
        } else if (block->magic == HEAP_FREE) {
            if (block->size >= size) {
                if (block->size - size < 0x20) {
                    size = block->size;
                } else {
                    split = (HeapBlock *)((u8 *)block + (size + sizeof(HeapBlock)));
                    split->magic = block->magic;
                    split->size = block->size - size - sizeof(HeapBlock);
                    split->prev = block;
                    next = block->next;
                    split->next = next;
                    if (next != NULL) {
                        next->prev = split;
                    }
                    block->next = split;
                }
                block->magic = HEAP_USED;
                block->size = size;
                g_heap_used += size + sizeof(HeapBlock);
                if (g_heap_peak < g_heap_used) {
                    g_heap_peak = g_heap_used;
                }
                if (block == g_heap_rover) {
                    g_heap_rover = block->next;
                }
                return block + 1;
            }
            block = block->next;
        } else {
            printf("malloc::magic field corrupt\n");
            return NULL;
        }
    }
    return NULL;
}

/* free: mark the block free and coalesce with free neighbours. */
void heap_free(void *ptr) {
    HeapBlock *block;
    HeapBlock *next;
    HeapBlock *prev;

    block = (HeapBlock *)ptr - 1;
    if (ptr == NULL) {
        return;
    }
    if (block->magic == HEAP_USED) {
        g_heap_used -= block->size;
        next = block->next;
        if (next != NULL) {
            if (next->magic == HEAP_FREE) {
                block->size += next->size + sizeof(HeapBlock);
                block->next = next->next;
                if (next->next != NULL) {
                    next->next->prev = block;
                }
                g_heap_used -= sizeof(HeapBlock);
            } else if (next->magic != HEAP_USED) {
                debug_printf("free::magic field corrupt(next)\n");
            }
        }
        prev = block->prev;
        if (prev != NULL && prev->magic == HEAP_FREE) {
            prev->size += block->size + sizeof(HeapBlock);
            prev->next = block->next;
            if (block->next != NULL) {
                block->next->prev = prev;
            }
            g_heap_used -= sizeof(HeapBlock);
            if (prev < g_heap_rover) {
                g_heap_rover = prev;
            }
        } else {
            block->magic = HEAP_FREE;
            if (block < g_heap_rover) {
                g_heap_rover = block;
            }
        }
    } else {
        debug_printf("free::magic field corrupt %08x\n", ptr);
    }
}

/* Initialise the heap as a single free block covering the whole area. */
s32 heap_init(HeapBlock *heap, s32 size) {
    g_heap_size = size;
    heap->magic = HEAP_FREE;
    g_heap_start = heap;
    heap->size = size - sizeof(HeapBlock);
    heap->next = NULL;
    heap->prev = NULL;
    g_heap_rover = heap;
    return 1;
}

s32 heap_get_peak(void) {
    return g_heap_peak;
}

/* Sanity-check a heap allocation's header and its neighbours' headers. */
s32 heap_check_block(void *ptr) {
    HeapBlock *block;
    HeapBlock *other;
    s32 result;

    result = 0;
    block = (HeapBlock *)((u8 *)ptr - 8);
    if (block->magic != HEAP_FREE && block->magic != HEAP_USED) {
        printf("WARNING:This block may be damaged!\n");
        result = -1;
    }
    other = block->prev;
    if (other != NULL && other->magic != HEAP_FREE && other->magic != HEAP_USED) {
        printf("WARNING:Before block may be damaged!\n");
        result = -1;
    }
    other = block->next;
    if (other != NULL && other->magic != HEAP_FREE && other->magic != HEAP_USED) {
        printf("WARNING:Next block may be damaged!\n");
        result = -1;
    }
    return result;
}

/* Free heap bytes: heap size minus bytes in use. */
s32 heap_get_free(void) {
    return g_heap_size - g_heap_used;
}

/* Upload a 240-line window of a tall scrolling strip (three 512-line TIMs
 * stacked vertically, 160 bytes per line) to VRAM at (448, 0). `scroll` is the
 * window's bottom edge (`pos`); `blank` fills the part outside the strip. */
void credits_upload_scroll(s32 pos, u32 *blank, u32 *tim0, u32 *tim1, u32 *tim2) {
    RECT rect;
    TIM_IMAGE tim;
    s32 split;
    s32 scroll;

    scroll = pos;

    if (scroll == 0) {
        return;
    }
    if (scroll < 0xF0) {
        rect.x = 0x1C0;
        rect.y = 0;
        rect.w = 0x50;
        rect.h = 0xF0;
        LoadImage(&rect, blank);
        OpenTIM(tim0);
        ReadTIM(&tim);
        rect.x = 0x1C0;
        rect.y = 0xF0 - scroll;
        rect.w = 0x50;
        rect.h = scroll;
        LoadImage(&rect, tim.paddr);
    } else if (scroll < 0x201) {
        OpenTIM(tim0);
        ReadTIM(&tim);
        rect.x = 0x1C0;
        rect.y = 0;
        rect.w = 0x50;
        rect.h = 0xF0;
        LoadImage(&rect, (u32 *)((u8 *)tim.paddr + (scroll - 0xF0) * 0xA0));
    } else if (scroll < 0x2F0) {
        scroll -= 0xF0;
        split = 0x200 - scroll;
        OpenTIM(tim0);
        ReadTIM(&tim);
        rect.x = 0x1C0;
        rect.y = 0;
        rect.w = 0x50;
        rect.h = split;
        LoadImage(&rect, (u32 *)((u8 *)tim.paddr + scroll * 0xA0));
        OpenTIM(tim1);
        ReadTIM(&tim);
        rect.x = 0x1C0;
        rect.y = split;
        rect.w = 0x50;
        rect.h = 0xF0 - split;
        LoadImage(&rect, tim.paddr);
    } else if (scroll < 0x400) {
        OpenTIM(tim1);
        ReadTIM(&tim);
        rect.x = 0x1C0;
        rect.y = 0;
        rect.w = 0x50;
        rect.h = 0xF0;
        LoadImage(&rect, (u32 *)((u8 *)tim.paddr + (scroll - 0x2F0) * 0xA0));
    } else if (scroll < 0x4F0) {
        scroll -= 0x2F0;
        split = 0x200 - scroll;
        OpenTIM(tim1);
        ReadTIM(&tim);
        rect.x = 0x1C0;
        rect.y = 0;
        rect.w = 0x50;
        rect.h = split;
        LoadImage(&rect, (u32 *)((u8 *)tim.paddr + scroll * 0xA0));
        OpenTIM(tim2);
        ReadTIM(&tim);
        rect.x = 0x1C0;
        rect.y = split;
        rect.w = 0x50;
        rect.h = 0xF0 - split;
        LoadImage(&rect, tim.paddr);
    } else if (scroll < 0x600) {
        OpenTIM(tim2);
        ReadTIM(&tim);
        rect.x = 0x1C0;
        rect.y = 0;
        rect.w = 0x50;
        rect.h = 0xF0;
        LoadImage(&rect, (u32 *)((u8 *)tim.paddr + (scroll - 0x4F0) * 0xA0));
    } else {
        rect.x = 0x1C0;
        rect.y = 0;
        rect.w = 0x50;
        rect.h = 0xF0;
        LoadImage(&rect, blank);
        scroll = pos - 0x4F0;
        split = 0x200 - scroll;
        OpenTIM(tim2);
        ReadTIM(&tim);
        rect.x = 0x1C0;
        rect.y = 0;
        rect.w = 0x50;
        rect.h = split;
        LoadImage(&rect, (u32 *)((u8 *)tim.paddr + scroll * 0xA0));
    }
}

static inline u32 *LoadTimClut(s32 index, s32 clut_x, s32 clut_y, TIM_IMAGE *tim) {
    void *packed;
    u8 *data;
    s32 id;

    packed = heap_alloc(g_bin_file_table[index].size);
    id = cd_load_archive_entry(3, index, g_bin_file_table, packed);
    while (cd_poll_load(id) == 0) {
    }
    data = lz_decompress(packed, g_bin_file_table[index].size);
    heap_free(packed);
    OpenTIM((u32 *)data);
    ReadTIM(tim);
    LoadClut(tim->caddr, clut_x, clut_y);
    return (u32 *)data;
}

static inline void LoadTimImage(s32 index, s32 x, s32 y, s32 clut_x, s32 clut_y, TIM_IMAGE *tim, RECT *rect) {
    u32 *data;

    data = LoadTimClut(index, clut_x, clut_y, tim);
    rect->x = x;
    rect->y = y;
    rect->w = tim->prect->w;
    rect->h = tim->prect->h;
    LoadImage(rect, tim->paddr);
    DrawSync(0);
    heap_free(data);
}

/* Credits / ending scroller: loads three tall TIM strips plus overlays, then
 * scrolls the strip for 0x6F0 frames under a grid of animated tiles. */
void credits_run(void) {
    u32 *strips[3];
    POLY_FT4 unused;
    POLY_FT4 left;
    POLY_FT4 frame_l;
    POLY_FT4 frame_r;
    POLY_FT4 tiles[20];
    RECT clear;
    TIM_IMAGE tim;
    RECT rect;
    u32 *data;
    u8 *blank;
    s32 i;
    s32 j;
    s32 u;

    clear.x = 0;
    clear.y = 0;
    clear.w = 0x280;
    clear.h = 0x1E0;
    ClearImage(&clear, 0, 0, 0);
    bgm_fade_out();
    strips[1] = LoadTimClut(0x10, 0x140, 0xF2, &tim);
    strips[2] = LoadTimClut(0x11, 0x140, 0xF2, &tim);
    strips[0] = LoadTimClut(0xF, 0x140, 0xF2, &tim);
    LoadTimImage(0x13, 0x140, 0, 0x140, 0xF0, &tim, &rect);
    LoadTimImage(0x12, 0x180, 0, 0x140, 0xF1, &tim, &rect);
    bgm_select_slot(1);
    bgm_play();

    setPolyFT4(&unused);
    setPolyFT4(&left);
    setPolyFT4(&frame_l);
    setPolyFT4(&frame_r);
    setSemiTrans(&left, 1);
    setSemiTrans(&frame_l, 1);
    setSemiTrans(&frame_r, 1);
    for (i = 0; i < 20; i++) {
        setPolyFT4(&tiles[i]);
        tiles[i].tpage = 5;
        tiles[i].clut = 0x3C14;
        tiles[i].r0 = 0x80;
        tiles[i].g0 = 0x80;
        tiles[i].b0 = 0x80;
        tiles[i].u0 = 0;
        tiles[i].v0 = 0;
        tiles[i].u1 = 0x3F;
        tiles[i].v1 = 0;
        tiles[i].u2 = 0;
        tiles[i].v2 = 0x3F;
        tiles[i].u3 = 0x3F;
        tiles[i].v3 = 0x3F;
        tiles[i].x0 = (i % 5) * 64;
        tiles[i].y0 = (i / 5) * 64;
        tiles[i].x1 = tiles[i].x0 + 64;
        tiles[i].y1 = tiles[i].y0;
        tiles[i].x2 = tiles[i].x0;
        tiles[i].y2 = tiles[i].y0 + 64;
        tiles[i].x3 = tiles[i].x0 + 64;
        tiles[i].y3 = tiles[i].y0 + 64;
    }

    left.tpage = 0x46;
    left.clut = 0x3C54;
    left.r0 = 0x80;
    left.g0 = 0x80;
    left.b0 = 0x80;
    left.x0 = 0;
    left.y0 = 0;
    left.x1 = 0x140;
    left.y1 = 0;
    left.x2 = 0;
    left.y2 = 0xF0;
    left.x3 = 0x140;
    left.y3 = 0xF0;
    left.u0 = 0;
    left.v0 = 0;
    left.u1 = 7;
    left.v1 = 0;
    left.u2 = 0;
    left.v2 = 0xEF;
    left.u3 = 7;
    left.v3 = 0xEF;

    frame_l.tpage = 7;
    frame_l.clut = 0x3C94;
    frame_l.r0 = 0x80;
    frame_l.g0 = 0x80;
    frame_l.b0 = 0x80;
    frame_l.x0 = 0;
    frame_l.y0 = 0;
    frame_l.x1 = 0x100;
    frame_l.y1 = 0;
    frame_l.x2 = 0;
    frame_l.y2 = 0xF0;
    frame_l.x3 = 0x100;
    frame_l.y3 = 0xF0;
    frame_l.u0 = 0;
    frame_l.v0 = 0;
    frame_l.u1 = 0xFF;
    frame_l.v1 = 0;
    frame_l.u2 = 0;
    frame_l.v2 = 0xF0;
    frame_l.u3 = 0xFF;
    frame_l.v3 = 0xF0;

    frame_r.tpage = 8;
    frame_r.clut = 0x3C94;
    frame_r.r0 = 0x80;
    frame_r.g0 = 0x80;
    frame_r.b0 = 0x80;
    frame_r.x0 = 0x100;
    frame_r.y0 = 0;
    frame_r.x1 = 0x140;
    frame_r.y1 = 0;
    frame_r.x2 = 0x100;
    frame_r.y2 = 0xF0;
    frame_r.x3 = 0x140;
    frame_r.y3 = 0xF0;
    frame_r.u0 = 0;
    frame_r.v0 = 0;
    frame_r.u1 = 0x3F;
    frame_r.v1 = 0;
    frame_r.u2 = 0;
    frame_r.v2 = 0xF0;
    frame_r.u3 = 0x3F;
    frame_r.v3 = 0xF0;

    blank = heap_alloc(0x9600);
    memset(blank, 0, 0x9600);
    for (i = 0; i < 0xF0; i++) {
        memset(blank + i * 0xA0, 0x77, 0x3E);
    }

    for (i = 0; i < 0x6F0;) {
        DrawSync(0);
        gfx_frame_begin();
        u = (i % 4) * 64;
        credits_upload_scroll(i++, (u32 *)blank, strips[0], strips[1], strips[2]);
        for (j = 0; j < 20; j++) {
            tiles[j].u0 = u;
            tiles[j].u1 = u + 0x3F;
            tiles[j].u2 = u;
            tiles[j].u3 = u + 0x3F;
            GsSortPoly(&tiles[j], &g_ot_2d[g_frame_buffer_index], 3);
        }
        GsSortPoly(&left, &g_ot_2d[g_frame_buffer_index], 0);
        GsSortPoly(&frame_r, &g_ot_2d[g_frame_buffer_index], 1);
        GsSortPoly(&frame_l, &g_ot_2d[g_frame_buffer_index], 1);
        VSync(0);
        ResetGraph(1);
        GsSwapDispBuff();
        GsSortClear(0, 0, 0, &g_ot_2d[g_frame_buffer_index]);
        GsDrawOt(&g_ot_2d[g_frame_buffer_index]);
    }
    DrawSync(0);
    VSync(0);
    ResetGraph(1);
    GsSwapDispBuff();
    bgm_fade_out();
    clear.x = 0;
    clear.y = 0;
    clear.w = 0x280;
    clear.h = 0x1E0;
    ClearImage(&clear, 0, 0, 0);
    DrawSync(0);
    bgm_select_slot(0);
    bgm_play();
    heap_free(strips[0]);
    heap_free(strips[1]);
    heap_free(strips[2]);
    heap_free(blank);
}

/* Load file `index`, decompress the TIM inside, and upload its CLUT and image
 * to VRAM at the given positions. */
void tim_load_file_at(s32 index, s32 x, s32 y, s32 clut_x, s32 clut_y) {
    TIM_IMAGE tim;
    RECT rect;
    void *packed;
    void *data;
    s32 id;

    packed = heap_alloc(g_bin_file_table[index].size);
    id = cd_load_archive_entry(3, index, g_bin_file_table, packed);
    while (cd_poll_load(id) == 0) {
    }
    data = lz_decompress(packed, g_bin_file_table[index].size);
    heap_free(packed);
    OpenTIM(data);
    ReadTIM(&tim);
    LoadClut(tim.caddr, clut_x, clut_y);
    rect.x = x;
    rect.y = y;
    rect.w = tim.prect->w;
    rect.h = tim.prect->h;
    LoadImage(&rect, tim.paddr);
    DrawSync(0);
    heap_free(data);
}

/* Load file `index`, decompress the TIM inside, and upload it to VRAM at the
 * positions stored in the TIM. */
void tim_load_file(s32 index) {
    TIM_IMAGE tim;
    RECT rect;
    void *packed;
    void *data;
    s32 id;

    packed = heap_alloc(g_bin_file_table[index].size);
    id = cd_load_archive_entry(3, index, g_bin_file_table, packed);
    while (cd_poll_load(id) == 0) {
    }
    data = lz_decompress(packed, g_bin_file_table[index].size);
    heap_free(packed);
    OpenTIM(data);
    ReadTIM(&tim);
    LoadClut(tim.caddr, tim.crect->x, tim.crect->y);
    rect.x = tim.prect->x;
    rect.y = tim.prect->y;
    rect.w = tim.prect->w;
    rect.h = tim.prect->h;
    LoadImage(&rect, tim.paddr);
    DrawSync(0);
    heap_free(data);
}

/* Load file `index`, decompress the TIM inside and upload only its CLUT.
 * Returns the decompressed buffer (caller frees it). */
void *tim_load_file_clut(s32 index, s32 clut_x, s32 clut_y) {
    TIM_IMAGE tim;
    RECT unused;
    void *packed;
    void *data;
    s32 id;

    packed = heap_alloc(g_bin_file_table[index].size);
    id = cd_load_archive_entry(3, index, g_bin_file_table, packed);
    while (cd_poll_load(id) == 0) {
    }
    data = lz_decompress(packed, g_bin_file_table[index].size);
    heap_free(packed);
    OpenTIM(data);
    ReadTIM(&tim);
    LoadClut(tim.caddr, clut_x, clut_y);
    return data;
}

/* Per-frame CD lid / disc watchdog. Returns 0 when idle or the lid is open,
 * 1 while busy, 2 when the drive needs reinitialising, -1/-2 on errors. */
static inline void CdInitState(void) {
    u8 result[8];
    s32 param;

    param = 0;
    CdControlB(0xE, (u8 *)&param, result);
    CdControlB(1, NULL, result);
    g_cd_watch_inited = 1;
    g_cd_idle_timeout = 300;
    g_cd_error_timeout = 600;
    g_cd_last_shell_open = result[0] & 0x10;
}

static inline s32 CdPollStatus(void) {
    u8 result[8];
    u8 param[4];
    s32 tries;

    tries = 60;
    param[0] = 0;
    param[1] = 0;
    param[2] = 0;
    param[3] = 0;
    while (1) {
        CdControlB(0x15, param, result);
        if (!(result[0] & 1)) {
            break;
        }
        if (result[1] & 0x40) {
            return 1;
        }
        if (result[0] & 0x10) {
            return 0x10;
        }
        if (tries == 0) {
            return 0;
        }
        tries--;
    }
    return 2;
}

s32 cd_lid_watch(void) {
    u8 result[8];

    if (g_cd_watch_inited == 0) {
        CdInitState();
    }
    CdControlB(1, NULL, result);
    if (g_cd_last_shell_open & 0x10) {
        if (!(result[0] & 0x10)) {
            if (result[0] & 2) {
                g_cd_idle_timeout = 300;
                if (CdControlB(0x13, NULL, result) == 0) {
                    return 1;
                }
                if (result[0] & 1) {
                    if (--g_cd_error_timeout == 0) {
                        g_cd_last_shell_open = result[0] & 0x10;
                        g_cd_error_timeout = 600;
                        return -2;
                    }
                }
                if (result[0] & 0xFD) {
                    return 1;
                }
                g_cd_error_timeout = 600;
                switch (CdPollStatus()) {
                    case 2:
                        g_cd_watch_inited = 0;
                        return 2;
                    case 1:
                        g_cd_last_shell_open = result[0] & 0x10;
                        return -1;
                    case 0x10:
                        g_cd_last_shell_open = result[0] & 0x10;
                        return 0;
                    case 0:
                        g_cd_last_shell_open = result[0] & 0x10;
                        return -2;
                }
                return 1;
            } else {
                if (--g_cd_idle_timeout == 0) {
                    g_cd_last_shell_open = result[0] & 0x10;
                    g_cd_idle_timeout = 300;
                    return -2;
                }
                return 1;
            }
        }
    } else {
        g_cd_last_shell_open = result[0] & 0x10;
    }
    return 0;
}

/* Poll the CD drive status (up to 60 tries). */
s32 cd_poll_drive_status(void) {
    return CdPollStatus();
}

/* Reset the CD drive and the lid watchdog state. */
void cd_lid_watch_reset(void) {
    CdInitState();
}

/* Detect which disc is inserted: 0 = disc 1, 1 = disc 2, -1 = neither. */
s32 cd_detect_disc(void) {
    CdlFILE file;

    if (CdSearchFile(&file, "\\LAIN_1.INF;1")) {
        return 0;
    }
    if (CdSearchFile(&file, "\\LAIN_2.INF;1")) {
        return 1;
    }
    return -1;
}

/* Scatter random 8x32 noise blocks over the logo's VRAM area. */
static inline void DrawNoise(RECT *rect) {
    s32 row;
    s32 i;
    s32 x;

    rect->w = 8;
    rect->h = 0x20;
    for (row = 0; row < 8; row++) {
        for (i = 0, x = 0x180; i < 7; i++, x += 8) {
            DrawSync(0);
            rect->x = (rand() % 6) * 8 + 0x1C0;
            rect->y = 0x148;
            MoveImage(rect, x, row * 32 + 0x100);
        }
    }
}

#ifdef NON_MATCHING
/* 58 diffs, register allocation: `delay` should be spilled and `blink` kept
 * in $fp. Also needs its own .rodata ("press ANY button").
 * The `tile_state[k] = 1; delay = 1;` order (from decomp-permuter) and
 * `(s16)(g_gate_obj_screen_y - 4)`, which keeps the subtraction off the u16 narrowing, both help.
 * Keeping `delay`/`done` in a stack array (s32 v[2]) gets 72: the original spills
 * them. Remaining: &tiles[0] is kept in a stack slot instead of being recomputed
 * from $sp, and a few constants land in t3 vs v0. */
/* Title screen: 44 logo tiles fly in one by one from random positions, then
 * the logo and a blinking "press ANY button" are shown until a button press. */
void gate_scene_run(void) {
    s16 tile_state[44];
    s16 tile_from[44][2];
    s16 tile_to[44][2];
    GsSPRITE tiles[44];
    RECT rect;
    SpriteEntry bars[4];
    RECT noise;
    s32 placed;
    s32 delay;
    s32 phase;
    s32 blink;
    s32 i;
    s32 k;
    s32 done;

    gate_models_init();
    tim_load_file_at(0x19, 0x1C0, 0x100, 0x140, 0x149);
    g_gate_sprites[5].w = font_render_string("press ANY button", 0x200, 0x100, 1, 1);
    phase = 0;
    blink = 0;
    placed = 0;
    delay = 0;
    g_gate_sprites[5].x = 0xA0 - g_gate_sprites[5].w / 2;
    memset(tile_state, 0, sizeof(tile_state));
    for (i = 0; i < 44; i++) {
        tiles[i].attribute = 0;
        tiles[i].tpage = 0x17;
        tiles[i].cx = 0x150;
        tiles[i].cy = 0x149;
        tiles[i].w = 8;
        tiles[i].h = 8;
        tiles[i].u = 0xA0;
        tiles[i].v = 0x20;
        tiles[i].r = 0x80;
        tiles[i].g = 0x80;
        tiles[i].b = 0x80;
    }
    for (i = 0; i < 4; i++) {
        switch (i) {
            case 1:
            case 3:
                bars[i].attr = 0x800000;
                bars[i].x = 0xA0;
                break;
            case 0:
            case 2:
                bars[i].attr = 0;
                bars[i].x = 0;
                break;
        }
        bars[i].y = (i < 2) ? 0x28 : 0x3A;
        bars[i].tpage = 0x17;
        bars[i].clut = 0x5254;
        bars[i].u = 0;
        bars[i].v = 0x20;
        bars[i].w = 0xA0;
        bars[i].h = 8;
        bars[i].r = 0x80;
        bars[i].g = 0x80;
        bars[i].b = 0x80;
    }
    rect.x = 0x390;
    rect.y = 0x59;
    rect.w = 0x10;
    rect.h = 1;
    LoadImage(&rect, g_gate_clut);
    rect.x = 0x380;
    rect.y = 0x58;
    rect.w = 8;
    rect.h = 0x58;
    ClearImage(&rect, 0, 0, 0);
    g_gate_fly_pending = 0;
    while (phase < 4) {
        pad_read_command();
        gfx_frame_begin();
        DrawNoise(&noise);
        GsSortFastSprite(&g_gate_sprites[0], &g_ot_2d[g_frame_buffer_index], 0);
        sprite_draw(&bars[0], &g_ot_2d[g_frame_buffer_index], 0);
        sprite_draw(&bars[1], &g_ot_2d[g_frame_buffer_index], 0);
        switch (phase) {
            case 0:
                if (delay == 0 && placed < 44) {
                    do {
                        k = rand() % 44;
                    } while (tile_state[k] != 0);
                    tile_state[k] = 1;
                    delay = 1;
                    placed++;
                    rect.x = 0x1E8;
                    rect.y = 0x120;
                    rect.w = 2;
                    rect.h = 8;
                    if (rand() & 1) {
                        tiles[k].u += 8;
                        rect.x += 2;
                    }
                    MoveImage(&rect, (k % 4) * 2 + 0x140, (k / 4) * 8);
                    tile_from[k][0] = rand() % 312;
                    tile_from[k][1] = rand() % 232;
                    tile_to[k][0] = g_gate_obj_screen_x + ((k % 4) - 2) * 8;
                    tile_to[k][1] = ((k / 4) - 5) * 8 + (s16)(g_gate_obj_screen_y - 4);
                } else {
                    delay--;
                }
                done = 0;
                for (i = 0; i < 44; i++) {
                    if (tile_state[i] != 0 && tile_state[i] < 5) {
                        tiles[i].x = tile_from[i][0] + (tile_to[i][0] - tile_from[i][0]) * tile_state[i] / 4;
                        tiles[i].y = tile_from[i][1] + (tile_to[i][1] - tile_from[i][1]) * tile_state[i] / 4;
                        tile_state[i]++;
                        GsSortFastSprite(&tiles[i], &g_ot_2d[g_frame_buffer_index], 1);
                    } else if (tile_state[i] != 0) {
                        done++;
                        GsSortFastSprite(&tiles[i], &g_ot_2d[g_frame_buffer_index], 1);
                    }
                }
                if (done == 44) {
                    phase++;
                }
                break;
            case 1:
                rect.x = 0x140;
                rect.y = 0;
                rect.w = 8;
                rect.h = 0x58;
                MoveImage(&rect, 0x380, 0x58);
                g_gate_fly_pending = 1;
                phase = 2;
                break;
            case 2:
                if (g_gate_fly_pending == 0) {
                    g_gate_level++;
                    phase = 3;
                    if (g_gate_level >= 4) {
                        snd_play_sfx(0x1E);
                    }
                }
                break;
            case 3:
                if (g_pad_command != 0) {
                    phase = 4;
                }
                if (g_gate_level >= 4) {
                    for (i = 1; i < 5; i++) {
                        GsSortFastSprite(&g_gate_sprites[i], &g_ot_2d[g_frame_buffer_index], 0);
                    }
                    sprite_draw(&bars[2], &g_ot_2d[g_frame_buffer_index], 0);
                    sprite_draw(&bars[3], &g_ot_2d[g_frame_buffer_index], 0);
                }
                if (blink < 20) {
                    GsSortFastSprite(&g_gate_sprites[5], &g_ot_2d[g_frame_buffer_index], 0);
                }
                blink++;
                if (blink == 40) {
                    blink = 0;
                }
                break;
        }
        gate_models_update();
        site_end_frame();
    }
    fog_disable();
}
#else
INCLUDE_ASM("asm/nonmatchings/game/80039734", gate_scene_run);
#endif

s32 mcard_start(void) {
    MemCardInit(0);
    MemCardStart();
    return 0;
}
