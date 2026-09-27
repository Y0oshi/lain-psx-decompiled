#include "common.h"

/* Minimal PsyQ libgte/libgs types used by this file. */
typedef struct {
    s16 m[3][3];
    s32 t[3];
} MATRIX;

typedef struct {
    s16 vx, vy, vz, pad;
} SVECTOR;

typedef struct {
    s16 vx, vy;
} DVECTOR;

typedef struct {
    s16 x, y;
    s16 w, h;
} RECT;

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
    u32 mode;
    RECT *crect;
    u32 *caddr;
    RECT *prect;
    u32 *paddr;
} TIM_IMAGE;

typedef struct GsCOORDINATE2 {
    u32 flg;
    MATRIX coord;
    MATRIX workm;
    void *param;
    struct GsCOORDINATE2 *super;
    struct GsCOORDINATE2 *sub;
} GsCOORDINATE2;

typedef struct {
    u32 length;
    void *org;
    u32 offset;
    u32 point;
    void *tag;
} GsOT;

typedef struct {
    u32 attribute;
    DVECTOR p0, p1;
    u8 r0, g0, b0;
    u8 r1, g1, b1;
} GsGLINE;

typedef struct {
    u32 attribute;
    GsCOORDINATE2 *coord2;
    u32 *tmd;
    u32 id;
} GsDOBJ2;

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
extern Model D_801E5B54[];
void model_update_matrix(Model *model);
void model_sort(Model *model, GsOT ot);

typedef struct {
    s32 vx, vy, vz;
    u8 r, g, b;
} GsF_LIGHT;

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
void tim_upload(u32 *tim);
void model_map_tmd(u32 *file, Model *model);
void model_link(Model *model, GsCOORDINATE2 *super);
s32 GsSetFlatLight(s32 id, GsF_LIGHT *light);

typedef struct {
    s32 vx, vy, vz, pad;
} VECTOR;

typedef struct {
    double x, y, z;
} DVECTOR3;

extern double g_gate_fly_time; /* spline time (frames), high word is D_800A6ABC */
extern s16 g_gate_level __attribute__((section(".data")));
extern s16 g_gate_fly_pending __attribute__((section(".data")));
extern s32 g_gate_level_angles[];
extern DVECTOR3 g_bezier_points[];
extern u8 g_bezier_coeffs[];
void GsGetLws(GsCOORDINATE2 *coord, MATRIX *lw, MATRIX *ls);
void PushMatrix(void);
void PopMatrix(void);
MATRIX *RotMatrix(SVECTOR *r, MATRIX *m);
void SetRotMatrix(MATRIX *m);
VECTOR *ApplyRotMatrix(SVECTOR *v0, VECTOR *v1);
void bezier_eval(s32 count, void *pts, DVECTOR3 *out, double t);
void bezier_prepare(s32 count, DVECTOR3 *in, void *out);
void gate_scroll_texture(void);

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

typedef struct {
    u32 tag;
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

/* PsyQ libgpu primitive-length setter (byte 3 of the tag word). */
#define setlen(p, n) (((u8 *)(p))[3] = (n))

/* 0x4C-byte glyph object of the name-entry screen, drawn by sprite_draw_rotated
 * (only the sprite-like part at 0x28 is set up here). */
typedef struct {
    u8 poly[0x28];
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
void GsSortFastSprite(GsSPRITE *sp, GsOT *ot, u16 pri);
void GsSortPoly(void *prim, GsOT *ot, u16 pri);

/* A point of a piecewise-linear curve. */
typedef struct {
    float x;
    float y;
} CurvePoint;

extern CurvePoint g_voice_envelope[];
s32 curve_eval_linear(u32 x, CurvePoint *curve, u32 count);

extern GsCOORDINATE2 D_801E4944; /* = g_models[1].coord */
extern s16 g_streak_spawn_y[];
extern s16 g_streak_spawn_x[];
extern s16 g_streak_spawn_z[];
extern Color3 g_streak_colors[];
extern GsOT g_ot[];
extern s32 g_frame_buffer_index __attribute__((section(".data")));
/* Camera/scene offsets; only the first word of each is used. */
extern s32 D_801EAEE4[]; /* = g_view.vpy */
extern s32 D_801EAEE8[]; /* = g_view.vpz */

s32 rand(void);
void rotcoord_pair_update_matrix(RotCoordPair *pair);
void rotcoord_pair_attach(RotCoordPair *pair, GsCOORDINATE2 *super);
void rotcoord_pair_project(RotCoordPair *pair, DVECTOR *screen0, DVECTOR *screen1);
s32 screen_point_offscreen(DVECTOR *pos, s16 margin);
void GsSortGLine(GsGLINE *line, GsOT *ot, u16 pri);

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

extern s32 g_media_id __attribute__((section(".data")));
extern s32 g_bin_file_table[];
extern s32 D_80098F8C[]; /* = inside g_bin_file_table: the word heap_alloc(g_bin_file_table[0x29]) reads */
extern u8 *g_voice_name_table;
extern u8 g_voice_prev_vowel; /* previous vowel, '\\' = none */
extern u8 *g_voice_pcm;
extern s32 g_voice_pcm_size;
extern s16 g_voice_clip_count;
extern FileEntry g_voice_clips[];
extern FileInfo g_voice_file_table[];

void loading_anim_draw(s32);
s32 strncmp(const char *, const char *, s32);
void *memcpy(void *, const void *, u32);
void heap_free(void *);
s32 heap_alloc(s32);
s32 cd_load_archive_entry(s32, s32, void *, s32);
s32 cd_poll_load(s32);
u8 *lz_decompress(s32, s32);
void loading_wait_tick(void);
void GsGetTimInfo(void *tim, GsIMAGE *image);
void voice_apply_envelope(s16 *samples);
void voice_trim_clips(void);
void voice_concat_clips(u8 *dst);

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

/* Spawn a streak at a random position on the top plane (falls along -Y). */
void streak_spawn_falling(Streak *s) {
    s16 idx;
    s16 speed;
    s16 length;

    rotcoord_pair_attach(&s->pos, &D_801E4944);
    s->origin.vx = g_streak_spawn_x[rand() & 0xF];
    s->origin.vy = 500;
    s->origin.vz = g_streak_spawn_z[rand() & 0xF];
    s->pos.coord0.coord.t[0] = s->origin.vx;
    s->pos.coord0.coord.t[1] = s->origin.vy;
    s->pos.coord0.coord.t[2] = s->origin.vz;
    s->limit.vx = s->origin.vx;
    s->limit.vy = -20000;
    s->limit.vz = s->origin.vz;
    s->pos.coord1.coord.t[0] = s->pos.coord0.coord.t[0];
    s->pos.coord1.coord.t[1] = s->pos.coord0.coord.t[1];
    s->pos.coord1.coord.t[2] = s->pos.coord0.coord.t[2];
    rotcoord_pair_update_matrix(&s->pos);
    s->line.attribute = 0x50000000;
    idx = (rand() & 0x300) >> 8;
    s->r = g_streak_colors[idx].r;
    s->g = g_streak_colors[idx].g;
    s->b = g_streak_colors[idx].b;
    s->line.r0 = g_streak_colors[idx].r;
    s->line.g0 = g_streak_colors[idx].g;
    s->line.b0 = g_streak_colors[idx].b;
    s->line.r1 = s->line.g1 = s->line.b1 = 0;
    speed = rand() & 0x7FF;
    if (speed < 500) {
        speed = 500;
    }
    s->speed = speed;
    length = rand() & 0xFFF;
    if (length < 2000) {
        length = 2000;
    }
    s->length = length;
}

/* Spawn a streak from a spawn descriptor (moves horizontally along -X/-Z). */
void streak_spawn(Streak *s, StreakSpawn *spawn) {
    s16 idx;
    s16 speed;
    s16 length;

    rotcoord_pair_attach(&s->pos, &D_801E4944);
    s->origin.vx = spawn->origin.vx;
    s->origin.vy = g_streak_spawn_y[rand() & 0xF];
    s->origin.vz = spawn->origin.vz - D_801EAEE8[0];
    s->limit.vx = spawn->limit.vx;
    s->limit.vy = spawn->limit.vy;
    s->limit.vz = spawn->limit.vz;
    s->pos.coord0.coord.t[0] = s->origin.vx;
    s->pos.coord0.coord.t[1] = s->origin.vy;
    s->pos.coord0.coord.t[2] = s->origin.vz;
    s->pos.coord1.coord.t[0] = s->pos.coord0.coord.t[0];
    s->pos.coord1.coord.t[1] = s->pos.coord0.coord.t[1];
    s->pos.coord1.coord.t[2] = s->pos.coord0.coord.t[2];
    rotcoord_pair_update_matrix(&s->pos);
    s->line.attribute = 0x50000000;
    idx = (rand() & 0x300) >> 8;
    s->r = g_streak_colors[idx].r;
    s->g = g_streak_colors[idx].g;
    s->b = g_streak_colors[idx].b;
    s->line.r0 = g_streak_colors[idx].r;
    s->line.g0 = g_streak_colors[idx].g;
    s->line.b0 = g_streak_colors[idx].b;
    s->line.r1 = s->line.g1 = s->line.b1 = 0;
    speed = rand() & 0x3FF;
    if (speed < 200) {
        speed = 200;
    }
    s->speed = speed;
    length = rand() & 0xFFF;
    if (length < 2000) {
        length = 2000;
    }
    s->length = length;
}

/* Per-frame update for a streak moving toward -X/-Z; respawns at its origin once past its limit. */
void streak_update_left(Streak *s) {
    s16 speed;
    s16 length;

    if (s->limit.vx < s->pos.coord0.coord.t[0] && s->limit.vz < s->pos.coord0.coord.t[2]) {
        s->pos.coord0.coord.t[0] -= s->speed;
        s->pos.coord0.coord.t[2] -= s->speed;
    } else {
        s->pos.coord0.coord.t[0] = s->origin.vx;
        s->pos.coord0.coord.t[1] = g_streak_spawn_y[rand() & 0xF];
        s->pos.coord0.coord.t[2] = s->origin.vz - D_801EAEE8[0];
        s->line.r0 = s->r;
        s->line.g0 = s->g;
        s->line.b0 = s->b;
        speed = rand() & 0x3FF;
        if (speed < 200) {
            speed = 200;
        }
        s->speed = speed;
        length = rand() & 0xFFF;
        if (length < 2000) {
            length = 2000;
        }
        s->length = length;
    }
    s->pos.coord1.coord.t[0] = s->pos.coord0.coord.t[0] + s->length;
    s->pos.coord1.coord.t[1] = s->pos.coord0.coord.t[1];
    s->pos.coord1.coord.t[2] = s->pos.coord0.coord.t[2] + s->length;
    rotcoord_pair_update_matrix(&s->pos);
    rotcoord_pair_project(&s->pos, &s->line.p0, &s->line.p1);
    if (screen_point_offscreen(&s->line.p0, 100) == 0) {
        GsSortGLine(&s->line, &g_ot[g_frame_buffer_index], 700);
    }
}

/* Per-frame update for a streak moving toward +X/-Z; respawns at its origin once past its limit. */
void streak_update_right(Streak *s) {
    s16 speed;
    s16 length;

    if (s->pos.coord0.coord.t[0] < s->limit.vx && s->limit.vz < s->pos.coord0.coord.t[2]) {
        s->pos.coord0.coord.t[0] += s->speed;
        s->pos.coord0.coord.t[2] -= s->speed;
    } else {
        s->pos.coord0.coord.t[0] = s->origin.vx;
        s->pos.coord0.coord.t[1] = g_streak_spawn_y[rand() & 0xF];
        s->pos.coord0.coord.t[2] = s->origin.vz - D_801EAEE8[0];
        s->line.r0 = s->r;
        s->line.g0 = s->g;
        s->line.b0 = s->b;
        speed = rand() & 0x3FF;
        if (speed < 200) {
            speed = 200;
        }
        s->speed = speed;
        length = rand() & 0xFFF;
        if (length < 2000) {
            length = 2000;
        }
        s->length = length;
    }
    s->pos.coord1.coord.t[0] = s->pos.coord0.coord.t[0] - s->length;
    s->pos.coord1.coord.t[1] = s->pos.coord0.coord.t[1];
    s->pos.coord1.coord.t[2] = s->pos.coord0.coord.t[2] + s->length;
    rotcoord_pair_update_matrix(&s->pos);
    rotcoord_pair_project(&s->pos, &s->line.p0, &s->line.p1);
    if (screen_point_offscreen(&s->line.p0, 100) == 0) {
        GsSortGLine(&s->line, &g_ot[g_frame_buffer_index], 700);
    }
}

/* Per-frame update for a falling streak; respawns at a random spot on the top plane. */
void streak_update_falling(Streak *s) {
    s16 speed;
    s16 length;

    if (s->limit.vy < s->pos.coord0.coord.t[1]) {
        s->pos.coord0.coord.t[1] -= s->speed;
    } else {
        s->pos.coord0.coord.t[0] = g_streak_spawn_x[rand() & 0xF];
        s->pos.coord0.coord.t[1] = 500;
        s->pos.coord0.coord.t[2] = g_streak_spawn_z[rand() & 0xF];
        s->line.r0 = s->r;
        s->line.g0 = s->g;
        s->line.b0 = s->b;
        s->limit.vy = D_801EAEE4[0];
        speed = rand() & 0x7FF;
        if (speed < 500) {
            speed = 500;
        }
        s->speed = speed;
        length = rand() & 0xFFF;
        if (length < 2000) {
            length = 2000;
        }
        s->length = length;
    }
    s->pos.coord1.coord.t[0] = s->pos.coord0.coord.t[0];
    s->pos.coord1.coord.t[1] = s->pos.coord0.coord.t[1] + s->length;
    s->pos.coord1.coord.t[2] = s->pos.coord0.coord.t[2];
    rotcoord_pair_update_matrix(&s->pos);
    if (s->pos.coord0.coord.t[1] > D_801EAEE4[0]) {
        rotcoord_pair_project(&s->pos, &s->line.p0, &s->line.p1);
        if (screen_point_offscreen(&s->line.p0, 500) == 0) {
            GsSortGLine(&s->line, &g_ot[g_frame_buffer_index], 700);
        }
    }
}

#ifdef NON_MATCHING
/* 3 diffs, in the model-table lookup: the sum `t += k` lands in v1 (the
 * original reuses the index's v0), which moves `lbu` after `lui v1,%hi(g_movie_model_tmds)`.
 * `g_movie_model_tmds[g_movie_model_kinds[i]]` passed directly makes GCC hoist the table address into $s8
 * (one more saved register, which shifts everything); going through `t` keeps it in the
 * loop like the original (lifetime 2 in -dL: "not desirable"). Loading the index into
 * `k` first (found by decomp-permuter) gets the index/base registers right. Tried for the
 * sum: `t[k]`, `*(t + k)` (hoist again, 33), a second pointer (33), `&t[k]`, s32/u32 k (3),
 * `*(t += k)` (3). movie_models_update has the same pattern. */
/* Set up the central model (40) and its eight satellites (41..48), textures and light. */
void movie_models_init(void) {
    u32 i;
    u32 **t;
    u8 k;

    model_reset_coord(&g_models[40]);
    model_map_tmd(&g_empty_tmd, &g_models[40]);
    model_link(&g_models[40], &g_models[1].coord);
    g_models[40].coord.coord.t[0] = g_movie_models_center_x;
    g_models[40].coord.coord.t[1] = g_movie_models_center_y;
    g_models[40].coord.coord.t[2] = g_movie_models_center_z - D_801EAEE8[0];
    model_update_matrix(&g_models[40]);
    for (i = 0; i < 8; i++) {
        model_reset_coord(&g_models[41 + i]);
        k = g_movie_model_kinds[i];
        t = g_movie_model_tmds;
        t += k;
        model_map_tmd(*t, &g_models[41 + i]);
        model_link(&g_models[41 + i], &g_models[40].coord);
        g_models[41 + i].coord.coord.t[0] = g_movie_model_start_pos[i].vx;
        g_models[41 + i].coord.coord.t[1] = g_movie_model_start_pos[i].vy;
        g_models[41 + i].coord.coord.t[2] = g_movie_model_start_pos[i].vz;
        g_models[41 + i].dobj.attribute = 0x80000000;
        model_update_matrix(&g_models[41 + i]);
        g_movie_model_steps[i].vx = (g_movie_model_start_pos[i].vx - g_movie_model_home_pos[i].vx) / 10;
        g_movie_model_steps[i].vy = (g_movie_model_start_pos[i].vy - g_movie_model_home_pos[i].vy) / 10;
        g_movie_model_steps[i].vz = (g_movie_model_start_pos[i].vz - g_movie_model_home_pos[i].vz) / 10;
    }
    g_movie_model1_spin = 0;
    g_movie_model0_spin = 0;
    g_movie_models_slide_frame = 0;
    g_movie_models_state = 0;
    tim_upload(&g_movie_model_tim0);
    tim_upload(&g_movie_model_tim1);
    g_flat_lights[2].vz = -700;
    g_flat_lights[2].vx = 0;
    g_flat_lights[2].vy = 0;
    g_flat_lights[2].g = 0x30;
    g_flat_lights[2].r = 0x30;
    GsSetFlatLight(2, &g_flat_lights[2]);
}
#else
INCLUDE_ASM("asm/nonmatchings/game/80031378", movie_models_init);
#endif

#ifdef NON_MATCHING
/* 16 diffs: `sel` and the hoisted &g_models[0] base swap $s6/$s7, and one
 * `lui` lands in a delay slot. Taking the table entries' addresses into `t` first (as in
 * movie_models_init) gets the index/base order right in both branches. Tried: declaration order,
 * u32/u8/s16 sel (16-20), `sel == g_movie_model_kinds[i]`, `k = g_movie_model_kinds[i]` shared by both
 * branches (16-25). decomp-permuter: wrapping the case-1 loop and its `break` in
 * `do { ... } while (0)` (a second loop level, which reweights the allocation) fixes
 * the s6/s7 swap (10 diffs); left out as implausible. What remains then is reorg filling
 * the `bne v0,s7` delay slot with the else branch's `lui v1` (the original leaves a nop).
 * Direct indexing: the original loads g_movie_model_kinds[i] once, into v0, then sll's it before building the
 * table base in each branch; with the table indexed directly, the base (a block-local
 * pseudo) is set before the sll, so local-alloc gives it v0 first (-dg). */
extern u32 *g_movie_model_tmds_hi[];
extern s16 g_text_window_busy __attribute__((section(".data")));

/* Per-frame update of the eight satellite models: slide them in, then swap models on input. */
void movie_models_update(s16 input) {
    u32 i;
    s32 sel;
    u32 **t;

    switch (g_movie_models_state) {
    case 0:
        if (g_movie_models_slide_frame < 10) {
            for (i = 0; i < 8; i++) {
                g_models[41 + i].coord.coord.t[0] -= g_movie_model_steps[i].vx;
                g_models[41 + i].coord.coord.t[1] -= g_movie_model_steps[i].vy;
                g_models[41 + i].coord.coord.t[2] -= g_movie_model_steps[i].vz;
                g_models[41 + i].dobj.attribute &= 0x7FFFFFFF;
                model_update_matrix(&g_models[41 + i]);
                model_sort(&g_models[41 + i], g_ot[g_frame_buffer_index]);
            }
            g_movie_models_slide_frame++;
        } else {
            for (i = 0; i < 8; i++) {
                model_sort(&D_801E5B54[i], g_ot[g_frame_buffer_index]);
            }
            g_movie_models_state = 1;
        }
        break;
    case 1:
        if (input < 2 && g_text_window_busy == 0) {
            if (input == 0) {
                sel = 1;
            } else if (input == g_movie_models_state) {
                sel = 0;
            }
        } else {
            g_movie_model1_spin = 0;
            g_movie_model0_spin = 0;
        }
        for (i = 0; i < 8; i++) {
            model_reset_coord(&g_models[41 + i]);
            if (g_movie_model_kinds[i] == sel) {
                t = &g_movie_model_tmds_hi[g_movie_model_kinds[i]];
                model_map_tmd(*t, &g_models[41 + i]);
                if (i < 2) {
                    if (i == 1) {
                        g_movie_model1_spin += 45;
                        g_movie_model0_spin = 0;
                        g_models[41 + i].rot.vy = g_movie_model1_spin;
                    } else if (i == 0) {
                        g_movie_model0_spin += 45;
                        g_movie_model1_spin = 0;
                        g_models[41 + i].rot.vy = g_movie_model0_spin;
                    }
                }
            } else {
                t = &g_movie_model_tmds[g_movie_model_kinds[i]];
                model_map_tmd(*t, &g_models[41 + i]);
                g_models[41 + i].rot.vy = 0;
            }
            model_link(&g_models[41 + i], &g_models[40].coord);
            g_models[41 + i].coord.coord.t[0] = g_movie_model_home_pos[i].vx;
            g_models[41 + i].coord.coord.t[1] = g_movie_model_home_pos[i].vy;
            g_models[41 + i].coord.coord.t[2] = g_movie_model_home_pos[i].vz;
            model_update_matrix(&g_models[41 + i]);
            g_models[41 + i].dobj.attribute &= 0x7FFFFFFF;
            model_sort(&g_models[41 + i], g_ot[g_frame_buffer_index]);
        }
        break;
    }
}
#else
INCLUDE_ASM("asm/nonmatchings/game/80031378", movie_models_update);
#endif

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
void tim_load_file(s32 id);
s32 ClearImage2(RECT *rect, u8 r, u8 g, u8 b);
void tmd_set_vertex(u32 index, SVECTOR *entry, ModelInfo *info);
void light_apply_preset(void);

/* Scene setup: clear the frame buffer, load the central model (40) and the ring models (41..46). */
void ring_models_init(void) {
    RECT rect;
    ModelInfo info;
    u32 j;
    s32 i;

    tim_load_file(0x1E);
    rect.w = 320;
    rect.x = 0;
    rect.y = 0;
    rect.h = 480;
    ClearImage2(&rect, 0, 0, 0);
    tim_load_file(0x1A);
    tim_load_file(0x1B);
    tim_load_file(0x1C);
    tim_load_file(0x1D);

    model_reset_coord(&g_models[40]);
    model_map_tmd(&g_empty_tmd, &g_models[40]);
    model_link(&g_models[40], NULL);
    g_models[40].coord.coord.t[0] = 0;
    g_models[40].coord.coord.t[1] = 0;
    g_models[40].coord.coord.t[2] = 0;
    model_update_matrix(&g_models[40]);

    model_reset_coord(&g_models[41]);
    model_map_tmd(&g_ring_models_center_tmd, &g_models[41]);
    model_link(&g_models[41], &g_models[40].coord);
    info = *(ModelInfo *)g_models[41].tmd;
    for (i = 0; i < info.n_vert; i++) {
        tmd_set_vertex(i, &g_ring_models_center_verts[i], &info);
    }
    g_models[41].coord.coord.t[0] = 0;
    g_models[41].coord.coord.t[1] = 0;
    g_models[41].coord.coord.t[2] = 0;
    g_models[41].dobj.attribute = 0;
    model_update_matrix(&g_models[41]);

    for (i = 1; i < 6; i++) {
        model_reset_coord(&g_models[41 + i]);
        model_map_tmd(&g_ring_models_outer_tmd, &g_models[41 + i]);
        model_link(&g_models[41 + i], &g_models[40].coord);
        info = *(ModelInfo *)g_models[41 + i].tmd;
        for (j = 0; j < info.n_vert; j++) {
            tmd_set_vertex(j, &g_ring_models_outer_verts[j], &info);
        }
        g_models[41 + i].coord.coord.t[0] = g_ring_models_outer_pos[i - 1].vx;
        g_models[41 + i].coord.coord.t[1] = g_ring_models_outer_pos[i - 1].vy;
        g_models[41 + i].coord.coord.t[2] = g_ring_models_outer_pos[i - 1].vz;
        g_models[41 + i].dobj.attribute = 0;
        model_update_matrix(&g_models[41 + i]);
    }
    g_scene_wait_frames = 0;
    g_ring_models_state = 0;
    g_ring_models_done = 0;
    light_apply_preset();
    g_flat_lights[2].vz = -700;
    g_flat_lights[2].vx = 0;
    g_flat_lights[2].vy = 0;
    g_flat_lights[2].g = 0x30;
    g_flat_lights[2].r = 0x30;
    GsSetFlatLight(2, &g_flat_lights[2]);
    DrawSync(0);
}

/* Spin the ring of models 41..46 one step (every `delay` calls) and draw them. */

void ring_models_spin(s32 delay) {
    s32 i;

    if (g_scene_wait_frames >= delay) {
        g_models[41].rot.vy += 0x16;
        model_update_matrix(&g_models[41]);
        for (i = 1; i < 6; i++) {
            g_models[i + 41].rot.vy -= 0x16;
            model_update_matrix(&g_models[i + 41]);
        }
        g_scene_wait_frames = 0;
    } else {
        g_scene_wait_frames++;
    }
    model_sort(&D_801E5B54[0], g_ot[g_frame_buffer_index]);
    for (i = 1; i < 6; i++) {
        model_sort(&D_801E5B54[i], g_ot[g_frame_buffer_index]);
    }
}

extern s32 g_interp_steps_left __attribute__((section(".data")));
extern s32 g_morph_mode __attribute__((section(".data")));
extern s32 g_saved_interp_steps_left __attribute__((section(".data")));
extern s32 g_saved_morph_mode __attribute__((section(".data")));
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

/* Morph animation state machine for models 41/42: collapse the vertices to a
 * point, morph back, then hide/show the ring models. */
void ring_models_collapse(void) {
    ModelInfo info;
    s32 i;

    switch (g_ring_models_state) {
    case 0:
        g_saved_interp_delta[0] = g_interp_deltas[0];
        g_saved_interp_steps_left = g_interp_steps_left;
        g_saved_morph_mode = g_morph_mode;
        for (i = 0; i < 84; i++) {
            g_ring_models_collapse_verts[i].vx = g_ring_models_collapse_x;
            g_ring_models_collapse_verts[i].vy = g_ring_models_collapse_y;
            g_ring_models_collapse_verts[i].vz = g_ring_models_collapse_z;
        }
        info = *(ModelInfo *)g_models[42].tmd;
        for (i = 0; i < info.n_vert; i++) {
            tmd_get_vertex(i, &g_ring_models_morph_verts[i], &info);
        }
        g_morph_mode = 6;
        vec_interp_start((SVECTOR *)g_ring_models_collapse_verts, g_ring_models_morph_verts, 30, 84);
        g_ring_models_state = 1;
        break;
    case 1:
        if (g_interp_steps_left > 0 && g_morph_mode == 6) {
            vec_interp_step(g_ring_models_morph_verts);
            info = *(ModelInfo *)g_models[42].tmd;
            for (i = 0; i < info.n_vert; i++) {
                tmd_set_vertex(i, &g_ring_models_morph_verts[i], &info);
            }
        } else {
            for (i = 1; i < 6; i++) {
                g_models[41 + i].dobj.attribute |= 0x80000000;
            }
            g_interp_deltas[0] = g_saved_interp_delta[0];
            g_ring_models_state = 2;
            g_interp_steps_left = g_saved_interp_steps_left;
            g_morph_mode = g_saved_morph_mode;
        }
        break;
    case 2:
        g_saved_interp_delta[0] = g_interp_deltas[0];
        g_saved_interp_steps_left = g_interp_steps_left;
        g_saved_morph_mode = g_morph_mode;
        for (i = 0; i < 80; i++) {
            g_ring_models_collapse_verts[i].vx = g_ring_models_collapse_x;
            g_ring_models_collapse_verts[i].vy = g_ring_models_collapse_y;
            g_ring_models_collapse_verts[i].vz = g_ring_models_collapse_z;
        }
        info = *(ModelInfo *)g_models[41].tmd;
        for (i = 0; i < info.n_vert; i++) {
            tmd_get_vertex(i, &g_ring_models_morph_verts[i], &info);
        }
        g_morph_mode = 6;
        vec_interp_start((SVECTOR *)g_ring_models_collapse_verts, g_ring_models_morph_verts, 60, 80);
        g_ring_models_state = 3;
        break;
    case 3:
        if (g_interp_steps_left > 0 && g_morph_mode == 6) {
            vec_interp_step(g_ring_models_morph_verts);
            info = *(ModelInfo *)g_models[41].tmd;
            for (i = 0; i < info.n_vert; i++) {
                tmd_set_vertex(i, &g_ring_models_morph_verts[i], &info);
            }
        } else {
            g_interp_deltas[0] = g_saved_interp_delta[0];
            g_ring_models_state = 4;
            g_models[41].dobj.attribute |= 0x80000000;
            g_interp_steps_left = g_saved_interp_steps_left;
            g_morph_mode = g_saved_morph_mode;
        }
        break;
    case 4:
        g_ring_models_done = 0xFF;
        g_ring_models_state = 0xFF;
        break;
    }
    if (g_ring_models_done == 0) {
        ring_models_spin(0);
    }
}

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
extern s16 g_gate_obj_screen_x __attribute__((section(".data")));
extern s16 g_gate_obj_screen_y __attribute__((section(".data")));
extern GsSPRITE g_gate_column_sprites[];
void model_project_origin(Model *model, DVECTOR *screen);
u16 GetTPage(s32 tp, s32 abr, s32 x, s32 y);

/* Scene setup: central model, four orbiting models, three extra models, and a column of 30 sprites. */
void gate_models_init(void) {
    DVECTOR screen;
    s32 unused[2];
    GsIMAGE image;
    u32 i;

    model_reset_coord(&g_models[40]);
    model_map_tmd(&g_empty_tmd, &g_models[40]);
    model_link(&g_models[40], NULL);
    g_models[40].coord.coord.t[0] = g_gate_center_x;
    g_models[40].coord.coord.t[1] = g_gate_center_y;
    g_models[40].coord.coord.t[2] = g_gate_center_z;
    model_update_matrix(&g_models[40]);
    for (i = 0; i < 4; i++) {
        model_reset_coord(&g_models[41 + i]);
        model_map_tmd(g_gate_model_tmds[i], &g_models[41 + i]);
        model_link(&g_models[41 + i], &g_models[40].coord);
        g_models[41 + i].coord.coord.t[0] = g_gate_orbit_pos[i].vx;
        g_models[41 + i].coord.coord.t[1] = g_gate_orbit_pos[i].vy;
        g_models[41 + i].coord.coord.t[2] = g_gate_orbit_pos[i].vz;
        g_models[41 + i].rot.vy = i << 10;
        g_models[41 + i].dobj.attribute = 0x80000000;
        model_update_matrix(&g_models[41 + i]);
    }

    model_reset_coord(&g_models[46]);
    model_map_tmd(g_gate_model_tmds[4], &g_models[46]);
    model_link(&g_models[46], NULL);
    g_models[46].coord.coord.t[0] = (rand() & 0x1FF) - 160;
    g_models[46].coord.coord.t[1] = (rand() & 0xFF) - 120;
    g_models[46].dobj.attribute = 0x80000000;
    g_models[46].coord.coord.t[2] = g_gate_obj_start_z;
    model_update_matrix(&g_models[46]);
    model_project_origin(&g_models[46], &screen);
    g_gate_obj_screen_x = screen.vx;
    g_gate_obj_screen_y = screen.vy;

    model_reset_coord(&g_models[47]);
    model_map_tmd(&g_gate_fixed_tmd0, &g_models[47]);
    model_link(&g_models[47], NULL);
    g_models[47].coord.coord.t[0] = 0;
    g_models[47].coord.coord.t[1] = 0;
    g_models[47].coord.coord.t[2] = 0;
    g_models[47].dobj.attribute = 0;
    model_update_matrix(&g_models[47]);

    model_reset_coord(&g_models[48]);
    model_map_tmd(&g_gate_fixed_tmd1, &g_models[48]);
    model_link(&g_models[48], NULL);
    g_models[48].coord.coord.t[0] = 0;
    g_models[48].coord.coord.t[1] = 0;
    g_models[48].coord.coord.t[2] = 0;
    g_models[48].dobj.attribute = 0;
    model_update_matrix(&g_models[48]);

    g_flat_lights[2].vz = -700;
    g_flat_lights[2].vx = 0;
    g_flat_lights[2].vy = 0;
    g_flat_lights[2].g = 0x30;
    g_flat_lights[2].r = 0x30;
    GsSetFlatLight(2, &g_flat_lights[2]);
    g_gate_fly_started = 0;
    g_gate_scroll_done = 0;
    tim_upload(&g_gate_scroll_tim);
    tim_upload(&g_gate_column_tim);
    GsGetTimInfo(&D_80094D1C, &image);
    for (i = 0; i < 30; i++) {
        g_gate_column_sprites[i].attribute = 0x60000000;
        g_gate_column_sprites[i].x = 160 - image.pw * 2;
        g_gate_column_sprites[i].y = i * image.ph + image.ph / 2;
        g_gate_column_sprites[i].r = g_gate_column_sprites[i].g = g_gate_column_sprites[i].b = 0x80;
        g_gate_column_sprites[i].scalex = g_gate_column_sprites[i].scaley = 1;
        g_gate_column_sprites[i].rotate = 0;
        g_gate_column_sprites[i].tpage = GetTPage(image.pmode, 2, image.px, image.py);
        g_gate_column_sprites[i].u = 0;
        g_gate_column_sprites[i].v = image.py;
        g_gate_column_sprites[i].w = image.pw * 4;
        g_gate_column_sprites[i].h = image.ph;
        g_gate_column_sprites[i].cx = image.cx;
        g_gate_column_sprites[i].cy = image.cy;
    }
}

/* Per-frame update: draw the sprite column and orbiting models, and fly model 46 along a spline. */
void gate_models_update(void) {
    MATRIX lw;
    MATRIX ls;
    MATRIX rot;
    DVECTOR3 pos;
    SVECTOR v;
    SVECTOR angle;
    SVECTOR unused1;
    VECTOR r;
    VECTOR unused2;
    u32 i;
    s32 n;

    for (i = 0; i < 30; i++) {
        GsSortFastSprite(&g_gate_column_sprites[i], &g_ot[g_frame_buffer_index], 700);
    }
    if (g_gate_level != 0) {
        g_models[40].rot.vy = (g_models[40].rot.vy + 22) & 0xFFF;
        model_update_matrix(&g_models[40]);
    }
    n = 4;
    if (g_gate_level < 5) {
        n = g_gate_level;
    }
    for (i = 0; i < n; i++) {
        g_models[41 + i].dobj.attribute &= 0x7FFFFFFF;
        model_sort(&g_models[41 + i], g_ot[g_frame_buffer_index]);
    }
    if (g_gate_level < 4) {
        if (g_gate_fly_started == 0) {
            g_gate_fly_started = 0xFF;
            g_gate_fly_time = 0;
            g_models[46].dobj.attribute &= 0x7FFFFFFF;
            GsGetLws(&g_models[46].coord, &lw, &ls);
            PushMatrix();
            angle.vy = 0;
            angle.vx = 0;
            angle.vz = 0;
            RotMatrix(&angle, &rot);
            SetRotMatrix(&rot);
            v.vx = lw.t[0];
            v.vy = lw.t[1];
            v.vz = lw.t[2];
            ApplyRotMatrix(&v, &r);
            g_bezier_points[0].x = r.vx;
            g_bezier_points[0].y = r.vy;
            g_bezier_points[0].z = r.vz;
            v.vx = 0;
            v.vy = 0;
            v.vz = 0;
            ApplyRotMatrix(&v, &r);
            g_bezier_points[1].x = r.vx;
            g_bezier_points[1].y = r.vy;
            g_bezier_points[1].z = r.vz;
            v.vx = g_gate_center_x;
            v.vy = g_gate_center_y;
            v.vz = g_gate_center_z;
            ApplyRotMatrix(&v, &r);
            g_bezier_points[2].x = r.vx;
            g_bezier_points[2].y = r.vy;
            g_bezier_points[2].z = r.vz;
            PopMatrix();
            bezier_prepare(3, g_bezier_points, g_bezier_coeffs);
            model_sort(&g_models[46], g_ot[g_frame_buffer_index]);
        } else if (g_gate_fly_pending != 0) {
            if (g_gate_fly_time < 60.0) {
                bezier_eval(3, g_bezier_coeffs, &pos, g_gate_fly_time / 60.0);
                g_models[46].coord.coord.t[0] = pos.x;
                g_models[46].coord.coord.t[1] = pos.y;
                g_models[46].coord.coord.t[2] = pos.z;
                model_update_matrix(&g_models[46]);
                g_gate_fly_time += 1.0;
            }
            model_sort(&g_models[46], g_ot[g_frame_buffer_index]);
            if (g_gate_fly_time >= 60.0 && (g_models[40].rot.vy >> 8) == g_gate_level_angles[g_gate_level]) {
                g_models[46].dobj.attribute |= 0x80000000;
                g_models[41 + g_gate_level].dobj.attribute &= 0x7FFFFFFF;
                g_gate_fly_started = 0;
                g_gate_fly_pending = 0;
                model_sort(&g_models[41 + g_gate_level], g_ot[g_frame_buffer_index]);
            }
        }
    }
    if (g_models[40].rot.vy & 0x20) {
        if (g_gate_scroll_done == 0) {
            gate_scroll_texture();
        }
    } else {
        g_gate_scroll_done = 0;
    }
    model_sort(&g_models[47], g_ot[g_frame_buffer_index]);
    model_sort(&g_models[48], g_ot[g_frame_buffer_index]);
}


extern u8 D_800920DC[]; /* = g_gate_scroll_tim + 4 */

void GsGetTimInfo(void *tim, GsIMAGE *image);
s32 MoveImage(RECT *rect, s32 x, s32 y);
s32 DrawSync(s32 mode);

/* Scroll a VRAM texture horizontally by one pixel (via a scratch copy to its right). */
void gate_scroll_texture(void) {
    GsIMAGE image;
    RECT rect;

    GsGetTimInfo(D_800920DC, &image);

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

    g_gate_scroll_done++;
}

extern u32 g_menu_scroll_step;
extern u32 *g_menu_scroll_tim;
s32 ClearImage(RECT *rect, u8 r, u8 g, u8 b);
s32 LoadImage(RECT *rect, u32 *p);
s32 OpenTIM(u32 *addr);
TIM_IMAGE *ReadTIM(TIM_IMAGE *timimg);

/* Set up two semi-transparent quads and progressively upload a TIM into VRAM (one step per call). */
void start_menu_update_panels(POLY_FT4 *a, POLY_FT4 *b, u8 shade) {
    RECT rect;
    TIM_IMAGE tim;
    s32 h;

    setlen(a, 9);
    a->code = 0x2C;
    setlen(b, 9);
    b->code = 0x2C;
    a->r0 = shade;
    a->g0 = shade;
    a->b0 = shade;
    b->r0 = shade;
    b->g0 = shade;
    b->b0 = shade;
    a->x0 = 58;
    a->y0 = -23;
    a->x1 = 181;
    a->y1 = -7;
    a->x2 = 21;
    a->y2 = 254;
    a->x3 = 144;
    a->y3 = 270;
    a->u0 = 0;
    a->v0 = 0;
    a->u1 = 128;
    a->v1 = 0;
    a->u2 = 0;
    a->v2 = 255;
    a->u3 = 128;
    a->v3 = 255;
    b->x0 = -23;
    b->y0 = -1;
    b->x1 = 37;
    b->y1 = -22;
    b->x2 = 243;
    b->y2 = 353;
    b->x3 = 303;
    b->y3 = 332;
    b->u0 = 0;
    b->v0 = 0;
    b->u1 = 128;
    b->v1 = 0;
    b->u2 = 0;
    b->v2 = 255;
    b->u3 = 128;
    b->v3 = 255;
    a->tpage = 0x17;
    b->tpage = 0x17;
    a->clut = 0x3D14;
    b->clut = 0x3D14;
    a->code |= 2;
    b->code |= 2;

    if (g_menu_scroll_step <= 0x80) {
        rect.x = 0x1C0;
        rect.y = 0x100;
        rect.h = 0x100;
        rect.w = 0x40;
        ClearImage(&rect, 0, 0, 0);
        h = g_menu_scroll_step * 2;
        if (h != 0) {
            OpenTIM(g_menu_scroll_tim);
            ReadTIM(&tim);
            rect.w = 0x20;
            rect.h = h;
            rect.y = 0x200 - g_menu_scroll_step * 2;
            LoadImage(&rect, tim.paddr);
        }
    } else {
        OpenTIM(g_menu_scroll_tim);
        ReadTIM(&tim);
        rect.x = 0x1C0;
        rect.y = 0x100;
        rect.w = 0x20;
        h = (g_menu_scroll_step & 0x7F) * 2;
        rect.h = 0x100 - h;
        LoadImage(&rect, tim.paddr + (g_menu_scroll_step & 0x7F) * 32);
        if (h != 0) {
            rect.y = 0x200 - h;
            rect.h = h;
            LoadImage(&rect, tim.paddr);
        }
    }
    g_menu_scroll_step++;
}

