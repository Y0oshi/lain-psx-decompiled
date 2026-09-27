#include "common.h"

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

/* Create the save file on memory card slot 0 (1 block). Returns 0 on success. */
s32 mcard_create_save_file(void) {
    return -MemCardCreateFile(0, g_mcard_file_name, 1);
}

s32 mcard_format(void) {
    return -MemCardFormat(0);
}

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

/* Build the memory card save image in buf (inverse of save_apply_image): card
 * header with a title naming the save count plus a random stat, the game
 * state, and per-block XOR checksums. Returns the number of blocks used. */
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
    if (rand() % 70 != 0) {
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
        p = (u8 *)(i * 0x80 + (s32)buf); /* int add: operand order matters */
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

INCLUDE_RODATA("asm/nonmatchings/game/8003C084", g_mcard_file_name);
INCLUDE_RODATA("asm/nonmatchings/game/8003C084", g_save_title);
INCLUDE_RODATA("asm/nonmatchings/game/8003C084", g_save_title_rare);
INCLUDE_RODATA("asm/nonmatchings/game/8003C084", g_save_title_tokimeki);
INCLUDE_RODATA("asm/nonmatchings/game/8003C084", g_save_title_harumage);
INCLUDE_RODATA("asm/nonmatchings/game/8003C084", g_save_title_gakkuri);
