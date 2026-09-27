/* libgs for the Lain port: coordinate systems, camera, lights, fog. */
#include "gs_internal.h"

#include "psx/inline_c.h"

#include <stdio.h>
#include <string.h>

MATRIX GsIDMATRIX;
MATRIX GsIDMATRIX2;
MATRIX GsWSMATRIX;
MATRIX GsLIGHTWSMATRIX;
MATRIX GsLIGHTCOLOR;

int gs_light_mode;
int gs_proj_h = 1000;
int gs_near_clip = 10, gs_far_clip = 0x3FFF;

/* Fixed-point helpers */

static short sat16(int64_t v) {
    if (v > 32767) {
        return 32767;
    }
    if (v < -32768) {
        return -32768;
    }
    return (short)v;
}

void gs_mat_identity(MATRIX *m) {
    memset(m, 0, sizeof(*m));
    m->m[0][0] = m->m[1][1] = m->m[2][2] = ONE;
}

void gs_mat_mul(const MATRIX *a, const MATRIX *b, MATRIX *out) {
    MATRIX r;
    int i, j;

    for (i = 0; i < 3; i++) {
        for (j = 0; j < 3; j++) {
            int64_t s = (int64_t)a->m[i][0] * b->m[0][j] + (int64_t)a->m[i][1] * b->m[1][j] +
                        (int64_t)a->m[i][2] * b->m[2][j];
            r.m[i][j] = sat16(s >> 12);
        }
    }
    memcpy(r.t, out->t, sizeof(r.t));
    memcpy(out->m, r.m, sizeof(r.m));
}

void gs_mat_apply(const MATRIX *m, const int v[3], int out[3]) {
    int r[3];
    int i;

    for (i = 0; i < 3; i++) {
        int64_t s = (int64_t)m->m[i][0] * v[0] + (int64_t)m->m[i][1] * v[1] +
                    (int64_t)m->m[i][2] * v[2];
        r[i] = (int)(s >> 12);
    }
    out[0] = r[0];
    out[1] = r[1];
    out[2] = r[2];
}

/* m2 = m1 * m2 (rotation and translation). */
void GsMulCoord2(MATRIX *m1, MATRIX *m2) {
    int t[3];

    gs_mat_apply(m1, m2->t, t);
    gs_mat_mul(m1, m2, m2);
    m2->t[0] = t[0] + m1->t[0];
    m2->t[1] = t[1] + m1->t[1];
    m2->t[2] = t[2] + m1->t[2];
}

/* m1 = m1 * m2 (rotation and translation). */
void GsMulCoord3(MATRIX *m1, MATRIX *m2) {
    int t[3];

    gs_mat_apply(m1, m2->t, t);
    gs_mat_mul(m1, m2, m1);
    m1->t[0] += t[0];
    m1->t[1] += t[1];
    m1->t[2] += t[2];
}

/* Display-size dependent matrices; lights and camera reset (GsInitGraph). */
void gs_reset_matrices(int w, int h) {
    gs_mat_identity(&GsIDMATRIX);
    GsIDMATRIX2 = GsIDMATRIX;
    /* Aspect correction: 1.0 for a 4:3 resolution such as 320x240. */
    if (w > 0) {
        GsIDMATRIX2.m[1][1] = (short)(((h << 14) / w) / 3);
    }
    GsWSMATRIX = GsIDMATRIX;
    memset(&GsLIGHTWSMATRIX, 0, sizeof(GsLIGHTWSMATRIX));
    memset(&GsLIGHTCOLOR, 0, sizeof(GsLIGHTCOLOR));
}

/* 3D setup */

void GsInit3D(void) {
    gs_origin_x = (short)(gs_disp_w / 2);
    gs_origin_y = (short)(gs_disp_h / 2);
    GsSetDrawBuffOffset();
    gs_near_clip = 10;
    gs_far_clip = 0x3FFF;
    gs_light_mode = 0;
}

void GsSetProjection(int h) {
    gs_proj_h = h;
    SetGeomScreen(h);
}

int GsGetProjection(void) {
    return (int)CFC2(26);
}

void GsSetNearClip(int nearz) {
    gs_near_clip = nearz;
}

void GsSetFarClip(int farz) {
    gs_far_clip = farz;
}

/* Coordinate systems */

void GsInitCoordinate2(GsCOORDINATE2 *super, GsCOORDINATE2 *base) {
    gs_mat_identity(&base->coord);
    base->flg = 0;
    base->super = super;
    if (super != NULL) {
        super->sub = base;
    }
}

#define GS_MAX_DEPTH 128

/* Local-to-world matrix, reusing the cached workm of every level whose
 * coord has not been marked dirty (flg == 0) since it was computed. */
void GsGetLw(GsCOORDINATE2 *coord, MATRIX *lw) {
    GsCOORDINATE2 *chain[GS_MAX_DEPTH];
    GsCOORDINATE2 *c = coord;
    int dirty = -1;
    int n = 0;
    int start;
    int i;

    for (;;) {
        chain[n] = c;
        if (c->super == NULL) {
            if (c->flg == PSDCNT || c->flg == 0) {
                c->workm = c->coord;
                c->flg = PSDCNT;
                *lw = c->coord;
                start = n;
            } else if (dirty < 0) {
                /* Nothing on the path changed: the cached result stands. */
                *lw = chain[0]->workm;
                start = 0;
            } else {
                start = dirty + 1;
                *lw = chain[start]->workm;
            }
            break;
        }
        if (c->flg == PSDCNT) {
            *lw = c->workm;
            start = n;
            break;
        }
        if (c->flg == 0) {
            dirty = n;
        }
        if (n + 1 >= GS_MAX_DEPTH) {
            fprintf(stderr, "libgs: coordinate hierarchy too deep\n");
            *lw = c->coord;
            start = n;
            break;
        }
        c = c->super;
        n++;
    }

    for (i = start - 1; i >= 0; i--) {
        GsMulCoord3(lw, &chain[i]->coord);
        chain[i]->workm = *lw;
        chain[i]->flg = PSDCNT;
    }
}

void GsGetLws(GsCOORDINATE2 *coord, MATRIX *lw, MATRIX *ls) {
    GsGetLw(coord, lw);
    *ls = *lw;
    GsMulCoord2(&GsWSMATRIX, ls);
}

void GsGetLs(GsCOORDINATE2 *coord, MATRIX *ls) {
    MATRIX lw;

    GsGetLws(coord, &lw, ls);
}

void GsSetLsMatrix(MATRIX *mp) {
    SetRotMatrix(mp);
    SetTransMatrix(mp);
}

void GsSetLightMatrix(MATRIX *mp) {
    MATRIX m = GsLIGHTWSMATRIX;

    gs_mat_mul(&GsLIGHTWSMATRIX, mp, &m);
    SetLightMatrix(&m);
}

/* Camera */

static void axis_matrix(MATRIX *m, int s, int c, char axis) {
    gs_mat_identity(m);
    switch (axis) {
    case 'x':
        m->m[1][1] = (short)c; m->m[1][2] = (short)-s;
        m->m[2][1] = (short)s; m->m[2][2] = (short)c;
        break;
    case 'y':
        m->m[0][0] = (short)c; m->m[0][2] = (short)s;
        m->m[2][0] = (short)-s; m->m[2][2] = (short)c;
        break;
    default:
        m->m[0][0] = (short)c; m->m[0][1] = (short)-s;
        m->m[1][0] = (short)s; m->m[1][1] = (short)c;
        break;
    }
}

/* Integer square root (floor). */
static int isqrt64(int64_t v) {
    uint64_t x = (uint64_t)v, r = 0, bit = (uint64_t)1 << 62;

    if (v <= 0) {
        return 0;
    }
    while (bit > x) {
        bit >>= 2;
    }
    while (bit != 0) {
        if (x >= r + bit) {
            x -= r + bit;
            r = (r >> 1) + bit;
        } else {
            r >>= 1;
        }
        bit >>= 2;
    }
    return (int)r;
}

int GsSetRefView2(GsRVIEW2 *pv) {
    MATRIX ws = GsIDMATRIX2;
    MATRIX rot;
    int v[6];
    int maxv = 0;
    int bits = 0;
    int i;
    int dx, dy, dz;
    int dist, horiz;
    int neg_vp[3];

    /* Twist around the view axis (rz: 4096 per degree). */
    if (pv->rz != 0) {
        int a = -pv->rz / 360;
        axis_matrix(&rot, rsin(a), rcos(a), 'z');
        gs_mat_mul(&ws, &rot, &ws);
    }

    /* Direction of view, scaled down to keep the products in range. */
    v[0] = pv->vpx; v[1] = pv->vpy; v[2] = pv->vpz;
    v[3] = pv->vrx; v[4] = pv->vry; v[5] = pv->vrz;
    for (i = 0; i < 6; i++) {
        int a = v[i] < 0 ? -v[i] : v[i];
        if (a > maxv) {
            maxv = a;
        }
    }
    while (maxv > 0) {
        maxv >>= 1;
        bits++;
    }
    if (bits >= 16) {
        for (i = 0; i < 6; i++) {
            v[i] >>= bits - 15;
        }
    }
    dx = v[3] - v[0];
    dy = v[4] - v[1];
    dz = v[5] - v[2];
    dist = isqrt64((int64_t)dx * dx + (int64_t)dy * dy + (int64_t)dz * dz);
    if (dist == 0) {
        return 1;
    }
    horiz = isqrt64((int64_t)dx * dx + (int64_t)dz * dz);

    /* Pitch, then yaw. */
    axis_matrix(&rot, (int)(((int64_t)dy << 12) / dist), (int)(((int64_t)horiz << 12) / dist), 'x');
    gs_mat_mul(&ws, &rot, &ws);
    if (horiz != 0) {
        axis_matrix(&rot, (int)(-(((int64_t)dx << 12) / horiz)), (int)(((int64_t)dz << 12) / horiz),
                    'y');
        gs_mat_mul(&ws, &rot, &ws);
    }

    neg_vp[0] = -pv->vpx;
    neg_vp[1] = -pv->vpy;
    neg_vp[2] = -pv->vpz;
    gs_mat_apply(&ws, neg_vp, ws.t);

    if (pv->super != NULL) {
        /* The view is given in super's coordinates: ws = ws * inverse(lw). */
        MATRIX lw, inv;
        int t[3];
        int r, c;

        GsGetLw(pv->super, &lw);
        for (r = 0; r < 3; r++) {
            for (c = 0; c < 3; c++) {
                inv.m[r][c] = lw.m[c][r];
            }
        }
        gs_mat_apply(&inv, lw.t, t);
        inv.t[0] = -t[0];
        inv.t[1] = -t[1];
        inv.t[2] = -t[2];
        GsMulCoord2(&ws, &inv);
        ws = inv;
    }

    GsWSMATRIX = ws;
    return 0;
}

/* Lights */

int GsSetFlatLight(int id, GsF_LIGHT *lt) {
    int64_t len2 = (int64_t)lt->vx * lt->vx + (int64_t)lt->vy * lt->vy + (int64_t)lt->vz * lt->vz;
    int len = isqrt64(len2);

    if (len == 0 || id < 0 || id > 2) {
        return -1;
    }
    /* Row id: the direction light travels towards, negated and normalised. */
    GsLIGHTWSMATRIX.m[id][0] = (short)(((int64_t)-lt->vx << 12) / len);
    GsLIGHTWSMATRIX.m[id][1] = (short)(((int64_t)-lt->vy << 12) / len);
    GsLIGHTWSMATRIX.m[id][2] = (short)(((int64_t)-lt->vz << 12) / len);
    /* Column id: the colour, 255 -> 4096. */
    GsLIGHTCOLOR.m[0][id] = (short)((lt->r << 12) / 255);
    GsLIGHTCOLOR.m[1][id] = (short)((lt->g << 12) / 255);
    GsLIGHTCOLOR.m[2][id] = (short)((lt->b << 12) / 255);
    SetColorMatrix(&GsLIGHTCOLOR);
    return 0;
}

void GsSetAmbient(int r, int g, int b) {
    SetBackColor(r >> 4, g >> 4, b >> 4);
}

void GsSetLightMode(int mode) {
    if (mode < 0 || mode > 3) {
        fprintf(stderr, "libgs: GsSetLightMode: bad mode %d\n", mode);
        return;
    }
    gs_light_mode = mode;
}

void GsSetFogParam(GsFOGPARAM *fogparm) {
    SetDQA(fogparm->dqa);
    SetDQB(fogparm->dqb);
    SetFarColor(fogparm->rfc, fogparm->gfc, fogparm->bfc);
}
