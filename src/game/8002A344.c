#include "common.h"

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
extern s16 g_site_tilt_offset_y;
extern s32 g_orb_fly_frames;
extern s32 g_orb_fly_grow;
extern s16 g_orb_fly_dx;
extern s16 g_orb_fly_dy;
extern s16 g_orb_pos;
extern s16 D_800A6A92; /* = g_orb_pos.vy */
extern s16 D_800A6A94; /* = g_orb_pos.vz */
extern u8 g_streaks_ready;
extern s32 g_site_level_delta;
extern u16 g_scene_wait_frames;
extern s16 g_site_scroll_step;
extern s32 g_site_scroll_speed;
extern s32 g_site_scroll_accum;
extern u8 g_site_models_dirty;
extern u8 g_ring_land_sfx_pending;
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
extern s32 g_site_action;
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
extern s16 g_orb_half_size;
extern s16 D_800A6A72; /* = g_orb_half_size.vy */

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
extern s32 g_ring_move_state ABS_DATA;
extern s32 g_ring_ripple_mode ABS_DATA;
extern s32 g_site_rotation ABS_DATA;
extern u8 D_800A6998 ABS_DATA;
extern s32 g_interp_steps_left ABS_DATA;
extern u16 g_clut_fade_busy ABS_DATA;
extern s16 g_node_select_anim_set ABS_DATA;
extern s16 g_text_window_busy ABS_DATA;
extern u32 g_vsync_counter ABS_DATA;
extern s16 g_ring_settle_up_y ABS_DATA;
extern s16 g_ring_settle_down_y ABS_DATA;
extern s32 g_site_tilted_up;
extern s32 g_site_tilted_down;
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
 * Matches; needs .rodata for jump tables jtbl_80011508 and jtbl_80011558.
 */
/*
 * Per-frame update of the grid/menu scene: when idle (g_site_action == 0) starts
 * the action requested in g_site_action_request, otherwise steps the running action, then
 * updates the cursor, grid projection and idle animations.
 */
void site_scene_update(void) {
    u32 col;

    site_get_level_count();
    if (g_site_action_request == 0 && g_site_action != 0) {
        g_site_action = 0;
    }
    if (g_site_action == 0) {
        switch (g_site_action_request) {
        case 15:
            if (g_site_level < site_get_level_count() - 1) {
                g_site_action = 15;
                g_site_action_busy = 0xFF;
                g_site_models_dirty = 1;
                g_site_scroll_step = 0;
                g_scene_wait_frames = 0;
                g_ring_ripple_mode = 1;
            }
            break;
        case 16:
            if (g_site_level > 0) {
                g_site_action = 16;
                g_site_action_busy = 0xFF;
                g_site_models_dirty = 1;
                g_site_scroll_step = 0;
                g_scene_wait_frames = 0;
                g_ring_ripple_mode = 1;
            }
            break;
        case 12:
            g_site_action = 12;
            g_scene_wait_frames = 0;
            g_site_anim_step = 0;
            g_site_action_busy = 0xFF;
            break;
        case 13:
            g_site_action = 13;
            g_scene_wait_frames = 0;
            g_site_anim_step = 0;
            g_site_action_busy = 0xFF;
            break;
        case 10:
            if (g_site_tilted_up == 0) {
                g_site_action = 10;
                g_site_anim_step = 0;
                g_site_action_busy = 0xFF;
                if (g_site_tilted_down == 0) {
                    g_site_tilted_up = 1;
                }
            }
            break;
        case 11:
            if (g_site_tilted_down == 0) {
                g_site_action = 11;
                g_site_anim_step = 0;
                g_site_action_busy = 0xFF;
                if (g_site_tilted_up == 0) {
                    g_site_tilted_down = 1;
                }
            }
            break;
        case 22:
            g_site_action = 22;
            g_site_action_busy = 0xFF;
            if (g_site_sel_level >= site_get_level_count()) {
                g_site_sel_level = site_get_level_count() - 1;
            }
            if (g_site_sel_col >= 24) {
                g_site_sel_col = 23;
            }
            g_site_level_delta = g_site_sel_level - g_site_level;
            col = g_site_sel_col;
            while (col >= 9) {
                col -= 8;
            }
            g_site_scroll_accum = 0;
            g_site_rotation_delta = col - g_site_rotation;
            g_models[7].y = g_ring_home_pos.vy + 900;
            model_update_matrix(&g_models[7]);
            break;
        case 23:
            g_site_action = 23;
            g_site_action_busy = 0xFF;
            if (g_site_sel_level >= site_get_level_count()) {
                g_site_sel_level = site_get_level_count() - 1;
            }
            if (g_site_sel_col >= 24) {
                g_site_sel_col = 23;
            }
            g_site_level_delta = g_site_sel_level - g_site_level;
            col = g_site_sel_col;
            while (col >= 9) {
                col -= 8;
            }
            g_site_rotation_delta = col - g_site_rotation;
            if (g_site_level_delta > 0) {
                g_site_scroll_speed = ABS(g_site_level_delta) * 100;
            } else {
                g_site_scroll_speed = ABS(g_site_level_delta) * 75;
            }
            g_site_scroll_accum = 0;
            g_site_scroll_step = 0;
            g_scene_wait_frames = 0;
            g_ring_ripple_mode = 1;
            break;
        case 24:
            site_cam_path_start(2);
            g_site_action = 24;
            break;
        case 25:
            site_cam_path_start(3);
            g_site_action = 25;
            break;
        case 26:
            node_open_anim_start();
            g_site_action = 26;
            break;
        case 27:
            site_ring_update();
            site_ring_burst_init();
            g_site_action = 27;
            break;
        case 28:
            site_orb_update();
            site_orb_fly_start();
            g_site_action = 28;
            break;
        case 29:
            g_site_action = 29;
            g_site_action_busy = 0xFF;
            g_site_bg_fade_timer = 12;
            g_site_bg_fade_level = 0xFF;
            break;
        }
    } else if (g_site_action_request != 0) {
        switch (g_site_action) {
        case 15:
            if (g_scene_wait_frames >= 12) {
                if (g_site_scroll_step < 9) {
                    if (g_site_scroll_step == 0) {
                        snd_play_sfx(8);
                    } else if (g_site_scroll_step == 2) {
                        snd_play_sfx(9);
                    }
                    if (g_site_scroll_step == 0) {
                        site_ring_morph_start(g_ring_vertices, 2, 2);
                    }
                    site_set_level_label(g_site_level + 1);
                    g_ring_move_state = 1;
                    g_models[5].y += 100;
                    model_update_matrix(&g_models[5]);
                    g_site_scroll_step++;
                } else {
                    g_ring_move_state = 3;
                    g_site_scroll_step = 0;
                    g_scene_wait_frames = 0;
                    g_site_action = 0;
                    g_site_action_request = 0;
                    g_site_action_busy = 0;
                    g_site_level++;
                    if (g_site_level >= site_get_level_count()) {
                        g_site_level = site_get_level_count() - 1;
                    }
                    site_ring_morph_start(g_ring_vertices, 1, 11);
                    g_ring_land_sfx_pending = 1;
                    g_site_models_dirty = 1;
                }
            } else {
                if (g_scene_wait_frames == 1) {
                    site_ring_morph_start(g_ring_vertices, 3, 11);
                }
                g_scene_wait_frames++;
            }
            break;
        case 16:
            if (g_scene_wait_frames >= 13) {
                if (g_site_scroll_step < 12) {
                    if (g_site_scroll_step == 0) {
                        snd_play_sfx(8);
                    } else if (g_site_scroll_step == 2) {
                        snd_play_sfx(9);
                    }
                    if (g_site_scroll_step == 0) {
                        site_ring_morph_start(g_ring_vertices, 3, 12);
                    }
                    site_set_level_label(g_site_level - 1);
                    g_ring_move_state = 2;
                    g_models[5].y -= 75;
                    model_update_matrix(&g_models[5]);
                    g_site_scroll_step++;
                } else {
                    snd_play_sfx(10);
                    g_ring_move_state = 4;
                    g_site_scroll_step = 0;
                    g_scene_wait_frames = 0;
                    g_site_action = 0;
                    g_site_action_request = 0;
                    g_site_action_busy = 0;
                    g_site_level--;
                    if (g_site_level < 0) {
                        g_site_level = 0;
                    } else {
                        site_set_level_label(g_site_level - 1);
                    }
                    site_ring_morph_start(g_ring_vertices, 1, 12);
                    g_site_models_dirty = 1;
                }
            } else {
                g_scene_wait_frames++;
            }
            break;
        case 12:
            if (g_scene_wait_frames >= 10) {
                if (g_site_anim_step < 25) {
                    if (g_site_anim_step == 0 || g_site_anim_step == 7 || g_site_anim_step == 14 || g_site_anim_step == 21) {
                        snd_play_sfx(11);
                    }
                    g_ring_move_state = 7;
                    g_models[5].angle = g_models[5].angle - 19.342222222222222;
                    model_update_matrix(&g_models[5]);
                    g_site_anim_step++;
                } else {
                    g_ring_move_state = 9;
                    g_site_anim_step = 0;
                    g_scene_wait_frames = 0;
                    g_site_action_busy = 0;
                    g_site_action = 0;
                    g_site_action_request = 0;
                    g_site_rotation++;
                    if (g_site_rotation >= 8) {
                        g_site_rotation = 0;
                    }
                    g_models[5].angle = g_site_rotation_angles[g_site_rotation] * 512;
                    model_update_matrix(&g_models[5]);
                }
            } else {
                g_scene_wait_frames++;
            }
            break;
        case 13:
            if (g_scene_wait_frames >= 10) {
                if (g_site_anim_step < 26) {
                    if (g_site_anim_step == 0 || g_site_anim_step == 7 || g_site_anim_step == 14 || g_site_anim_step == 21) {
                        snd_play_sfx(11);
                    }
                    g_ring_move_state = 8;
                    g_models[5].angle = g_models[5].angle + 19.342222222222222;
                    model_update_matrix(&g_models[5]);
                    g_site_anim_step++;
                } else {
                    g_ring_move_state = 10;
                    g_site_anim_step = 0;
                    g_scene_wait_frames = 0;
                    g_site_action_busy = 0;
                    g_site_action = 0;
                    g_site_action_request = 0;
                    g_site_rotation--;
                    if (g_site_rotation < 0) {
                        g_site_rotation = 7;
                    }
                    g_models[5].angle = g_site_rotation_angles[g_site_rotation] * 512;
                    model_update_matrix(&g_models[5]);
                }
            } else {
                g_scene_wait_frames++;
            }
            break;
        case 10:
            if (g_site_anim_step < 6) {
                g_models[1].tilt -= 34;
                model_update_matrix(&g_models[1]);
                g_site_anim_step += 3;
                g_site_tilt_offset_y += 21;
            } else {
                g_site_anim_step = 0;
                g_site_action_busy = 0;
                g_site_tilted_down = 0;
                g_site_action = 0;
                g_site_action_request = 0;
            }
            break;
        case 11:
            if (g_site_anim_step < 6) {
                g_models[1].tilt += 34;
                model_update_matrix(&g_models[1]);
                g_site_anim_step += 3;
                g_site_tilt_offset_y -= 21;
            } else {
                g_site_anim_step = 0;
                g_site_action_busy = 0;
                g_site_tilted_up = 0;
                g_site_action = 0;
                g_site_action_request = 0;
            }
            break;
        case 22:
            site_goto_run();
            break;
        case 23:
            site_jump_scroll_step();
            break;
        case 24:
        case 25:
            site_cam_path_step();
            break;
        case 26:
            if (g_site_open_phase == 0) {
                node_open_anim_approach();
            } else {
                node_open_anim_effect();
            }
            break;
        case 27:
            site_ring_burst_update();
            break;
        case 28:
            site_orb_fly_update();
            break;
        case 29:
            site_fade_background();
            break;
        default:
            g_site_action = 0;
            break;
        }
    }
    site_intro_cam_update();
    light_fade_step();
    site_draw_node_map();
    site_project_nodes();
    site_orb_update();
    if (g_camera_view == 1 && g_site_action != 0x1B) {
        site_ring_update();
    }
}

/* Scrolls the grid by one row per 900 units of g_site_scroll_speed in the direction of g_site_level_delta. */
void site_jump_scroll_step(void) {
    if (g_site_level_delta > 0) {
        if (g_scene_wait_frames >= 12) {
            if (g_site_scroll_step == 0) {
                snd_play_sfx(8);
            } else if (g_site_scroll_step == 2) {
                snd_play_sfx(9);
            }
            if (g_site_scroll_step == 0) {
                site_ring_morph_start(g_ring_vertices, 2, 2);
            }
            g_models[5].y += g_site_scroll_speed;
            g_site_scroll_accum += g_site_scroll_speed;
            model_update_matrix(&g_models[5]);
            g_ring_move_state = 1;
            g_site_scroll_step++;
            if (g_site_scroll_accum >= 900) {
                site_set_level_label(g_site_level + 1);
                g_site_models_dirty = 1;
                g_site_scroll_accum -= 900;
                g_site_level++;
                g_site_level_delta--;
                if (g_site_level >= site_get_level_count()) {
                    g_site_level = site_get_level_count() - 1;
                    g_site_level_delta = 0;
                }
            }
        } else {
            if (g_scene_wait_frames == 1) {
                site_ring_morph_start(g_ring_vertices, 3, 11);
            }
            g_scene_wait_frames++;
        }
        if (g_site_level_delta == 0) {
            g_ring_move_state = 3;
            site_ring_morph_start(g_ring_vertices, 1, 11);
            g_ring_land_sfx_pending = 1;
        }
    } else if (g_site_level_delta < 0) {
        if (g_scene_wait_frames >= 13) {
            if (g_site_scroll_step == 0) {
                snd_play_sfx(8);
            } else if (g_site_scroll_step == 2) {
                snd_play_sfx(9);
            }
            if (g_site_scroll_step == 0) {
                site_ring_morph_start(g_ring_vertices, 3, 12);
            }
            g_models[5].y -= g_site_scroll_speed;
            g_site_scroll_accum += g_site_scroll_speed;
            model_update_matrix(&g_models[5]);
            g_ring_move_state = 2;
            g_site_scroll_step++;
            if (g_site_scroll_accum >= 900) {
                g_site_scroll_accum -= 900;
                g_site_models_dirty = 1;
                g_site_level--;
                g_site_level_delta++;
                if (g_site_level < 0) {
                    g_site_level = 0;
                    g_site_level_delta = 0;
                } else {
                    site_set_level_label(g_site_level - 1);
                }
            }
        } else {
            g_scene_wait_frames++;
        }
        if (g_site_level_delta == 0) {
            snd_play_sfx(10);
            g_ring_move_state = 4;
            site_ring_morph_start(g_ring_vertices, 1, 12);
        }
    } else {
        g_scene_wait_frames = 0;
        g_site_scroll_step = 0;
        g_site_action_busy = 0;
        g_site_action = 0;
        g_site_action_request = 0;
    }
}

/*
 * Per-frame step of the grid animation: scroll by rows (g_site_level_delta) or
 * rotate by 1/8 turns (g_site_rotation_delta), then reset the state when both are idle.
 */
void site_goto_step(void) {
    if (g_site_level_delta > 0) {
        g_models[5].y += 100;
        g_site_scroll_accum += 100;
        model_update_matrix(&g_models[5]);
        site_set_level_label(g_site_level + 1);
        if (g_site_scroll_accum >= 900) {
            g_site_scroll_accum -= 900;
            g_site_models_dirty = 1;
            g_site_level++;
            g_site_level_delta--;
            if (g_site_level >= site_get_level_count()) {
                g_site_level = site_get_level_count() - 1;
                g_site_level_delta = 0;
            }
        }
    } else if (g_site_level_delta < 0) {
        g_models[5].y -= 100;
        g_site_scroll_accum += 100;
        model_update_matrix(&g_models[5]);
        if (g_site_scroll_accum >= 900) {
            g_site_scroll_accum -= 900;
            g_site_models_dirty = 1;
            g_site_level--;
            g_site_level_delta++;
            if (g_site_level < 0) {
                g_site_level = 0;
                g_site_level_delta = 0;
            } else {
                site_set_level_label(g_site_level - 1);
            }
        }
    } else if (g_site_rotation_delta > 0) {
        g_models[5].angle = g_models[5].angle - 19.342222222222222;
        model_update_matrix(&g_models[5]);
        g_site_anim_step++;
        if (g_site_anim_step >= 25) {
            g_site_anim_step = 0;
            g_site_rotation++;
            g_site_rotation_delta--;
            if (g_site_rotation >= 8) {
                g_site_rotation = 0;
            }
            g_models[5].angle = g_site_rotation_angles[g_site_rotation] * 512;
            model_update_matrix(&g_models[5]);
        }
    } else if (g_site_rotation_delta < 0) {
        g_models[5].angle = g_models[5].angle + 19.342222222222222;
        model_update_matrix(&g_models[5]);
        g_site_anim_step++;
        if (g_site_anim_step >= 26) {
            g_site_anim_step = 0;
            g_site_rotation--;
            g_site_rotation_delta++;
            if (g_site_rotation < 0) {
                g_site_rotation = 7;
            }
            g_models[5].angle = g_site_rotation_angles[g_site_rotation] * 512;
            model_update_matrix(&g_models[5]);
        }
    } else {
        g_ring_move_state = 3;
        g_site_action_busy = 0;
        g_site_action = 0;
        g_site_action_request = 0;
    }
}

/* Draws the four quarter sprites of g_site_bg_sprites around the g_lain_anim_frame window. */
void site_draw_background(void) {
    g_site_bg_sprites[0].x = (s16)(g_site_bg_offset_x + 0x94) - (s16)g_lain_anim_frame.x;
    g_site_bg_sprites[0].y = (s16)(g_site_tilt_offset_y + 0x130) - (s16)g_lain_anim_frame.y;
    if (g_lain_anim_frame.w > 0x100) {
        g_site_bg_sprites[0].w = 0x100;
    } else {
        g_site_bg_sprites[0].w = g_lain_anim_frame.w;
    }
    if (g_lain_anim_frame.h > 0x100) {
        g_site_bg_sprites[0].h = 0x100;
    } else {
        g_site_bg_sprites[0].h = g_lain_anim_frame.h;
    }
    GsSortSprite(&g_site_bg_sprites[0], &g_ot[g_frame_buffer_index], 0x15E);
    DrawSync(0);
    g_site_bg_sprites[1].w = 0x40;
    g_site_bg_sprites[1].x = g_site_bg_sprites[0].x + g_site_bg_sprites[0].w;
    g_site_bg_sprites[1].y = (s16)(g_site_tilt_offset_y + 0x130) - (s16)g_lain_anim_frame.y;
    g_site_bg_sprites[1].h = g_site_bg_sprites[0].h;
    GsSortSprite(&g_site_bg_sprites[1], &g_ot[g_frame_buffer_index], 0x15E);
    DrawSync(0);
    g_site_bg_sprites[2].x = (s16)(g_site_bg_offset_x + 0x94) - (s16)g_lain_anim_frame.x;
    g_site_bg_sprites[2].y = g_site_bg_sprites[0].y + g_site_bg_sprites[0].h;
    g_site_bg_sprites[2].w = g_site_bg_sprites[0].w;
    g_site_bg_sprites[2].h = 0x1E0 - g_site_bg_sprites[0].h;
    GsSortSprite(&g_site_bg_sprites[2], &g_ot[g_frame_buffer_index], 0x15E);
    DrawSync(0);
    g_site_bg_sprites[3].w = 0x40;
    g_site_bg_sprites[3].x = g_site_bg_sprites[0].x + g_site_bg_sprites[0].w;
    g_site_bg_sprites[3].y = g_site_bg_sprites[0].y + g_site_bg_sprites[0].h;
    g_site_bg_sprites[3].h = 0x1E0 - g_site_bg_sprites[0].h;
    GsSortSprite(&g_site_bg_sprites[3], &g_ot[g_frame_buffer_index], 0x15E);
    DrawSync(0);
}

/* Fades the colour of the four g_site_bg_sprites sprites; resets state once finished. */
void site_fade_background(void) {
    u8 level;

    if (g_site_bg_fade_timer != 0) {
        g_site_bg_fade_timer--;
        level = g_site_bg_fade_level;
        g_site_bg_sprites[0].r = g_site_bg_sprites[0].g = g_site_bg_sprites[0].b = level;
        g_site_bg_sprites[1].r = g_site_bg_sprites[1].g = g_site_bg_sprites[1].b = level;
        g_site_bg_sprites[2].r = g_site_bg_sprites[2].g = g_site_bg_sprites[2].b = level;
        g_site_bg_sprites[3].r = g_site_bg_sprites[3].g = g_site_bg_sprites[3].b = level;
        g_site_bg_fade_level -= 10;
    } else if (g_site_tilt_offset_y <= 0) {
        g_site_bg_sprites[0].r = g_site_bg_sprites[0].g = g_site_bg_sprites[0].b = 0x80;
        g_site_bg_sprites[1].r = g_site_bg_sprites[1].g = g_site_bg_sprites[1].b = 0x80;
        g_site_bg_sprites[2].r = g_site_bg_sprites[2].g = g_site_bg_sprites[2].b = 0x80;
        g_site_bg_sprites[3].r = g_site_bg_sprites[3].g = g_site_bg_sprites[3].b = 0x80;
        g_site_bg_fade_timer = 0;
        g_site_action_busy = 0;
        g_site_action = 0;
        g_site_action_request = 0;
    }
}

/* Projects a 3-row window of the 24-column grid around row g_site_level to screen space. */
void site_project_nodes(void) {
    DVECTOR screen;
    s32 count;
    s32 row;
    u32 i;
    u32 j;

    count = site_get_level_count();
    row = g_site_level;
    if (row == count - 1) {
        row = count - 3;
    } else if (row > 0) {
        row--;
    }
    for (i = 0; i < 3; i++) {
        for (j = 0; j < 24; j++) {
            model_project_origin(&D_801E6054[g_site_grid[row][j].index], &screen);
            g_site_grid[row][j].x = screen.vx;
            g_site_grid[row][j].y = screen.vy;
        }
        row++;
    }
}

/*
 * Tooling only: g_site_open_cell ({row, col}) is accessed as %gp_rel(g_site_open_cell+4);
 * maspsx in ASPSX 2.56 mode refuses $gp offsets on .comm symbols. 0 diffs with
 * PROBE_ASPSX=2.79.
 */
/*
 * Draws the visible part of the grid: the row above/below the current window
 * when scrolling (g_site_action = 16/15), the three visible rows with their row
 * markers (g_site_level_models), and the cursor objects.
 */
void site_draw_node_map(void) {
    s16 angles[3][2];
    u32 group;
    s32 row;
    u32 col;
    u32 n;
    s32 index;

    if (g_site_level >= site_get_level_count() - 1) {
        row = g_site_level - 2;
    } else {
        row = g_site_level;
        if (row > 0) {
            row--;
        }
    }
    group = 0;
    for (n = 0; n < 3; n++) {
        angles[n][0] = g_site_level_models[n + 1].angle;
        angles[n][1] = g_site_level_models[n + 5].angle;
    }
    if (g_site_models_dirty == 1) {
        g_site_node_model_count = 0;
    }
    if (row != 0) {
        if (g_site_action == 16) {
            for (col = 0; col < 24; col++) {
                if (g_site_models_dirty == 1) {
                    site_node_model_create(row - 1, col);
                }
                if (g_site_grid[row - 1][col].node != -1) {
                    index = g_site_grid[row - 1][col].index;
                    if (g_site_node_opening == 0 || g_site_open_cell.row != row || g_site_open_cell.col != col) {
                        g_site_node_models[index + 1].flags &= 0x7FFFFFFF;
                        model_sort(&g_site_node_models[index + 1], g_ot[g_frame_buffer_index]);
                    } else {
                        g_site_node_models[index + 1].flags |= 0x80000000;
                    }
                }
            }
            if (g_site_models_dirty == 1) {
                site_level_models_place(group, row - 1, 0, 0);
            }
            g_site_level_models[group + 1].flags &= 0x7FFFFFFF;
            model_sort(&g_site_level_models[group + 1], g_ot[g_frame_buffer_index]);
            g_site_level_models[group + 5].flags &= 0x7FFFFFFF;
            model_sort(&g_site_level_models[group + 5], g_ot[g_frame_buffer_index]);
            g_site_level_models[group + 9].flags &= 0x7FFFFFFF;
            model_sort(&g_site_level_models[group + 9], g_ot[g_frame_buffer_index]);
            g_site_level_models[group + 13].flags &= 0x7FFFFFFF;
            model_sort(&g_site_level_models[group + 13], g_ot[g_frame_buffer_index]);
            g_site_level_models[group + 17].flags &= 0x7FFFFFFF;
            model_sort(&g_site_level_models[group + 17], g_ot[g_frame_buffer_index]);
            group++;
        } else {
            for (col = 0; col < 24; col++) {
                if (g_site_grid[row - 1][col].node != -1) {
                    index = g_site_grid[row - 1][col].index;
                    g_site_node_models[index + 1].flags |= 0x80000000;
                }
            }
        }
    }
    for (n = 0; n < 3; n++) {
        for (col = 0; col < 24; col++) {
            if (g_site_models_dirty == 1) {
                site_node_model_create(row, col);
            }
            if (g_site_grid[row][col].node != -1) {
                index = g_site_grid[row][col].index;
                if (g_site_node_opening == 0 || g_site_open_cell.row != row || g_site_open_cell.col != col) {
                    if (row == g_site_sel_level && col == g_site_sel_col && g_text_window_busy == 0) {
                        site_draw_cursor_node();
                    } else {
                        g_site_node_models[index + 1].flags &= 0x7FFFFFFF;
                        model_sort(&g_site_node_models[index + 1], g_ot[g_frame_buffer_index]);
                    }
                } else {
                    g_site_node_models[index + 1].flags |= 0x80000000;
                }
            }
        }
        if (g_site_models_dirty == 1) {
            site_level_models_place(group, row, angles[n][0], angles[n][1]);
        }
        g_site_level_models[group + 1].flags &= 0x7FFFFFFF;
        model_sort(&g_site_level_models[group + 1], g_ot[g_frame_buffer_index]);
        g_site_level_models[group + 5].angle -= 22;
        model_update_matrix(&g_site_level_models[group + 5]);
        g_site_level_models[group + 5].flags &= 0x7FFFFFFF;
        model_sort(&g_site_level_models[group + 5], g_ot[g_frame_buffer_index]);
        row++;
        g_site_level_models[group + 9].flags &= 0x7FFFFFFF;
        model_sort(&g_site_level_models[group + 9], g_ot[g_frame_buffer_index]);
        g_site_level_models[group + 13].flags &= 0x7FFFFFFF;
        model_sort(&g_site_level_models[group + 13], g_ot[g_frame_buffer_index]);
        g_site_level_models[group + 17].flags &= 0x7FFFFFFF;
        model_sort(&g_site_level_models[group + 17], g_ot[g_frame_buffer_index]);
        group++;
    }
    if ((u32)row < site_get_level_count()) {
        if (g_site_action == 15) {
            for (col = 0; col < 24; col++) {
                if (g_site_models_dirty == 1) {
                    site_node_model_create(row, col);
                }
                if (g_site_grid[row][col].node != -1) {
                    index = g_site_grid[row][col].index;
                    if (g_site_node_opening == 0 || g_site_open_cell.row != row || g_site_open_cell.col != col) {
                        g_site_node_models[index + 1].flags &= 0x7FFFFFFF;
                        model_sort(&g_site_node_models[index + 1], g_ot[g_frame_buffer_index]);
                    } else {
                        g_site_node_models[index + 1].flags |= 0x80000000;
                    }
                }
            }
            if (g_site_models_dirty == 1) {
                site_level_models_place(group, row, 0, 0);
            }
            g_site_level_models[group + 1].flags &= 0x7FFFFFFF;
            model_sort(&g_site_level_models[group + 1], g_ot[g_frame_buffer_index]);
            g_site_level_models[group + 5].flags &= 0x7FFFFFFF;
            model_sort(&g_site_level_models[group + 5], g_ot[g_frame_buffer_index]);
            g_site_level_models[group + 9].flags &= 0x7FFFFFFF;
            model_sort(&g_site_level_models[group + 9], g_ot[g_frame_buffer_index]);
            g_site_level_models[group + 13].flags &= 0x7FFFFFFF;
            model_sort(&g_site_level_models[group + 13], g_ot[g_frame_buffer_index]);
            g_site_level_models[group + 17].flags &= 0x7FFFFFFF;
            model_sort(&g_site_level_models[group + 17], g_ot[g_frame_buffer_index]);
        } else {
            for (col = 0; col < 24; col++) {
                if (g_site_grid[row][col].node != -1) {
                    index = g_site_grid[row][col].index;
                    g_site_node_models[index + 1].flags |= 0x80000000;
                }
            }
        }
    }
    if (g_site_node_opening != 0 || g_site_open_phase != 0) {
        model_sort(&g_site_node_models[99], g_ot[g_frame_buffer_index]);
        if (g_site_open_shards_on != 0) {
            model_sort(&g_site_node_models[100], g_ot[g_frame_buffer_index]);
            model_sort(&g_site_node_models[101], g_ot[g_frame_buffer_index]);
            model_sort(&g_site_node_models[102], g_ot[g_frame_buffer_index]);
        }
    }
    g_site_models_dirty = 0;
}

/* Formats number+1 and blits its two digit glyphs from the font TIMs into VRAM. */
void site_set_level_label(s32 number) {
    char text[8];
    RECT rect;
    RECT unusedRect;     /* unused; only reserves stack space */
    GsIMAGE base;
    GsIMAGE font;
    s32 unused[8];       /* unused; only reserves stack space */
    RECT *r;

    sprintf(text, g_level_number_fmt, number + 1);
    GsGetTimInfo(D_8007D7BC, &base);
    if (number & 1) {
        GsGetTimInfo(D_8007E00C, &font);
        r = &rect;
        rect.x = base.px + (u8)(text[1] - '0') * font.pw;
        rect.y = base.py;
        rect.w = font.pw;
        rect.h = font.ph;
        MoveImage(r, font.px, font.py);
        DrawSync(0);
        GsGetTimInfo(&D_8007DF3C, &font);
    } else {
        GsGetTimInfo(D_8007DE6C, &font);
        r = &rect;
        rect.x = base.px + (u8)(text[1] - '0') * font.pw;
        rect.y = base.py;
        rect.w = font.pw;
        rect.h = font.ph;
        MoveImage(r, font.px, font.py);
        DrawSync(0);
        GsGetTimInfo(&D_8007DD9C, &font);
    }
    rect.x = base.px + (u8)(text[0] - '0') * font.pw;
    rect.y = base.py;
    rect.w = font.pw;
    rect.h = font.ph;
    MoveImage(r, font.px, font.py);
    DrawSync(0);
}

/*
 * Tooling only: the three stores to the double g_site_cam_path_t emit
 * %gp_rel(g_site_cam_path_t+4) for the high word, which maspsx (ASPSX 2.56 mode)
 * refuses on the .comm symbol from extern_sdata.py (38 diffs from that).
 * Matches with PROBE_ASPSX=2.79 (which allows $gp offsets).
 */
/* Switches the camera/light preset (mode 2, 3 or 4) by loading three vectors into g_bezier_points. */
void site_cam_path_start(u32 mode) {
    u32 current;

    switch (mode) {
    case 2:
        if (g_camera_view == mode) {
            goto reset;
        }
        g_bezier_points[0] = g_view_preset_map[0];
        g_bezier_points[1] = g_view_preset_map[1];
        g_bezier_points[2] = g_view_preset_map[2];
        g_bezier_points[3] = g_view_path_mid_menu[0];
        g_bezier_points[4] = g_view_path_mid_menu[1];
        g_bezier_points[5] = g_view_path_mid_menu[2];
        g_bezier_points[6] = g_view_preset_menu[0];
        g_bezier_points[7] = g_view_preset_menu[1];
        g_bezier_points[8] = g_view_preset_menu[2];
        bezier_prepare(3, g_bezier_points, g_bezier_coeffs);
        g_site_action_busy = 0xFF;
        g_camera_view = mode;
        break;
    case 3:
        if (g_camera_view == mode) {
            goto reset;
        }
        g_bezier_points[0] = g_view_preset_menu[0];
        g_bezier_points[1] = g_view_preset_menu[1];
        g_bezier_points[2] = g_view_preset_menu[2];
        g_bezier_points[3] = g_view_path_mid_menu[0];
        g_bezier_points[4] = g_view_path_mid_menu[1];
        g_bezier_points[5] = g_view_path_mid_menu[2];
        g_bezier_points[6] = g_view_preset_map[0];
        g_bezier_points[7] = g_view_preset_map[1];
        g_bezier_points[8] = g_view_preset_map[2];
        bezier_prepare(3, g_bezier_points, g_bezier_coeffs);
        g_site_cam_path_t = 0.0;
        g_site_action_busy = 0xFF;
        g_camera_view = mode;
        break;
    case 4:
        current = g_camera_view;
        if (current != mode) {
            goto reset;
        }
        g_bezier_points[0] = g_view_preset_intro[0];
        g_bezier_points[1] = g_view_preset_intro[1];
        g_bezier_points[2] = g_view_preset_intro[2];
        g_bezier_points[3] = g_view_path_mid_intro[0];
        g_bezier_points[4] = g_view_path_mid_intro[1];
        g_bezier_points[5] = g_view_path_mid_intro[2];
        g_bezier_points[6] = g_view_preset_map[0];
        g_bezier_points[7] = g_view_preset_map[1];
        g_bezier_points[8] = g_view_preset_map[2];
        bezier_prepare(3, g_bezier_points, g_bezier_coeffs);
        g_site_cam_path_t = 0.0;
        g_site_action_busy = 0xFF;
        g_site_intro_cam_done = 0;
        g_camera_view = current;
        break;
    reset:
        g_site_action = 0;
        g_site_action_request = 0;
        g_site_action_busy = 0;
        break;
    }
    g_site_cam_path_t = 0.0;
}

/*
 * Tooling only (29 diffs): g_site_cam_path_t is a double whose high word is
 * %gp_rel(g_site_cam_path_t+4); maspsx in ASPSX 2.56 mode refuses $gp offsets on the
 * .comm symbol extern_sdata.py creates. Matches with PROBE_ASPSX=2.79.
 */
/* Moves the view point along the preset path (g_site_cam_path_t = 0..50) and finishes the preset change. */
void site_cam_path_step(void) {
    double viewPoint[3];

    if (g_site_cam_path_t <= 50.0) {
        bezier_eval(3, g_bezier_coeffs, viewPoint, g_site_cam_path_t / 50.0);
        g_view.vpx = viewPoint[0];
        g_view.vpy = viewPoint[1];
        g_view.vpz = viewPoint[2];
        GsSetProjection(0x15E);
        GsSetRefView2(&g_view);
        g_site_cam_path_t += 1.0;
    } else {
        g_site_cam_path_t = 0.0;
        g_site_action = 0;
        g_site_action_request = 0;
        light_apply_preset();
        if (g_camera_view == 3) {
            site_ring_init();
            g_camera_view = 1;
        }
        if (g_camera_view != 4) {
            g_site_action_busy = 0;
        } else {
            g_site_intro_cam_done = 0xFF;
        }
    }
}

/*
 * Tooling only: g_site_cursor_cell is an 8-byte {row, col} pair whose second word is
 * %gp_rel(g_site_cursor_cell+4); maspsx in ASPSX 2.56 mode refuses $gp offsets on
 * .comm symbols. 0 diffs with PROBE_ASPSX=2.79.
 */
/* Moves the grid cursor to (g_site_sel_level, g_site_sel_col): highlights the cell, places and rotates the cursor object, and loads the cell's texture. */
void site_draw_cursor_node(void) {
    GsIMAGE image;
    RECT rect;
    s32 unused[2]; /* unused; only reserves stack space */
    u32 prev[2];
    u32 col;
    s32 index;

    prev[0] = g_site_cursor_cell.row;
    prev[1] = g_site_cursor_cell.col;
    g_site_cursor_cell.row = g_site_sel_level;
    g_site_cursor_cell.col = g_site_sel_col;
    if (prev[0] == g_site_cursor_cell.row && prev[1] == g_site_cursor_cell.col) {
        tim_clut_fade_toward(&g_node_cursor_tim, g_site_node_tims[g_site_grid[prev[0]][prev[1]].texture]);
    } else {
        tim_upload(&g_node_cursor_tim);
    }
    if (g_clut_fade_busy == 0) {
        tim_upload(&g_node_cursor_tim);
    }
    if (g_site_cursor_cell.row != g_site_sel_level || g_site_cursor_cell.col != g_site_sel_col) {
        tim_upload(&g_node_cursor_tim);
    }
    index = g_site_grid[g_site_cursor_cell.row][g_site_cursor_cell.col].index;
    g_site_node_models[index + 1].flags |= 0x80000000;
    model_reset_coord(&g_site_node_models[98]);
    model_map_tmd(g_node_cursor_tmd, &g_site_node_models[98]);
    model_link(&g_site_node_models[98], &g_models[5].coord);
    col = g_site_cursor_cell.col;
    g_site_node_models[98].x = g_site_node_positions[col].vx;
    g_site_node_models[98].y = g_site_node_positions[col].vy + 100 - g_site_cursor_cell.row * 900;
    g_site_node_models[98].z = g_site_node_positions[col].vz;
    g_site_node_models[98].angle = g_site_node_models[98].angle +
        ((col * 45 + 22.5) - g_site_cursor_cell.col * 360) * 4096.0 / 360.0;
    g_site_node_models[98].flags = 0x50000000;
    model_update_matrix(&g_site_node_models[98]);
    GsGetTimInfo(g_site_node_tims[g_site_grid[g_site_cursor_cell.row][g_site_cursor_cell.col].texture] + 1, &image);
    rect.x = image.px;
    rect.y = image.py;
    rect.w = image.pw;
    rect.h = image.ph;
    GsGetTimInfo(D_800867C4, &image);
    MoveImage(&rect, image.px, image.py);
    DrawSync(0);
    model_sort(&g_site_node_models[98], g_ot[g_frame_buffer_index]);
}

/*
 * Tooling only: g_site_open_cell ({row, col}) and the double g_site_open_path_t are
 * accessed as %gp_rel(sym+4); maspsx in ASPSX 2.56 mode refuses $gp offsets on
 * .comm symbols. 0 diffs with PROBE_ASPSX=2.79.
 */
/* Enters the selected grid cell: moves the camera object there, loads its textures and starts the camera path. */
void node_open_anim_start(void) {
    GsIMAGE image;
    RECT rect;
    RECT clutRect;
    MATRIX lw;
    MATRIX ls;
    MATRIX rotation;
    SVECTOR in;
    SVECTOR angle;
    s32 unused1[2]; /* unused; only reserves stack space */
    VECTOR out;
    s32 unused2[4]; /* unused; only reserves stack space */
    ModelInfo info;
    u32 col;
    s32 index;
    u32 i;

    rotation = g_zero_matrix;
    g_site_open_shards_on = 0;
    g_site_open_cell.row = g_site_sel_level;
    g_site_open_cell.col = g_site_sel_col;
    g_site_open_anim = g_node_select_anim_set;
    if (g_site_grid[g_site_open_cell.row][g_site_open_cell.col].node != -1) {
        DrawSync(0);
        index = g_site_grid[g_site_open_cell.row][g_site_open_cell.col].index;
        GsGetLws(&g_site_node_models[index + 1].coord, &lw, &ls);
        g_site_node_models[index + 1].flags |= 0x80000000;
        model_reset_coord(&g_site_node_models[99]);
        model_map_tmd(g_node_open_tmd, &g_site_node_models[99]);
        model_link(&g_site_node_models[99], &g_models[5].coord);
        col = g_site_open_cell.col;
        g_site_node_models[99].x = g_site_node_positions[col].vx;
        g_site_node_models[99].y = g_site_node_positions[col].vy + 100 - g_site_open_cell.row * 900;
        g_site_node_models[99].z = g_site_node_positions[col].vz;
        g_site_node_models[99].angle = g_site_node_models[99].angle +
            ((col * 45 + 22.5) - g_site_open_cell.col * 360) * 4096.0 / 360.0;
        g_site_node_models[99].flags = 0x50000000;
        model_update_matrix(&g_site_node_models[99]);
        GsGetTimInfo(D_800867C4, &image);
        rect.x = image.px;
        rect.y = image.py;
        rect.w = image.pw;
        rect.h = image.ph;
        clutRect.x = image.cx;
        clutRect.y = image.cy;
        clutRect.w = image.cw;
        clutRect.h = image.ch;
        GsGetTimInfo(D_80086384, &image);
        MoveImage(&rect, image.px, image.py);
        DrawSync(0);
        MoveImage(&clutRect, image.cx, image.cy);
        DrawSync(0);
        PushMatrix();
        angle.vx = 0;
        angle.vz = 0;
        angle.vy = ((g_site_rotation * 45 - g_site_rotation * 360) << 12) / 360;
        RotMatrix(&angle, &rotation);
        SetRotMatrix(&rotation);
        in.vx = lw.t[0];
        in.vy = lw.t[1];
        in.vz = lw.t[2];
        ApplyRotMatrix(&in, &out);
        g_bezier_points[0] = out.vx;
        g_bezier_points[1] = (double)out.vy - g_site_level * 900;
        g_bezier_points[2] = out.vz;
        in.vx = g_site_open_path_mid[g_site_open_anim].vx;
        in.vy = g_site_open_path_mid[g_site_open_anim].vy;
        in.vz = g_site_open_path_mid[g_site_open_anim].vz;
        ApplyRotMatrix(&in, &out);
        g_bezier_points[3] = out.vx;
        g_bezier_points[4] = (double)out.vy - g_site_level * 900;
        g_bezier_points[5] = out.vz;
        in.vx = g_site_open_path_end[g_site_open_anim].vx;
        in.vy = g_site_open_path_end[g_site_open_anim].vy;
        in.vz = g_site_open_path_end[g_site_open_anim].vz;
        ApplyRotMatrix(&in, &out);
        g_bezier_points[6] = out.vx;
        g_bezier_points[7] = (double)out.vy - g_site_level * 900;
        g_bezier_points[8] = out.vz;
        PopMatrix();
        bezier_prepare(3, g_bezier_points, g_bezier_coeffs);
        g_saved_interp_delta[0] = g_interp_deltas[0];
        info = *g_site_node_models[99].model;
        g_saved_interp_steps_left = g_interp_steps_left;
        g_saved_morph_mode = g_morph_mode;
        for (i = 0; i < info.n_vert; i++) {
            g_site_open_node_verts[i] = g_site_open_shape_plate[i];
            tmd_set_vertex(i, &g_site_open_shape_plate[i], &info);
        }
        g_morph_mode = 5;
        vec_interp_start(g_site_open_shape_cube, g_site_open_node_verts, (s32)g_site_open_path_len[g_site_open_anim] - 1, 8);
        g_site_open_path_t = 0.0;
        g_site_action_busy = 0xFF;
        g_site_node_opening = 0xFF;
        g_site_open_phase = 0;
    } else {
        g_site_action_busy = 0;
        g_site_action = 0;
        g_site_action_request = 0;
    }
}

/*
 * Tooling only (also needs .rodata: switch jump table jtbl_800115A8).
 * g_site_open_path_t is a double whose high word is %gp_rel(g_site_open_path_t+4); maspsx in
 * ASPSX 2.56 mode refuses $gp offsets on .comm symbols. 0 diffs with
 * PROBE_ASPSX=2.79.
 */
/* Advances the camera along the current path segment; at its end, selects the next scene step. */
void node_open_anim_approach(void) {
    double pos[3];
    s32 unused[2]; /* unused; only reserves stack space */
    ModelInfo info;
    u32 i;

    DrawSync(0);
    if (g_site_open_path_t <= g_site_open_path_len[g_site_open_anim]) {
        if (g_interp_steps_left > 0 && g_morph_mode == 5) {
            vec_interp_step(g_site_open_node_verts);
            info = *D_801E8D28[0];
            for (i = 0; i < info.n_vert; i++) {
                tmd_set_vertex(i, &g_site_open_node_verts[i], &info);
            }
        }
        bezier_eval(3, g_bezier_coeffs, pos, g_site_open_path_t / g_site_open_path_len[g_site_open_anim]);
        g_site_node_models[99].x = pos[0];
        g_site_node_models[99].y = pos[1];
        g_site_node_models[99].z = pos[2];
        g_site_node_models[99].angle = g_site_rotation * 512;
        model_update_matrix(&g_site_node_models[99]);
        g_site_open_path_t += 1.0;
    } else {
        g_interp_steps_left = g_saved_interp_steps_left;
        g_morph_mode = g_saved_morph_mode;
        g_interp_deltas[0] = g_saved_interp_delta[0];
        switch (g_site_open_anim) {
        case 0:
            g_site_open_phase = 0xFF;
            g_site_open_timer = 0x15;
            g_site_open_step = 10;
            snd_play_sfx(12);
            break;
        case 1:
            g_site_open_phase = 0xFF;
            g_site_open_timer = 0x1F;
            g_site_open_step = 15;
            snd_play_sfx(12);
            g_saved_interp_delta[0] = g_interp_deltas[0];
            g_saved_interp_steps_left = g_interp_steps_left;
            g_saved_morph_mode = g_morph_mode;
            g_morph_mode = 5;
            vec_interp_start(g_site_open_shape_point, g_site_open_node_verts, 4, 8);
            break;
        case 2:
            g_site_open_phase = 0xFF;
            g_site_open_timer = 0x19;
            break;
        case 3:
            g_site_open_phase = 0xFF;
            g_site_open_timer = 0x16;
            g_site_open_step = 0;
            break;
        case 4:
            g_site_open_phase = 0xFF;
            g_site_open_timer = 0x3F;
            break;
        default:
            g_site_action_busy = 0;
            g_site_action = 0;
            g_site_action_request = 0;
            g_site_open_phase = 0;
            g_site_node_opening = 0;
            break;
        }
        info = *D_801E8D28[0];
        for (i = 0; i < info.n_normal; i++) {
            tmd_set_normal(i, &g_site_open_node_normals[i], &info);
        }
    }
}

#ifdef NON_MATCHING
/*
 * Also needs .rodata (jump table jtbl_800115C0). g_site_open_cell and g_site_open_burst_pos
 * are accessed as %gp_rel(sym+off), which maspsx in ASPSX 2.56 mode refuses
 * for .comm symbols. With PROBE_ASPSX=2.79: 13 diffs, all from the loop counter
 * reset (i = 0) in case 2 being placed at the loop instead of in the delay slot
 * of the tim_upload call. In the original, `i = 0` comes right before the jal,
 * and DrawSync(0) still uses $zero. Here combine merges i = 0 into the duplicated
 * loop-entry test (a 3-insn combine that leaves the set next to the branch). With
 * `if (count2) do ... while` instead, sched1 moves the set down into the lw delay.
 * An asm barrier plus the do-while gets to 2 diffs (i = 0 after the call, and
 * DrawSync gets `move a0,s2`), so the original probably has a real block boundary
 * here. Moving i = 0 before or after the call, while forms and
 * `do {} while (0)` do not help. decomp-permuter (20 min) found nothing better.
 */
/*
 * Runs one step of the scene-transition camera sequence selected by
 * g_site_open_anim (0-4), counting g_site_open_timer down to 0; includes the particle
 * line bursts (g_site_open_lines) and the final camera path.
 */
void node_open_anim_effect(void) {
    GsIMAGE image;
    RECT rect;
    MATRIX lw;
    MATRIX ls;
    MATRIX rotation;
    SVECTOR in;
    SVECTOR angle;
    SVECTOR sparks[20];
    VECTOR out;
    s32 unused[4]; /* unused; only reserves stack space */
    double pos[3];
    GsGLINE line;
    ModelInfo info;
    s32 i;

    DrawSync(0);
    switch (g_site_open_anim) {
    case 0:
        PushMatrix();
        angle.vx = 0;
        angle.vz = 0;
        angle.vy = ((g_site_rotation * 45 - g_site_rotation * 360) << 12) / 360;
        RotMatrix(&angle, &rotation);
        SetRotMatrix(&rotation);
        if (g_site_open_step != 0) {
            in.vx = g_site_open_anim0_path[10 - g_site_open_step].vx;
            in.vy = g_site_open_anim0_path[10 - g_site_open_step].vy;
            in.vz = g_site_open_anim0_path[10 - g_site_open_step].vz;
            ApplyRotMatrix(&in, &out);
            g_site_node_models[99].x = out.vx;
            g_site_node_models[99].y = out.vy - g_site_level * 900;
            g_site_node_models[99].z = out.vz;
            PopMatrix();
            g_site_open_step--;
        } else {
            in.vx = 20;
            in.vy = 0;
            in.vz = 150;
            ApplyRotMatrix(&in, &out);
            g_site_node_models[99].x -= out.vx;
            g_site_node_models[99].z -= out.vz;
            PopMatrix();
            g_site_node_models[99].angle += 45;
            g_site_node_models[99].roll += 45;
        }
        model_update_matrix(&g_site_node_models[99]);
        if (--g_site_open_timer == 11) {
            snd_play_sfx(13);
        }
        if (g_site_open_timer == 9) {
            snd_play_sfx(14);
        }
        if (g_site_open_timer == 0) {
            g_site_action_busy = 0;
            g_site_action = 0;
            g_site_action_request = 0;
            g_site_open_phase = 0;
        }
        break;
    case 1:
        if (g_site_open_step != 0) {
            PushMatrix();
            angle.vx = 0;
            angle.vz = 0;
            angle.vy = ((g_site_rotation * 45 - g_site_rotation * 360) << 12) / 360;
            RotMatrix(&angle, &rotation);
            SetRotMatrix(&rotation);
            in.vx = g_site_open_anim1_path[15 - g_site_open_step].vx;
            in.vy = g_site_open_anim1_path[15 - g_site_open_step].vy;
            in.vz = g_site_open_anim1_path[15 - g_site_open_step].vz;
            ApplyRotMatrix(&in, &out);
            g_site_node_models[99].x = out.vx;
            g_site_node_models[99].y = out.vy - g_site_level * 900;
            g_site_node_models[99].z = out.vz;
            PopMatrix();
            g_site_node_models[99].angle += 45;
            g_site_node_models[99].roll += 45;
            g_site_open_step--;
            model_update_matrix(&g_site_node_models[99]);
        }
        if (g_site_open_step < 5 && g_interp_steps_left > 0 && g_morph_mode == 5) {
            vec_interp_step(g_site_open_node_verts);
            info = *D_801E8D28[0];
            for (i = 0; i < info.n_vert; i++) {
                tmd_set_vertex(i, &g_site_open_node_verts[i], &info);
            }
        } else if (g_interp_steps_left == 0 && g_morph_mode == 5) {
            g_interp_steps_left = g_saved_interp_steps_left;
            g_morph_mode = g_saved_morph_mode;
            g_interp_deltas[0] = g_saved_interp_delta[0];
        }
        if (g_site_open_timer == 13) {
            snd_play_sfx(15);
            GsGetLws(&g_site_node_models[99].coord, &lw, &ls);
            angle.vx = 0;
            angle.vz = 0;
            g_site_open_burst_pos[0] = lw.t[0];
            g_site_open_burst_pos[1] = lw.t[1];
            D_800A69CC = lw.t[2];
            angle.vy = ((g_site_rotation * 45 - g_site_rotation * 360) << 12) / 360;
            PushMatrix();
            RotMatrix(&angle, &rotation);
            SetRotMatrix(&rotation);
            for (i = 0; i < 4; i++) {
                model_reset_coord(&g_site_node_models[99 + i]);
                model_map_tmd(g_site_open_shard_tmds[i], &g_site_node_models[99 + i]);
                model_link(&g_site_node_models[99 + i], &g_models[5].coord);
                in.vx = g_site_open_anim1_path[14].vx;
                in.vy = g_site_open_anim1_path[14].vy;
                in.vz = g_site_open_anim1_path[14].vz;
                ApplyRotMatrix(&in, &out);
                g_site_node_models[99 + i].x = out.vx;
                g_site_node_models[99 + i].y = out.vy - g_site_level * 900;
                g_site_node_models[99 + i].z = out.vz;
                model_update_matrix(&g_site_node_models[99 + i]);
                g_site_open_shards_on = 0xFF;
            }
            PopMatrix();
        } else if (g_site_open_timer < 13) {
            if (g_site_open_timer == 11) {
                snd_play_sfx(16);
            }
            angle.vx = 0;
            angle.vz = 0;
            angle.vy = ((g_site_rotation * 45 - g_site_rotation * 360) << 12) / 360;
            PushMatrix();
            RotMatrix(&angle, &rotation);
            SetRotMatrix(&rotation);
            for (i = 0; i < 4; i++) {
                in.vx = g_site_open_shard_steps[i].vx;
                in.vy = g_site_open_shard_steps[i].vy;
                in.vz = g_site_open_shard_steps[i].vz;
                ApplyRotMatrix(&in, &out);
                g_site_node_models[99 + i].x -= out.vx;
                g_site_node_models[99 + i].z -= out.vz;
                g_site_node_models[99 + i].angle += 45;
                g_site_node_models[99 + i].roll += 45;
                model_update_matrix(&g_site_node_models[99 + i]);
            }
            PopMatrix();
        }
        if (g_site_open_timer == 11 || g_site_open_timer == 12) {
            for (i = 0; i < 20; i++) {
                rotcoord_pair_attach(&g_site_open_lines[i], &g_models[5].coord);
                PushMatrix();
                angle.vx = 0;
                angle.vz = 0;
                angle.vy = ((g_site_rotation * 45 - g_site_rotation * 360) << 12) / 360;
                RotMatrix(&angle, &rotation);
                SetRotMatrix(&rotation);
                in.vx = g_site_open_burst_pos[0];
                in.vy = g_site_open_burst_pos[1] - g_site_level * 900;
                in.vz = D_800A69CC;
                ApplyRotMatrix(&in, &out);
                g_site_open_lines[i].start.vx = out.vx;
                g_site_open_lines[i].start.vy = out.vy;
                g_site_open_lines[i].start.vz = out.vz;
                in.vx = (g_site_open_burst_pos[0] - g_ring_wave_table[rand() & 0x7FF]) >> 8;
                in.vy = ((g_site_open_burst_pos[1] - g_ring_wave_table[rand() & 0x7FF]) >> 8) - g_site_level * 900;
                in.vz = (D_800A69CC - g_ring_wave_table[rand() & 0x7FF]) >> 8;
                ApplyRotMatrix(&in, &out);
                g_site_open_lines[i].end.vx = out.vx;
                g_site_open_lines[i].end.vy = out.vy;
                g_site_open_lines[i].end.vz = out.vz;
                PopMatrix();
                line.attribute = 0x50000000;
                line.r0 = 0xFF;
                line.g0 = rand();
                line.b0 = 0;
                line.r1 = line.g1 = line.b1 = 0;
                rotcoord_pair_update_matrix(&g_site_open_lines[i]);
                rotcoord_pair_project(&g_site_open_lines[i], &line.x0, &line.x1);
                if (screen_point_offscreen(&line.x0, 100) == 0) {
                    GsSortGLine(&line, &g_ot[g_frame_buffer_index], 0x15E);
                }
            }
        }
        if (--g_site_open_timer == 0) {
            g_site_action_busy = 0;
            g_site_action = 0;
            g_site_action_request = 0;
            g_site_open_phase = 0;
            g_site_open_shards_on = 0;
        }
        break;
    case 2:
        if (g_site_open_timer != 0) {
            g_site_open_timer--;
        }
        if (g_site_open_timer < 6) {
            g_site_node_models[99].flags |= 0x80000000;
        } else {
            if (g_site_open_timer == 12) {
                tim_upload(g_node_open_tim);
                GsGetTimInfo(g_site_node_tims[g_site_grid[g_site_open_cell.row][g_site_open_cell.col].texture] + 1, &image);
                rect.x = image.cx;
                rect.y = image.cy;
                rect.w = image.cw;
                rect.h = image.ch;
                GsGetTimInfo(D_80086384, &image);
                MoveImage(&rect, image.px, image.py);
                DrawSync(0);
                info = *D_801E8D28[0];
                for (i = 0; i < info.n_normal; i++) {
                    tmd_set_normal(i, &g_site_open_node_normals[i], &info);
                }
                snd_play_sfx(17);
            }
            if (g_site_open_timer < 12) {
                GsGetLws(&g_site_node_models[99].coord, &lw, &ls);
                for (i = 0; i < 5; i++) {
                    rotcoord_pair_attach(&g_site_open_lines[i], &g_models[5].coord);
                    PushMatrix();
                    angle.vx = 0;
                    angle.vz = 0;
                    angle.vy = ((g_site_rotation * 45 - g_site_rotation * 360) << 12) / 360;
                    RotMatrix(&angle, &rotation);
                    SetRotMatrix(&rotation);
                    in.vx = lw.t[0];
                    in.vy = lw.t[1] - g_site_level * 900;
                    in.vz = lw.t[2];
                    ApplyRotMatrix(&in, &out);
                    g_site_open_lines[i].start.vx = out.vx;
                    g_site_open_lines[i].start.vy = out.vy;
                    g_site_open_lines[i].start.vz = out.vz;
                    in.vx = (lw.t[0] - g_ring_wave_table[rand() & 0x7FF]) >> 7;
                    in.vy = ((lw.t[1] - g_ring_wave_table[rand() & 0x7FF]) >> 7) - g_site_level * 900;
                    in.vz = (lw.t[2] - g_ring_wave_table[rand() & 0x7FF]) >> 7;
                    ApplyRotMatrix(&in, &out);
                    g_site_open_lines[i].end.vx = out.vx;
                    g_site_open_lines[i].end.vy = out.vy;
                    g_site_open_lines[i].end.vz = out.vz;
                    PopMatrix();
                    line.attribute = 0x50000000;
                    line.r0 = 0xFF;
                    line.g0 = rand();
                    line.b0 = 0;
                    line.r1 = line.g1 = line.b1 = 0;
                    rotcoord_pair_update_matrix(&g_site_open_lines[i]);
                    rotcoord_pair_project(&g_site_open_lines[i], &line.x0, &line.x1);
                    if (screen_point_offscreen(&line.x0, 100) == 0) {
                        GsSortGLine(&line, &g_ot[g_frame_buffer_index], 0x15E);
                    }
                }
            }
            g_site_node_models[99].angle += 91;
            g_site_node_models[99].roll += 91;
            model_update_matrix(&g_site_node_models[99]);
        }
        goto finish;
    case 3:
        if (--g_site_open_timer == 6) {
            GsGetLws(&g_site_node_models[99].coord, &lw, &ls);
            PushMatrix();
            angle.vx = 0;
            angle.vz = 0;
            angle.vy = ((g_site_rotation * 45 - g_site_rotation * 360) << 12) / 360;
            RotMatrix(&angle, &rotation);
            SetRotMatrix(&rotation);
            in.vx = lw.t[0];
            in.vy = lw.t[1];
            in.vz = lw.t[2];
            ApplyRotMatrix(&in, &out);
            g_bezier_points[0] = out.vx;
            g_bezier_points[1] = (double)out.vy - g_site_level * 900;
            g_bezier_points[2] = out.vz;
            in.vx = g_site_open_path_mid[g_site_open_anim].vx;
            in.vy = g_site_open_path_mid[g_site_open_anim].vy;
            in.vz = g_site_open_path_mid[g_site_open_anim].vz;
            ApplyRotMatrix(&in, &out);
            g_bezier_points[3] = out.vx;
            g_bezier_points[4] = (double)out.vy - g_site_level * 900;
            g_bezier_points[5] = out.vz;
            in.vx = g_site_node_positions[g_site_open_cell.col].vx;
            in.vy = g_site_node_positions[g_site_open_cell.col].vy + 100;
            in.vz = g_site_node_positions[g_site_open_cell.col].vz;
            ApplyRotMatrix(&in, &out);
            g_bezier_points[6] = out.vx;
            g_bezier_points[7] = (double)out.vy - g_site_level * 900;
            g_bezier_points[8] = out.vz;
            PopMatrix();
            bezier_prepare(3, g_bezier_points, g_bezier_coeffs);
        }
        if (g_site_open_timer < 6) {
            bezier_eval(3, g_bezier_coeffs, pos, g_site_open_step / 6.0);
            g_site_node_models[99].x = pos[0];
            g_site_node_models[99].y = pos[1];
            g_site_node_models[99].z = pos[2];
            g_site_node_models[99].angle = g_site_rotation * 512;
            model_update_matrix(&g_site_node_models[99]);
            g_site_open_step++;
        }
        if (g_site_open_timer == 12 || g_site_open_timer == 8) {
            snd_play_sfx(18);
        }
        if (g_site_open_timer == 0) {
            g_site_action_busy = 0;
            g_site_action = 0;
            g_site_action_request = 0;
            g_site_open_phase = 0;
            g_site_node_opening = 0;
            g_site_open_step = 0;
        }
        break;
    case 4:
        if (--g_site_open_timer == 37) {
            for (i = 0; i < 20; i++) {
                sparks[i].vx = (s16)(g_ring_wave_table[i] >> 16) * 10;
                sparks[i].vy = lw.t[1];
                sparks[i].vz = (s16)(g_ring_wave_table[i] >> 16) * 10;
                g_site_open_spark_coords[i].pos.vx = lw.t[0];
                g_site_open_spark_coords[i].pos.vy = lw.t[1];
                g_site_open_spark_coords[i].pos.vz = lw.t[2];
            }
        }
        if (g_site_open_timer < 37) {
            g_site_node_models[99].flags |= 0x80000000;
        }
        if (g_site_open_timer == 53 || g_site_open_timer == 49 || g_site_open_timer == 45 || g_site_open_timer == 41) {
            snd_play_sfx(18);
        }
        if (g_site_open_timer == 31) {
            snd_play_sfx(19);
        }
    finish:
        if (g_site_open_timer == 0) {
            g_site_action_busy = 0;
            g_site_action = 0;
            g_site_action_request = 0;
            g_site_open_phase = 0;
            g_site_node_opening = 0;
            snd_play_sfx(31);
        }
        break;
    default:
        g_site_action_busy = 0;
        g_site_action = 0;
        g_site_action_request = 0;
        g_site_open_phase = 0;
        g_site_node_opening = 0;
        break;
    }
}
#else
INCLUDE_ASM("asm/nonmatchings/game/8002A344", node_open_anim_effect);
#endif

/* Sets up the four texture pages of g_site_bg_sprites and clears the right half of VRAM. */
void site_bg_init(void) {
    RECT rect;

    g_site_bg_sprites[0].tpage = GetTPage(2, 1, 0x140, 0);
    g_site_bg_sprites[1].tpage = GetTPage(2, 1, 0x240, 0);
    g_site_bg_sprites[2].tpage = GetTPage(2, 1, 0x140, 0x100);
    g_site_bg_sprites[3].tpage = GetTPage(2, 1, 0x240, 0x100);
    rect.x = 0x140;
    rect.w = 0x140;
    rect.y = 0;
    rect.h = 0x1E0;
    ClearImage2(&rect, 0, 0, 0);
}

void site_scene_reset_to_node(u32 index, u32 row) {
    s32 unused[2]; /* unused local that only reserves stack space */

    camera_init();
    light_apply_preset();
    site_scene_init();
    if (index >= site_get_level_count()) {
        index = site_get_level_count() - 1;
    }
    if (row >= 0x18) {
        row = 0x17;
    }
    g_site_action_request = 0x16;
    g_site_sel_level = index;
    g_site_sel_col = row;
    site_intro_cam_start();
    light_apply_preset();
}

void site_goto_run(void) {
    if (g_camera_view == 4) {
        while (g_site_action_busy != 0) {
            site_goto_step();
        }
        g_site_action_busy = 0xFF;
    } else {
        site_goto_step();
    }
}

void site_intro_cam_start(void) {
    camera_set_view(4);
    site_cam_path_start(4);
}

/*
 * 16 diffs, tooling only: the high word of the double g_site_cam_path_t is accessed as
 * %gp_rel(g_site_cam_path_t+4); maspsx (ASPSX 2.56 mode) refuses $gp offsets on the
 * .comm symbol that tools/extern_sdata.py creates, so it emits lui+lw instead.
 * 0 diffs with PROBE_ASPSX=2.79.
 */
void site_intro_cam_update(void) {
    if (g_camera_view == 4 && g_site_action_request != 0x16) {
        site_cam_path_step();
        if (g_site_cam_path_t < 50.0) {
            g_site_bg_offset_x = 12;
        } else {
            g_site_bg_offset_x = 0;
        }
        if (g_site_intro_cam_done != 0) {
            g_site_intro_cam_done = 0;
            g_site_bg_offset_x = 0;
            g_site_action_busy = 0;
            g_site_action = 0;
            g_site_action_request = 0;
            g_camera_view = 1;
            light_apply_preset();
            site_ring_init();
        }
    }
}
