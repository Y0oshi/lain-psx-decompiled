#include "common.h"

/* "d:\\usr\\tmp\\psx.log" - a separate object in the original (see config: psx_log). */
extern char g_psx_log_path[];

typedef struct {
    u8 minute, second, sector, track;
} CdlLOC;

typedef struct {
    CdlLOC pos;
    u32 size;
    char name[16];
} CdlFILE;

/* One pending CD read request (24 slots at g_cd_slots). */
typedef struct {
    CdlFILE file;
    s32 done;
    s32 inUse;
    s32 id;
    s32 sector;
    s32 size;
    u8 *dest;
} FileSlot;

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

extern GsSPRITE g_jump_menu_sprites[];
extern s16 g_jump_menu_slide;
/* GCC treats these as small ($gp) data but the original assembler addressed
 * them absolutely; the second declaration's section attribute suppresses the
 * `.extern` size hint so maspsx does the same. */
extern s32 g_site_level;
extern s32 g_site_level __attribute__((section(".data")));

/* Boot and main loop */

void ResetCallback(void);
void debug_log_open(char *);
void gfx_init(void);
void FntLoad(s32 tx, s32 ty);
s32 FntOpen(s32 x, s32 y, s32 w, s32 h, s32 isbg, s32 n);
void SetDumpFnt(s32 id);
void cd_loader_init(void);
void lain_anim_init(void);
void mcard_start(void);
void pad_init(void);
void text_window_init(s32, s32);
void menu_load_texture(void);
void site_scene_init(void);
void VSyncCallback(void (*f)(void));
void loading_anim_load(void);
void site_init_new_game(void);
void node_reset_all(void);
void node_grid_build(void);
void snd_init(s32);
void lain_anim_load_site_enter(void);
void vsync_counter_tick(void);
void xa_update(void);
void snd_vsync_update(void);
void game_vsync_callback(void);
void SetMemSize(s32);
void debug_printf(char *fmt, ...);
s32 heap_get_free(void);
void start_menu_run(void);
s32 lain_anim_ready_site_enter(void);
void loading_anim_reload(void);
void disc_change_request(s32);
s32 site_run(void);
void site_reload(void);
void lain_anim_free_idle(void);
void SsSetMVol(s16 voll, s16 volr);
s32 gfx_bg_load_step(s32);

int main(void) {
    s32 mode;

    SetMemSize(2);
    ResetCallback();
    debug_log_open(g_psx_log_path);
    gfx_init();
    FntLoad(0x3C0, 0x100);
    SetDumpFnt(FntOpen(16, 16, 256, 200, 0, 512));
    cd_loader_init();
    lain_anim_init();
    snd_init(0);
    mcard_start();
    pad_init();
    text_window_init(0x44, 0x31);
    menu_load_texture();
    site_scene_init();
    site_init_new_game();
    node_grid_build();
    lain_anim_load_site_enter();
    VSyncCallback(game_vsync_callback);
    mode = 0;
    while (1) {
        debug_printf("1left memory %d\n", heap_get_free());
        start_menu_run();
        debug_printf("2left memory %d\n", heap_get_free());
        while (1) {
            while (lain_anim_ready_site_enter() == 0) {
            }
            loading_anim_reload();
            disc_change_request(mode);
            switch (site_run()) {
            case 1:
                mode = 0;
                site_reload();
                break;
            case 2:
                mode = 1;
                site_reload();
                break;
            case 3:
                mode = 0;
                site_reload();
                SsSetMVol(0, 0);
                loading_anim_load();
                while (gfx_bg_load_step(1) == 0) {
                }
                SsSetMVol(0x7F, 0x7F);
                text_window_init(0x44, 0x31);
                break;
            default:
                goto restart;
            }
            lain_anim_free_idle();
            lain_anim_load_site_enter();
        }
    restart:
        mode = 0;
        loading_anim_load();
        site_init_new_game();
        node_reset_all();
        node_grid_build();
        snd_init(1);
        lain_anim_load_site_enter();
    }
}

/* Unnamed in the symbol map (its code is part of main's asm): reloads the debug font. */
void gfx_reinit_debug_font(void) {
    gfx_init();
    FntLoad(0x3C0, 0x100);
    SetDumpFnt(FntOpen(16, 16, 256, 200, 0, 512));
}


void game_vsync_callback(void) {
    vsync_counter_tick();
    xa_update();
    snd_vsync_update();
}


/* Boot-time system initialisation; installs game_vsync_callback as the VSync callback. */
void game_boot_init(void) {
    ResetCallback();
    debug_log_open(g_psx_log_path);
    gfx_init();
    FntLoad(0x3C0, 0x100);
    SetDumpFnt(FntOpen(16, 16, 256, 200, 0, 512));
    cd_loader_init();
    lain_anim_init();
    snd_init(0);
    mcard_start();
    pad_init();
    text_window_init(0x44, 0x31);
    menu_load_texture();
    site_scene_init();
    site_init_new_game();
    node_grid_build();
    lain_anim_load_site_enter();
    VSyncCallback(game_vsync_callback);
}


void game_restart_reset(void) {
    loading_anim_load();
    site_init_new_game();
    node_reset_all();
    node_grid_build();
    snd_init(1);
    lain_anim_load_site_enter();
}

/* Entry in the current archive's table (see anim_open_archive). */
typedef struct {
    s32 offset; /* from the data start g_anim_data */
    s16 x;
    s16 y;
    s32 param;
} ArchiveEntry;

extern s32 g_anim_data;
extern ArchiveEntry *g_anim_entry;
extern s32 g_anim_vram_x;
extern s32 g_anim_vram_y;
extern volatile s32 g_vsync_counter; /* frame counter (VSync) */
extern s32 g_anim_decode_count;
extern s32 g_anim_decode_vblanks;
extern u8 g_anim_mask_buf[];

/* LZ-style decompressor: each flag byte (MSB first) selects either a literal
 * byte or a back-reference (distance - 1, length - 3). Output goes to
 * g_anim_mask_buf; input starts at an offset table in the current file buffer. */
void anim_decompress_mask(void) {
    s32 out;
    u8 *hdr;
    u8 *src;
    s32 remaining;
    s32 bit;
    u8 flags;
    u8 *buf;

    out = 0;
    hdr = (u8 *)(g_anim_data + g_anim_entry->offset) + 8;
    hdr = hdr + *(s32 *)hdr;
    src = hdr + 8;
    remaining = *(s32 *)(hdr + 4);
    buf = g_anim_mask_buf;

    while (remaining > 0) {
        flags = *src++;
        for (bit = 0; bit < 8; bit++) {
            if (remaining <= 0) {
                return;
            }
            if (flags & (0x80 >> bit)) {
                s32 back, len, i;
                back = *src++ + 1;
                len = *src++;
                len += 3;
                for (i = 0; i < len; i++) {
                    (buf + out)[i] = (buf + out - back)[i];
                }
                out += len;
                remaining -= len;
            } else {
                u8 *d = &buf[out];
                out++;
                *d = *src++;
                remaining--;
            }
        }
    }
}


/* AND mask and OR value per nibble. g_anim_mask_and[0] is the last word of .text
 * (splat puts it in the libpress asm); the other 15 words follow in .data. */
extern u32 g_anim_mask_and[];
extern u32 g_anim_mask_or[];

/* Applies a 4-bit mask image from g_anim_mask_buf onto `dst`: each nibble controls
 * one word (0 = clear, 0xF = keep, otherwise (word & mask[n]) | value[n]). */
void anim_apply_mask(u32 *dst, s32 x, s32 stride, s32 rows) {
    u8 *row;
    u8 *src;
    s32 y;
    s32 i;
    s32 nibble;
    s32 step;

    row = g_anim_mask_buf + x / 4;
    step = stride / 4;
    for (y = 0; y < rows; y++) {
        src = row;
        for (i = 4; i > 0; i--) {
            if (*src != 0) {
                if (*src != 0xFF) {
                    nibble = *src >> 4;
                    if (nibble != 0) {
                        if (nibble != 0xF) {
                            *dst &= g_anim_mask_and[nibble];
                            *dst |= g_anim_mask_or[nibble];
                        }
                    } else {
                        *dst = 0;
                    }
                    dst++;
                    nibble = *src & 0xF;
                    if (nibble != 0) {
                        if (nibble != 0xF) {
                            *dst &= g_anim_mask_and[nibble];
                            *dst |= g_anim_mask_or[nibble];
                        }
                    } else {
                        *dst = 0;
                    }
                    dst++;
                } else {
                    dst += 2;
                }
            } else {
                *dst++ = 0;
                *dst++ = 0;
            }
            src++;
        }
        row += step;
    }
}

typedef struct {
    s16 x, y, w, h;
} RECT;

typedef struct {
    s32 w;
    s32 h;
} Size32;

typedef struct {
    u16 a;
    u16 b;
} HalfPair;

typedef struct {
    s32 w;
    s32 h;
    s32 x;
    s32 y;
    s32 param;
} ImageInfo;

extern s16 g_anim_max_frame_size; /* largest single-frame data size seen */
extern s16 g_anim_stat_vlc_vblanks;
extern s16 g_anim_stat_mask_vblanks;
extern s16 g_anim_stat_mdec_vblanks;
extern s16 g_anim_stat_total_vblanks;
extern s16 g_anim_last_frame_size;
extern u16 g_vlc_qscale_chroma;
extern u16 g_vlc_qscale_chroma __attribute__((section(".data")));
extern u16 g_vlc_qscale_luma;
extern u16 g_vlc_qscale_luma __attribute__((section(".data")));
extern u32 g_mdec_rl_buf[];
extern u32 g_mdec_strip_buf[];
s32 ClearImage(RECT *rect, s32 r, s32 g, s32 b);
s32 LoadImage(RECT *rect, u32 *p);
s32 DrawSync(s32 mode);
void DecDCTin(u32 *buf, s32 mode);
void DecDCTout(u32 *buf, s32 size);
s32 DecDCToutSync(s32 mode);
s32 vlc_decode_frame(u32 *bs, u32 *buf);
void anim_apply_mask(u32 *dst, s32 x, s32 stride, s32 rows);

#ifdef NON_MATCHING
/* 10 diffs, all in the scheduling of the statistics block at the end.
 * g_vsync_counter must be volatile to keep `x = 0` before DecDCTin. The
 * original loads size.w before the g_vsync_counter read and orders the loads
 * of g_anim_entry/g_anim_decode_count after the info->h store. That looks as if
 * `size` were non-struct stack scalars, which conflict differently with the stores
 * through `info`. Tried: all orders of the last 5-6 statements, and info as an s32 array
 * (11-15 diffs). decomp-permuter (50 min) lowers its own score with `h = size.w;
 * info->w = h;` (reusing h), but that is 13 by check.sh. */
/* Decodes the current archive entry (an MDEC-compressed picture) to VRAM in
 * 16-pixel strips and records timing statistics. Returns 1 on success. */
s32 anim_decode_frame(ImageInfo *info) {
    RECT rect;
    Size32 size;
    HalfPair hdr;
    u8 *entry;
    u8 *bs;
    u32 dataSize;
    s32 t0, t1, t2;
    s32 x;
    s32 h;
    s32 words;
    s16 *e0;
    u16 *e1;

    e0 = (s16 *)(g_anim_data + g_anim_entry->offset);
    size.w = e0[0];
    size.h = h = e0[1];
    info->x = g_anim_entry->x;
    info->y = g_anim_entry->y;
    entry = (u8 *)(g_anim_data + g_anim_entry->offset);
    dataSize = *(u32 *)(entry + 0xC);
    bs = entry + 0xC;
    if (dataSize < 0x40000 && h < 360) {
        rect.x = 320;
        rect.y = 0;
        rect.w = 320;
        rect.h = size.h;
        ClearImage(&rect, 0, 0, 0);
        t0 = g_vsync_counter;
        e1 = (u16 *)(g_anim_data + g_anim_entry->offset);
        hdr.a = e1[2];
        g_vlc_qscale_luma = hdr.a;
        hdr.b = e1[3];
        g_vlc_qscale_chroma = hdr.b;
        vlc_decode_frame((u32 *)bs, g_mdec_rl_buf);
        t1 = g_vsync_counter;
        x = 0;
        DecDCTin(g_mdec_rl_buf, 0);
        rect.w = 16;
        rect.y = g_anim_vram_y;
        rect.h = size.h;
        words = size.h * 8;
        anim_decompress_mask();
        t2 = g_vsync_counter;
        DrawSync(0);
        for (; x < size.w; x += 16) {
            DecDCTout(g_mdec_strip_buf, words);
            DecDCToutSync(0);
            anim_apply_mask(g_mdec_strip_buf, x, size.w, size.h);
            rect.x = g_anim_vram_x + x;
            DrawSync(0);
            LoadImage(&rect, g_mdec_strip_buf);
        }
        DrawSync(0);
        t0 = t1 - t0;
        t1 = t2 - t1;
        t2 = g_vsync_counter - t2;
        g_anim_decode_vblanks += t0 + t1 + t2;
        g_anim_decode_count++;
        info->h = size.h;
        info->w = size.w;
        info->param = g_anim_entry->param;
        if (t0 == 3 && g_anim_max_frame_size < dataSize) {
            g_anim_max_frame_size = dataSize;
        }
        g_anim_last_frame_size = dataSize;
        g_anim_stat_vlc_vblanks = t0;
        g_anim_stat_mask_vblanks = t1;
        g_anim_stat_mdec_vblanks = t2;
        g_anim_stat_total_vblanks = g_anim_decode_vblanks;
        return 1;
    }
    rect.x = 320;
    rect.w = 320;
    rect.y = 0;
    rect.h = 480;
    return ClearImage(&rect, 255, 255, 255);
}
#else
INCLUDE_ASM("asm/nonmatchings/game/80013138", anim_decode_frame);
#endif

extern u8 g_anim_vlc_table[];
void DecDCTReset(s32 mode);
void vlc_build_table(void *);

void anim_decoder_init(s32 arg0, s32 arg1) {
    g_anim_vram_x = arg0;
    g_anim_vram_y = arg1;
    DecDCTReset(0);
    vlc_build_table(g_anim_vlc_table);
    g_vsync_counter = 0;
    g_anim_decode_count = 0;
    g_anim_decode_vblanks = 0;
}

extern s32 g_anim_frame_index;
extern s32 g_anim_frame_count;

/* Opens an archive: count at +8, then a table of 12-byte entries, then data. */
s32 anim_open_archive(u8 *archive) {
    s32 count;

    archive += 8;
    g_anim_frame_index = 0;
    count = *(s32 *)archive;
    archive += 4;
    g_anim_entry = (ArchiveEntry *)archive;
    g_anim_frame_count = count;
    g_anim_data = (s32)(archive + count * 12);
    return 1;
}

/* Advances to the next archive entry; returns 0 when at the last one. */
s32 anim_next_frame(void) {
    if (g_anim_frame_index < g_anim_frame_count - 1) {
        g_anim_frame_index++;
        g_anim_entry++;
        return 1;
    }
    return 0;
}

void vsync_counter_tick(void) {
    g_vsync_counter++;
}

/* CD file loader */

extern FileSlot g_cd_slots[];
extern volatile s32 g_cd_read_state; /* 0 = idle, 1-4 = cd_sync_callback step */
extern volatile s32 g_cd_queue_len; /* next request id */
extern s32 g_cd_start_sector; /* current sector */
extern s32 g_cd_last_sector; /* last sector read */
extern s32 g_cd_cur_slot;
extern u8 *g_cd_read_dest;
extern volatile s32 g_cd_read_remaining;
s32 CdInit(void);
s32 CdIntToPos(s32 i, CdlLOC *p);
s32 CdControlF(u8 com, u8 *param);
CdlFILE *CdSearchFile(CdlFILE *fp, char *name);
s32 CdPosToInt(CdlLOC *p);
void *CdSyncCallback(void (*func)());
void cd_sync_callback(s32 status, u8 *result);
void EnterCriticalSection(void);
void ExitCriticalSection(void);

/* The original inlined these two helpers into the request functions; these
 * static inline copies are used only for inlining, the real definitions
 * follow at their original address. */
static inline void CdPurgeInline(void) {
    CdlLOC loc;
    s32 sector;

    sector = g_cd_slots[g_cd_cur_slot].sector;
    g_cd_last_sector = 0;
    g_cd_start_sector = sector;
    CdIntToPos(sector, &loc);
    CdControlF(2, (u8 *)&loc);
    g_cd_read_remaining = g_cd_slots[g_cd_cur_slot].size;
    g_cd_read_dest = g_cd_slots[g_cd_cur_slot].dest;
}

static inline s32 CdSearchInline(char *name, s32 *size) {
    CdlFILE file;

    if (CdSearchFile(&file, name) != NULL) {
        *size = file.size;
        return CdPosToInt(&file.pos);
    }
    return -1;
}

extern volatile s32 g_cd_error_count;
extern volatile s32 D_800A5EF8;
extern volatile s32 g_cd_sectors_received;
void cd_ready_callback(s32 status);

/* Notes on matching:
 * - D_800A5EF8/g_cd_sectors_received volatile. Case 3 stays in source order, so cases 1 and 2
 *   cross-jump at the CdControlF call.
 * - The seek code in its own inline helper, so &loc is not CSEd into s1 and the frame is 0x28.
 * - The helper takes the new state as a parameter and g_cd_error_count is volatile; with a
 *   constant `g_cd_read_state = 1` at the call site sched1 hoisted `li 1` and reload_cse
 *   turned `sll 1` into `sllv`.
 */
/* Seek back to the current request's first sector and enter `state`. Inlined into the
 * status-5 and data-ended-early paths, which share its tail via cross-jumping. */
static inline void CdSeekInline(s32 state) {
    CdlLOC loc;
    s32 sector;

    sector = g_cd_slots[g_cd_cur_slot].sector;
    g_cd_read_state = state;
    g_cd_last_sector = 0;
    g_cd_start_sector = sector;
    CdIntToPos(sector, &loc);
    CdControlF(2, (u8 *)&loc);
    g_cd_read_remaining = g_cd_slots[g_cd_cur_slot].size;
    g_cd_read_dest = g_cd_slots[g_cd_cur_slot].dest;
}

/* CdSyncCallback handler: the read state machine. g_cd_read_state steps through
 * 1 (seeking) -> 2 (set mode) -> 3 (reading) -> 4 (waiting for data), then
 * moves on to the oldest pending request or goes idle. */
void cd_sync_callback(s32 status, u8 *result) {
    u8 param[8];
    s32 i;
    s32 next;

    if (status == 5) {
        if (g_cd_read_state == 4) {
            CdControlF(9, NULL);
            g_cd_error_count++;
            return;
        }
        /* disk error: seek to the current request again */
        g_cd_error_count++;
        CdSeekInline(1);
    } else if (status == 2) {
        switch (g_cd_read_state) {
        case 1:
            param[0] = 0xA0;
            CdControlF(0xE, param);
            g_cd_read_state++;
            break;
        case 2:
            CdReadyCallback(cd_ready_callback);
            CdControlF(6, NULL);
            g_cd_read_state++;
            break;
        case 3:
            g_cd_read_state++;
            D_800A5EF8 = 0;
            g_cd_sectors_received = 0;
            break;
        case 4:
            if (g_cd_read_remaining == 0) {
                g_cd_queue_len--;
                g_cd_slots[g_cd_cur_slot].done = 1;
                if (g_cd_queue_len != 0) {
                    for (i = 0; i < 24; i++) {
                        if (g_cd_slots[i].inUse != 0 && g_cd_slots[i].done == 0) {
                            if (--g_cd_slots[i].id == 0) {
                                next = i;
                            }
                        }
                    }
                    g_cd_cur_slot = next;
                    CdPurgeInline();
                    g_cd_read_state = 2;
                } else {
                    g_cd_read_state = 0;
                    CdSyncCallback(NULL);
                }
            } else {
                /* data ended early: read the request again */
                CdSeekInline(2);
            }
            break;
        }
    }
}

typedef struct {
    s32 sector;
    s32 size;
} FileEntry;

extern FileEntry g_disc_file_table[]; /* sector/size table of the files on disc */

/* Queues a read of disc file `fileId` (from the table) into `dest`; returns the slot or -1. */
s32 cd_load_file_by_id(s32 fileId, u8 *dest) {
    s32 slot;
    FileSlot *req;

    for (slot = 0; slot < 24; slot++) {
        if (g_cd_slots[slot].inUse == 0) {
            break;
        }
    }
    if (slot == 24) {
        return -1;
    }
    EnterCriticalSection();
    req = &g_cd_slots[slot];
    req->sector = g_disc_file_table[fileId].sector;
    req->size = g_disc_file_table[fileId].size;
    req->dest = dest;
    req->id = g_cd_queue_len;
    g_cd_queue_len++;
    req->inUse = 1;
    if (g_cd_read_state == 0) {
        CdSyncCallback(cd_sync_callback);
        g_cd_read_state = 1;
        g_cd_cur_slot = slot;
        CdPurgeInline();
    }
    ExitCriticalSection();
    return slot;
}

/* Queues a read of sub-file `entry` of archive `fileId` (offsets from `table`) into `dest`. */
s32 cd_load_archive_entry(s32 fileId, s32 entry, FileEntry *table, u8 *dest) {
    s32 slot;
    FileSlot *req;

    for (slot = 0; slot < 24; slot++) {
        if (g_cd_slots[slot].inUse == 0) {
            break;
        }
    }
    if (slot == 24) {
        return -1;
    }
    EnterCriticalSection();
    req = &g_cd_slots[slot];
    req->sector = g_disc_file_table[fileId].sector + table[entry].sector;
    req->size = table[entry].size;
    req->dest = dest;
    req->id = g_cd_queue_len;
    g_cd_queue_len++;
    req->inUse = 1;
    if (g_cd_read_state == 0) {
        g_cd_read_state = 1;
        g_cd_cur_slot = slot;
        CdSyncCallback(cd_sync_callback);
        CdPurgeInline();
    }
    ExitCriticalSection();
    return slot;
}

/* Queues a read of the named disc file into `dest`; returns the slot or -1. */
s32 cd_load_file_by_name(char *name, u8 *dest) {
    s32 slot;
    s32 sector;
    s32 size;
    FileSlot *req;

    for (slot = 0; slot < 24; slot++) {
        if (g_cd_slots[slot].inUse == 0) {
            break;
        }
    }
    if (slot == 24) {
        return -1;
    }
    sector = CdSearchInline(name, &size);
    if (sector < 0) {
        return -1;
    }
    req = &g_cd_slots[slot];
    CdIntToPos(sector, &req->file.pos);
    req->id = g_cd_queue_len;
    g_cd_queue_len++;
    req->inUse = 1;
    req->dest = dest;
    req->size = size;
    if (g_cd_read_state == 0) {
        CdSyncCallback(cd_sync_callback);
        g_cd_read_state = 1;
        g_cd_cur_slot = slot;
        CdPurgeInline();
    }
    return slot;
}


void cd_loader_init(void) {
    s32 i;

    for (i = 0; i < 24; i++) {
        g_cd_slots[i].inUse = 0;
        g_cd_slots[i].done = 0;
    }
    CdInit();
}

extern volatile s32 g_cd_sectors_received;
s32 CdGetSector(void *madr, s32 size);
void *CdReadyCallback(void (*func)());

/* CdReadyCallback handler: copies each arriving sector to the destination
 * buffer and stops reading when done or when a sector arrives out of order. */
void cd_ready_callback(s32 status) {
    u32 header[3];
    s32 sector;
    s32 error;

    g_cd_sectors_received++;
    error = 0;
    if (status == 1) {
        CdGetSector(header, 3);
        sector = CdPosToInt((CdlLOC *)header);
        if (g_cd_last_sector == 0) {
            error = g_cd_start_sector != sector;
        } else if (sector != g_cd_last_sector + 1) {
            error = 1;
        }
        g_cd_last_sector = sector;
        if (g_cd_read_remaining >= 0x800) {
            CdGetSector(g_cd_read_dest, 0x200);
            g_cd_read_dest += 0x800;
            g_cd_read_remaining -= 0x800;
        } else {
            CdGetSector(g_cd_read_dest, (g_cd_read_remaining + 3) / 4);
            g_cd_read_remaining = 0;
        }
        if (g_cd_read_remaining == 0) {
            CdReadyCallback(NULL);
            CdControlF(9, NULL);
        }
    } else {
        error = 1;
    }
    if (error) {
        g_cd_read_remaining = -1;
        CdReadyCallback(NULL);
        CdControlF(9, NULL);
    }
}


/* Starts reading the current file slot: seeks to its sector and latches its params. */
void cd_seek_current_slot(void) {
    CdlLOC loc;
    s32 sector;

    sector = g_cd_slots[g_cd_cur_slot].sector;
    g_cd_last_sector = 0;
    g_cd_start_sector = sector;
    CdIntToPos(sector, &loc);
    CdControlF(2, (u8 *)&loc);
    g_cd_read_remaining = g_cd_slots[g_cd_cur_slot].size;
    g_cd_read_dest = g_cd_slots[g_cd_cur_slot].dest;
}



/* Looks a file up on the CD; stores its size and returns its sector (-1 if missing). */
s32 cd_find_file(char *name, s32 *size) {
    CdlFILE file;

    if (CdSearchFile(&file, name) != NULL) {
        *size = file.size;
        return CdPosToInt(&file.pos);
    }
    return -1;
}

s32 cd_poll_load(s32 index) {
    if (g_cd_slots[index].inUse != 0) {
        if (g_cd_slots[index].done != 0) {
            g_cd_slots[index].inUse = 0;
            g_cd_slots[index].done = 0;
            return 1;
        }
        return 0;
    }
    return -1;
}

typedef struct {
    s16 id;     /* index into g_node_table, or -1 when empty */
    s16 flags;  /* +8 once the node has been viewed */
    u8 pad4[4];
    s16 x;
    s16 y;
} MapCell; /* 12 bytes, 24 per row */

typedef struct {
    char name[8];
    s16 lines[4];
    s16 text[3];    /* 0x10: indices into g_node_text_strings, <0 = none */
    s16 params[3];  /* 0x16: passed to the media player */
    s16 media;      /* 0x1C: media id for media_play */
    s16 pad1E;
    u32 flags;      /* 0x20: bits 20-23 = node type, bit 27 = skippable */
    u8 pad24[4];
} InfoEntry; /* 0x28 bytes */

extern MapCell g_site_grid[][24];
extern InfoEntry g_node_table[];
extern char *g_node_label_strings[];
extern char g_text_window_next_name[];
extern char g_text_window_labels[][18];
extern s16 g_site_cursor_on_node;
char *strcpy(char *dst, const char *src);
char *strncpy(char *dst, const char *src, u32 n);

/* Fills the info-panel text (name + 4 lines) for map cell (row, col). */
void site_set_node_info(s32 row, s32 col) {
    s32 i;

    if (g_site_grid[row][col].id >= 0) {
        strncpy(g_text_window_next_name, g_node_table[g_site_grid[row][col].id].name, 6);
        g_text_window_next_name[6] = 0;
        for (i = 0; i < 4; i++) {
            strncpy(g_text_window_labels[i], g_node_label_strings[g_node_table[g_site_grid[row][col].id].lines[i]], 15);
            g_text_window_labels[i][15] = 0;
        }
    } else {
        g_site_cursor_on_node = 0;
        strcpy(g_text_window_next_name, "Unknown");
        for (i = 0; i < 4; i++) {
            strcpy(g_text_window_labels[i], "");
        }
    }
}

extern s16 g_site_target_col;
extern u16 g_site_target_row;
extern s16 g_site_col_right_neighbour[]; /* per-column neighbour to the right */
extern s16 g_site_col_left_neighbour[]; /* per-column neighbour to the left */
extern s32 g_site_rotation NO_GP; /* column rotation of the map */

/* Is map cell (row, col) accessible? 3 sub-rows of 8 columns per g_site_grid row. */
static inline s32 CELL_OK(s32 row, s32 col) {
    s32 r = row / 3;
    s32 c = col + (row % 3) * 8;
    return g_site_grid[r][c].id >= 0;
}

/* Tries column `next` (relative to the map rotation): same row, then one
 * sub-row down/up. */
#define TRY_COL(next, skip)                                                         \
    if ((next) >= 0 && mode != (skip)) {                                            \
        col = ((next) + g_site_rotation) % 8;                                            \
        if ((r = (s16)g_site_target_row, CELL_OK(r, col))) {                               \
            g_site_target_col = col;                                                       \
            return 1;                                                               \
        }                                                                           \
        if (sub < 2 && dir != 2 && (r = (s16)g_site_target_row + 1, CELL_OK(r, col))) {    \
            g_site_target_col = col;                                                       \
            g_site_target_row = g_site_target_row + 1;                                            \
            return 1;                                                               \
        }                                                                           \
        if (sub > 0 && dir != 3 && (r = (s16)g_site_target_row - 1, CELL_OK(r, col))) {    \
            g_site_target_col = col;                                                       \
            g_site_target_row = g_site_target_row - 1;                                            \
            return 1;                                                               \
        }                                                                           \
    }

/* Moves the target cell (g_site_target_col, g_site_target_row) to the nearest accessible
 * cell: first within the same column (+1/-1/+2/-2 sub-rows, `dir` 2/3 forbid
 * moving down/up), then, unless `mode` is 1, in the neighbouring columns up to
 * three steps left/right (`mode` 4/5 skip left/right). Returns 1 if found. */
s32 site_find_nearest_node(s32 mode, s32 dir) {
    s32 rel;
    s32 col;
    s32 sub;
    s32 r;

    rel = g_site_target_col - g_site_rotation;
    if (rel < 0) {
        rel += 8;
    }
    col = g_site_target_col;
    sub = (s16)((s16)g_site_target_row % 3);

    if (sub < 2 && dir != 2 && (r = (s16)g_site_target_row + 1, CELL_OK(r, col))) {
        g_site_target_row = g_site_target_row + 1;
        return 1;
    }
    if (sub > 0 && dir != 3 && (r = (s16)g_site_target_row - 1, CELL_OK(r, col))) {
        g_site_target_row = g_site_target_row - 1;
        return 1;
    }
    if (sub <= 0 && dir != 2 && (r = (s16)g_site_target_row + 2, CELL_OK(r, col))) {
        g_site_target_row = g_site_target_row + 2;
        return 1;
    }
    if (sub >= 2 && dir != 3 && (r = (s16)g_site_target_row - 2, CELL_OK(r, col))) {
        g_site_target_row = g_site_target_row - 2;
        return 1;
    }
    if (mode == 1) {
        return 0;
    }

    TRY_COL(g_site_col_left_neighbour[rel], 4);
    TRY_COL(g_site_col_right_neighbour[rel], 5);
    TRY_COL(g_site_col_left_neighbour[g_site_col_left_neighbour[rel]], 4);
    TRY_COL(g_site_col_right_neighbour[g_site_col_right_neighbour[rel]], 5);
    TRY_COL(g_site_col_left_neighbour[g_site_col_left_neighbour[g_site_col_left_neighbour[rel]]], 4);
    TRY_COL(g_site_col_right_neighbour[g_site_col_right_neighbour[g_site_col_right_neighbour[rel]]], 5);
    return 0;
}
#undef TRY_COL

extern s16 g_site_prev_level; /* current page */
extern s16 g_site_cursor_col;
extern u16 g_site_cursor_row;
extern s16 g_site_target_col;
extern u16 g_site_target_row;
extern s16 g_site_sel_col;
extern s16 g_site_sel_col __attribute__((section(".data")));
extern s16 g_site_sel_level;
extern s16 g_site_sel_level __attribute__((section(".data")));
extern u16 g_text_window_target_x;
extern u16 g_text_window_target_x __attribute__((section(".data")));
extern u16 g_text_window_target_y;
extern u16 g_text_window_target_y __attribute__((section(".data")));
void text_window_draw_select(s16 x, s16 y, s32 reset);
s32 site_find_nearest_node(s32, s32);
extern s16 g_site_col_order[]; /* per-column neighbour counts */
extern s16 g_site_col_step_left[]; /* per-column offset when moving left */
extern s16 g_site_col_step_right[]; /* per-column offset when moving right */
extern u16 g_site_prev_cursor_row;   /* previous cursor row */
extern s16 g_pad_command;
extern s16 g_pad_command __attribute__((section(".data")));
extern s32 g_site_rotation;   /* column rotation of the map */
extern s32 g_site_rotation __attribute__((section(".data")));
void text_window_run(s32);

#ifdef NON_MATCHING
/* 75 diffs: almost all are col/row landing in s3/s2 instead of s2/s3, plus the
 * block order of one site_find_nearest_node(0, 5) exit. */
/* Moves the map cursor by (dx, dy). Returns 1 when the move leaves the
 * visible page (g_pad_command says which way), 0 otherwise. */
s32 site_move_cursor(s32 dx, s32 dy) {
    s32 rel;
    s32 col;
    s32 row;
    s32 r;
    s32 c;

    g_site_cursor_on_node = 1;
    rel = g_site_cursor_col - g_site_rotation;
    g_site_prev_cursor_row = g_site_cursor_row;
    if (rel < 0) {
        rel += 8;
    }
    if (dx > 0) {
        if (rel == 0) {
            g_pad_command = 12;
            return 1;
        }
        rel = g_site_col_step_right[rel] + g_site_rotation;
        if (rel >= 8) {
            rel -= 8;
        }
        col = rel;
    } else {
        col = g_site_cursor_col;
        if (dx < 0) {
            if (rel == 7) {
                g_pad_command = 13;
                return 1;
            }
            rel = g_site_col_step_left[rel] + g_site_rotation;
            if (rel >= 8) {
                rel -= 8;
            }
            col = rel;
        }
    }
    if (dy < 0) {
        if ((s16)g_site_cursor_row > 0 && ((s16)g_site_cursor_row + dy) / 3 * 3 == (s16)g_site_cursor_row + dy - 2) {
            g_site_target_row = g_site_cursor_row + dy;
            g_site_target_col = col;
            g_pad_command = 16;
            return 1;
        }
    } else if (dy > 0) {
        if ((s16)g_site_cursor_row + dy == ((s16)g_site_cursor_row + dy) / 3 * 3) {
            g_site_target_row = g_site_cursor_row + dy;
            g_site_target_col = col;
            g_pad_command = 15;
            return 1;
        }
    }
    row = (s16)g_site_cursor_row + dy;
    if (col < 0) {
        col = 7;
    } else if (col >= 8) {
        col = 0;
    }
    if (row < 0) {
        row = 0;
    }
    r = row / 3;
    c = col + (row % 3) * 8;
    if (g_site_grid[r][c].id < 0) {
        rel = col - g_site_rotation;
        if (rel < 0) {
            rel += 8;
        }
        if (dy > 0 && dx == 0) {
            g_site_target_col = col;
            g_site_target_row = row;
            if (site_find_nearest_node(0, 3) == 0) {
                g_site_target_row = row + 3;
                g_site_target_col = col;
                g_pad_command = 15;
                return 1;
            }
            col = g_site_target_col;
            row = (s16)g_site_target_row;
        } else if (dy < 0 && dx == 0) {
            g_site_target_col = col;
            g_site_target_row = row;
            if (site_find_nearest_node(0, 2) == 0) {
                g_site_target_row = row - 3;
                g_site_target_col = col;
                g_pad_command = 16;
                return 1;
            }
            col = g_site_target_col;
            row = (s16)g_site_target_row;
        } else {
            g_site_target_col = col;
            g_site_target_row = row;
            if (site_find_nearest_node(1, 0) == 0) {
                if (dx < 0) {
                    if (g_site_col_order[rel] >= 3) {
                        g_pad_command = 13;
                        return 1;
                    }
                    rel = g_site_col_step_left[rel] + g_site_rotation;
                    if (rel >= 8) {
                        rel -= 8;
                    }
                    col = rel;
                    r = row / 3;
                    c = col + (row % 3) * 8;
                    if (g_site_grid[r][c].id < 0) {
                        g_site_target_col = col;
                        g_site_target_row = row;
                        if (site_find_nearest_node(0, 4) == 0) {
                            g_pad_command = 13;
                            return 1;
                        }
                        col = g_site_target_col;
                        row = (s16)g_site_target_row;
                    }
                } else {
                    if (g_site_col_order[rel] <= 0) {
                        g_pad_command = 12;
                        return 1;
                    }
                    rel = g_site_col_step_right[rel] + g_site_rotation;
                    if (rel >= 8) {
                        rel -= 8;
                    }
                    col = rel;
                    r = row / 3;
                    c = col + (row % 3) * 8;
                    if (g_site_grid[r][c].id < 0) {
                        g_site_target_col = col;
                        g_site_target_row = row;
                        if (site_find_nearest_node(0, 5) != 0) {
                            col = g_site_target_col;
                            row = (s16)g_site_target_row;
                        } else {
                            g_pad_command = 12;
                            return 1;
                        }
                    }
                }
            } else {
                col = g_site_target_col;
                row = (s16)g_site_target_row;
            }
        }
        r = row / 3;
        c = col + (row % 3) * 8;
    }
    if (g_site_cursor_col != col || (s16)g_site_cursor_row != row) {
        g_text_window_target_x = g_site_grid[r][c].x;
        g_text_window_target_y = g_site_grid[r][c].y;
        site_set_node_info(r, c);
        text_window_run(1);
        g_site_cursor_col = col;
        g_site_cursor_row = row;
        g_site_sel_level = r;
        g_site_sel_col = c;
        return 0;
    }
    text_window_run(0);
    return 0;
}
#else
INCLUDE_ASM("asm/nonmatchings/game/80013138", site_move_cursor);
#endif


/* Moves the map cursor: highlights the current cell and, if `select`, jumps
 * to the target cell (or keeps the old one when it is not accessible). The
 * map is 3 rows of 8 cells per g_site_grid row. */
void site_cursor_goto_target(s32 select) {
    s32 row;
    s32 col;
    s32 r;
    s32 c;

    row = (s16)g_site_cursor_row;
    col = g_site_cursor_col;
    r = row / 3;
    c = col + (row - g_site_prev_level * 3) * 8;
    text_window_draw_select(g_site_grid[r][c].x, g_site_grid[r][c].y, select);
    if (select) {
        row = (s16)g_site_target_row;
        col = g_site_target_col;
        g_site_cursor_on_node = 1;
        r = row / 3;
        c = col + (row % 3) * 8;
        if (g_site_grid[r][c].id < 0) {
            if (site_find_nearest_node(0, 0) == 0) {
                g_site_cursor_on_node = 0;
            } else {
                col = g_site_target_col;
                row = (s16)g_site_target_row;
            }
        }
        g_site_cursor_col = col;
        g_site_cursor_row = row;
        r = row / 3;
        c = col + (row % 3) * 8;
        g_site_sel_level = r;
        g_site_sel_col = c;
        g_text_window_target_x = g_site_grid[r][c].x;
        g_text_window_target_y = g_site_grid[r][c].y;
        site_set_node_info(r, c);
    }
}

extern s16 g_site_state; /* menu state */
extern s16 g_site_return_state; /* previous menu state */
extern s16 g_site_select_anim;
extern s16 g_site_view_toggle;
extern s16 g_anim_load_started;
extern u16 g_site_idle_frames;
extern s16 g_stat_harumage;
extern u16 g_text_window_show_options;
extern u16 g_text_window_show_options __attribute__((section(".data")));
s16 lain_anim_load_move(s32);
s32 site_get_level_count(void);
s16 lain_anim_load_level_move(s32);
s16 node_pick_select_anim(void);
void snd_play_sfx(s32);
s16 lain_anim_load_select(s32);
s16 lain_anim_load_save(void);

#ifdef NON_MATCHING
/* 94 diffs (jump tables live in .rodata): mostly delay-slot filling. Everything before
 * case 10 matches. In the case 10/11/17 sub-switches, reorg fills the `beq` delay slot from
 * the target (`li v0,9`), while the original steals `a0 = 0` from the `goto done`
 * fall-through. The RTL before reorg (the .sched2 dump) has the same block layout as the
 * original, so the difference is in the jump/label shape reorg sees.
 * More precisely: the original leaves the final `jal text_window_run` delay slot empty
 * and keeps `a0 = 0` as a separate insn before it, which every `goto done`/`break` path
 * then steals into its own branch/jump delay slot; here fill_simple_delay_slots moves
 * `a0 = 0` into the jal slot first, so there is nothing to steal and the `beq`s take
 * the case bodies' `li v0,N` instead. A duplicated `text_window_run(0); return; done:
 * text_window_run(0);` tail and a `do {} while (0)` after `done:` don't change anything.
 * decomp-permuter's best (59) sets g_site_return_state before the case-10 sub-switch, which
 * changes behaviour on the default path, so it isn't usable. */
/* Handles the pending map command in g_pad_command (cursor moves, page turns,
 * menu toggles), re-dispatching while a move spills onto another page. */
void site_handle_command(void) {
    s16 old;
    s32 next;
    s32 dx;
    s32 dy;
    s32 moved;

    if (g_pad_command != 0) {
        g_site_idle_frames = 0;
    }
    moved = 0;
dispatch:
    switch (g_pad_command) {
    case 0:
        text_window_run(0);
        g_site_idle_frames++;
        return;
    case 1:
    case 2:
    case 3:
    case 4:
    case 5:
    case 6:
    case 7:
    case 8:
        moved = 1;
        switch ((s16)(g_pad_command - 1)) {
        case 0:
            dx = 0;
            dy = 1;
            break;
        case 1:
            dx = 1;
            dy = 1;
            break;
        case 2:
            dx = -1;
            dy = 1;
            break;
        case 3:
            dx = 1;
            dy = 0;
            break;
        case 4:
            dx = -1;
            dy = 0;
            break;
        case 5:
            dx = 0;
            dy = -1;
            break;
        case 6:
            dx = 1;
            dy = -1;
            break;
        case 7:
            dx = -1;
            dy = -1;
            break;
        }
        if (site_move_cursor(dx, dy) == 0) {
            return;
        }
        goto dispatch;
    case 9:
        switch (g_site_state) {
        case 0:
            if (g_site_view_toggle == 0) {
                g_pad_command = 10;
                g_site_view_toggle = 1;
            } else {
                g_pad_command = 11;
                g_site_view_toggle = 0;
            }
            break;
        case 2:
            g_pad_command = 10;
            break;
        case 1:
            g_pad_command = 11;
            break;
        }
        goto dispatch;
    case 10:
        old = g_site_state;
        switch (g_site_state) {
        case 0:
            next = 6;
            break;
        case 2:
            next = 9;
            break;
        default:
            goto done;
        }
        g_site_return_state = old;
        g_site_state = next;
        break;
    case 11:
        old = g_site_state;
        switch (g_site_state) {
        case 0:
            next = 8;
            break;
        case 1:
            next = 7;
            break;
        default:
            goto done;
        }
        g_site_return_state = old;
        g_site_state = next;
        break;
    case 12:
        g_anim_load_started = lain_anim_load_move(0);
        text_window_run(0);
        g_site_return_state = g_site_state;
        g_site_state = 10;
        g_site_target_col = g_site_cursor_col + 1;
        g_site_target_row = g_site_cursor_row;
        if (g_site_target_col >= 8) {
            g_site_target_col = g_site_cursor_col - 7;
        }
        return;
    case 13:
        g_anim_load_started = lain_anim_load_move(1);
        text_window_run(0);
        g_site_return_state = g_site_state;
        g_site_state = 13;
        g_site_target_col = g_site_cursor_col - 1;
        g_site_target_row = g_site_cursor_row;
        if (g_site_target_col < 0) {
            g_site_target_col = g_site_cursor_col + 7;
        }
        return;
    case 15:
        text_window_run(0);
        if (g_site_level < site_get_level_count() - 1) {
            g_anim_load_started = lain_anim_load_level_move(0);
            g_site_return_state = g_site_state;
            if (!moved) {
                g_site_target_col = g_site_cursor_col;
                g_site_target_row = g_site_cursor_row + 3;
            }
            switch (g_site_state) {
            case 0:
                g_site_state = 16;
                break;
            case 1:
                g_site_state = 18;
                break;
            case 2:
                g_site_state = 20;
                break;
            }
        }
        return;
    case 16:
        text_window_run(0);
        if (g_site_level > 0) {
            g_anim_load_started = lain_anim_load_level_move(1);
            g_site_return_state = g_site_state;
            if (!moved) {
                g_site_target_col = g_site_cursor_col;
                g_site_target_row = g_site_cursor_row - 3;
            }
            switch (g_site_state) {
            case 0:
                g_site_state = 17;
                break;
            case 1:
                g_site_state = 19;
                break;
            case 2:
                g_site_state = 21;
                break;
            }
        }
        return;
    case 21:
        g_text_window_show_options = ~g_text_window_show_options;
        break;
    case 17:
        g_site_select_anim = node_pick_select_anim();
        if (g_site_select_anim >= 0) {
            snd_play_sfx(0);
            g_anim_load_started = lain_anim_load_select(g_site_select_anim);
            g_site_return_state = g_site_state;
            switch (g_site_state) {
            case 0:
                g_site_state = 3;
                break;
            case 1:
                g_site_state = 22;
                break;
            case 2:
                g_site_state = 23;
                break;
            }
        } else if (g_stat_harumage < 100) {
            g_stat_harumage++;
        }
        break;
    case 19:
        snd_play_sfx(7);
        g_anim_load_started = lain_anim_load_save();
        g_site_return_state = g_site_state;
        switch (g_site_state) {
        case 0:
            g_site_state = 4;
            break;
        case 1:
            g_site_state = 24;
            break;
        case 2:
            g_site_state = 25;
            break;
        }
        break;
    case 14:
        g_site_return_state = g_site_state;
        g_site_state = 5;
        break;
    }
done:
    text_window_run(0);
}
#else
INCLUDE_ASM("asm/nonmatchings/game/80013138", site_handle_command);
#endif

/* Resets the slide animation and sets the two digit sprites' UVs from g_site_level + 1. */
void jump_menu_init(void) {
    s32 digit;

    g_jump_menu_slide = 0;
    digit = (g_site_level + 1) / 10;
    if (digit < 8) {
        g_jump_menu_sprites[0].u = digit * 24 + 48;
        g_jump_menu_sprites[0].v = 24;
    } else {
        g_jump_menu_sprites[0].u = (digit - 8) * 24 + 48;
        g_jump_menu_sprites[0].v = 56;
    }
    digit = (g_site_level + 1) % 10;
    if (digit < 8) {
        g_jump_menu_sprites[1].u = digit * 24 + 48;
        g_jump_menu_sprites[1].v = 24;
    } else {
        g_jump_menu_sprites[1].u = (digit - 8) * 24 + 48;
        g_jump_menu_sprites[1].v = 56;
    }
}

typedef struct {
    s16 x;
    s16 y;
} Point16;


extern Point16 g_jump_menu_sprite_pos[];

/* Slide-in animation step for the 7 menu sprites (sprite 5 slides horizontally).
 * g_jump_menu_sprite_pos[i] is indexed in both branches: cse substitutes i == 5 in the else
 * branch (hence 0x14(a3)), and `x - 236 + n * 236 / 8` gives the add order. */
void jump_menu_slide_in(void) {
    s32 i;
    s32 x, y;
    s32 off;

    i = 0;
    if (g_jump_menu_slide < 9) {
        for (; i < 7; i++) {
            if (i != 5) {
                off = g_jump_menu_slide * 22 - 176;
                x = g_jump_menu_sprite_pos[i].x;
                y = g_jump_menu_sprite_pos[i].y - off;
            } else {
                x = g_jump_menu_sprite_pos[i].x - 236 + g_jump_menu_slide * 236 / 8;
                y = g_jump_menu_sprite_pos[i].y;
            }
            g_jump_menu_sprites[i].x = x;
            g_jump_menu_sprites[i].y = y;
        }
        g_jump_menu_slide++;
    }
}

/* Slide-out animation step: the reverse of jump_menu_slide_in. */
void jump_menu_slide_out(void) {
    s32 i;
    s32 x, y;
    s32 off;

    if (g_jump_menu_slide >= 9) {
        g_jump_menu_slide = 8;
    }
    i = 0;
    if (g_jump_menu_slide >= 0) {
        for (; i < 7; i++) {
            if (i != 5) {
                off = g_jump_menu_slide * 22 - 176;
                x = g_jump_menu_sprite_pos[i].x;
                y = g_jump_menu_sprite_pos[i].y - off;
            } else {
                x = g_jump_menu_sprite_pos[i].x - 236 + g_jump_menu_slide * 236 / 8;
                y = g_jump_menu_sprite_pos[i].y;
            }
            g_jump_menu_sprites[i].x = x;
            g_jump_menu_sprites[i].y = y;
        }
        g_jump_menu_slide--;
    }
}

/* Sets the two digit sprites' UVs to show number + 1. */
void jump_menu_set_level(s32 number) {
    s32 digit;

    digit = (number + 1) / 10;
    if (digit < 8) {
        g_jump_menu_sprites[0].u = digit * 24 + 48;
        g_jump_menu_sprites[0].v = 24;
    } else {
        g_jump_menu_sprites[0].u = (digit - 8) * 24 + 48;
        g_jump_menu_sprites[0].v = 56;
    }
    digit = (number + 1) % 10;
    if (digit < 8) {
        g_jump_menu_sprites[1].u = digit * 24 + 48;
        g_jump_menu_sprites[1].v = 24;
    } else {
        g_jump_menu_sprites[1].u = (digit - 8) * 24 + 48;
        g_jump_menu_sprites[1].v = 56;
    }
}

extern s16 g_jump_level;
extern s16 g_current_site;
extern s32 g_frame_buffer_index;
extern s32 g_frame_buffer_index __attribute__((section(".data")));
extern GsOT g_ot_2d[];
void GsSortFastSprite(GsSPRITE *sp, GsOT *ot, u16 pri);

/* Draws the 7 menu sprites; sprites 2 and 3 are arrows hidden at the list ends. */
void jump_menu_draw(void) {
    s32 i;
    s32 visible;
    s32 limit;
    s32 cur;

    for (i = 0; i < 7; i++) {
        visible = 1;
        switch (i) {
        case 2:
            limit = 13;
            cur = g_jump_level;
            if (cur != g_site_level + 5) {
                cur++;
                if (g_current_site == 0) {
                    limit = 22;
                }
                if (cur != limit) {
                    break;
                }
            }
            if (g_jump_menu_sprites[2].cx == 0) {
                visible = 0;
            }
            break;
        case 3:
            if (g_jump_level == g_site_level - 5 || g_jump_level == 0) {
                if (g_jump_menu_sprites[3].cx == 0) {
                    visible = 0;
                }
            }
            break;
        }
        if (visible) {
            GsSortFastSprite(&g_jump_menu_sprites[i], &g_ot_2d[g_frame_buffer_index], 12);
        }
    }
}

void gfx_frame_end(void);
extern s16 g_site_bg_visible;
void gfx_frame_begin(void);
void site_scene_update(void);
void site_draw_background(void);
void site_effects_update(void);

/*
 * site_end_frame and site_draw_frame were `inline` functions in the original
 * file: they are inlined everywhere below, and GCC emitted their out-of-line
 * copies after the last function of the file (they are the two functions that
 * follow site_run, at the end of the 80013138 TU). Calling them through
 * a macro instead does NOT match: the inlined RTL changes register allocation
 * (e.g. save/load sub-states 2 and 6).
 */

inline void site_end_frame(void) {
    gfx_frame_end();
}

inline void site_draw_frame(void) {
    gfx_frame_begin();
    site_scene_update();
    if (g_site_bg_visible != 0) {
        site_draw_background();
    }
    site_effects_update();
}

extern GsSPRITE g_jump_menu_sprites[];
extern MapCell g_site_grid[][24];
extern InfoEntry g_node_table[];
extern char *g_node_text_strings[];
extern char g_text_window_next_name[];

/* $gp globals */
extern u16 g_polytan_parts; /* bit set of the "P2-0x" nodes seen */
extern u16 g_sskn_level;
extern s16 g_media_played_count;
extern s16 g_site_state; /* game state */
extern s16 g_site_return_state; /* state to return to */
extern s16 g_site_substate;
extern s16 g_site_select_anim;
extern s16 g_site_view_toggle;
extern s16 g_jump_level; /* selected level (Jump To) */
extern s16 g_jump_scroll;
extern s16 g_site_prev_level; /* current page */
extern s16 g_site_input_delay;
extern s16 g_anim_load_started;
extern s16 g_site_bg_visible;
extern u16 g_site_idle_frames;
extern s16 g_site_link_level;
extern s16 g_site_link_col;
extern s16 g_site_cursor_col;
extern u16 g_site_cursor_row;
extern s16 g_other_site_cursor_col; /* other map's cursor column */
extern s16 g_other_site_cursor_row; /* other map's cursor row */
extern s16 g_stat_gakkuri;
extern s16 D_800A6840;
extern s16 g_site_target_col;
extern u16 g_site_target_row;
extern s16 g_stat_harumage;
extern s16 g_stat_tokimeki;
extern s16 g_current_site; /* which of the two maps */
extern s16 g_screensaver_count;

/* absolutely addressed small globals */
extern s16 g_text_window_busy NO_GP;
extern s16 g_site_sel_col NO_GP;
extern s16 g_site_action_request NO_GP;
extern s16 g_pad_command NO_GP;
extern s16 g_site_sel_level NO_GP;
extern u16 g_text_window_x NO_GP;
extern u16 g_text_window_y NO_GP;
extern u16 g_text_window_target_x NO_GP;
extern u16 g_text_window_target_y NO_GP;
extern s16 g_node_select_anim_set NO_GP;
extern s32 g_site_action_busy NO_GP;
extern s32 g_site_level NO_GP; /* level offset of the map */
extern u8 g_site_node_opening NO_GP;

s32 ClearImage(RECT *rect, s32 r, s32 g, s32 b);
char *strcpy(char *dst, const char *src);
s32 strcmp(const char *a, const char *b);

void site_set_node_info(s32 row, s32 col);
s32 site_find_nearest_node(s32, s32);
void site_cursor_goto_target(s32 select);
void site_handle_command(void);
void jump_menu_init(void);
void jump_menu_slide_in(void);
void jump_menu_slide_out(void);
void jump_menu_set_level(s32 number);
void jump_menu_draw(void);
void pad_read_command(void);
s32 media_play(s32 media, s16 *skipped, s16 *params);
void idle_play_random_media(void);
s16 font_render_string(char *text, s32 x, s32 y, s32 arg3, s32 arg4);
void site_draw_notice(void);
void text_window_show_cell(s16 arg0, s16 arg1, s32 arg2, s32 arg3);
void text_window_run(s32 reset);
void text_window_set_frame_mode(s32 arg0);
s16 lain_anim_load_move(s32);
s16 lain_anim_load_level_move(s32);
s16 lain_anim_load_select(s32);
s16 lain_anim_load_save(void);
void lain_anim_idle_update(s32 arg0);
s32 lain_anim_ready_move(void);
s32 lain_anim_play_move(void);
s32 lain_anim_ready_level_move(void);
s32 lain_anim_play_level_move(void);
s32 lain_anim_ready_select(void);
s32 lain_anim_play_select(void);
s32 lain_anim_ready_save(void);
s32 lain_anim_play_save(void);
s32 lain_anim_play_site_enter(void);
void lain_anim_free_idle(void);
s32 lain_anim_is_idle(void);
void node_grid_build(void);
s32 node_find_by_keyword(s16 nodeIdx, s32 slot, s16 *outCol, s16 *outRow);
void node_unlock_children(s32 nodeIdx, s32 quiet);
s32 node_cursor_is_media(void);
void snd_play_sfx(s32);
void bgm_play(void);
void bgm_fade_out(void);
void bgm_fade_in(void);
void bgm_stop_main(void);
s32 menu_open(void);
s32 menu_run(void);
s32 menu_close(void);
void spinner_init(char *arg0, s32 arg1);
void spinner_free(void);
void gfx_frame_begin(void);
void gfx_frame_end(void);
void site_node_model_reload(s32, s32);
void site_scene_update(void);
void site_draw_background(void);
void site_scene_reset_to_node(s32 a, s32 b);
void site_effects_update(void);
s32 sskn_scene_run(void);
void gate_scene_run(void);
s32 site_change_prompt_run(void);
void polytan_scene_run(void);

void gfx_frame_end(void);


#define DRAW_MAP() site_draw_frame()

/* Copy the cursor cell's screen position to g_text_window_x/2 (unless paused). */
#define SYNC_CURSOR_POS()                                             \
    do {                                                              \
        s32 y_ = (s16)g_site_cursor_row;                                     \
        s32 r_ = y_ / 3;                                              \
        s32 c_ = g_site_cursor_col + (y_ % 3) * 8;                           \
        if (g_text_window_busy == 0) {                                        \
            g_text_window_x = g_site_grid[r_][c_].x;                        \
            g_text_window_y = g_site_grid[r_][c_].y;                        \
        }                                                             \
    } while (0)

#define CELL(r, c) g_site_grid[r][c]

/*
 * Main game-state machine of the node map, run from main()'s loop until it
 * returns:
 *   1  a save of the current map was loaded (state 4): main() reloads it
 *   2  a save of the other map was loaded (state 4): main() switches maps
 *   else  the result of site_change_prompt_run() after a media was watched to the end
 *         (state 3); main() handles 3 and restarts on anything else
 *
 * g_site_state is the state, g_site_substate the per-state sub-step, g_site_return_state the
 * state to go back to, g_site_action_request the fade/transition effect to start:
 *   0-2   idle on the map (cursor input in site_handle_command); after 500 idle
 *         frames goes to 27 (screensaver)
 *   3     open the node under the cursor: media nodes show their text and
 *         play (media_play), are marked as seen and may link to another
 *         node (-> 28) or fail (-> 29); special nodes (type 7/8/9) run their
 *         action and disappear from the map
 *   4     save/load menu (menu_open, menu_run, menu_close)
 *   5     "Jump To" level selection, then scroll to the level
 *   6-9   fade, then go to state 1/0/2/0
 *   10-15 page change (lain_anim_load_move / site_cursor_goto_target)
 *   16-17 bank change (lain_anim_load_level_move)
 *   18-21 fade, then 16/17;  22-23 fade, then 3;  24-25 fade, then 4
 *   26    initial state: wait for the map to finish loading
 *   27    screensaver (idle_play_random_media)
 *   28    move the cursor to the node a media linked to
 *   29    wait for the message box (g_pad_command == 17) after a failed link
 * Any other state spins forever.
 *
 * Matching notes: matches on 2.8.0-psx and 2.8.1-psx. Needs NEEDS_SDATA (the
 * "", "P2-0x" and "Jump To" literals live in .sdata at D_800A5F20..5F54).
 */
s32 site_run(void) {
    RECT rect_end;
    RECT rect_ss;
    RECT rect_node;
    RECT rect_unused; /* never used, but the original frame has these 8 bytes */
    s16 skipped;
    s16 col;
    s16 row;
    s32 ret;      /* return value for main() */
    s32 synced;   /* cursor position already copied to g_text_window_x/2 */
    s32 sel;      /* page/bank change: 0 = none, 1 = animating, else the state it ended in */
    s16 i;
    s32 res;      /* result of the media player */
    s32 action;   /* save/load result: 1 = reload this map, 2 = switch maps */
    s32 limit;    /* highest level + 1 of the current map */

    synced = 0;
    {
        s16 r2 = (s16)((s16)g_site_cursor_row / 3);
        s16 m2 = (s16)g_site_cursor_row % 3;
        text_window_show_cell(0, 0, r2, g_site_cursor_col + m2 * 8);
    }
    g_site_state = 26;
    g_site_view_toggle = 0;
    g_site_substate = 0;
    g_site_input_delay = 0;
    g_site_bg_visible = 1;
    g_site_idle_frames = 0;
    D_800A6840 = 0;
    bgm_play();

    while (1) {
        switch (g_site_state) {
        case 26:
            if (lain_anim_play_site_enter() == 0 && g_site_action_busy == 0) {
                s32 y = (s16)g_site_cursor_row;
                s32 r = y / 3;
                s32 c = g_site_cursor_col + (y % 3) * 8;

                g_site_state = 0;
                g_site_bg_visible = 1;
                if (g_text_window_busy == 0) {
                    g_text_window_x = CELL(r, c).x;
                    g_text_window_y = CELL(r, c).y;
                }
            }
            DRAW_MAP();
            site_end_frame();
            break;

        case 0:
        case 1:
        case 2:
            pad_read_command();
            if (g_text_window_busy == 0 && synced == 0) {
                s32 y = (s16)g_site_cursor_row;
                s32 r = y / 3;
                s32 c = g_site_cursor_col + (y % 3) * 8;
                g_text_window_x = CELL(r, c).x;
                g_text_window_y = CELL(r, c).y;
                synced = 1;
            }
            if ((s16)g_site_idle_frames >= 100) {
                lain_anim_idle_update(0);
            } else {
                lain_anim_idle_update(1);
            }
            DRAW_MAP();
            if (g_text_window_busy != 0) {
                text_window_run(0);
            } else if (g_site_input_delay != 0) {
                g_site_input_delay--;
            } else {
                site_handle_command();
            }
            site_end_frame();
            if ((s16)g_site_idle_frames >= 500 && lain_anim_is_idle() != 0) {
                SYNC_CURSOR_POS();
                g_site_return_state = g_site_state;
                g_site_state = 27;
                g_site_action_request = 28;
                g_site_idle_frames = 0;
                if (g_screensaver_count < 9) {
                    g_screensaver_count++;
                }
            }
            break;

        case 29:
            pad_read_command();
            lain_anim_idle_update(1);
            DRAW_MAP();
            site_draw_notice();
            site_end_frame();
            if (g_pad_command == 17) {
                g_pad_command = 0;
                g_site_state = g_site_return_state;
            }
            break;

        case 27:
            lain_anim_idle_update(1);
            DRAW_MAP();
            text_window_run(0);
            site_end_frame();
            if (g_site_action_busy == 0) {
                rect_ss.x = 0;
                rect_ss.y = 0;
                rect_ss.w = 320;
                rect_ss.h = 480;
                ClearImage(&rect_ss, 0, 0, 0);
                lain_anim_free_idle();
                bgm_fade_out();
                snd_play_sfx(30);
                idle_play_random_media();
                g_site_state = g_site_return_state;
                bgm_fade_in();
            }
            break;

        case 3:
            if (g_site_substate == 0) {
                if (g_anim_load_started == 0) {
                    g_anim_load_started = lain_anim_load_select(g_site_select_anim);
                    lain_anim_idle_update(1);
                } else if (lain_anim_ready_select() == 0) {
                    lain_anim_idle_update(1);
                } else if (lain_anim_play_select() >= 0) {
                    /* cursor column in the level's 3-row strip */
                    s32 col_in_level = g_site_cursor_col + ((s16)g_site_cursor_row - g_site_level * 3) * 8;

                    D_800A6840 = 1;
                    g_site_action_request = 26;
                    g_site_substate++;
                    g_site_sel_col = col_in_level;
                    g_site_sel_level = (s16)g_site_cursor_row / 3;
                }
            } else if (lain_anim_play_select() == 0) {
                g_site_substate = 0;
                g_site_state = g_site_return_state;
            }
            DRAW_MAP();
            text_window_run(0);
            site_end_frame();
            if (g_site_state == g_site_return_state && g_node_select_anim_set < 2) {
                if (g_stat_tokimeki < 100) {
                    g_stat_tokimeki++;
                }
                rect_node.x = 320;
                rect_node.y = 0;
                rect_node.w = 320;
                rect_node.h = 480;
                row = (s16)g_site_cursor_row / 3;
                col = g_site_cursor_col + (s16)((s16)g_site_cursor_row % 3) * 8;
                ClearImage(&rect_node, 0, 0, 0);
                g_site_node_opening = 0;
                bgm_fade_out();
                if (node_cursor_is_media() != 0) {
                    /* media node: show its text, then play it */
                    font_render_string(g_node_table[CELL(row, col).id].name, 320, 120, 1, 1);
                    for (i = 0; i < 3; i++) {
                        s16 t = g_node_table[CELL(row, col).id].text[i];
                        if (t >= 0) {
                            font_render_string(g_node_text_strings[t], 320, i * 16 + 72, 1, 1);
                        } else {
                            font_render_string("", 320, i * 16 + 72, 1, 1);
                        }
                    }
                    lain_anim_free_idle();
                    spinner_init("", 351);
                    skipped = (g_node_table[CELL(row, col).id].flags >> 27) & 1;
                    res = media_play(g_node_table[CELL(row, col).id].media, &skipped,
                                        g_node_table[CELL(row, col).id].params);
                    g_site_input_delay = 2;
                    D_800A6840 = 0;
                    spinner_free();
                    pad_read_command();
                    switch ((s16)res) {
                    case 1:
                        if (g_media_played_count < 9999) {
                            g_media_played_count++;
                        }
                        node_unlock_children(CELL(row, col).id, 1);
                        if (CELL(row, col).flags < 8) {
                            CELL(row, col).flags += 8;
                            site_node_model_reload(row, col);
                        }
                        ret = site_change_prompt_run(); /* nonzero: leave this map (main() gets it) */
                        if (ret != 0) {
                            s32 tc = g_site_cursor_col;
                            s32 tr = (s16)g_site_cursor_row;
                            g_site_cursor_col = g_other_site_cursor_col;
                            g_site_cursor_row = g_other_site_cursor_row;
                            g_other_site_cursor_col = tc;
                            g_other_site_cursor_row = tr;
                            if (g_current_site != 0) {
                                g_current_site = 0;
                            } else {
                                g_current_site = 1;
                            }
                            g_site_cursor_col = 6;
                            g_site_cursor_row = 10;
                            g_other_site_cursor_col = 5;
                            g_other_site_cursor_row = 0;
                        }
                        goto end;
                    case 2:
                        node_unlock_children(CELL(row, col).id, 0);
                        if (CELL(row, col).flags < 8) {
                            CELL(row, col).flags += 8;
                            site_node_model_reload(row, col);
                        }
                        /* fallthrough */
                    default:
                        if (skipped != 0) {
                            if (node_find_by_keyword(CELL(row, col).id, skipped - 1, &col, &row) != 0) {
                                /* jump to the linked node */
                                g_site_return_state = g_site_state;
                                g_site_state = 28;
                                g_site_action_request = 22;
                                g_site_sel_level = row;
                                g_site_sel_col = col;
                                g_site_link_level = row;
                                g_site_link_col = col;
                                g_site_cursor_col = col % 8;
                                g_site_cursor_row = row * 3 + col / 8;
                                g_site_bg_visible = 0;
                            } else {
                                snd_play_sfx(28);
                                g_site_return_state = g_site_state;
                                g_site_state = 29;
                                D_800A6840 = 0;
                            }
                        } else {
                            D_800A6840 = 0;
                        }
                        break;
                    }
                } else {
                    /* special node */
                    switch ((s32)((g_node_table[CELL(row, col).id].flags >> 20) & 0xF)) {
                    case 7:
                        if (sskn_scene_run() != 0) {
                            node_unlock_children(CELL(row, col).id, 0);
                            CELL(row, col).id = -1;
                            site_node_model_reload(row, col);
                            g_sskn_level++;
                            text_window_show_cell(0, 0, row, col);
                            SYNC_CURSOR_POS();
                        }
                        break;
                    case 8:
                        gate_scene_run();
                        node_unlock_children(CELL(row, col).id, 0);
                        CELL(row, col).id = -1;
                        site_node_model_reload(row, col);
                        text_window_show_cell(0, 0, row, col);
                        SYNC_CURSOR_POS();
                        break;
                    case 9:
                        if (strcmp("P2-01", g_node_table[CELL(row, col).id].name) == 0) {
                            g_polytan_parts |= 2;
                        } else if (strcmp("P2-02", g_node_table[CELL(row, col).id].name) == 0) {
                            g_polytan_parts |= 4;
                        } else if (strcmp("P2-03", g_node_table[CELL(row, col).id].name) == 0) {
                            g_polytan_parts |= 0x10;
                        } else if (strcmp("P2-04", g_node_table[CELL(row, col).id].name) == 0) {
                            g_polytan_parts |= 8;
                        } else if (strcmp("P2-05", g_node_table[CELL(row, col).id].name) == 0) {
                            g_polytan_parts |= 0x20;
                        } else if (strcmp("P2-06", g_node_table[CELL(row, col).id].name) == 0) {
                            g_polytan_parts |= 1;
                        }
                        polytan_scene_run();
                        node_unlock_children(CELL(row, col).id, 0);
                        CELL(row, col).id = -1;
                        site_node_model_reload(row, col);
                        text_window_show_cell(0, 0, row, col);
                        SYNC_CURSOR_POS();
                        break;
                    }
                }
                bgm_fade_in();
            } else if (g_stat_gakkuri < 100) {
                g_stat_gakkuri++;
            }
            break;

        case 5:
            /* "Jump To" level selection */
            switch (g_site_substate) {
            case 0:
                lain_anim_idle_update(1);
                DRAW_MAP();
                g_text_window_target_x = 138;
                g_text_window_target_y = 106;
                strcpy(g_text_window_next_name, "Jump To");
                jump_menu_init();
                text_window_set_frame_mode(1);
                text_window_run(1);
                site_end_frame();
                g_site_substate++;
                g_jump_level = g_site_level;
                break;
            case 1:
                lain_anim_idle_update(1);
                DRAW_MAP();
                jump_menu_slide_in();
                jump_menu_draw();
                text_window_run(0);
                site_end_frame();
                if (g_text_window_busy == 0) {
                    g_site_substate++;
                }
                break;
            case 2:
                pad_read_command();
                g_jump_menu_sprites[2].cx = 0;
                g_jump_menu_sprites[3].cx = 0;
                switch (g_pad_command) {
                case 1:
                case 2:
                case 3:
                    /* up; `limit` must be assigned inside the condition to match */
                    if (g_jump_level + 1 < (limit = g_current_site == 0 ? 22 : 13) &&
                        g_jump_level + 1 <= g_site_level + 5) {
                        g_jump_level++;
                        jump_menu_set_level(g_jump_level);
                        g_jump_menu_sprites[2].cx = 16;
                    }
                    break;
                case 6:
                case 7:
                case 8:
                    /* down */
                    if (g_jump_level - 1 >= 0 && g_jump_level - 1 >= g_site_level - 5) {
                        g_jump_level--;
                        jump_menu_set_level(g_jump_level);
                        g_jump_menu_sprites[3].cx = 16;
                    }
                    break;
                case 17: {
                    /* confirm: start scrolling to the selected level */
                    s16 r3;
                    s32 c3;

                    if (g_jump_level != g_site_level) {
                        if (g_site_level < g_jump_level) {
                            g_anim_load_started = lain_anim_load_level_move(0);
                        } else {
                            g_anim_load_started = lain_anim_load_level_move(1);
                        }
                        g_jump_scroll = 1;
                    } else {
                        g_jump_scroll = 0;
                    }
                    g_site_substate++;
                    r3 = (s16)g_site_cursor_row / 3;
                    c3 = g_site_cursor_col + (s16)((s16)g_site_cursor_row % 3) * 8;
                    g_text_window_target_x = CELL(r3, c3).x;
                    g_text_window_target_y = CELL(r3, c3).y;
                    site_set_node_info(r3, c3);
                    break;
                }
                case 18: {
                    /* cancel */
                    s16 r3;
                    s32 c3;

                    g_site_substate++;
                    r3 = (s16)g_site_cursor_row / 3;
                    c3 = g_site_cursor_col + (s16)((s16)g_site_cursor_row % 3) * 8;
                    g_jump_scroll = 0;
                    g_text_window_target_x = CELL(r3, c3).x;
                    g_text_window_target_y = CELL(r3, c3).y;
                    site_set_node_info(r3, c3);
                    break;
                }
                }
                lain_anim_idle_update(1);
                DRAW_MAP();
                jump_menu_draw();
                if (g_site_substate != 2) {
                    text_window_set_frame_mode(2);
                    text_window_run(1);
                } else {
                    text_window_run(0);
                }
                site_end_frame();
                break;
            case 3:
                lain_anim_idle_update(1);
                DRAW_MAP();
                jump_menu_slide_out();
                jump_menu_draw();
                text_window_run(0);
                site_end_frame();
                if (g_text_window_busy == 0) {
                    g_site_substate++;
                }
                break;
            case 4:
                lain_anim_idle_update(1);
                DRAW_MAP();
                text_window_run(0);
                site_end_frame();
                if (g_jump_scroll != 0) {
                    g_site_substate++;
                } else {
                    g_site_substate = 0;
                    g_site_state = g_site_return_state;
                }
                break;
            case 5:
                lain_anim_idle_update(1);
                DRAW_MAP();
                text_window_run(0);
                site_end_frame();
                if (g_anim_load_started != 0) {
                    if (lain_anim_ready_level_move() != 0) {
                        g_site_substate++;
                    }
                } else if (g_site_level < g_jump_level) {
                    g_anim_load_started = lain_anim_load_level_move(0);
                } else {
                    g_anim_load_started = lain_anim_load_level_move(1);
                }
                break;
            case 6:
                /* scrolled: move the cursor to the same place in the new level */
                if (lain_anim_play_level_move() >= 0) {
                    s32 r3;
                    s32 c;

                    g_site_target_row = g_site_cursor_row + (g_jump_level - g_site_level) * 3;
                    g_site_target_col = g_site_cursor_col;
                    r3 = (s16)((s16)g_site_target_row / 3);
                    c = g_site_target_col + (s16)((s16)g_site_target_row % 3) * 8;
                    if (CELL(r3, c).id < 0) {
                        site_find_nearest_node(0, 0); /* empty cell: pick another target */
                        r3 = (s16)((s16)g_site_target_row / 3);
                        c = g_site_target_col + (s16)((s16)g_site_target_row % 3) * 8;
                    }
                    g_site_action_request = 23;
                    g_site_sel_level = r3;
                    g_site_sel_col = c;
                    g_site_substate++;
                }
                DRAW_MAP();
                text_window_run(0);
                site_end_frame();
                break;
            case 7:
                if (lain_anim_play_level_move() == 0 && g_site_action_busy == 0) {
                    g_site_substate++;
                }
                DRAW_MAP();
                site_end_frame();
                break;
            case 8: {
                s16 r3;
                s32 c;

                r3 = (s16)g_site_target_row / 3;
                c = g_site_target_col + (s16)((s16)g_site_target_row % 3) * 8;
                g_site_cursor_col = g_site_target_col;
                g_site_cursor_row = g_site_target_row;
                text_window_show_cell(CELL(r3, c).x, CELL(r3, c).y, r3, c);
                SYNC_CURSOR_POS();
                g_site_substate = 0;
                g_site_state = g_site_return_state;
                lain_anim_idle_update(1);
                DRAW_MAP();
                text_window_run(0);
                site_end_frame();
                break;
            }
            }
            break;

        case 28:
            /* jump to the linked node */
            DRAW_MAP();
            site_end_frame();
            switch (g_site_substate) {
            case 0:
                if (g_site_action_busy == 0) {
                    s16 r3;
                    s32 c;

                    DRAW_MAP();
                    site_end_frame();
                    r3 = (s16)g_site_cursor_row / 3;
                    c = g_site_cursor_col + (s16)((s16)g_site_cursor_row % 3) * 8;
                    text_window_show_cell(CELL(r3, c).x, CELL(r3, c).y, r3, c);
                    SYNC_CURSOR_POS();
                    g_site_action_request = 29;
                    g_site_substate++;
                    lain_anim_idle_update(1);
                    g_site_bg_visible = 1;
                }
                break;
            case 1:
                if (g_site_action_busy == 0) {
                    g_site_substate = 0;
                    g_site_state = g_site_return_state;
                }
                break;
            }
            break;

        case 4:
            /* save / load */
            switch (g_site_substate) {
            case 0:
                lain_anim_idle_update(1);
                DRAW_MAP();
                SYNC_CURSOR_POS();
                text_window_run(0);
                site_end_frame();
                if (g_anim_load_started != 0) {
                    if (lain_anim_ready_save() != 0) {
                        g_site_action_request = 27;
                        g_site_substate++;
                    }
                } else {
                    g_anim_load_started = lain_anim_load_save();
                }
                break;
            case 1:
                if (lain_anim_play_save() == 0 && g_site_action_busy == 0) {
                    g_site_substate++;
                }
                DRAW_MAP();
                SYNC_CURSOR_POS();
                text_window_run(0);
                site_end_frame();
                break;
            case 2:
                g_site_action_request = 24;
                g_site_substate++;
                DRAW_MAP();
                site_end_frame();
                break;
            case 3:
                if (g_site_action_busy == 0) {
                    g_site_substate++;
                }
                DRAW_MAP();
                site_end_frame();
                break;
            case 4:
                DRAW_MAP();
                if (menu_open() != 0) {
                    g_site_substate++;
                }
                site_end_frame();
                break;
            case 5:
                /* the save/load menu itself */
                DRAW_MAP();
                switch (menu_run()) {
                case 1:
                    g_site_substate++;
                    break;
                case -1:
                    action = 1;
                    goto dispatch;
                case -2:
                    action = 2;
                    goto dispatch;
                }
                site_end_frame();
                action = 0;
            dispatch:
                switch (action) {
                case 1: {
                    /* loaded a save of this map: reload it */
                    s32 r2;
                    s32 c2;

                    ret = 1;
                    g_stat_tokimeki = 0;
                    g_screensaver_count = 0;
                    g_stat_harumage = 0;
                    g_stat_gakkuri = 0;
                    r2 = (s16)((s16)g_site_cursor_row / 3);
                    c2 = g_site_cursor_col + (s16)((s16)g_site_cursor_row % 3) * 8;
                    g_site_sel_level = r2;
                    g_site_sel_col = c2;
                    node_grid_build();
                    site_scene_reset_to_node(r2, c2);
                    goto end;
                }
                case 2: {
                    /* loaded a save of the other map: swap the cursors, switch maps */
                    s32 tc = g_site_cursor_col;
                    s32 tr = (s16)g_site_cursor_row;
                    s32 r2;
                    s32 c2;

                    g_site_cursor_col = g_other_site_cursor_col;
                    g_site_cursor_row = g_other_site_cursor_row;
                    g_other_site_cursor_col = tc;
                    g_other_site_cursor_row = tr;
                    ret = 2;
                    if (g_current_site != 0) {
                        g_current_site = 0;
                    } else {
                        g_current_site = 1;
                    }
                    g_stat_tokimeki = 0;
                    g_screensaver_count = 0;
                    g_stat_harumage = 0;
                    g_stat_gakkuri = 0;
                    r2 = (s16)((s16)g_site_cursor_row / 3);
                    c2 = g_site_cursor_col + (s16)((s16)g_site_cursor_row % 3) * 8;
                    g_site_sel_level = r2;
                    g_site_sel_col = c2;
                    node_grid_build();
                    site_scene_reset_to_node(r2, c2);
                    goto end;
                }
                }
                break;
            case 6:
                g_site_action_request = 25;
                g_site_substate++;
                DRAW_MAP();
                menu_close();
                site_end_frame();
                break;
            case 7:
                DRAW_MAP();
                if (menu_close() != 0) {
                    g_site_substate++;
                }
                site_end_frame();
                break;
            case 8:
                if (g_site_action_busy == 0) {
                    g_site_substate = 0;
                    g_site_state = g_site_return_state;
                }
                DRAW_MAP();
                site_end_frame();
                break;
            }
            break;

        case 6:
            if (g_site_substate == 0) {
                g_site_action_request = 10;
                g_site_substate++;
                snd_play_sfx(29);
            } else if (g_site_action_busy == 0) {
                g_site_state = 1;
                g_site_action_request = 0;
                g_site_substate = 0;
            }
            lain_anim_idle_update(1);
            DRAW_MAP();
            SYNC_CURSOR_POS();
            text_window_run(0);
            site_end_frame();
            break;
        case 7:
            if (g_site_substate == 0) {
                g_site_action_request = 11;
                g_site_substate++;
                snd_play_sfx(29);
            } else if (g_site_action_busy == 0) {
                g_site_state = 0;
                g_site_action_request = 0;
                g_site_substate = 0;
            }
            lain_anim_idle_update(1);
            DRAW_MAP();
            SYNC_CURSOR_POS();
            text_window_run(0);
            site_end_frame();
            break;
        case 8:
            if (g_site_substate == 0) {
                g_site_action_request = 11;
                g_site_substate++;
                snd_play_sfx(29);
            } else if (g_site_action_busy == 0) {
                g_site_state = 2;
                g_site_action_request = 0;
                g_site_substate = 0;
            }
            lain_anim_idle_update(1);
            DRAW_MAP();
            SYNC_CURSOR_POS();
            text_window_run(0);
            site_end_frame();
            break;
        case 9:
            if (g_site_substate == 0) {
                g_site_action_request = 10;
                g_site_substate++;
                snd_play_sfx(29);
            } else if (g_site_action_busy == 0) {
                g_site_state = 0;
                g_site_action_request = 0;
                g_site_substate = 0;
            }
            lain_anim_idle_update(1);
            DRAW_MAP();
            SYNC_CURSOR_POS();
            text_window_run(0);
            site_end_frame();
            break;

        case 10:
        case 11:
        case 12:
        case 13:
        case 14:
        case 15:
            /* page change */
            sel = 0;
            if (g_site_substate == 0) {
                if (g_anim_load_started == 0) {
                    switch (g_site_state) {
                    case 10:
                    case 11:
                    case 12:
                        g_anim_load_started = lain_anim_load_move(0);
                        break;
                    case 13:
                    case 14:
                    case 15:
                        g_anim_load_started = lain_anim_load_move(1);
                        break;
                    }
                    lain_anim_idle_update(1);
                } else if (lain_anim_ready_move() == 0) {
                    lain_anim_idle_update(1);
                } else if (lain_anim_play_move() >= 0) {
                    g_site_substate++;
                    g_site_prev_level = g_site_level;
                    switch (g_site_state) {
                    case 10:
                    case 11:
                    case 12:
                        g_site_action_request = 12;
                        break;
                    case 13:
                    case 14:
                    case 15:
                        g_site_action_request = 13;
                        break;
                    }
                    sel = 1;
                    snd_play_sfx(6);
                }
            } else if (lain_anim_play_move() == 0) {
                sel = g_site_state;
                g_site_substate = 0;
                g_site_state = g_site_return_state;
            } else {
                sel = 1;
            }
            DRAW_MAP();
            if (sel == 0) {
                text_window_run(0);
            } else {
                site_cursor_goto_target(sel == 1 ? 0 : sel);
            }
            site_end_frame();
            if (g_site_state == g_site_return_state) {
                g_site_action_request = 0;
            }
            break;

        case 16:
        case 17:
            /* bank change */
            sel = 0;
            if (g_site_substate == 0) {
                if (g_anim_load_started == 0) {
                    switch (g_site_state) {
                    case 16:
                        g_anim_load_started = lain_anim_load_level_move(0);
                        break;
                    case 17:
                        g_anim_load_started = lain_anim_load_level_move(1);
                        break;
                    }
                    lain_anim_idle_update(1);
                } else if (lain_anim_ready_level_move() == 0) {
                    lain_anim_idle_update(1);
                } else if (lain_anim_play_level_move() >= 0) {
                    g_site_substate++;
                    g_site_prev_level = g_site_level;
                    switch (g_site_state) {
                    case 16:
                        g_site_action_request = 15;
                        snd_play_sfx(4);
                        break;
                    case 17:
                        g_site_action_request = 16;
                        snd_play_sfx(5);
                        break;
                    }
                    sel = 1;
                }
            } else if (lain_anim_play_level_move() == 0) {
                sel = g_site_state;
                g_site_substate = 0;
                g_site_state = g_site_return_state;
            } else {
                sel = 1;
            }
            DRAW_MAP();
            if (sel == 0) {
                text_window_run(0);
            } else {
                site_cursor_goto_target(sel == 1 ? 0 : sel);
            }
            site_end_frame();
            if (g_site_state == g_site_return_state) {
                g_site_action_request = 0;
            }
            break;

        case 18:
        case 19:
        case 20:
        case 21:
            if (g_site_substate == 0) {
                switch (g_site_state) {
                case 18:
                case 19:
                    g_site_action_request = 11;
                    break;
                case 20:
                case 21:
                    g_site_action_request = 10;
                    break;
                }
                g_site_return_state = 0;
                g_site_substate++;
            } else if (g_site_action_busy == 0) {
                switch (g_site_state) {
                case 18:
                    g_site_state = 16;
                    break;
                case 19:
                    g_site_state = 17;
                    break;
                case 20:
                    g_site_state = 16;
                    break;
                case 21:
                    g_site_state = 17;
                    break;
                }
                g_site_action_request = 0;
                g_site_substate = 0;
            }
            lain_anim_idle_update(1);
            SYNC_CURSOR_POS();
            DRAW_MAP();
            text_window_run(0);
            site_end_frame();
            break;

        case 22:
        case 23:
            if (g_site_substate == 0) {
                switch (g_site_state) {
                case 22:
                    g_site_action_request = 11;
                    break;
                case 23:
                    g_site_action_request = 10;
                    break;
                }
                g_site_return_state = 0;
                g_site_substate++;
            } else if (g_site_action_busy == 0) {
                g_site_action_request = 0;
                g_site_substate = 0;
                g_site_state = 3;
            }
            lain_anim_idle_update(1);
            DRAW_MAP();
            SYNC_CURSOR_POS();
            text_window_run(0);
            site_end_frame();
            break;

        case 24:
        case 25:
            if (g_site_substate == 0) {
                switch (g_site_state) {
                case 24:
                    g_site_action_request = 11;
                    break;
                case 25:
                    g_site_action_request = 10;
                    break;
                }
                g_site_return_state = 0;
                g_site_substate++;
            } else if (g_site_action_busy == 0) {
                g_site_action_request = 0;
                g_site_substate = 0;
                g_site_state = 4;
            }
            lain_anim_idle_update(1);
            DRAW_MAP();
            SYNC_CURSOR_POS();
            text_window_run(0);
            site_end_frame();
            break;
        }
    }

end:
    rect_end.x = 0;
    rect_end.y = 0;
    rect_end.w = 320;
    rect_end.h = 480;
    ClearImage(&rect_end, 0, 0, 0);
    bgm_stop_main();
    return ret;
}

