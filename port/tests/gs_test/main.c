/* libgs smoke test: sprites (fast, scaled/rotated, flipped, semi-transparent)
 * from a TIM of the user's disc, flat/gouraud lines, a caller-built polygon,
 * and TMD models (one built here with textured/flat/gouraud/unlit/gradation
 * primitives, one read from the game EXE), lit by two flat lights, one copy
 * fogged. Runs a few frames with double buffering and saves a PNG of the
 * last frame to GS_TEST_OUT/gs_test.png. */
#include "psx/libgs.h"
#include "psx/libetc.h"
#include "PsyX/PsyX_public.h"
#include "PsyX/common/glad.h"

#include <SDL.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include "png_write.h"

#define OT_LEN 14
#define SPR_OT_LEN 4

static GsOT ot[2], sprite_ot[2];
static GsOT_TAG ot_tags[2][1 << OT_LEN];
static GsOT_TAG sprite_tags[2][1 << SPR_OT_LEN];
static PACKET work[2][16]; /* only used as distinct keys by the port */

static unsigned char *read_file(const char *path, size_t *size) {
    FILE *f = fopen(path, "rb");
    unsigned char *buf;
    long n;

    if (!f) {
        fprintf(stderr, "cannot open %s\n", path);
        return NULL;
    }
    fseek(f, 0, SEEK_END);
    n = ftell(f);
    fseek(f, 0, SEEK_SET);
    buf = malloc((size_t)n);
    if (fread(buf, 1, (size_t)n, f) != (size_t)n) {
        fclose(f);
        free(buf);
        return NULL;
    }
    fclose(f);
    *size = (size_t)n;
    return buf;
}

/* Upload a TIM (pointer at its ID word) and return its image info. */
static void load_tim(u_int *tim, GsIMAGE *img) {
    RECT16 r;

    GsGetTimInfo(tim + 1, img);
    setRECT(&r, img->px, img->py, img->pw, img->ph);
    LoadImage(&r, img->pixel);
    if (img->pmode & 8) {
        setRECT(&r, img->cx, img->cy, img->cw, img->ch);
        LoadImage(&r, img->clut);
    }
    DrawSync(0);
}

/* ---- a TMD cube built in code ------------------------------------------- */

static u_int cube_tmd[512];

static u_int pkt_hdr(int olen, int ilen, int flag, int mode) {
    return (u_int)olen | ((u_int)ilen << 8) | ((u_int)flag << 16) | ((u_int)mode << 24);
}

static u_int rgbw(int r, int g, int b, int x) {
    return (u_int)r | ((u_int)g << 8) | ((u_int)b << 16) | ((u_int)x << 24);
}

static u_int pair(int lo, int hi) {
    return (u_int)(lo & 0xFFFF) | ((u_int)(hi & 0xFFFF) << 16);
}

/* Face vertex indices (TL, TR, BL, BR as seen from outside) for a cube whose
 * vertex i is at (+-s, +-s, +-s) with bit0 = x>0, bit1 = y>0, bit2 = z>0. */
static void face_verts(int face, int v[4]) {
    /* normal, right, down as axis ids: 0=x 1=y 2=z, sign */
    static const int def[6][6] = {
        /* n axis, n sign, r axis, r sign, d axis, d sign */
        {2, -1, 0, +1, 1, +1}, /* -z */
        {2, +1, 0, -1, 1, +1}, /* +z */
        {0, +1, 2, +1, 1, +1}, /* +x */
        {0, -1, 2, -1, 1, +1}, /* -x */
        {1, -1, 0, +1, 2, -1}, /* -y (top) */
        {1, +1, 0, +1, 2, +1}, /* +y (bottom) */
    };
    int k;

    for (k = 0; k < 4; k++) {
        int c[3];
        int rs = (k & 1) ? 1 : -1;
        int ds = (k & 2) ? 1 : -1;
        c[def[face][0]] = def[face][1];
        c[def[face][2]] = def[face][3] * rs;
        c[def[face][4]] = def[face][5] * ds;
        v[k] = (c[0] > 0) | ((c[1] > 0) << 1) | ((c[2] > 0) << 2);
    }
}

static void build_cube(int s, u_short tpage, u_short clut) {
    u_int *t = cube_tmd;
    u_int *obj;
    u_int *p;
    int prim_count = 0;
    int v[4];
    int i;
    SVECTOR *sv;

    memset(cube_tmd, 0, sizeof(cube_tmd));
    t[0] = 0x41;
    t[1] = 0;
    t[2] = 1;
    obj = t + 3;
    p = obj + 7; /* primitives follow the object table */

    /* -z: textured flat quad (FT4, lit). */
    face_verts(0, v);
    *p++ = pkt_hdr(9, 7, 0, 0x2C);
    *p++ = 0 | (0 << 8) | ((u_int)clut << 16);
    *p++ = 127 | (0 << 8) | ((u_int)tpage << 16);
    *p++ = 0 | (127 << 8);
    *p++ = 127 | (127 << 8);
    *p++ = pair(0, v[0]);
    *p++ = pair(v[1], v[2]);
    *p++ = pair(v[3], 0);
    prim_count++;

    /* +z: flat quad, red (F4, lit). */
    face_verts(1, v);
    *p++ = pkt_hdr(5, 4, 0, 0x28);
    *p++ = rgbw(200, 40, 40, 0x28);
    *p++ = pair(1, v[0]);
    *p++ = pair(v[1], v[2]);
    *p++ = pair(v[3], 0);
    prim_count++;

    /* +x: gouraud quad, per-vertex normals (G4, lit), blue. */
    face_verts(2, v);
    *p++ = pkt_hdr(8, 5, 0, 0x38);
    *p++ = rgbw(60, 80, 220, 0x38);
    for (i = 0; i < 4; i++) {
        *p++ = pair(2, v[i]);
    }
    prim_count++;

    /* -x: unlit gouraud quad with vertex colours (G4, no light). */
    face_verts(3, v);
    *p++ = pkt_hdr(8, 6, 1, 0x38);
    *p++ = rgbw(255, 0, 0, 0x38);
    *p++ = rgbw(0, 255, 0, 0);
    *p++ = rgbw(0, 0, 255, 0);
    *p++ = rgbw(255, 255, 0, 0);
    *p++ = pair(v[0], v[1]);
    *p++ = pair(v[2], v[3]);
    prim_count++;

    /* top: two lit flat triangles with per-vertex colours (F3 + gradation). */
    face_verts(4, v);
    *p++ = pkt_hdr(6, 5, 4, 0x20);
    *p++ = rgbw(255, 255, 255, 0x20);
    *p++ = rgbw(255, 0, 255, 0);
    *p++ = rgbw(0, 255, 255, 0);
    *p++ = pair(4, v[0]);
    *p++ = pair(v[1], v[2]);
    *p++ = pkt_hdr(6, 5, 4, 0x20);
    *p++ = rgbw(255, 0, 255, 0x20);
    *p++ = rgbw(255, 255, 0, 0);
    *p++ = rgbw(0, 255, 255, 0);
    *p++ = pair(4, v[1]);
    *p++ = pair(v[3], v[2]);
    prim_count += 2;

    /* bottom: two plain lit flat triangles, yellow (F3). */
    face_verts(5, v);
    *p++ = pkt_hdr(4, 3, 0, 0x20);
    *p++ = rgbw(230, 210, 40, 0x20);
    *p++ = pair(5, v[0]);
    *p++ = pair(v[1], v[2]);
    *p++ = pkt_hdr(4, 3, 0, 0x20);
    *p++ = rgbw(230, 210, 40, 0x20);
    *p++ = pair(5, v[1]);
    *p++ = pair(v[3], v[2]);
    prim_count += 2;

    /* Vertices, then normals. Offsets are from the object table (obj). */
    obj[0] = (u_int)((p - obj) * 4);
    obj[1] = 8;
    sv = (SVECTOR *)p;
    for (i = 0; i < 8; i++) {
        sv[i].vx = (short)((i & 1) ? s : -s);
        sv[i].vy = (short)((i & 2) ? s : -s);
        sv[i].vz = (short)((i & 4) ? s : -s);
        sv[i].pad = 0;
    }
    p += 8 * 2;
    obj[2] = (u_int)((p - obj) * 4);
    obj[3] = 6;
    sv = (SVECTOR *)p;
    memset(sv, 0, 6 * sizeof(SVECTOR));
    sv[0].vz = -ONE;
    sv[1].vz = ONE;
    sv[2].vx = ONE;
    sv[3].vx = -ONE;
    sv[4].vy = -ONE;
    sv[5].vy = ONE;
    p += 6 * 2;
    obj[4] = 7 * 4; /* primitives right after the single object entry */
    obj[5] = (u_int)prim_count;
    obj[6] = 0;
}

typedef struct {
    GsDOBJ2 dobj;
    GsCOORDINATE2 coord;
    SVECTOR rot;
} Model;

static void model_init(Model *m, u_int *tmd_file, int x, int y, int z) {
    GsMapModelingData(tmd_file + 1);
    GsLinkObject4(tmd_file + 3, &m->dobj, 0);
    GsInitCoordinate2(WORLD, &m->coord);
    m->dobj.coord2 = &m->coord;
    m->dobj.attribute = 0;
    m->coord.coord.t[0] = x;
    m->coord.coord.t[1] = y;
    m->coord.coord.t[2] = z;
    memset(&m->rot, 0, sizeof(m->rot));
}

static void model_update(Model *m) {
    MATRIX mat;

    RotMatrix(&m->rot, &mat);
    mat.t[0] = m->coord.coord.t[0];
    mat.t[1] = m->coord.coord.t[1];
    mat.t[2] = m->coord.coord.t[2];
    m->coord.coord = mat;
    m->coord.flg = 0;
}

static void model_sort(Model *m, GsOT *o) {
    MATRIX lw, ls;

    GsGetLws(m->dobj.coord2, &lw, &ls);
    GsSetLightMatrix(&lw);
    GsSetLsMatrix(&ls);
    GsSortObject4(&m->dobj, o, 14 - OT_LEN, NULL);
}


/* Walk an OT and print a summary of what is in it (GS_TEST_DUMP=1). */
static void dump_ot(GsOT *o) {
    P_TAG *p = (P_TAG *)o->tag;
    int count[256] = {0};
    int shown = 0;
    int i;

    for (;;) {
        if (getlen(p) > 0) {
            count[p->code]++;
#if USE_PGXP /* lain: the port builds PsyCross without PGXP (psx/CMakeLists.txt) */
            if ((p->code & 0xE0) == 0x20 && shown < 12) {
                POLY_F3 *q = (POLY_F3 *)p;
                printf("  code %02x len %d pgxp %04x xy (%g,%g) (%g,%g) (%g,%g) rgb %d,%d,%d\n", p->code,
                       getlen(p), p->pgxp_index, from_half_float(q->x0), from_half_float(q->y0),
                       from_half_float(q->x1), from_half_float(q->y1), from_half_float(q->x2),
                       from_half_float(q->y2), q->r0, q->g0, q->b0);
                shown++;
                {
                    PGXPVData vd;
                    uint lk = *(u_short *)&q->x0 | (*(u_short *)&q->y0 << 16);
                    int ok = PGXP_GetCacheData(&vd, lk, p->pgxp_index - 2);
                    printf("    pgxp ok=%d p=(%g,%g,%g) h=%g ofs=(%g,%g)\n", ok, vd.px, vd.py, vd.pz,
                           vd.scr_h, vd.ofx, vd.ofy);
                }
            }
#endif
        }
        if (isendprim(p)) {
            break;
        }
        p = (P_TAG *)nextPrim(p);
    }
    for (i = 0; i < 256; i++) {
        if (count[i]) {
            printf("  %d x code %02x\n", count[i], i);
        }
    }
}

static void save_screenshot(const char *path) {
    int w = 0, h = 0;
    unsigned char *px, *rgb;
    int y, x;

    SDL_GL_GetDrawableSize(SDL_GL_GetCurrentWindow(), &w, &h);
    px = malloc((size_t)w * h * 4);
    rgb = malloc((size_t)w * h * 3);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glReadBuffer(GL_BACK);
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glReadPixels(0, 0, w, h, GL_RGBA, GL_UNSIGNED_BYTE, px);
    for (y = 0; y < h; y++) {
        for (x = 0; x < w; x++) {
            const unsigned char *s = px + ((size_t)(h - 1 - y) * w + x) * 4;
            unsigned char *d = rgb + ((size_t)y * w + x) * 3;
            d[0] = s[0];
            d[1] = s[1];
            d[2] = s[2];
        }
    }
    if (png_write_rgb(path, rgb, w, h) == 0) {
        printf("wrote %s (%dx%d)\n", path, w, h);
    } else {
        fprintf(stderr, "failed to write %s\n", path);
    }
    free(px);
    free(rgb);
}

int main(int argc, char **argv) {
    size_t bin_size = 0, exe_size = 0;
    unsigned char *bin, *exe;
    u_int *tim;
    GsIMAGE img;
    u_short tpage, clut;
    u_int *ring_tmd;
    Model cube, cube_fog, ring;
    GsRVIEW2 view;
    GsF_LIGHT light;
    GsFOGPARAM fog;
    GsSPRITE fast, plain, rotated, flipped, trans;
    GsLINE line;
    GsGLINE gline;
    POLY_G4 quad;
    int frames = argc > 1 ? atoi(argv[1]) : 90;
    int frame;
    int i;

    bin = read_file(LAIN_REPO_ROOT "/extract/disc1/BIN.BIN", &bin_size);
    exe = read_file(LAIN_REPO_ROOT "/extract/disc1/SLPS_016.03", &exe_size);
    if (!bin || !exe) {
        return 1;
    }

    PsyX_Initialise("gs_test", 640, 480, 0);
    if (getenv("GS_TEST_NO_PGXP")) {
        g_cfg_pgxpTextureCorrection = 0;
    }

    ResetGraph(0);
    GsInitGraph(320, 240, GsOFSGPU | GsNONINTER, 1, 0);
    GsDefDispBuff2(0, 0, 0, 240);
    GsInit3D();
    GsSetOrign(0, 0);
    GsSetProjection(350);
    for (i = 0; i < 2; i++) {
        ot[i].length = OT_LEN;
        ot[i].org = ot_tags[i];
        sprite_ot[i].length = SPR_OT_LEN;
        sprite_ot[i].org = sprite_tags[i];
    }

    /* Second TIM of BIN.BIN: 4-bit, 256x136 at VRAM (768,0), 4 CLUT rows. */
    tim = (u_int *)(bin + 0x1800);
    load_tim(tim, &img);
    printf("TIM: pmode %u, pixels at (%d,%d) %dx%d, clut at (%d,%d) %dx%d\n", img.pmode, img.px,
           img.py, img.pw, img.ph, img.cx, img.cy, img.cw, img.ch);
    tpage = GetTPage(img.pmode & 3, 0, img.px, img.py);
    clut = GetClut(img.cx, img.cy);

    /* Sprites. */
    memset(&fast, 0, sizeof(fast));
    fast.attribute = (img.pmode & 3) << 24;
    fast.x = 4;
    fast.y = 4;
    fast.w = 128;
    fast.h = 68;
    fast.tpage = tpage & 0x1F;
    fast.u = 0;
    fast.v = 0;
    fast.cx = img.cx;
    fast.cy = img.cy;
    fast.r = fast.g = fast.b = 128;
    fast.scalex = fast.scaley = ONE;

    plain = fast;          /* GsSortSprite, simple path, pivot offset */
    plain.x = 200;
    plain.y = 4;
    plain.mx = 8;
    plain.u = 128;
    plain.cy = img.cy + 1;
    plain.r = 255; plain.g = 200; plain.b = 128; /* brightened */

    rotated = fast;        /* rotation + scaling around the centre */
    rotated.w = 64;
    rotated.h = 64;
    rotated.u = 64;
    rotated.v = 64;
    rotated.mx = 32;
    rotated.my = 32;
    rotated.x = 60;
    rotated.y = 180;
    rotated.scalex = ONE * 3 / 2;
    rotated.scaley = ONE * 3 / 2;
    rotated.cy = img.cy + 2;

    flipped = fast;        /* horizontal flip */
    flipped.attribute |= 1u << 23;
    flipped.x = 250;
    flipped.y = 150;
    flipped.w = 64;
    flipped.h = 64;
    flipped.mx = 0;
    flipped.u = 0;
    flipped.v = 70;

    trans = fast;          /* semi-transparent (50/50), over the ring */
    trans.attribute |= GsALON;
    trans.x = 120;
    trans.y = 170;
    trans.w = 96;
    trans.h = 60;
    trans.u = 100;
    trans.v = 70;

    memset(&line, 0, sizeof(line));
    line.x0 = 4; line.y0 = 236; line.x1 = 316; line.y1 = 120;
    line.r = 255; line.g = 60; line.b = 60;
    memset(&gline, 0, sizeof(gline));
    gline.x0 = 4; gline.y0 = 120; gline.x1 = 316; gline.y1 = 236;
    gline.r0 = 0; gline.g0 = 255; gline.b0 = 0;
    gline.r1 = 0; gline.g1 = 0; gline.b1 = 255;

    memset(&quad, 0, sizeof(quad));
    setPolyG4(&quad);
    setXY4(&quad, _HF(270), _HF(80), _HF(316), _HF(80), _HF(270), _HF(126), _HF(316), _HF(126));
    setRGB0(&quad, 255, 0, 0);
    setRGB1(&quad, 0, 255, 0);
    setRGB2(&quad, 0, 0, 255);
    setRGB3(&quad, 255, 255, 255);

    /* Models. */
    build_cube(150, tpage, clut);
    model_init(&cube, cube_tmd, -250, -50, 0);
    model_init(&cube_fog, cube_tmd, 250, -50, 0); /* same TMD, second instance */
    cube_fog.dobj.attribute = (1u << 5) | (1u << 3); /* fog on for this one */
    {
        /* The platform ring model of the EXE (TMD at 0x8007F158). */
        const u_int addr = 0x8007F158;
        size_t off = addr - 0x80010000 + 0x800;
        ring_tmd = malloc(3016);
        memcpy(ring_tmd, exe + off, 3016);
        model_init(&ring, ring_tmd, 0, 260, 200);
    }

    /* Camera, lights, fog. */
    memset(&view, 0, sizeof(view));
    view.vpx = 0; view.vpy = -700; view.vpz = -1500;
    view.vrx = 0; view.vry = 0; view.vrz = 0;
    view.rz = 0;
    view.super = WORLD;

    light.vx = 100; light.vy = 100; light.vz = 100;
    light.r = light.g = light.b = 255;
    GsSetFlatLight(0, &light);
    light.vx = -100; light.vy = 0; light.vz = 0;
    light.r = 40; light.g = 40; light.b = 160;
    GsSetFlatLight(1, &light);
    GsSetAmbient(ONE / 5, ONE / 5, ONE / 5);
    GsSetLightMode(0);
    fog.dqa = -800;
    fog.dqb = 0x1400000;
    fog.rfc = 60; fog.gfc = 60; fog.bfc = 90;
    GsSetFogParam(&fog);

    for (frame = 0; frame < frames; frame++) {
        int buff = GsGetActiveBuff();

        GsSetWorkBase(work[buff]);
        GsClearOt(0, 0, &ot[buff]);
        GsClearOt(0, 0, &sprite_ot[buff]);

        /* 3D (geometry offset = screen centre, as the game sets it). */
        SetGeomOffset(160, 120);
        GsSetRefView2(&view);
        cube.rot.vy = (short)(frame * 24 + 300);
        cube.rot.vx = (short)(frame * 9 + 350);
        cube_fog.rot = cube.rot;
        ring.rot.vy = (short)(frame * 8);
        model_update(&cube);
        model_update(&cube_fog);
        model_update(&ring);
        model_sort(&cube, &ot[buff]);
        model_sort(&cube_fog, &ot[buff]);
        model_sort(&ring, &ot[buff]);

        /* 2D into a small OT, sorted in front of the 3D. The rotated sprite
         * goes through the GTE and gets its screen offset, so clear it. */
        SetGeomOffset(0, 0);
        rotated.rotate = frame * 4 * ONE;
        GsSortFastSprite(&fast, &sprite_ot[buff], 3);
        GsSortSprite(&plain, &sprite_ot[buff], 3);
        GsSortSprite(&rotated, &sprite_ot[buff], 2);
        GsSortSprite(&flipped, &sprite_ot[buff], 2);
        GsSortFastSprite(&trans, &sprite_ot[buff], 1);
        GsSortLine(&line, &sprite_ot[buff], 5);
        GsSortGLine(&gline, &sprite_ot[buff], 5);
        GsSortPoly(&quad, &sprite_ot[buff], 0);
        GsSortOt(&sprite_ot[buff], &ot[buff]);

        if (frame == frames - 1 && getenv("GS_TEST_DUMP")) {
            dump_ot(&ot[buff]);
        }
        DrawSync(0);
        VSync(0);
        GsSwapDispBuff();
        GsSortClear(20, 20, 50, &ot[buff]);
        GsDrawOt(&ot[buff]);

        if (frame == frames - 1) {
            mkdir(GS_TEST_OUT, 0755);
            save_screenshot(GS_TEST_OUT "/gs_test.png");
        }
    }
    GsSwapDispBuff();
    PsyX_Shutdown();
    return 0;
}
