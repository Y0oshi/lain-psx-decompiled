#include "common.h"
/* port: PsyQ types, macros and prototypes come from psx_sdk.h (port/psx headers). */

/* Declarations carried over from 8003C084.c (same original headers). */

/*
 * Small globals that this TU addresses absolutely (lui + %lo, via the
 * assembler's macro expansion) even though cc1 treats them as small data.
 * A non-small-data section attribute keeps cc1 from emitting the `.extern
 * sym,size` hint, so maspsx doesn't turn them into $gp accesses; cc1 still
 * emits the unsplit `lw $2,sym` macro form the original assembler expanded.
 */
#define ABS 

/* PsyQ libmcrd */


/* Save file name, "BISLPS-01603LAIN..." (in .rodata) */
extern char g_mcard_file_name[];
/* Digit strings "0".."9" */
extern char *g_sjis_digit_strings[];
extern char g_sjis_space[];

/* PsyQ libgpu / libgs types */





typedef struct {
    s32 sector;
    s32 size;
} FileEntry;

extern s32 cd_load_archive_entry(s32, s32, FileEntry *, void *); /* port: exact prototype (s16 return and s32 dest in the matching source; slot ids fit s16) */

extern FileEntry g_bin_file_table[];
extern s16 g_gfx_load_entry;
extern s16 g_gfx_load_req;
extern void *g_gfx_load_buf; /* port: void * (s32 on the PS1); malloc'd load buffer */
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
    POLY_FT4 poly; /* port: host POLY_FT4 (0x28 bytes on the PS1); the fields below follow it */
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



/* Draw one frame of a menu: 24 text/box objects plus up to 10 sprites whose
 * visibility depends on mode, the cursor and the current selection state. */
void disc_change_draw_screen(s32 mode, s32 cursor, s32 state) {
    MenuObject *obj;
    s32 i;
    s32 visible;
    U16Pair *uv;
    s32 shade;

    i = 0;
    shade = 0x80;
    obj = g_disc_change_text;
    uv = g_disc_change_text_pos;
    /* A goto loop: a for/while loop gets its constants hoisted by loop.c. */
loop:
    if (i & 1) {
        obj->attribute = 0x40800000;
    } else {
        obj->attribute = 0x40000000;
    }
    obj->tpage = 0x5C;
    obj->clut = 0x79C0;
    obj->x = uv->x;
    obj->y = uv->y;
    if (i < 20) {
        obj->u = 0;
        obj->v = 0x50;
    } else {
        obj->u = 0;
        obj->v = 0x90;
    }
    obj->w = 0x90;
    obj->h = 8;
    obj->r = shade;
    obj->g = shade;
    obj->b = shade;
    obj++;
    uv++;
    i++;
    if (i < 24) {
        goto loop;
    }
    DrawSync(0);
    gfx_frame_begin();
    for (i = 0; i < 10; i++) {
        visible = 1;
        switch (i) {
            case 0:
                if (mode != 0) {
                    visible = 0;
                }
                break;
            case 1:
                if (mode == 0) {
                    visible = 0;
                }
                break;
            case 2:
                if (state == 1) {
                    visible = 0;
                }
                break;
            case 4:
                if (g_current_site != 0) {
                    visible = 0;
                }
                break;
            case 5:
                if (g_current_site == 0) {
                    visible = 0;
                }
                break;
            case 6:
                visible = 0;
                if (cursor != 0) {
                    visible = (state == -1);
                }
                break;
            case 7:
                if (state != 1) {
                    visible = 0;
                }
                break;
            case 8:
                visible = 0;
                if (cursor != 0) {
                    visible = (state == -2);
                }
                break;
        }
        if (visible) {
            GsSortFastSprite(&g_disc_change_sprites[i], &g_ot_2d[g_frame_buffer_index], 1);
        }
    }
    for (i = 0; i < 24; i++) {
        sprite_draw(&g_disc_change_text[i], &g_ot_2d[g_frame_buffer_index], 1);
    }
    VSync(0);
    ResetGraph(1);
    GsSwapDispBuff();
    GsSortClear(0xFF, 0xFF, 0xFF, &g_ot_2d[g_frame_buffer_index]);
    GsDrawOt(&g_ot_2d[g_frame_buffer_index]);
}


typedef struct {
    s32 pos;
    s32 size;
} Pair;

extern Pair g_disc_file_table[100];
extern Pair g_disc1_file_table[100];
extern Pair g_disc2_file_table[100];

/* Run a menu until cd_detect_disc agrees with the current disc/mode
 * (g_current_site), then load the 100-entry table matching that mode. */
void disc_change_request(s32 arg0) {
    s32 selected;
    s32 prev;
    s32 result; /* also reused as the copy index */

    selected = 0;
    if (cd_detect_disc() != g_current_site) {
        prev = selected;
        for (;;) {
            do {
                disc_change_draw_screen(arg0, selected, prev);
                result = cd_lid_watch();
            } while (result == 0);
            if (result == 1) {
                do {
                    disc_change_draw_screen(arg0, selected, result);
                    result = cd_lid_watch();
                } while (result == 1);
            }
            if (result == 2) {
                if (cd_detect_disc() == g_current_site) {
                    break;
                }
                result = -1;
            }
            selected = 1;
            prev = result;
        }
    }
    if (g_current_site == 0) {
        for (result = 0; result < 100; result++) {
            g_disc_file_table[result].pos = g_disc1_file_table[result].pos;
            g_disc_file_table[result].size = g_disc1_file_table[result].size;
        }
    } else {
        for (result = 0; result < 100; result++) {
            g_disc_file_table[result].pos = g_disc2_file_table[result].pos;
            g_disc_file_table[result].size = g_disc2_file_table[result].size;
        }
    }
}


/* Debug: dump a VRAM rectangle as a TIM file on the PC host (via PCcreat).
 * mode is the TIM pixel mode (0 = 4bpp, 1 = 8bpp, 2 = 16bpp); 4/8bpp dumps
 * include a 256-entry CLUT read from (clutX, clutY). */
void vram_dump_tim(char *name, s32 mode, s32 x, s32 y, s32 width, s32 height, s32 clutX, s32 clutY) {
    RECT rect;
    s32 hasClut;
    s32 vramWidth;
    s32 imageSize;
    s32 size;
    s32 fd;
    u32 *buf;
    u8 *p;

    switch (mode) {
        case 0:
            if (width & 3) {
                width += 4 - width % 4;
            }
            vramWidth = (width + 3) / 4;
            hasClut = 1;
            break;
        case 1:
            if (width & 1) {
                width = (width / 2) * 2 + 2;
            }
            vramWidth = (width + 1) / 2;
            hasClut = 1;
            break;
        case 2:
            vramWidth = width;
            hasClut = 0;
            break;
        default:
            return;
    }
    imageSize = (vramWidth * height) << 1;
    size = 0x14; /* TIM header (8) + pixel block header (12) */
    size = hasClut * 0x20C + (imageSize + size);
    buf = heap_alloc(size);
    if (buf == NULL) {
        return;
    }
    p = (u8 *)(buf + 2);
    buf[0] = 0x10;
    buf[1] = (hasClut << 3) | mode;
    if (hasClut) {
        buf[2] = 0x20C;
        ((s16 *)buf)[6] = 0;
        ((s16 *)buf)[7] = 0;
        ((s16 *)buf)[8] = 16;
        ((s16 *)buf)[9] = 16;
        rect.x = clutX;
        rect.y = clutY;
        rect.w = 256;
        rect.h = 1;
        StoreImage2(&rect, buf + 5);
        DrawSync(0);
        p = (u8 *)(buf + 0x85);
    }
    *(s32 *)p = imageSize + 12;
    p += 4;
    *(s16 *)p = 0;
    p += 2;
    *(s16 *)p = 0;
    p += 2;
    *(s16 *)p = vramWidth;
    p += 2;
    *(s16 *)p = height;
    rect.x = x;
    rect.y = y;
    rect.w = vramWidth;
    rect.h = height;
    StoreImage2(&rect, (u32 *)(p + 2));
    DrawSync(0);
    fd = PCcreat(name, 0);
    p = (u8 *)buf;
    while (size != 0) {
        if (size >= 0x1000) {
            PCread(fd, (char *)p, 0x1000); /* port: libsn prototype */
            size -= 0x1000;
            p += 0x1000;
        } else {
            PCread(fd, (char *)p, size);
            size = 0;
        }
    }
    PCclose(fd); /* port: PCclose is libsn PCclose */
    heap_free(buf);
}

/* "press ANY button" (in .rodata) */
extern char g_str_press_any_button[];
extern GsSPRITE g_polytan_sprites[];
extern s16 g_pad_command ABS;
/* The "press ANY button" sprite (g_polytan_sprites[15]) */
extern GsSPRITE g_polytan_press_any;
extern GsSPRITE g_site_prompt_sprites[];
/* g_site_prompt_sprites[10] and g_site_prompt_sprites[11] */
extern GsSPRITE g_site_prompt_flash_sprite;
extern GsSPRITE g_site_prompt_strip_sprites[];
extern MenuObject g_site_prompt_ring;
extern void sprite_draw_rotated(MenuObject *obj, GsOT *ot, s32 pri);
