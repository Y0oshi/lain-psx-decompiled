#include "common.h"
/* port: PsyQ types, macros and prototypes come from psx_sdk.h (port/psx headers). */

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
char *g_sjis_digit_strings[10]; /* port: defined here (host-typed) */
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

/* Create the save file on memory card slot 0 (1 block). Returns 0 on success. */
s32 mcard_create_save_file(void) {
    return -MemCardCreateFile(0, g_mcard_file_name, 1);
}

/* Format the memory card in slot 0. */
s32 mcard_format(void) {
    return -MemCardFormat(0);
}

/* Unformat the memory card in slot 0. */
s32 mcard_unformat(void) {
    return -MemCardUnformat(0);
}

void mcard_close_file(void) {
    MemCardClose();
}

/* Open the save file: write mode when nonzero, read mode otherwise. */
s32 mcard_open_save_file(s32 write) {
    s32 flag;

    flag = 2;
    if (!write) {
        flag = 1;
    }
    return -MemCardOpen(0, g_mcard_file_name, flag);
}

/* Start a card accept on slot 0 and block until it finishes. */
s32 mcard_accept_sync(void) {
    s32 cmds;
    s32 result;

    if (MemCardAccept(0) != 0) {
        while (MemCardSync(1, &cmds, &result) != 1) {
        }
        return -result;
    }
    return 0;
}

/* Start a card accept on slot 0 (non-blocking). */
s32 mcard_accept(void) {
    return MemCardAccept(0);
}

s32 mcard_read_save(u32 *buf, s32 bytes) {
    return MemCardReadData(buf, 0, bytes);
}

s32 mcard_write_save(u32 *buf, s32 bytes) {
    return MemCardWriteData(buf, 0, bytes);
}

/* Poll the pending card operation: 0 = still busy, 1 = done OK, <0 = error. */
s32 mcard_poll(void) {
    s32 cmds;
    s32 result;

    if (MemCardSync(1, &cmds, &result) == 1) {
        if (result == 0) {
            return 1;
        }
        return -result;
    }
    return 0;
}

/* Append a number (clamped to 9999) to buf as decimal text. When pad is
 * set, zero hundreds/tens digits are written as g_sjis_space. */
void sjis_append_number(char *buf, s32 num, s32 pad) {
    s32 digit;

    if (num >= 10000) {
        num = 9999;
    }
    digit = num / 1000;
    if (digit != 0) {
        strcat(buf, g_sjis_digit_strings[digit]);
    }
    digit = (num - digit * 1000) / 100;
    if (digit != 0) {
        strcat(buf, g_sjis_digit_strings[digit]);
    } else if (pad) {
        strcat(buf, g_sjis_space);
    }
    digit = (num % 100) / 10;
    if (digit != 0) {
        strcat(buf, g_sjis_digit_strings[digit]);
    } else if (pad) {
        strcat(buf, g_sjis_space);
    }
    digit = num % 10;
    strcat(buf, g_sjis_digit_strings[digit]);
}

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

/* Build the memory card save image in buf (inverse of save_apply_image): card
 * header with a title naming the save count plus a random stat, the game
 * state, and per-block XOR checksums. Returns the number of blocks used. */
extern int port_cheat_genome_title; /* port: game/port_libc.c */

s32 save_build_image(u8 *buf) {
    u8 *src;
    u8 *dst;
    u8 *p;
    s32 i;
    s32 j;
    u8 sum;
    u8 *sums;
    s32 count;

    memset(buf, 0, 0x2000);
    *(SaveIcon *)buf = g_mcard_header_template;
    /* port: the "Genome save title" cheat always takes the 1-in-70 title; rand()
     * is still called first, so the game's random sequence is unchanged. */
    if (rand() % 70 != 0 && !port_cheat_genome_title) {
        *(SaveTitle *)(buf + 4) = g_save_title;
    } else {
        *(SaveTitle *)(buf + 4) = g_save_title_rare;
    }
    sjis_append_number((char *)buf + 4, g_save_count + 1, 0);
    strcat((char *)buf + 4, g_save_title_count_sep);
    switch (rand() % 4) {
        case 0:
            strcat((char *)buf + 4, g_save_title_tokimeki);
            sjis_append_number((char *)buf + 4, g_stat_tokimeki, 1);
            strcat((char *)buf + 4, g_save_title_percent);
            break;
        case 1:
            if (g_screensaver_count != 0) {
                strcat((char *)buf + 4, g_save_title_denpa);
                sjis_append_number((char *)buf + 4, g_screensaver_count, 1);
                strcat((char *)buf + 4, g_save_title_denpa_unit);
                break;
            }
            /* fallthrough */
        case 2:
            strcat((char *)buf + 4, g_save_title_harumage);
            sjis_append_number((char *)buf + 4, g_stat_harumage, 1);
            strcat((char *)buf + 4, g_save_title_percent);
            break;
        case 3:
            strcat((char *)buf + 4, g_save_title_gakkuri);
            sjis_append_number((char *)buf + 4, g_stat_gakkuri, 1);
            strcat((char *)buf + 4, g_save_title_percent);
            break;
    }
    *(s32 *)(buf + 0x200) = 0x4C41494E;
    memcpy(buf + 0x204, &g_player_name, sizeof(SaveName));
    *(u32 *)(buf + 0x218) = g_save_count;
    *(s16 *)(buf + 0x21C) = g_current_site;
    *(s16 *)(buf + 0x21E) = g_gate_level;
    *(s16 *)(buf + 0x220) = g_polytan_parts;
    *(s16 *)(buf + 0x222) = g_sskn_level;
    count = 4;
    *(s16 *)(buf + 0x224) = g_media_played_count;
    *(s16 *)(buf + 0x226) = g_site_cursor_col;
    *(s16 *)(buf + 0x228) = g_site_cursor_row;
    *(s16 *)(buf + 0x22A) = g_other_site_cursor_col;
    *(s16 *)(buf + 0x22C) = g_other_site_cursor_row;
    dst = buf + 0x22E;
    for (i = 0; i < 0x2CC; i++) {
        src = (u8 *)&g_node_table[i];
        src += 0x22;
        dst[0] = src[0];
        dst[1] = src[1];
        dst[2] = src[2];
        dst[3] = src[3];
        dst += 4;
    }
    count += 23;
    for (i = 0; i < count; i++) {
        p = (u8 *)(i * 0x80 + (uintptr_t)buf); /* int add: operand order matters; port: uintptr_t, not s32 */
        sum = 0;
        for (j = 0; j < 0x80; j++) {
            sum ^= *p++;
        }
        (count * 0x80 + buf)[i] = sum;
    }
    return count + 1;
}


/* Validate a loaded save image (27 XOR-checksummed 128-byte blocks, checksums
 * at 0xD80, magic "NIAL" at 0x200) and unpack it into the game state.
 * Returns 1 on success, 0 if the data is corrupt. */
s32 save_apply_image(u8 *buf) {
    u8 *src;
    s32 i;
    s32 j;
    s32 sum;
    u8 *p;
    u8 *data;
    u8 *dst;
    u8 expected;
    u8 *sums;
    s32 count;

    count = 27;
    for (i = 0, sums = buf + 0xD80, data = buf; i < count; i++) {
        p = data;
        expected = sums[i];
        for (sum = 0, j = 0; j < 0x80; j++) {
            sum ^= *p++;
        }
        if (expected != (u8)sum) {
            return 0;
        }
        data += 0x80;
    }
    if (*(s32 *)(buf + 0x200) != 0x4C41494E) {
        return 0;
    }
    g_player_name = *(SaveName *)(buf + 0x204);
    g_save_count = *(u32 *)(buf + 0x218);
    g_current_site = *(s16 *)(buf + 0x21C);
    g_gate_level = *(s16 *)(buf + 0x21E);
    g_polytan_parts = *(s16 *)(buf + 0x220);
    g_sskn_level = *(s16 *)(buf + 0x222);
    g_media_played_count = *(s16 *)(buf + 0x224);
    g_site_cursor_col = *(s16 *)(buf + 0x226);
    g_site_cursor_row = *(s16 *)(buf + 0x228);
    g_other_site_cursor_col = *(s16 *)(buf + 0x22A);
    g_other_site_cursor_row = *(s16 *)(buf + 0x22C);
    src = buf + 0x22E;
    for (i = 0; i < 0x2CC; i++) {
        dst = (u8 *)&g_node_table[i];
        dst += 0x22;
        dst[0] = src[0];
        dst[1] = src[1];
        dst[2] = src[2];
        dst[3] = src[3];
        src += 4;
    }
    if (g_save_count < 9999) {
        g_save_count++;
    }
    g_text_window_show_options = 0;
    return 1;
}

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

INCLUDE_RODATA("asm/nonmatchings/game/8003C084", g_mcard_file_name);
INCLUDE_RODATA("asm/nonmatchings/game/8003C084", g_save_title);
INCLUDE_RODATA("asm/nonmatchings/game/8003C084", g_save_title_rare);
INCLUDE_RODATA("asm/nonmatchings/game/8003C084", g_save_title_tokimeki);
INCLUDE_RODATA("asm/nonmatchings/game/8003C084", g_save_title_harumage);
INCLUDE_RODATA("asm/nonmatchings/game/8003C084", g_save_title_gakkuri);
