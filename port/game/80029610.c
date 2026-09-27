#include "common.h"
/* port: PsyQ types, macros and prototypes come from psx_sdk.h (port/psx headers). */

/* Declarations carried over from 800281A8.c (same original headers). */











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

/* port: model helpers from 800281A8.c (their Model signatures keep them out of game_protos.h) */
void model_reset_coord(Model *model);
void model_update_matrix(Model *model);
void model_map_tmd(u32 *file, Model *model);
void model_link(Model *model, GsCOORDINATE2 *super);


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


extern s32 g_interp_steps_left NO_GP;
extern s32 g_frame_buffer_index;
extern s32 g_interp_count NO_GP;
extern s32 g_light_fade_index;
extern u8 g_light_fade_active NO_GP;
extern u8 g_camera_view;
extern s32 g_dobj_id_next;
extern u8 D_800A6998;
extern void *g_light_fade_target; /* port: s32 in the decomp; FlatLight * (target light colour, see 80023468.c) */
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
extern SVECTOR g_site_level_ring_pos NO_GP;
extern SVECTOR g_site_level_spinner_pos NO_GP;

extern GsRVIEW2 g_view_preset_map;
extern GsRVIEW2 g_view_preset_menu;
extern GsRVIEW2 g_view_preset_intro;
extern u32 *g_site_node_tmds[];
extern SVECTOR g_level_label_offsets[];
extern SVECTOR g_site_node_positions[];
extern GridRow g_site_grid[];
extern u8 g_gs_packet_area[][36000];
extern SVECTOR g_interp_deltas[];
/* port: D_801E4B14 (= &g_models[5].coord) is written as g_models[5].coord; Model has pointers. */
extern Model g_models[];
extern Model g_site_node_models[];
extern Model g_site_level_models[];
extern GsFOGPARAM g_fog_param;
extern GsOT g_ot_2d[];
extern GsOT g_ot[];
extern GsRVIEW2 g_view;
extern MATRIX g_zero_matrix;

/* Grid models are numbered from 1 (slot 0 is unused). */
#define GRID_MODEL(n) (g_site_node_models[(n) + 1])

typedef struct {
    SVECTOR *vecs0;
    s32 nVert;
    SVECTOR *vecs1;
} SVectorTables;

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
extern volatile s32 g_vsync_counter NO_GP;
extern s32 g_site_level NO_GP;
extern s32 g_ring_idle_morph_time NO_GP;
extern s32 g_ring_wave_decay_time NO_GP;
extern s32 g_site_tilted_up;
extern s32 g_site_cursor_cell;
extern s32 D_800A6A14;
extern s32 g_site_cam_path_t;
extern s32 D_800A6A24;
extern s32 g_site_tilted_down;
extern s16 g_site_tilt_offset_y;
extern s32 g_site_action;
extern u8 g_ring_land_sfx_pending;
extern u8 g_site_node_opening;
extern u8 g_site_models_dirty;
extern s16 g_site_bg_offset_x;


/* 26 diffs: g_interp_count, g_interp_steps_left, g_light_fade_active, g_ring_wave_decay_time, g_ring_idle_morph_time,
 * g_vsync_counter and g_site_level are small but addressed absolutely (lui/%lo via
 * $at) in the original, while other functions in this file use $gp for the
 * first three. That split suggests this function belongs to a different
 * translation unit. */
/* Scene setup for the grid: load and place all models, upload the textures
 * and reset the scene state. */
void site_scene_init(void) {
    u32 i;
    u32 j;

    DrawSync(0);
    VSync(0);
    model_reset_coord(&g_models[1]);
    model_map_tmd(&g_empty_tmd, &g_models[1]);
    model_link(&g_models[1], NULL);
    g_models[1].coord.coord.t[2] = g_view.vpz;
    model_reset_coord(&g_models[5]);
    model_map_tmd(&g_empty_tmd, &g_models[5]);
    model_link(&g_models[5], &g_models[1].coord);
    g_site_node_model_count = 0;
    g_site_models_dirty = 1;
    g_models[5].coord.coord.t[2] = -g_view.vpz;
    for (i = 0; i < 4; i++) {
        for (j = 0; j < 24; j++) {
            site_node_model_create(i, j);
        }
        site_level_models_place(i, i, 0, 0);
    }
    tim_upload(&g_level_ring_lof_tim);
    tim_upload(&g_level_ring_text_tim);
    tim_upload(&g_level_ring_line_tim);
    tim_upload(&g_level_digits_tim);
    tim_upload(&g_level_label_even_tens_tim);
    tim_upload(&g_level_label_even_ones_tim);
    tim_upload(&g_level_label_odd_tens_tim);
    tim_upload(&g_level_label_odd_ones_tim);
    tim_upload(&g_node_icon0_tim);
    tim_upload(&g_node_icon1_tim);
    tim_upload(&g_node_icon2_tim);
    tim_upload(&g_node_icon3_tim);
    tim_upload(&g_node_icon4_tim);
    tim_upload(&g_node_icon5_tim);
    tim_upload(&g_node_icon6_tim);
    tim_upload(&g_node_icon7_tim);
    tim_upload(&g_node_open_tim);
    tim_upload(&g_node_cursor_tim);
    tim_upload(&g_node_icon8_tim);
    tim_upload(&g_node_icon9_tim);
    tim_upload(&g_node_icon10_tim);
    tim_upload(&g_node_icon11_tim);
    tim_upload(&g_node_icon12_tim);
    tim_upload(&g_node_icon13_tim);
    tim_upload(&g_node_icon14_tim);
    tim_upload(&g_node_icon15_tim);
    tim_upload(g_current_site[0] == 0 ? &g_level_label_site_a_tim : &g_level_label_site_b_tim);
    g_site_action = 0;
    g_site_tilted_down = 0;
    g_site_tilted_up = 0;
    g_site_tilt_offset_y = 0;
    g_interp_count = 0;
    g_interp_steps_left = 0;
    g_site_cam_path_t = 0;
    D_800A6A24 = 0;
    g_light_fade_active = 0;
    g_site_node_opening = 0;
    g_ring_land_sfx_pending = 0;
    g_site_models_dirty = 0;
    g_site_bg_offset_x = 12;
    g_ring_wave_decay_time = g_vsync_counter;
    g_ring_idle_morph_time = g_vsync_counter;
    site_bg_init();
    site_effects_init();
    site_set_level_label(g_site_level);
    g_site_cursor_cell = 0xFF;
    D_800A6A14 = 0xFF;
    srand(10);
    site_intro_cam_start();
    light_apply_preset();
    site_orb_init();
}

/* Allocate a new model slot for grid cell (row, col), load its TMD and place it. */
void site_node_model_create(u32 row, u32 col) {
    s32 height;

    if (row < site_get_level_count() && col < 24) {
        if (++g_site_node_model_count < 98) {
            model_reset_coord(&GRID_MODEL(g_site_node_model_count));
            if (g_site_grid[row].cells[col].kind == -1) {
                model_map_tmd((u32 *)&g_node_open_tmd, &GRID_MODEL(g_site_node_model_count));
            } else {
                model_map_tmd(g_site_node_tmds[g_site_grid[row].cells[col].tmdIndex], &GRID_MODEL(g_site_node_model_count));
            }
            model_link(&GRID_MODEL(g_site_node_model_count), &g_models[5].coord);
            GRID_MODEL(g_site_node_model_count).coord.coord.t[0] = g_site_node_positions[col].vx;
            height = row * 900 - 100;
            GRID_MODEL(g_site_node_model_count).coord.coord.t[1] = g_site_node_positions[col].vy - height;
            GRID_MODEL(g_site_node_model_count).coord.coord.t[2] = g_site_node_positions[col].vz;
            GRID_MODEL(g_site_node_model_count).rot.vy = ((col * 45 + 22.5) - col * 360) * 4096.0 / 360.0;
            model_update_matrix(&GRID_MODEL(g_site_node_model_count));
            GRID_MODEL(g_site_node_model_count).dobj.attribute = 0xD0000000;
            g_site_grid[row].cells[col].model = g_site_node_model_count;
        }
    }
}

/* Reload the model already assigned to grid cell (row, col) and place it again. */
void site_node_model_reload(u32 row, u32 col) {
    s32 height;

    if (row < site_get_level_count() && col < 24) {
        g_site_node_model_count = g_site_grid[row].cells[col].model;
        model_reset_coord(&GRID_MODEL(g_site_node_model_count));
        if (g_site_grid[row].cells[col].kind == -1) {
            model_map_tmd((u32 *)&g_node_open_tmd, &GRID_MODEL(g_site_node_model_count));
        } else {
            model_map_tmd(g_site_node_tmds[g_site_grid[row].cells[col].tmdIndex], &GRID_MODEL(g_site_node_model_count));
        }
        model_link(&GRID_MODEL(g_site_node_model_count), &g_models[5].coord);
        GRID_MODEL(g_site_node_model_count).coord.coord.t[0] = g_site_node_positions[col].vx;
        height = row * 900 - 100;
        GRID_MODEL(g_site_node_model_count).coord.coord.t[1] = g_site_node_positions[col].vy - height;
        GRID_MODEL(g_site_node_model_count).coord.coord.t[2] = g_site_node_positions[col].vz;
        GRID_MODEL(g_site_node_model_count).rot.vy = ((col * 45 + 22.5) - col * 360) * 4096.0 / 360.0;
        model_update_matrix(&GRID_MODEL(g_site_node_model_count));
        GRID_MODEL(g_site_node_model_count).dobj.attribute = 0xD0000000;
        g_site_grid[row].cells[col].model = g_site_node_model_count;
    }
}

/* g_site_level_ring_pos/g_site_level_spinner_pos are 8-byte SVECTORs (small, but absolute in this TU):
 * .vy/.vz are D_800A66CE/D_800A66D0 and D_800A66D6/D_800A66D8. */
/* Load and place the five models of set n (0-3) at the given row. */
void site_level_models_place(s32 n, s32 row, s32 angle0, s32 angle1) {
    model_reset_coord(&g_site_level_models[n + 1]);
    model_map_tmd(&g_level_ring_tmd, &g_site_level_models[n + 1]);
    model_link(&g_site_level_models[n + 1], &g_models[5].coord);
    g_site_level_models[n + 1].coord.coord.t[0] = g_site_level_ring_pos.vx;
    g_site_level_models[n + 1].coord.coord.t[1] = g_site_level_ring_pos.vy - row * 900;
    g_site_level_models[n + 1].coord.coord.t[2] = g_site_level_ring_pos.vz;
    g_site_level_models[n + 1].rot.vy = angle0;
    g_site_level_models[n + 1].dobj.attribute = 0x80000000;
    g_site_level_models[n + 1].rot.vy = -227;
    model_update_matrix(&g_site_level_models[n + 1]);

    model_reset_coord(&g_site_level_models[n + 5]);
    model_map_tmd(&g_level_spinner_tmd, &g_site_level_models[n + 5]);
    model_link(&g_site_level_models[n + 5], &g_models[5].coord);
    g_site_level_models[n + 5].coord.coord.t[0] = g_site_level_spinner_pos.vx;
    g_site_level_models[n + 5].coord.coord.t[1] = g_site_level_spinner_pos.vy - row * 900;
    g_site_level_models[n + 5].coord.coord.t[2] = g_site_level_spinner_pos.vz;
    g_site_level_models[n + 5].rot.vy = angle1;
    g_site_level_models[n + 5].dobj.attribute = 0x80000000;

    model_reset_coord(&g_site_level_models[n + 9]);
    if (row & 1) {
        model_map_tmd(&g_level_label_odd_tmd, &g_site_level_models[n + 9]);
    } else {
        model_map_tmd(&g_level_label_even_tmd, &g_site_level_models[n + 9]);
    }
    model_link(&g_site_level_models[n + 9], &g_site_level_models[n + 5].coord);
    g_site_level_models[n + 9].coord.coord.t[0] = g_level_label_offsets[0].vx;
    g_site_level_models[n + 9].coord.coord.t[1] = g_level_label_offsets[0].vy;
    g_site_level_models[n + 9].coord.coord.t[2] = g_level_label_offsets[0].vz;
    g_site_level_models[n + 9].rot.vy = 0;
    model_update_matrix(&g_site_level_models[n + 9]);
    g_site_level_models[n + 9].dobj.attribute = 0x80000000;

    model_reset_coord(&g_site_level_models[n + 13]);
    if (row & 1) {
        model_map_tmd(&g_level_label_odd_tmd, &g_site_level_models[n + 13]);
    } else {
        model_map_tmd(&g_level_label_even_tmd, &g_site_level_models[n + 13]);
    }
    model_link(&g_site_level_models[n + 13], &g_site_level_models[n + 5].coord);
    g_site_level_models[n + 13].coord.coord.t[0] = g_level_label_offsets[1].vx;
    g_site_level_models[n + 13].coord.coord.t[1] = g_level_label_offsets[1].vy;
    g_site_level_models[n + 13].coord.coord.t[2] = g_level_label_offsets[1].vz;
    g_site_level_models[n + 13].rot.vy = 0x800;
    model_update_matrix(&g_site_level_models[n + 13]);
    g_site_level_models[n + 13].dobj.attribute = 0x80000000;

    model_reset_coord(&g_site_level_models[n + 17]);
    model_map_tmd(&D_800840D8, &g_site_level_models[n + 17]);
    model_link(&g_site_level_models[n + 17], &g_models[5].coord);
    g_site_level_models[n + 17].coord.coord.t[0] = g_site_level_ring_pos.vx;
    g_site_level_models[n + 17].coord.coord.t[1] = g_site_level_ring_pos.vy - row * 900;
    g_site_level_models[n + 17].coord.coord.t[2] = g_site_level_ring_pos.vz;
    g_site_level_models[n + 17].dobj.attribute = 0x80000000;
}
