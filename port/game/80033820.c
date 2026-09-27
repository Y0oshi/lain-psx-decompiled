#include "common.h"
/* port: PsyQ types, macros and prototypes come from psx_sdk.h (port/psx headers). */

/* Declarations carried over from 80031378.c (same original headers). */

/* Minimal PsyQ libgte/libgs types used by this file. */










/* A TMD model instance (see 800281A8.c). */
typedef struct {
    GsDOBJ2 dobj;         /* 0x00 */
    GsCOORDINATE2 coord;  /* 0x10 */
    SVECTOR rot;          /* 0x60 */
    u32 nobj;             /* 0x68 */
    u32 *tmd;             /* 0x6C */
    s32 id;               /* 0x70 */
} Model;

extern Model g_models[];  /* scene models; D_801E5AE0 = [40], D_801E5B54 = [41] */
/* port: D_801E5B54 (= &g_models[41]) is written as g_models[41 + i]; Model has pointers. */
void model_update_matrix(Model *model);
void model_sort(Model *model, GsOT ot);


extern u32 g_empty_tmd;
extern u32 g_movie_model_tim0;
extern u32 g_movie_model_tim1;
extern u32 *g_movie_model_tmds[];
extern u8 g_movie_model_kinds[];
extern s16 g_movie_models_center_x;
extern s16 g_movie_models_center_y;
extern s16 g_movie_models_center_z;
extern SVECTOR g_movie_model_home_pos[];
extern SVECTOR g_movie_model_start_pos[];
extern SVECTOR g_movie_model_steps[];
extern s32 g_scene_wait_frames;
extern u8 g_gate_scroll_done;
extern s32 g_movie_model1_spin;
extern s32 g_movie_model0_spin;
extern s32 g_movie_models_slide_frame;
extern u8 g_movie_models_state;
extern GsF_LIGHT g_flat_lights[];
void model_reset_coord(Model *model);
void model_map_tmd(u32 *file, Model *model);
void model_link(Model *model, GsCOORDINATE2 *super);


typedef struct {
    double x, y, z;
} DVECTOR3;

extern double g_gate_fly_time; /* spline time (frames), high word is D_800A6ABC */
extern s16 g_gate_level ;
extern s16 g_gate_fly_pending ;
extern s32 g_gate_level_angles[];
extern DVECTOR3 g_bezier_points[];
extern u8 g_bezier_coeffs[];
void bezier_eval(s32 count, void *pts, DVECTOR3 *out, double t);
void bezier_prepare(s32 count, DVECTOR3 *in, void *out);

/* Two coordinate systems sharing one rotation (see 800281A8.c). */
typedef struct {
    GsCOORDINATE2 coord0; /* 0x00 */
    GsCOORDINATE2 coord1; /* 0x50 */
    SVECTOR rot;          /* 0xA0 */
} RotCoordPair;

typedef struct {
    u8 r, g, b;
} Color3;

/* A falling streak (rain/snow line): head and tail positions projected and
 * drawn as a gouraud line. */
typedef struct {
    RotCoordPair pos;     /* 0x00: coord0 = head, coord1 = tail */
    GsGLINE line;         /* 0xA8 */
    SVECTOR origin;       /* 0xBC: respawn position */
    SVECTOR limit;        /* 0xC4: position at which it respawns */
    s16 speed;            /* 0xCC */
    s16 length;           /* 0xCE */
    u8 r, g, b;           /* 0xD0 */
} Streak;

typedef struct {
    SVECTOR limit;
    SVECTOR origin;
} StreakSpawn;



/* PsyQ libgpu primitive-length setter (byte 3 of the tag word). */

/* 0x4C-byte glyph object of the name-entry screen, drawn by sprite_draw_rotated
 * (only the sprite-like part at 0x28 is set up here). */
typedef struct {
    POLY_FT4 poly; /* port: host POLY_FT4 (0x28 bytes on the PS1); the fields below follow it */
    u32 attribute;
    s16 tpage;
    s16 clut;
    s16 scaleX;
    s16 scaleY;
    s16 x, y;
    s16 w, h;
    s16 rotX;
    s16 rotY;
    s16 rotZ;
    s16 unk1A;
    u8 u, v;
    u8 r, g, b;
    u8 pad[3];
} Glyph;

/* Work area of the name-entry screen (0x1240 bytes, lives on the stack of name_entry_run). */
typedef struct {
    POLY_FT4 grid[81];    /* 0x000: 9x9 window onto the character table */
    POLY_FT4 cursor;      /* 0xCA8: highlighted cell */
    GsSPRITE labels[5];   /* 0xCD0 */
    u8 unkD84[0x24];
    GsSPRITE sprites[9];  /* 0xDA8: [0] cursor, [1..8] entered characters */
    Glyph glyphs[8];      /* 0xEEC */
    u8 unk114C[0xE4];
    s16 row;              /* 0x1230: cursor row in the character table */
    s16 col;              /* 0x1232: cursor column */
    s16 count;            /* 0x1234: number of entered characters */
    u8 chars[8];          /* 0x1236 */
} MenuWork;

void name_entry_init(MenuWork *w);
void name_entry_draw_grid(MenuWork *w);
void name_entry_draw_chars(MenuWork *w);
void name_entry_cursor_down(MenuWork *w);
void name_entry_press_cell(MenuWork *w);
s32 name_entry_input_loop(MenuWork *w);
void name_entry_to_romaji(MenuWork *w);

extern GsOT g_ot_2d[];

/* A point of a piecewise-linear curve. */
typedef struct {
    float x;
    float y;
} CurvePoint;

extern CurvePoint g_voice_envelope[];
s32 curve_eval_linear(u32 x, CurvePoint *curve, u32 count);

/* port: D_801E4944 (= &g_models[1].coord) is written as g_models[1].coord. */
extern s16 g_streak_spawn_y[];
extern s16 g_streak_spawn_x[];
extern s16 g_streak_spawn_z[];
extern Color3 g_streak_colors[];
extern GsOT g_ot[];
extern s32 g_frame_buffer_index ;
/* Camera/scene offsets; only the first word of each is used. */
/* port: D_801EAEE4/D_801EAEE8 alias g_view.vpy/.vpz (GsRVIEW2, which holds a
 * pointer and is a host global), so they are written as members of g_view. */
extern GsRVIEW2 g_view;

void rotcoord_pair_update_matrix(RotCoordPair *pair);
void rotcoord_pair_attach(RotCoordPair *pair, GsCOORDINATE2 *super);
void rotcoord_pair_project(RotCoordPair *pair, DVECTOR *screen0, DVECTOR *screen1);
s32 screen_point_offscreen(DVECTOR *pos, s16 margin);

/* A loaded file in the concatenation list (see voice_load_clip/voice_trim_clips/voice_concat_clips). */
typedef struct {
    u8 *data;
    s32 offset;           /* 1 = trim leading padding */
    s32 tail;             /* 1 = trim trailing padding */
    s16 continued;        /* name contained '_' */
    s16 unkE;
    s32 size;
} FileEntry;

/* Directory entry of the file table searched by name. */
typedef struct {
    s32 sector;
    s32 size;
} FileInfo;

extern s32 g_media_id ;
extern s32 g_bin_file_table[];
extern s32 D_80098F8C[];
u8 *g_voice_name_table; /* port: defined here (host-typed) */
extern u8 g_voice_prev_vowel; /* previous vowel, '\\' = none */
u8 *g_voice_pcm; /* port: defined here (host-typed) */
extern s32 g_voice_pcm_size;
extern s16 g_voice_clip_count;
FileEntry g_voice_clips[17]; /* port: defined here (host-typed) */
extern FileInfo g_voice_file_table[];

s32 cd_load_archive_entry(s32, s32, void *, void *); /* port: dest is s32 on the PS1 */

/* Inlined twin of voice_find_clip: index of `name` in the name table (0x27E if absent). */
static __inline__ s32 FindName(char *name) {
    char line[16];
    u8 *p;
    s32 index;
    s32 len;
    u8 c;

    index = 0;
    p = g_voice_name_table;
    do {
        len = 0;
        while ((c = *p) != '\r') {
            p++;
            line[len++] = c;
        }
        line[len] = 0;
        if (strncmp(name, line, len) == 0) {
            break;
        }
        index++;
        p += 2;
    } while (index < 0x27E);
    return index;
}

extern u32 *g_movie_model_tmds_hi[];
extern s16 g_text_window_busy ;

/* 0x1C-byte model descriptor at the start of a loaded TMD block (see 8002A344.c). */
typedef struct {
    s32 vert_top; /* PsyQ TMD object entry: vertex, normal, primitive tables + scale */
    u32 n_vert;
    s32 normal_top;
    u32 n_normal;
    s32 primitive_top;
    s32 n_primitive;
    s32 scale;
} ModelInfo;

extern u32 g_ring_models_center_tmd;
extern u32 g_ring_models_outer_tmd;
extern SVECTOR g_ring_models_center_verts[];
extern SVECTOR g_ring_models_outer_verts[];
extern SVECTOR g_ring_models_outer_pos[];
extern u8 g_ring_models_state;
extern u8 g_ring_models_done;
void tmd_set_vertex(u32 index, SVECTOR *entry, ModelInfo *info);

/* Spin the ring of models 41..46 one step (every `delay` calls) and draw them. */

extern s32 g_interp_steps_left ;
extern s32 g_morph_mode ;
extern s32 g_saved_interp_steps_left ;
extern s32 g_saved_morph_mode ;
extern SVECTOR g_interp_deltas[];
extern SVECTOR g_saved_interp_delta[];
extern SVECTOR g_ring_models_morph_verts[];
extern volatile SVECTOR g_ring_models_collapse_verts[];
extern volatile s16 g_ring_models_collapse_x;
extern volatile s16 g_ring_models_collapse_y;
extern volatile s16 g_ring_models_collapse_z;
void tmd_get_vertex(u32 index, SVECTOR *entry, ModelInfo *info);
void vec_interp_start(SVECTOR *to, SVECTOR *from, s16 steps, u32 count);
void vec_interp_step(SVECTOR *vecs);

extern s16 g_gate_center_x;
extern s16 g_gate_center_y;
extern s16 g_gate_center_z;
extern s16 g_gate_obj_start_z;
extern SVECTOR g_gate_orbit_pos[];
extern u32 *g_gate_model_tmds[];
extern u32 g_gate_fixed_tmd0;
extern u32 g_gate_fixed_tmd1;
extern u32 g_gate_scroll_tim;
extern u32 g_gate_column_tim;
extern u32 D_80094D1C; /* TIM data following g_gate_column_tim's header word */
extern u8 g_gate_fly_started;
extern s16 g_gate_obj_screen_x ;
extern s16 g_gate_obj_screen_y ;
extern GsSPRITE g_gate_column_sprites[];
void model_project_origin(Model *model, DVECTOR *screen);

extern u8 D_800920DC[];


extern u32 g_menu_scroll_step;
extern u32 *g_menu_scroll_tim;



#ifdef NON_MATCHING
/* Declarations used by start_menu_run. */

/* Sprite layout entry (same layout as SpriteDef / g_sskn_sprite_defs in 800379C8.c). */
typedef struct {
    s16 x, y;
    u16 w, h;
    u16 tpage;
    u8 u, v;
    s16 cx, cy;
} SpriteDef;

extern SpriteDef g_start_menu_sprite_defs[];   /* sprite layout of this menu */
extern char g_str_yes[];        /* .sdata "Yes" */
extern char g_str_no[];        /* .sdata "No" */

void sprite_draw(Glyph *g, GsOT *ot, u16 pri);

/* Also declared in src/game/80031378.c. */

extern GsOT g_ot_2d[];
extern s32 g_frame_buffer_index ;
extern u32 g_menu_scroll_step;
extern u32 *g_menu_scroll_tim;
void start_menu_update_panels(POLY_FT4 *a, POLY_FT4 *b, u8 shade);
extern s16 g_pad_command ; /* pad input (button index) */
void sprite_draw_rotated(Glyph *g, GsOT *ot, u16 pri);

/*
 * Start menu (new game / load game).
 *
 * Saves the VRAM strip used for text, renders "Are you sure?", "Yes" and "No"
 * into it, then runs its own frame loop:
 *   0      title animation: sprites flicker through two frame sequences on random
 *          timers until one sequence is used up
 *   1      transition into the menu (title fades out, logo slides)
 *   2      left/right choice (sprites[16]/[17] cursor), confirm
 *   10-12  choice 0: slide to the name-entry screen (name_entry_run), return
 *          here if it is cancelled, leave the menu if a name was entered
 *   3,4    choice 1: slide in the confirm box, up/down yes/no, confirm
 *   5      yes: memory card access (sub-state machine, messages via
 *          msgbox_draw); success leaves the menu, errors go to 9
 *   9      slide back to state 2
 * States 1-12 also draw the two background quads (start_menu_update_panels, which also
 * streams a TIM into VRAM a step per frame).
 * On exit VRAM is cleared, the text strip restored and the buffers freed.
 *
 * NON_MATCHING: 35 of 1825 instructions differ after alignment (2.8.1-psx);
 * check.sh reports 184 because the missing instruction shifts every later
 * relocation. Remaining differences:
 *  - case 0: the delay slot of the sprites[0..1] loop branch is filled from
 *    the fall-through (li a2,1) instead of the loop head (move a0,s0), so
 *    the function is one instruction short and everything after shifts.
 *    Cause: a (use (reg)) that combine leaves after the loop for the
 *    idx[0] sign-extension temp (idx[0] is CSE'd into the idx[0]++).
 *  - case 0: which / idx[] temp in a0/v1 swapped.
 *  - every "t % 16" / "t % 64 / 4" angle: quotient in v0 instead of v1
 *    (local-alloc ties the shift chain; the original doesn't).
 *  - memory-card sub-state 3: li a0,0x10 duplicated in the jal delay slot.
 */
void start_menu_run(void) {
    s16 idx[2];            /* 0x18: position in each frame sequence */
    s16 timers[2][6];      /* 0x20: frames each sequence step lasts */
    u8 order[2][6] = {     /* 0x38: sprite index sequences of the title flicker */
        { 3, 4, 5, 6, 7, 8 },
        { 9, 10, 11, 6, 12, 8 },
    };
    RECT rect;             /* 0x48 */
    GsSPRITE sprites[21];  /* 0x50 */
    Glyph glyphs[8];       /* 0x348 */
    POLY_FT4 polys[2];     /* 0x5A8 */
    s32 i;
    s32 j;
    s32 k;
    s32 t;
    s32 state;
    s32 sub;
    s32 wait;
    s32 which;
    s32 frame;
    s32 choice;
    s32 busy;
    s32 r;
    u8 shade;
    s32 fade;
    u32 *save;
    u32 *buf;

    g_menu_scroll_step = 0;
    save = (u32 *)heap_alloc(0x4000);
    rect.x = 0x140;
    rect.y = 0x1E0;
    rect.w = 0x100;
    rect.h = 0x20;
    StoreImage2(&rect, save);

    while (gfx_bg_load_step(0) == 0) {
    }

    for (j = 0; j < 14; j++) {
        sprites[j].attribute = 0;
        sprites[j].x = g_start_menu_sprite_defs[j].x;
        sprites[j].y = g_start_menu_sprite_defs[j].y;
        sprites[j].w = g_start_menu_sprite_defs[j].w;
        sprites[j].h = g_start_menu_sprite_defs[j].h;
        sprites[j].tpage = g_start_menu_sprite_defs[j].tpage;
        sprites[j].u = g_start_menu_sprite_defs[j].u;
        sprites[j].v = g_start_menu_sprite_defs[j].v;
        sprites[j].cx = g_start_menu_sprite_defs[j].cx;
        sprites[j].cy = g_start_menu_sprite_defs[j].cy;
        sprites[j].r = 0x80;
        sprites[j].g = 0x80;
        sprites[j].b = 0x80;
        sprites[j].scalex = 0x1000;
        sprites[j].scaley = 0x1000;
        sprites[j].rotate = 0;
    }
    for (j = 18; j < 22; j++) {
        sprites[j - 4].attribute = 0;
        sprites[j - 4].x = g_start_menu_sprite_defs[j].x;
        sprites[j - 4].y = g_start_menu_sprite_defs[j].y;
        sprites[j - 4].w = g_start_menu_sprite_defs[j].w;
        sprites[j - 4].h = g_start_menu_sprite_defs[j].h;
        sprites[j - 4].tpage = g_start_menu_sprite_defs[j].tpage;
        sprites[j - 4].u = g_start_menu_sprite_defs[j].u;
        sprites[j - 4].v = g_start_menu_sprite_defs[j].v;
        sprites[j - 4].cx = g_start_menu_sprite_defs[j].cx;
        sprites[j - 4].cy = g_start_menu_sprite_defs[j].cy;
        sprites[j - 4].r = 0x80;
        sprites[j - 4].g = 0x80;
        sprites[j - 4].b = 0x80;
        sprites[j - 4].scalex = 0x1000;
        sprites[j - 4].scaley = 0x1000;
        sprites[j - 4].rotate = 0;
    }
    for (j = 26; j < 29; j++) {
        sprites[j - 8].attribute = 0;
        sprites[j - 8].x = g_start_menu_sprite_defs[j].x;
        sprites[j - 8].y = g_start_menu_sprite_defs[j].y;
        sprites[j - 8].w = g_start_menu_sprite_defs[j].w;
        sprites[j - 8].h = g_start_menu_sprite_defs[j].h;
        sprites[j - 8].tpage = g_start_menu_sprite_defs[j].tpage;
        sprites[j - 8].u = g_start_menu_sprite_defs[j].u;
        sprites[j - 8].v = g_start_menu_sprite_defs[j].v;
        sprites[j - 8].cx = g_start_menu_sprite_defs[j].cx;
        sprites[j - 8].cy = g_start_menu_sprite_defs[j].cy;
        sprites[j - 8].r = 0x80;
        sprites[j - 8].g = 0x80;
        sprites[j - 8].b = 0x80;
        sprites[j - 8].scalex = 0x1000;
        sprites[j - 8].scaley = 0x1000;
        sprites[j - 8].rotate = 0;
    }
    for (j = 0; j < 8; j++) {
        if (j < 4) {
            k = j + 14;
        } else {
            k = j + 18;
        }
        glyphs[j].attribute = 0;
        glyphs[j].x = g_start_menu_sprite_defs[k].x;
        glyphs[j].y = g_start_menu_sprite_defs[k].y;
        glyphs[j].w = g_start_menu_sprite_defs[k].w;
        glyphs[j].h = g_start_menu_sprite_defs[k].h;
        glyphs[j].tpage = g_start_menu_sprite_defs[k].tpage;
        glyphs[j].u = g_start_menu_sprite_defs[k].u;
        glyphs[j].v = g_start_menu_sprite_defs[k].v;
        glyphs[j].clut = (g_start_menu_sprite_defs[k].cy << 6) | ((g_start_menu_sprite_defs[k].cx >> 4) & 0x3F);
        glyphs[j].r = 0x80;
        glyphs[j].g = 0x80;
        glyphs[j].b = 0x80;
        glyphs[j].scaleX = 0x1000;
        glyphs[j].scaleY = 0x1000;
        glyphs[j].rotX = 0;
        glyphs[j].rotY = 0;
        glyphs[j].rotZ = 0;
    }
    glyphs[1].attribute = 0x800000;
    glyphs[4].attribute = 0x800000;
    glyphs[5].attribute = 0x40000000;
    glyphs[6].attribute = 0x40800000;

    font_render_string("Are you sure?", 0x2C0, 0x170, 0, 0);
    font_render_string(g_str_yes, 0x2C0, 0x188, 0, 0);
    font_render_string(g_str_no, 0x2C0, 0x1A0, 0, 0);

    for (j = 0; j < 6; j++) {
        timers[0][j] = rand() % 40 + 10;
        timers[1][j] = rand() % 40 + 10;
    }
    frame = 0;
    idx[0] = 0;
    idx[1] = 0;
    bg_curves_init();
    SetDispMask(1);

    state = 0;
    t = 0;
    for (;;) {
        DrawSync(0);
        gfx_frame_begin();
        pad_read_command();

        switch (state) {
        case 0:
            /* Title: flicker through the frame sequences. */
            bg_curves_update();
            for (i = 0; i < 2; i++) {
                GsSortFastSprite(&sprites[i], &g_ot_2d[g_frame_buffer_index], 1);
            }
            GsSortFastSprite(&sprites[13], &g_ot_2d[g_frame_buffer_index], 1);
            timers[0][idx[0]]--;
            timers[1][idx[1]]--;
            which = 0;
            if (timers[0][idx[0]] == 0) {
                idx[0]++;
                which = 1;
            }
            if (timers[1][idx[1]] == 0) {
                idx[1]++;
                if (which == 0) {
                    which = 2;
                }
            }
            switch (which) {
            case 0:
                frame = order[0][0];
                break;
            case 1:
                frame = order[0][idx[0]];
                break;
            case 2:
                frame = order[1][idx[1]];
                break;
            }
            GsSortFastSprite(&sprites[frame], &g_ot_2d[g_frame_buffer_index], 1);
            if (rand() & 1) {
                sprites[2].y = 0xC5;
            } else {
                sprites[2].y = 0xBD;
            }
            GsSortFastSprite(&sprites[2], &g_ot_2d[g_frame_buffer_index], 1);
            sprite_draw(&glyphs[0], &g_ot_2d[g_frame_buffer_index], 1);
            sprite_draw(&glyphs[1], &g_ot_2d[g_frame_buffer_index], 1);
            glyphs[3].rotZ = (t % 16 * 256 + 256) % 4096;
            sprite_draw_rotated(&glyphs[3], &g_ot_2d[g_frame_buffer_index], 1);
            sprite_draw_rotated(&glyphs[2], &g_ot_2d[g_frame_buffer_index], 1);
            t++;
            if (idx[0] >= 5 || idx[1] >= 5) {
                choice = 0;
                t = 0;
                state++;
            }
            break;

        case 1:
            /* Transition into the menu. */
            shade = 0x80 - t * 4;
            sprites[0].r = shade;
            sprites[0].g = shade;
            sprites[0].b = shade;
            sprites[1].r = shade;
            sprites[1].g = shade;
            sprites[1].b = shade;
            for (i = 0; i < 2; i++) {
                GsSortFastSprite(&sprites[i], &g_ot_2d[g_frame_buffer_index], 1);
            }
            sprites[13].x = t * 105 / 32 + 111;
            sprites[13].y = t * -78 / 32 + 100;
            GsSortFastSprite(&sprites[13], &g_ot_2d[g_frame_buffer_index], 1);
            shade = 0x80 - t * 4;
            glyphs[0].r = shade;
            glyphs[0].g = shade;
            glyphs[0].b = shade;
            glyphs[1].r = shade;
            glyphs[1].g = shade;
            glyphs[1].b = shade;
            sprite_draw(&glyphs[0], &g_ot_2d[g_frame_buffer_index], 1);
            sprite_draw(&glyphs[1], &g_ot_2d[g_frame_buffer_index], 1);
            glyphs[3].rotZ = (t % 16 * 256 + 256) % 4096;
            glyphs[3].x = 0x98;
            glyphs[3].y = t * -54 / 32 + 0xAC;
            sprite_draw_rotated(&glyphs[3], &g_ot_2d[g_frame_buffer_index], 1);
            glyphs[2].r = shade;
            glyphs[2].g = shade;
            glyphs[2].b = shade;
            sprite_draw_rotated(&glyphs[2], &g_ot_2d[g_frame_buffer_index], 1);
            if (choice == 0) {
                sprites[14].cx = 0x160;
                sprites[15].cx = 0x150;
            } else {
                sprites[14].cx = 0x150;
                sprites[15].cx = 0x160;
            }
            GsSortFastSprite(&sprites[14], &g_ot_2d[g_frame_buffer_index], 1);
            GsSortFastSprite(&sprites[15], &g_ot_2d[g_frame_buffer_index], 1);
            if (choice == 0) {
                GsSortFastSprite(&sprites[16], &g_ot_2d[g_frame_buffer_index], 1);
            } else {
                GsSortFastSprite(&sprites[17], &g_ot_2d[g_frame_buffer_index], 1);
            }
            start_menu_update_panels(&polys[0], &polys[1], 0x40);
            GsSortPoly(&polys[0], &g_ot_2d[g_frame_buffer_index], 10);
            GsSortPoly(&polys[1], &g_ot_2d[g_frame_buffer_index], 10);
            t++;
            if (t >= 33) {
                busy = 1;
                t = 0;
                state++;
            }
            break;

        case 2:
            /* Left/right choice. */
            switch (g_pad_command) {
            case 1:
            case 2:
            case 3:
                snd_play_sfx(1);
                choice = 0;
                break;
            case 6:
            case 7:
            case 8:
                snd_play_sfx(1);
                choice = 1;
                break;
            case 17:
                snd_play_sfx(0);
                busy = 0;
                break;
            }
            DrawSync(0);
            gfx_frame_begin();
            GsSortFastSprite(&sprites[13], &g_ot_2d[g_frame_buffer_index], 1);
            glyphs[3].rotZ = (t % 64 / 4 * 256 + 256) % 4096;
            sprite_draw_rotated(&glyphs[3], &g_ot_2d[g_frame_buffer_index], 1);
            if (choice == 0) {
                sprites[14].cx = 0x160;
                sprites[15].cx = 0x150;
            } else {
                sprites[14].cx = 0x150;
                sprites[15].cx = 0x160;
            }
            GsSortFastSprite(&sprites[14], &g_ot_2d[g_frame_buffer_index], 1);
            GsSortFastSprite(&sprites[15], &g_ot_2d[g_frame_buffer_index], 1);
            if (choice == 0) {
                GsSortFastSprite(&sprites[16], &g_ot_2d[g_frame_buffer_index], 1);
            } else {
                GsSortFastSprite(&sprites[17], &g_ot_2d[g_frame_buffer_index], 1);
            }
            start_menu_update_panels(&polys[0], &polys[1], 0x40);
            GsSortPoly(&polys[0], &g_ot_2d[g_frame_buffer_index], 10);
            GsSortPoly(&polys[1], &g_ot_2d[g_frame_buffer_index], 10);
            t++;
            t %= 64;
            if (busy == 0) {
                i = 0;
                if (choice != 0) {
                    state++;
                } else {
                    state = 10;
                }
            }
            break;

        case 3:
            /* Slide in the confirm box. */
            fade = 0x80 - i * 4;
            sprites[13].r = fade;
            sprites[13].g = fade;
            sprites[13].b = fade;
            sprites[14].r = fade;
            sprites[14].g = fade;
            sprites[14].b = fade;
            glyphs[3].r = fade;
            glyphs[3].g = fade;
            glyphs[3].b = fade;
            GsSortFastSprite(&sprites[13], &g_ot_2d[g_frame_buffer_index], 1);
            glyphs[3].rotZ = (t % 64 / 4 * 256 + 256) % 4096;
            sprite_draw_rotated(&glyphs[3], &g_ot_2d[g_frame_buffer_index], 1);
            GsSortFastSprite(&sprites[14], &g_ot_2d[g_frame_buffer_index], 1);
            sprites[15].x = i * -91 / 32 + 106;
            sprites[15].y = i * 32 / 32 + 157;
            GsSortFastSprite(&sprites[15], &g_ot_2d[g_frame_buffer_index], 1);
            t++;
            start_menu_update_panels(&polys[0], &polys[1], 0x40);
            GsSortPoly(&polys[0], &g_ot_2d[g_frame_buffer_index], 10);
            GsSortPoly(&polys[1], &g_ot_2d[g_frame_buffer_index], 10);
            t %= 64;
            i++;
            if (i >= 33) {
                choice = 0;
                busy = 1;
                state++;
            }
            break;

        case 4:
            /* Up/down yes/no. */
            switch (g_pad_command) {
            case 2:
            case 4:
            case 7:
                snd_play_sfx(1);
                choice = 0;
                break;
            case 3:
            case 5:
            case 8:
                snd_play_sfx(1);
                choice = 1;
                break;
            case 17:
                snd_play_sfx(choice == 0 ? 0x1A : 0x1B);
                busy = 0;
                break;
            }
            glyphs[7].x = choice ? 0xE6 : 0x22;
            sprite_draw(&glyphs[4], &g_ot_2d[g_frame_buffer_index], 1);
            sprite_draw(&glyphs[5], &g_ot_2d[g_frame_buffer_index], 1);
            sprite_draw(&glyphs[6], &g_ot_2d[g_frame_buffer_index], 1);
            sprite_draw(&glyphs[7], &g_ot_2d[g_frame_buffer_index], 1);
            GsSortFastSprite(&sprites[15], &g_ot_2d[g_frame_buffer_index], 1);
            GsSortFastSprite(&sprites[18], &g_ot_2d[g_frame_buffer_index], 0);
            GsSortFastSprite(&sprites[19], &g_ot_2d[g_frame_buffer_index], 0);
            GsSortFastSprite(&sprites[20], &g_ot_2d[g_frame_buffer_index], 0);
            start_menu_update_panels(&polys[0], &polys[1], 0x40);
            GsSortPoly(&polys[0], &g_ot_2d[g_frame_buffer_index], 10);
            GsSortPoly(&polys[1], &g_ot_2d[g_frame_buffer_index], 10);
            if (busy == 0) {
                i = 0;
                if (choice == 0) {
                    sub = 0;
                    state++;
                } else {
                    state = 9;
                }
            }
            break;

        case 5:
            /* Memory card access. */
            switch (sub) {
            case 0:
                sub++;
                mcard_accept();
                break;
            case 1:
                r = mcard_poll();
                if (r != 1) {
                    r = -r;
                    switch (r) {
                    case 1:
                        snd_play_sfx(0x1C);
                        sub = 10;
                        wait = 60;
                        break;
                    case 2:
                        snd_play_sfx(0x1C);
                        sub = 20;
                        wait = 60;
                        break;
                    case 3:
                        sub = 0;
                        break;
                    case 4:
                        snd_play_sfx(0x1C);
                        sub = 30;
                        wait = 60;
                        break;
                    }
                } else {
                    sub = 100;
                }
                break;
            case 100:
                switch (-mcard_open_save_file(0)) {
                case 0:
                    sub++;
                    wait = 10;
                    break;
                case 1:
                    snd_play_sfx(0x1C);
                    sub = 10;
                    wait = 60;
                    break;
                case 2:
                    snd_play_sfx(0x1C);
                    sub = 20;
                    wait = 60;
                    break;
                case 4:
                    snd_play_sfx(0x1C);
                    sub = 30;
                    wait = 60;
                    break;
                case 5:
                    snd_play_sfx(0x1C);
                    sub = 40;
                    wait = 60;
                    break;
                }
                break;
            case 101:
                buf = (u32 *)heap_alloc(0x6000);
                mcard_read_save(buf, save_build_image((u8 *)buf) << 7);
                msgbox_draw(0xF);
                sub = 2;
                break;
            case 2:
                msgbox_draw(0xF);
                r = mcard_poll();
                if (r != 0) {
                    if (r == 1) {
                        if (save_apply_image((u8 *)buf) != 0) {
                            sub++;
                        } else {
                            sub += 2;
                        }
                        wait = 30;
                        mcard_close_file();
                    } else {
                        r = -r;
                        switch (r) {
                        case 1:
                            sub = 0;
                            if (--wait == 0) {
                                snd_play_sfx(0x1C);
                                sub = 10;
                                wait = 60;
                                mcard_close_file();
                            }
                            mcard_close_file();
                            heap_free(buf);
                            break;
                        case 2:
                            snd_play_sfx(0x1C);
                            sub = 20;
                            wait = 60;
                            heap_free(buf);
                            mcard_close_file();
                            break;
                        case 3:
                            sub = 0;
                            heap_free(buf);
                            mcard_close_file();
                            break;
                        }
                    }
                }
                break;
            case 3:
                if (--wait == 0) {
                    site_reload(); /* port: the PS1 call passed 0x10, which the (void) callee ignores */
                    heap_free(buf);
                    goto done;
                }
                msgbox_draw(0x10);
                break;
            case 4:
                if (--wait == 0) {
                    heap_free(buf);
                    state = 9;
                }
                msgbox_draw(0xE);
                break;
            case 10:
                if (--wait == 0) {
                    state = 9;
                }
                msgbox_draw(1);
                break;
            case 20:
                if (--wait == 0) {
                    state = 9;
                }
                msgbox_draw(6);
                break;
            case 30:
                if (--wait == 0) {
                    state = 9;
                }
                msgbox_draw(0x11);
                break;
            case 40:
                if (--wait == 0) {
                    state = 9;
                }
                msgbox_draw(0xD);
                break;
            }
            sprite_draw(&glyphs[4], &g_ot_2d[g_frame_buffer_index], 2);
            sprite_draw(&glyphs[5], &g_ot_2d[g_frame_buffer_index], 2);
            sprite_draw(&glyphs[6], &g_ot_2d[g_frame_buffer_index], 2);
            sprite_draw(&glyphs[7], &g_ot_2d[g_frame_buffer_index], 2);
            GsSortFastSprite(&sprites[15], &g_ot_2d[g_frame_buffer_index], 2);
            GsSortFastSprite(&sprites[18], &g_ot_2d[g_frame_buffer_index], 1);
            GsSortFastSprite(&sprites[19], &g_ot_2d[g_frame_buffer_index], 1);
            GsSortFastSprite(&sprites[20], &g_ot_2d[g_frame_buffer_index], 1);
            start_menu_update_panels(&polys[0], &polys[1], 0x40);
            GsSortPoly(&polys[0], &g_ot_2d[g_frame_buffer_index], 10);
            GsSortPoly(&polys[1], &g_ot_2d[g_frame_buffer_index], 10);
            break;

        case 9:
            /* Slide the confirm box out, back to the choice. */
            fade = i * 4;
            sprites[13].r = fade;
            sprites[13].g = fade;
            sprites[13].b = fade;
            sprites[14].r = fade;
            sprites[14].g = fade;
            sprites[14].b = fade;
            glyphs[3].r = fade;
            glyphs[3].g = fade;
            glyphs[3].b = fade;
            GsSortFastSprite(&sprites[13], &g_ot_2d[g_frame_buffer_index], 1);
            glyphs[3].rotZ = (t % 64 / 4 * 256 + 256) % 4096;
            sprite_draw_rotated(&glyphs[3], &g_ot_2d[g_frame_buffer_index], 1);
            GsSortFastSprite(&sprites[14], &g_ot_2d[g_frame_buffer_index], 1);
            sprites[15].x = i * 91 / 32 + 15;
            sprites[15].y = i * -32 / 32 + 189;
            GsSortFastSprite(&sprites[15], &g_ot_2d[g_frame_buffer_index], 1);
            t++;
            start_menu_update_panels(&polys[0], &polys[1], 0x40);
            GsSortPoly(&polys[0], &g_ot_2d[g_frame_buffer_index], 10);
            GsSortPoly(&polys[1], &g_ot_2d[g_frame_buffer_index], 10);
            t %= 64;
            i++;
            if (i >= 33) {
                choice = 1;
                busy = 1;
                state = 2;
            }
            break;

        case 10:
            /* Slide towards the name entry. */
            fade = 0x80 - i * 4;
            sprites[13].r = fade;
            sprites[13].g = fade;
            sprites[13].b = fade;
            sprites[15].r = fade;
            sprites[15].g = fade;
            sprites[15].b = fade;
            glyphs[3].r = fade;
            glyphs[3].g = fade;
            glyphs[3].b = fade;
            GsSortFastSprite(&sprites[13], &g_ot_2d[g_frame_buffer_index], 1);
            glyphs[3].rotZ = (t % 64 / 4 * 256 + 256) % 4096;
            sprite_draw_rotated(&glyphs[3], &g_ot_2d[g_frame_buffer_index], 1);
            sprites[14].x = i * 71 / 32 + 83;
            sprites[14].y = i * -54 / 32 + 73;
            GsSortFastSprite(&sprites[14], &g_ot_2d[g_frame_buffer_index], 1);
            t++;
            GsSortFastSprite(&sprites[15], &g_ot_2d[g_frame_buffer_index], 1);
            start_menu_update_panels(&polys[0], &polys[1], 0x40);
            GsSortPoly(&polys[0], &g_ot_2d[g_frame_buffer_index], 10);
            GsSortPoly(&polys[1], &g_ot_2d[g_frame_buffer_index], 10);
            t %= 64;
            i++;
            if (i >= 33) {
                choice = 0;
                busy = 1;
                state++;
            }
            break;

        case 11:
            /* Name entry: leave the menu once a name was entered. */
            if (name_entry_run() != 0) {
                goto done;
            }
            i = 0;
            state++;
            break;

        case 12:
            /* Name entry cancelled: slide back to the choice. */
            fade = i * 4;
            sprites[13].r = fade;
            sprites[13].g = fade;
            sprites[13].b = fade;
            sprites[15].r = fade;
            sprites[15].g = fade;
            sprites[15].b = fade;
            glyphs[3].r = fade;
            glyphs[3].g = fade;
            glyphs[3].b = fade;
            GsSortFastSprite(&sprites[13], &g_ot_2d[g_frame_buffer_index], 1);
            glyphs[3].rotZ = (t % 64 / 4 * 256 + 256) % 4096;
            sprite_draw_rotated(&glyphs[3], &g_ot_2d[g_frame_buffer_index], 1);
            sprites[14].x = i * -71 / 32 + 154;
            sprites[14].y = i * 54 / 32 + 19;
            GsSortFastSprite(&sprites[14], &g_ot_2d[g_frame_buffer_index], 1);
            t++;
            GsSortFastSprite(&sprites[15], &g_ot_2d[g_frame_buffer_index], 1);
            start_menu_update_panels(&polys[0], &polys[1], 0x40);
            GsSortPoly(&polys[0], &g_ot_2d[g_frame_buffer_index], 10);
            GsSortPoly(&polys[1], &g_ot_2d[g_frame_buffer_index], 10);
            t %= 64;
            i++;
            if (i >= 33) {
                state = 2;
                choice = 0;
                busy = 1;
            }
            break;
        }

        VSync(0);
        ResetGraph(1);
        GsSwapDispBuff();
        GsSortClear(0, 0, 0, &g_ot_2d[g_frame_buffer_index]);
        GsDrawOt(&g_ot_2d[g_frame_buffer_index]);
    }

done:
    DrawSync(0);
    VSync(0);
    ResetGraph(1);
    GsSwapDispBuff();
    rect.x = 0;
    rect.y = 0;
    rect.w = 0x280;
    rect.h = 0x1E0;
    ClearImage(&rect, 0, 0, 0);
    bg_curves_free();
    rect.x = 0x140;
    rect.y = 0x1E0;
    rect.w = 0x100;
    rect.h = 0x20;
    LoadImage2(&rect, save);
    heap_free(g_menu_scroll_tim);
    heap_free(save);
}
#else
INCLUDE_RODATA("asm/nonmatchings/game/80033820", start_menu_flicker_order_init);
INCLUDE_ASM("asm/nonmatchings/game/80033820", start_menu_run);
#endif

extern s16 g_name_entry_mesh_x[][10]; /* mesh x coordinates */
extern u8 g_name_entry_mesh_y[][10];  /* mesh y coordinates */
extern u8 g_name_entry_cell_shade[];      /* cell brightness */
extern u8 g_name_entry_glyph_pos[][2];   /* glyph positions */

/* Initialise the name-entry screen: the warped 9x9 character mesh, labels and sprites. */
void name_entry_init(MenuWork *w) {
    s32 i;
    s32 j;

    for (i = 0; i < 9; i++) {
        for (j = 0; j < 9; j++) {
            setlen(&w->grid[i * 9 + j], 9);
            w->grid[i * 9 + j].code = 0x2C;
            w->grid[i * 9 + j].clut = 0x3C54;
            w->grid[i * 9 + j].r0 = g_name_entry_cell_shade[i * 9 + j];
            w->grid[i * 9 + j].g0 = g_name_entry_cell_shade[i * 9 + j];
            w->grid[i * 9 + j].b0 = g_name_entry_cell_shade[i * 9 + j];
            w->grid[i * 9 + j].x0 = g_name_entry_mesh_x[i + 1][j];
            w->grid[i * 9 + j].y0 = g_name_entry_mesh_y[i + 1][j];
            w->grid[i * 9 + j].x1 = g_name_entry_mesh_x[i][j];
            w->grid[i * 9 + j].y1 = g_name_entry_mesh_y[i][j];
            w->grid[i * 9 + j].x2 = g_name_entry_mesh_x[i + 1][j + 1];
            w->grid[i * 9 + j].y2 = g_name_entry_mesh_y[i + 1][j + 1];
            w->grid[i * 9 + j].x3 = g_name_entry_mesh_x[i][j + 1];
            w->grid[i * 9 + j].y3 = g_name_entry_mesh_y[i][j + 1];
        }
    }

    setlen(&w->cursor, 9);
    w->cursor.code = 0x2C;
    w->cursor.clut = 0x3C55;
    w->cursor.r0 = 0x80;
    w->cursor.g0 = 0x80;
    w->cursor.b0 = 0x80;
    w->cursor.x0 = 0xA7;
    w->cursor.y0 = 0x66;
    w->cursor.x1 = 0xEC;
    w->cursor.y1 = 0x74;
    w->cursor.x2 = 0x92;
    w->cursor.y2 = 0x7E;
    w->cursor.x3 = 0xDB;
    w->cursor.y3 = 0x8F;

    w->labels[0].attribute = 0;
    w->labels[0].x = 0x9A;
    w->labels[0].y = 0x13;
    w->labels[0].w = 0xA8;
    w->labels[0].h = 0x10;
    w->labels[0].u = 0;
    w->labels[0].v = 0x18;
    w->labels[0].tpage = 0x16;
    w->labels[0].cx = 0x160;
    w->labels[0].cy = 0xF2;
    w->labels[0].r = 0x80;
    w->labels[0].g = 0x80;
    w->labels[0].b = 0x80;

    w->labels[1].attribute = 0;
    w->labels[1].x = 0x92;
    w->labels[1].y = 0x2A;
    w->labels[1].w = 0xA8;
    w->labels[1].h = 0x10;
    w->labels[1].u = 0;
    w->labels[1].v = 0x28;
    w->labels[1].tpage = 0x16;
    w->labels[1].cx = 0x160;
    w->labels[1].cy = 0xF2;
    w->labels[1].r = 0x80;
    w->labels[1].g = 0x80;
    w->labels[1].b = 0x80;

    w->labels[2].attribute = 0;
    w->labels[2].x = 0x40;
    w->labels[2].y = 0x20;
    w->labels[2].w = 0x100;
    w->labels[2].h = 8;
    w->labels[2].u = 0;
    w->labels[2].v = 0;
    w->labels[2].tpage = 0x16;
    w->labels[2].cx = 0x140;
    w->labels[2].cy = 0xF2;
    w->labels[2].r = 0x80;
    w->labels[2].g = 0x80;
    w->labels[2].b = 0x80;

    w->labels[3].attribute = 0;
    w->labels[3].x = 0;
    w->labels[3].y = 0x97;
    w->labels[3].w = 0x98;
    w->labels[3].h = 8;
    w->labels[3].u = 0;
    w->labels[3].v = 0x38;
    w->labels[3].tpage = 0x16;
    w->labels[3].cx = 0x140;
    w->labels[3].cy = 0xF2;
    w->labels[3].r = 0x80;
    w->labels[3].g = 0x80;
    w->labels[3].b = 0x80;

    w->labels[4].attribute = 0;
    w->labels[4].x = 0x98;
    w->labels[4].y = 0x67;
    w->labels[4].w = 0x48;
    w->labels[4].h = 0x38;
    w->labels[4].u = 0xA8;
    w->labels[4].v = 8;
    w->labels[4].tpage = 0x16;
    w->labels[4].cx = 0x140;
    w->labels[4].cy = 0xF2;
    w->labels[4].r = 0x80;
    w->labels[4].g = 0x80;
    w->labels[4].b = 0x80;

    for (i = 0; i < 8; i++) {
        w->glyphs[i].attribute = 0;
        w->glyphs[i].tpage = 0x15;
        w->glyphs[i].clut = 0x5015;
        w->glyphs[i].scaleX = 0x1000;
        w->glyphs[i].scaleY = 0x1000;
        w->glyphs[i].w = 0x10;
        w->glyphs[i].h = 0x10;
        w->glyphs[i].rotX = 0;
        w->glyphs[i].rotY = 0;
        w->glyphs[i].x = g_name_entry_glyph_pos[i][0];
        w->glyphs[i].y = g_name_entry_glyph_pos[i][1];
        w->glyphs[i].u = 0;
        w->glyphs[i].v = 8;
        w->glyphs[i].r = 0x80;
        w->glyphs[i].g = 0x80;
        w->glyphs[i].b = 0x80;
    }
    w->glyphs[0].rotZ = 0x100;
    w->glyphs[1].rotZ = 0x300;
    w->glyphs[2].rotZ = 0x100;
    w->glyphs[3].rotZ = 0x300;
    w->glyphs[4].rotZ = 0x100;
    w->glyphs[5].rotZ = 0x300;
    w->glyphs[6].rotZ = 0x100;
    w->glyphs[7].rotZ = 0x300;

    for (i = 0; i < 9; i++) {
        w->sprites[i].x = 0x88 - i * 16;
        w->sprites[i].attribute = 0;
        w->sprites[i].y = 0x87;
        w->sprites[i].w = 0x10;
        w->sprites[i].h = 0x10;
        w->sprites[i].tpage = 0x16;
        w->sprites[i].cx = 0x140;
        w->sprites[i].cy = 0xF3;
        w->sprites[i].r = 0x80;
        w->sprites[i].g = 0x80;
        w->sprites[i].b = 0x80;
    }
    w->row = 5;
    w->col = 1;
    w->count = 0;
}

#ifdef NON_MATCHING
/* 4 diffs: scheduling of the cell address vs. the u coordinate computation.
 * sched1's "birthing" rule gives an insn that sets a single-set pseudo
 * LAUNCH priority (0x7f000001 in -dS). `p` has one set, so its address chain
 * is scheduled first in reverse and ends up after the u chain; the original
 * order needs p set twice (REG_N_SETS > 1) with neither set optimised away.
 * Tried: reordering p/u/u1, s32/u8/u32 for u/u1, no p variable, p set in each tpage
 * branch (28-40), dead extra sets of p (removed, still 4). */
/* Lay out and draw the visible 9x9 window of the character table around the cursor. */
void name_entry_draw_grid(MenuWork *w) {
    POLY_FT4 *p;
    s32 i;
    s32 j;
    s32 row;
    s32 r;
    u32 col;
    s32 idx;
    s16 u;
    s16 u1;
    s32 v;

    for (i = 0; i < 9; i++) {
        row = w->row - 3 + i;
        if (row < 0) {
            continue;
        }
        if (row >= 13) {
            break;
        }
        for (j = 0; j < 9; j++) {
            col = w->col - 4 + j;
            if (col >= 5) {
                continue;
            }
            r = row;
            idx = i * 9 + j;
            if (r < 5) {
                w->grid[idx].tpage = 7;
            } else if (r < 10) {
                w->grid[idx].tpage = 6;
                r -= 5;
            } else {
                w->grid[idx].tpage = 5;
                r -= 10;
            }
            u = (4 - r) * 48;
            p = &w->grid[idx];
            u1 = u + 47;
            p->u0 = u;
            p->u2 = u;
            p->v0 = col * 32;
            p->v1 = col * 32;
            p->u1 = u1;
            p->v2 = col * 32 + 31;
            p->u3 = u1;
            p->v3 = col * 32 + 31;
            if (p->x0 > 0 && p->x1 > 0 && p->x2 > 0 && p->x3 > 0) {
                if (i == 3 && (j == 3 || j == 4)) {
                    if (j == 4) {
                        w->cursor.tpage = p->tpage;
                        w->cursor.u0 = p->u0;
                        w->cursor.v0 = p->v0;
                        w->cursor.u1 = p->u1;
                        w->cursor.v1 = p->v1;
                        w->cursor.u2 = p->u2;
                        w->cursor.v2 = p->v2;
                        w->cursor.u3 = p->u3;
                        w->cursor.v3 = p->v3;
                        GsSortPoly(&w->cursor, &g_ot_2d[g_frame_buffer_index], 9);
                    }
                } else {
                    GsSortPoly(&w->grid[idx], &g_ot_2d[g_frame_buffer_index], 10);
                }
            }
        }
    }
}
#else
INCLUDE_ASM("asm/nonmatchings/game/80033820", name_entry_draw_grid);
#endif

/* Draw the entered characters (from a 15-column font sheet) and the cursor sprite. */
void name_entry_draw_chars(MenuWork *w) {
    s32 i;
    s32 j;

    j = w->count - 1;
    for (i = 0; i < w->count; i++, j--) {
        if (w->count != 8) {
            w->sprites[i + 1].u = (w->chars[j] % 15) * 16;
            w->sprites[i + 1].v = (w->chars[j] / 15) * 16 + 0x41;
            GsSortFastSprite(&w->sprites[i + 1], &g_ot_2d[g_frame_buffer_index], 2);
        } else {
            w->sprites[i].u = (w->chars[j] % 15) * 16;
            w->sprites[i].v = (w->chars[j] / 15) * 16 + 0x41;
            GsSortFastSprite(&w->sprites[i], &g_ot_2d[g_frame_buffer_index], 2);
        }
    }
    if (w->count != 8) {
        w->sprites[0].u = 0xE0;
        w->sprites[0].v = 0x81;
        GsSortFastSprite(&w->sprites[0], &g_ot_2d[g_frame_buffer_index], 2);
    }
}

/* Move the cursor down one row, skipping the gaps in the character table. */
void name_entry_cursor_down(MenuWork *w) {
    s16 row = w->row;

    if (row < 12) {
        if (row == 6) {
            if (w->col != 1 && w->col != 3) {
                w->row = row + 1;
            } else {
                w->row += 2;
            }
        } else if (row == 8) {
            if (w->col == 0) {
                w->row = row + 1;
            } else if (w->col == 4) {
                w->row = row + 1;
            } else {
                w->row = row + 2;
            }
        } else if (row == 10) {
            if (w->col == 1) {
            } else if (w->col == 3) {
                w->row = row + 2;
            } else {
                w->row = row + 1;
            }
        } else {
            w->row = row + 1;
        }
    }
}

/* Matches; needs .rodata for its jump table. */
/* Handle the confirm button on the character table: enter a character, apply a
 * modifier (small kana, voiced marks, long vowel) to the last one, etc. */
void name_entry_press_cell(MenuWork *w) {
    u8 c;
    s32 i;
    s32 one;

    if (w->count < 8) {
        if (w->row < 10) {
            if (*(s32 *)&w->row == 0x40009 && w->count == 0) {
                return;
            }
            if (w->row < 7) {
                w->chars[w->count] = w->col + w->row * 5;
            } else if (w->row == 7) {
                w->chars[w->count] = w->col / 2 + 0x23;
            } else if (w->row == 8) {
                w->chars[w->count] = w->col + 0x26;
            } else {
                w->chars[w->count] = w->col / 4 + 0x2B;
            }
            w->count++;
            return;
        }
        one = 1; /* kept in a register for the w->col check below */
        if (w->row == 10) {
            if (w->count == 0) {
                return;
            }
            switch (w->chars[w->count - 1]) {
            case 0x3E:
                if (w->col != one) {
                    return;
                }
                w->chars[w->count] = 0x4C;
                break;
            case 0x3F:
                if (w->col != 2) {
                    return;
                }
                w->chars[w->count] = 0x4D;
                break;
            case 0x1B:
                w->chars[w->count] = w->col + 0x4B;
                break;
            default:
                return;
            }
            w->count++;
            return;
        }
        if (w->row == 11) {
            if (w->count == 0) {
                return;
            }
            switch (w->chars[w->count - 1]) {
            case 6:
            case 11:
            case 16:
            case 21:
            case 26:
            case 31:
            case 39:
            case 50:
            case 55:
            case 60:
            case 65:
            case 70:
                w->chars[w->count] = w->col / 2 + 0x2E;
                w->count++;
                break;
            }
            return;
        }
    }
    if (w->count == 0 || w->row != 12) {
        return;
    }
    switch (w->col) {
    case 0:
        if (w->count < 8 && w->chars[w->count - 1] != 0x2D) {
            w->chars[w->count] = 0x2D;
            w->count++;
        }
        break;
    case 2:
        if (w->chars[w->count - 1] >= 5 && w->chars[w->count - 1] < 20 && w->chars[w->count - 1] != 16 && w->chars[w->count - 1] != 17) {
            w->chars[w->count - 1] += 44;
            return;
        }
        c = w->chars[w->count - 1];
        if (c >= 25 && c < 30) {
            w->chars[w->count - 1] = c + 39;
        }
        break;
    case 3:
        c = w->chars[w->count - 1];
        if (c >= 25 && c < 30) {
            w->chars[w->count - 1] = c + 44;
        }
        break;
    case 4:
        if (w->count < 8) {
            i = w->chars[w->count - 1];
            if (i < 0x2C || (i >= 0x2E && i != 0x50)) {
                w->chars[w->count] = 0x50;
                w->count++;
            }
        }
        break;
    }
}

extern s16 g_pad_command ; /* pad input (button index) */
void sprite_draw_rotated(Glyph *g, GsOT *ot, u16 pri);

/* Cursor movement helpers of the name-entry table (inlined in name_entry_input_loop). */
static __inline__ void MoveUp(MenuWork *w) {
    s16 row = w->row;

    if (row > 0) {
        if (row == 12) {
            if (w->col != 3) {
                w->row = row - 1;
            } else {
                w->row = row - 2;
            }
        } else if (row == 10) {
            if (w->col == 0) {
                w->row = row - 1;
            } else if (w->col == 4) {
                w->row = row - 1;
            } else {
                w->row = row - 2;
            }
        } else if (row == 8) {
            if (w->col != 1 && w->col != 3) {
                w->row = row - 1;
            } else {
                w->row -= 2;
            }
        } else {
            w->row = row - 1;
        }
    }
}

static __inline__ void MoveLeft(MenuWork *w) {
    s16 col = w->col;

    if (col > 0) {
        if (w->row == 7 || w->row == 11) {
            if (col == 2) {
                w->col = 0;
            } else if (col == 4) {
                w->col = 2;
            }
        } else if (w->row == 9) {
            w->col = 0;
        } else if (w->row == 12 && col == 2) {
            w->col = 0;
        } else {
            w->col = col - 1;
        }
    }
}

static __inline__ void MoveRight(MenuWork *w) {
    s16 col = w->col;

    if (col < 4) {
        if (w->row == 7 || w->row == 11) {
            if (col == 0) {
                w->col = 2;
            } else if (col == 2) {
                w->col = 4;
            }
        } else if (w->row == 9) {
            if (col == 0) {
                w->col = 4;
            }
        } else if (w->row == 12 && col == 0) {
            w->col = 2;
        } else {
            w->col = col + 1;
        }
    }
}

/* Name-entry main loop: returns 1 when the name is confirmed, 0 when cancelled. */
s32 name_entry_input_loop(MenuWork *w) {
    s32 i;

    for (;;) {
        DrawSync(0);
        gfx_frame_begin();
        pad_read_command();
        switch (g_pad_command) {
        case 1:
            MoveLeft(w);
            break;
        case 2:
            name_entry_cursor_down(w);
            MoveLeft(w);
            break;
        case 3:
            MoveUp(w);
            MoveLeft(w);
            break;
        case 4:
            name_entry_cursor_down(w);
            break;
        case 5:
            MoveUp(w);
            break;
        case 6:
            MoveRight(w);
            break;
        case 7:
            name_entry_cursor_down(w);
            MoveRight(w);
            break;
        case 8:
            MoveUp(w);
            MoveRight(w);
            break;
        case 17:
            snd_play_sfx(0);
            name_entry_press_cell(w);
            break;
        case 18:
            snd_play_sfx(0x1B);
            if (w->count != 0) {
                if (w->count > 0) {
                    w->count--;
                }
            } else {
                return 0;
            }
            break;
        case 20:
            if (w->count != 0) {
                snd_play_sfx(0x1E);
                return 1;
            }
            break;
        }
        name_entry_draw_grid(w);
        for (i = 4; i >= 0; i--) {
            if (i != 1 || w->count != 0) {
                GsSortFastSprite(&w->labels[i], &g_ot_2d[g_frame_buffer_index], 1);
            }
        }
        for (i = 0; i < 8; i++) {
            if (7 - i != w->count) {
                sprite_draw_rotated(&w->glyphs[i], &g_ot_2d[g_frame_buffer_index], 3);
            }
        }
        name_entry_draw_chars(w);
        VSync(0);
        ResetGraph(1);
        GsSwapDispBuff();
        GsSortClear(0, 0, 0, &g_ot_2d[g_frame_buffer_index]);
        GsDrawOt(&g_ot_2d[g_frame_buffer_index]);
    }
}

extern u8 g_romaji_vowels[]; /* vowels "AIUEO" */
extern u8 g_romaji_consonants[]; /* consonants of rows K..M */
extern u8 g_romaji_y_vowels[]; /* "AUO" (ya/yu/yo) */
extern u8 g_romaji_voiced_consonants[]; /* voiced consonants G, Z, D, B, P */
extern u8 g_player_name[];

/* Convert the entered kana codes to a romaji string in g_player_name. */
void name_entry_to_romaji(MenuWork *w) {
    s32 i;
    s32 len;
    s32 c;

    i = 0;
    len = 0;
    for (; i < w->count; i++) {
        c = w->chars[i];
        if (c < 5) {
            g_player_name[len++] = g_romaji_vowels[c];
        } else if (c < 35) {
            if (c % 5 == 1 && i + 1 < w->count && w->chars[i + 1] >= 0x2E && w->chars[i + 1] < 0x31) {
                g_player_name[len++] = g_romaji_consonants[c / 5 - 1];
                g_player_name[len++] = 'Y';
                g_player_name[len++] = g_romaji_y_vowels[w->chars[i + 1] - 0x2E];
                i++;
            } else {
                g_player_name[len++] = g_romaji_consonants[c / 5 - 1];
                g_player_name[len++] = g_romaji_vowels[c % 5];
            }
        } else if (c < 38) {
            g_player_name[len++] = 'Y';
            g_player_name[len++] = g_romaji_y_vowels[c - 35];
        } else if (c < 43) {
            c -= 38;
            if (c == 1 && i + 1 < w->count && w->chars[i + 1] >= 0x2E && w->chars[i + 1] < 0x31) {
                g_player_name[len++] = 'R';
                g_player_name[len++] = 'Y';
                g_player_name[len++] = g_romaji_y_vowels[w->chars[i + 1] - 0x2E];
                i++;
            } else {
                g_player_name[len++] = 'R';
                g_player_name[len++] = g_romaji_vowels[c];
            }
        } else if (c == 43) {
            g_player_name[len++] = 'W';
            g_player_name[len++] = 'A';
        } else if (c == 44) {
            g_player_name[len++] = 'N';
            g_player_name[len++] = 'N';
        } else if (c == 45) {
            g_player_name[len++] = '#';
        } else if (c >= 49 && c < 70) {
            c -= 49;
            if (c % 5 == 1 && i + 1 < w->count && w->chars[i + 1] >= 0x2E && w->chars[i + 1] < 0x31) {
                g_player_name[len++] = g_romaji_voiced_consonants[c / 5];
                g_player_name[len++] = 'Y';
                g_player_name[len++] = g_romaji_y_vowels[w->chars[i + 1] - 0x2E];
                i++;
            } else {
                g_player_name[len++] = g_romaji_voiced_consonants[c / 5];
                g_player_name[len++] = g_romaji_vowels[c % 5];
            }
        } else if (c >= 75 && c < 80) {
            c -= 75;
            switch (w->chars[i - 1]) {
            case 0x3E:
                g_player_name[len - 2] = 'D';
                g_player_name[len - 1] = 'I';
                break;
            case 0x3F:
                g_player_name[len - 2] = 'D';
                g_player_name[len - 1] = 'U';
                break;
            case 0x1B:
                g_player_name[len - 2] = 'F';
                g_player_name[len - 1] = g_romaji_vowels[c % 5];
                break;
            }
        } else if (c == 0x50) {
            g_player_name[len++] = '-';
        }
    }
    g_player_name[len] = 0;
}

s32 name_entry_run(void) {
    s32 unused[2];
    MenuWork work;
    MenuWork *p;
    s32 result;

    p = &work;
    name_entry_init(p);
    result = name_entry_input_loop(p);
    name_entry_to_romaji(p);
    return result;
}

/* Evaluate a piecewise-linear curve at x (clamped to its end points). */
s32 curve_eval_linear(u32 x, CurvePoint *curve, u32 count) {
    u32 i;
    s32 result;
    float t;

    if (count < 2) {
        return 0;
    }
    if (x <= curve[0].x) {
        return curve[0].y;
    }
    if (x >= curve[count - 1].x) {
        return curve[count - 1].y;
    }
    for (i = 0; i < count; i++) {
        if (x < curve[i].x) {
            t = (x - curve[i - 1].x) / (curve[i].x - curve[i - 1].x);
            result = (1.0f - t) * curve[i - 1].y + t * curve[i].y;
            break;
        }
    }
    return result;
}


/* Clamp each sample's magnitude to a 3-point envelope curve spanning the total length. */
void voice_apply_envelope(s16 *samples) {
    s32 i;
    s32 limit;

    g_voice_envelope[0].x = 0;
    g_voice_envelope[1].x = g_voice_pcm_size / 4;
    g_voice_envelope[2].x = g_voice_envelope[1].x + g_voice_envelope[1].x;
    for (i = g_voice_envelope[0].x; i < g_voice_envelope[2].x; i++) {
        limit = curve_eval_linear(i, g_voice_envelope, 3);
        if (*samples >= 0) {
            if (limit < *samples) {
                *samples = limit;
            }
        } else {
            limit = -limit;
            if (*samples < limit) {
                *samples = limit;
            }
        }
        samples++;
    }
}

/* Load a named file and append it to the file list (used to stitch audio/data segments). Always returns 1. */
s32 voice_load_clip(char *name) {
    s16 i;
    s16 continued;
    s32 head;
    u16 tail;
    void *data; /* port: s32 on the PS1 */
    s32 handle;

    continued = i = 0;
    for (; name[i] != 0; i++) {
        if (name[i] == '_') {
            continued = 1;
            break;
        }
    }
    if (g_voice_clip_count == 0) {
        head = 0;
        tail = 1;
    } else {
        tail = head = 1;
        if (!continued) {
            head = 0;
            if (g_voice_clip_count > 0) {
                g_voice_clips[g_voice_clip_count - 1].tail = 0;
            }
        }
    }

    data = heap_alloc(g_voice_file_table[FindName(name)].size);
    g_voice_clips[g_voice_clip_count].data = data;
    handle = cd_load_archive_entry(7, FindName(name), g_voice_file_table, data);
    while (cd_poll_load(handle) == 0) {
        loading_wait_tick();
    }
    g_voice_clips[g_voice_clip_count].offset = head;
    g_voice_clips[g_voice_clip_count].tail = tail;
    g_voice_clips[g_voice_clip_count].continued = continued;
    g_voice_clips[g_voice_clip_count].size = g_voice_file_table[FindName(name)].size;
    g_voice_clip_count++;
    return 1;
}


/* Inlined helper: nonzero if `name` is present in the name table g_voice_name_table. */
static __inline__ s32 NameExists(char *name) {
    char line[16];
    u8 *p;
    u32 i;
    s32 len;
    u8 c;

    i = 0;
    p = g_voice_name_table;
    do {
        len = 0;
        while ((c = *p) != '\r') {
            p++;
            line[len++] = c;
        }
        line[len] = 0;
        if (strncmp(name, line, len) == 0) {
            return 1;
        }
        i++;
        p += 2;
    } while (i < 0x27E);
    return 0;
}

/* Build a voice clip for a romaji string: load one WAV per mora (with a
 * transition sample from the previous vowel when available), stitch them into
 * one buffer and return it. */
u8 *voice_synth_romaji(u8 *text, s32 *outSize, u8 *outCount) {
    char vowels[7] = "AIUEO#-";
    char name[16];
    char syl[4];
    void *file; /* port: s32 on the PS1 */
    s32 handle;
    s16 i;
    s16 matched;

    g_voice_prev_vowel = '\\';
    g_voice_pcm_size = 0;
    g_voice_clip_count = 0;
    file = heap_alloc(g_bin_file_table[0x29]);
    handle = cd_load_archive_entry(3, 0x14, g_bin_file_table, file);
    while (cd_poll_load(handle) == 0) {
        loading_wait_tick();
    }
    g_voice_name_table = lz_decompress(file, D_80098F8C[0]);
    heap_free(file);

    while (*text != 0) {
        matched = 0;
        for (i = 0; i < 7; i++) {
            if (*text == vowels[i]) {
                if (*text != '-') {
                    if (g_voice_prev_vowel != '\\') {
                        sprintf(name, "%c_%c.WAV", g_voice_prev_vowel, vowels[i]);
                        if (!NameExists(name)) {
                            sprintf(name, "%c.WAV", vowels[i]);
                        }
                    } else {
                        sprintf(name, "%c.WAV", vowels[i]);
                    }
                } else {
                    sprintf(name, "%c_%c.WAV", g_voice_prev_vowel, g_voice_prev_vowel);
                }
                voice_load_clip(name);
                matched = 1;
                g_voice_prev_vowel = vowels[i];
                text++;
                break;
            }
        }
        if (!matched) {
            syl[0] = *text++;
            syl[1] = *text++;
            syl[2] = 0;
            if (syl[1] == 'Y' || syl[1] == 'H') {
                syl[2] = *text++;
                syl[3] = 0;
            }
            if (g_voice_prev_vowel != '\\') {
                sprintf(name, "%c_%s.WAV", g_voice_prev_vowel, syl);
                if (!NameExists(name)) {
                    sprintf(name, "%s.WAV", syl);
                }
            } else {
                sprintf(name, "%s.WAV", syl);
            }
            if (syl[1] == 'Y' || syl[1] == 'H') {
                g_voice_prev_vowel = syl[2];
            } else {
                g_voice_prev_vowel = syl[1];
            }
            voice_load_clip(name);
        }
    }
    if (g_voice_clip_count > 0) {
        g_voice_clips[g_voice_clip_count - 1].tail = 0;
    }
    voice_trim_clips();
    g_voice_pcm = (u8 *)heap_alloc(g_voice_pcm_size);
    voice_concat_clips(g_voice_pcm);
    voice_apply_envelope((s16 *)g_voice_pcm);
    *outSize = g_voice_pcm_size;
    *outCount = g_voice_clip_count;
    heap_free(g_voice_name_table);
    return g_voice_pcm;
}

/* Trim leading/trailing padding (rounded to even) of each file entry and sum the total size. */
void voice_trim_clips(void) {
    FileEntry *entry;
    s32 i;
    u32 pad;
    s32 size;

    g_voice_pcm_size = 0;
    for (i = 0; i < g_voice_clip_count; i++) {
        entry = &g_voice_clips[i];
        size = entry->size;
        pad = (u32)(size * 10) / 45;
        if (pad & 1) {
            pad = (pad / 2 + 1) * 2;
        }
        if (entry->offset == 1) {
            entry->size = size - pad * 2;
            entry->offset = pad * 2;
        }
        if (entry->tail == 1) {
            entry->tail = pad;
            entry->size -= pad;
        }
        g_voice_pcm_size += entry->size;
    }
}

/* Look up a name in the CR/LF-separated name table; returns its index (0x27E if absent). */
s32 voice_find_clip(char *name) {
    char line[16];
    u8 *p;
    s32 index;
    s32 len;
    u8 c;

    p = g_voice_name_table;
    index = 0;
    do {
        len = 0;
        while ((c = *p) != '\r') {
            p++;
            line[len++] = c;
        }
        line[len] = 0;
        if (strncmp(name, line, len) == 0) {
            break;
        }
        index++;
        p += 2;
    } while (index < 0x27E);
    return index;
}

void voice_load_name_table(void) {
    void *file; /* port: s32 on the PS1 */
    s32 handle;

    file = heap_alloc(g_bin_file_table[0x29]);
    handle = cd_load_archive_entry(3, 0x14, g_bin_file_table, file);
    while (cd_poll_load(handle) == 0) {
        loading_wait_tick();
    }
    g_voice_name_table = lz_decompress(file, D_80098F8C[0]);
    heap_free(file);
}


/* Look up a name in the name table and synchronously load the matching entry into dst. */
void voice_load_clip_to(char *name, void *dst) { /* port: dst is s32 on the PS1 */
    s32 handle;

    handle = cd_load_archive_entry(7, FindName(name), g_voice_file_table, dst);
    while (cd_poll_load(handle) == 0) {
        loading_wait_tick();
    }
}

/* Concatenate every loaded file entry into dst, freeing each source buffer. */
void voice_concat_clips(u8 *dst) {
    FileEntry *entry;
    s32 i;

    for (i = 0; i < g_voice_clip_count; i++) {
        entry = &g_voice_clips[i];
        memcpy(dst, entry->data + entry->offset, entry->size);
        dst += entry->size;
        heap_free(entry->data);
    }
}

/* Idle/yield callback while waiting on async loads (skipped in mode 0x2DD). */
void loading_wait_tick(void) {
    if (g_media_id != 0x2DD) {
        loading_anim_draw(0);
    }
}
