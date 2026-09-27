#include "common.h"

/* Declarations carried over from 8002A344.c (same original headers). */

typedef struct {
    s16 vx, vy, vz, pad;
} Block8;

typedef struct {
    s16 x, y, w, h;
} RECT;

typedef struct {
    s16 vx, vy, vz, pad;
} SVECTOR;

typedef struct {
    s32 vx, vy, vz, pad;
} VECTOR;

typedef struct {
    s16 m[3][3];
    s32 t[3];
} MATRIX;

typedef struct {
    u32 pmode;
    s16 px, py;
    u16 pw, ph;
    u32 *pixel;
    s16 cx, cy;
    u16 cw, ch;
    u32 *clut;
} GsIMAGE;

typedef struct {
    s32 vx, vy, vz;
    u8 r, g, b;
} GsF_LIGHT;

typedef struct {
    u8 tag_addr[3];
    u8 tag_len;
    u8 r0, g0, b0, code;
    s16 x0, y0;
    u8 u0, v0;
    u16 clut;
    s16 x1, y1;
    u8 u1, v1;
    u16 tpage;
    s16 x2, y2;
    u8 u2, v2;
    u16 pad1;
    s16 x3, y3;
    u8 u3, v3;
    u16 pad2;
} POLY_FT4;

typedef struct {
    s16 vx, vy;
} DVECTOR;

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

/* 0x14 bytes */
typedef struct {
    u32 length;
    void *org;
    u32 offset;
    u32 point;
    void *tag;
} GsOT;

typedef struct {
    s32 w;
    s32 h;
    s32 x;
    s32 y;
} Struct801AA780;

/* 0xC-byte grid cell: object index and its projected screen position. */
typedef struct {
    s16 node;
    s16 texture;
    s32 index;
    s16 x;
    s16 y;
} GridEntry;

typedef struct {
    u32 row;
    u32 col;
} GridPos;

/* 0xA8-byte particle line object (g_site_open_lines). */
typedef struct {
    u8 unk0[0x18];
    VECTOR start;
    u8 unk28[0x40];
    VECTOR end;
    u8 unk78[0x30];
} LineObj;

/* 0x58-byte object (g_site_open_spark_coords). */
typedef struct {
    u8 unk0[0x18];
    VECTOR pos;
    u8 unk28[0x30];
} Obj58;

typedef struct {
    u32 attribute;
    s16 x0, y0;
    s16 x1, y1;
    u8 r0, g0, b0;
    u8 r1, g1, b1;
} GsGLINE;

/* 0x1C-byte descriptor copied from *D_801E8D28. */
typedef struct ModelInfo {
    s32 vert_top; /* PsyQ TMD object entry: vertex, normal, primitive tables + scale */
    u32 n_vert;
    s32 normal_top;
    u32 n_normal;
    s32 primitive_top;
    s32 n_primitive;
    s32 scale;
} ModelInfo;

/* 0xD4-byte entry initialised by streak_spawn. */
typedef struct {
    u8 unk0[0xD4];
} StructD4;

/* 0x74-byte object (model/coordinate block) in the g_models table. */
typedef struct {
    u32 flags;
    u8 unk4[0xC];
    u8 coord[0x18]; /* start of the GsCOORDINATE2 (x, y, z below are its coord.t) */
    s32 x;
    s32 y;
    s32 z;
    u8 unk34[0x2C];
    s16 tilt;
    s16 angle;
    s16 roll;
    u8 unk66[0x6];
    struct ModelInfo *model;
    u8 unk70[0x4];
} Obj74;

typedef struct {
    s32 vpx;
    s32 vpy;
    s32 vpz;
} Struct801EAEE0;

extern GsSPRITE g_site_bg_sprites[];
extern Struct801AA780 g_lain_anim_frame;
extern GsOT g_ot[];
extern Obj74 g_models[];
extern Struct801EAEE0 g_view;
extern void *g_ring_burst_tmds[];
extern u8 g_ring_tmd[];
extern u8 g_ring_tim[];
extern u8 g_orb_tim[];
extern u32 D_8008135C[]; /* = g_orb_tim + 4 */
extern Block8 g_ring_vertices[];
extern Block8 g_ring_shape_rest[];
extern Block8 g_ring_shape_idle[];
extern Block8 g_ring_shape_bend_up[];
extern Block8 g_ring_shape_bend_down[];
extern POLY_FT4 g_orb_poly;
extern GridEntry g_site_grid[][24];
extern Obj74 D_801E6054[]; /* = g_site_node_models[1] */
extern StructD4 g_streaks[];
extern u8 g_streak_lane_left0[];
extern u8 g_streak_lane_right0[];
extern u8 g_streak_lane_left1[];
extern u8 g_streak_lane_right1[];
extern u32 D_8007D57C[]; /* = g_ring_tim + 4 */
extern u32 D_8007D7BC[]; /* = g_level_digits_tim + 4 */
extern u32 D_8007DE6C[]; /* = g_level_label_even_ones_tim + 4 */
extern u32 D_8007E00C[]; /* = g_level_label_odd_ones_tim + 4 */
extern char g_level_number_fmt[];

extern double g_site_cam_path_t;
extern s16 g_site_bg_fade_level;
extern s16 g_site_bg_fade_timer;
extern s16 g_site_tilt_offset_y NO_GP;
extern s32 g_orb_fly_frames;
extern s32 g_orb_fly_grow;
extern s16 g_orb_fly_dx;
extern s16 g_orb_fly_dy;
extern SVECTOR g_orb_pos; /* position of the g_orb_poly sprite */
extern u8 g_streaks_ready;
extern s32 g_site_level_delta;
extern u16 g_scene_wait_frames;
extern s16 g_site_scroll_step;
extern s32 g_site_scroll_speed;
extern s32 g_site_scroll_accum;
extern u8 g_site_models_dirty;
extern u8 g_ring_land_sfx_pending NO_GP;
extern s32 g_site_rotation_delta;
extern s16 g_site_anim_step;
extern s32 g_site_rotation_angles[];
extern u8 g_site_band_tim[];
extern u32 D_8007AFBC[]; /* = g_site_band_tim + 4 (TIM data after the ID word) */
extern u8 D_80082578[];
extern GsSPRITE g_site_band_sprites[];
extern s32 D_800A6A98;
extern u8 D_800A6A9D;
extern GsF_LIGHT g_flat_lights[];
extern double g_site_open_path_t;
extern s32 g_site_open_anim;
extern u8 g_site_open_phase;
extern s32 g_site_open_timer;
extern s32 g_site_open_step;
extern u8 g_site_node_opening;
extern s32 g_saved_interp_steps_left;
extern s32 g_saved_morph_mode;
extern double g_site_open_path_len[];
extern Obj74 g_site_node_models[];
extern Block8 g_interp_deltas[];
extern Block8 g_saved_interp_delta[];
extern Block8 g_site_open_node_verts[];
extern Block8 g_site_open_shape_point[];
extern Block8 g_site_open_node_normals[];
extern ModelInfo *D_801E8D28[]; /* = g_site_node_models[99].model */
extern GridPos g_site_cursor_cell;
extern u32 *g_site_node_tims[];
extern u32 g_node_cursor_tim;
extern u32 D_800867C4[]; /* = g_node_cursor_tim + 4 */
extern u8 g_node_cursor_tmd[];
extern SVECTOR g_site_node_positions[];
extern GridPos g_site_open_cell;
extern u8 g_site_open_shards_on;
extern MATRIX g_zero_matrix;
extern u32 D_80086384[]; /* = g_node_open_tim + 4 */
extern u8 g_node_open_tmd[];
extern VECTOR g_site_open_path_mid[];
extern VECTOR g_site_open_path_end[];
extern Block8 g_site_open_shape_plate[];
extern Block8 g_site_open_shape_cube[];
extern VECTOR g_orb_orbit_point;
extern s16 D_800A6A80;
extern s16 D_800A6A82;
extern s16 D_800A6A84;
extern double g_bezier_points[];
extern u8 g_bezier_coeffs[];
extern s32 g_view_preset_map[];
extern s32 g_view_preset_menu[];
extern s32 g_view_path_mid_menu[];
extern s32 g_view_preset_intro[];
extern s32 g_view_path_mid_intro[];
extern u8 g_site_intro_cam_done;
extern s16 g_site_bg_offset_x;
extern s32 g_site_action NO_GP;
extern s32 g_ring_wave_phase;
extern s32 g_ring_burst_frame;
extern s32 g_ring_wave_shift;
extern s16 D_800A6A50;
extern s16 g_ring_wobble_angle;
extern s32 g_ring_wobble_state;
extern s32 g_ring_burst_stage;
extern s32 g_ring_burst_pulse;
extern s32 g_site_node_model_count;
extern s16 g_site_open_burst_pos[2];
extern s16 D_800A69CC; /* = z of g_site_open_burst_pos */
extern LineObj g_site_open_lines[];
extern Obj58 g_site_open_spark_coords[];
extern u8 g_node_open_tim[];
extern SVECTOR g_site_open_anim0_path[];
extern SVECTOR g_site_open_anim1_path[];
extern SVECTOR g_site_open_shard_steps[];
extern void *g_site_open_shard_tmds[];
extern Obj74 g_site_level_models[];
extern u32 g_ring_idle_morph_time;
extern u32 g_ring_wave_decay_time;
extern s32 g_ring_wave_table[];
extern s32 g_orb_angle;
extern DVECTOR g_orb_half_size;

u16 GetTPage(s32 tp, s32 abr, s32 x, s32 y);
u16 GetClut(s32 x, s32 y);
s32 ClearImage2(RECT *rect, s32 r, s32 g, s32 b);
void GsGetTimInfo(u32 *tim, GsIMAGE *image);
s32 MoveImage(RECT *rect, s32 x, s32 y);
s32 DrawSync(s32 mode);
s32 sprintf(char *buf, const char *fmt, ...);
void model_project_origin();
void model_update_matrix();
void streak_spawn_falling();
void bezier_eval(s32 count, void *vectors, double *out, double t);
void GsSetProjection(s32);
s32 GsSetRefView2(void *view);
void bezier_prepare(s32 count, double *vectors, void *out);
void SetSemiTrans(void *prim, s32 abe);
s32 rand(void);
void GsSortPoly(void *prim, GsOT *ot, u16 pri);
s32 GsSetFlatLight(s32 id, GsF_LIGHT *light);
void site_orb_init(void);
void site_ring_scroll_texture(void);
void site_streaks_spawn(void);
void GsSortFastSprite(GsSPRITE *sprite, GsOT *ot, u16 pri);
void streak_update_left();
void streak_update_right();
void streak_update_falling();
void tim_clut_fade_toward();
void rotcoord_pair_attach();
void rotcoord_pair_update_matrix();
void rotcoord_pair_project();
s32 screen_point_offscreen();
void GsSortGLine(GsGLINE *line, GsOT *ot, u16 pri);
void site_node_model_create(u32 row, u32 col);
void site_level_models_place(u32 group, u32 row, s16 a, s16 b);
void site_draw_cursor_node(void);
void GsGetLws(void *coord, MATRIX *lw, MATRIX *ls);
VECTOR *ApplyRotMatrix(SVECTOR *v0, VECTOR *v1);
void vec_interp_step(Block8 *);
void tmd_set_vertex(u32 index, Block8 *entry, ModelInfo *info);
void svector_copy(Block8 *dst, Block8 *src, s32 count);
void tmd_set_normal(u32 index, Block8 *entry, ModelInfo *info);
void model_sort(Obj74 *obj, GsOT ot);
void PushMatrix(void);
void PopMatrix(void);
MATRIX *RotMatrix(SVECTOR *r, MATRIX *m);
MATRIX *TransMatrix(MATRIX *m, VECTOR *v);
void SetRotMatrix(MATRIX *m);
void SetTransMatrix(MATRIX *m);
VECTOR *ApplyMatrix(MATRIX *m, SVECTOR *v0, VECTOR *v1);
void snd_play_sfx(s32);
void site_ring_morph_start(void *dst, u32 mode, s32 count);
void site_set_level_label(s32 number);
void GsSortSprite(GsSPRITE *sprite, GsOT *ot, u16 pri);
void streak_spawn();
void model_reset_coord();
void tim_upload();
void model_map_tmd();
void model_link();
void vec_interp_start(Block8 *src, void *dst, s16 count, s32 size);
void site_cam_path_step();
void site_ring_init();

/*
 * Small globals that the original code addresses with an absolute
 * `lui $at` macro sequence instead of via $gp. Marking the extern with an
 * explicit (non-small) section keeps cc1 emitting a symbolic reference while
 * suppressing the `.extern sym,size` hint, which reproduces that addressing.
 */
#define ABS_DATA __attribute__((section(".data")))

extern u8 g_camera_view ABS_DATA;
extern SVECTOR g_ring_home_pos ABS_DATA;
extern s32 g_morph_mode ABS_DATA;
extern s32 g_site_level ABS_DATA;
extern s32 g_ring_move_state;
extern s32 g_ring_ripple_mode;
extern s32 g_site_rotation ABS_DATA;
extern u8 D_800A6998 ABS_DATA;
extern s32 g_interp_steps_left ABS_DATA;
extern u16 g_clut_fade_busy ABS_DATA;
extern s16 g_node_select_anim_set ABS_DATA;
extern s16 g_text_window_busy ABS_DATA;
extern u32 g_vsync_counter ABS_DATA;
extern s16 g_ring_settle_up_y ABS_DATA;
extern s16 g_ring_settle_down_y ABS_DATA;
extern s32 g_site_tilted_up NO_GP;
extern s32 g_site_tilted_down NO_GP;
extern s32 g_frame_buffer_index ABS_DATA;
extern u32 D_8007DD9C ABS_DATA; /* = g_level_label_even_tens_tim + 4 */
extern u32 D_8007DF3C ABS_DATA; /* = g_level_label_odd_tens_tim + 4 */
extern s32 g_site_action_busy ABS_DATA;
extern s16 g_site_sel_col ABS_DATA;
extern s16 g_site_action_request ABS_DATA;
extern s16 g_site_sel_level ABS_DATA;

s32 site_get_level_count();
void light_apply_preset();
void camera_init();
void camera_set_view();
void site_scene_init();
void site_goto_step();
void site_cam_path_start();
void site_intro_cam_start();
void site_goto_run();
void site_jump_scroll_step();
void node_open_anim_approach();
void node_open_anim_effect();
void site_ring_burst_update();
void site_orb_fly_update();
void site_fade_background();
void site_intro_cam_update();
void light_fade_step();
void site_draw_node_map();
void site_project_nodes();
void site_orb_update();
void node_open_anim_start();
void site_ring_update();
void site_ring_burst_init();
void site_orb_fly_start();

#define ABS(x) ((x) < 0 ? -(x) : (x))


/*
 * Matches (0 diffs) when g_ring_move_state is addressed via $gp. This function belongs
 * to a different original TU than site_jump_scroll_step/site_goto_step, which address
 * g_ring_move_state absolutely; one C file cannot do both (see ABS_DATA above).
 */
void site_ring_init(void) {
    u8 unused[0x28]; /* unused local that only reserves stack space */

    g_ring_wave_shift = 4;
    g_ring_wave_phase = 0x80;
    g_ring_wobble_angle = 0;
    D_800A6A50 = 0;
    g_ring_wobble_state = 0;
    g_ring_move_state = 3;
    model_reset_coord(&g_models[7]);
    model_map_tmd(g_ring_tmd, &g_models[7]);
    model_link(&g_models[7], &g_models[1].coord);
    g_models[7].flags = 0xD0000000;
    g_models[7].x = g_ring_home_pos.vx;
    g_models[7].y = 900 + g_ring_home_pos.vy;
    g_models[7].z = -g_view.vpz + g_ring_home_pos.vz;
    tim_upload(g_ring_tim);
    svector_copy(g_ring_vertices, g_ring_shape_rest, 0x40);
}

void site_ring_burst_init(void) {
    u32 i;

    g_ring_burst_stage = 1;
    g_site_action_busy = 0xFF;
    g_ring_burst_frame = 0;
    for (i = 0; i < 32; i++) {
        model_reset_coord(&g_models[i + 8]);
        model_map_tmd(g_ring_burst_tmds[i], &g_models[i + 8]);
        model_link(&g_models[i + 8], &g_models[1].coord);
        g_models[i + 8].x = g_ring_home_pos.vx;
        g_models[i + 8].y = g_ring_home_pos.vy;
        g_models[i + 8].z = -g_view.vpz + g_ring_home_pos.vz;
        g_models[i + 8].flags = 0x50000000;
    }
}

/*
 * Different original TU (also needs .rodata: jump tables jtbl_800115D4 and
 * jtbl_800115FC). 0 diffs when g_site_action and g_ring_land_sfx_pending are declared ABS_DATA
 * and g_ring_move_state is a plain ($gp) extern, the opposite of the other TU here.
 */
/* Idle/scroll animation of the main object g_models[7]: movement states (g_ring_move_state), tilt/roll wobble (g_ring_wobble_state) and the vertex ripple effect. */
void site_ring_update(void) {
    ModelInfo info;
    s32 unused[2]; /* unused; only reserves stack space */
    Block8 vertA;
    Block8 vertB;
    u32 i;
    s16 noise;

    if (g_camera_view == 2 || g_camera_view == 4) {
        return;
    }
    g_models[7].flags &= 0x7FFFFFFF;
    site_ring_scroll_texture();
    if (g_vsync_counter - g_ring_idle_morph_time >= 3600) {
        g_ring_idle_morph_time = g_vsync_counter;
        if (g_morph_mode == 0 && (rand() & 0x3000) && g_site_action == 0 && g_ring_move_state == 0) {
            site_ring_morph_start(g_ring_vertices, 4, 20);
        }
    }
    switch (g_ring_move_state) {
    case 1:
        g_models[7].y += 100;
        model_update_matrix(&g_models[7]);
        break;
    case 3:
        if (g_ring_wobble_state == 0) {
            g_ring_wobble_state = 1;
        }
        if (g_ring_settle_up_y < g_models[7].y) {
            g_models[7].y -= 100;
            model_update_matrix(&g_models[7]);
        } else {
            g_ring_move_state = 5;
            if (g_ring_land_sfx_pending != 0) {
                snd_play_sfx(10);
                g_ring_land_sfx_pending = 0;
            }
        }
        break;
    case 5:
        if (g_models[7].y < g_ring_home_pos.vy) {
            g_models[7].y += 10;
            model_update_matrix(&g_models[7]);
        } else {
            g_ring_wobble_state = 5;
            g_ring_move_state = 0;
        }
        break;
    case 2:
        g_models[7].y -= 75;
        model_update_matrix(&g_models[7]);
        break;
    case 4:
        if (g_ring_wobble_state == 0) {
            g_ring_wobble_state = 2;
        }
        if (g_models[7].y < g_ring_settle_down_y) {
            g_models[7].y += 75;
            model_update_matrix(&g_models[7]);
        } else {
            g_ring_move_state = 6;
        }
        break;
    case 6:
        if (g_ring_home_pos.vy < g_models[7].y) {
            g_models[7].y -= 10;
            model_update_matrix(&g_models[7]);
        } else {
            g_ring_wobble_state = 5;
            g_ring_move_state = 0;
        }
        break;
    case 7:
        g_models[7].x += 6;
        g_models[7].y += 1;
        g_models[7].z += 6;
        model_update_matrix(&g_models[7]);
        break;
    case 9:
        if (g_ring_wobble_state == 0) {
            g_ring_wobble_state = 3;
        }
        if (g_ring_home_pos.vx < g_models[7].x) {
            g_ring_ripple_mode = 2;
            g_models[7].x -= 6;
            g_models[7].y -= 1;
            g_models[7].z -= 6;
            model_update_matrix(&g_models[7]);
        } else {
            g_ring_wobble_state = 6;
            g_ring_move_state = 0;
        }
        break;
    case 8:
        g_models[7].x -= 6;
        g_models[7].y += 1;
        g_models[7].z += 6;
        model_update_matrix(&g_models[7]);
        break;
    case 10:
        if (g_ring_wobble_state == 0) {
            g_ring_wobble_state = 4;
        }
        if (g_models[7].x < g_ring_home_pos.vx) {
            g_ring_ripple_mode = 2;
            g_models[7].x += 6;
            g_models[7].y -= 1;
            g_models[7].z -= 6;
            model_update_matrix(&g_models[7]);
        } else {
            g_ring_wobble_state = 6;
            g_ring_move_state = 0;
        }
        break;
    default:
        g_ring_move_state = 0;
        break;
    }
    switch (g_ring_wobble_state) {
    case 1:
        if (g_ring_wobble_angle < 8) {
            g_ring_wobble_angle += 2;
        } else {
            g_ring_wobble_state = 2;
        }
        g_models[7].tilt = (g_ring_wobble_angle << 12) / 360;
        model_update_matrix(&g_models[7]);
        break;
    case 2:
        if (g_ring_wobble_angle >= -7) {
            g_ring_wobble_angle -= 2;
        } else {
            g_ring_wobble_state = 1;
        }
        g_models[7].tilt = (g_ring_wobble_angle << 12) / 360;
        model_update_matrix(&g_models[7]);
        break;
    case 3:
        if (g_ring_wobble_angle >= -4) {
            g_ring_wobble_angle -= 1;
        } else {
            g_ring_wobble_state = 4;
        }
        g_models[7].roll = (g_ring_wobble_angle << 12) / 360;
        model_update_matrix(&g_models[7]);
        break;
    case 4:
        if (g_ring_wobble_angle < 5) {
            g_ring_wobble_angle += 1;
        } else {
            g_ring_wobble_state = 3;
        }
        g_models[7].roll = (g_ring_wobble_angle << 12) / 360;
        model_update_matrix(&g_models[7]);
        break;
    case 5:
        if (g_ring_wobble_angle > 0) {
            g_ring_wobble_angle--;
        } else if (g_ring_wobble_angle < 0) {
            g_ring_wobble_angle++;
        } else if (g_ring_wobble_angle == 0) {
            g_ring_wobble_state = 0;
            g_ring_ripple_mode = 0;
        }
        g_models[7].tilt = (g_ring_wobble_angle << 12) / 360;
        model_update_matrix(&g_models[7]);
        break;
    case 6:
        if (g_ring_wobble_angle > 0) {
            g_ring_wobble_angle--;
        } else if (g_ring_wobble_angle < 0) {
            g_ring_wobble_angle++;
        } else if (g_ring_wobble_angle == 0) {
            g_ring_wobble_state = 0;
            g_ring_ripple_mode = 0;
        }
        g_models[7].roll = (g_ring_wobble_angle << 12) / 360;
        model_update_matrix(&g_models[7]);
        break;
    default:
        g_ring_wobble_state = 0;
        break;
    }
    if (g_interp_steps_left > 0) {
        if (g_morph_mode != 5) {
            vec_interp_step(g_ring_vertices);
        }
    } else if (g_morph_mode == 4) {
        site_ring_morph_start(g_ring_vertices, 1, 20);
    } else {
        if (g_morph_mode == 1) {
            svector_copy(g_ring_vertices, g_ring_shape_rest, 0x40);
        }
        g_morph_mode = 0;
    }
    info = *g_models[7].model;
    g_ring_wave_phase++;
    for (i = 0; i < info.n_vert / 2; noise = rand() & 0x7F) {
        switch (g_ring_ripple_mode) {
        case 0: {
            s32 wave = (u8)g_ring_wave_table[((i + g_ring_wave_phase) >> 2) & 0x7FF] >> g_ring_wave_shift;

            vertA.vx = g_ring_vertices[i].vx;
            vertB.vx = g_ring_vertices[i + info.n_vert / 2].vx;
            vertA.vy = g_ring_vertices[i].vy + wave;
            vertB.vy = g_ring_vertices[i + info.n_vert / 2].vy + wave;
            vertA.vz = g_ring_vertices[i].vz;
            vertB.vz = g_ring_vertices[i + info.n_vert / 2].vz;
            break;
        }
        case 2: {
            s32 wave = (u8)g_ring_wave_table[(noise + g_ring_wave_phase) & 0x7FF] >> 4;

            vertA.vx = g_ring_vertices[i].vx + wave;
            vertB.vx = g_ring_vertices[i + info.n_vert / 2].vx + wave;
            vertA.vy = g_ring_vertices[i].vy + wave;
            vertB.vy = g_ring_vertices[i + info.n_vert / 2].vy + wave;
            vertA.vz = g_ring_vertices[i].vz + wave;
            vertB.vz = g_ring_vertices[i + info.n_vert / 2].vz + wave;
            break;
        }
        default:
            vertA.vx = g_ring_vertices[i].vx;
            vertB.vx = g_ring_vertices[i + info.n_vert / 2].vx;
            vertA.vy = g_ring_vertices[i].vy;
            vertB.vy = g_ring_vertices[i + info.n_vert / 2].vy;
            g_ring_wave_phase = 0x80;
            vertA.vz = g_ring_vertices[i].vz;
            vertB.vz = g_ring_vertices[i + info.n_vert / 2].vz;
            g_ring_wave_shift = 16;
            break;
        }
        tmd_set_vertex(i, &vertA, &info);
        tmd_set_vertex(i + info.n_vert / 2, &vertB, &info);
        i++;
    }
    if (g_ring_ripple_mode == 0) {
        if (g_ring_wave_shift >= 5 && g_vsync_counter - g_ring_wave_decay_time >= 30) {
            g_ring_wave_decay_time = g_vsync_counter;
            g_ring_wave_shift--;
        }
        if (g_ring_wave_phase > 0x580) {
            g_ring_wave_phase = 0x80;
        }
    }
    model_sort(&g_models[7], g_ot[g_frame_buffer_index]);
}

/*
 * Different original TU (also needs .rodata: jump table jtbl_80011614).
 * 0 diffs when g_site_action is declared ABS_DATA; functions from the other TU in
 * this file address it via $gp.
 */
/* Runs the 32-piece burst animation (stages g_ring_burst_stage = 1..7) on objects g_models[8..39]. */
void site_ring_burst_update(void) {
    MATRIX matrix;
    SVECTOR offset;
    SVECTOR angle;
    VECTOR move;
    s32 unused[4]; /* unused; only reserves stack space */
    u32 i;

    if (g_ring_burst_frame < 40) {
        site_ring_scroll_texture();
    }
    if (g_ring_burst_frame == 51) {
        snd_play_sfx(23);
    }
    for (i = 0; i < 32; i++) {
        g_models[i + 8].flags &= 0x7FFFFFFF;
        switch (g_ring_burst_stage) {
        case 1:
            if (g_ring_burst_frame < 15) {
                g_models[i + 8].y -= 18;
                model_update_matrix(&g_models[i + 8]);
            } else {
                g_ring_burst_stage = 2;
            }
            break;
        case 2:
            if (g_ring_burst_frame >= 18) {
                g_ring_burst_stage = 3;
                g_ring_burst_pulse = 0;
            }
            break;
        case 3:
            PushMatrix();
            angle.vy = i * 11.25 * 4096.0 / 360.0;
            angle.vx = 0;
            angle.vz = 0;
            RotMatrix(&angle, &matrix);
            SetRotMatrix(&matrix);
            offset.vx = 0;
            offset.vy = 0;
            offset.vz = 50;
            ApplyRotMatrix(&offset, &move);
            PopMatrix();
            if (g_ring_burst_pulse == 1) {
                g_models[i + 8].x += move.vx;
                g_models[i + 8].y += move.vy;
                g_models[i + 8].z += move.vz;
            } else if (g_ring_burst_pulse == 0xFF) {
                g_models[i + 8].x -= move.vx;
                g_models[i + 8].y -= move.vy;
                g_models[i + 8].z -= move.vz;
            }
            model_update_matrix(&g_models[i + 8]);
            break;
        case 4:
            if (g_ring_burst_frame >= 28) {
                g_ring_burst_stage = 5;
                g_ring_burst_pulse = 0;
            }
            break;
        case 5:
            PushMatrix();
            angle.vy = i * 11.25 * 4096.0 / 360.0;
            angle.vx = 0;
            angle.vz = 0;
            RotMatrix(&angle, &matrix);
            SetRotMatrix(&matrix);
            offset.vx = 0;
            offset.vy = 0;
            offset.vz = 50;
            ApplyRotMatrix(&offset, &move);
            PopMatrix();
            if (g_ring_burst_pulse == 1) {
                g_models[i + 8].x += move.vx;
                g_models[i + 8].y += move.vy;
                g_models[i + 8].z += move.vz;
            } else if (g_ring_burst_pulse == 0xFF) {
                g_models[i + 8].x -= move.vx;
                g_models[i + 8].y -= move.vy;
                g_models[i + 8].z -= move.vz;
            }
            model_update_matrix(&g_models[i + 8]);
            break;
        case 6:
            if (g_ring_burst_frame >= 40) {
                g_ring_burst_stage = 7;
                g_ring_wobble_angle = 0;
            }
            break;
        case 7:
            if (g_ring_burst_frame < 43) {
                if (g_ring_wobble_angle < 15) {
                    g_ring_wobble_angle = g_ring_wobble_angle + 0.5;
                }
                g_models[i + 8].tilt = (g_ring_wobble_angle << 12) / 360;
                model_update_matrix(&g_models[i + 8]);
            } else if (g_ring_burst_frame < 51) {
                PushMatrix();
                angle.vy = i * 11.25 * 4096.0 / 360.0;
                angle.vx = 0;
                angle.vz = 0;
                RotMatrix(&angle, &matrix);
                SetRotMatrix(&matrix);
                offset.vx = 0;
                offset.vy = 0;
                offset.vz = 100;
                ApplyRotMatrix(&offset, &move);
                PopMatrix();
                g_models[i + 8].x += move.vx;
                g_models[i + 8].y += move.vy;
                g_models[i + 8].z += move.vz;
                model_update_matrix(&g_models[i + 8]);
            } else {
                g_models[i + 8].flags |= 0x80000000;
                g_site_action = 0;
                g_site_action_request = 0;
                g_site_action_busy = 0;
                g_ring_burst_stage = 0;
                g_ring_burst_frame = 0;
            }
            break;
        default:
            g_site_action = 0;
            g_site_action_request = 0;
            g_site_action_busy = 0;
            break;
        }
        model_sort(&g_models[i + 8], g_ot[g_frame_buffer_index]);
    }
    if (g_ring_burst_stage != 0) {
        g_ring_burst_frame++;
    }
    if (g_ring_burst_stage == 3 || g_ring_burst_stage == 5) {
        if (g_ring_burst_pulse == 0) {
            g_ring_burst_pulse = 1;
        } else if (g_ring_burst_pulse == 1) {
            g_ring_burst_pulse = 0xFF;
        } else if (g_ring_burst_pulse == 0xFF) {
            if (g_ring_burst_stage == 3) {
                g_ring_burst_stage = 4;
            } else if (g_ring_burst_stage == 5) {
                g_ring_burst_stage = 6;
            }
        }
    }
    if (g_ring_burst_stage == 0) {
        site_ring_init();
    }
}

void site_ring_scroll_texture(void) {
    GsIMAGE image;
    RECT rect;

    GsGetTimInfo(D_8007D57C, &image);
    rect.x = image.px;
    rect.y = image.py;
    rect.w = image.pw;
    rect.h = image.ph;
    MoveImage(&rect, image.px + image.pw, image.py);
    DrawSync(0);
    rect.x = image.px + image.pw - 1;
    rect.y = image.py;
    rect.w = image.pw;
    rect.h = image.ph;
    MoveImage(&rect, image.px + image.pw, image.py);
    DrawSync(0);
    rect.x = image.px + image.pw;
    rect.y = image.py;
    rect.w = image.pw;
    rect.h = image.ph;
    MoveImage(&rect, image.px, image.py);
    DrawSync(0);
}

/* Copies `count` 8-byte blocks from src to dst. */
void svector_copy(Block8 *dst, Block8 *src, s32 count) {
    s32 i;

    for (i = 0; i < count; i++) {
        dst[i] = src[i];
    }
}

void site_ring_morph_start(void *dst, u32 mode, s32 count) {
    Block8 *table;

    switch (mode) {
    case 2:
        table = g_ring_shape_bend_up;
        break;
    case 3:
        table = g_ring_shape_bend_down;
        break;
    case 4:
        table = g_ring_shape_idle;
        break;
    case 1:
        table = g_ring_shape_rest;
        break;
    default:
        table = g_ring_shape_rest;
        count = 10;
        break;
    }
    g_morph_mode = mode;
    vec_interp_start(table, dst, count - 1, 0x40);
}

/* Loads the TIM at g_orb_tim and builds the full-size sprite g_orb_poly for it. */
void site_orb_init(void) {
    GsIMAGE image;
    POLY_FT4 *poly;

    tim_upload(g_orb_tim);
    GsGetTimInfo(D_8008135C, &image);
    poly = &g_orb_poly;
    poly->r0 = poly->g0 = poly->b0 = 0x80;
    poly->tpage = GetTPage(image.pmode, 1, image.px, image.py);
    poly->clut = GetClut(image.cx, image.cy);
    poly->x0 = 0;
    poly->y0 = 0;
    poly->x1 = 0;
    poly->y1 = 0;
    poly->x2 = 0;
    poly->y2 = 0;
    poly->x3 = 0;
    poly->y3 = 0;
    g_orb_angle = 0;
    g_orb_half_size.vx = image.pw - 1;
    g_orb_half_size.vy = (image.ph >> 1) - 1;
    poly->u0 = image.px;
    poly->v0 = image.py;
    poly->u1 = image.px + image.pw * 2 - 1;
    poly->v1 = image.py;
    poly->u2 = image.px;
    poly->v2 = image.py + image.ph - 1;
    poly->u3 = image.px + image.pw * 2 - 1;
    poly->v3 = image.py + image.ph - 1;
}

/*
 * Different original TU: g_site_action and g_site_tilt_offset_y are addressed absolutely
 * here but via $gp elsewhere in this file. Matches only with g_orb_pos as a
 * struct: the poly stores may then alias it and stay after its stores.
 */
/* Swings the sprite g_orb_poly and light 2 around a rotating orbit point (g_orb_orbit_point). */
void site_orb_update(void) {
    MATRIX matrix;
    MATRIX unused[2]; /* unused; only reserves stack space */
    SVECTOR point;
    SVECTOR angle;
    VECTOR pos;
    VECTOR trans;
    POLY_FT4 *poly;
    u32 shade;

    if (g_camera_view == 1 && g_site_action != 0x1C) {
        poly = &g_orb_poly;
        poly->tag_len = 9;
        poly->code = 0x2C;
        SetSemiTrans(poly, 1);
        shade = rand() & 0xFF;
        if (shade < 0x80) {
            shade = 0x80;
        }
        poly->r0 = poly->g0 = poly->b0 = shade;
        PushMatrix();
        angle.vx = (g_orb_angle << 12) / 360;
        angle.vy = 0;
        angle.vz = (g_orb_angle << 13) / 360;
        RotMatrix(&angle, &matrix);
        trans.vx = 0;
        trans.vy = 0;
        trans.vz = 0;
        TransMatrix(&matrix, &trans);
        SetRotMatrix(&matrix);
        SetTransMatrix(&matrix);
        point.vx = g_orb_orbit_point.vx;
        point.vy = g_orb_orbit_point.vy;
        point.vz = g_orb_orbit_point.vz;
        ApplyMatrix(&matrix, &point, &pos);
        PopMatrix();
        g_orb_angle += 2;
        if (g_orb_angle >= 360) {
            g_orb_angle = 0;
        }
        g_orb_pos.vx = pos.vx + 0xA0;
        g_orb_pos.vy = pos.vy + (s16)(g_site_tilt_offset_y + 0x78);
        g_orb_pos.vz = pos.vz;
        poly->x0 = g_orb_pos.vx - g_orb_half_size.vx;
        poly->y0 = g_orb_pos.vy - g_orb_half_size.vy;
        poly->x1 = g_orb_pos.vx + g_orb_half_size.vx;
        poly->y1 = g_orb_pos.vy - g_orb_half_size.vx;
        poly->x2 = g_orb_pos.vx - g_orb_half_size.vy;
        poly->y2 = g_orb_pos.vy + g_orb_half_size.vy;
        poly->x3 = g_orb_pos.vx + g_orb_half_size.vx;
        poly->y3 = g_orb_pos.vy + g_orb_half_size.vy;
        GsSortPoly(poly, &g_ot[g_frame_buffer_index], pos.vz + 0x15E);
        DrawSync(0);
        g_flat_lights[2].r = g_flat_lights[2].g = 0x30;
        g_flat_lights[2].vx = ~pos.vx;
        g_flat_lights[2].vy = ~pos.vy;
        g_flat_lights[2].vz = ~pos.vz;
        GsSetFlatLight(2, &g_flat_lights[2]);
    } else {
        g_orb_angle = 0;
        D_800A6A80 = g_orb_orbit_point.vx;
        D_800A6A82 = g_orb_orbit_point.vy;
        D_800A6A84 = g_orb_orbit_point.vz;
    }
}

/* g_site_action is absolute here (other TU). g_orb_pos and g_orb_half_size are structs. */
/* Animates the flickering sprite g_orb_poly toward its target and moves light 2 with it. */
void site_orb_fly_update(void) {
    POLY_FT4 *poly;
    u32 shade;
    s16 x;
    s16 y;
    s16 halfW;
    s16 halfH;

    poly = &g_orb_poly;
    poly->tag_len = 9;
    poly->code = 0x2C;
    SetSemiTrans(poly, 1);
    shade = rand() & 0xFF;
    if (shade < 0x80) {
        shade = 0x80;
    }
    poly->r0 = poly->g0 = poly->b0 = shade;
    if (g_orb_fly_frames > 0) {
        g_orb_pos.vz -= 4;
        g_orb_pos.vx += g_orb_fly_dx;
        g_orb_fly_grow += 4;
        g_orb_pos.vy += g_orb_fly_dy;
        x = g_orb_pos.vx;
        y = g_orb_pos.vy;
        halfW = g_orb_half_size.vx + g_orb_fly_grow;
        halfH = g_orb_half_size.vy + g_orb_fly_grow;
        poly->x0 = x - halfW;
        poly->y0 = y - halfH;
        poly->x1 = x + halfW;
        poly->y1 = y - halfW;
        poly->x2 = x - halfH;
        poly->y2 = y + halfH;
        poly->x3 = x + halfW;
        poly->y3 = y + halfH;
        GsSortPoly(poly, &g_ot[g_frame_buffer_index], g_orb_pos.vz + 0x15E);
        DrawSync(0);
        g_flat_lights[2].r = g_flat_lights[2].g = 0x20;
        g_flat_lights[2].vx = ~g_orb_pos.vx;
        g_flat_lights[2].vy = ~g_orb_pos.vy;
        g_flat_lights[2].vz = ~g_orb_pos.vz;
        GsSetFlatLight(2, &g_flat_lights[2]);
        g_orb_fly_frames--;
    } else {
        site_orb_init();
        g_site_action_busy = 0;
        g_site_action = 0;
        g_site_action_request = 0;
    }
}

void site_orb_fly_start(void) {
    s32 unused[8]; /* unused local that only reserves stack space */

    g_site_action_busy = 0xFF;
    g_orb_angle = 0;
    g_orb_fly_grow = 0;
    if (g_orb_pos.vz >= 0) {
        g_orb_pos.vz = 50;
        g_orb_fly_frames = 20;
    } else {
        g_orb_fly_frames = 15;
    }
    g_orb_fly_dx = (160 - g_orb_pos.vx) / g_orb_fly_frames;
    g_orb_fly_dy = (120 - g_orb_pos.vy) / g_orb_fly_frames;
}

/* Builds the ten font sprites from the TIM at g_site_band_tim and resets the text/object state. */
void site_effects_init(void) {
    GsIMAGE image;
    u32 i;
    s32 x;

    x = 0;
    tim_upload(g_site_band_tim);
    GsGetTimInfo(D_8007AFBC, &image);
    for (i = 0; i < 10; i++) {
        g_site_band_sprites[i].y = 0x58;
        g_site_band_sprites[i].attribute = 0;
        g_site_band_sprites[i].x = x;
        g_site_band_sprites[i].r = g_site_band_sprites[i].g = g_site_band_sprites[i].b = 0xC0;
        g_site_band_sprites[i].scalex = g_site_band_sprites[i].scaley = 1;
        g_site_band_sprites[i].rotate = 0;
        g_site_band_sprites[i].tpage = GetTPage(image.pmode, 0, image.px, image.py);
        g_site_band_sprites[i].u = 0;
        g_site_band_sprites[i].v = image.py;
        g_site_band_sprites[i].w = image.pw * 4;
        g_site_band_sprites[i].h = image.ph;
        g_site_band_sprites[i].cx = image.cx;
        g_site_band_sprites[i].cy = image.cy;
        x += image.pw * 4;
    }
    for (i = 0; i < 100; i++) {
        streak_spawn_falling(&g_streaks[i]);
    }
    model_reset_coord(&g_models[6]);
    model_map_tmd(D_80082578, &g_models[6]);
    model_link(&g_models[6], &g_models[1].coord);
    g_models[6].x = 0;
    g_models[6].y = 0;
    g_models[6].flags = 0xD0000000;
    g_models[6].z = -g_view.vpz;
    D_800A6A98 = 0;
    D_800A6A9D = 0;
    g_streaks_ready = 0;
}

void site_streaks_spawn(void) {
    u32 i;

    for (i = 0; i < 8; i++) {
        streak_spawn(&g_streaks[i], g_streak_lane_left0);
        streak_spawn(&g_streaks[i + 8], g_streak_lane_right0);
        streak_spawn(&g_streaks[i + 16], g_streak_lane_left1);
        streak_spawn(&g_streaks[i + 24], g_streak_lane_right1);
    }
    g_streaks_ready = 0xFF;
}

/*
 * Different original TU: matches (0 diffs) when g_site_action, g_site_tilt_offset_y,
 * g_site_tilted_up and g_site_tilted_down are declared ABS_DATA; other functions in this
 * file address them via $gp.
 */
/* Draws the ten font sprites and updates/draws the g_streaks particle objects. */
void site_effects_update(void) {
    s32 unused[2]; /* unused; only reserves stack space */
    u32 i;

    if (g_view.vpy >= -0xA8B || g_view.vpy == 0x80000000) {
        if (g_streaks_ready == 0) {
            site_streaks_spawn();
        } else {
            DrawSync(0);
            for (i = 0; i < 10; i++) {
                if (g_site_action == 10 || g_site_action == 11) {
                    g_site_band_sprites[i].y = g_site_tilt_offset_y + 0x58;
                } else if (g_site_action == 24 || g_site_action == 25 ||
                           (D_800A6998 != 0 && g_site_tilted_up == 0 && g_site_tilted_down == 0)) {
                    g_site_band_sprites[i].y = g_view.vpy / 4 + 0x58;
                }
                GsSortFastSprite(&g_site_band_sprites[i], &g_ot[g_frame_buffer_index], 0x41A);
            }
            DrawSync(0);
            for (i = 0; i < 8; i++) {
                streak_update_left(&g_streaks[i]);
                streak_update_right(&g_streaks[i + 8]);
                streak_update_left(&g_streaks[i + 16]);
                streak_update_right(&g_streaks[i + 24]);
            }
            g_models[6].angle += 45;
            model_update_matrix(&g_models[6]);
            g_models[6].flags &= 0x7FFFFFFF;
            model_sort(&g_models[6], g_ot[g_frame_buffer_index]);
        }
    }
    if (g_camera_view == 4 && g_view.vpy != 0x80000000) {
        DrawSync(0);
        if (g_view.vpy < -0xA8C) {
            for (i = 0; i < 100; i++) {
                streak_update_falling(&g_streaks[i]);
            }
        } else {
            for (i = 0; i < 50; i++) {
                streak_update_falling(&g_streaks[i + 32]);
            }
        }
    }
}
