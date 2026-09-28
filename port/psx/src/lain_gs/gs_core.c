/* libgs for the Lain port: graphics system, double buffering, ordering
 * tables and packet memory. */
#include "gs_internal.h"

#include "psx/libetc.h"
#include "PsyX/PsyX_public.h"
#include "../gpu/PsyX_GPU.h" /* activeDispEnv / currentDispEnv (C linkage) */

#include <stdio.h>
#include <string.h>

DRAWENV GsDRAWENV;
DISPENV GsDISPENV;
u_int PSDCNT = 1;
u_short PSDIDX = 0;
PACKET *GsOUT_PACKET_P;

int gs_disp_w = 320, gs_disp_h = 240;
short gs_buf_x[2], gs_buf_y[2];
short gs_origin_x, gs_origin_y;
short gs_gte_ofs_x, gs_gte_ofs_y;
int gs_ofs_mode = GsOFSGPU;
RECT16 gs_clip;

/* Resets the matrices that depend on the display size (gs_coord.c). */
void gs_reset_matrices(int w, int h);

/* Packet memory
 * On the PS1 the game hands libgs a packet buffer per frame buffer
 * (GsSetWorkBase). Host primitives are larger (64-bit tags), so the port
 * keeps its own memory instead: one ring per distinct base pointer, rewound
 * when that base is set again. PsyCross's DrawOTag consumes the primitives
 * immediately, so a ring slot is free again once its OT has been drawn. */
#define GS_RING_SLOTS 4
#define GS_RING_SIZE (2u << 20)

static struct {
    PACKET *base;          /* game's base pointer this slot stands for */
    size_t pos;
    _Alignas(16) unsigned char mem[GS_RING_SIZE];
} gs_ring[GS_RING_SLOTS];
static int gs_ring_cur;
static int gs_ring_next_victim;

void GsSetWorkBase(PACKET *base) {
    int i;

    for (i = 0; i < GS_RING_SLOTS; i++) {
        if (gs_ring[i].base == base) {
            break;
        }
    }
    if (i == GS_RING_SLOTS) {
        i = gs_ring_next_victim;
        gs_ring_next_victim = (gs_ring_next_victim + 1) % GS_RING_SLOTS;
        gs_ring[i].base = base;
    }
    gs_ring_cur = i;
    gs_ring[i].pos = 0;
    GsOUT_PACKET_P = gs_ring[i].mem;
}

PACKET *GsGetWorkBase(void) {
    return GsOUT_PACKET_P;
}

static void gs_note_packet(const void *pkt); /* frame interpolation tags, below */

void *gs_alloc_packet(size_t size) {
    size_t pos;

    size = (size + 15) & ~(size_t)15;
    pos = gs_ring[gs_ring_cur].pos;
    if (pos + size > GS_RING_SIZE) {
        /* Wrap: the oldest packets of this ring have long been drawn. */
        pos = 0;
    }
    gs_ring[gs_ring_cur].pos = pos + size;
    GsOUT_PACKET_P = gs_ring[gs_ring_cur].mem + pos + size;
    gs_note_packet(gs_ring[gs_ring_cur].mem + pos);
    return gs_ring[gs_ring_cur].mem + pos;
}

/* Primitive identities (lain: frame interpolation)
 * The renderer draws in-between frames by blending each primitive from its
 * position in the previous frame. To find "the same primitive" it asks for the
 * tag of a packet (LainGs_PacketTag): a hash of what the packet draws. */
#define GS_TAG_SLOTS 32768u
static struct {
    const void *pkt;
    uint64_t tag;
    int is2d;
} gs_tags[GS_TAG_SLOTS];
static uint64_t gs_cur_tag;
static int gs_cur_2d;
static u_int gs_cur_sub;

#define GS_OCC_SLOTS 1024u
static struct {
    const void *src;
    u_int count, frame;
} gs_occ[GS_OCC_SLOTS];
static u_int gs_frame = 1;

static uint64_t gs_mix(uint64_t x) {
    x ^= x >> 33;
    x *= 0xFF51AFD7ED558CCDull;
    x ^= x >> 33;
    x *= 0xC4CEB9FE1A85EC53ull;
    x ^= x >> 33;
    return x;
}

uint64_t gs_source_key(const void *src) {
    u_int h = (u_int)(gs_mix((uintptr_t)src) & (GS_OCC_SLOTS - 1));
    u_int n;

    for (n = 0; n < GS_OCC_SLOTS; n++, h = (h + 1) & (GS_OCC_SLOTS - 1)) {
        if (gs_occ[h].frame != gs_frame) {
            gs_occ[h].src = src;
            gs_occ[h].count = 0;
            gs_occ[h].frame = gs_frame;
            break;
        }
        if (gs_occ[h].src == src) {
            break;
        }
    }
    /* Never 0: 0 means "untagged". */
    return gs_mix((uintptr_t)src * 31u + gs_occ[h].count++) | 1;
}

void gs_set_tag(uint64_t key, u_int index) {
    gs_cur_tag = key ? gs_mix(key + index) | 1 : 0;
    gs_cur_sub = 0;
    gs_cur_2d = 0;
}

void gs_set_tag_2d(uint64_t key) {
    gs_set_tag(key, 0);
    gs_cur_2d = 1;
}

static void gs_note_packet(const void *pkt) {
    u_int h = (u_int)(((uintptr_t)pkt >> 4) & (GS_TAG_SLOTS - 1));

    gs_tags[h].pkt = pkt;
    gs_tags[h].tag = gs_cur_tag ? gs_cur_tag + gs_cur_sub++ : 0;
    gs_tags[h].is2d = gs_cur_2d;
}

uint64_t LainGs_PacketTag(const void *pkt) {
    u_int h = (u_int)(((uintptr_t)pkt >> 4) & (GS_TAG_SLOTS - 1));

    return gs_tags[h].pkt == pkt ? gs_tags[h].tag : 0;
}

/* A 2D sprite, line or polygon from GsSort* rather than a TMD. */
int LainGs_PacketIs2D(const void *pkt) {
    u_int h = (u_int)(((uintptr_t)pkt >> 4) & (GS_TAG_SLOTS - 1));

    return gs_tags[h].pkt == pkt && gs_tags[h].is2d;
}

/* Ordering tables */

GsOT_TAG *gs_ot_entry(GsOT *otp, int pri) {
    int idx = pri - (int)otp->offset;
    int n = 1 << otp->length;

    if (idx < 0) {
        idx = 0;
    } else if (idx >= n) {
        idx = n - 1;
    }
    return &otp->org[idx];
}

void gs_link(GsOT_TAG *entry, void *prim, int len_words) {
    setlen(prim, len_words);
    setaddr(prim, getaddr(entry));
    setaddr(entry, prim);
#if USE_PGXP && USE_EXTENDED_PRIM_POINTERS
    /* 2D primitive: no PGXP vertex data. The 3D paths overwrite this. */
    ((P_TAG *)prim)->pgxp_index = 0xFFFF;
#endif
}

void GsClearOt(u_short offset, u_short point, GsOT *otp) {
    int n = 1 << otp->length;

    otp->offset = offset;
    otp->point = point;
    otp->tag = &otp->org[n - 1];
    ClearOTagR((u_long *)otp->org, n);
}

GsOT *GsSortOt(GsOT *ot_src, GsOT *ot_dest) {
    P_TAG *last;
    GsOT_TAG *entry;

    /* The source list runs from its head (tag) down to org[0] and on through
     * whatever was sorted at org[0]; find its last node. */
    last = (P_TAG *)ot_src->org;
    while (!isendprim(last)) {
        last = (P_TAG *)nextPrim(last);
    }

    entry = gs_ot_entry(ot_dest, (int)ot_src->point);
    setaddr(last, getaddr(entry));
    setaddr(entry, ot_src->tag);
    return ot_dest;
}

void GsDrawOt(GsOT *otp) {
    DrawOTag((u_long *)otp->tag);
}

void GsSortClear(u_char r, u_char g, u_char b, GsOT *otp) {
    TILE *t;

    gs_set_tag(0, 0);
    t = (TILE *)gs_alloc_packet(sizeof(TILE));
    int w = gs_disp_w;

    if (GsDISPENV.isrgb24) {
        w = w * 3 / 2;
    }
    setTile(t);
    setRGB0(t, r, g, b);
    /* A fill of the whole drawing buffer: relative to the drawing offset. */
    setXY0(t, _HF(GsDRAWENV.clip.x - gs_clip.x - GsDRAWENV.ofs[0]),
              _HF(GsDRAWENV.clip.y - gs_clip.y - GsDRAWENV.ofs[1]));
    setWH(t, _HF(w), _HF(gs_disp_h));
    /* At the head of the list, so it is drawn before everything else. */
    gs_link(otp->tag, t, 3);
}

/* Display / drawing environments */

/* PsyCross renders straight into the window and positions primitives
 * relative to its "active display" area, so point that at the buffer being
 * drawn (the PS1 would be showing the other one meanwhile). */
static void gs_sync_psyx_display(void) {
    DISPENV env = GsDISPENV;

    env.disp.x = gs_buf_x[PSDIDX];
    env.disp.y = gs_buf_y[PSDIDX];
    env.disp.w = gs_disp_w;
    env.disp.h = gs_disp_h;
    activeDispEnv = env;
    currentDispEnv = env;
}

static void gs_init_common(u_short x_res, u_short y_res, u_short intmode, u_short dith,
                           u_short varmmode) {
    gs_ofs_mode = intmode & GsOFSGPU;
    gs_disp_w = x_res;
    gs_disp_h = y_res;
    GsDISPENV.disp.w = x_res;
    GsDISPENV.disp.h = y_res;
    GsDISPENV.isinter = intmode & 1;
    GsDISPENV.isrgb24 = (u_char)varmmode;
    GsDRAWENV.tpage = 0;
    GsDRAWENV.dtd = (u_char)dith;
    /* PsyCross renders to the window only when "draw on display area" is set
     * (dfe = 0 selects its off-screen render target, where PGXP 3D does not
     * work), so set it the way SetDefDrawEnv does for non-interlaced heights.
     * The PS1 libgs leaves it 0, which makes no difference on the console. */
    GsDRAWENV.dfe = y_res < 289 ? 1 : 0;
    GsDRAWENV.isbg = 0;

    gs_origin_x = gs_origin_y = 0;
    gs_clip.x = 0;
    gs_clip.y = 0;
    gs_clip.w = x_res;
    gs_clip.h = y_res;
    PSDCNT = 1;
    gs_reset_matrices(x_res, y_res);
}

void GsInitGraph(u_short x_res, u_short y_res, u_short intmode, u_short dith, u_short varmmode) {
    /* libgs orders everything, 2D and 3D, through the ordering tables. The
     * PGXP depth buffer would let opaque 2D primitives (GsSortClear, sprites
     * and lines sorted behind models) hide 3D polygons that the OT puts in
     * front of them, so draw in OT order only (PGXP precision stays on).
     * A client that wants the depth buffer can turn it back on after this. */
    g_cfg_pgxpZBuffer = 0;

    ResetGraph(((intmode >> 4) & 3) == 3 ? 3 : 0);

    memset(&GsDRAWENV, 0, sizeof(GsDRAWENV));
    memset(&GsDISPENV, 0, sizeof(GsDISPENV));
    gs_init_common(x_res, y_res, intmode, dith, varmmode);
    PutDrawEnv(&GsDRAWENV);
    PutDispEnv(&GsDISPENV);

    /* GTE defaults. */
    InitGeom();
    SetFarColor(0, 0, 0);
    SetGeomOffset(0, 0);
    gs_gte_ofs_x = gs_gte_ofs_y = 0;

    PSDIDX = 0;
    if (GsOUT_PACKET_P == NULL) {
        GsSetWorkBase(NULL);
    }
    GsSetDrawBuffClip();
    GsSetDrawBuffOffset();
}

void GsInitGraph2(u_short x_res, u_short y_res, u_short intmode, u_short dith, u_short varmmode) {
    /* Same settings as GsInitGraph, applied at the next buffer swap. */
    gs_init_common(x_res, y_res, intmode, dith, varmmode);
}

void GsDefDispBuff2(u_short x0, u_short y0, u_short x1, u_short y1) {
    gs_buf_x[0] = x0;
    gs_buf_y[0] = y0;
    gs_buf_x[1] = x1;
    gs_buf_y[1] = y1;
}

void GsDefDispBuff(u_short x0, u_short y0, u_short x1, u_short y1) {
    GsDefDispBuff2(x0, y0, x1, y1);
}

int GsGetActiveBuff(void) {
    return PSDIDX;
}

void GsSetOrign(int x, int y) {
    gs_origin_x = (short)x;
    gs_origin_y = (short)y;
}

void GsSetOrigin(int x, int y) {
    GsSetOrign(x, y);
}

void GsSetDrawBuffClip(void) {
    GsDRAWENV.clip.x = gs_buf_x[PSDIDX] + gs_clip.x;
    GsDRAWENV.clip.y = gs_buf_y[PSDIDX] + gs_clip.y;
    GsDRAWENV.clip.w = gs_clip.w;
    GsDRAWENV.clip.h = gs_clip.h;
    PutDrawEnv(&GsDRAWENV);
    gs_sync_psyx_display();
}

void GsSetDrawBuffOffset(void) {
    if (gs_ofs_mode == GsOFSGPU) {
        /* The GPU drawing offset places everything; 2D coordinates are
         * relative to the origin. */
        GsDRAWENV.ofs[0] = gs_buf_x[PSDIDX] + gs_origin_x;
        GsDRAWENV.ofs[1] = gs_buf_y[PSDIDX] + gs_origin_y;
        gs_gte_ofs_x = gs_gte_ofs_y = 0;
        PutDrawEnv(&GsDRAWENV);
    } else {
        /* GTE mode: the offset goes into the geometry offset and is added to
         * 2D primitives by hand; the GPU offset stays as it is. */
        gs_gte_ofs_x = gs_buf_x[PSDIDX] + gs_origin_x;
        gs_gte_ofs_y = gs_buf_y[PSDIDX] + gs_origin_y;
        SetGeomOffset(gs_gte_ofs_x, gs_gte_ofs_y);
    }
    gs_sync_psyx_display();
}

void GsSwapDispBuff(void) {
    static unsigned int presented_at_last_swap;

    /* Present the frame drawn into the current buffer (a no-op when the game
     * already did so, e.g. through ResetGraph(1)). A frame with no drawing at
     * all (a movie frame uploaded with LoadImage) still has to be shown: open
     * an empty scene, which starts from the display area of VRAM. */
    if (g_lain_scenesPresented == presented_at_last_swap) {
        PsyX_BeginScene();
    }
    PsyX_EndScene();
    presented_at_last_swap = g_lain_scenesPresented;
    gs_frame++; /* new frame: source occurrence counts restart */

    GsDISPENV.disp.x = gs_buf_x[PSDIDX];
    GsDISPENV.disp.y = gs_buf_y[PSDIDX];
    GsDISPENV.disp.w = gs_disp_w;
    GsDISPENV.disp.h = gs_disp_h;
    PutDispEnv(&GsDISPENV);
    SetDispMask(1);

    PSDCNT++;
    if (PSDCNT == 0) {
        PSDCNT = 1;
    }
    PSDIDX ^= 1;
    GsSetDrawBuffClip();
    GsSetDrawBuffOffset();
}
