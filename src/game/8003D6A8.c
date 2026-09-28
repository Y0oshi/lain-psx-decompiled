#include "common.h"

/* Declarations carried over from 8003D1B4.c (same original headers). */

/* Declarations carried over from 8003CB08.c (same original headers). */

/* Declarations carried over from 8003C084.c (same original headers). */

/*
 * Small globals that this TU addresses absolutely (lui + %lo, via the
 * assembler's macro expansion) even though cc1 treats them as small data.
 * A non-small-data section attribute keeps cc1 from emitting the `.extern
 * sym,size` hint, so maspsx doesn't turn them into $gp accesses; cc1 still
 * emits the unsplit `lw $2,sym` macro form the original assembler expanded.
 */
#define ABS __attribute__((section(".data")))

/* PsyQ libmcrd */
extern s32 MemCardCreateFile(s32 chan, char *file, s32 blocks);
extern s32 MemCardFormat(s32 chan);
extern s32 MemCardUnformat(s32 chan);
extern void MemCardClose(void);
extern s32 MemCardOpen(s32 chan, char *file, s32 flag);
extern s32 MemCardAccept(s32 chan);
extern s32 MemCardSync(s32 mode, s32 *cmds, s32 *result);
extern s32 MemCardReadData(u32 *adrs, s32 ofs, s32 bytes);
extern s32 MemCardWriteData(u32 *adrs, s32 ofs, s32 bytes);

extern char *strcat(char *dst, const char *src);

/* Save file name, "BISLPS-01603LAIN..." (in .rodata) */
extern char g_mcard_file_name[];
/* Digit strings "0".."9" */
extern char *g_sjis_digit_strings[];
extern char g_sjis_space[];

/* PsyQ libgpu / libgs types */
typedef struct {
    s16 x, y, w, h;
} RECT;

typedef struct {
    u32 mode;
    RECT *crect;
    u32 *caddr;
    RECT *prect;
    u32 *paddr;
} TIM_IMAGE;

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

typedef struct {
    u32 length;
    void *org;
    u32 offset;
    u32 point;
    void *tag;
} GsOT;

extern s32 OpenTIM(u32 *addr);
extern TIM_IMAGE *ReadTIM(TIM_IMAGE *tim);
extern u16 LoadClut(u32 *clut, s32 x, s32 y);
extern s32 LoadImage(RECT *rect, u32 *p);
extern s32 DrawSync(s32 mode);
extern s32 VSync(s32 mode);
extern s32 ResetGraph(s32 mode);
extern void GsSortFastSprite(GsSPRITE *sp, GsOT *ot, u16 pri);
extern void GsSwapDispBuff(void);
extern void GsSortClear(u8 r, u8 g, u8 b, GsOT *ot);
extern void GsDrawOt(GsOT *ot);

typedef struct {
    s32 sector;
    s32 size;
} FileEntry;

extern void *heap_alloc(s32 size);
extern s16 cd_load_archive_entry(s32, s32, FileEntry *, s32);
extern s32 cd_poll_load(s16);
extern u32 *lz_decompress(s32, s32);
extern void heap_free(void *);
extern void gfx_frame_begin(void);

extern FileEntry g_bin_file_table[];
extern s16 g_gfx_load_entry;
extern s16 g_gfx_load_req;
extern s32 g_gfx_load_buf;
extern s16 g_loading_anim_frame;
extern GsSPRITE g_loading_anim_sprites[];
extern s32 g_frame_buffer_index ABS; /* current double-buffer index */
extern GsOT g_ot_2d[];

typedef struct {
    u8 bytes[20];
} SaveName;

typedef struct {
    u8 unk0[0x22];
    u8 unk22[4];
    u8 unk26[2];
} SaveEntry; /* 0x28 bytes */

extern SaveName g_player_name;
extern SaveEntry g_node_table[];
extern u32 g_save_count ABS;
extern s16 g_current_site ABS;
extern s16 g_gate_level ABS;
extern s16 g_polytan_parts ABS;
extern s16 g_sskn_level ABS;
extern s16 g_media_played_count ABS;
extern s16 g_site_cursor_col ABS;
extern s16 g_site_cursor_row ABS;
extern s16 g_other_site_cursor_col ABS;
extern s16 g_other_site_cursor_row ABS;
extern s16 g_text_window_show_options ABS;

typedef struct {
    u8 bytes[0x200];
} SaveIcon;

typedef struct {
    char text[31];
} SaveTitle;

extern void *memset(void *dst, s32 c, s32 n);
extern void *memcpy(void *dst, const void *src, u32 n);
extern s32 rand(void);
/* 512-byte memory card header/icon block */
extern SaveIcon g_mcard_header_template;
/* Shift-JIS save titles (.rodata); the second is a rare (1 in 70) variant */
extern SaveTitle g_save_title;
extern SaveTitle g_save_title_rare;
/* Shift-JIS stat labels (.rodata) */
extern char g_save_title_tokimeki[];
extern char g_save_title_harumage[];
extern char g_save_title_gakkuri[];
extern char g_save_title_count_sep[];
extern char g_save_title_percent[];
extern char g_save_title_denpa[];
extern char g_save_title_denpa_unit[];
extern s16 g_stat_gakkuri ABS;
extern s16 g_stat_harumage ABS;
extern s16 g_stat_tokimeki ABS;
extern s16 g_screensaver_count ABS;

typedef struct {
    u8 unk0[0x28];
    u32 attribute;
    s16 tpage;
    s16 clut;
    s16 scaleX;
    s16 scaleY;
    u16 x;
    u16 y;
    s16 w;
    s16 h;
    s16 rotX;
    s16 rotY;
    s16 rotZ;
    s16 unk42;
    u8 u;
    u8 v;
    u8 r;
    u8 g;
    u8 b;
    u8 unk49[3];
} MenuObject; /* 0x4C bytes */

typedef struct {
    u16 x;
    u16 y;
} U16Pair;

extern MenuObject g_disc_change_text[24];
extern U16Pair g_disc_change_text_pos[24];
extern GsSPRITE g_disc_change_sprites[10];
extern void sprite_draw(MenuObject *obj, GsOT *ot, s32 arg2);


extern s32 cd_detect_disc(void);
extern s32 cd_lid_watch(void);
void disc_change_draw_screen(s32 arg0, s32 selected, s32 input);

typedef struct {
    s32 pos;
    s32 size;
} Pair;

extern Pair g_disc_file_table[100];
extern Pair g_disc1_file_table[100];
extern Pair g_disc2_file_table[100];

extern s32 StoreImage2(RECT *rect, u32 *p);
extern s32 PCcreat(char *name, s32 perms);
extern s32 PCread(s32 fd, void *buf, s32 len);
extern s32 PCclose(s32 fd);

extern void tim_load_file_at(s32 id, s32 x, s32 y, s32 cx, s32 cy);
extern s16 font_render_string(char *text, s32 x, s32 y, s32 arg3, s32 arg4);
/* "press ANY button" (in .rodata) */
extern char g_str_press_any_button[];
extern GsSPRITE g_polytan_sprites[];
extern s32 ClearImage(RECT *rect, u8 r, u8 g, u8 b);
extern void snd_play_sfx(s32);
extern void pad_read_command(void);
extern s16 g_pad_command ABS;
/* The "press ANY button" sprite (g_polytan_sprites[15]) */
extern GsSPRITE g_polytan_press_any;
extern GsSPRITE g_site_prompt_sprites[];
/* g_site_prompt_sprites[10] and g_site_prompt_sprites[11] */
extern GsSPRITE g_site_prompt_flash_sprite;
extern GsSPRITE g_site_prompt_strip_sprites[];
extern MenuObject g_site_prompt_ring;
extern void sprite_draw_rotated(MenuObject *obj, GsOT *ot, s32 pri);


/* Load the title-screen images and lay out the "press ANY button" prompt. */
void polytan_scene_load(void) {
    tim_load_file_at(0x21, 0x140, 0, 0x140, 0x90);
    tim_load_file_at(0x25, 0x158, 0, 0x140, 0x90);
    tim_load_file_at(0x20, 0x180, 0, 0x140, 0x90);
    tim_load_file_at(0x23, 0x1A0, 0, 0x140, 0x90);
    tim_load_file_at(0x24, 0x140, 0x50, 0x140, 0x90);
    tim_load_file_at(0x22, 0x158, 0x50, 0x140, 0x90);
    tim_load_file_at(0x26, 0x1C0, 0, 0x1C0, 0x70);
    tim_load_file_at(0x27, 0x1FC, 0, 0x1C0, 0x70);
    tim_load_file_at(0x29, 0x140, 0x100, 0x140, 0x1A0);
    tim_load_file_at(0x28, 0x180, 0x100, 0x180, 0x190);
    g_polytan_sprites[15].w = font_render_string("press ANY button", 0x200, 0x100, 1, 1);
    g_polytan_sprites[15].x = 0x88 - (g_polytan_sprites[5].w >> 1);
}


INCLUDE_RODATA("asm/nonmatchings/game/8003D6A8", D_80011D40);

/* Title screen loop: draw the title sprites (some gated by g_polytan_parts flag
 * bits) until a button is pressed, blinking the "press ANY button" prompt.
 * Each case draws on its own; cross-jumping merges the calls into one. */
void polytan_scene_run(void) {
    RECT rect;
    s32 frame;
    s32 blink;
    s32 i;

    rect.x = 0;
    rect.y = 0;
    rect.w = 320;
    rect.h = 480;
    ClearImage(&rect, 0, 0, 0);
    polytan_scene_load();
    frame = 0;
    blink = frame;
    if (g_polytan_parts == 0x3F) {
        snd_play_sfx(30);
    }
    for (;;) {
        DrawSync(0);
        gfx_frame_begin();
        pad_read_command();
        for (i = 0; i < 15; i++) {
            switch (i) {
                case 6:
                    if (g_polytan_parts & 0x8) {
                        GsSortFastSprite(&g_polytan_sprites[i], &g_ot_2d[g_frame_buffer_index], 1);
                    }
                    break;
                case 7:
                    if (g_polytan_parts & 0x1) {
                        GsSortFastSprite(&g_polytan_sprites[i], &g_ot_2d[g_frame_buffer_index], 1);
                    }
                    break;
                case 8:
                    if (g_polytan_parts & 0x2) {
                        GsSortFastSprite(&g_polytan_sprites[i], &g_ot_2d[g_frame_buffer_index], 1);
                    }
                    break;
                case 9:
                    if (g_polytan_parts & 0x4) {
                        GsSortFastSprite(&g_polytan_sprites[i], &g_ot_2d[g_frame_buffer_index], 1);
                    }
                    break;
                case 10:
                    if (g_polytan_parts & 0x10) {
                        GsSortFastSprite(&g_polytan_sprites[i], &g_ot_2d[g_frame_buffer_index], 1);
                    }
                    break;
                case 11:
                    if (g_polytan_parts & 0x20) {
                        GsSortFastSprite(&g_polytan_sprites[i], &g_ot_2d[g_frame_buffer_index], 1);
                    }
                    break;
                default:
                    GsSortFastSprite(&g_polytan_sprites[i], &g_ot_2d[g_frame_buffer_index], 1);
                    break;
            }
        }
        if (frame >= 30) {
            if (g_pad_command != 0) {
                break;
            }
            if (blink < 30) {
                GsSortFastSprite(&g_polytan_press_any, &g_ot_2d[g_frame_buffer_index], 0);
            }
            blink++;
            if (blink == 60) {
                blink = 0;
            }
        }
        VSync(0);
        ResetGraph(1);
        GsSwapDispBuff();
        GsSortClear(0, 0, 0, &g_ot_2d[g_frame_buffer_index]);
        GsDrawOt(&g_ot_2d[g_frame_buffer_index]);
        if (frame < 30) {
            frame++;
        }
    }
    rect.x = 0;
    rect.y = 0;
    rect.w = 320;
    rect.h = 480;
    ClearImage(&rect, 0, 0, 0);
}

extern s16 g_gfx_load_step;
extern u32 *g_menu_scroll_tim ABS;
void loading_anim_draw(s32 reset);

/* Start an async load of file entry `index` (see loading_anim_load). */
#define LOAD_START(index)                                                   \
    g_gfx_load_buf = (s32)heap_alloc(g_bin_file_table[index].size);                \
    g_gfx_load_entry = (index);                                                   \
    g_gfx_load_req = cd_load_archive_entry(3, (index), g_bin_file_table, g_gfx_load_buf);         \
    g_gfx_load_step++

/* Once the pending load is done, upload the TIM: CLUT to (cx, cy), pixels
 * to (x, y). */
#define LOAD_FINISH(cx, cy, px, py)                                         \
    if (cd_poll_load(g_gfx_load_req) == 0) {                                   \
        break;                                                              \
    }                                                                       \
    data = lz_decompress(g_gfx_load_buf, g_bin_file_table[g_gfx_load_entry].size);         \
    heap_free((void *)g_gfx_load_buf);                                      \
    OpenTIM(data);                                                          \
    ReadTIM(&tim);                                                          \
    LoadClut(tim.caddr, (cx), (cy));                                        \
    rect.x = (px);                                                          \
    rect.y = (py);                                                          \
    rect.w = tim.prect->w;                                                  \
    rect.h = tim.prect->h;                                                  \
    LoadImage(&rect, tim.paddr);                                            \
    DrawSync(0);                                                            \
    heap_free(data);                                                    \
    g_gfx_load_step++

/* Background loader state machine (one step per call): loads a series of
 * TIM images into VRAM while loading_anim_draw animates the loading screen.
 * Returns 1 once everything is loaded. With skipLast set, the image of
 * entry 14 is not kept in g_menu_scroll_tim. */
s32 gfx_bg_load_step(s32 skipLast) {
    TIM_IMAGE tim;
    RECT rect;
    u32 *data;

    switch (g_gfx_load_step) {
        case 2:
            LOAD_START(13);
            break;
        case 3:
            LOAD_FINISH(0x140, 0x1E9, 0x140, 0x199);
            break;
        case 4:
            LOAD_START(6);
            break;
        case 5:
            LOAD_FINISH(0x140, 0xF1, 0x180, 0);
            break;
        case 6:
            LOAD_START(7);
            break;
        case 7:
            LOAD_FINISH(0x140, 0xF1, 0x158, 0);
            break;
        case 8:
            LOAD_START(5);
            break;
        case 9:
            LOAD_FINISH(0x140, 0xF1, 0x1C0, 0);
            break;
        case 10:
            LOAD_START(4);
            break;
        case 11:
            LOAD_FINISH(0x140, 0xF2, 0x180, 0x100);
            break;
        case 12:
            LOAD_START(8);
            break;
        case 13:
            LOAD_FINISH(0x140, 0xF3, 0x180, 0x141);
            break;
        case 14:
            LOAD_START(22);
            break;
        case 15:
            LOAD_FINISH(0, 0x1E5, 0x380, 0x100);
            break;
        case 16:
            LOAD_START(23);
            break;
        case 17:
            LOAD_FINISH(0, 0x1E6, 0x340, 0x100);
            break;
        case 18:
            LOAD_START(24);
            break;
        case 19:
            LOAD_FINISH(0, 0x1E7, 0x300, 0x100);
            break;
        case 20:
            LOAD_START(31);
            break;
        case 21:
            LOAD_FINISH(0, 0x1E8, 0x300, 0x198);
            break;
        case 22:
            LOAD_START(14);
            break;
        case 23:
            if (cd_poll_load(g_gfx_load_req) == 0) {
                break;
            }
            if (!skipLast) {
                FileEntry *entry = &g_bin_file_table[g_gfx_load_entry];

                data = lz_decompress(g_gfx_load_buf, entry->size);
                heap_free((void *)g_gfx_load_buf);
                OpenTIM(data);
                ReadTIM(&tim);
                LoadClut(tim.caddr, 0x140, 0xF4);
                DrawSync(0);
                g_menu_scroll_tim = data;
            } else {
                heap_free((void *)g_gfx_load_buf);
            }
            /* fallthrough */
        case 0:
        case 1:
            g_gfx_load_step++;
            break;
        case 24:
            LOAD_START(3);
            break;
        case 25:
            if (cd_poll_load(g_gfx_load_req) == 0) {
                break;
            }
            data = lz_decompress(g_gfx_load_buf, g_bin_file_table[g_gfx_load_entry].size);
            heap_free((void *)g_gfx_load_buf);
            OpenTIM(data);
            ReadTIM(&tim);
            LoadClut(tim.caddr, 0x140, 0x198);
            rect.x = 0x140;
            rect.y = 0x100;
            rect.w = tim.prect->w;
            rect.h = tim.prect->h;
            LoadImage(&rect, tim.paddr);
            DrawSync(0);
            heap_free(data);
            g_gfx_load_step = 0;
            g_loading_anim_frame = 0;
            return 1;
    }
    if (g_gfx_load_step >= 2) {
        loading_anim_draw(0);
    }
    return 0;
}

/* Load file entry 42 (a TIM image, with CLUT) and upload it to VRAM at (640, 256). */
void loading_anim_load(void) {
    TIM_IMAGE tim;
    RECT rect;
    u32 *data;

    g_gfx_load_buf = (s32)heap_alloc(g_bin_file_table[42].size);
    g_gfx_load_entry = 42;
    g_gfx_load_req = cd_load_archive_entry(3, 42, g_bin_file_table, g_gfx_load_buf);
    while (cd_poll_load(g_gfx_load_req) == 0) {
    }
    data = lz_decompress(g_gfx_load_buf, g_bin_file_table[g_gfx_load_entry].size);
    heap_free((void *)g_gfx_load_buf);
    OpenTIM(data);
    ReadTIM(&tim);
    LoadClut(tim.caddr, 640, 376);
    rect.x = 640;
    rect.y = 256;
    rect.w = tim.prect->w;
    rect.h = tim.prect->h;
    LoadImage(&rect, tim.paddr);
    DrawSync(0);
    heap_free(data);
    g_loading_anim_frame = 0;
}

/* Load file entry 43 (a TIM image, no CLUT) and upload it to VRAM at (640, 256),
 * over what loading_anim_load put there; main calls it before each site_run. */
void loading_anim_reload(void) {
    TIM_IMAGE tim;
    RECT rect;
    u32 *data;

    g_gfx_load_buf = (s32)heap_alloc(g_bin_file_table[43].size);
    g_gfx_load_entry = 43;
    g_gfx_load_req = cd_load_archive_entry(3, 43, g_bin_file_table, g_gfx_load_buf);
    while (cd_poll_load(g_gfx_load_req) == 0) {
    }
    data = lz_decompress(g_gfx_load_buf, g_bin_file_table[g_gfx_load_entry].size);
    heap_free((void *)g_gfx_load_buf);
    OpenTIM(data);
    ReadTIM(&tim);
    rect.x = 640;
    rect.y = 256;
    rect.w = tim.prect->w;
    rect.h = tim.prect->h;
    LoadImage(&rect, tim.paddr);
    DrawSync(0);
    heap_free(data);
    g_loading_anim_frame = 0;
}

/* Draw one frame of a 29-frame loading animation (sprite atlas cells of
 * 24x40) and flip buffers. Nonzero reset restarts at frame 0. */
void loading_anim_draw(s32 reset) {
    s16 col;

    if (reset) {
        g_loading_anim_frame = 0;
    }
    DrawSync(0);
    gfx_frame_begin();
    col = g_loading_anim_frame % 10;
    g_loading_anim_sprites[0].u = col * 24;
    g_loading_anim_sprites[0].v = (s16)(g_loading_anim_frame / 10) * 40;
    GsSortFastSprite(&g_loading_anim_sprites[0], &g_ot_2d[g_frame_buffer_index], 0);
    GsSortFastSprite(&g_loading_anim_sprites[1], &g_ot_2d[g_frame_buffer_index], 0);
    VSync(0);
    ResetGraph(1);
    GsSwapDispBuff();
    GsSortClear(0, 0, 0, &g_ot_2d[g_frame_buffer_index]);
    GsDrawOt(&g_ot_2d[g_frame_buffer_index]);
    g_loading_anim_frame++;
    if (g_loading_anim_frame >= 29) {
        g_loading_anim_frame = 0;
    }
}
