/*
 * libgs.h: "extended graphics" library for the Lain native port.
 *
 * An independent reimplementation of the subset of the PlayStation libgs
 * interface that Serial Experiments Lain uses, built on PsyCross's libgpu and
 * libgte. Written from public documentation of the interface and from the
 * behaviour of the game's own code; no Sony source or header text is used.
 * Implementation: port/psx/src/lain_gs/.
 *
 * Structure layouts match the PS1 ones field for field. On 64-bit hosts the
 * pointer members are 8 bytes, which changes sizes/offsets of the structs
 * that contain pointers (GsOT, GsCOORDINATE2, GsDOBJ2, GsRVIEW2, GsIMAGE).
 * Structs without pointers (GsSPRITE, GsLINE, GsGLINE, GsF_LIGHT, GsFOGPARAM,
 * GsCOORD2PARAM) are byte-identical to the PS1 layout.
 *
 * Screen coordinates written into GPU primitives follow the PsyCross
 * convention: with PGXP enabled (the default) the x/y/w/h fields are half
 * floats, so C code must store them with _HF(). This matters for primitives
 * the game builds itself and passes to GsSortPoly().
 */
#ifndef LIBGS_H
#define LIBGS_H

#include <assert.h> /* libgpu.h uses static_assert */

#include "types.h"
#include "libgte.h"
#include "libgpu.h"

#if defined(_LANGUAGE_C_PLUS_PLUS) || defined(__cplusplus) || defined(c_plusplus)
extern "C" {
#endif

/* Fixed-point one (4096 = 1.0). */
#ifndef ONE
#define ONE 4096
#endif

/* Packet memory unit. */
typedef unsigned char PACKET;

/* ---- ordering tables ---------------------------------------------------- */

/* One OT entry. Identical to libgpu's OT_TAG so DrawOTag can walk it. */
typedef OT_TAG GsOT_TAG;

typedef struct {
    u_int length;     /* log2 of the number of entries (1..14) */
    GsOT_TAG *org;    /* first entry of the tag array (1 << length entries) */
    u_int offset;     /* Z value mapped to entry 0 */
    u_int point;      /* position of this OT when sorted into another (GsSortOt) */
    GsOT_TAG *tag;    /* head of the list (the entry drawn first) */
} GsOT;

/* ---- 2D objects --------------------------------------------------------- */

/* Sprite. attribute bits:
 *   6      brightness adjustment off (texture drawn as is)
 *   22/23  vertical / horizontal flip (GsSortSprite)
 *   24-25  colour mode: 0 = 4-bit CLUT, 1 = 8-bit CLUT, 2 = 15-bit direct
 *   27     rotation / scaling off (GsSortSprite)
 *   28-29  semi-transparency rate
 *   30     semi-transparency on
 *   31     not displayed */
typedef struct {
    u_int attribute;
    short x, y;          /* position (of the pivot for GsSortSprite) */
    u_short w, h;        /* size in pixels */
    u_short tpage;       /* texture page number (bits 0-4 used) */
    u_char u, v;         /* offset inside the texture page */
    short cx, cy;        /* CLUT position in VRAM */
    u_char r, g, b;      /* brightness (128 = unchanged) */
    short mx, my;        /* rotation/scaling pivot, relative to the sprite */
    short scalex, scaley;/* 4096 = 1.0 */
    int rotate;          /* 4096 = 1 degree */
} GsSPRITE;

/* Flat line. attribute bits 28-29 semi-transparency rate, 30 on, 31 hidden. */
typedef struct {
    u_int attribute;
    short x0, y0;
    short x1, y1;
    u_char r, g, b;
} GsLINE;

/* Gouraud line. */
typedef struct {
    u_int attribute;
    short x0, y0;
    short x1, y1;
    u_char r0, g0, b0;
    u_char r1, g1, b1;
} GsGLINE;

/* TIM image description filled in by GsGetTimInfo. */
typedef struct {
    u_int pmode;       /* TIM flags word: bits 0-2 pixel mode, bit 3 CLUT present */
    short px, py;      /* pixel data position in VRAM */
    u_short pw, ph;    /* pixel data size in 16-bit units */
    u_int *pixel;      /* pixel data */
    short cx, cy;      /* CLUT position */
    u_short cw, ch;    /* CLUT size */
    u_int *clut;       /* CLUT data */
} GsIMAGE;

/* ---- 3D ----------------------------------------------------------------- */

typedef struct {
    VECTOR scale;
    SVECTOR rotate;
    VECTOR trans;
} GsCOORD2PARAM;

/* Hierarchical coordinate system. Set flg to 0 after changing coord. */
typedef struct GsCOORDINATE2 {
    u_int flg;                   /* 0 = dirty, else frame the cache was built */
    MATRIX coord;                /* local matrix (relative to super) */
    MATRIX workm;                /* cached local-to-world matrix */
    GsCOORD2PARAM *param;
    struct GsCOORDINATE2 *super; /* parent, NULL = world */
    struct GsCOORDINATE2 *sub;
} GsCOORDINATE2;

/* A TMD object instance. attribute bits:
 *   3-4    light mode when bit 5 set (bit 3: fog)
 *   5      use bits 3-4 instead of the global GsSetLightMode setting
 *   6      lighting off (material colours)
 *   9-11   polygon subdivision (ignored by this implementation)
 *   30     semi-transparency on
 *   31     not displayed */
typedef struct {
    u_int attribute;
    GsCOORDINATE2 *coord2;
    u_int *tmd;        /* object entry inside a mapped TMD (set by GsLinkObject4) */
    u_int id;
} GsDOBJ2;

/* Camera: viewpoint vp looking at vr, twisted by rz (4096 = 1 degree),
 * both expressed in the coordinates of super (NULL = world). */
typedef struct {
    int vpx, vpy, vpz;
    int vrx, vry, vrz;
    int rz;
    GsCOORDINATE2 *super;
} GsRVIEW2;

/* Parallel light source: direction (any length) and colour. */
typedef struct {
    int vx, vy, vz;
    u_char r, g, b;
} GsF_LIGHT;

/* Depth-cue ("fog") parameters, as loaded into the GTE. */
typedef struct {
    short dqa;
    int dqb;
    u_char rfc, gfc, bfc;
} GsFOGPARAM;

/* ---- constants ---------------------------------------------------------- */

#define GsOFSGTE 0      /* GsInitGraph: offset applied by the GTE */
#define GsOFSGPU 4      /* GsInitGraph: offset applied by the GPU drawing offset */
#define GsINTER  1      /* interlaced */
#define GsNONINTER 0
#define GsRESET0 0
#define GsRESET3 (3 << 4)

#define GsDOFF   (1u << 31)
#define GsALON   (1u << 30)
#define GsROTOFF (1u << 27)
#define GsLOFF   (1u << 6)

#define WORLD ((GsCOORDINATE2 *)0)

/* ---- globals (libgs state that games may read) -------------------------- */

extern DRAWENV GsDRAWENV;        /* current drawing environment */
extern DISPENV GsDISPENV;        /* current display environment */
extern MATRIX GsIDMATRIX;        /* identity */
extern MATRIX GsIDMATRIX2;       /* identity with aspect-ratio correction */
extern MATRIX GsWSMATRIX;        /* world-to-screen matrix (GsSetRefView2) */
extern MATRIX GsLIGHTWSMATRIX;   /* light directions (GsSetFlatLight) */
extern MATRIX GsLIGHTCOLOR;      /* light colours (GsSetFlatLight) */
extern u_int PSDCNT;             /* frame counter (never 0), used by GsGetLws caching */
extern u_short PSDIDX;           /* active (drawing) buffer index */
extern PACKET *GsOUT_PACKET_P;   /* next free byte of the packet area */

/* ---- graphics system ---------------------------------------------------- */

void GsInitGraph(u_short x_res, u_short y_res, u_short intmode, u_short dith, u_short varmmode);
void GsInitGraph2(u_short x_res, u_short y_res, u_short intmode, u_short dith, u_short varmmode);
void GsDefDispBuff2(u_short x0, u_short y0, u_short x1, u_short y1);
void GsDefDispBuff(u_short x0, u_short y0, u_short x1, u_short y1);
void GsSwapDispBuff(void);
int GsGetActiveBuff(void);
void GsSetDrawBuffClip(void);
void GsSetDrawBuffOffset(void);
void GsSetOrign(int x, int y);
void GsSetOrigin(int x, int y);

/* Packet area. The port keeps its own (larger) packet memory: one ring per
 * distinct base pointer, reset whenever GsSetWorkBase is called with it.
 * The base's contents are never touched. */
void GsSetWorkBase(PACKET *base);
PACKET *GsGetWorkBase(void);

/* ---- ordering tables ---------------------------------------------------- */

void GsClearOt(u_short offset, u_short point, GsOT *otp);
GsOT *GsSortOt(GsOT *ot_src, GsOT *ot_dest);
void GsDrawOt(GsOT *otp);
void GsSortClear(u_char r, u_char g, u_char b, GsOT *otp);

/* ---- 2D ----------------------------------------------------------------- */

void GsSortSprite(GsSPRITE *sp, GsOT *otp, u_short pri);
void GsSortFastSprite(GsSPRITE *sp, GsOT *otp, u_short pri);
void GsSortLine(GsLINE *lp, GsOT *otp, u_short pri);
void GsSortGLine(GsGLINE *lp, GsOT *otp, u_short pri);
/* Copy a caller-built POLY_F3/F4/G3/G4/FT3/FT4/GT3/GT4 into the packet area
 * and sort it at pri. */
void GsSortPoly(void *prim, GsOT *otp, u_short pri);

/* p points just past the TIM ID word (at the flags word). */
void GsGetTimInfo(u_int *im, GsIMAGE *tim);

/* ---- 3D ----------------------------------------------------------------- */

void GsInit3D(void);
void GsSetProjection(int h);
int GsGetProjection(void);
void GsSetNearClip(int nearz);
void GsSetFarClip(int farz);

void GsInitCoordinate2(GsCOORDINATE2 *super, GsCOORDINATE2 *base);
void GsGetLw(GsCOORDINATE2 *coord, MATRIX *lw);
void GsGetLws(GsCOORDINATE2 *coord, MATRIX *lw, MATRIX *ls);
void GsGetLs(GsCOORDINATE2 *coord, MATRIX *ls);
void GsSetLsMatrix(MATRIX *mp);
void GsSetLightMatrix(MATRIX *mp);
int GsSetRefView2(GsRVIEW2 *pv);
void GsMulCoord2(MATRIX *m1, MATRIX *m2); /* m2 = m1 * m2 (with translation) */
void GsMulCoord3(MATRIX *m1, MATRIX *m2); /* m1 = m1 * m2 (with translation) */

int GsSetFlatLight(int id, GsF_LIGHT *lt);
void GsSetAmbient(int r, int g, int b);   /* 4096 = full intensity */
void GsSetLightMode(int mode);            /* bit 0: fog */
void GsSetFogParam(GsFOGPARAM *fogparm);

/* p points at the TMD flags word (just past the 0x41 ID). Idempotent.
 * 64-bit port: the object table's vert_top/normal_top/primitive_top stay
 * 32-bit and are rewritten as offsets relative to their own object entry
 * (instead of absolute addresses); flags bit 0 is set as usual. */
void GsMapModelingData(u_int *p);
/* lain: 64-bit extension. TMDs that lie inside [mem, mem + size) are mapped
 * with PS1-style 32-bit addresses (addr + offset into mem) instead of
 * self-relative offsets, so code that reads vert_top/normal_top/primitive_top
 * from the object table can resolve them (addr = 0x80000000 for PS1 RAM).
 * Call before mapping. */
void GsSetMapBase(void *mem, size_t size, u_int addr);
/* tmd_base points at the object table (just past the object count). */
void GsLinkObject4(u_int *tmd_base, GsDOBJ2 *objp, int n);
/* scratch is not used by this implementation (may be NULL). */
void GsSortObject4(GsDOBJ2 *objp, GsOT *otp, int shift, u_int *scratch);

#if defined(_LANGUAGE_C_PLUS_PLUS) || defined(__cplusplus) || defined(c_plusplus)
}
#endif

#endif /* LIBGS_H */
