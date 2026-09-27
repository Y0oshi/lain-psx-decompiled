#include "common.h"

/* Minimal PsyQ libgte/libgpu/libgs types used by this file. */
typedef struct {
    s16 m[3][3];
    s32 t[3];
} MATRIX;

typedef struct {
    s16 vx, vy, vz, pad;
} SVECTOR;

typedef struct {
    s16 x, y, w, h;
} RECT;

typedef struct GsCOORDINATE2 {
    u32 flg;
    MATRIX coord;
    MATRIX workm;
    void *param;
    struct GsCOORDINATE2 *super;
    struct GsCOORDINATE2 *sub;
} GsCOORDINATE2;

typedef struct {
    u32 attribute;
    GsCOORDINATE2 *coord2;
    u32 *tmd;
    u32 id;
} GsDOBJ2;

typedef struct {
    u32 length;
    void *org;
    u32 offset;
    u32 point;
    void *tag;
} GsOT;

typedef struct {
    s16 vx, vy;
} DVECTOR;

typedef struct {
    s32 vpx, vpy, vpz;
    s32 vrx, vry, vrz;
    s32 rz;
    GsCOORDINATE2 *super;
} GsRVIEW2;

typedef struct {
    s16 dqa;
    s32 dqb;
    u8 rfc, gfc, bfc;
} GsFOGPARAM;

typedef struct {
    u32 pmode;
    s16 px, py;
    u16 pw, ph;
    u32 *pixel;
    s16 cx, cy;
    u16 cw, ch;
    u32 *clut;
} GsIMAGE;

/* A TMD model instance: one GsDOBJ2 per sub-object, its coordinate system
 * and rotation. */
typedef struct {
    GsDOBJ2 dobj;         /* 0x00 */
    GsCOORDINATE2 coord;  /* 0x10 */
    SVECTOR rot;          /* 0x60 */
    u32 nobj;             /* 0x68 */
    u32 *tmd;             /* 0x6C */
    s32 id;               /* 0x70 */
} Model;

/* A coordinate system paired with its rotation. */
typedef struct {
    GsCOORDINATE2 coord;  /* 0x00 */
    SVECTOR rot;          /* 0x50 */
} RotCoord;

/* Two coordinate systems sharing one rotation. */
typedef struct {
    GsCOORDINATE2 coord0; /* 0x00 */
    GsCOORDINATE2 coord1; /* 0x50 */
    SVECTOR rot;          /* 0xA0 */
} RotCoordPair;

void GsSetLightMode(s32 mode);
void GsSetFogParam(GsFOGPARAM *fogparam);
void GsInitCoordinate2(GsCOORDINATE2 *super, GsCOORDINATE2 *base);
void GsMapModelingData(u32 *p);
void GsGetLws(GsCOORDINATE2 *coord, MATRIX *lw, MATRIX *ls);
void GsSetLightMatrix(MATRIX *mp);
void GsSetLsMatrix(MATRIX *mp);
void GsSortObject4(GsDOBJ2 *objp, GsOT *otp, s32 shift, u32 *scratch);
void GsLinkObject4(u32 *tmd, GsDOBJ2 *objp, s32 n);
void GsGetTimInfo(u32 *im, GsIMAGE *tim);
s32 LoadImage(RECT *rect, u32 *p);
MATRIX *RotMatrix(SVECTOR *r, MATRIX *m);
s32 GsSetRefView2(GsRVIEW2 *pv);
s32 GsGetActiveBuff(void);
void GsClearOt(u16 offset, u16 point, GsOT *otp);
GsOT *GsSortOt(GsOT *ot_src, GsOT *ot_dest);
s32 DrawSync(s32 mode);
s32 VSync(s32 mode);
s32 ResetGraph(s32 mode);
void GsSwapDispBuff(void);
void GsSortClear(u8 r, u8 g, u8 b, GsOT *otp);
void GsDrawOt(GsOT *otp);

typedef struct {
    double x, y, z;
} DVECTOR3;

/* One cell of the placement grid: which TMD to use and the model slot it got. */
typedef struct {
    s16 kind; /* -1: default model */
    s16 tmdIndex;
    u32 model;
    s32 screenXY;
} GridCell;

typedef struct {
    GridCell cells[24];
} GridRow;

u32 site_get_level_count(void);
void camera_set_view(s32 view);
void GsSetNearClip(s32 arg0);
void GsSetFarClip(s32 arg0);
void GsSetProjection(s32 arg0);
void GsSetWorkBase(u8 *base);
s32 GsGetProjection(void);

extern s32 g_interp_steps_left;
extern s32 g_frame_buffer_index;
extern s32 g_interp_count;
extern s32 g_light_fade_index;
extern u8 g_light_fade_active;
extern u8 g_camera_view;
extern s32 g_dobj_id_next;
extern u8 D_800A6998;
extern s32 g_light_fade_target;
extern u32 g_site_node_model_count;

/* TMD files. Declared as small scalars (only their address is used) so that
 * GCC loads the address with a single `la`, as the original does. */
extern u32 g_node_open_tmd;
extern u32 g_level_ring_tmd;
extern u32 g_level_spinner_tmd;
extern u32 g_level_label_even_tmd;
extern u32 g_level_label_odd_tmd;
extern u32 D_800840D8;

/* Placement offsets for the models of a set (x, y, z of two SVECTORs). */
extern s16 g_site_level_ring_pos[];
extern s16 D_800A66CE[]; /* = g_site_level_ring_pos.vy */
extern s16 D_800A66D0[]; /* = g_site_level_ring_pos.vz */
extern s16 g_site_level_spinner_pos[];
extern s16 D_800A66D6[]; /* = g_site_level_spinner_pos.vy */
extern s16 D_800A66D8[]; /* = g_site_level_spinner_pos.vz */

extern GsRVIEW2 g_view_preset_map;
extern GsRVIEW2 g_view_preset_menu;
extern GsRVIEW2 g_view_preset_intro;
extern u32 *g_site_node_tmds[];
extern SVECTOR g_level_label_offsets[];
extern SVECTOR g_site_node_positions[];
extern GridRow g_site_grid[];
extern u8 g_gs_packet_area[][36000];
extern SVECTOR g_interp_deltas[];
extern GsCOORDINATE2 D_801E4B14; /* = g_models[5].coord (root of the node grid) */
extern Model g_site_node_models[];
extern Model g_site_level_models[];
extern GsFOGPARAM g_fog_param;
extern GsOT g_ot_2d[];
extern GsOT g_ot[];
extern GsRVIEW2 g_view;
extern MATRIX g_zero_matrix;

/* Grid models are numbered from 1 (slot 0 is unused). */
#define GRID_MODEL(n) (g_site_node_models[(n) + 1])

/* Evaluate a Bezier curve at t from binomial-scaled control points
 * (see bezier_prepare). */
void bezier_eval(s32 count, DVECTOR3 *pts, DVECTOR3 *out, double t) {
    DVECTOR3 tmp[12];
    s32 i;
    s32 n = count - 1;
    double tt;
    double s;
    double ss;

    tmp[0].x = pts[0].x;
    tmp[0].y = pts[0].y;
    tmp[0].z = pts[0].z;
    tt = t;
    for (i = 1; i <= n; i++) {
        tmp[i].x = pts[i].x * tt;
        tmp[i].y = pts[i].y * tt;
        tmp[i].z = pts[i].z * tt;
        tt = tt * t;
    }
    out->x = tmp[n].x;
    out->y = tmp[n].y;
    out->z = tmp[n].z;
    s = 1.0 - t;
    ss = s;
    for (i = n - 1; i >= 0; i--) {
        out->x += tmp[i].x * ss;
        out->y += tmp[i].y * ss;
        out->z += tmp[i].z * ss;
        ss = ss * s;
    }
}

/* Begin a frame: set the view, pick the packet area and clear both OTs. */
void gfx_frame_begin(void) {
    s32 buff;

    GsSetRefView2(&g_view);
    buff = GsGetActiveBuff();
    g_frame_buffer_index = buff;
    GsSetWorkBase(g_gs_packet_area[buff]);
    GsClearOt(0, 0, &g_ot[g_frame_buffer_index]);
    GsClearOt(0, 0, &g_ot_2d[g_frame_buffer_index]);
}

/* Sort every sub-object of a model into the given OT. */
void model_sort(Model *model, GsOT ot) {
    MATRIX ls;
    MATRIX lw;
    GsDOBJ2 *dobj;
    u32 i;

    dobj = &model->dobj;
    for (i = 0; i < model->nobj; i++, dobj++) {
        GsGetLws(dobj->coord2, &lw, &ls);
        GsSetLightMatrix(&lw);
        GsSetLsMatrix(&ls);
        GsSortObject4(dobj, &ot, 0, (u32 *)0x1F800000);
    }
}

/* End a frame: sort, wait for vsync, swap buffers and draw. */
void gfx_frame_end(void) {
    DrawSync(0);
    GsSortOt(&g_ot_2d[g_frame_buffer_index], &g_ot[g_frame_buffer_index]);
    DrawSync(0);
    VSync(0);
    ResetGraph(1);
    GsSwapDispBuff();
    GsSortClear(0, 0, 0, &g_ot[g_frame_buffer_index]);
    GsDrawOt(&g_ot[g_frame_buffer_index]);
}

void camera_init(void) {
    camera_set_view(3);
    GsSetNearClip(100);
    GsSetFarClip(1000);
}

/* Select one of the preset camera views. */
void camera_set_view(s32 view) {
    GsSetProjection(350);
    view &= 0xFF;
    switch (view) {
    case 2:
        g_view = g_view_preset_menu;
        g_camera_view = view;
        break;
    case 3:
        g_view = g_view_preset_map;
        g_camera_view = view;
        break;
    case 4:
        g_view = g_view_preset_intro;
        g_camera_view = view;
        break;
    default:
        g_view = g_view_preset_map;
        g_camera_view = 3;
        break;
    }
    GsSetRefView2(&g_view);
    D_800A6998 = 0xFF;
}

s32 light_fade_start(s32 arg0, s32 arg1) {
    if (g_light_fade_active == 0 || (g_light_fade_active == 1 && g_light_fade_index == arg0)) {
        g_light_fade_index = arg0;
        g_light_fade_target = arg1;
        g_light_fade_active = 1;
        return 0;
    }
    return 1;
}

void fog_disable(void) {
    GsSetLightMode(0);
}

/* Set up depth-cue fog with the given colour and enable it. */
void fog_enable(u8 r, u8 g, u8 b, s16 dqa) {
    GsFOGPARAM *fog = &g_fog_param;

    fog->rfc = r;
    fog->dqa = dqa;
    fog->dqb = 0x1400000;
    fog->gfc = g;
    fog->bfc = b;
    GsSetFogParam(fog);
    GsSetLightMode(1);
}

void model_reset_coord(Model *model) {
    GsInitCoordinate2(NULL, &model->coord);
    model->rot.vz = 0;
    model->rot.vy = 0;
    model->rot.vx = 0;
    model->coord.coord.t[2] = 0;
}

/* Rebuild a model's local matrix from its rotation, keeping the translation. */
void model_update_matrix(Model *model) {
    MATRIX mat;

    mat = g_zero_matrix;
    mat.t[0] = model->coord.coord.t[0];
    mat.t[1] = model->coord.coord.t[1];
    mat.t[2] = model->coord.coord.t[2];
    RotMatrix(&model->rot, &mat);
    model->coord.coord = mat;
    model->coord.flg = 0;
}

void rotcoord_update_matrix(RotCoord *rc) {
    MATRIX mat;

    mat = g_zero_matrix;
    mat.t[0] = rc->coord.coord.t[0];
    mat.t[1] = rc->coord.coord.t[1];
    mat.t[2] = rc->coord.coord.t[2];
    RotMatrix(&rc->rot, &mat);
    rc->coord.coord = mat;
    rc->coord.flg = 0;
}

void rotcoord_pair_update_matrix(RotCoordPair *pair) {
    MATRIX mat;

    mat = g_zero_matrix;
    mat.t[0] = pair->coord0.coord.t[0];
    mat.t[1] = pair->coord0.coord.t[1];
    mat.t[2] = pair->coord0.coord.t[2];
    RotMatrix(&pair->rot, &mat);
    pair->coord0.coord = mat;
    pair->coord0.flg = 0;

    mat = g_zero_matrix;
    mat.t[0] = pair->coord1.coord.t[0];
    mat.t[1] = pair->coord1.coord.t[1];
    mat.t[2] = pair->coord1.coord.t[2];
    RotMatrix(&pair->rot, &mat);
    pair->coord1.coord = mat;
    pair->coord1.flg = 0;
}

/* Upload a TIM image (and its CLUT, if any) to VRAM. */
void tim_upload(u32 *tim) {
    RECT rect;
    GsIMAGE image;

    GsGetTimInfo(tim + 1, &image);
    rect.x = image.px;
    rect.y = image.py;
    rect.w = image.pw;
    rect.h = image.ph;
    LoadImage(&rect, image.pixel);
    if ((image.pmode >> 3) & 1) {
        rect.x = image.cx;
        rect.y = image.cy;
        rect.w = image.cw;
        rect.h = image.ch;
        LoadImage(&rect, image.clut);
    }
}

/* Map a TMD file into memory and record its object count and data. */
void model_map_tmd(u32 *file, Model *model) {
    file++;
    GsMapModelingData(file);
    file++;
    model->nobj = *file++;
    model->tmd = file;
}

/* Link a model's sub-objects to its TMD data and coordinate system. */
void model_link(Model *model, GsCOORDINATE2 *super) {
    u32 i;
    u32 *tmd;
    GsDOBJ2 *dobj;

    model->id = g_dobj_id_next;
    tmd = model->tmd;
    for (i = 0; i < model->nobj; i++) {
        GsLinkObject4(tmd, &model->dobj, i);
    }
    g_dobj_id_next += i;
    for (i = 0, dobj = &model->dobj; i < model->nobj; i++) {
        dobj->coord2 = &model->coord;
        dobj->attribute = 0;
        dobj++;
    }
    if (super != NULL) {
        GsInitCoordinate2(super, &model->coord);
    }
}

void coord_attach(GsCOORDINATE2 *coord, GsCOORDINATE2 *super) {
    if (super != NULL) {
        GsInitCoordinate2(super, coord);
    }
}

void rotcoord_pair_attach(RotCoordPair *pair, GsCOORDINATE2 *super) {
    if (super != NULL) {
        GsInitCoordinate2(super, &pair->coord0);
        GsInitCoordinate2(super, &pair->coord1);
    }
}

void model_project_origin(Model *model, DVECTOR *screen) {
    MATRIX lw;
    MATRIX ls;
    s32 h;

    GsGetLws(&model->coord, &lw, &ls);
    model->coord.flg = 0;
    h = GsGetProjection();
    if (ls.t[2] == 0) {
        ls.t[2] = 40;
    }
    screen->vx = ls.t[0] * h / ls.t[2] + 160;
    screen->vy = ls.t[1] * h / ls.t[2] + 120;
}

void coord_project_origin(GsCOORDINATE2 *coord, DVECTOR *screen) {
    MATRIX lw;
    MATRIX ls;
    s32 h;

    GsGetLws(coord, &lw, &ls);
    coord->flg = 0;
    h = GsGetProjection();
    if (ls.t[2] == 0) {
        ls.t[2] = 40;
    }
    screen->vx = ls.t[0] * h / ls.t[2] + 160;
    screen->vy = ls.t[1] * h / ls.t[2] + 120;
}

void rotcoord_pair_project(RotCoordPair *pair, DVECTOR *screen0, DVECTOR *screen1) {
    MATRIX lw;
    MATRIX ls;
    s32 h;

    GsGetLws(&pair->coord0, &lw, &ls);
    pair->coord0.flg = 0;
    h = GsGetProjection();
    if (ls.t[2] == 0) {
        ls.t[2] = 40;
    }
    screen0->vx = ls.t[0] * h / ls.t[2] + 160;
    screen0->vy = ls.t[1] * h / ls.t[2] + 120;

    GsGetLws(&pair->coord1, &lw, &ls);
    pair->coord1.flg = 0;
    h = GsGetProjection();
    if (ls.t[2] == 0) {
        ls.t[2] = 40;
    }
    screen1->vx = ls.t[0] * h / ls.t[2] + 160;
    screen1->vy = ls.t[1] * h / ls.t[2] + 120;
}

typedef struct {
    SVECTOR *vecs0;
    s32 nVert;
    SVECTOR *vecs1;
} SVectorTables;

void tmd_set_vertex(s32 index, SVECTOR *src, SVectorTables *tables) {
    tables->vecs0[index] = *src;
}

void tmd_get_vertex(s32 index, SVECTOR *dst, SVectorTables *tables) {
    *dst = tables->vecs0[index];
}

void tmd_set_normal(s32 index, SVECTOR *src, SVectorTables *tables) {
    tables->vecs1[index] = *src;
}

void tmd_get_normal(s32 index, SVECTOR *dst, SVectorTables *tables) {
    *dst = tables->vecs1[index];
}

/* Scale 'count' control points by the binomial coefficients C(count-1, i). */
void bezier_prepare(s32 count, DVECTOR3 *in, DVECTOR3 *out) {
    s32 i;
    s32 n = count - 1;
    s32 coef;
    double c;

    for (i = 0; i <= n; i++) {
        if (i == 0) {
            coef = 1;
        } else if (i == 1) {
            coef = n;
        } else {
            coef = coef * (n - i + 1) / i;
        }
        c = coef;
        out[i].x = in[i].x * c;
        out[i].y = in[i].y * c;
        out[i].z = in[i].z * c;
    }
}

/* Set up an interpolation from 'from' to 'to' over 'steps' steps for 'count' vectors. */
void vec_interp_start(SVECTOR *to, SVECTOR *from, s16 steps, u32 count) {
    u32 i;

    for (i = 0; i < count; i++) {
        g_interp_deltas[i].vx = (to[i].vx - from[i].vx) / steps;
        g_interp_deltas[i].vy = (to[i].vy - from[i].vy) / steps;
        g_interp_deltas[i].vz = (to[i].vz - from[i].vz) / steps;
    }
    g_interp_steps_left = steps;
    g_interp_count = count;
}

/* Advance an interpolation by one step: add the per-step deltas to each vector. */
void vec_interp_step(SVECTOR *vecs) {
    s32 i;

    for (i = 0; i < g_interp_count; i++) {
        vecs[i].vx += g_interp_deltas[i].vx;
        vecs[i].vy += g_interp_deltas[i].vy;
        vecs[i].vz += g_interp_deltas[i].vz;
    }
    g_interp_steps_left--;
}

/* Returns 1 if the screen point lies outside the 320x240 screen grown by margin. */
s32 screen_point_offscreen(DVECTOR *pos, s16 margin) {
    s32 x = pos->vx;

    if (-margin < x && x < margin + 320 && -margin < pos->vy && pos->vy < margin + 240) {
        return 0;
    }
    return 1;
}

extern u32 g_level_ring_lof_tim;
extern u32 g_level_ring_text_tim;
extern u32 g_level_ring_line_tim;
extern u32 g_level_digits_tim;
extern u32 g_level_label_even_tens_tim;
extern u32 g_level_label_even_ones_tim;
extern u32 g_level_label_odd_tens_tim;
extern u32 g_level_label_odd_ones_tim;
extern u32 g_node_icon0_tim;
extern u32 g_node_icon1_tim;
extern u32 g_node_icon2_tim;
extern u32 g_node_icon3_tim;
extern u32 g_node_icon4_tim;
extern u32 g_node_icon5_tim;
extern u32 g_node_icon6_tim;
extern u32 g_node_icon7_tim;
extern u32 g_node_open_tim;
extern u32 g_node_cursor_tim;
extern u32 g_node_icon8_tim;
extern u32 g_node_icon9_tim;
extern u32 g_node_icon10_tim;
extern u32 g_node_icon11_tim;
extern u32 g_node_icon12_tim;
extern u32 g_node_icon13_tim;
extern u32 g_node_icon14_tim;
extern u32 g_node_icon15_tim;
extern u32 g_level_label_site_a_tim;
extern u32 g_level_label_site_b_tim;
extern u32 g_empty_tmd;
extern Model g_models[];
/* In the original these are addressed absolutely (lui/%lo), unlike the $gp
 * accesses elsewhere in this file; declared as arrays to get that. */
extern s16 g_current_site[];
extern s32 g_vsync_counter[];
extern s32 g_site_level[];
extern s32 g_ring_idle_morph_time[];
extern s32 g_ring_wave_decay_time[];
extern s32 g_site_tilted_up;
extern s32 g_site_cursor_cell;
extern s32 D_800A6A14; /* = g_site_cursor_cell + 4 (.col) */
extern s32 g_site_cam_path_t;
extern s32 D_800A6A24; /* = high word of the double g_site_cam_path_t */
extern s32 g_site_tilted_down;
extern s16 g_site_tilt_offset_y;
extern s32 g_site_action;
extern u8 g_ring_land_sfx_pending;
extern u8 g_site_node_opening;
extern u8 g_site_models_dirty;
extern s16 g_site_bg_offset_x;
void site_bg_init(void);
void site_effects_init(void);
void site_set_level_label(s32 arg0);
void srand(u32 seed);
void site_intro_cam_start(void);
void light_apply_preset(void);
void site_orb_init(void);
void site_node_model_create(u32 row, u32 col);
void site_level_models_place(s32 n, s32 row, s32 angle0, s32 angle1);
