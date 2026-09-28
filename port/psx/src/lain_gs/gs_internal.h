/* Shared state of the port's libgs (see include/psx/libgs.h). */
#ifndef LAIN_GS_INTERNAL_H
#define LAIN_GS_INTERNAL_H

#include "psx/libgs.h"

#include <stddef.h>
#include <stdint.h>

/* Display / buffer state. */
extern int gs_disp_w, gs_disp_h;        /* display resolution */
extern short gs_buf_x[2], gs_buf_y[2];  /* double-buffer origins in VRAM */
extern short gs_origin_x, gs_origin_y;  /* GsSetOrign */
extern short gs_gte_ofs_x, gs_gte_ofs_y;/* offset added to 2D primitives (GTE offset mode) */
extern int gs_ofs_mode;                 /* GsOFSGTE / GsOFSGPU */
extern RECT16 gs_clip;                  /* drawing clip inside a buffer */

/* 3D state. */
extern int gs_light_mode;               /* GsSetLightMode */
extern int gs_proj_h;                   /* projection distance */
extern int gs_near_clip, gs_far_clip;

/* Primitive identities for frame interpolation (gs_core.c). A key names the
 * source of what is drawn (a GsDOBJ2, a GsSPRITE, ...) plus how many times it
 * was already sorted this frame; gs_set_tag(key, index) tags the packets
 * allocated next (index: the polygon in a TMD object). gs_set_tag(0, 0) stops. */
uint64_t gs_source_key(const void *src);
void gs_set_tag(uint64_t key, u_int index);
void gs_set_tag_2d(uint64_t key); /* same for 2D sorts */

/* Packet allocation (ring buffers, see gs_core.c). Never fails. */
void *gs_alloc_packet(size_t size);

/* The OT entry for priority pri (pri - offset, clamped to the table). */
GsOT_TAG *gs_ot_entry(GsOT *otp, int pri);

/* Link prim at the head of an OT entry (drawn before what is already there). */
void gs_link(GsOT_TAG *entry, void *prim, int len_words);

/* Fixed-point matrix helpers (GTE rounding: sum of products >> 12). */
void gs_mat_mul(const MATRIX *a, const MATRIX *b, MATRIX *out); /* out.m = a.m * b.m */
void gs_mat_apply(const MATRIX *m, const int v[3], int out[3]);  /* out = m.m * v */
void gs_mat_identity(MATRIX *m);

#endif
