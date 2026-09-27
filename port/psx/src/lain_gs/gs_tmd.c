/* libgs for the Lain port: TMD models (GsMapModelingData, GsLinkObject4,
 * GsSortObject4).
 *
 * TMD layout (public format description):
 *   header:  u32 id (0x41), u32 flags (bit 0: addresses resolved), u32 nobj
 *   object:  vert_top, n_vert, normal_top, n_normal, primitive_top,
 *            n_primitive, scale  (7 words; *_top are offsets from the start
 *            of the object table until mapped)
 *   vertex / normal: SVECTOR (x, y, z, pad)
 *   primitive: u8 olen, u8 ilen, u8 flag, u8 mode, then ilen data words.
 *     mode:  0x20 polygon | 0x10 gouraud | 0x08 quad | 0x04 textured
 *            | 0x02 semi-transparent | 0x01 texture brightness off
 *     flag:  0x01 no lighting | 0x02 double sided | 0x04 per-vertex colours
 *   data, in order: texture coords (u, v, and CLUT / tpage in the high half
 *   of the first / second word) when textured; colours (r, g, b, x); then
 *   u16 indices: lit flat = normal, v0..vn; lit gouraud = (normal, vertex)
 *   pairs; unlit = v0..vn.
 *
 * 64-bit: GsMapModelingData cannot store host addresses in the 32-bit
 * fields, so it rewrites each *_top as an offset from its own object entry.
 * GsSortObject4 resolves them from the entry pointer GsLinkObject4 stores.
 * TMDs inside the area given to GsSetMapBase (the port's emulated PS1 RAM)
 * get addresses of that space instead, like the PS1, because the game reads
 * vert_top/normal_top itself. */
#include "gs_internal.h"

#include "psx/inline_c.h"

#include <stdio.h>

#define OBJ_WORDS 7

/* Optional emulated address space (GsSetMapBase): TMDs inside it are mapped
 * with 32-bit "absolute" addresses in that space, as on the PS1. */
static u_char *map_mem;
static size_t map_size;
static u_int map_addr;

void GsSetMapBase(void *mem, size_t size, u_int addr) {
    map_mem = mem;
    map_size = size;
    map_addr = addr;
}

static int in_map(const void *p) {
    return map_mem && (const u_char *)p >= map_mem && (const u_char *)p < map_mem + map_size;
}

void GsMapModelingData(u_int *p) {
    u_int nobj;
    u_int *table;
    u_int i;

    if (p[0] & 1) {
        return; /* already mapped */
    }
    p[0] |= 1;
    nobj = p[1];
    table = p + 2;
    for (i = 0; i < nobj; i++) {
        u_int *obj = table + i * OBJ_WORDS;
        u_int self = i * OBJ_WORDS * 4; /* entry's offset from the table */

        if (in_map(table)) {
            u_int t = map_addr + (u_int)((u_char *)table - map_mem);
            obj[0] += t; /* vert_top */
            obj[2] += t; /* normal_top */
            obj[4] += t; /* primitive_top */
        } else {
            obj[0] -= self; /* vert_top */
            obj[2] -= self; /* normal_top */
            obj[4] -= self; /* primitive_top */
        }
    }
}

void GsLinkObject4(u_int *tmd_base, GsDOBJ2 *objp, int n) {
    objp->tmd = tmd_base + n * OBJ_WORDS;
}

static const void *obj_ptr(const u_int *obj, int field) {
    if (in_map(obj)) {
        return map_mem + (obj[field] - map_addr);
    }
    return (const u_char *)obj + (int32_t)obj[field];
}

enum { SHADE_LIT, SHADE_FOG, SHADE_NONE };

/* Light one colour with one normal (GTE NCCS/NCDS: back colour + light). */
static u_int light_color(const SVECTOR *n, u_int rgb, int shade) {
    gte_ldv0(n);
    MTC2(rgb, 6);
    if (shade == SHADE_FOG) {
        gte_ncds();
    } else {
        gte_nccs();
    }
    return MFC2(22);
}

static int warned_ilen;

void GsSortObject4(GsDOBJ2 *objp, GsOT *otp, int shift, u_int *scratch) {
    u_int a = objp->attribute;
    const u_int *obj;
    const SVECTOR *verts, *norms;
    const u_int *pkt;
    u_int nprim, pi;
    int shade;
    int abe;
    int ot_n;

    (void)scratch;
    if (a & GsDOFF) {
        return;
    }
    obj = objp->tmd;
    verts = (const SVECTOR *)obj_ptr(obj, 0);
    norms = (const SVECTOR *)obj_ptr(obj, 2);
    pkt = (const u_int *)obj_ptr(obj, 4);
    nprim = obj[5];

    if (a & GsLOFF) {
        shade = SHADE_NONE;
    } else {
        int fog = (a & (1u << 5)) ? ((a >> 3) & 1) : (gs_light_mode & 1);
        shade = fog ? SHADE_FOG : SHADE_LIT;
    }
    abe = (a >> 30) & 1;
    ot_n = 1 << otp->length;
    {
        const uint64_t key = gs_source_key(objp);

    for (pi = 0; pi < nprim; pi++) {
        u_int hdr = pkt[0];
        int ilen = (hdr >> 8) & 0xFF;
        int flag = (hdr >> 16) & 0xFF;
        int mode = hdr >> 24;
        const u_int *next = pkt + 1 + ilen;
        int iip, quad, tme, lit, fce, grd, nv, ncol, nidx, words;
        const u_char *d;
        const u_short *ix;
        u_char uv[4][2];
        u_short clut = 0, tpage = 0;
        u_int col[4];
        int vi[4], ni[4];
        u_int sxy[4];
        u_int out[4];
        int gour;
        int i, nclip, otz, idx;
        u_char code;
        GsOT_TAG *entry;
        void *prim;

        if ((mode & 0xE0) != 0x20) {
            pkt = next; /* lines / sprites in TMDs: not used by Lain */
            continue;
        }
        gs_set_tag(key, pi);
        iip = mode & 0x10;
        quad = mode & 0x08;
        tme = mode & 0x04;
        lit = !(flag & 1);
        fce = flag & 2;
        grd = flag & 4;
        nv = quad ? 4 : 3;

        if (lit) {
            ncol = tme ? 0 : (grd ? nv : 1);
            nidx = iip ? 2 * nv : nv + 1;
        } else {
            ncol = (iip || grd) ? nv : 1;
            nidx = nv;
        }
        words = (tme ? nv : 0) + ncol + (nidx + 1) / 2;
        if (words != ilen) {
            if (!warned_ilen) {
                warned_ilen = 1;
                fprintf(stderr, "libgs: TMD packet mode %02x flag %02x: %d words, expected %d\n",
                        mode, flag, ilen, words);
            }
            pkt = next;
            continue;
        }

        /* Decode. */
        d = (const u_char *)(pkt + 1);
        if (tme) {
            for (i = 0; i < nv; i++, d += 4) {
                uv[i][0] = d[0];
                uv[i][1] = d[1];
                if (i == 0) {
                    clut = (u_short)(d[2] | (d[3] << 8));
                } else if (i == 1) {
                    tpage = (u_short)(d[2] | (d[3] << 8));
                }
            }
        }
        for (i = 0; i < ncol; i++, d += 4) {
            col[i] = d[0] | (d[1] << 8) | (d[2] << 16);
        }
        for (; i < nv; i++) {
            col[i] = col[0];
        }
        if (tme && lit) {
            for (i = 0; i < nv; i++) {
                col[i] = 0x808080;
            }
        }
        ix = (const u_short *)d;
        for (i = 0; i < nv; i++) {
            if (!lit) {
                vi[i] = ix[i];
                ni[i] = 0;
            } else if (iip) {
                ni[i] = ix[2 * i];
                vi[i] = ix[2 * i + 1];
            } else {
                ni[i] = ix[0];
                vi[i] = ix[1 + i];
            }
        }
        pkt = next;

        /* Transform, reject on GTE errors (behind / too near / far off
         * screen) and back faces, like the PS1 handlers. */
        gte_ldv3(&verts[vi[0]], &verts[vi[1]], &verts[vi[2]]);
        gte_rtpt();
        if (CFC2(31) & 0x80000000u) {
            continue;
        }
        gte_nclip();
        nclip = MFC2_S(24);
        if (!fce && nclip <= 0) {
            continue;
        }
        gte_stsxy3(&sxy[0], &sxy[1], &sxy[2]);
        if (quad) {
            gte_ldv0(&verts[vi[3]]);
            gte_rtps();
            if (CFC2(31) & 0x80000000u) {
                continue;
            }
            gte_stsxy(&sxy[3]);
            gte_avsz4();
        } else {
            gte_avsz3();
        }
        otz = (int)MFC2(7);
        idx = (otz - (int)otp->offset) >> shift;
        if (idx < 0) {
            idx = 0;
        } else if (idx >= ot_n) {
            idx = ot_n - 1;
        }
        entry = &otp->org[idx];

        /* Colours. */
        gour = iip || ncol > 1;
        code = (u_char)(0x20 | (gour ? 0x10 : 0) | (quad ? 0x08 : 0) | (tme ? 0x04 : 0) |
                        (mode & 0x03) | (abe << 1));
        for (i = 0; i < (gour ? nv : 1); i++) {
            u_int rgb = col[i] | ((u_int)code << 24);

            if (lit && shade != SHADE_NONE) {
                out[i] = light_color(&norms[iip ? ni[i] : ni[0]], rgb, shade) & 0xFFFFFF;
            } else {
                out[i] = col[i];
            }
        }

#define SET_RGB(p, n, c) ((p)->r##n = (c) & 0xFF, (p)->g##n = ((c) >> 8) & 0xFF, (p)->b##n = ((c) >> 16) & 0xFF)
#define SET_XY(p, n) (*(u_int *)&(p)->x##n = sxy[n])
#define SET_UV(p, n) ((p)->u##n = uv[n][0], (p)->v##n = uv[n][1])

        switch (code & 0x1C) {
        case 0x00: { POLY_F3 *q = gs_alloc_packet(sizeof(*q)); prim = q; setPolyF3(q);
            SET_RGB(q, 0, out[0]); SET_XY(q, 0); SET_XY(q, 1); SET_XY(q, 2); break; }
        case 0x08: { POLY_F4 *q = gs_alloc_packet(sizeof(*q)); prim = q; setPolyF4(q);
            SET_RGB(q, 0, out[0]); SET_XY(q, 0); SET_XY(q, 1); SET_XY(q, 2); SET_XY(q, 3); break; }
        case 0x10: { POLY_G3 *q = gs_alloc_packet(sizeof(*q)); prim = q; setPolyG3(q);
            SET_RGB(q, 0, out[0]); SET_RGB(q, 1, out[1]); SET_RGB(q, 2, out[2]);
            q->pad1 = q->pad2 = 0;
            SET_XY(q, 0); SET_XY(q, 1); SET_XY(q, 2); break; }
        case 0x18: { POLY_G4 *q = gs_alloc_packet(sizeof(*q)); prim = q; setPolyG4(q);
            SET_RGB(q, 0, out[0]); SET_RGB(q, 1, out[1]); SET_RGB(q, 2, out[2]); SET_RGB(q, 3, out[3]);
            q->pad1 = q->pad2 = q->pad3 = 0;
            SET_XY(q, 0); SET_XY(q, 1); SET_XY(q, 2); SET_XY(q, 3); break; }
        case 0x04: { POLY_FT3 *q = gs_alloc_packet(sizeof(*q)); prim = q; setPolyFT3(q);
            SET_RGB(q, 0, out[0]); SET_XY(q, 0); SET_XY(q, 1); SET_XY(q, 2);
            SET_UV(q, 0); SET_UV(q, 1); SET_UV(q, 2); q->clut = clut; q->tpage = tpage; q->pad1 = 0; break; }
        case 0x0C: { POLY_FT4 *q = gs_alloc_packet(sizeof(*q)); prim = q; setPolyFT4(q);
            SET_RGB(q, 0, out[0]); SET_XY(q, 0); SET_XY(q, 1); SET_XY(q, 2); SET_XY(q, 3);
            SET_UV(q, 0); SET_UV(q, 1); SET_UV(q, 2); SET_UV(q, 3); q->clut = clut; q->tpage = tpage;
            q->pad1 = q->pad2 = 0; break; }
        case 0x14: { POLY_GT3 *q = gs_alloc_packet(sizeof(*q)); prim = q; setPolyGT3(q);
            SET_RGB(q, 0, out[0]); SET_RGB(q, 1, out[1]); SET_RGB(q, 2, out[2]); q->p1 = q->p2 = 0;
            SET_XY(q, 0); SET_XY(q, 1); SET_XY(q, 2);
            SET_UV(q, 0); SET_UV(q, 1); SET_UV(q, 2); q->clut = clut; q->tpage = tpage; q->pad2 = 0; break; }
        default: { POLY_GT4 *q = gs_alloc_packet(sizeof(*q)); prim = q; setPolyGT4(q);
            SET_RGB(q, 0, out[0]); SET_RGB(q, 1, out[1]); SET_RGB(q, 2, out[2]); SET_RGB(q, 3, out[3]);
            q->p1 = q->p2 = q->p3 = 0;
            SET_XY(q, 0); SET_XY(q, 1); SET_XY(q, 2); SET_XY(q, 3);
            SET_UV(q, 0); SET_UV(q, 1); SET_UV(q, 2); SET_UV(q, 3); q->clut = clut; q->tpage = tpage;
            q->pad2 = q->pad3 = 0; break; }
        }
#undef SET_RGB
#undef SET_XY
#undef SET_UV

        ((P_TAG *)prim)->code = code;
        gs_link(entry, prim, getlen(prim));
#if USE_PGXP && USE_EXTENDED_PRIM_POINTERS
        ((P_TAG *)prim)->pgxp_index = PGXP_GetIndex(1);
#endif
    }
    }
    gs_set_tag(0, 0);
}
