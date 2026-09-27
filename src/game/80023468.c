#include "common.h"

typedef struct {
    s16 x, y, w, h;
} RECT;

typedef struct {
    u32 tag;
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

/* A 2D sprite drawn as a textured quad. */
typedef struct {
    POLY_FT4 poly; /* 0x00 */
    s32 attr;      /* 0x28: 0x40000000 semi-trans, 0x800000 flip U, 0x400000 flip V */
    s16 tpage;     /* 0x2C */
    s16 clut;      /* 0x2E */
    u16 scaleX;    /* 0x30: 0x1000 = 1.0 */
    u16 scaleY;    /* 0x32 */
    s16 x;         /* 0x34 */
    s16 y;         /* 0x36 */
    s16 w;         /* 0x38 */
    s16 h;         /* 0x3A */
    s16 rotX;      /* 0x3C: rotation (4096 = 360 deg), used by sprite_draw_rotated */
    s16 rotY;      /* 0x3E */
    s16 rotZ;      /* 0x40 */
    s16 unk42;     /* 0x42 */
    u8 u;          /* 0x44 */
    u8 v;          /* 0x45 */
    u8 r;          /* 0x46 */
    u8 g;          /* 0x47 */
    u8 b;          /* 0x48 */
    u8 pad49[3];
} SpriteEntry; /* size 0x4C */

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
} Sprite; /* GsSPRITE */

typedef struct {
    u32 length;
    void *org;
    u32 offset;
    u32 point;
    void *tag;
} GsOT;

/* Two frame counters for the row open/close animations. */
typedef struct {
    s16 label_frame;
    s16 counter;
} AnimState;

extern AnimState g_menu_row_flip_frames;

extern s16 g_bgm_slot;
extern s16 g_bgm_seq_main;
extern s16 g_bgm_seq_credits;
extern s16 g_menu_anim_frame;
extern u16 g_spinner_ot_pri;
extern s16 g_menu_row; /* currently open row, -1 = none */
extern s16 g_menu_wrap_bank;
/* widths of the dialog text labels */
extern s16 g_label_w_sure;
extern s16 g_label_w_yes;
extern s16 g_label_w_no;
extern s16 g_label_w_version;
extern s16 g_label_w_permission;
extern s16 g_label_w_denied;
extern u8 g_menu_cell_flips[][2]; /* per-cell flip flags */
extern SpriteEntry *g_menu_sprites;
/* Addressed absolutely (lui/%lo) although small: keep it out of $gp. */
#define NO_GP __attribute__((section(".data")))
extern s32 g_frame_buffer_index NO_GP; /* current double-buffer index */
extern GsOT g_ot_2d[2];
extern GsOT g_ot[2];
extern u8 g_menu_rows[][4]; /* per-row {first cell, cell count, ...} */
void sprite_draw(SpriteEntry *entry, GsOT *ot, u16 pri);

void SsSeqSetVol(s16 access_num, s16 voll, s16 volr);
void SsSeqPlay(s16 access_num, char play_mode, s16 l_count);
void SsSeqStop(s16 access_num);
void SsSeqSetDecrescendo(s16 access_num, s16 vol, s32 v_time);
void SsSeqSetCrescendo(s16 access_num, s16 vol, s32 v_time);
void heap_free(void *);
s32 DrawSync(s32);
void *memset(void *s, int c, unsigned int n);
s32 heap_alloc(s32);
void sprite_draw_rotated(SpriteEntry *entry, GsOT *ot, u16 pri);
u16 GetTPage(s32 tp, s32 abr, s32 x, s32 y);
void menu_draw_grid_masked(s32 a, s32 b);
s32 menu_row_flip_to_icons(s32 row);
extern s16 g_menu_state;

/* Start the current BGM sequence at full volume, playing once. */
void bgm_play(void) {
    if (g_bgm_slot == 0) {
        SsSeqSetVol(g_bgm_seq_main, 0x7F, 0x7F);
        SsSeqPlay(g_bgm_seq_main, 1, 0);
    } else {
        SsSeqSetVol(g_bgm_seq_credits, 0x7F, 0x7F);
        SsSeqPlay(g_bgm_seq_credits, 1, 0);
    }
}

/* Stop the currently selected sequence. */
void bgm_stop(void) {
    if (g_bgm_slot == 0) {
        SsSeqStop(g_bgm_seq_main);
    } else {
        SsSeqStop(g_bgm_seq_credits);
    }
}

/* Fade out the currently selected sequence. */
void bgm_fade_out(void) {
    if (g_bgm_slot == 0) {
        SsSeqSetDecrescendo(g_bgm_seq_main, 0x7F, 0xF0);
    } else {
        SsSeqSetDecrescendo(g_bgm_seq_credits, 0x7F, 0xF0);
    }
}

/* Fade in the currently selected sequence. */
void bgm_fade_in(void) {
    if (g_bgm_slot == 0) {
        SsSeqSetCrescendo(g_bgm_seq_main, 0x7F, 0xF0);
    } else {
        SsSeqSetCrescendo(g_bgm_seq_credits, 0x7F, 0xF0);
    }
}

/* Select which sequence slot the other sequence helpers act on. */
void bgm_select_slot(s16 slot) {
    g_bgm_slot = slot;
}

void bgm_stop_main(void) {
    SsSeqStop(g_bgm_seq_main);
}

extern SpriteEntry g_msgbox_bars[];
extern Sprite g_msgbox_sprite;

/* Initialise four pairs of menu sprite entries and one extra sprite. */
void msgbox_init(void) {
    s32 i;

    for (i = 0; i < 4; i++) {
        g_msgbox_bars[i * 2].clut = 0x7941;
        g_msgbox_bars[i * 2].x = 0x10;
        g_msgbox_bars[i * 2].y = i * 0x10 + 0x99;
        g_msgbox_bars[i * 2].tpage = 0x1E;
        g_msgbox_bars[i * 2].u = 0;
        g_msgbox_bars[i * 2].v = 0xE0;
        g_msgbox_bars[i * 2].w = 0x90;
        g_msgbox_bars[i * 2].h = 0x10;
        g_msgbox_bars[i * 2].r = 0x80;
        g_msgbox_bars[i * 2].g = 0x80;
        g_msgbox_bars[i * 2].b = 0x80;
        g_msgbox_bars[i * 2].attr = 0;
        g_msgbox_bars[i * 2 + 1].y = i * 0x10 + 0x99;
        g_msgbox_bars[i * 2 + 1].x = 0xA0;
        g_msgbox_bars[i * 2 + 1].attr = 0x800000;
        g_msgbox_bars[i * 2 + 1].clut = 0x7941;
        g_msgbox_bars[i * 2 + 1].tpage = 0x1E;
        g_msgbox_bars[i * 2 + 1].u = 0;
        g_msgbox_bars[i * 2 + 1].v = 0xE0;
        g_msgbox_bars[i * 2 + 1].w = 0x90;
        g_msgbox_bars[i * 2 + 1].h = 0x10;
        g_msgbox_bars[i * 2 + 1].r = 0x80;
        g_msgbox_bars[i * 2 + 1].g = 0x80;
        g_msgbox_bars[i * 2 + 1].b = 0x80;
    }
    g_msgbox_sprite.attribute = 0;
    g_msgbox_sprite.x = 0x34;
    g_msgbox_sprite.y = 0x9C;
    g_msgbox_sprite.tpage = 0x1E;
    g_msgbox_sprite.cy = 0x1E5;
    g_msgbox_sprite.cx = 0;
    g_msgbox_sprite.r = 0x80;
    g_msgbox_sprite.g = 0x80;
    g_msgbox_sprite.b = 0x80;
}

extern s32 g_msgbox_inited;
extern u8 g_msgbox_table[][5];
void GsSortFastSprite(Sprite *sp, GsOT *ot, u16 pri);

/* Draw the menu frame sprite for `menu` (1-based) and its rows of entries. */
void msgbox_draw(s32 menu) {
    Sprite *frame;
    s32 i;
    s32 rows;

    if (g_msgbox_inited == 0) {
        msgbox_init();
        g_msgbox_inited = 1;
    }
    frame = &g_msgbox_sprite;
    frame->u = g_msgbox_table[menu - 1][0];
    frame->v = g_msgbox_table[menu - 1][1];
    frame->w = g_msgbox_table[menu - 1][2];
    frame->h = g_msgbox_table[menu - 1][3];
    frame->x = 0xA0 - frame->w / 2;
    GsSortFastSprite(frame, &g_ot_2d[g_frame_buffer_index], 0);
    rows = g_msgbox_table[menu - 1][4];
    for (i = 0; i < rows; i++) {
        sprite_draw(&g_msgbox_bars[i * 2], &g_ot_2d[g_frame_buffer_index], 0);
        sprite_draw(&g_msgbox_bars[i * 2 + 1], &g_ot_2d[g_frame_buffer_index], 0);
    }
}

extern u8 g_menu_cell_icons[];

/* Set up the sprite for item slot `idx` (entry idx + 0x17) from its icon/flip tables. */
void menu_cell_set_icon(s32 idx) {
    s32 slot = idx + 0x17;

    g_menu_sprites[slot].clut = 0x78C0;
    g_menu_sprites[slot].w = 0x10;
    g_menu_sprites[slot].h = 0x10;
    g_menu_sprites[slot].tpage = 0x1A;
    if (g_menu_cell_icons[idx] < 13) {
        g_menu_sprites[slot].u = (g_menu_cell_icons[idx] - 1) * 16;
        g_menu_sprites[slot].v = 0xC8;
    } else {
        g_menu_sprites[slot].u = (g_menu_cell_icons[idx] - 13) * 16;
        g_menu_sprites[slot].v = 0xD8;
    }
    if (g_menu_cell_flips[idx][0]) {
        g_menu_sprites[slot].attr |= 0x400000;
    }
    if (g_menu_cell_flips[idx][1]) {
        g_menu_sprites[slot].attr |= 0x800000;
    }
}

/* Same as menu_cell_set_icon but for sprite entry `idx` directly. */
void spinner_cell_set_icon(s32 idx) {
    s32 slot = idx;

    g_menu_sprites[slot].clut = 0x78C0;
    g_menu_sprites[slot].w = 0x10;
    g_menu_sprites[slot].h = 0x10;
    g_menu_sprites[slot].tpage = 0x1A;
    if (g_menu_cell_icons[idx] < 13) {
        g_menu_sprites[slot].u = (g_menu_cell_icons[idx] - 1) * 16;
        g_menu_sprites[slot].v = 0xC8;
    } else {
        g_menu_sprites[slot].u = (g_menu_cell_icons[idx] - 13) * 16;
        g_menu_sprites[slot].v = 0xD8;
    }
    if (g_menu_cell_flips[idx][0]) {
        g_menu_sprites[slot].attr |= 0x400000;
    }
    if (g_menu_cell_flips[idx][1]) {
        g_menu_sprites[slot].attr |= 0x800000;
    }
}

/* Allocate-time setup of the dialog / grid sprite table (128 entries). */
void menu_sprites_init(void) {
    volatile s32 i;
    s32 n;

    memset(g_menu_sprites, 0, 0x2600);
    for (i = 0; i < 128; i++) {
        g_menu_sprites[i].r = 0x80;
        g_menu_sprites[i].g = 0x80;
        g_menu_sprites[i].b = 0x80;
        g_menu_sprites[i].scaleX = 0x1000;
        g_menu_sprites[i].scaleY = 0x1000;
    }
    for (i = 0; i < 49; i++) {
        n = i + 72;
        g_menu_sprites[n].x = 0x99;
        g_menu_sprites[n].y = 0x61;
        g_menu_sprites[n].clut = 0x78C1;
        g_menu_sprites[n].tpage = 0x1A;
        g_menu_sprites[n].w = 0xE;
        g_menu_sprites[n].h = 0xE;
        g_menu_sprites[n].u = 0;
        g_menu_sprites[n].v = 0xE8;
        if (g_menu_cell_flips[i][0]) {
            g_menu_sprites[n].attr |= 0x800000;
        }
        if (g_menu_cell_flips[i][1]) {
            g_menu_sprites[n].attr |= 0x400000;
        }
        g_menu_sprites[n].rotX = 0;
        g_menu_sprites[n].rotY = 0x400;
        g_menu_sprites[n].rotZ = 0x400;
    }
    for (i = 0; i < 49; i++) {
        n = i + 23;
        g_menu_sprites[n].x = 0x98;
        g_menu_sprites[n].y = 0x60;
        menu_cell_set_icon(i);
        g_menu_sprites[n].rotX = 0;
        g_menu_sprites[n].rotY = 0x400;
        g_menu_sprites[n].rotZ = 0x400;
    }

    g_menu_sprites[0].clut = 0x7841;
    g_menu_sprites[0].x = 0x53;
    g_menu_sprites[0].y = 0x6C;
    g_menu_sprites[0].tpage = 0x1B;
    g_menu_sprites[0].u = 0;
    g_menu_sprites[0].v = 0x70;
    g_menu_sprites[0].w = g_label_w_sure;
    g_menu_sprites[0].h = 0x18;

    g_menu_sprites[1].clut = 0x78C1;
    g_menu_sprites[1].x = 0;
    g_menu_sprites[1].y = 0x69;
    g_menu_sprites[1].tpage = 0x1A;
    g_menu_sprites[1].u = 0x40;
    g_menu_sprites[1].v = 0xE0;
    g_menu_sprites[1].w = 0xA0;
    g_menu_sprites[1].h = 0x19;
    g_menu_sprites[1].attr = 0x40000000;

    g_menu_sprites[2].clut = 0x78C1;
    g_menu_sprites[2].x = 0xA0;
    g_menu_sprites[2].y = 0x69;
    g_menu_sprites[2].tpage = 0x1A;
    g_menu_sprites[2].u = 0x40;
    g_menu_sprites[2].v = 0xE0;
    g_menu_sprites[2].w = 0xA0;
    g_menu_sprites[2].h = 0x19;
    g_menu_sprites[2].attr = 0x40800000;

    g_menu_sprites[3].clut = 0x7841;
    g_menu_sprites[3].x = 0x28;
    g_menu_sprites[3].y = 0x8F;
    g_menu_sprites[3].tpage = 0x1B;
    g_menu_sprites[3].u = 0;
    g_menu_sprites[3].v = 0x88;
    g_menu_sprites[3].w = g_label_w_yes;
    g_menu_sprites[3].h = 0x18;

    g_menu_sprites[4].clut = 0x7841;
    g_menu_sprites[4].x = 0xF3;
    g_menu_sprites[4].y = 0x8F;
    g_menu_sprites[4].tpage = 0x1B;
    g_menu_sprites[4].u = 0;
    g_menu_sprites[4].v = 0xA0;
    g_menu_sprites[4].w = g_label_w_no;
    g_menu_sprites[4].h = 0x18;

    g_menu_sprites[5].x = 0xE6;
    g_menu_sprites[5].clut = 0x78C1;
    g_menu_sprites[5].y = 0x8C;
    g_menu_sprites[5].tpage = 0x1A;
    g_menu_sprites[5].u = 0xC0;
    g_menu_sprites[5].v = 0xC8;
    g_menu_sprites[5].w = 0x33;
    g_menu_sprites[5].h = 0x19;
    g_menu_sprites[5].attr = 0x40000000;

    g_menu_sprites[6].clut = 0x78C1;
    g_menu_sprites[6].y = 0xD1;
    g_menu_sprites[6].x = 0x8C;
    g_menu_sprites[6].tpage = 0x1A;
    g_menu_sprites[6].u = 0xC2;
    g_menu_sprites[6].v = 0xCA;
    g_menu_sprites[6].h = 0xA;
    g_menu_sprites[6].scaleX = 0x7000;
    g_menu_sprites[6].scaleY = 0x5000;
    g_menu_sprites[6].w = 0x28;
    g_menu_sprites[6].attr = 0x40000000;

    g_menu_sprites[7].clut = 0x7840;
    g_menu_sprites[7].x = 0xA0 - g_label_w_version / 2;
    g_menu_sprites[7].y = 0xC8;
    g_menu_sprites[7].tpage = 0x1B;
    g_menu_sprites[7].u = 0;
    g_menu_sprites[7].v = 0x58;
    g_menu_sprites[7].w = 0x100;
    g_menu_sprites[7].h = 0x10;

    g_menu_sprites[8].clut = 0x7841;
    g_menu_sprites[8].x = 0xA0 - (g_label_w_permission + g_label_w_denied) / 2;
    g_menu_sprites[8].y = 0x6C;
    g_menu_sprites[8].tpage = 0x1B;
    g_menu_sprites[8].u = 0;
    g_menu_sprites[8].v = 0x40;
    g_menu_sprites[8].w = g_label_w_permission;
    g_menu_sprites[8].h = 0x18;

    g_menu_sprites[9].clut = 0x7841;
    g_menu_sprites[9].y = 0x6C;
    g_menu_sprites[9].tpage = 0x1B;
    g_menu_sprites[9].x = 0xA0 - (g_label_w_permission + g_label_w_denied) / 2 + g_label_w_permission;
    g_menu_sprites[9].u = 0;
    g_menu_sprites[9].v = 0x28;
    g_menu_sprites[9].w = g_label_w_denied;
    g_menu_sprites[9].h = 0x18;
}

extern char g_menu_str_site[]; /* "Site" */
extern char g_menu_str_save[];
extern char g_menu_str_exit[];
extern char g_menu_str_load[];
extern char g_menu_str_about[];
extern char g_menu_str_change[];
extern char g_menu_str_yes[]; /* "Yes" */
extern char g_menu_str_no[];
extern char g_menu_str_denied[]; /* "denied" */
extern s16 g_sskn_level NO_GP;
s16 font_render_string(char *text, s32 x, s32 y, s32 arg3, s32 arg4);
s32 sprintf(char *buf, const char *fmt, ...);

/* Create the text labels for the site/version/permission dialog. */
void menu_render_labels(void) {
    char buf[80];

    font_render_string(g_menu_str_site, 0x2C0, 0x1B8, 0, 1);
    font_render_string(g_menu_str_save, 0x2C0, 0x1D0, 0, 1);
    font_render_string(g_menu_str_exit, 0x2C0, 0x1E8, 0, 1);
    font_render_string(g_menu_str_load, 0x2D8, 0x1B8, 0, 1);
    font_render_string(g_menu_str_about, 0x2D8, 0x1D0, 0, 1);
    font_render_string(g_menu_str_change, 0x2D8, 0x1E8, 0, 1);
    g_label_w_sure = font_render_string("Are you sure?", 0x2C0, 0x170, 0, 0);
    g_label_w_yes = font_render_string(g_menu_str_yes, 0x2C0, 0x188, 0, 0);
    g_label_w_no = font_render_string(g_menu_str_no, 0x2C0, 0x1A0, 0, 0);
    sprintf(buf, "Application Version %d.0", g_sskn_level + 1);
    g_label_w_version = font_render_string(buf, 0x2C0, 0x158, 1, 1);
    g_label_w_permission = font_render_string("Permission ", 0x2C0, 0x140, 0, 0);
    g_label_w_denied = font_render_string(g_menu_str_denied, 0x2C0, 0x128, 0, 0);
}

/* Draw the item grid, hiding the cells covered by rows `a` and `b` (-1 = none). */
void menu_draw_grid_masked(s32 a, s32 b) {
    u8 visible[49];
    s32 col;
    s32 count;
    s32 i;

    memset(visible, 1, 49);
    if (a != -1) {
        col = (u8)(g_menu_rows[a][0] % 7);
        count = g_menu_rows[a][1];
        if (col + count >= 8) {
            count = 7 - col;
        }
        for (i = 0; i < count; i++) {
            visible[g_menu_rows[a][0] + i] = 0;
        }
    }
    if (b != -1) {
        col = (u8)(g_menu_rows[b][0] % 7);
        count = g_menu_rows[b][1];
        if (col + count >= 8) {
            count = 7 - col;
        }
        for (i = 0; i < count; i++) {
            visible[g_menu_rows[b][0] + i] = 0;
        }
    }
    for (i = 0; i < 49; i++) {
        if (visible[i]) {
            sprite_draw(&g_menu_sprites[i + 23], &g_ot_2d[g_frame_buffer_index], i + 23);
        }
        sprite_draw(&g_menu_sprites[i + 72], &g_ot_2d[g_frame_buffer_index], i + 72);
    }
    sprite_draw_rotated(&g_menu_sprites[7], &g_ot_2d[g_frame_buffer_index], 4);
    sprite_draw_rotated(&g_menu_sprites[6], &g_ot_2d[g_frame_buffer_index], 4);
}


/* Draw the whole item grid plus the cells of the open row that wrap onto the next line. */
void menu_draw(void) {
    s32 i;
    s32 excess;
    s32 shift;

    for (i = 0; i < 49; i++) {
        sprite_draw(&g_menu_sprites[i + 23], &g_ot_2d[g_frame_buffer_index], i + 23);
        sprite_draw(&g_menu_sprites[i + 72], &g_ot_2d[g_frame_buffer_index], i + 72);
    }
    if (g_menu_row != -1) {
        excess = (u8)(g_menu_rows[g_menu_row][0] % 7) + g_menu_rows[g_menu_row][1] - 7;
        shift = (g_menu_wrap_bank == 0) * 4;
        for (i = 0; i < excess; i++) {
            sprite_draw(&g_menu_sprites[i + 15 + shift], &g_ot_2d[g_frame_buffer_index], i + 15 + shift);
        }
    }
    if (g_menu_state == 1) {
        sprite_draw(&g_menu_sprites[0], &g_ot_2d[g_frame_buffer_index], 1);
        sprite_draw(&g_menu_sprites[1], &g_ot_2d[g_frame_buffer_index], 2);
        sprite_draw(&g_menu_sprites[2], &g_ot_2d[g_frame_buffer_index], 2);
        sprite_draw(&g_menu_sprites[3], &g_ot_2d[g_frame_buffer_index], 3);
        sprite_draw(&g_menu_sprites[4], &g_ot_2d[g_frame_buffer_index], 3);
        sprite_draw(&g_menu_sprites[5], &g_ot_2d[g_frame_buffer_index], 4);
    }
    if (g_menu_state == 8) {
        sprite_draw(&g_menu_sprites[8], &g_ot_2d[g_frame_buffer_index], 1);
        sprite_draw(&g_menu_sprites[9], &g_ot_2d[g_frame_buffer_index], 1);
        sprite_draw(&g_menu_sprites[1], &g_ot_2d[g_frame_buffer_index], 2);
        sprite_draw(&g_menu_sprites[2], &g_ot_2d[g_frame_buffer_index], 2);
    }
    sprite_draw_rotated(&g_menu_sprites[7], &g_ot_2d[g_frame_buffer_index], 5);
    sprite_draw_rotated(&g_menu_sprites[6], &g_ot_2d[g_frame_buffer_index], 5);
}


#define getTPage(tp, abr, x, y) \
    ((((tp) & 0x3) << 7) | (((abr) & 0x3) << 5) | (((y) & 0x100) >> 4) | (((x) & 0x3FF) >> 6) | \
     (((y) & 0x200) << 2))

/* Row open animation (17 frames) for the text/label version of a row; returns 1 when done. */
s32 menu_row_flip_to_label(s32 row) {
    volatile s32 i;
    u8 start;
    s32 excess;
    s32 col;
    s32 idx;
    s32 x;
    s32 y;
    s32 a;
    s32 b;
    s32 c;
    s32 d;
    s32 tpX;

    if (g_menu_row_flip_frames.label_frame == 9) {
        start = g_menu_rows[row][0];
        excess = (u8)(start % 7) + g_menu_rows[row][1] - 7;
        if (excess < 0) {
            excess = 0;
        }
        x = 0xF8;
        y = (u8)(start / 7) * 24 + 24;
        for (i = 0; i < excess; i++, x += 24) {
            d = g_menu_wrap_bank * 4 + 15;
            idx = i + d;
            g_menu_sprites[idx].w = 0x10;
            g_menu_sprites[idx].h = 0x18;
            g_menu_sprites[idx].x = x;
            g_menu_sprites[idx].y = y;
            g_menu_sprites[idx].clut = 0x7842;
            tpX = 0x2C0; /* a variable, so the 0xB term isn't folded to the end of the OR chain */
            g_menu_sprites[idx].tpage = getTPage(0, 0, tpX, 0x200 - g_menu_rows[row][3] * 24);
            g_menu_sprites[idx].u = g_menu_rows[row][2] * 96 + (g_menu_rows[row][1] - excess + i) * 16;
            g_menu_sprites[idx].v = -(g_menu_rows[row][3] * 24);
        }
        for (i = 0; i < g_menu_rows[row][1] - excess; i++) {
            if (i == 0) {
                g_menu_sprites[i + g_menu_rows[row][0] + 23].clut = 0x7841;
            } else {
                g_menu_sprites[i + g_menu_rows[row][0] + 23].clut = 0x7842;
            }
            g_menu_sprites[i + g_menu_rows[row][0] + 23].tpage = GetTPage(0, 0, 0x2C0, 0x200 - g_menu_rows[row][3] * 24);
            g_menu_sprites[i + g_menu_rows[row][0] + 23].u = g_menu_rows[row][2] * 96 + i * 16;
            g_menu_sprites[i + g_menu_rows[row][0] + 23].v = -(g_menu_rows[row][3] * 24);
            g_menu_sprites[i + g_menu_rows[row][0] + 23].attr = 0;
        }
    }
    for (i = 0; i < g_menu_rows[row][1]; i++) {
        start = g_menu_rows[row][0];
        col = (u8)(start % 7);
        if (col + i >= 7) {
            a = i;
            a = a + 15;
            d = col + a;
            a = d;
            b = g_menu_wrap_bank * 4;
            b = b - 7;
            idx = a + b;
        } else {
            a = i;
            b = start + 23;
            idx = a + b;
        }
        c = g_menu_row_flip_frames.label_frame;
        if (c < 9) {
            g_menu_sprites[idx].rotY = c << 8;
        } else {
            g_menu_sprites[idx].rotY = 0x800 - ((c - 8) << 8);
        }
        if (g_menu_row_flip_frames.label_frame != 8) {
            g_menu_sprites[idx].rotX += 0x100;
            g_menu_sprites[idx].rotX %= 0x1000;
        }
        if (g_menu_row_flip_frames.label_frame >= 9 || idx >= 23) {
            sprite_draw_rotated(&g_menu_sprites[idx], &g_ot_2d[g_frame_buffer_index], idx);
        }
    }
    if (g_menu_row_flip_frames.label_frame == 16) {
        g_menu_row_flip_frames.label_frame = 0;
        return 1;
    }
    g_menu_row_flip_frames.label_frame++;
    return 0;
}

/* Row open animation (17 frames): unfold the cells of row `row`; returns 1 when done. */
s32 menu_row_flip_to_icons(s32 row) {
    volatile s32 i;
    u8 start;
    s32 excess;
    s32 col;
    s32 idx;
    s32 c;
    s32 z;
    s32 a;
    s32 b;
    s32 d;

    if (g_menu_row_flip_frames.counter == 9) {
        start = g_menu_rows[row][0];
        excess = (u8)(start % 7) + g_menu_rows[row][1] - 7;
        if (excess < 0) {
            excess = 0;
        }
        i = 1;
        g_menu_sprites[start + 23].clut = 0x7840;
        for (; i < g_menu_rows[row][1] - excess; i++) {
            menu_cell_set_icon(g_menu_rows[row][0] + i);
        }
    }
    for (i = 0; i < g_menu_rows[row][1]; i++) {
        col = (u8)(g_menu_rows[row][0] % 7);
        if (col + i >= 7) {
            z = g_menu_wrap_bank == 0;
            b = z * 4;
            d = i;
            d = d + 15;
            a = col + d;
            b = b - 7;
            idx = a + b;
        } else {
            a = i;
            b = g_menu_rows[row][0] + 23;
            idx = a + b;
        }
        c = g_menu_row_flip_frames.counter;
        if (c < 9) {
            g_menu_sprites[idx].rotY = c << 8;
        } else {
            g_menu_sprites[idx].rotY = 0x800 - ((c - 8) << 8);
        }
        if (g_menu_row_flip_frames.counter != 8) {
            g_menu_sprites[idx].rotX += 0x100;
            g_menu_sprites[idx].rotX %= 0x1000;
        }
        if (g_menu_row_flip_frames.counter < 9 || idx >= 23) {
            sprite_draw_rotated(&g_menu_sprites[idx], &g_ot_2d[g_frame_buffer_index], idx);
        }
    }
    if (g_menu_row_flip_frames.counter == 16) {
        g_menu_row_flip_frames.counter = 0;
        return 1;
    }
    g_menu_row_flip_frames.counter++;
    return 0;
}

extern u8 g_menu_cell_unfold_order[]; /* per-cell row number */
extern s16 g_menu_busy;
void menu_render_labels(void);
void menu_sprites_init(void);
s32 menu_row_flip_to_label(s32 row);

/* Dialog open sequence: zoom the grid in, then unfold each cell row by row; returns 1 when done. */
s32 menu_open(void) {
    volatile s32 i;
    volatile s32 t;
    s16 step;
    s32 n;
    s32 begin;
    s32 u;
    s32 rowIdx;

    if (g_menu_anim_frame == 0) {
        g_menu_sprites = (SpriteEntry *)heap_alloc(0x2600);
        if (g_menu_sprites == NULL) {
            return 0;
        }
        menu_render_labels();
        menu_sprites_init();
    }
    if (g_menu_sprites == NULL) {
        return 0;
    }
    if (g_menu_anim_frame < 17) {
        for (i = 0; i < 49; i++) {
            g_menu_sprites[i + 23].x = ((i % 7) * 24 - 72) * g_menu_anim_frame / 16 + 0x98;
            g_menu_sprites[i + 23].y = ((i / 7) * 24 - 72) * g_menu_anim_frame / 16 + 0x60;
            g_menu_sprites[i + 72].x = ((i % 7) * 21 - 63) * g_menu_anim_frame / 16 + 0x99;
            g_menu_sprites[i + 72].y = ((i / 7) * 21 - 63) * g_menu_anim_frame / 16 + 0x61;
            if (g_menu_anim_frame != 0) {
                g_menu_sprites[i + 23].rotY -= 0x40;
                g_menu_sprites[i + 23].rotZ -= 0x40;
                g_menu_sprites[i + 72].rotY -= 0x40;
                g_menu_sprites[i + 72].rotZ -= 0x40;
            }
            sprite_draw_rotated(&g_menu_sprites[i + 23], &g_ot_2d[g_frame_buffer_index], i + 23);
            sprite_draw_rotated(&g_menu_sprites[i + 72], &g_ot_2d[g_frame_buffer_index], i + 72);
        }
        g_menu_anim_frame++;
        goto end;
    }
    if (g_menu_anim_frame < 82) {
        step = (g_menu_anim_frame - 17) / 4;
        for (i = 0; i < 49; i++) {
            begin = g_menu_cell_unfold_order[i] * 4 + 17;
            t = g_menu_anim_frame - begin;
            if (step >= (s16)g_menu_cell_unfold_order[i] && t >= 0 && t < 17) {
                if (t < 9) {
                    g_menu_sprites[i + 23].rotY = t << 8;
                } else {
                    g_menu_sprites[i + 23].rotY = 0x800 - ((t - 8) << 8);
                    if (t == 9) {
                        switch (i) {
                        case 8:
                        case 19:
                        case 24:
                        case 36:
                        case 47:
                            switch (i) {
                            case 8:
                                u = 0x60;
                                rowIdx = 3;
                                break;
                            case 19:
                                u = 0x60;
                                rowIdx = 2;
                                break;
                            case 24:
                                u = 0x60;
                                rowIdx = 1;
                                break;
                            case 36:
                                u = 0;
                                rowIdx = 2;
                                break;
                            case 47:
                                u = 0;
                                rowIdx = 1;
                                break;
                            }
                            n = i + 23;
                            g_menu_sprites[n].clut = 0x7840;
                            g_menu_sprites[n].tpage = GetTPage(0, 0, 0x2C0, 0x200 - rowIdx * 24);
                            g_menu_sprites[n].u = u;
                            g_menu_sprites[n].v = -(rowIdx * 24);
                            g_menu_sprites[n].attr = 0;
                            break;
                        }
                    }
                }
                if (t != 8) {
                    g_menu_sprites[i + 23].rotX += 0x100;
                    g_menu_sprites[i + 23].rotX %= 0x1000;
                }
            }
            sprite_draw_rotated(&g_menu_sprites[i + 23], &g_ot_2d[g_frame_buffer_index], i + 23);
            sprite_draw(&g_menu_sprites[i + 72], &g_ot_2d[g_frame_buffer_index], i + 72);
        }
        g_menu_anim_frame++;
        goto end;
    }
    if (g_menu_anim_frame < 100) {
        menu_draw_grid_masked(2, -1);
        if (menu_row_flip_to_label(2)) {
            g_menu_busy = 0;
            g_menu_wrap_bank = 1;
            g_menu_state = 0;
            g_menu_anim_frame = 0;
            return 1;
        }
        g_menu_wrap_bank = 0;
        g_menu_row = 2;
    }
end:
    return 0;
}

s32 menu_confirm_show(void) {
    s32 unused[2]; /* unreferenced local: only reserves stack space */

    sprite_draw(&g_menu_sprites[0], &g_ot_2d[g_frame_buffer_index], 0);
    sprite_draw(&g_menu_sprites[1], &g_ot_2d[g_frame_buffer_index], 1);
    sprite_draw(&g_menu_sprites[2], &g_ot_2d[g_frame_buffer_index], 1);
    sprite_draw(&g_menu_sprites[3], &g_ot_2d[g_frame_buffer_index], 2);
    sprite_draw(&g_menu_sprites[4], &g_ot_2d[g_frame_buffer_index], 2);
    sprite_draw(&g_menu_sprites[5], &g_ot_2d[g_frame_buffer_index], 3);
    if (g_menu_anim_frame == 8) {
        g_menu_anim_frame = 0;
        return 1;
    }
    g_menu_anim_frame++;
    return 0;
}

s32 menu_confirm_hide(void) {
    s32 unused[2]; /* unreferenced local: only reserves stack space */

    sprite_draw(&g_menu_sprites[0], &g_ot_2d[g_frame_buffer_index], 0);
    sprite_draw(&g_menu_sprites[1], &g_ot_2d[g_frame_buffer_index], 1);
    sprite_draw(&g_menu_sprites[2], &g_ot_2d[g_frame_buffer_index], 1);
    sprite_draw(&g_menu_sprites[3], &g_ot_2d[g_frame_buffer_index], 2);
    sprite_draw(&g_menu_sprites[4], &g_ot_2d[g_frame_buffer_index], 2);
    sprite_draw(&g_menu_sprites[5], &g_ot_2d[g_frame_buffer_index], 3);
    if (g_menu_anim_frame == 8) {
        g_menu_anim_frame = 0;
        return 1;
    }
    g_menu_anim_frame++;
    return 0;
}

extern SpriteEntry *g_menu_sprites;
extern s16 g_menu_row; /* currently open row, -1 = none */
extern s16 g_menu_wrap_bank;
extern s16 g_menu_busy;
extern s16 g_menu_state; /* menu/dialog state */

/* Used by menu_run. */
extern s16 g_pad_command NO_GP; /* pad command code for this frame (pad_read_command) */
extern s16 g_gate_level NO_GP;
extern s16 g_mcard_accept_pending;       /* mcard_accept issued, result not polled yet */
extern s16 g_menu_prev_row;
extern s16 g_menu_confirm_yes;       /* yes/no cursor: 1 = yes, 0 = no */
extern s16 g_menu_timer;       /* message timer / retry counter */
extern s16 g_menu_card_state;       /* memory card sub-state (save: state 5, load: state 6) */
extern u8 *g_menu_save_buf;       /* 0x6000-byte save buffer */

void pad_read_command(void);
void lain_anim_idle_update(s32 arg0);
void snd_play_sfx(s16 prog);
void msgbox_draw(s32 menu);
void menu_draw_grid_masked(s32 a, s32 b);
void menu_draw(void);
s32 menu_row_flip_to_label(s32 row);
s32 menu_row_flip_to_icons(s32 row);
s32 menu_confirm_show(void);
s32 menu_confirm_hide(void);
s32 heap_alloc(s32);
void heap_free(void *);
void credits_run(void);
/* memory card wrappers (src/game/8003C084.c); errors come back negated */
s32 mcard_create_save_file(void);
s32 mcard_format(void);
void mcard_close_file(void);
s32 mcard_open_save_file(s32 write);
s32 mcard_accept(void);
s32 mcard_read_save(u32 *buf, s32 bytes);
s32 mcard_write_save(u32 *buf, s32 bytes);
s32 mcard_poll(void);
s32 save_build_image(u8 *buf);
s32 save_apply_image(u8 *buf);

/*
 * Pause/system menu driver, run once per frame.
 * g_menu_state: 0 = choose a row (up/down, circle), 1 = yes/no dialog,
 * 2 = row switch animation, 3/4 = dialog open/close, 5 = save to memory card,
 * 6 = load from memory card, 7 = leave (returns -2), 8 = timed message.
 * Returns 1 to close the menu, -1 after a successful load, -2 to leave, else 0.
 *
 * Save (state 5) and load (state 6) are driven by the sub-state g_menu_card_state,
 * using the memory card wrappers (mcard_accept starts an accept/poll,
 * mcard_poll polls it: 0 = busy, 1 = done, -1/-2/-3/-4 = card errors).
 * Error states: 10 / 20 / 30 / 40 / 50 show a message for 120 frames
 * (g_menu_timer) and then drop back to the menu.
 *
 * Matching notes (GCC 2.8 cross-jumping keeps the LAST copy of identical
 * code; the shape of the jumps decides which copy that is):
 * - "goto end" skips the per-frame redraw (menu_draw); plain
 *   "return 0" there schedules the v0 store early and breaks the tail merge.
 * - msgbox_draw(msg) is shared through show_msg (save state 4).
 * - The empty "do {} while (0);" statements (probably compiled-out debug
 *   macros) put a loop note between a label and its jump, which stops jump
 *   threading before reload. That keeps those jumps out of the cross-jump
 *   chains, so the load state's duplicate blocks merge into the save
 *   state's copies as in the original.
 */
s32 menu_run(void) {
    s32 input;
    s32 r;
    s32 msg;

    pad_read_command();
    switch (g_menu_state) {
    case 0:
        switch (g_pad_command) {
        case 1:
        case 2:
        case 3:
            snd_play_sfx(1);
            input = 1;
            break;
        case 6:
        case 7:
        case 8:
            snd_play_sfx(1);
            input = -1;
            break;
        case 0x11:
            input = 2;
            snd_play_sfx(0);
            break;
        default:
            input = 0;
            break;
        }
        if (input == 1) {
            if (g_menu_row > 0) {
                g_menu_prev_row = g_menu_row;
                g_menu_row--;
                menu_row_flip_to_icons(g_menu_prev_row);
                menu_row_flip_to_label(g_menu_row);
                menu_draw_grid_masked(g_menu_prev_row, g_menu_row);
                g_menu_busy = 1;
                g_menu_state = 2;
                snd_play_sfx(1);
                goto end;
            }
        } else if (input < 0) {
            if (g_menu_row < 4) {
                g_menu_prev_row = g_menu_row;
                g_menu_row++;
                menu_row_flip_to_icons(g_menu_prev_row);
                menu_row_flip_to_label(g_menu_row);
                menu_draw_grid_masked(g_menu_prev_row, g_menu_row);
                g_menu_busy = 1;
                g_menu_state = 2;
                snd_play_sfx(1);
                goto end;
            }
        } else if (input == 2) {
            menu_draw();
            switch (g_menu_row) {
            case 1:
                credits_run();
                lain_anim_idle_update(1);
                goto end;
            case 2:
                if (g_gate_level < 4) {
                    g_menu_state = 8;
                    g_menu_timer = 60;
                    goto end;
                }
                /* fallthrough */
            case 0:
            case 3:
                g_menu_busy = 1;
                g_menu_state = 3;
                g_menu_confirm_yes = 0;
                g_menu_sprites[5].x = 0xE6;
                goto end;
            case 4:
                return 1;
            }
            goto end;
        }
        break;

    case 1: /* yes/no dialog */
        switch (g_pad_command) {
        case 2:
        case 4:
        case 7:
            snd_play_sfx(1);
            input = -1;
            break;
        case 3:
        case 5:
        case 8:
            snd_play_sfx(1);
            input = 1;
            break;
        case 0x11:
            input = 2;
            snd_play_sfx(0x1A);
            break;
        default:
            input = 0;
            break;
        }
        if (input < 0) {
            if (g_menu_confirm_yes == 0) {
                g_menu_confirm_yes = 1;
                snd_play_sfx(1);
            }
        } else if (input == 1) {
            if (g_menu_confirm_yes != 0) {
                g_menu_confirm_yes = 0;
                snd_play_sfx(1);
            }
        }
        if (g_menu_confirm_yes != 0) {
            g_menu_sprites[5].x = 0x22;
        } else {
            g_menu_sprites[5].x = 0xE6;
        }
        menu_draw();
        if (input == 2) {
            if (g_menu_confirm_yes == 0) {
                g_menu_state = 4;
                g_menu_busy = 1;
                goto end;
            }
            switch (g_menu_row) {
            case 0:
                g_menu_state = 6;
                break;
            case 2:
                g_menu_state = 7;
                break;
            case 3:
                g_menu_state = 5;
                break;
            }
            g_menu_card_state = 0;
        }
        goto end;

    case 2: /* row switch animation */
        menu_row_flip_to_icons(g_menu_prev_row);
        if (menu_row_flip_to_label(g_menu_row)) {
            g_menu_busy = 0;
            g_menu_state = 0;
            if (++g_menu_wrap_bank == 2) {
                g_menu_wrap_bank = 0;
            }
        }
        menu_draw_grid_masked(g_menu_prev_row, g_menu_row);
        goto end;

    case 3:
        menu_draw();
        if (menu_confirm_show()) {
            g_menu_state = 1;
            g_menu_busy = 0;
        }
        goto end;

    case 4:
        menu_draw();
        if (menu_confirm_hide()) {
            g_menu_state = 0;
            g_menu_busy = 0;
        }
        goto end;

    case 5: /* save */
        switch (g_menu_card_state) {
        case 0:
        case 0x24:
            mcard_accept();
            g_menu_card_state++;
            break;
        case 1:
            r = mcard_poll();
            if (r != 1) {
                switch (-r) {
                case 1:
                    snd_play_sfx(0x1C);
                    g_menu_card_state = 10;
                    g_menu_timer = 120;
                    break;
                case 2:
                    snd_play_sfx(0x1C);
                    g_menu_card_state = 20;
                    g_menu_timer = 120;
                    break;
                case 3:
                    g_menu_card_state = 0;
                    break;
                case 4:
                    g_menu_card_state = 0x23;
                    break;
                }
            } else {
                g_menu_card_state++;
            }
            break;
        case 2:
            switch (-mcard_open_save_file(1)) {
            case 0:
                g_menu_card_state++;
                break;
            case 1:
                snd_play_sfx(0x1C);
                g_menu_card_state = 10;
                g_menu_timer = 120;
                break;
            case 2:
                snd_play_sfx(0x1C);
                g_menu_card_state = 20;
                g_menu_timer = 120;
                break;
            case 4:
                g_menu_card_state = 0x23;
                break;
            case 5:
                snd_play_sfx(0x1C);
                g_menu_card_state = 0x28;
                break;
            }
            break;
        case 3:
            msgbox_draw(8);
            g_menu_card_state++;
            break;
        case 4:
            if (g_mcard_accept_pending != 0) {
                r = mcard_poll();
                if (r != 0) {
                    g_mcard_accept_pending = 0;
                    if (r != 1) {
                        g_menu_card_state = 9;
                        g_menu_timer = 5;
                        break;
                    }
                }
            } else {
                mcard_accept();
                g_mcard_accept_pending = 1;
            }
            if (g_pad_command != 0) {
                if (g_pad_command == 0x14) {
                    snd_play_sfx(0x1A);
                    if (g_mcard_accept_pending == 0) {
                        msgbox_draw(10);
                        g_menu_card_state++;
                    } else {
                        g_menu_card_state = 8;
                    }
                } else {
                    snd_play_sfx(0x1B);
                    if (g_mcard_accept_pending != 0) {
                        g_menu_card_state = 0x5A;
                    } else {
                        g_menu_state = 0;
                        mcard_close_file();
                    }
                }
            } else {
                msg = 9;
            show_msg:
                msgbox_draw(msg);
            }
            break;
        case 5:
            g_menu_save_buf = (u8 *)heap_alloc(0x6000);
            mcard_write_save((u32 *)g_menu_save_buf, save_build_image(g_menu_save_buf) << 7);
            msgbox_draw(10);
            g_menu_card_state++;
            break;
        case 6:
            msgbox_draw(10);
            r = mcard_poll();
            if (r != 0) {
                heap_free(g_menu_save_buf);
                g_menu_save_buf = NULL;
                mcard_close_file();
                if (r == 1) {
                    g_menu_timer = 120;
                    g_menu_card_state++;
                } else {
                    switch (-r) {
                    case 1:
                        snd_play_sfx(0x1C);
                        g_menu_card_state = 10;
                        g_menu_timer = 120;
                        break;
                    case 2:
                        snd_play_sfx(0x1C);
                        g_menu_card_state = 20;
                        g_menu_timer = 120;
                        break;
                    case 3:
                        g_menu_card_state = 0;
                        break;
                    }
                }
            }
            break;
        case 7:
            if (--g_menu_timer == 0) {
                g_menu_state = 0;
            }
            msg = 11;
            goto show_msg;
        case 8:
            r = mcard_poll();
            if (r != 0) {
                g_mcard_accept_pending = 0;
                if (r == 1) {
                    msgbox_draw(10);
                    g_menu_card_state = 5;
                } else {
                    g_menu_card_state = 9;
                    g_menu_timer = 5;
                }
            }
            break;
        case 9:
            if (g_menu_timer > 0) {
                g_menu_timer--;
            }
            if (g_menu_timer == 0) {
                if (g_mcard_accept_pending == 0) {
                    mcard_accept();
                    g_mcard_accept_pending = 1;
                } else {
                    r = mcard_poll();
                    if (r != 0) {
                        g_mcard_accept_pending = 0;
                        if (r == 1) {
                            g_menu_card_state = 4;
                        } else {
                            switch (-r) {
                            case 0:
                                g_menu_card_state = 4;
                                break;
                            case 1:
                                snd_play_sfx(0x1C);
                                g_menu_card_state = 10;
                                g_menu_timer = 120;
                                mcard_close_file();
                                break;
                            case 2: /* same handling as state 39 */
                                goto card_error_2;
                            case 3:
                                goto card_error_3;
                            }
                        }
                    }
                }
            }
            break;
        case 10:
            if (--g_menu_timer == 0) {
                g_menu_state = 0;
            }
            msg = 1;
            goto show_msg;
        case 20:
            if (--g_menu_timer == 0) {
                g_menu_state = 0;
            }
            msg = 6;
            goto show_msg;
        case 30:
            msgbox_draw(3);
            g_menu_card_state++;
            break;
        case 31:
            if (g_mcard_accept_pending != 0) {
                r = mcard_poll();
                if (r != 0) {
                    g_mcard_accept_pending = 0;
                    if (r != -4) {
                        g_menu_card_state = 0x27;
                        g_menu_timer = 5;
                        break;
                    }
                }
            } else {
                mcard_accept();
                g_mcard_accept_pending = 1;
            }
            if (g_pad_command != 0) {
                if (g_pad_command == 0x14) {
                    snd_play_sfx(0x1A);
                    if (g_mcard_accept_pending == 0) {
                        msgbox_draw(4);
                        g_menu_card_state++;
                    } else {
                        g_menu_card_state = 0x26;
                    }
                } else {
                    snd_play_sfx(0x1B);
                    if (g_mcard_accept_pending != 0) {
                        g_menu_card_state = 0x5A;
                    } else {
                        g_menu_state = 0;
                    }
                }
            } else {
                msg = 3;
                goto show_msg;
            }
            break;
        case 32:
            switch (-mcard_format()) {
            case 0:
                g_menu_card_state = 0x28;
                msg = 5;
                goto show_msg;
            case 1:
                snd_play_sfx(0x1C);
                g_menu_card_state = 10;
                g_menu_timer = 120;
                break;
            case 2:
                snd_play_sfx(0x1C);
                g_menu_card_state = 20;
                g_menu_timer = 120;
                break;
            }
            break;
        case 35:
            g_menu_timer = 0;
            g_menu_card_state++;
            break;
        case 37:
            r = mcard_poll();
            if (r != 0) {
                if (r == 1) {
                    g_menu_card_state = 0;
                } else {
                    switch (-r) {
                    case 1:
                        snd_play_sfx(0x1C);
                        g_menu_card_state = 10;
                        g_menu_timer = 120;
                        break;
                    case 2:
                        snd_play_sfx(0x1C);
                        g_menu_card_state = 20;
                        g_menu_timer = 120;
                        break;
                    case 3:
                        g_menu_card_state = 0;
                        break;
                    case 4:
                        if (++g_menu_timer == 5) {
                            snd_play_sfx(0x1C);
                            g_menu_card_state = 0x1E;
                        } else {
                            g_menu_card_state = 0x24;
                        }
                        break;
                    }
                }
            }
            break;
        case 38:
            r = mcard_poll();
            if (r != 0) {
                g_mcard_accept_pending = 0;
                if (r == -4) {
                    msgbox_draw(4);
                    g_menu_card_state = 0x20;
                } else {
                    g_menu_card_state = 0x27;
                    g_menu_timer = 5;
                }
            }
            break;
        case 39:
            if (g_menu_timer > 0) {
                g_menu_timer--;
            }
            if (g_menu_timer == 0) {
                if (g_mcard_accept_pending == 0) {
                    mcard_accept();
                    g_mcard_accept_pending = 1;
                } else {
                    r = mcard_poll();
                    if (r != 0) {
                        g_mcard_accept_pending = 0;
                        if (r == 1) {
                            g_menu_card_state = 0x1F;
                        } else {
                            switch (-r) {
                            case 0:
                                g_menu_card_state = 0x1F;
                                break;
                            default:
                                do {} while (0);
                                break;
                            case 1:
                                snd_play_sfx(0x1C);
                                g_menu_card_state = 10;
                                g_menu_timer = 120;
                                mcard_close_file();
                                break;
                            case 2:
                            card_error_2:
                                snd_play_sfx(0x1C);
                                g_menu_card_state = 20;
                                g_menu_timer = 120;
                                mcard_close_file();
                                break;
                            case 3:
                            card_error_3:
                                g_menu_card_state = 0;
                                mcard_close_file();
                                break;
                            }
                        }
                    }
                }
            }
            break;
        case 40:
            switch (-mcard_create_save_file()) {
            case 0:
                g_menu_card_state++;
                break;
            case 1:
                snd_play_sfx(0x1C);
                g_menu_card_state = 10;
                g_menu_timer = 120;
                break;
            case 2:
                snd_play_sfx(0x1C);
                g_menu_card_state = 20;
                g_menu_timer = 120;
                break;
            case 4:
                g_menu_card_state = 0x23;
                break;
            case 7:
                snd_play_sfx(0x1C);
                g_menu_card_state = 0x32;
                g_menu_timer = 120;
                break;
            }
            break;
        case 41:
            switch (-mcard_open_save_file(1)) {
            case 0:
                g_menu_card_state = 5;
                break;
            case 1:
                snd_play_sfx(0x1C);
                g_menu_card_state = 10;
                g_menu_timer = 120;
                break;
            case 2:
                snd_play_sfx(0x1C);
                g_menu_card_state = 20;
                g_menu_timer = 120;
                break;
            case 4:
                g_menu_card_state = 0x23;
                break;
            case 5:
                g_menu_card_state = 0x28;
                break;
            }
            break;
        case 50:
            if (--g_menu_timer == 0) {
                g_menu_state = 0;
            }
            msg = 7;
            goto show_msg;
        case 90:
            if (mcard_poll() != 0) {
                mcard_close_file();
                g_menu_state = 0;
                g_mcard_accept_pending = 0;
            }
            break;
        }
        break;

    case 6: /* load */
        switch (g_menu_card_state) {
        case 0:
            mcard_accept();
            g_menu_card_state++;
            break;
        case 1:
            r = mcard_poll();
            if (r != 1) {
                switch (-r) {
                case 1:
                    snd_play_sfx(0x1C);
                    g_menu_card_state = 10;
                    g_menu_timer = 120;
                    break;
                case 2:
                    snd_play_sfx(0x1C);
                    g_menu_card_state = 20;
                    g_menu_timer = 120;
                    break;
                case 3:
                    g_menu_card_state = 0;
                    break;
                case 4:
                    snd_play_sfx(0x1C);
                    g_menu_card_state = 30;
                    g_menu_timer = 120;
                    break;
                }
            } else {
                g_menu_card_state = 100;
            }
            break;
        case 100:
            switch (-mcard_open_save_file(0)) {
            case 0:
                g_menu_timer = 10;
                g_menu_card_state++;
                break;
            case 1:
                snd_play_sfx(0x1C);
                g_menu_card_state = 10;
                g_menu_timer = 120;
                break;
            case 2:
                snd_play_sfx(0x1C);
                g_menu_card_state = 20;
                g_menu_timer = 120;
                break;
            case 4:
                snd_play_sfx(0x1C);
                g_menu_card_state = 30;
                g_menu_timer = 120;
                break;
            case 5:
                snd_play_sfx(0x1C);
                g_menu_card_state = 40;
                g_menu_timer = 120;
                break;
            }
            break;
        case 101:
            g_menu_save_buf = (u8 *)heap_alloc(0x6000);
            mcard_read_save((u32 *)g_menu_save_buf, save_build_image(g_menu_save_buf) << 7);
            msgbox_draw(15);
            g_menu_card_state = 2;
            break;
        case 2:
            msgbox_draw(15);
            r = mcard_poll();
            if (r != 0) {
                if (r == 1) {
                    if (save_apply_image(g_menu_save_buf)) {
                        g_menu_card_state++;
                    } else {
                        g_menu_card_state += 2;
                    }
                    g_menu_timer = 120;
                    mcard_close_file();
                    break;
                } else {
                    switch (-r) {
                    case 1:
                        if (--g_menu_timer == 0) {
                            snd_play_sfx(0x1C);
                            g_menu_card_state = 10;
                            g_menu_timer = 120;
                        } else {
                            g_menu_card_state = 0;
                        }
                        mcard_close_file();
                        heap_free(g_menu_save_buf);
                        break;
                    case 2:
                        snd_play_sfx(0x1C);
                        g_menu_card_state = 20;
                        g_menu_timer = 120;
                        heap_free(g_menu_save_buf);
                        mcard_close_file();
                        break;
                    case 3:
                        g_menu_card_state = 0;
                        heap_free(g_menu_save_buf);
                        mcard_close_file();
                        break;
                    }
                    goto load_done; /* skips the loop note below */
                }
            }
            break;
        case 3:
            if (--g_menu_timer == 0) {
                g_menu_state = 0;
                heap_free(g_menu_save_buf);
                return -1;
            }
            msg = 16;
            goto show_msg2;
        case 4:
            if (--g_menu_timer == 0) {
                g_menu_state = 0;
                heap_free(g_menu_save_buf);
            }
            msg = 14;
            goto show_msg2;
        case 10:
            if (--g_menu_timer == 0) {
                g_menu_state = 0;
            }
            msg = 1;
            goto show_msg2;
        case 20:
            if (--g_menu_timer == 0) {
                g_menu_state = 0;
            }
            msg = 6;
            goto show_msg2;
        case 30:
            if (--g_menu_timer == 0) {
                g_menu_state = 0;
            }
            msg = 17;
            goto show_msg2;
        case 40:
            if (--g_menu_timer == 0) {
                g_menu_state = 0;
            }
            msg = 13;
            goto show_msg2;
        show_msg2: /* load messages reuse save's show_msg */
            do {} while (0);
            goto show_msg;
        }
        do {} while (0);
    load_done:
        break;

    case 7:
        menu_draw();
        return -2;

    case 8:
        if (--g_menu_timer == 0) {
            g_menu_state = 0;
        }
        break;

    default:
        return 0;
    }
    menu_draw();
end:
    return 0;
}


/* Grid close animation; returns 1 once finished (and frees the sprite table). */
s32 menu_close(void) {
    s32 i;
    s32 col;
    s32 row;

    if (g_menu_anim_frame == 0) {
        if (menu_row_flip_to_icons(g_menu_row)) {
            g_menu_anim_frame++;
        }
        menu_draw_grid_masked(g_menu_row, -1);
        return 0;
    }
    if (g_menu_anim_frame > 0) {
        for (i = 0; i < 49; i++) {
            col = i % 7 - 3;
            row = i / 7 - 3;
            if (g_menu_anim_frame != 0) {
                g_menu_sprites[i + 23].rotY += 0x40;
                g_menu_sprites[i + 23].rotZ += 0x40;
                g_menu_sprites[i + 72].rotY += 0x40;
                g_menu_sprites[i + 72].rotZ += 0x40;
            }
            g_menu_sprites[i + 23].x += col * 5;
            g_menu_sprites[i + 23].y += row * 5;
            g_menu_sprites[i + 72].x += col * 5;
            g_menu_sprites[i + 72].y += row * 5;
            sprite_draw_rotated(&g_menu_sprites[i + 23], &g_ot_2d[g_frame_buffer_index], i + 23);
            sprite_draw_rotated(&g_menu_sprites[i + 72], &g_ot_2d[g_frame_buffer_index], i + 72);
        }
        if (g_menu_anim_frame == 16) {
            heap_free(g_menu_sprites);
            g_menu_anim_frame = 0;
            return 1;
        }
        g_menu_anim_frame++;
    }
    return 0;
}

/* Allocate and initialise the 98-entry sprite table (49 item icons + 49 overlays). */
void spinner_init(s32 unused, s16 arg1) {
    s32 i;

    g_spinner_ot_pri = arg1;
    g_menu_anim_frame = -0x8000;
    g_menu_sprites = (SpriteEntry *)heap_alloc(0x1D18);
    if (g_menu_sprites != NULL) {
        memset(g_menu_sprites, 0, 0x1D18);
        for (i = 0; i < 49; i++) {
            g_menu_sprites[i].x = 0x98;
            g_menu_sprites[i].y = 0x60;
            g_menu_sprites[i].r = 0x80;
            g_menu_sprites[i].g = 0x80;
            g_menu_sprites[i].b = 0x80;
            g_menu_sprites[i].scaleX = 0x1000;
            g_menu_sprites[i].scaleY = 0x1000;
            spinner_cell_set_icon(i);
            g_menu_sprites[i + 49].r = 0x80;
            g_menu_sprites[i + 49].g = 0x80;
            g_menu_sprites[i + 49].b = 0x80;
            g_menu_sprites[i + 49].x = 0x99;
            g_menu_sprites[i + 49].y = 0x61;
            g_menu_sprites[i + 49].clut = 0x78C1;
            g_menu_sprites[i + 49].w = 0xE;
            g_menu_sprites[i + 49].h = 0xE;
            g_menu_sprites[i + 49].tpage = 0x1A;
            g_menu_sprites[i + 49].scaleX = 0x1000;
            g_menu_sprites[i + 49].scaleY = 0x1000;
            g_menu_sprites[i + 49].u = 0;
            g_menu_sprites[i + 49].v = 0xE8;
        }
    }
}

/* Zoom animation step for the 7x7 item grid: scale positions by g_menu_anim_frame and draw both layers. */
void spinner_draw(void) {
    s32 i;
    s32 col;
    s32 row;
    s16 t;

    if (g_menu_sprites != NULL) {
        for (i = 0; i < 49; i++) {
            row = i / 7;
            col = i % 7;
            t = g_menu_anim_frame;
            g_menu_sprites[i].x = (col * 24 - 72) * t / 16 + 0x98;
            g_menu_sprites[i].y = (row * 24 - 72) * t / 16 + 0x60;
            g_menu_sprites[i + 49].x = (col * 21 - 63) * t / 16 + 0x99;
            g_menu_sprites[i + 49].y = (row * 21 - 63) * t / 16 + 0x61;
            if (t != 0) {
                g_menu_sprites[i].rotY -= 0x40;
                g_menu_sprites[i].rotZ -= 0x40;
                g_menu_sprites[i + 49].rotY -= 0x40;
                g_menu_sprites[i + 49].rotZ -= 0x40;
            }
            sprite_draw_rotated(&g_menu_sprites[i], &g_ot[g_frame_buffer_index], g_spinner_ot_pri);
            sprite_draw_rotated(&g_menu_sprites[i + 49], &g_ot[g_frame_buffer_index], g_spinner_ot_pri + 1);
        }
        g_menu_anim_frame = (g_menu_anim_frame + 1) | 0x8000;
    }
}

typedef struct {
    u32 mode;
    RECT *crect;
    u32 *caddr;
    RECT *prect;
    u32 *paddr;
} TIM_IMAGE;

extern s32 g_bin_file_table[];
extern s32 D_80098EFC[]; /* = inside g_bin_file_table: entry 2 .size */
s32 cd_load_archive_entry(s32, s32, s32 *, s32);
s32 cd_poll_load(s32);
void loading_anim_draw(s32);
s32 lz_decompress(s32, s32);
void free(void *);
s32 OpenTIM(u32 *);
TIM_IMAGE *ReadTIM(TIM_IMAGE *);
u16 LoadClut(u32 *clut, s32 x, s32 y);
s32 LoadImage(RECT *rect, u32 *p);

/* Load a TIM from disc, upload its CLUT and image to VRAM. */
void menu_load_texture(void) {
    RECT rect;
    TIM_IMAGE tim;
    s32 buf;
    s32 req;
    s32 data;

    buf = heap_alloc(g_bin_file_table[5]);
    req = cd_load_archive_entry(3, 2, g_bin_file_table, buf);
    while (cd_poll_load(req) == 0) {
        loading_anim_draw(0);
    }
    data = lz_decompress(buf, D_80098EFC[0]);
    free((void *)buf);
    OpenTIM((u32 *)data);
    ReadTIM(&tim);
    LoadClut(tim.caddr, 0, 0x1E3);
    rect.x = 0x280;
    rect.y = 0x1C8;
    rect.w = 0x3E;
    rect.h = 0x30;
    LoadImage(&rect, tim.paddr);
    heap_free((void *)data);
    g_menu_anim_frame = 0;
}

void menu_sprites_free(void) {
    heap_free(g_menu_sprites);
    g_menu_anim_frame = 0;
}

void spinner_free(void) {
    heap_free(g_menu_sprites);
    g_menu_anim_frame = 0;
}

void GsSortPoly(void *prim, GsOT *ot, u16 pri);

/* Build the entry's POLY_FT4 from its sprite fields and add it to the OT. */
void sprite_draw(SpriteEntry *e, GsOT *ot, u16 pri) {
    ((u8 *)&e->poly.tag)[3] = 9; /* setlen */
    e->poly.code = 0x2C;
    e->poly.r0 = e->r;
    e->poly.g0 = e->g;
    e->poly.b0 = e->b;
    e->poly.tpage = e->tpage;
    e->poly.clut = e->clut;
    if (e->attr & 0x40000000) {
        e->poly.code |= 2;
    } else {
        e->poly.code &= ~2;
    }
    if (e->attr & 0x800000) {
        e->poly.u0 = e->u + e->w - 1;
        e->poly.u1 = e->u;
        e->poly.u2 = e->u + e->w - 1;
        e->poly.u3 = e->u;
    } else {
        e->poly.u0 = e->u;
        e->poly.u1 = e->u + e->w - 1;
        e->poly.u2 = e->u;
        e->poly.u3 = e->u + e->w - 1;
    }
    if (e->attr & 0x400000) {
        e->poly.v0 = e->v + e->h - 1;
        e->poly.v1 = e->v + e->h - 1;
        e->poly.v2 = e->v;
        e->poly.v3 = e->v;
    } else {
        e->poly.v0 = e->v;
        e->poly.v1 = e->v;
        e->poly.v2 = e->v + e->h - 1;
        e->poly.v3 = e->v + e->h - 1;
    }
    e->poly.x0 = e->x;
    e->poly.y0 = e->y;
    e->poly.x1 = e->x + e->w;
    e->poly.y1 = e->y;
    e->poly.x2 = e->x;
    e->poly.y2 = e->y + e->h;
    e->poly.x3 = e->x + e->w;
    e->poly.y3 = e->y + e->h;
    GsSortPoly(e, ot, pri);
}

typedef struct {
    s16 vx, vy, vz, pad;
} SVECTOR;

typedef struct {
    s32 vx, vy, vz, pad;
} VECTOR;

typedef struct {
    s16 m[3][3];
    s32 t[3];
} MATRIX;

void PushMatrix(void);
void PopMatrix(void);
MATRIX *RotMatrix_gte(SVECTOR *r, MATRIX *m);
void SetRotMatrix(MATRIX *m);
VECTOR *ApplyRotMatrixLV(VECTOR *v0, VECTOR *v1);

/* Like sprite_draw but rotates/scales the quad around its centre (with perspective). */
void sprite_draw_rotated(SpriteEntry *e, GsOT *ot, u16 pri) {
    VECTOR in[4];
    VECTOR out[4];
    SVECTOR rot;
    MATRIX m;
    s32 halfW;
    s32 halfH;
    s32 cx;
    s32 cy;
    s32 z;
    s32 i;
    s32 dist;
    s32 add;

    ((u8 *)&e->poly.tag)[3] = 9; /* setlen */
    e->poly.code = 0x2C;
    e->poly.r0 = e->r;
    e->poly.g0 = e->g;
    e->poly.b0 = e->b;
    e->poly.tpage = e->tpage;
    e->poly.clut = e->clut;
    if (e->attr & 0x40000000) {
        e->poly.code |= 2;
    } else {
        e->poly.code &= ~2;
    }
    if (e->attr & 0x800000) {
        e->poly.u0 = e->u + e->w - 1;
        e->poly.u1 = e->u;
        e->poly.u2 = e->u + e->w - 1;
        e->poly.u3 = e->u;
    } else {
        e->poly.u0 = e->u;
        e->poly.u1 = e->u + e->w - 1;
        e->poly.u2 = e->u;
        e->poly.u3 = e->u + e->w - 1;
    }
    if (e->attr & 0x400000) {
        e->poly.v0 = e->v + e->h - 1;
        e->poly.v1 = e->v + e->h - 1;
        e->poly.v2 = e->v;
        e->poly.v3 = e->v;
    } else {
        e->poly.v0 = e->v;
        e->poly.v1 = e->v;
        e->poly.v2 = e->v + e->h - 1;
        e->poly.v3 = e->v + e->h - 1;
    }
    halfW = e->w / 2;
    halfH = e->h / 2;
    cx = e->x + halfW;
    cy = e->y + halfH;
    PushMatrix();
    rot.vx = e->rotX;
    rot.vy = e->rotY;
    rot.vz = e->rotZ;
    RotMatrix_gte(&rot, &m);
    SetRotMatrix(&m);
    in[0].vx = -halfW * e->scaleX / 4096;
    in[0].vy = -halfH * e->scaleY / 4096;
    in[0].vz = 0;
    in[1].vx = halfW * e->scaleX / 4096;
    in[1].vy = -halfH * e->scaleY / 4096;
    in[1].vz = 0;
    in[2].vx = -halfW * e->scaleX / 4096;
    in[2].vy = halfH * e->scaleY / 4096;
    in[2].vz = 0;
    in[3].vx = halfW * e->scaleX / 4096;
    in[3].vy = halfH * e->scaleY / 4096;
    in[3].vz = 0;
    ApplyRotMatrixLV(&in[0], &out[0]);
    ApplyRotMatrixLV(&in[1], &out[1]);
    ApplyRotMatrixLV(&in[2], &out[2]);
    ApplyRotMatrixLV(&in[3], &out[3]);
    z = out[0].vz;
    for (i = 1; i < 4; i++) {
        if (out[i].vz < z) {
            z = out[i].vz;
        }
    }
    if (z < 0) {
        z = -z;
    }
    dist = z + 350;
    add = dist;
    for (i = 0; i < 4; i++) {
        out[i].vz += add;
    }
    e->poly.x0 = dist * out[0].vx / out[0].vz + cx;
    e->poly.y0 = dist * out[0].vy / out[0].vz + cy;
    e->poly.x1 = dist * out[1].vx / out[1].vz + cx;
    e->poly.y1 = dist * out[1].vy / out[1].vz + cy;
    e->poly.x2 = dist * out[2].vx / out[2].vz + cx;
    e->poly.y2 = dist * out[2].vy / out[2].vz + cy;
    e->poly.x3 = dist * out[3].vx / out[3].vz + cx;
    e->poly.y3 = dist * out[3].vy / out[3].vz + cy;
    PopMatrix();
    GsSortPoly(e, ot, pri);
}

extern s32 g_dobj_id_next;
extern u8 g_ot_tags0[];
extern u8 g_ot_tags1[];
extern u32 g_ot_2d_tags[2][256];
s32 ResetGraph(s32);
void InitGeom(void);
void GsInitGraph(u16 x, u16 y, u16 intmode, u16 dith, u16 varmmode);
void GsDefDispBuff2(u16 x0, u16 y0, u16 x1, u16 y1);
void GsInit3D(void);
void GsSetOrign(s32 x, s32 y);
void SetGeomOffset(s32 x, s32 y);
void camera_init(void);
void light_apply_preset(void);
void GsSetAmbient(s32 r, s32 g, s32 b);
s32 GsSetLightMode(s32 mode);
s32 ClearImage2(RECT *rect, u8 r, u8 g, u8 b);

/* Graphics initialisation: GPU reset, display buffers, ordering tables, lights. */
void gfx_init(void) {
    RECT rect;

    g_dobj_id_next = 0;
    ResetGraph(0);
    DrawSync(0);
    InitGeom();
    GsInitGraph(0x140, 0xF0, 4, 1, 0);
    GsDefDispBuff2(0, 0, 0, 0xF0);
    GsInit3D();
    GsSetOrign(0, 0);
    SetGeomOffset(0xA0, 0x78);
    g_ot[0].length = 14;
    g_ot[0].org = g_ot_tags0;
    g_ot[1].length = 14;
    g_ot[1].org = g_ot_tags1;
    g_ot_2d[0].length = 8;
    g_ot_2d[0].org = g_ot_2d_tags[0];
    g_ot_2d[1].length = 8;
    g_ot_2d[1].org = g_ot_2d_tags[1];
    camera_init();
    light_apply_preset();
    GsSetAmbient(0x80, 0x80, 0x80);
    GsSetLightMode(0);
    rect.x = 0;
    rect.y = 0;
    rect.w = 0x140;
    rect.h = 0x200;
    ClearImage2(&rect, 0, 0, 0);
}

typedef struct {
    s32 vx, vy, vz;
    u8 r, g, b;
    u8 pad;
} FlatLight; /* GsF_LIGHT */

extern FlatLight g_flat_lights[];
extern FlatLight g_light_preset_default[];
extern FlatLight g_light_preset_alt[];
extern u8 g_camera_view;
s32 GsSetFlatLight(s32 id, FlatLight *lt);

/* Set up the three flat lights from one of two presets depending on mode. */
void light_apply_preset(void) {
    s32 i;

    for (i = 0; i < 3; i++) {
        switch (g_camera_view) {
        case 2:
        case 4:
            g_flat_lights[i] = g_light_preset_alt[i];
            break;
        case 3:
            g_flat_lights[i] = g_light_preset_default[i];
            break;
        default:
            g_flat_lights[i] = g_light_preset_default[i];
            break;
        }
        GsSetFlatLight(i, &g_flat_lights[i]);
    }
}

extern s32 g_light_fade_index;
extern u8 g_light_fade_active;
extern FlatLight *g_light_fade_target;

/* Step the colour of flat light g_light_fade_index by 2 towards the target colour; stop when reached. */
void light_fade_step(void) {
    if (g_light_fade_active == 1) {
        if (g_flat_lights[g_light_fade_index].r != g_light_fade_target->r) {
            if (g_flat_lights[g_light_fade_index].r > g_light_fade_target->r) {
                g_flat_lights[g_light_fade_index].r -= 2;
            } else if (g_flat_lights[g_light_fade_index].r < g_light_fade_target->r) {
                g_flat_lights[g_light_fade_index].r += 2;
            }
        }
        if (g_flat_lights[g_light_fade_index].g != g_light_fade_target->g) {
            if (g_flat_lights[g_light_fade_index].g > g_light_fade_target->g) {
                g_flat_lights[g_light_fade_index].g -= 2;
            } else if (g_flat_lights[g_light_fade_index].g < g_light_fade_target->g) {
                g_flat_lights[g_light_fade_index].g += 2;
            }
        }
        if (g_flat_lights[g_light_fade_index].b != g_light_fade_target->b) {
            if (g_flat_lights[g_light_fade_index].b > g_light_fade_target->b) {
                g_flat_lights[g_light_fade_index].b -= 2;
            } else if (g_flat_lights[g_light_fade_index].b < g_light_fade_target->b) {
                g_flat_lights[g_light_fade_index].b += 2;
            }
        }
        GsSetFlatLight(g_light_fade_index, &g_flat_lights[g_light_fade_index]);
        if (g_flat_lights[g_light_fade_index].r == g_light_fade_target->r &&
            g_flat_lights[g_light_fade_index].g == g_light_fade_target->g &&
            g_flat_lights[g_light_fade_index].b == g_light_fade_target->b) {
            g_light_fade_active = 0;
        }
    }
}

typedef struct {
    u32 pmode;
    s16 px, py;
    u16 pw, ph;
    u32 *pixel;
    s16 cx, cy;
    u16 cw, ch;
    u32 *clut;
} GsIMAGE;

extern s16 g_clut_fade_busy;
extern u16 g_clut_fade_buf[];
extern u16 g_clut_fade_target[];
void GsGetTimInfo(u32 *im, GsIMAGE *tim);
s32 StoreImage(RECT *rect, u32 *p);

/* Move the CLUT of `tim` one step per channel towards the CLUT of `target`; g_clut_fade_busy set while still changing. */
void tim_clut_fade_toward(u32 *tim, u32 *target) {
    GsIMAGE info;
    RECT rect;
    RECT rect2;
    u8 cur[3];
    u8 dst[3];
    s32 count;
    s32 i;

    g_clut_fade_busy = 0;
    GsGetTimInfo(tim + 1, &info);
    rect.x = info.cx;
    rect.y = info.cy;
    rect.w = info.cw;
    rect.h = info.ch;
    StoreImage(&rect, (u32 *)g_clut_fade_buf);
    DrawSync(0);
    if (info.pmode & 8) {
        if ((info.pmode & 7) == 0) {
            count = 16; /* 4-bit CLUT */
        } else {
            count = 256; /* 8-bit CLUT */
        }
        GsGetTimInfo(target + 1, &info);
        rect2.x = info.cx;
        rect2.y = info.cy;
        rect2.w = info.cw;
        rect2.h = info.ch;
        StoreImage(&rect2, (u32 *)g_clut_fade_target);
        DrawSync(0);
        for (i = 0; i < count; i++) {
            cur[0] = g_clut_fade_buf[i] & 0x1F;
            cur[1] = (g_clut_fade_buf[i] >> 5) & 0x1F;
            cur[2] = (g_clut_fade_buf[i] >> 10) & 0x1F;
            dst[0] = g_clut_fade_target[i] & 0x1F;
            dst[1] = (g_clut_fade_target[i] >> 5) & 0x1F;
            dst[2] = (g_clut_fade_target[i] >> 10) & 0x1F;
            if (cur[0] > dst[0]) {
                cur[0]--;
            } else if (cur[0] < dst[0]) {
                cur[0]++;
            }
            if (cur[1] > dst[1]) {
                cur[1]--;
            } else if (cur[1] < dst[1]) {
                cur[1]++;
            }
            if (cur[2] > dst[2]) {
                cur[2]--;
            } else if (cur[2] < dst[2]) {
                cur[2]++;
            }
            g_clut_fade_buf[i] = cur[0];
            g_clut_fade_buf[i] |= cur[1] << 5;
            g_clut_fade_buf[i] |= cur[2] << 10;
            g_clut_fade_buf[i] |= g_clut_fade_target[i] & 0x8000;
            if (g_clut_fade_buf[i] != g_clut_fade_target[i]) {
                g_clut_fade_busy = 0xFF;
            }
        }
        LoadImage(&rect, (u32 *)g_clut_fade_buf);
        DrawSync(0);
    }
}

/* Darken the CLUT of a TIM by one step per channel (fade out); g_clut_fade_busy stays 0 once all black. */
void tim_clut_fade_out(u32 *tim) {
    GsIMAGE info;
    RECT rect;
    u8 rgb[3];
    s32 count;
    s32 i;

    g_clut_fade_busy = 0;
    GsGetTimInfo(tim + 1, &info);
    rect.x = info.cx;
    rect.y = info.cy;
    rect.w = info.cw;
    rect.h = info.ch;
    StoreImage(&rect, (u32 *)g_clut_fade_buf);
    DrawSync(0);
    if (info.pmode & 8) {
        if ((info.pmode & 7) == 0) {
            count = 16; /* 4-bit CLUT */
        } else {
            count = 256; /* 8-bit CLUT */
        }
        for (i = 0; i < count; i++) {
            g_clut_fade_target[0] = g_clut_fade_buf[i] & 0x8000;
            rgb[0] = g_clut_fade_buf[i] & 0x1F;
            rgb[1] = (g_clut_fade_buf[i] >> 5) & 0x1F;
            rgb[2] = (g_clut_fade_buf[i] >> 10) & 0x1F;
            if (rgb[0] != 0) {
                rgb[0]--;
            }
            if (rgb[1] != 0) {
                rgb[1]--;
            }
            if (rgb[2] != 0) {
                rgb[2]--;
            }
            g_clut_fade_buf[i] = rgb[0];
            g_clut_fade_buf[i] |= rgb[1] << 5;
            g_clut_fade_buf[i] |= rgb[2] << 10;
            if (g_clut_fade_buf[i] != 0) {
                g_clut_fade_busy = 0xFF;
            }
            g_clut_fade_buf[i] |= g_clut_fade_target[0];
        }
        LoadImage(&rect, (u32 *)g_clut_fade_buf);
        DrawSync(0);
    }
}
