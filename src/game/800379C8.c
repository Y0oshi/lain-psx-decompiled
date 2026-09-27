#include "common.h"

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
extern void gs_sprite_setup(GsSPRITE *sp, s32 a1, s32 a2, s32 a3, s32 a4, u16 tpage, s32 a6, s32 a7, s32 a8,
                          s32 a9, u32 a10, s32 a11, s32 a12);
extern s16 player_get_quit(void);
extern s16 font_render_string(char *text, s32 x, s32 y, s32 arg3, s32 arg4);
extern void lain_anim_load_set6(void);
extern s32 lain_anim_play_set6_last(void);
extern s32 lain_anim_ready_set6(void);
extern s32 lain_anim_play_next(void);
extern void snd_play_sfx(s16 prog);
extern void bgm_play(void);
extern void bgm_fade_out(void);
extern void bgm_select_slot(s16 slot);
extern void sprite_draw(SpriteEntry *entry, GsOT *ot, u16 pri);
extern void sprite_draw_rotated(SpriteEntry *entry, GsOT *ot, u16 pri);
extern void gfx_frame_begin(void);
extern void fog_disable(void);
extern void site_draw_background(void);
extern void ring_models_init(void);
extern void ring_models_spin(s32 arg0);
extern void ring_models_collapse(void);
extern void gate_models_init(void);
extern void gate_models_update(void);
extern u8 *voice_synth_romaji(u8 *arg0, s32 *size, u8 *count);
extern void loading_anim_reload(void);

u8 audio_node_update(void);
void bg_curve_draw(s32 index);
void bg_curve_randomize(s32 index, s32 mirror);
void debug_printf(const char *fmt, ...);
u8 *lz_decompress(u8 *src, s32 size);
void *heap_alloc(u32 size);
void heap_free(void *ptr);
void credits_upload_scroll(s32 pos, u32 *blank, u32 *tim0, u32 *tim1, u32 *tim2);
void tim_load_file_at(s32 index, s32 x, s32 y, s32 clut_x, s32 clut_y);

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

/* Key off one SPU voice. */
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
/* 16 diffs: t and b swap registers ($a2/$v1): local-alloc gives t (5 refs over 9 insns)
 * a higher priority than the (0x100 - t)^2 temp, the original the other way round.
 * Tried: a `0x100 - t` local, no t (g_bg_curve_t[index] read each time), u16/u32 t, every
 * operand order and statement order of the a/b products, shifts folded in (16-26).
 * decomp-permuter (20 min) found nothing valid. */
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
            src = (Point *)(pos * sizeof(Point) + (u32)g_bg_curve_history[i]);
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

void debug_log_open(void) {
}

void func_800395E8(void) {
}

void func_800395F0(void) {
}

/* Decompress an LZ-packed buffer (magic g_lz_magic, u32 unpacked size, then
 * flag-byte-driven literals / back-references) into a newly allocated one. */
u8 *lz_decompress(u8 *src, s32 size) {
    u8 *dst;
    s32 out;
    s32 remaining;
    s32 bit;
    u8 flags;

    if (strncmp(src, g_lz_magic, 4) != 0) {
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
