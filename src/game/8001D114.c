#include "common.h"

/* Minimal PsyQ types used by this file. */
typedef struct {
    u8 minute, second, sector, track;
} CdlLOC;

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
    s16 x, y, w, h;
} RECT;

typedef struct {
    u32 length;
    void *org;
    u32 offset;
    u32 point;
    void *tag;
} GsOT;

typedef struct {
    u32 mode;
    RECT *crect;
    u32 *caddr;
    RECT *prect;
    u32 *paddr;
} TIM_IMAGE;

s32 OpenTIM(u32 *addr);
TIM_IMAGE *ReadTIM(TIM_IMAGE *tim);
u16 LoadClut(u32 *clut, s32 x, s32 y);
void GsSortFastSprite(GsSPRITE *sp, GsOT *ot, u16 pri);
void GsSortSprite(GsSPRITE *sp, GsOT *ot, u16 pri);
u16 GetTPage(s32 tp, s32 abr, s32 x, s32 y);
s32 DrawSync(s32 mode);
s32 LoadImage(RECT *rect, u32 *p);
s32 strlen(const char *s);
char *strncpy(char *dst, const char *src, s32 n);
void *memset(void *s, s32 c, s32 n);
s32 CdIntToPos(s32 i, CdlLOC *p);
s32 CdControl(u8 com, u8 *param, u8 *result);

extern s16 g_text_window_next_frame_mode;

/* One streamed voice/sound slot (0x10 bytes). */
typedef struct {
    s32 handle;   /* 0x00 */
    s16 id;       /* 0x04 */
    s16 unk6;     /* 0x06 */
    s16 state;    /* 0x08: 0 free, 1 loading, 2 ready, -1 failed */
    s16 sfx;     /* 0x0A */
    s16 count;    /* 0x0C */
    s16 padE;     /* 0x0E */
} SoundSlot;

typedef struct {
    s16 x, y;
} Pos16;

extern s16 g_text_window_state;
extern s16 g_text_window_frame_mode;
extern s16 g_text_window_x;
extern s16 g_text_window_y;
extern Pos16 g_text_window_saved_pos;
extern s32 g_lain_idle_loading;
extern s32 g_lain_action_anim_active;
extern s32 g_lain_anim_playing;
extern s32 g_lain_idle_load_count;
extern s32 g_lain_idle_anim_cur;
extern s32 g_lain_action_anim_index;
extern s32 D_800A6920;
extern SoundSlot g_lain_anim_default;
extern SoundSlot g_lain_idle_anims[];
extern SoundSlot g_lain_action_anims[];

typedef struct {
    u16 value;
    u16 pad;
} Pad16;

/* Sound bank tables: file index and associated value per bank. */
extern s32 g_lain_level_anim_files[2];
extern Pad16 g_lain_level_anim_sfx[1];
extern s32 g_lain_save_anim_file[1];
extern Pad16 g_lain_save_anim_sfx[1];
extern s32 g_lain_site_enter_anim_file[1];
extern Pad16 g_lain_site_enter_anim_sfx[1];
extern s32 D_800D6760[];
extern u8 g_heap_area[];
extern s16 g_text_window_busy;
extern s16 g_text_window_show_options;
extern s16 g_lain_anim_sfx;
extern s16 g_text_window_anim_frame;
extern s16 g_text_window_char_count;
extern s16 g_text_window_char_delay[]; /* per-character start delay */
extern s16 g_text_window_char_step[]; /* per-character animation step */
extern s16 g_text_window_target_x;
extern s16 g_text_window_target_y;
extern char g_text_window_next_name[];
extern s32 g_lain_anim_frame[];
extern s16 g_lain_idle_anim_files[];

typedef struct {
    s32 sector;
    s32 size;
} FileEntry;

extern FileEntry g_lapks_file_table[];
extern s16 g_text_window_text_width;
extern s16 g_site_cursor_col __attribute__((section(".data")));
extern s16 g_site_prev_cursor_row __attribute__((section(".data")));
extern u16 g_site_cursor_row __attribute__((section(".data")));
extern s32 g_site_rotation __attribute__((section(".data")));
extern s32 g_frame_buffer_index __attribute__((section(".data"))); /* current double-buffer index */
extern GsOT g_ot_2d[];
extern GsSPRITE g_text_window_bar_sprite0;
extern GsSPRITE g_text_window_bar_sprite1;
extern GsSPRITE g_site_notice_sprites[3];
extern s16 g_text_window_char_widths[];
extern GsSPRITE g_text_window_char_sprites[];
extern char g_text_window_saved_message[];
extern char g_text_window_message[];

char *strcpy(char *dst, const char *src);
void text_window_build_message(void);
void text_window_load_cell(s32 arg0, s32 arg1);
s32 font_render_string(char *text, s32 x, s32 y, s32 mode, s32 wide);
s32 cd_poll_load(s32 id);
void heap_free(s32 handle);
void anim_decode_frame(void *arg0);
s32 anim_open_archive(s32 handle);
s32 anim_next_frame(void);
void text_window_draw_idle(void);
void text_window_draw_leave(void);
void text_window_draw_enter(void);
void snd_play_anim_sfx(s32 arg0);
void lain_anim_play_idle(void);
void snd_stop_anim_sfx(void);
s32 rand(void);
s32 FntPrint(const char *fmt, ...);
void anim_decoder_init(s32 arg0, s32 arg1);
void heap_init(void *heap, s32 size);
void loading_anim_load(void);
void loading_anim_draw(s32 arg0);
s32 heap_alloc(s32 arg0);
s32 cd_load_archive_entry(s32 arg0, s32 index, FileEntry *table, s32 handle);

/* Free every idle (ready but not playing) sound slot. Expanded inline in
 * each loader, with its own locals. */
#define PURGE_IDLE_SOUNDS()                                    \
    {                                                          \
        s32 n;                                                 \
        SoundSlot *idle;                                       \
                                                               \
        FntPrint("PurgeIdle\n");                              \
        for (n = 0; n < 1; n++) {                              \
            idle = &g_lain_idle_anims[n];                             \
            if (idle->state == 2 && g_lain_idle_anim_cur != n) {         \
                idle->state = 0;                               \
                heap_free(idle->handle);                   \
            }                                                  \
        }                                                      \
    }

extern s16 *g_spu_decode_buf;
extern s32 g_spu_irq_addr;

/* Peak absolute sample in the current half of a 0x200-entry s16 buffer, scaled to 0..7. */
s16 spu_decoded_peak_level(void) {
    s16 value;
    s16 peak;
    s32 i;
    s32 start;
    s32 end;

    peak = 0;
    if (g_spu_irq_addr == 0) {
        start = 0;
        end = 0x100;
    } else {
        start = 0x100;
        end = 0x200;
    }
    for (i = start; i < end - 50; i++) {
        value = g_spu_decode_buf[i];
        if (value < 0) {
            value = -value;
        }
        if (peak < value) {
            peak = value;
        }
    }
    return peak / 4681;
}

/* Same peak scan as spu_decoded_peak_level, scaled to 0..5. */
s16 spu_decoded_peak_level6(void) {
    s16 value;
    s16 peak;
    s32 i;
    s32 start;
    s32 end;

    peak = 0;
    if (g_spu_irq_addr == 0) {
        start = 0;
        end = 0x100;
    } else {
        start = 0x100;
        end = 0x200;
    }
    for (i = start; i < end - 50; i++) {
        value = g_spu_decode_buf[i];
        if (value < 0) {
            value = -value;
        }
        if (peak < value) {
            peak = value;
        }
    }
    return peak / 6553;
}

extern s32 g_xa_file_ids[];
extern s32 g_disc_file_table[];
extern u8 g_media_xa_file;

/* Seek the CD to the start of the current track's file and start playback. */
void xa_seek_file(void) {
    CdlLOC loc[6];

    CdIntToPos(g_disc_file_table[g_xa_file_ids[g_media_xa_file - 1] * 2], loc);
    CdControl(2, (u8 *)loc, 0);
    CdControl(0x16, (u8 *)loc, 0);
}

/* Initialise a GsSPRITE with neutral (0x80) colour. */
void gs_sprite_setup(GsSPRITE *sprite, s16 x, s16 y, u16 w, u16 h, u16 tpage, u8 u, u8 v,
                   s16 cx, s16 cy, u32 attribute, s16 mx, s16 my) {
    sprite->x = x;
    sprite->y = y;
    sprite->w = w;
    sprite->r = 0x80;
    sprite->g = 0x80;
    sprite->b = 0x80;
    sprite->h = h;
    sprite->tpage = tpage;
    sprite->u = u;
    sprite->v = v;
    sprite->cx = cx;
    sprite->cy = cy;
    sprite->mx = mx;
    sprite->my = my;
    sprite->attribute = attribute;
}

typedef struct {
    u8 pad00[0x28];
    s32 attribute;
    s16 tpage;
    s16 clut;
    u8 pad30[4];
    s16 x;
    s16 y;
    s16 w;
    s16 h;
    u8 pad3C[8];
    u8 u;
    u8 v;
    u8 r, g, b;
} Unk8001D350;

void sprite_entry_setup(Unk8001D350 *obj, s16 a, s16 b, s16 c, s16 d, s16 e, u8 f, u8 g,
                   s16 h, s32 i) {
    obj->x = a;
    obj->y = b;
    obj->w = c;
    obj->r = 0x80;
    obj->g = 0x80;
    obj->b = 0x80;
    obj->h = d;
    obj->tpage = e;
    obj->u = f;
    obj->v = g;
    obj->clut = h;
    obj->attribute = i;
}

extern s16 g_player_quit;

s16 player_get_quit(void) {
    return g_player_quit;
}

typedef struct {
    u8 x;    /* source x in the font texture, in pixels */
    u8 info; /* bits 0-2: glyph row, bits 3-7: glyph width */
} FontGlyph;

extern FontGlyph g_font_glyphs_large[];
extern u8 *g_font_pixels;
extern u8 g_text_staging_buf[];

/* Copy the 4bpp glyph for character `ch` from the font texture into the text
 * buffer at `pos`; returns the advance width (0 for unknown characters). */
s32 font_copy_glyph(s32 pos, s32 ch, s32 wide) {
    u32 info;
    s32 width;
    s32 row;
    u8 *dst;
    u8 *src;
    s32 height;
    s32 y;
    s32 i;

    if (ch >= 0x80) {
        return 0;
    }
    info = g_font_glyphs_large[ch].info;
    ch = g_font_glyphs_large[ch].x;
    row = info & 7;
    width = info >> 3;
    if (wide) {
        dst = g_text_staging_buf + pos * 8;
        if (width > 16) {
            width = 16;
        }
    } else {
        dst = g_text_staging_buf + pos / 2;
    }
    if (width != 0) {
        ch /= 2;
        width++;
        if (row < 2) {
            pos = row * 16;
            height = 16;
        } else {
            pos = (row - 2) * 24 + 32;
            height = 24;
        }
        row = (width + 1) / 2; /* the original reuses row as the byte count */
        src = g_font_pixels + ch + pos * 128;
        for (y = 0; y < height; y++) {
            for (i = 0; i < row; i++) {
                dst[i] = src[i];
            }
            src += 0x80;
            dst += 0x80;
        }
    }
    return width;
}

s32 font_copy_glyph(s32 pos, s32 ch, s32 wide);

/* Render the current message string (g_text_window_message) into VRAM and build one
 * sprite per character. */
void text_window_build_message(void) {
    RECT rect;
    s32 len;
    s32 i;
    s32 width;

    len = strlen(g_text_window_message);
    g_text_window_char_count = len;
    rect.x = 0x100;
    rect.y = 0x1E0;
    rect.w = 0x40;
    rect.h = 0x18;
    memset(g_text_staging_buf, 0, 0xC00);
    g_text_window_text_width = 0;
    for (i = 0; i < len; i++) {
        width = font_copy_glyph(i, (u8)g_text_window_message[i], 1);
        g_text_window_char_widths[i] = width;
        g_text_window_text_width += width;
    }
    DrawSync(0);
    LoadImage(&rect, (u32 *)g_text_staging_buf);
    for (i = 0; i < len; i++) {
        g_text_window_char_sprites[i].w = g_text_window_char_widths[i];
        g_text_window_char_sprites[i].h = 0x18;
        g_text_window_char_sprites[i].tpage = GetTPage(0, 0, 0x100, 0x1E0);
        g_text_window_char_sprites[i].u = i << 4;
        g_text_window_char_sprites[i].v = 0xE0;
        if (i == 0 || g_text_window_next_frame_mode != 0) {
            g_text_window_char_sprites[i].cx = 0x10;
        } else {
            g_text_window_char_sprites[i].cx = 0x20;
        }
        g_text_window_char_sprites[i].cy = 0x1E1;
        g_text_window_char_sprites[i].r = 0x80;
        g_text_window_char_sprites[i].g = 0x80;
        g_text_window_char_sprites[i].b = 0x80;
        g_text_window_char_sprites[i].attribute = 0;
        g_text_window_char_sprites[i].mx = 0;
        g_text_window_char_sprites[i].my = 0;
        g_text_window_char_sprites[i].scalex = 0x1000;
        g_text_window_char_sprites[i].scaley = 0x1000;
        g_text_window_char_sprites[i].rotate = 0;
    }
}

extern FontGlyph g_font_glyphs_small[];

/* The glyph lookup comes before `dst` so loop.c hoists g_font_glyphs_small first (it
 * then gets fp, the g_text_staging_buf copy s7). Each branch has its own glyph pointer:
 * a single-set pseudo gets sched1's birthing priority, so the glyph address is
 * emitted after the dst address as in the original. Splitting `top` stops
 * combine folding (row * 8 + 0x50) * 128 into (row << 10) + 0x2800. */
/* Render `text` into the VRAM staging buffer and upload it at (x, y).
 * mode 0: large font via font_copy_glyph, mode 1: small 8px font. Returns the
 * rendered width in pixels. */
s32 font_render_string(char *text, s32 x, s32 y, s32 mode, s32 wide) {
    RECT rect;
    s32 i;
    s32 len;
    s32 pos;
    s32 ch;
    s32 advance;
    u8 info;
    u32 gx;
    s32 top;
    s32 width;
    s32 j;
    s32 k;
    u8 *dst;
    u8 *src;
    FontGlyph *glyph;
    FontGlyph *glyph2;

    memset(g_text_staging_buf, 0, 0xC00);
    len = strlen(text);
    pos = 0;
    for (i = 0; i < len; i++) {
        switch (mode) {
        case 0:
            if (wide) {
                pos += font_copy_glyph(i, (u8)text[i], wide);
            } else {
                pos += font_copy_glyph(pos, (u8)text[i], 0);
            }
            if (pos & 1) {
                pos++;
            }
            break;
        case 1:
            if (wide) {
                ch = (u8)text[i];
                if (ch >= 0x80) {
                    advance = 0;
                } else {
                    glyph = &g_font_glyphs_small[ch];
                    dst = g_text_staging_buf + i * 4;
                    info = glyph->info;
                    gx = glyph->x;
                    width = info >> 3;
                    top = info & 7;
                    if (width != 0) {
                        width++;
                        gx >>= 1;
                        top = top * 8 + 0x50;
                        src = g_font_pixels + gx + top * 128;
                        for (j = 0; j < width; j++) {
                            for (k = 0; k < 4; k++) {
                                dst[k] = src[k];
                            }
                            src += 0x80;
                            dst += 0x80;
                        }
                    }
                    advance = 8;
                }
            } else {
                ch = (u8)text[i];
                if (ch >= 0x80) {
                    advance = 0;
                } else {
                    glyph2 = &g_font_glyphs_small[ch];
                    dst = g_text_staging_buf + pos / 2;
                    info = glyph2->info;
                    gx = glyph2->x;
                    width = info >> 3;
                    top = info & 7;
                    if (width != 0) {
                        width++;
                        gx >>= 1;
                        top = top * 8 + 0x50;
                        src = g_font_pixels + gx + top * 128;
                        for (j = 0; j < width; j++) {
                            for (k = 0; k < 4; k++) {
                                dst[k] = src[k];
                            }
                            src += 0x80;
                            dst += 0x80;
                        }
                    }
                    advance = 8;
                }
            }
            pos += advance;
            if (pos & 1) {
                pos++;
            }
            break;
        }
    }
    rect.x = x;
    rect.y = y;
    rect.w = 0x40;
    switch (mode) {
    case 0:
        rect.h = 0x18;
        break;
    case 1:
        rect.h = 0x10;
        break;
    case 2:
        rect.h = 8;
        break;
    default:
        rect.h = 0;
        break;
    }
    DrawSync(0);
    LoadImage(&rect, (u32 *)g_text_staging_buf);
    return pos;
}

typedef struct {
    s16 msg;
    s16 unk2[5];
} MenuCell; /* 0xC */

typedef struct {
    char name[8];
    s16 options[4];
    u8 pad10[0x18];
} MsgRecord; /* 0x28 */

typedef struct {
    char s[8];
} Str8;

extern MenuCell g_site_grid[][24];
extern MsgRecord g_node_table[];
extern char *g_node_label_strings[];
extern char g_text_window_labels[4][0x12];
extern char g_text_window_unknown_str[];
extern char g_text_window_empty_str[];
/* Addressed absolutely here (gp-relative in other files). */
extern s16 g_site_cursor_on_node __attribute__((section(".data")));

/* Show the message title and its four option labels for menu cell (row, col). */
void text_window_load_cell(s32 row, s32 col) {
    s32 i;
    s32 msg;

    msg = g_site_grid[row][col].msg;
    if (msg >= 0) {
        strncpy(g_text_window_message, g_node_table[msg].name, 6);
        g_text_window_message[6] = 0;
        text_window_build_message();
        for (i = 0; i < 4; i++) {
            strncpy(g_text_window_labels[i], g_node_label_strings[g_node_table[g_site_grid[row][col].msg].options[i]], 15);
            g_text_window_labels[i][15] = 0;
        }
        font_render_string(g_text_window_labels[0], 0x180, 0x1E0, 1, 1);
        font_render_string(g_text_window_labels[1], 0x180, 0x1F0, 1, 1);
        font_render_string(g_text_window_labels[2], 0x1C0, 0x1E0, 1, 1);
        font_render_string(g_text_window_labels[3], 0x1C0, 0x1F0, 1, 1);
    } else {
        g_site_cursor_on_node = 0;
        *(Str8 *)g_text_window_message = *(Str8 *)g_text_window_unknown_str;
        text_window_build_message();
        for (i = 0; i < 4; i++) {
            g_text_window_labels[i][0] = g_text_window_empty_str[0];
        }
        font_render_string(g_text_window_empty_str, 0x180, 0x1E0, 1, 1);
        font_render_string(g_text_window_empty_str, 0x180, 0x1F0, 1, 1);
        font_render_string(g_text_window_empty_str, 0x1C0, 0x1E0, 1, 1);
        font_render_string(g_text_window_empty_str, 0x1C0, 0x1F0, 1, 1);
    }
}

extern FileEntry g_bin_file_table[];
extern u32 g_font_tim[];
extern GsSPRITE g_text_window_option_boxes[4];
extern GsSPRITE g_text_window_option_labels[4];

/* Load the text-window font and frame graphics and set up their sprites. */
void text_window_init(s16 x, s16 y) {
    RECT rect;
    TIM_IMAGE tim;
    u32 *frameData;
    u32 *iconData;
    u32 *pixels;
    s32 id;
    s32 i;

    id = cd_load_archive_entry(3, 1, g_bin_file_table, (s32)g_font_tim);
    while (cd_poll_load(id) == 0) {
        loading_anim_draw(0);
    }
    frameData = (u32 *)heap_alloc(0x1200);
    id = cd_load_archive_entry(3, 0, g_bin_file_table, (s32)frameData);
    while (cd_poll_load(id) == 0) {
        loading_anim_draw(0);
    }
    iconData = (u32 *)heap_alloc(g_bin_file_table[9].size);
    id = cd_load_archive_entry(3, 9, g_bin_file_table, (s32)iconData);

    OpenTIM(g_font_tim);
    ReadTIM(&tim);
    g_font_pixels = (u8 *)tim.paddr;
    LoadClut(tim.caddr, 0, 0x1E1);
    g_text_window_frame_mode = 0;
    g_text_window_next_frame_mode = 0;
    g_text_window_state = 0;
    g_text_window_x = x;
    g_text_window_y = y;

    OpenTIM(frameData);
    ReadTIM(&tim);
    pixels = tim.paddr;
    LoadClut(tim.caddr, 0, 0x1E2);
    rect.x = 0x140;
    rect.y = 0x1E0;
    rect.w = 0x40;
    rect.h = 0x20;
    LoadImage(&rect, pixels);

    while (cd_poll_load(id) == 0) {
        loading_anim_draw(0);
    }
    OpenTIM(iconData);
    ReadTIM(&tim);
    LoadClut(tim.caddr, 0, 0x1E4);
    rect.x = 0x3C0;
    rect.y = 0x1A0;
    rect.w = 0x3E;
    rect.h = 0x60;
    LoadImage(&rect, tim.paddr);

    g_text_window_bar_sprite1.w = 0x100;
    g_text_window_bar_sprite1.h = 8;
    g_text_window_bar_sprite1.tpage = GetTPage(0, 0, 0x140, 0x1E0);
    g_text_window_bar_sprite1.u = 0;
    g_text_window_bar_sprite1.v = 0xE8;
    g_text_window_bar_sprite1.cx = 0;
    g_text_window_bar_sprite1.cy = 0x1E2;
    g_text_window_bar_sprite1.attribute = 0x40000000;
    g_text_window_bar_sprite1.r = 0x80;
    g_text_window_bar_sprite1.g = 0x80;
    g_text_window_bar_sprite1.b = 0x80;
    g_text_window_bar_sprite1.mx = g_text_window_bar_sprite1.w / 2;
    g_text_window_bar_sprite1.my = 0;
    g_text_window_bar_sprite1.scalex = 0x1000;
    g_text_window_bar_sprite1.scaley = 0x1000;
    g_text_window_bar_sprite1.rotate = 0;

    g_text_window_bar_sprite0.w = 0xF8;
    g_text_window_bar_sprite0.h = 8;
    g_text_window_bar_sprite0.tpage = GetTPage(0, 0, 0x140, 0x1E0);
    g_text_window_bar_sprite0.u = 0;
    g_text_window_bar_sprite0.v = 0xE0;
    g_text_window_bar_sprite0.cx = 0;
    g_text_window_bar_sprite0.cy = 0x1E2;
    g_text_window_bar_sprite0.attribute = 0x40000000;
    g_text_window_bar_sprite0.r = 0x80;
    g_text_window_bar_sprite0.g = 0x80;
    g_text_window_bar_sprite0.b = 0x80;
    g_text_window_bar_sprite0.mx = g_text_window_bar_sprite0.w / 2;
    g_text_window_bar_sprite0.my = 0;
    g_text_window_bar_sprite0.scalex = 0x1000;
    g_text_window_bar_sprite0.scaley = 0x1000;
    g_text_window_bar_sprite0.rotate = 0;

    for (i = 0; i < 4; i++) {
        g_text_window_option_boxes[i].w = 0x88;
        g_text_window_option_boxes[i].h = 0x10;
        g_text_window_option_boxes[i].tpage = GetTPage(0, 0, 0x140, 0x1E0);
        g_text_window_option_boxes[i].u = 0;
        g_text_window_option_boxes[i].v = 0xF0;
        g_text_window_option_boxes[i].cy = 0x1E2;
        g_text_window_option_boxes[i].attribute = 0x40000000;
        g_text_window_option_boxes[i].cx = 0;
        g_text_window_option_boxes[i].r = 0x80;
        g_text_window_option_boxes[i].g = 0x80;
        g_text_window_option_boxes[i].b = 0x80;
        g_text_window_option_boxes[i].mx = 0x44;
        g_text_window_option_boxes[i].my = 0;
        g_text_window_option_boxes[i].scalex = 0x1000;
        g_text_window_option_boxes[i].scaley = 0x1000;
        g_text_window_option_boxes[i].rotate = 0;

        g_text_window_option_labels[i].w = 0x78;
        g_text_window_option_labels[i].h = 0x10;
        g_text_window_option_labels[i].cy = 0x1E1;
        g_text_window_option_labels[i].u = 0;
        g_text_window_option_labels[i].cx = 0;
        g_text_window_option_labels[i].attribute = 0;
        g_text_window_option_labels[i].r = 0x80;
        g_text_window_option_labels[i].g = 0x80;
        g_text_window_option_labels[i].b = 0x80;
        g_text_window_option_labels[i].mx = 0;
        g_text_window_option_labels[i].my = 0;
        g_text_window_option_labels[i].scalex = 0x1000;
        g_text_window_option_labels[i].scaley = 0x1000;
        g_text_window_option_labels[i].rotate = 0;
    }
    g_text_window_option_labels[0].tpage = 0x16;
    g_text_window_option_labels[0].v = 0xE0;
    g_text_window_option_labels[1].tpage = 0x16;
    g_text_window_option_labels[1].v = 0xF0;
    g_text_window_option_labels[2].tpage = 0x17;
    g_text_window_option_labels[2].v = 0xE0;
    g_text_window_option_labels[3].tpage = 0x17;
    g_text_window_option_labels[3].v = 0xF0;
    heap_free((s32)frameData);
    heap_free((s32)iconData);
}

/* Draw the text window (mode 0): frame, option highlight boxes and the
 * message characters. The frame is mirrored when the window is on the right. */
void text_window_draw_idle(void) {
    s32 textX;
    s32 textY;
    s32 blink;
    s32 i;

    textX = g_text_window_x;
    textY = g_text_window_y;
    blink = g_site_cursor_col - g_site_rotation;
    if (blink < 0) {
        blink += 8;
    }
    if (blink == 3) {
        blink = 1;
    } else {
        blink = 0;
    }
    if (g_text_window_frame_mode == 0) {
        if (g_text_window_x < 0xA0) {
            g_text_window_bar_sprite0.x = g_text_window_x - 0x9C;
            g_text_window_bar_sprite1.x = g_text_window_x + 0x4D;
            g_text_window_bar_sprite0.attribute &= ~0x800000;
            g_text_window_bar_sprite1.attribute &= ~0x800000;
            g_text_window_bar_sprite0.y = g_text_window_y + 0x12;
            g_text_window_bar_sprite1.y = g_text_window_y + 0xC;
            GsSortFastSprite(&g_text_window_bar_sprite0, &g_ot_2d[g_frame_buffer_index], g_text_window_char_count + 3);
            GsSortFastSprite(&g_text_window_bar_sprite1, &g_ot_2d[g_frame_buffer_index], g_text_window_char_count + 4);
        } else {
            g_text_window_bar_sprite0.attribute |= 0x800000;
            g_text_window_bar_sprite1.attribute |= 0x800000;
            g_text_window_bar_sprite0.x = g_text_window_x + (s16)(g_text_window_bar_sprite0.w / 2 - 0xA1);
            g_text_window_bar_sprite0.y = g_text_window_y - 0x66;
            g_text_window_bar_sprite1.x = g_text_window_x + (s16)(g_text_window_bar_sprite1.w / 2 - 0x198);
            g_text_window_bar_sprite1.y = g_text_window_y - 0x6C;
            GsSortSprite(&g_text_window_bar_sprite0, &g_ot_2d[g_frame_buffer_index], g_text_window_char_count + 3);
            GsSortSprite(&g_text_window_bar_sprite1, &g_ot_2d[g_frame_buffer_index], g_text_window_char_count + 4);
        }
        if (g_text_window_show_options != 0) {
            for (i = 0; i < 4; i++) {
                if (g_text_window_x < 0xA0) {
                    g_text_window_option_boxes[i].attribute &= ~0x800000;
                    if ((s16)((s16)g_site_cursor_row % 3) > 0 || blink) {
                        g_text_window_option_boxes[i].x = 0xB5 - i * 6;
                        g_text_window_option_boxes[i].y = i * 17 + (s16)(g_text_window_y + 0x14);
                        g_text_window_option_labels[i].x = 0xBC - i * 6;
                        g_text_window_option_labels[i].y = i * 17 + (s16)(g_text_window_y + 0x16);
                    } else {
                        g_text_window_option_boxes[i].x = 0xB5 - i * 6;
                        g_text_window_option_boxes[i].y = i * 17 + (s16)(g_text_window_y - 0x2F);
                        g_text_window_option_labels[i].x = 0xBC - i * 6;
                        g_text_window_option_labels[i].y = i * 17 + (s16)(g_text_window_y - 0x2D);
                    }
                    GsSortFastSprite(&g_text_window_option_labels[i], &g_ot_2d[g_frame_buffer_index], g_text_window_char_count + 2);
                    GsSortFastSprite(&g_text_window_option_boxes[i], &g_ot_2d[g_frame_buffer_index], g_text_window_char_count + 2);
                } else {
                    g_text_window_option_boxes[i].attribute |= 0x800000;
                    if ((s16)((s16)g_site_cursor_row % 3) > 0 || blink) {
                        g_text_window_option_boxes[i].x = i * 6 - 0x58;
                        g_text_window_option_boxes[i].y = g_text_window_y + i * 17 - 0x64;
                        g_text_window_option_labels[i].x = i * 6 + 0xD;
                        g_text_window_option_labels[i].y = i * 17 + (s16)(g_text_window_y + 0x16);
                    } else {
                        g_text_window_option_boxes[i].x = i * 6 - 0x58;
                        g_text_window_option_boxes[i].y = g_text_window_y + i * 17 - 0xA7;
                        g_text_window_option_labels[i].x = i * 6 + 0xD;
                        g_text_window_option_labels[i].y = i * 17 + (s16)(g_text_window_y - 0x2D);
                    }
                    GsSortFastSprite(&g_text_window_option_labels[i], &g_ot_2d[g_frame_buffer_index], g_text_window_char_count + 2);
                    GsSortSprite(&g_text_window_option_boxes[i], &g_ot_2d[g_frame_buffer_index], g_text_window_char_count + 2);
                }
            }
        } else if (g_text_window_x < 0xA0) {
            g_text_window_option_boxes[0].attribute &= ~0x800000;
            if ((s16)((s16)g_site_cursor_row % 3) > 0 || blink) {
                g_text_window_option_boxes[0].x = 0xB5;
                g_text_window_option_labels[0].x = 0xBC;
                g_text_window_option_boxes[0].y = g_text_window_y + 0x14;
                g_text_window_option_labels[0].y = g_text_window_y + 0x16;
            } else {
                g_text_window_option_boxes[0].x = 0xA3;
                g_text_window_option_labels[0].x = 0xAA;
                g_text_window_option_boxes[0].y = g_text_window_y + 4;
                g_text_window_option_labels[0].y = g_text_window_y + 6;
            }
            GsSortFastSprite(&g_text_window_option_labels[0], &g_ot_2d[g_frame_buffer_index], g_text_window_char_count + 2);
            GsSortFastSprite(&g_text_window_option_boxes[0], &g_ot_2d[g_frame_buffer_index], g_text_window_char_count + 2);
        } else {
            g_text_window_option_boxes[0].attribute |= 0x800000;
            if ((s16)((s16)g_site_cursor_row % 3) > 0 || blink) {
                g_text_window_option_boxes[0].x = -0x58;
                g_text_window_option_labels[0].x = 0xD;
                g_text_window_option_boxes[0].y = g_text_window_y - 0x64;
                g_text_window_option_labels[0].y = g_text_window_y + 0x16;
            } else {
                g_text_window_option_boxes[0].x = -0x46;
                g_text_window_option_labels[0].x = 0x1F;
                g_text_window_option_boxes[0].y = g_text_window_y - 0x74;
                g_text_window_option_labels[0].y = g_text_window_y + 6;
            }
            GsSortFastSprite(&g_text_window_option_labels[0], &g_ot_2d[g_frame_buffer_index], g_text_window_char_count + 2);
            GsSortSprite(&g_text_window_option_boxes[0], &g_ot_2d[g_frame_buffer_index], g_text_window_char_count + 2);
        }
    }
    for (i = 0; i < g_text_window_char_count; i++) {
        g_text_window_char_sprites[i].x = textX;
        g_text_window_char_sprites[i].y = textY;
        textX += g_text_window_char_widths[i];
        GsSortFastSprite(&g_text_window_char_sprites[i], &g_ot_2d[g_frame_buffer_index], i + 2);
    }
}

/* Draw the text window while it opens (mode 1): the frame slides in over four
 * frames and the characters fly from the old to the new text position. */
void text_window_draw_leave(void) {
    s32 blink;
    s32 pos;
    s32 i;

    blink = g_site_prev_cursor_row - g_site_rotation;
    if (blink < 0) {
        blink += 8;
    }
    if (blink == 3) {
        blink = 1;
    } else {
        blink = 0;
    }
    if (g_text_window_frame_mode == 0 && g_text_window_anim_frame < 4) {
        if (g_text_window_x < 0xA0) {
            g_text_window_bar_sprite0.attribute &= ~0x800000;
            g_text_window_bar_sprite1.attribute &= ~0x800000;
            g_text_window_bar_sprite0.x = g_text_window_x + (s16)((-0x5C - g_text_window_x) * g_text_window_anim_frame / 4 - 0x9C);
            g_text_window_bar_sprite0.y = g_text_window_y + 0x12;
            g_text_window_bar_sprite1.x = g_text_window_x + (s16)((0xF3 - g_text_window_x) * g_text_window_anim_frame / 4 + 0x4D);
            g_text_window_bar_sprite1.y = g_text_window_y + 0xC;
            GsSortFastSprite(&g_text_window_bar_sprite0, &g_ot_2d[g_frame_buffer_index], g_text_window_char_count + 2);
            GsSortFastSprite(&g_text_window_bar_sprite1, &g_ot_2d[g_frame_buffer_index], g_text_window_char_count + 3);
        } else {
            g_text_window_bar_sprite0.attribute |= 0x800000;
            g_text_window_bar_sprite1.attribute |= 0x800000;
            g_text_window_bar_sprite0.x = g_text_window_x + (s16)((0x141 - g_text_window_x) * g_text_window_anim_frame / 4 - 1) +
                           (s16)(g_text_window_bar_sprite0.w / 2 - 0xA0);
            g_text_window_bar_sprite0.y = g_text_window_y - 0x66;
            g_text_window_bar_sprite1.x = g_text_window_x + (s16)((-8 - g_text_window_x) * g_text_window_anim_frame / 4 - 0xF8) +
                           (s16)(g_text_window_bar_sprite1.w / 2 - 0xA0);
            g_text_window_bar_sprite1.y = g_text_window_y - 0x6C;
            GsSortSprite(&g_text_window_bar_sprite0, &g_ot_2d[g_frame_buffer_index], g_text_window_char_count + 2);
            GsSortSprite(&g_text_window_bar_sprite1, &g_ot_2d[g_frame_buffer_index], g_text_window_char_count + 3);
        }
        if (g_text_window_show_options != 0) {
            for (i = 0; i < 4; i++) {
                if (g_text_window_x < 0xA0) {
                    g_text_window_option_boxes[i].attribute &= ~0x800000;
                    if ((s16)((s16)g_site_prev_cursor_row % 3) > 0 || blink) {
                        g_text_window_option_boxes[i].x = (s16)(g_text_window_anim_frame * 150 / 4 + 0xB5) - i * 6;
                        g_text_window_option_boxes[i].y = i * 17 + (s16)(g_text_window_y + 0x14);
                        g_text_window_option_labels[i].x = (s16)(g_text_window_anim_frame * 150 / 4 + 0xBC) - i * 6;
                        g_text_window_option_labels[i].y = i * 17 + (s16)(g_text_window_y + 0x16);
                    } else {
                        g_text_window_option_boxes[i].x = (s16)(g_text_window_anim_frame * 150 / 4 + 0xB5) - i * 6;
                        g_text_window_option_boxes[i].y = i * 17 + (s16)(g_text_window_y - 0x2F);
                        g_text_window_option_labels[i].x = (s16)(g_text_window_anim_frame * 150 / 4 + 0xBC) - i * 6;
                        g_text_window_option_labels[i].y = i * 17 + (s16)(g_text_window_y - 0x2D);
                    }
                    GsSortFastSprite(&g_text_window_option_labels[i], &g_ot_2d[g_frame_buffer_index], g_text_window_char_count + 2);
                    GsSortFastSprite(&g_text_window_option_boxes[i], &g_ot_2d[g_frame_buffer_index], g_text_window_char_count + 2);
                } else {
                    g_text_window_option_boxes[i].attribute |= 0x800000;
                    if ((s16)((s16)g_site_prev_cursor_row % 3) > 0 || blink) {
                        g_text_window_option_boxes[i].x = i * 6 - (s16)(g_text_window_anim_frame * 158 / 4 + 0x58);
                        g_text_window_option_boxes[i].y = g_text_window_y + i * 17 - 0x64;
                        g_text_window_option_labels[i].x = i * 6 - (s16)(g_text_window_anim_frame * 158 / 4 - 0xD);
                        g_text_window_option_labels[i].y = i * 17 + (s16)(g_text_window_y + 0x16);
                    } else {
                        g_text_window_option_boxes[i].x = i * 6 - (s16)(g_text_window_anim_frame * 158 / 4 + 0x58);
                        g_text_window_option_boxes[i].y = g_text_window_y + i * 17 - 0xA7;
                        g_text_window_option_labels[i].x = i * 6 - (s16)(g_text_window_anim_frame * 158 / 4 - 0xD);
                        g_text_window_option_labels[i].y = i * 17 + (s16)(g_text_window_y - 0x2D);
                    }
                    GsSortFastSprite(&g_text_window_option_labels[i], &g_ot_2d[g_frame_buffer_index], g_text_window_char_count + 2);
                    GsSortSprite(&g_text_window_option_boxes[i], &g_ot_2d[g_frame_buffer_index], g_text_window_char_count + 2);
                }
            }
        } else if (g_text_window_x < 0xA0) {
            g_text_window_option_boxes[0].attribute &= ~0x800000;
            if ((s16)((s16)g_site_prev_cursor_row % 3) > 0 || blink) {
                g_text_window_option_boxes[0].x = g_text_window_anim_frame * 150 / 4 + 0xB5;
                g_text_window_option_boxes[0].y = g_text_window_y + 0x14;
                g_text_window_option_labels[0].x = g_text_window_anim_frame * 150 / 4 + 0xBC;
                g_text_window_option_labels[0].y = g_text_window_y + 0x16;
            } else {
                g_text_window_option_boxes[0].x = g_text_window_anim_frame * 150 / 4 + 0xA3;
                g_text_window_option_boxes[0].y = g_text_window_y + 4;
                g_text_window_option_labels[0].x = g_text_window_anim_frame * 150 / 4 + 0xAA;
                g_text_window_option_labels[0].y = g_text_window_y + 6;
            }
            GsSortFastSprite(&g_text_window_option_labels[0], &g_ot_2d[g_frame_buffer_index], g_text_window_char_count + 2);
            GsSortFastSprite(&g_text_window_option_boxes[0], &g_ot_2d[g_frame_buffer_index], g_text_window_char_count + 2);
        } else {
            g_text_window_option_boxes[0].attribute |= 0x800000;
            if ((s16)((s16)g_site_prev_cursor_row % 3) > 0 || blink) {
                g_text_window_option_boxes[0].x = -0x58 - g_text_window_anim_frame * 158 / 4;
                g_text_window_option_boxes[0].y = g_text_window_y - 0x64;
                g_text_window_option_labels[0].x = 0xD - g_text_window_anim_frame * 158 / 4;
                g_text_window_option_labels[0].y = g_text_window_y + 0x16;
            } else {
                g_text_window_option_boxes[0].x = -0x46 - g_text_window_anim_frame * 158 / 4;
                g_text_window_option_boxes[0].y = g_text_window_y - 0x74;
                g_text_window_option_labels[0].x = 0x1F - g_text_window_anim_frame * 158 / 4;
                g_text_window_option_labels[0].y = g_text_window_y + 6;
            }
            GsSortFastSprite(&g_text_window_option_labels[0], &g_ot_2d[g_frame_buffer_index], g_text_window_char_count + 2);
            GsSortSprite(&g_text_window_option_boxes[0], &g_ot_2d[g_frame_buffer_index], g_text_window_char_count + 2);
        }
    }

    /* Characters: each one flies in over four steps once its delay expires. */
    if (g_text_window_char_step[0] >= 4) {
        g_text_window_char_sprites[0].x = g_text_window_target_x;
        g_text_window_char_sprites[0].y = g_text_window_target_y;
    } else {
        g_text_window_char_sprites[0].x = g_text_window_x + (g_text_window_target_x - g_text_window_x) * g_text_window_char_step[0] / 4;
        g_text_window_char_sprites[0].y = g_text_window_y + (g_text_window_target_y - g_text_window_y) * g_text_window_char_step[0] / 4;
    }
    g_text_window_char_step[0]++;
    GsSortFastSprite(&g_text_window_char_sprites[0], &g_ot_2d[g_frame_buffer_index], 2);
    pos = g_text_window_char_widths[0];
    for (i = 1; i < g_text_window_char_count; i++) {
        if (g_text_window_char_delay[i] != 0) {
            g_text_window_char_delay[i]--;
            for (; i < g_text_window_char_count; i++) {
                g_text_window_char_sprites[i].y = g_text_window_y;
                g_text_window_char_sprites[i].x = g_text_window_x + pos;
                GsSortFastSprite(&g_text_window_char_sprites[i], &g_ot_2d[g_frame_buffer_index], i + 2);
                pos += g_text_window_char_widths[i];
            }
            break;
        }
        if (g_text_window_char_step[i] >= 4) {
            g_text_window_char_sprites[i].x = g_text_window_target_x;
            g_text_window_char_sprites[i].y = g_text_window_target_y;
        } else {
            g_text_window_char_sprites[i].x = g_text_window_x + (g_text_window_target_x - g_text_window_x) * g_text_window_char_step[i] / 4;
            g_text_window_char_sprites[i].y = g_text_window_y + (g_text_window_target_y - g_text_window_y) * g_text_window_char_step[i] / 4;
        }
        g_text_window_char_step[i]++;
        GsSortFastSprite(&g_text_window_char_sprites[i], &g_ot_2d[g_frame_buffer_index], i + 2);
        pos += g_text_window_char_widths[i];
    }
    g_text_window_anim_frame++;
    if (g_text_window_char_step[g_text_window_char_count - 1] >= 5) {
        if (g_text_window_frame_mode == 2) {
            g_text_window_frame_mode = 0;
        }
        g_text_window_state = 2;
        g_text_window_anim_frame = 0;
        g_text_window_x = g_text_window_target_x;
        g_text_window_y = g_text_window_target_y;
        strcpy(g_text_window_message, g_text_window_next_name);
        if (g_text_window_next_frame_mode == 1) {
            g_text_window_frame_mode = 1;
        } else if (g_text_window_next_frame_mode == 2) {
            g_text_window_frame_mode = 0;
            g_text_window_next_frame_mode = 0;
        }
        text_window_build_message();
        font_render_string(g_text_window_labels[0], 0x180, 0x1E0, 1, 1);
        font_render_string(g_text_window_labels[1], 0x180, 0x1F0, 1, 1);
        font_render_string(g_text_window_labels[2], 0x1C0, 0x1E0, 1, 1);
        font_render_string(g_text_window_labels[3], 0x1C0, 0x1F0, 1, 1);
        for (i = 0; i < g_text_window_char_count; i++) {
            g_text_window_char_delay[i] = 1;
            g_text_window_char_step[i] = 0;
        }
        snd_stop_anim_sfx();
    }
}

/* Draw the text window while it closes (mode 2): the frame slides out over
 * four frames and the characters collapse onto the first one. */
void text_window_draw_enter(void) {
    s32 blink;
    s32 i;
    s32 px;
    s32 py;

    blink = g_site_cursor_col - g_site_rotation;
    if (blink < 0) {
        blink += 8;
    }
    if (blink == 3) {
        blink = 1;
    } else {
        blink = 0;
    }
    if (g_text_window_frame_mode == 0) {
        if (g_text_window_x < 0xA0) {
            if (g_text_window_anim_frame < 4) {
                g_text_window_bar_sprite0.attribute &= ~0x800000;
                g_text_window_bar_sprite1.attribute &= ~0x800000;
                g_text_window_bar_sprite0.x = (g_text_window_x + 0x5C) * g_text_window_anim_frame / 4 - 0xF8;
                g_text_window_bar_sprite0.y = g_text_window_y + 0x12;
                g_text_window_bar_sprite1.x = (g_text_window_x - 0xF3) * g_text_window_anim_frame / 4 + 0x140;
                g_text_window_bar_sprite1.y = g_text_window_y + 0xC;
                GsSortFastSprite(&g_text_window_bar_sprite0, &g_ot_2d[g_frame_buffer_index], g_text_window_char_count + 2);
                GsSortFastSprite(&g_text_window_bar_sprite1, &g_ot_2d[g_frame_buffer_index], g_text_window_char_count + 3);
            } else {
                g_text_window_bar_sprite0.x = g_text_window_x - 0x9C;
                g_text_window_bar_sprite1.x = g_text_window_x + 0x4D;
                g_text_window_bar_sprite0.attribute &= ~0x800000;
                g_text_window_bar_sprite1.attribute &= ~0x800000;
                g_text_window_bar_sprite0.y = g_text_window_y + 0x12;
                g_text_window_bar_sprite1.y = g_text_window_y + 0xC;
                GsSortFastSprite(&g_text_window_bar_sprite0, &g_ot_2d[g_frame_buffer_index], g_text_window_char_count + 2);
                GsSortFastSprite(&g_text_window_bar_sprite1, &g_ot_2d[g_frame_buffer_index], g_text_window_char_count + 3);
            }
        } else if (g_text_window_anim_frame < 4) {
            g_text_window_bar_sprite0.attribute |= 0x800000;
            g_text_window_bar_sprite1.attribute |= 0x800000;
            g_text_window_bar_sprite0.x = (g_text_window_x - 0x141) * g_text_window_anim_frame / 4 + (s16)(g_text_window_bar_sprite0.w / 2 + 0xA0);
            g_text_window_bar_sprite0.y = g_text_window_y - 0x66;
            g_text_window_bar_sprite1.x = (g_text_window_x + 8) * g_text_window_anim_frame / 4 + (s16)(g_text_window_bar_sprite1.w / 2 - 0x1A0);
            g_text_window_bar_sprite1.y = g_text_window_y - 0x6C;
            GsSortSprite(&g_text_window_bar_sprite0, &g_ot_2d[g_frame_buffer_index], g_text_window_char_count + 2);
            GsSortSprite(&g_text_window_bar_sprite1, &g_ot_2d[g_frame_buffer_index], g_text_window_char_count + 3);
        } else {
            g_text_window_bar_sprite0.attribute |= 0x800000;
            g_text_window_bar_sprite1.attribute |= 0x800000;
            g_text_window_bar_sprite0.x = g_text_window_x + (s16)(g_text_window_bar_sprite0.w / 2 - 0xA1);
            g_text_window_bar_sprite0.y = g_text_window_y - 0x66;
            g_text_window_bar_sprite1.x = g_text_window_x + (s16)(g_text_window_bar_sprite1.w / 2 - 0x198);
            g_text_window_bar_sprite1.y = g_text_window_y - 0x6C;
            GsSortSprite(&g_text_window_bar_sprite0, &g_ot_2d[g_frame_buffer_index], g_text_window_char_count + 2);
            GsSortSprite(&g_text_window_bar_sprite1, &g_ot_2d[g_frame_buffer_index], g_text_window_char_count + 3);
        }
        if (g_text_window_show_options != 0) {
            for (i = 0; i < 4; i++) {
                if (g_text_window_anim_frame < 4) {
                    if (g_text_window_x < 0xA0) {
                        g_text_window_option_boxes[i].attribute &= ~0x800000;
                        if ((s16)((s16)g_site_cursor_row % 3) > 0 || blink) {
                            g_text_window_option_boxes[i].x = (s16)((4 - g_text_window_anim_frame) * 150 / 4 + 0xB5) - i * 6;
                            g_text_window_option_boxes[i].y = i * 17 + (s16)(g_text_window_y + 0x14);
                            g_text_window_option_labels[i].x = (s16)((4 - g_text_window_anim_frame) * 150 / 4 + 0xBC) - i * 6;
                            g_text_window_option_labels[i].y = i * 17 + (s16)(g_text_window_y + 0x16);
                        } else {
                            g_text_window_option_boxes[i].x = (s16)((4 - g_text_window_anim_frame) * 150 / 4 + 0xB5) - i * 6;
                            g_text_window_option_boxes[i].y = i * 17 + (s16)(g_text_window_y - 0x2F);
                            g_text_window_option_labels[i].x = (s16)((4 - g_text_window_anim_frame) * 150 / 4 + 0xBC) - i * 6;
                            g_text_window_option_labels[i].y = i * 17 + (s16)(g_text_window_y - 0x2D);
                        }
                        GsSortFastSprite(&g_text_window_option_labels[i], &g_ot_2d[g_frame_buffer_index], g_text_window_char_count + 2);
                        GsSortFastSprite(&g_text_window_option_boxes[i], &g_ot_2d[g_frame_buffer_index], g_text_window_char_count + 2);
                    } else {
                        g_text_window_option_boxes[i].attribute |= 0x800000;
                        if ((s16)((s16)g_site_cursor_row % 3) > 0 || blink) {
                            g_text_window_option_boxes[i].x = i * 6 - (s16)((4 - g_text_window_anim_frame) * 158 / 4 + 0x58);
                            g_text_window_option_boxes[i].y = g_text_window_y + i * 17 - 0x64;
                            g_text_window_option_labels[i].x = i * 6 - (s16)((4 - g_text_window_anim_frame) * 158 / 4 - 0xD);
                            g_text_window_option_labels[i].y = i * 17 + (s16)(g_text_window_y + 0x16);
                        } else {
                            g_text_window_option_boxes[i].x = i * 6 - (s16)((4 - g_text_window_anim_frame) * 158 / 4 + 0x58);
                            g_text_window_option_boxes[i].y = g_text_window_y + i * 17 - 0xA7;
                            g_text_window_option_labels[i].x = i * 6 - (s16)((4 - g_text_window_anim_frame) * 158 / 4 - 0xD);
                            g_text_window_option_labels[i].y = i * 17 + (s16)(g_text_window_y - 0x2D);
                        }
                        GsSortFastSprite(&g_text_window_option_labels[i], &g_ot_2d[g_frame_buffer_index], g_text_window_char_count + 2);
                        GsSortSprite(&g_text_window_option_boxes[i], &g_ot_2d[g_frame_buffer_index], g_text_window_char_count + 2);
                    }
                } else if (g_text_window_x < 0xA0) {
                    g_text_window_option_boxes[i].attribute &= ~0x800000;
                    if ((s16)((s16)g_site_cursor_row % 3) > 0 || blink) {
                        g_text_window_option_boxes[i].x = 0xB5 - i * 6;
                        g_text_window_option_boxes[i].y = i * 17 + (s16)(g_text_window_y + 0x14);
                        g_text_window_option_labels[i].x = 0xBC - i * 6;
                        g_text_window_option_labels[i].y = i * 17 + (s16)(g_text_window_y + 0x16);
                    } else {
                        g_text_window_option_boxes[i].x = 0xB5 - i * 6;
                        g_text_window_option_boxes[i].y = i * 17 + (s16)(g_text_window_y - 0x2F);
                        g_text_window_option_labels[i].x = 0xBC - i * 6;
                        g_text_window_option_labels[i].y = i * 17 + (s16)(g_text_window_y - 0x2D);
                    }
                    GsSortFastSprite(&g_text_window_option_labels[i], &g_ot_2d[g_frame_buffer_index], g_text_window_char_count + 2);
                    GsSortFastSprite(&g_text_window_option_boxes[i], &g_ot_2d[g_frame_buffer_index], g_text_window_char_count + 2);
                } else {
                    g_text_window_option_boxes[i].attribute |= 0x800000;
                    if ((s16)((s16)g_site_cursor_row % 3) > 0 || blink) {
                        g_text_window_option_boxes[i].x = i * 6 - 0x58;
                        g_text_window_option_boxes[i].y = g_text_window_y + i * 17 - 0x64;
                        g_text_window_option_labels[i].x = i * 6 + 0xD;
                        g_text_window_option_labels[i].y = i * 17 + (s16)(g_text_window_y + 0x16);
                    } else {
                        g_text_window_option_boxes[i].x = i * 6 - 0x58;
                        g_text_window_option_boxes[i].y = g_text_window_y + i * 17 - 0xA7;
                        g_text_window_option_labels[i].x = i * 6 + 0xD;
                        g_text_window_option_labels[i].y = i * 17 + (s16)(g_text_window_y - 0x2D);
                    }
                    GsSortFastSprite(&g_text_window_option_labels[i], &g_ot_2d[g_frame_buffer_index], g_text_window_char_count + 2);
                    GsSortSprite(&g_text_window_option_boxes[i], &g_ot_2d[g_frame_buffer_index], g_text_window_char_count + 2);
                }
            }
        } else if (g_text_window_anim_frame < 4) {
            if (g_text_window_x < 0xA0) {
                g_text_window_option_boxes[0].attribute &= ~0x800000;
                if ((s16)((s16)g_site_cursor_row % 3) > 0 || blink) {
                    g_text_window_option_boxes[0].x = (4 - g_text_window_anim_frame) * 150 / 4 + 0xB5;
                    g_text_window_option_boxes[0].y = g_text_window_y + 0x14;
                    g_text_window_option_labels[0].x = (4 - g_text_window_anim_frame) * 150 / 4 + 0xBC;
                    g_text_window_option_labels[0].y = g_text_window_y + 0x16;
                } else {
                    g_text_window_option_boxes[0].x = (4 - g_text_window_anim_frame) * 150 / 4 + 0xA3;
                    g_text_window_option_boxes[0].y = g_text_window_y + 4;
                    g_text_window_option_labels[0].x = (4 - g_text_window_anim_frame) * 150 / 4 + 0xAA;
                    g_text_window_option_labels[0].y = g_text_window_y + 6;
                }
                GsSortFastSprite(&g_text_window_option_labels[0], &g_ot_2d[g_frame_buffer_index], g_text_window_char_count + 2);
                GsSortFastSprite(&g_text_window_option_boxes[0], &g_ot_2d[g_frame_buffer_index], g_text_window_char_count + 2);
            } else {
                g_text_window_option_boxes[0].attribute |= 0x800000;
                if ((s16)((s16)g_site_cursor_row % 3) > 0 || blink) {
                    g_text_window_option_boxes[0].x = -0x58 - (4 - g_text_window_anim_frame) * 158 / 4;
                    g_text_window_option_boxes[0].y = g_text_window_y - 0x64;
                    g_text_window_option_labels[0].x = 0xD - (4 - g_text_window_anim_frame) * 158 / 4;
                    g_text_window_option_labels[0].y = g_text_window_y + 0x16;
                } else {
                    g_text_window_option_boxes[0].x = -0x46 - (4 - g_text_window_anim_frame) * 158 / 4;
                    g_text_window_option_boxes[0].y = g_text_window_y - 0x74;
                    g_text_window_option_labels[0].x = 0x1F - (4 - g_text_window_anim_frame) * 158 / 4;
                    g_text_window_option_labels[0].y = g_text_window_y + 6;
                }
                GsSortFastSprite(&g_text_window_option_labels[0], &g_ot_2d[g_frame_buffer_index], g_text_window_char_count + 2);
                GsSortSprite(&g_text_window_option_boxes[0], &g_ot_2d[g_frame_buffer_index], g_text_window_char_count + 2);
            }
        } else if (g_text_window_x < 0xA0) {
            g_text_window_option_boxes[0].attribute &= ~0x800000;
            if ((s16)((s16)g_site_cursor_row % 3) > 0 || blink) {
                g_text_window_option_boxes[0].x = 0xB5;
                g_text_window_option_labels[0].x = 0xBC;
                g_text_window_option_boxes[0].y = g_text_window_y + 0x14;
                g_text_window_option_labels[0].y = g_text_window_y + 0x16;
            } else {
                g_text_window_option_boxes[0].x = 0xA3;
                g_text_window_option_labels[0].x = 0xAA;
                g_text_window_option_boxes[0].y = g_text_window_y + 4;
                g_text_window_option_labels[0].y = g_text_window_y + 6;
            }
            GsSortFastSprite(&g_text_window_option_labels[0], &g_ot_2d[g_frame_buffer_index], g_text_window_char_count + 2);
            GsSortFastSprite(&g_text_window_option_boxes[0], &g_ot_2d[g_frame_buffer_index], g_text_window_char_count + 2);
        } else {
            g_text_window_option_boxes[0].attribute |= 0x800000;
            if ((s16)((s16)g_site_cursor_row % 3) > 0 || blink) {
                g_text_window_option_boxes[0].x = -0x58;
                g_text_window_option_labels[0].x = 0xD;
                g_text_window_option_boxes[0].y = g_text_window_y - 0x64;
                g_text_window_option_labels[0].y = g_text_window_y + 0x16;
            } else {
                g_text_window_option_boxes[0].x = -0x46;
                g_text_window_option_labels[0].x = 0x1F;
                g_text_window_option_boxes[0].y = g_text_window_y - 0x74;
                g_text_window_option_labels[0].y = g_text_window_y + 6;
            }
            GsSortFastSprite(&g_text_window_option_labels[0], &g_ot_2d[g_frame_buffer_index], g_text_window_char_count + 2);
            GsSortSprite(&g_text_window_option_boxes[0], &g_ot_2d[g_frame_buffer_index], g_text_window_char_count + 2);
        }
    }

    /* Characters: each one slides back onto the previous one. */
    g_text_window_char_sprites[0].x = g_text_window_target_x;
    g_text_window_char_sprites[0].y = g_text_window_target_y;
    px = g_text_window_target_x;
    py = g_text_window_target_y;
    GsSortFastSprite(&g_text_window_char_sprites[0], &g_ot_2d[g_frame_buffer_index], 2);
    for (i = 1; i < g_text_window_char_count; i++) {
        if (g_text_window_char_delay[i] != 0) {
            g_text_window_char_delay[i]--;
            for (; i < g_text_window_char_count; i++) {
                g_text_window_char_sprites[i].x = px;
                g_text_window_char_sprites[i].y = py;
                GsSortFastSprite(&g_text_window_char_sprites[i], &g_ot_2d[g_frame_buffer_index], i + 2);
            }
            break;
        }
        if (g_text_window_char_step[i] >= 2) {
            g_text_window_char_sprites[i].x = px + g_text_window_char_widths[i];
            g_text_window_char_sprites[i].y = py;
            px = g_text_window_char_sprites[i].x;
        } else {
            g_text_window_char_sprites[i].x = px + g_text_window_char_widths[i] * g_text_window_char_step[i] / 2;
            g_text_window_char_sprites[i].y = py;
            px = g_text_window_char_sprites[i].x;
        }
        GsSortFastSprite(&g_text_window_char_sprites[i], &g_ot_2d[g_frame_buffer_index], i + 2);
        g_text_window_char_step[i]++;
    }
    g_text_window_anim_frame++;
    if (g_text_window_char_step[g_text_window_char_count - 1] >= 3 && g_text_window_anim_frame >= 5) {
        g_text_window_state = 0;
        g_text_window_anim_frame = 0;
        g_text_window_busy = 0;
    }
}

void text_window_draw_select(s16 x, s16 y, s32 reset) {
    s32 i;

    g_text_window_frame_mode = 2;
    g_text_window_busy = 1;
    g_text_window_x = x;
    g_text_window_y = y;
    if (x < 0xA0) {
        if (g_text_window_anim_frame < 4) {
            g_text_window_bar_sprite0.x = x + g_text_window_text_width - (x + g_text_window_text_width + 14) * g_text_window_anim_frame / 4 - 210;
            g_text_window_bar_sprite0.y = y + 7;
            GsSortFastSprite(&g_text_window_bar_sprite0, &g_ot_2d[g_frame_buffer_index], g_text_window_char_count + 2);
        }
    } else if (g_text_window_anim_frame < 4) {
        g_text_window_bar_sprite1.x = x + (s16)((334 - x) * g_text_window_anim_frame / 4 - 14);
        g_text_window_bar_sprite1.y = y + 7;
        GsSortFastSprite(&g_text_window_bar_sprite1, &g_ot_2d[g_frame_buffer_index], g_text_window_char_count + 2);
    }
    g_text_window_anim_frame++;
    text_window_draw_idle();
    if (reset) {
        g_text_window_state = 1;
        g_text_window_anim_frame = 0;
        g_text_window_busy = 1;
        for (i = 0; i < g_text_window_char_count; i++) {
            g_text_window_char_delay[i] = 1;
            g_text_window_char_step[i] = 0;
        }
    }
}

/* Set up and draw the notice banner (two 160x48 halves and an 88x24 label) that
 * site_run state 29 shows when a keyword link finds no node; waits for confirm. */
void site_draw_notice(void) {
    s32 i;
    GsOT *ot;

    g_site_notice_sprites[0].x = 0;
    g_site_notice_sprites[0].y = 0x6F;
    g_site_notice_sprites[0].w = 0xA0;
    g_site_notice_sprites[0].h = 0x30;
    g_site_notice_sprites[0].tpage = GetTPage(0, 0, 0x3C0, 0x1A0);
    g_site_notice_sprites[0].u = 0;
    g_site_notice_sprites[0].v = 0xA0;
    g_site_notice_sprites[0].cx = 0x10;
    g_site_notice_sprites[0].cy = 0x1E4;
    g_site_notice_sprites[0].attribute = 0x40000000;
    g_site_notice_sprites[0].r = 0x80;
    g_site_notice_sprites[0].g = 0x80;
    g_site_notice_sprites[0].b = 0x80;
    g_site_notice_sprites[0].mx = 0;
    g_site_notice_sprites[0].my = 0;
    g_site_notice_sprites[0].scalex = 0x1000;
    g_site_notice_sprites[0].scaley = 0x1000;
    g_site_notice_sprites[0].rotate = 0;

    g_site_notice_sprites[1].x = 0xA0;
    g_site_notice_sprites[1].y = 0x6F;
    g_site_notice_sprites[1].w = 0xA0;
    g_site_notice_sprites[1].h = 0x30;
    g_site_notice_sprites[1].tpage = GetTPage(0, 0, 0x3C0, 0x1A0);
    g_site_notice_sprites[1].u = 0;
    g_site_notice_sprites[1].v = 0xD0;
    g_site_notice_sprites[1].cx = 0x10;
    g_site_notice_sprites[1].cy = 0x1E4;
    g_site_notice_sprites[1].attribute = 0x40000000;
    g_site_notice_sprites[1].r = 0x80;
    g_site_notice_sprites[1].g = 0x80;
    g_site_notice_sprites[1].b = 0x80;
    g_site_notice_sprites[1].mx = 0;
    g_site_notice_sprites[1].my = 0;
    g_site_notice_sprites[1].scalex = 0x1000;
    g_site_notice_sprites[1].scaley = 0x1000;
    g_site_notice_sprites[1].rotate = 0;

    g_site_notice_sprites[2].x = 0x2A;
    g_site_notice_sprites[2].y = 0x75;
    g_site_notice_sprites[2].w = 0x58;
    g_site_notice_sprites[2].h = 0x18;
    g_site_notice_sprites[2].tpage = GetTPage(0, 0, 0x3C0, 0x1A0);
    g_site_notice_sprites[2].u = 0xA0;
    g_site_notice_sprites[2].v = 0xA0;
    g_site_notice_sprites[2].cx = 0;
    g_site_notice_sprites[2].cy = 0x1E4;
    g_site_notice_sprites[2].attribute = 0x40000000;
    g_site_notice_sprites[2].r = 0x80;
    g_site_notice_sprites[2].g = 0x80;
    g_site_notice_sprites[2].b = 0x80;
    g_site_notice_sprites[2].mx = 0;
    g_site_notice_sprites[2].my = 0;
    g_site_notice_sprites[2].scalex = 0x1000;
    g_site_notice_sprites[2].scaley = 0x1000;
    g_site_notice_sprites[2].rotate = 0;

    for (i = 2; i >= 0; i--) {
        ot = g_ot_2d;
        GsSortFastSprite(&g_site_notice_sprites[i], &ot[g_frame_buffer_index], 2);
    }
}

void text_window_show_cell(s16 arg0, s16 arg1, s32 arg2, s32 arg3) {
    g_text_window_x = arg0;
    g_text_window_y = arg1;
    g_text_window_frame_mode = 0;
    g_text_window_next_frame_mode = 0;
    g_text_window_state = 0;
    text_window_load_cell(arg2, arg3);
}

/* 19 diffs, all from one access: the original addresses g_text_window_saved_pos + 2 via $gp,
 * which maspsx only allows for a symbol defined in this file (or ASPSX >= 2.70).
 * With g_text_window_saved_pos defined here (e.g. `static Pos16 g_text_window_saved_pos;`) it matches. */
/* Show `text` at (x, y); mode 0 restores the previous message and position. */
void text_window_set_text(s32 x, s32 y, char *text, s32 mode) {
    if (mode == 0) {
        g_text_window_x = g_text_window_saved_pos.x;
        g_text_window_y = g_text_window_saved_pos.y;
        strcpy(g_text_window_message, g_text_window_saved_message);
    } else {
        g_text_window_saved_pos.x = g_text_window_x;
        g_text_window_saved_pos.y = g_text_window_y;
        strcpy(g_text_window_saved_message, g_text_window_message);
        g_text_window_x = x;
        g_text_window_y = y;
        strcpy(g_text_window_message, text);
    }
    g_text_window_frame_mode = mode;
    g_text_window_state = 0;
    text_window_build_message();
}

void text_window_run(s32 reset) {
    s32 i;

    if (reset) {
        g_text_window_state = 1;
        g_text_window_anim_frame = 0;
        g_text_window_busy = 1;
        for (i = 0; i < g_text_window_char_count; i++) {
            g_text_window_char_delay[i] = 1;
            g_text_window_char_step[i] = 0;
        }
        snd_play_anim_sfx(1);
    }
    switch (g_text_window_state) {
    case 0:
        text_window_draw_idle();
        break;
    case 1:
        text_window_draw_leave();
        break;
    case 2:
        text_window_draw_enter();
        break;
    }
}

void text_window_set_frame_mode(s32 arg0) {
    if (arg0 < 3) {
        if (arg0 > 0) {
            g_text_window_next_frame_mode = arg0;
        }
    }
}

/* Initialise the sound subsystem: reset all slots and load the two fixed sounds. */
void lain_anim_init(void) {
    SoundSlot *slot;
    s32 i;

    anim_decoder_init(0x140, 0);
    heap_init(g_heap_area, 0xD4000);
    loading_anim_load();
    g_lain_idle_loading = 0;
    g_lain_action_anim_active = 0;
    g_lain_anim_playing = 0;
    g_lain_idle_anim_cur = 0;
    g_lain_action_anim_index = 0;
    g_lain_idle_load_count = 0;
    for (i = 0; i < 1; i++) {
        slot = &g_lain_idle_anims[i];
        slot->unk6 = 0;
        slot->state = 0;
        slot->count = 0;
    }
    for (i = 0; i < 6; i++) {
        g_lain_action_anims[i].unk6 = 0;
        g_lain_action_anims[i].state = 0;
        g_lain_action_anims[i].handle = 0;
        D_800D6760[i] = -1;
    }
    g_lain_anim_default.handle = heap_alloc(g_lapks_file_table[9].size);
    g_lain_idle_anims[0].handle = heap_alloc(g_lapks_file_table[8].size);
    g_lain_anim_default.id = cd_load_archive_entry(4, 9, g_lapks_file_table, g_lain_anim_default.handle);
    while (cd_poll_load(g_lain_anim_default.id) == 0) {
        loading_anim_draw(0);
    }
    g_lain_anim_default.state = 2;
    g_lain_idle_anims[0].id = cd_load_archive_entry(4, 8, g_lapks_file_table, g_lain_idle_anims[0].handle);
    while (cd_poll_load(g_lain_idle_anims[0].id) == 0) {
        loading_anim_draw(0);
    }
    g_lain_idle_anims[0].state = 2;
    D_800A6920 = 0;
}

/* Poll loading sound slots; mark them ready or failed. */
void lain_anim_poll_loads(void) {
    SoundSlot *slot;
    s32 result;
    s32 i;

    if (g_lain_idle_loading != 0) {
        for (i = 0; i < 1; i++) {
            slot = &g_lain_idle_anims[i];
            if (slot->state == 1) {
                result = cd_poll_load(slot->id);
                if (result != 0) {
                    if (result > 0) {
                        slot->state = 2;
                        slot->count = 0;
                    } else {
                        heap_free(slot->handle);
                        slot->state = -1;
                    }
                    if (--g_lain_idle_load_count == 0) {
                        g_lain_idle_loading = 0;
                    }
                }
            }
        }
    }
}

/* Load a random sound (one of 38) into slot `index`. */
void lain_anim_load_random_idle(s32 index) {
    SoundSlot *slot;
    s16 *pick;
    FileEntry *table;
    s32 handle;
    s32 r;
    SoundSlot *slots;
    s16 *picks;

    if (g_lain_action_anim_active != 0 || g_lain_idle_anim_cur != index) {
        slots = g_lain_idle_anims;
        slot = &slots[index];
        if (slot->state == 2) {
            heap_free(slot->handle);
        }
        r = rand() % 38;
        table = g_lapks_file_table;
        picks = g_lain_idle_anim_files;
        pick = &picks[r];
        handle = heap_alloc(table[*pick].size);
        slot->handle = handle;
        if (handle != 0) {
            slot->id = cd_load_archive_entry(4, *pick, table, handle);
            slot->state = 1;
            switch (*pick) {
            case 36:
                slot->sfx = 20;
                break;
            case 42:
                slot->sfx = 21;
                break;
            default:
                g_lain_idle_anims[index].sfx = 0;
                break;
            }
            if (g_lain_idle_loading == 0) {
                g_lain_idle_loading = 1;
            }
            g_lain_idle_load_count++;
        } else {
            slot->state = 0;
        }
    }
}

void lain_anim_play_idle(void) {
    SoundSlot *slot;
    s32 minCount;
    s32 best;
    s32 i;
    SoundSlot *slots;

    if (g_lain_anim_playing == 0) {
        minCount = -1;
        best = -1;
        for (i = 0; i < 1; i++) {
            slot = &g_lain_idle_anims[i];
            if (slot->state == 2 && (minCount < 0 || slot->count < minCount)) {
                minCount = slot->count;
                best = i;
            }
        }
        if (best != -1) {
            slots = g_lain_idle_anims;
            slot = &slots[best];
            slot->count++;
            anim_open_archive(slot->handle);
            if (slot->sfx != 0) {
                g_lain_anim_sfx = slot->sfx;
            }
        } else {
            anim_open_archive(g_lain_anim_default.handle);
        }
        g_lain_idle_anim_cur = best;
        g_lain_anim_playing = 1;
    }
    anim_decode_frame(g_lain_anim_frame);
    if (g_lain_anim_frame[4] != 0) {
        snd_play_anim_sfx(g_lain_anim_sfx);
    }
    if (anim_next_frame() == 0) {
        if (g_lain_anim_sfx != 0) {
            snd_stop_anim_sfx();
            g_lain_anim_sfx = 0;
        }
        if (g_lain_idle_anim_cur != -1) {
            g_lain_idle_anims[g_lain_idle_anim_cur].state = 0;
            heap_free(g_lain_idle_anims[g_lain_idle_anim_cur].handle);
        }
        g_lain_anim_playing = 0;
    }
}

s32 lain_anim_play_sequence(s32 count) {
    SoundSlot *slot;

    if (g_lain_anim_playing == 0) {
        if (g_lain_action_anim_index < count) {
            anim_open_archive(g_lain_action_anims[g_lain_action_anim_index].handle);
            slot = &g_lain_action_anims[g_lain_action_anim_index];
            if (slot->sfx != 0) {
                g_lain_anim_sfx = slot->sfx;
            }
            g_lain_action_anim_index++;
            g_lain_action_anim_active = 1;
            g_lain_anim_playing = 1;
        } else {
            lain_anim_play_idle();
            return 0;
        }
    }
    anim_decode_frame(g_lain_anim_frame);
    if (anim_next_frame() == 0) {
        if (g_lain_anim_sfx != 0) {
            g_lain_anim_sfx = 0;
        }
        if (g_lain_action_anim_active == 1) {
            g_lain_action_anims[g_lain_action_anim_index - 1].state = 0;
            heap_free(g_lain_action_anims[g_lain_action_anim_index - 1].handle);
            g_lain_action_anim_active = 0;
            g_lain_action_anims[g_lain_action_anim_index - 1].handle = 0;
        }
        g_lain_anim_playing = 0;
        if (g_lain_action_anim_index == count) {
            return 0;
        }
    }
    if (g_lain_action_anim_active != 0) {
        return 1;
    }
    return -1;
}

typedef struct {
    s32 file[3];
} SoundSet;

extern SoundSet g_lain_anim_sets3[];
extern Pad16 g_lain_anim_sets3_sfx[3];

/* Allocate and start loading the three sounds of sound set `set`. */
s32 lain_anim_load_move(s32 set) {
    s32 i;
    s32 j;
    s32 *files;

    g_lain_action_anims[0].handle = heap_alloc(g_lapks_file_table[g_lain_anim_sets3[set].file[0]].size);
    if (g_lain_action_anims[0].handle == 0) {
        PURGE_IDLE_SOUNDS();
    }
    g_lain_action_anims[1].handle = heap_alloc(g_lapks_file_table[g_lain_anim_sets3[set].file[1]].size);
    if (g_lain_action_anims[1].handle == 0) {
        PURGE_IDLE_SOUNDS();
    }
    g_lain_action_anims[2].handle = heap_alloc(g_lapks_file_table[g_lain_anim_sets3[set].file[2]].size);
    if (g_lain_action_anims[2].handle == 0) {
        PURGE_IDLE_SOUNDS();
    }
    if (g_lain_action_anims[0].handle != 0 && g_lain_action_anims[1].handle != 0 && g_lain_action_anims[2].handle != 0) {
        files = g_lain_anim_sets3[set].file;
        for (j = 0; j < 3; j++) {
            g_lain_action_anims[j].id = cd_load_archive_entry(4, files[j], g_lapks_file_table, g_lain_action_anims[j].handle);
            g_lain_action_anims[j].state = 1;
            g_lain_action_anims[j].sfx = g_lain_anim_sets3_sfx[j].value;
        }
        g_lain_action_anim_index = 0;
        return 1;
    }
    heap_free(g_lain_action_anims[0].handle);
    heap_free(g_lain_action_anims[1].handle);
    heap_free(g_lain_action_anims[2].handle);
    return 0;
}

/* Allocate and start loading the one-sound bank `bank`. */
s32 lain_anim_load_level_move(s32 bank) {
    s32 i;
    s32 *files;
    Pad16 *sfx;

    g_lain_action_anims[0].handle = heap_alloc(g_lapks_file_table[g_lain_level_anim_files[bank]].size);
    if (g_lain_action_anims[0].handle == 0) {
        PURGE_IDLE_SOUNDS();
        g_lain_action_anims[0].handle = heap_alloc(g_lapks_file_table[g_lain_level_anim_files[bank]].size);
    }
    if (g_lain_action_anims[0].handle != 0) {
        files = &g_lain_level_anim_files[bank];
        sfx = g_lain_level_anim_sfx;
        for (i = 0; i < 1; i++) {
            g_lain_action_anims[i].id = cd_load_archive_entry(4, files[i], g_lapks_file_table, g_lain_action_anims[i].handle);
            g_lain_action_anims[i].state = 1;
            g_lain_action_anims[i].sfx = sfx[i].value;
        }
    } else {
        g_lain_action_anims[0].state = -1;
        return 0;
    }
    g_lain_action_anim_index = 0;
    return 1;
}

typedef struct {
    s32 file[4];
} SoundSet4;

extern SoundSet4 g_lain_anim_sets4[];
extern Pad16 g_lain_anim_sets4_sfx[4];
extern s16 g_node_select_anim_set;

/* Allocate and start loading the four sounds of sound set `set`. */
s32 lain_anim_load_select(s32 set) {
    s32 i;
    s32 j;
    s32 *files;

    g_lain_action_anims[0].handle = heap_alloc(g_lapks_file_table[g_lain_anim_sets4[set].file[0]].size);
    if (g_lain_action_anims[0].handle == 0) {
        PURGE_IDLE_SOUNDS();
    }
    g_lain_action_anims[1].handle = heap_alloc(g_lapks_file_table[g_lain_anim_sets4[set].file[1]].size);
    if (g_lain_action_anims[1].handle == 0) {
        PURGE_IDLE_SOUNDS();
    }
    g_lain_action_anims[2].handle = heap_alloc(g_lapks_file_table[g_lain_anim_sets4[set].file[2]].size);
    if (g_lain_action_anims[2].handle == 0) {
        PURGE_IDLE_SOUNDS();
    }
    g_lain_action_anims[3].handle = heap_alloc(g_lapks_file_table[g_lain_anim_sets4[set].file[3]].size);
    if (g_lain_action_anims[3].handle == 0) {
        PURGE_IDLE_SOUNDS();
    }
    if (g_lain_action_anims[0].handle != 0 && g_lain_action_anims[1].handle != 0 && g_lain_action_anims[2].handle != 0 &&
        g_lain_action_anims[3].handle != 0) {
        files = g_lain_anim_sets4[set].file;
        for (j = 0; j < 4; j++) {
            g_lain_action_anims[j].id = cd_load_archive_entry(4, files[j], g_lapks_file_table, g_lain_action_anims[j].handle);
            g_lain_action_anims[j].state = 1;
            g_lain_action_anims[j].sfx = g_lain_anim_sets4_sfx[j].value;
        }
        g_lain_action_anim_index = 0;
        g_node_select_anim_set = set;
        return 1;
    }
    for (i = 0; i < 4; i++) {
        if (g_lain_action_anims[i].handle != 0) {
            heap_free(g_lain_action_anims[i].handle);
        }
    }
    return 0;
}

/* Allocate and start loading the first sound bank (purging idle slots if memory is short). */
s32 lain_anim_load_save(void) {
    s32 i;
    s32 *files;
    Pad16 *sfx;

    g_lain_action_anims[0].handle = heap_alloc(g_lapks_file_table[g_lain_save_anim_file[0]].size);
    if (g_lain_action_anims[0].handle == 0) {
        PURGE_IDLE_SOUNDS();
        g_lain_action_anims[0].handle = heap_alloc(g_lapks_file_table[g_lain_save_anim_file[0]].size);
    }
    if (g_lain_action_anims[0].handle != 0) {
        files = g_lain_save_anim_file;
        sfx = g_lain_save_anim_sfx;
        for (i = 0; i < 1; i++) {
            g_lain_action_anims[i].id = cd_load_archive_entry(4, files[i], g_lapks_file_table, g_lain_action_anims[i].handle);
            g_lain_action_anims[i].state = 1;
            g_lain_action_anims[i].sfx = sfx[i].value;
        }
    } else {
        g_lain_action_anims[0].state = -1;
        return 0;
    }
    g_lain_action_anim_index = 0;
    return 1;
}

/* Same as lain_anim_load_save for the second sound bank. */
s32 lain_anim_load_site_enter(void) {
    s32 i;
    s32 *files;
    Pad16 *sfx;

    g_lain_action_anims[0].handle = heap_alloc(g_lapks_file_table[g_lain_site_enter_anim_file[0]].size);
    if (g_lain_action_anims[0].handle == 0) {
        PURGE_IDLE_SOUNDS();
        g_lain_action_anims[0].handle = heap_alloc(g_lapks_file_table[g_lain_site_enter_anim_file[0]].size);
    }
    if (g_lain_action_anims[0].handle != 0) {
        files = g_lain_site_enter_anim_file;
        sfx = g_lain_site_enter_anim_sfx;
        for (i = 0; i < 1; i++) {
            g_lain_action_anims[i].id = cd_load_archive_entry(4, files[i], g_lapks_file_table, g_lain_action_anims[i].handle);
            g_lain_action_anims[i].state = 1;
            g_lain_action_anims[i].sfx = sfx[i].value;
        }
    } else {
        g_lain_action_anims[0].state = -1;
        return 0;
    }
    g_lain_action_anim_index = 0;
    return 1;
}

extern s32 g_lain_anim_set6[6];
extern Pad16 g_lain_anim_set6_sfx[6];

/* Allocate and start loading the six sounds of the fixed sound set. */
s32 lain_anim_load_set6(void) {
    s32 i;
    s32 j;
    s32 *files;

    g_lain_action_anims[0].handle = heap_alloc(g_lapks_file_table[g_lain_anim_set6[0]].size);
    if (g_lain_action_anims[0].handle == 0) {
        PURGE_IDLE_SOUNDS();
    }
    g_lain_action_anims[1].handle = heap_alloc(g_lapks_file_table[g_lain_anim_set6[1]].size);
    if (g_lain_action_anims[1].handle == 0) {
        PURGE_IDLE_SOUNDS();
    }
    g_lain_action_anims[2].handle = heap_alloc(g_lapks_file_table[g_lain_anim_set6[2]].size);
    if (g_lain_action_anims[2].handle == 0) {
        PURGE_IDLE_SOUNDS();
    }
    g_lain_action_anims[3].handle = heap_alloc(g_lapks_file_table[g_lain_anim_set6[3]].size);
    if (g_lain_action_anims[3].handle == 0) {
        PURGE_IDLE_SOUNDS();
    }
    g_lain_action_anims[4].handle = heap_alloc(g_lapks_file_table[g_lain_anim_set6[4]].size);
    if (g_lain_action_anims[4].handle == 0) {
        PURGE_IDLE_SOUNDS();
    }
    g_lain_action_anims[5].handle = heap_alloc(g_lapks_file_table[g_lain_anim_set6[5]].size);
    if (g_lain_action_anims[5].handle == 0) {
        PURGE_IDLE_SOUNDS();
    }
    if (g_lain_action_anims[0].handle != 0 && g_lain_action_anims[1].handle != 0 && g_lain_action_anims[2].handle != 0 &&
        g_lain_action_anims[3].handle != 0 && g_lain_action_anims[4].handle != 0 && g_lain_action_anims[5].handle != 0) {
        files = g_lain_anim_set6;
        for (j = 0; j < 6; j++) {
            g_lain_action_anims[j].id = cd_load_archive_entry(4, files[j], g_lapks_file_table, g_lain_action_anims[j].handle);
            g_lain_action_anims[j].state = 1;
            g_lain_action_anims[j].sfx = g_lain_anim_set6_sfx[j].value;
        }
        g_lain_action_anim_index = 0;
        return 1;
    }
    for (i = 0; i < 6; i++) {
        if (g_lain_action_anims[i].handle != 0) {
            heap_free(g_lain_action_anims[i].handle);
        }
    }
    return 0;
}

s32 lain_anim_play_set6_last(void) {
    SoundSlot *slot;
    s32 i;

    if (g_lain_anim_playing == 0 && g_lain_action_anim_index == 5) {
        anim_open_archive(g_lain_action_anims[5].handle);
        g_lain_action_anim_active = 1;
        g_lain_anim_playing = 1;
        g_lain_action_anim_index++;
        for (i = 0, slot = g_lain_action_anims; i < 5; i++, slot++) {
            slot->state = 0;
            heap_free(slot->handle);
            slot->handle = 0;
        }
    }
    anim_decode_frame(g_lain_anim_frame);
    if (anim_next_frame() == 0) {
        if (g_lain_anim_sfx != 0) {
            g_lain_anim_sfx = 0;
        }
        if (g_lain_action_anim_active == 1) {
            g_lain_action_anims[g_lain_action_anim_index - 1].state = 0;
            heap_free(g_lain_action_anims[g_lain_action_anim_index - 1].handle);
            g_lain_action_anim_active = 0;
            g_lain_action_anims[g_lain_action_anim_index - 1].handle = 0;
        }
        g_lain_anim_playing = 0;
        if (g_lain_action_anim_index == 6) {
            return 0;
        }
    }
    if (g_lain_action_anim_active != 0) {
        return 1;
    }
    return -1;
}

s32 func_800220C4(void) {
    return 1;
}


