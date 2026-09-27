#include "common.h"
/* port: PsyQ types, macros and prototypes come from psx_sdk.h (port/psx headers). */

/* Declarations carried over from 8003CB08.c (same original headers). */

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

MenuObject g_disc_change_text[24]; /* port: defined here (host-typed: embeds a host POLY_FT4) */
extern U16Pair g_disc_change_text_pos[24];
extern GsSPRITE g_disc_change_sprites[10];
extern void sprite_draw(MenuObject *obj, GsOT *ot, s32 arg2);



typedef struct {
    s32 pos;
    s32 size;
} Pair;

extern Pair g_disc_file_table[100];
extern Pair g_disc1_file_table[100];
extern Pair g_disc2_file_table[100];


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
MenuObject g_site_prompt_ring; /* port: defined here (host-typed) */
extern void sprite_draw_rotated(MenuObject *obj, GsOT *ot, s32 pri);


#ifdef NON_MATCHING
/* 2 diffs (also needs .rodata for its jump table): the hoisted loads of
 * &g_ot_2d and &g_site_prompt_sprites before the loop come out in swapped order
 * (loop.c hoists in order of first use; the y store uses g_site_prompt_sprites first). */
/* "New game / Continue" style menu: animate the cursor and decorations
 * until the confirm input (g_pad_command == 17). Returns 3 or 0 for the
 * selected option. */
s32 site_change_prompt_run(void) {
    RECT rect;
    s32 rotation;
    s32 choice;
    s32 frame;
    s32 phase;
    s32 flash;
    s32 i;
    s32 next;
    s32 result;

    tim_load_file_at(0x1F, 0x300, 0x198, 0, 0x1E8);
    rect.x = 0;
    rect.y = 0;
    rect.w = 320;
    rect.h = 480;
    ClearImage(&rect, 0, 0, 0);
    rotation = 0;
    choice = rotation;
    frame = rotation;
    phase = rotation;
    flash = rotation;
    g_site_prompt_ring.attribute = 0;
    g_site_prompt_ring.tpage = 0x1C;
    g_site_prompt_ring.clut = 0x7A02;
    g_site_prompt_ring.scaleX = 0x1000;
    g_site_prompt_ring.scaleY = 0x1000;
    g_site_prompt_ring.x = 0x98;
    g_site_prompt_ring.y = 0x70;
    g_site_prompt_ring.w = 0x10;
    g_site_prompt_ring.h = 0x10;
    g_site_prompt_ring.rotX = 0;
    g_site_prompt_ring.rotY = 0;
    g_site_prompt_ring.rotZ = 0;
    g_site_prompt_ring.u = 0xA0;
    g_site_prompt_ring.v = 0xA8;
    g_site_prompt_ring.r = 0x80;
    g_site_prompt_ring.g = 0x80;
    g_site_prompt_ring.b = 0x80;
    for (;;) {
        DrawSync(0);
        gfx_frame_begin();
        pad_read_command();
        switch (g_pad_command) {
            case 1:
            case 2:
            case 3:
                snd_play_sfx(1);
                choice = 0;
                break;
            case 6:
            case 7:
            case 8:
                snd_play_sfx(1);
                choice = 1;
                break;
            case 17:
                snd_play_sfx(0);
                if (!choice) {
                    result = 0;
                } else {
                    result = 3;
                }
                goto done;
        }
        if (!choice) {
            g_site_prompt_sprites[frame].y = 0x58;
        } else {
            g_site_prompt_sprites[frame].y = 0x98;
        }
        GsSortFastSprite(&g_site_prompt_sprites[frame], &g_ot_2d[g_frame_buffer_index], 4);
        if (flash != 0) {
            if (flash < 17) {
                g_site_prompt_sprites[10].b = g_site_prompt_sprites[10].g = g_site_prompt_sprites[10].r = -0x80 - (16 - flash) * 8;
            } else if (flash >= 164) {
                g_site_prompt_sprites[10].b = g_site_prompt_sprites[10].g = g_site_prompt_sprites[10].r = (180 - flash) * 8;
            }
            GsSortFastSprite(&g_site_prompt_flash_sprite, &g_ot_2d[g_frame_buffer_index], 5);
            flash--;
        } else if (rand() % 500 == 0) {
            flash = 180;
        }
        for (i = 9; i >= 8; i--) {
            GsSortFastSprite(&g_site_prompt_sprites[i], &g_ot_2d[g_frame_buffer_index], 5);
        }
        next = phase + 1;
        if (next >= 4) {
            next = 0;
        }
        g_site_prompt_sprites[phase + 11].x = 0;
        g_site_prompt_sprites[next + 11].x = 0xA0;
        GsSortFastSprite(&g_site_prompt_strip_sprites[phase], &g_ot_2d[g_frame_buffer_index], 6);
        GsSortFastSprite(&g_site_prompt_strip_sprites[next], &g_ot_2d[g_frame_buffer_index], 6);
        g_site_prompt_ring.rotZ = rotation;
        sprite_draw_rotated(&g_site_prompt_ring, &g_ot_2d[g_frame_buffer_index], 4);
        VSync(0);
        VSync(0);
        ResetGraph(1);
        GsSwapDispBuff();
        rotation += 0x10;
        GsSortClear(0, 0, 0, &g_ot_2d[g_frame_buffer_index]);
        GsDrawOt(&g_ot_2d[g_frame_buffer_index]);
        if (rotation >= 0x1000) {
            rotation = 0;
        }
        if ((rotation & 0x1F) == 0) {
            frame++;
            if (frame >= 8) {
                frame = 0;
            }
        }
        phase++;
        if (phase >= 4) {
            phase = 0;
        }
    }
done:
    rect.x = 0;
    rect.y = 0;
    rect.w = 320;
    rect.h = 480;
    ClearImage(&rect, 0, 0, 0);
    return result;
}
#else
INCLUDE_ASM("asm/nonmatchings/game/8003D1B4", site_change_prompt_run);
#endif
