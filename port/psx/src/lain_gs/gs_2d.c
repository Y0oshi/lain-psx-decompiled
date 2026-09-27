/* libgs for the Lain port: sprites, lines, caller-built polygons, TIM info. */
#include "gs_internal.h"

#include "psx/inline_c.h"

#include <string.h>

#define ATTR_BRIGHT_OFF(a) (((a) >> 6) & 1)
#define ATTR_VFLIP(a) ((a) & (1u << 22))
#define ATTR_HFLIP(a) ((a) & (1u << 23))
#define ATTR_COLMODE(a) (((a) >> 24) & 3)
#define ATTR_ROTOFF(a) ((a) & (1u << 27))
#define ATTR_ABR(a) (((a) >> 28) & 3)
#define ATTR_ABE(a) (((a) >> 30) & 1)
#define ATTR_HIDDEN(a) ((a) & (1u << 31))

static u_short sprite_tpage(const GsSPRITE *sp) {
    return (u_short)((sp->tpage & 0x1F) | (ATTR_COLMODE(sp->attribute) << 7) |
                     (ATTR_ABR(sp->attribute) << 5));
}

/* Link a texture-page change in front of what is already at entry. */
static void sort_tpage(GsOT_TAG *entry, u_int tpage_bits) {
    DR_TPAGE *dt = (DR_TPAGE *)gs_alloc_packet(sizeof(DR_TPAGE));

    /* E1 command: texture page, dithering on. The draw-on-display-area bit
     * follows GsDRAWENV (see gs_init_common: PsyCross needs it set). */
    dt->code[0] = 0xE1000200u | (GsDRAWENV.dfe ? 0x400u : 0u) | (tpage_bits & 0x1FF);
    gs_link(entry, dt, 1);
}

/* Unrotated sprite as a SPRT primitive, preceded by its texture page. */
static void sort_sprt(GsSPRITE *sp, GsOT *otp, u_short pri, int x, int y) {
    GsOT_TAG *entry = gs_ot_entry(otp, pri);
    u_int a = sp->attribute;
    SPRT *s = (SPRT *)gs_alloc_packet(sizeof(SPRT));

    setSprt(s);
    setSemiTrans(s, ATTR_ABE(a));
    setShadeTex(s, ATTR_BRIGHT_OFF(a));
    setRGB0(s, sp->r, sp->g, sp->b);
    setXY0(s, _HF(x + gs_gte_ofs_x), _HF(y + gs_gte_ofs_y));
    setUV0(s, sp->u, sp->v);
    s->clut = getClut(sp->cx, sp->cy);
    setWH(s, _HF(sp->w), _HF(sp->h));
    gs_link(entry, s, 4);
    /* Linked second, so it ends up in front of (drawn before) the sprite. */
    sort_tpage(entry, sprite_tpage(sp));
}

void GsSortFastSprite(GsSPRITE *sp, GsOT *otp, u_short pri) {
    gs_set_tag(gs_source_key(sp), 0);
    if (ATTR_HIDDEN(sp->attribute) || sp->w == 0 || sp->h == 0) {
        return;
    }
    sort_sprt(sp, otp, pri, sp->x, sp->y);
}

/* m = m * diag(sx, sy, sz): scales the columns (the sprite's own axes). */
static void scale_columns(MATRIX *m, int sx, int sy, int sz) {
    int i;

    for (i = 0; i < 3; i++) {
        m->m[i][0] = (short)((m->m[i][0] * sx) >> 12);
        m->m[i][1] = (short)((m->m[i][1] * sy) >> 12);
        m->m[i][2] = (short)((m->m[i][2] * sz) >> 12);
    }
}

void GsSortSprite(GsSPRITE *sp, GsOT *otp, u_short pri) {
    u_int a = sp->attribute;
    int unit_scale;
    MATRIX m;
    SVECTOR c[4];
    POLY_FT4 *p;
    int u0, u1, v0, v1;
    int wm1, hm1;

    gs_set_tag(gs_source_key(sp), 0);

    if (ATTR_HIDDEN(a) || sp->w == 0 || sp->h == 0) {
        return;
    }

    unit_scale = sp->scalex == ONE && sp->scaley == ONE;
    if (ATTR_ROTOFF(a) ||
        (unit_scale && sp->rotate == 0 && (a & ((1u << 22) | (1u << 23))) == 0)) {
        sort_sprt(sp, otp, pri, sp->x - sp->mx, sp->y - sp->my);
        return;
    }

    /* Rotate (rotate: 4096 per degree), scale, and place at depth h so that
     * the perspective transform maps one unit to one pixel. Like the original
     * this goes through the GTE and replaces its rotation/translation, and the
     * GTE screen offset applies. */
    if (sp->rotate == 0) {
        gs_mat_identity(&m);
    } else {
        SVECTOR r = {0, 0, 0, 0};
        r.vz = (short)(sp->rotate / 360);
        RotMatrix(&r, &m);
    }
    if (!unit_scale) {
        scale_columns(&m, sp->scalex, sp->scaley, 0);
    }
    m.t[0] = sp->x;
    m.t[1] = sp->y;
    m.t[2] = (int)CFC2(26); /* projection distance h */
    SetRotMatrix(&m);
    SetTransMatrix(&m);

    c[0].vx = -sp->mx;         c[0].vy = -sp->my;
    c[1].vx = sp->w - sp->mx;  c[1].vy = -sp->my;
    c[2].vx = -sp->mx;         c[2].vy = sp->h - sp->my;
    c[3].vx = sp->w - sp->mx;  c[3].vy = sp->h - sp->my;
    c[0].vz = c[1].vz = c[2].vz = c[3].vz = 0;
    c[0].pad = c[1].pad = c[2].pad = c[3].pad = 0;

    p = (POLY_FT4 *)gs_alloc_packet(sizeof(POLY_FT4));
    gte_ldv3(&c[0], &c[1], &c[2]);
    gte_rtpt();
    gte_stsxy3(&p->x0, &p->x1, &p->x2);
    gte_ldv0(&c[3]);
    gte_rtps();
    gte_stsxy(&p->x3);

    wm1 = sp->w - 1;
    hm1 = sp->h - 1;
    if (ATTR_HFLIP(a)) {
        u0 = (sp->u + wm1) & 0xFF;
        u1 = sp->u;
    } else {
        u0 = sp->u;
        u1 = (sp->u + wm1) & 0xFF;
    }
    if (ATTR_VFLIP(a)) {
        v0 = (sp->v + hm1) & 0xFF;
        v1 = sp->v;
    } else {
        v0 = sp->v;
        v1 = (sp->v + hm1) & 0xFF;
    }

    setPolyFT4(p);
    setSemiTrans(p, ATTR_ABE(a));
    setShadeTex(p, ATTR_BRIGHT_OFF(a));
    setRGB0(p, sp->r, sp->g, sp->b);
    setUV4(p, u0, v0, u1, v0, u0, v1, u1, v1);
    p->clut = getClut(sp->cx, sp->cy);
    p->tpage = sprite_tpage(sp);
    p->pad1 = 0;
    p->pad2 = 0;
    gs_link(gs_ot_entry(otp, pri), p, 9);
#if USE_PGXP && USE_EXTENDED_PRIM_POINTERS
    ((P_TAG *)p)->pgxp_index = PGXP_GetIndex(1);
#endif
}

void GsSortLine(GsLINE *lp, GsOT *otp, u_short pri) {
    GsOT_TAG *entry;
    LINE_F2 *l;

    gs_set_tag(gs_source_key(lp), 0);

    if (ATTR_HIDDEN(lp->attribute)) {
        return;
    }
    entry = gs_ot_entry(otp, pri);
    l = (LINE_F2 *)gs_alloc_packet(sizeof(LINE_F2));
    setLineF2(l);
    setSemiTrans(l, ATTR_ABE(lp->attribute));
    setRGB0(l, lp->r, lp->g, lp->b);
    setXY2(l, _HF(lp->x0 + gs_gte_ofs_x), _HF(lp->y0 + gs_gte_ofs_y),
              _HF(lp->x1 + gs_gte_ofs_x), _HF(lp->y1 + gs_gte_ofs_y));
    gs_link(entry, l, 3);
    sort_tpage(entry, ATTR_ABR(lp->attribute) << 5);
}

void GsSortGLine(GsGLINE *lp, GsOT *otp, u_short pri) {
    GsOT_TAG *entry;
    LINE_G2 *l;

    gs_set_tag(gs_source_key(lp), 0);

    if (ATTR_HIDDEN(lp->attribute)) {
        return;
    }
    entry = gs_ot_entry(otp, pri);
    l = (LINE_G2 *)gs_alloc_packet(sizeof(LINE_G2));
    setLineG2(l);
    setSemiTrans(l, ATTR_ABE(lp->attribute));
    setRGB0(l, lp->r0, lp->g0, lp->b0);
    setRGB1(l, lp->r1, lp->g1, lp->b1);
    l->p1 = 0;
    setXY2(l, _HF(lp->x0 + gs_gte_ofs_x), _HF(lp->y0 + gs_gte_ofs_y),
              _HF(lp->x1 + gs_gte_ofs_x), _HF(lp->y1 + gs_gte_ofs_y));
    gs_link(entry, l, 4);
    sort_tpage(entry, ATTR_ABR(lp->attribute) << 5);
}

static void shift_xy(VERTTYPE *x, VERTTYPE *y) {
#if USE_PGXP
    *x = to_half_float(from_half_float(*x) + gs_gte_ofs_x);
    *y = to_half_float(from_half_float(*y) + gs_gte_ofs_y);
#else
    *x += gs_gte_ofs_x;
    *y += gs_gte_ofs_y;
#endif
}

void GsSortPoly(void *prim, GsOT *otp, u_short pri) {
    u_char code = ((P_TAG *)prim)->code;
    size_t size;
    void *p;

    gs_set_tag(gs_source_key(prim), 0);

    if ((code & 0xE0) != 0x20) {
        return; /* not a polygon */
    }
    switch (code & 0x1C) {
    case 0x00: size = sizeof(POLY_F3); break;
    case 0x04: size = sizeof(POLY_FT3); break;
    case 0x08: size = sizeof(POLY_F4); break;
    case 0x0C: size = sizeof(POLY_FT4); break;
    case 0x10: size = sizeof(POLY_G3); break;
    case 0x14: size = sizeof(POLY_GT3); break;
    case 0x18: size = sizeof(POLY_G4); break;
    default:   size = sizeof(POLY_GT4); break;
    }
    p = gs_alloc_packet(size);
    memcpy(p, prim, size);

    if (gs_gte_ofs_x != 0 || gs_gte_ofs_y != 0) {
        switch (code & 0x1C) {
        case 0x00: { POLY_F3 *q = p; shift_xy(&q->x0, &q->y0); shift_xy(&q->x1, &q->y1); shift_xy(&q->x2, &q->y2); break; }
        case 0x04: { POLY_FT3 *q = p; shift_xy(&q->x0, &q->y0); shift_xy(&q->x1, &q->y1); shift_xy(&q->x2, &q->y2); break; }
        case 0x08: { POLY_F4 *q = p; shift_xy(&q->x0, &q->y0); shift_xy(&q->x1, &q->y1); shift_xy(&q->x2, &q->y2); shift_xy(&q->x3, &q->y3); break; }
        case 0x0C: { POLY_FT4 *q = p; shift_xy(&q->x0, &q->y0); shift_xy(&q->x1, &q->y1); shift_xy(&q->x2, &q->y2); shift_xy(&q->x3, &q->y3); break; }
        case 0x10: { POLY_G3 *q = p; shift_xy(&q->x0, &q->y0); shift_xy(&q->x1, &q->y1); shift_xy(&q->x2, &q->y2); break; }
        case 0x14: { POLY_GT3 *q = p; shift_xy(&q->x0, &q->y0); shift_xy(&q->x1, &q->y1); shift_xy(&q->x2, &q->y2); break; }
        case 0x18: { POLY_G4 *q = p; shift_xy(&q->x0, &q->y0); shift_xy(&q->x1, &q->y1); shift_xy(&q->x2, &q->y2); shift_xy(&q->x3, &q->y3); break; }
        default:   { POLY_GT4 *q = p; shift_xy(&q->x0, &q->y0); shift_xy(&q->x1, &q->y1); shift_xy(&q->x2, &q->y2); shift_xy(&q->x3, &q->y3); break; }
        }
    }
    gs_link(gs_ot_entry(otp, pri), p, (int)(size / 4) - P_LEN);
}

void GsGetTimInfo(u_int *im, GsIMAGE *tim) {
    u_int *blk;

    tim->pmode = im[0];
    blk = im + 1;
    if (im[0] & 8) {
        /* CLUT block: length, x/y, w/h, data. */
        u_int len = blk[0];
        tim->cx = (short)(blk[1] & 0xFFFF);
        tim->cy = (short)(blk[1] >> 16);
        tim->cw = (u_short)(blk[2] & 0xFFFF);
        tim->ch = (u_short)(blk[2] >> 16);
        tim->clut = blk + 3;
        blk = (u_int *)((u_char *)blk + (len & ~3u));
    }
    tim->px = (short)(blk[1] & 0xFFFF);
    tim->py = (short)(blk[1] >> 16);
    tim->pw = (u_short)(blk[2] & 0xFFFF);
    tim->ph = (u_short)(blk[2] >> 16);
    tim->pixel = blk + 3;
}
