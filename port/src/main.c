/* Lain native client entry point: launcher/setup window, then the game. */
#include <assert.h> /* libgpu.h uses static_assert */
#include <SDL.h>
#ifndef _WIN32 /* POSIX crash handler (debug aid) */
#include <execinfo.h>
#include <signal.h>
#include <unistd.h>
#endif
#ifdef __APPLE__
#include <mach-o/dyld.h>
#endif
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "libgte.h"
#include "libgpu.h" /* before PsyX_public.h: it uses struct _RECT16 */
#include "PsyX/PsyX_public.h"
#include "PsyX/common/glad.h"
#include "libcd.h"
#include "libspu.h" /* LainSpu_SetOutputGain: master volume */
#include "libgs.h"
#include "tracks.h"
#include "subtitle_kit.h"
#include "subtitles.h"
#include "dub_player.h"
#include "libmcrd.h"
#include "libetc.h"
#include "lain_xa.h"

#include "discs.h"
#include "mods.h"
#include "modrt.h"
#include "hdtex.h"
#include "overlay.h"
#include "app_icon.h"
#include "psx_arena.h"
#include "psx_mem.h"
#include "settings.h"
#include "setup_ui.h"

/* `lain --import disc.cue [disc2.cue]`: import discs without the launcher window. */
static int import_cli(int n, char **paths) {
    for (int i = 0; i < n; i++) {
        ImportJob job;
        discs_import_start(&job, paths[i], settings_data_dir());
        int last = -1;
        while (job.state == IMPORT_RUNNING) {
            int pct = (int)(job.progress * 100);
            if (pct / 10 != last / 10) {
                printf("  %s: %d%%\n", paths[i], pct);
                fflush(stdout);
                last = pct;
            }
            SDL_Delay(100);
        }
        printf("%s\n", job.message);
        if (job.state == IMPORT_FAILED) {
            return 1;
        }
        printf("  sha1 %s\n", job.sha1);
    }
    return 0;
}

/* `lain --selftest`: load the arena from the imported disc and check the layout. */
extern uint8_t g_psx_log_path[];
static int selftest(void) {
    char disc1[1024], err[256];
    if (!discs_imported(settings_data_dir(), DISC_1, disc1, sizeof disc1)) {
        printf("selftest: import disc 1 first\n");
        return 1;
    }
    if (psx_mem_load(disc1, err, sizeof err) != 0) {
        printf("selftest: %s\n", err);
        return 1;
    }
    int ok = 1;
    /* The first rodata object is the debug log path "d:\\...". */
    if (memcmp(PSX_PTR(0x80010000), "d:\\", 3) != 0) {
        printf("selftest: EXE not at its load address\n");
        ok = 0;
    }
    if ((void *)g_psx_log_path != PSX_PTR(0x80010000)) {
        printf("selftest: arena symbol labels don't line up\n");
        ok = 0;
    }
    printf("selftest: %s\n", ok ? "arena loaded, symbols line up" : "FAILED");
    return ok ? 0 : 1;
}

/* Debug hooks (environment variables)
 *   LAIN_SCREENSHOT=path.bmp   save the window at the frames in LAIN_SCREENSHOT_FRAMES
 *   LAIN_SCREENSHOT_FRAMES=N,M,... (default 300); files are path-<frame>.bmp
 *   LAIN_EXIT_FRAME=N          quit after N presented frames
 *   LAIN_FRAME_LOG=1           print a line every 60 frames
 *   LAIN_KEYS=F:KEY[:N],...    hold SDL key KEY (e.g. Return, C, V, Up) for N frames
 *                              (default 6) from frame F: scripted input for test runs */
static int frame_count;

int lain_frame_count(void) {
    return frame_count;
}
static const char *shot_path;
static int shot_frames[64], shot_count;
static int exit_frame;
static int frame_log;

static struct {
    int frame, frames;
    SDL_Scancode key;
} keys[64];
static int key_count;

/* Scripted input: sets the key in SDL's own keyboard state array, which is
 * what PsyCross's pad emulation polls, and queues the key event (for the F1
 * menu). Debug use only. */
static void push_key_event(SDL_Scancode sc, int down) {
    SDL_Event e;
    SDL_zero(e);
    e.type = down ? SDL_KEYDOWN : SDL_KEYUP;
    e.key.state = down ? SDL_PRESSED : SDL_RELEASED;
    e.key.keysym.scancode = sc;
    e.key.keysym.sym = SDL_GetKeyFromScancode(sc);
    e.key.windowID = SDL_GetWindowID(SDL_GL_GetCurrentWindow()); /* ImGui drops other windows' keys */
    SDL_PushEvent(&e);
}

static void apply_keys(void) {
    Uint8 *state = (Uint8 *)SDL_GetKeyboardState(NULL);
    for (int i = 0; i < key_count; i++) {
        if (frame_count == keys[i].frame) {
            state[keys[i].key] = 1;
            push_key_event(keys[i].key, 1);
            printf("[lain] frame %d: press %s\n", frame_count, SDL_GetScancodeName(keys[i].key));
            fflush(stdout);
        } else if (frame_count == keys[i].frame + keys[i].frames) {
            state[keys[i].key] = 0;
            push_key_event(keys[i].key, 0);
        }
    }
}

static void parse_keys(const char *spec) {
    char buf[1024];
    snprintf(buf, sizeof buf, "%s", spec);
    for (char *tok = strtok(buf, ","); tok && key_count < 64; tok = strtok(NULL, ",")) {
        char name[64] = "";
        int frame = 0, frames = 6;
        if (sscanf(tok, "%d:%63[^:]:%d", &frame, name, &frames) < 2) {
            continue;
        }
        SDL_Scancode sc = SDL_GetScancodeFromName(name);
        if (sc == SDL_SCANCODE_UNKNOWN) {
            fprintf(stderr, "[lain] unknown key '%s'\n", name);
            continue;
        }
        keys[key_count].frame = frame;
        keys[key_count].frames = frames;
        keys[key_count].key = sc;
        key_count++;
    }
}

static void save_window(const char *path) {
    int w = 0, h = 0; /* the window's pixels: the scaled frame plus overlays */
    SDL_GL_GetDrawableSize(SDL_GL_GetCurrentWindow(), &w, &h);
    if (w <= 0 || h <= 0) {
        return;
    }
    unsigned char *px = malloc((size_t)w * h * 4);
    glReadPixels(0, 0, w, h, GL_RGBA, GL_UNSIGNED_BYTE, px);
    /* GL rows are bottom-up. */
    SDL_Surface *surf = SDL_CreateRGBSurfaceWithFormat(0, w, h, 32, SDL_PIXELFORMAT_RGBA32);
    for (int y = 0; y < h; y++) {
        memcpy((unsigned char *)surf->pixels + (size_t)y * surf->pitch,
               px + (size_t)(h - 1 - y) * w * 4, (size_t)w * 4);
    }
    SDL_SaveBMP(surf, path);
    SDL_FreeSurface(surf);
    free(px);
    printf("[lain] frame %d saved to %s\n", frame_count, path);
    fflush(stdout);
}

/* Debug: summary of the primitives linked in the game's two 3D ordering
 * tables (LAIN_OT_DUMP=1 at screenshot frames). */
extern GsOT g_ot[2];
extern void GR_SaveVRAM(const char *outputFileName, int x, int y, int width, int height,
                        int bReadFromFrameBuffer); /* PsyCross renderer (C linkage) */
extern DISPENV activeDispEnv;
extern DRAWENV activeDrawEnv;
extern int g_GPUDisabledState;
static void dump_ot(void) {
    printf("[ot] gpu disabled %d, rgb24 active %d gs %d\n", g_GPUDisabledState, activeDispEnv.isrgb24,
           GsDISPENV.isrgb24);
    printf("[ot] drawenv clip %d,%d %dx%d ofs %d,%d dfe %d isbg %d; dispenv %d,%d %dx%d; GsDRAWENV clip %d,%d %dx%d ofs %d,%d dfe %d\n",
           activeDrawEnv.clip.x, activeDrawEnv.clip.y, activeDrawEnv.clip.w, activeDrawEnv.clip.h,
           activeDrawEnv.ofs[0], activeDrawEnv.ofs[1], activeDrawEnv.dfe, activeDrawEnv.isbg,
           activeDispEnv.disp.x, activeDispEnv.disp.y, activeDispEnv.disp.w, activeDispEnv.disp.h,
           GsDRAWENV.clip.x, GsDRAWENV.clip.y, GsDRAWENV.clip.w, GsDRAWENV.clip.h, GsDRAWENV.ofs[0],
           GsDRAWENV.ofs[1], GsDRAWENV.dfe);
    for (int b = 0; b < 2; b++) {
        GsOT *ot = &g_ot[b];
        if (!ot->tag) {
            continue;
        }
        int counts[256] = {0}, total = 0, guard = 0;
        P_TAG *p = (P_TAG *)ot->tag;
        static char first[65536]; first[0] = 0;
        int nfirst = 0;
        while (!isendprim(p) && guard++ < 200000) {
            if (p->len > 0) {
                counts[p->code]++;
                total++;
                if (nfirst < 400) {
                    POLY_F4 *q = (POLY_F4 *)p;
                    char tmp[64];
                    if (p->code == 0xE1) {
                        snprintf(tmp, sizeof tmp, " E1[%08X]", *(u_int *)&p->pad0);
                        strncat(first, tmp, sizeof first - strlen(first) - 1);
                        nfirst++;
                        p = (P_TAG *)nextPrim(p);
                        continue;
                    }
                    if ((p->code & 0xFC) == 0x2C) {
                        POLY_FT4 *f = (POLY_FT4 *)p;
                        char big[160];
                        snprintf(big, sizeof big, "\n[ot]   FT4 %02X xy %d,%d %d,%d %d,%d %d,%d uv %d,%d %d,%d %d,%d %d,%d tpage %04X clut %04X rgb %d,%d,%d",
                                 p->code, f->x0, f->y0, f->x1, f->y1, f->x2, f->y2, f->x3, f->y3, f->u0, f->v0, f->u1,
                                 f->v1, f->u2, f->v2, f->u3, f->v3, f->tpage, f->clut, f->r0, f->g0, f->b0);
                        strncat(first, big, sizeof first - strlen(first) - 1);
                        nfirst++;
                        p = (P_TAG *)nextPrim(p);
                        continue;
                    }
                    if ((p->code & 0xF0) == 0x60) {
                        SPRT *sp = (SPRT *)p;
                        snprintf(tmp, sizeof tmp, "\n[ot]   %02X xy %d,%d wh %d,%d uv %d,%d clut %04X rgb %d,%d,%d", p->code,
                                 sp->x0, sp->y0, sp->w, sp->h, sp->u0, sp->v0, sp->clut, sp->r0, sp->g0, sp->b0);
                        strncat(first, tmp, sizeof first - strlen(first) - 1);
                        nfirst++;
                        p = (P_TAG *)nextPrim(p);
                        continue;
                    }
                    snprintf(tmp, sizeof tmp, " %02X(%d,%d rgb %d,%d,%d w%d h%d)", p->code, q->x0, q->y0, q->r0,
                             q->g0, q->b0, q->x1, q->y1);
                    strncat(first, tmp, sizeof first - strlen(first) - 1);
                    nfirst++;
                }
            }
            p = (P_TAG *)nextPrim(p);
        }
        printf("[ot] buffer %d: %d prims:", b, total);
        for (int c = 0; c < 256; c++) {
            if (counts[c]) {
                printf(" %02X x%d", c, counts[c]);
            }
        }
        printf("\n[ot]   first:%s\n", first);
    }
    fflush(stdout);
}

/* Disc swapping
 * The game keeps the disc it wants in g_current_site (0 = disc 1, 1 = disc 2) and,
 * when that isn't the inserted one, shows "DISC Change Request" and waits for
 * the lid to open and close (disc_change_request / cd_lid_watch). The client plays
 * the player's part: swap the image and open/close the lid. */
extern short g_current_site; /* s16 in the game */
static const char *disc_paths[2];
static int inserted_disc;

/* Puts disc `disc` (0/1) in the drive: swaps the image and opens/closes the lid. */
void lain_insert_disc(int disc) {
    if (disc < 0 || disc > 1 || !disc_paths[disc]) {
        return;
    }
    PsyX_CDFS_SwapImage(disc_paths[disc]);
    LainCD_OpenShell(700);
    inserted_disc = disc;
}

static void check_disc_swap(void) {
    static int mismatch_frames, delay = -1;
    int want = g_current_site;
    if (delay < 0) {
        /* LAIN_DISC_SWAP_DELAY=N: frames to leave the wrong disc in (tests the
         * game's change request screen); default: swap right away. */
        delay = getenv("LAIN_DISC_SWAP_DELAY") ? atoi(getenv("LAIN_DISC_SWAP_DELAY")) : 0;
    }
    if (want != 0 && want != 1) {
        return;
    }
    mismatch_frames = want != inserted_disc ? mismatch_frames + 1 : 0;
    if (mismatch_frames > delay && disc_paths[want]) {
        printf("[lain] frame %d: game wants disc %d, swapping images\n", frame_count, want + 1);
        fflush(stdout);
        lain_insert_disc(want);
        overlay_toast(want == 0 ? "Disc 1 inserted" : "Disc 2 inserted", 3.0f);
    }
}

/* The node whose media (voice file or movie) the game is playing, e.g. "Cou001", or
 * NULL. The media player (media_play) keeps the media id in g_media_id; each of the
 * 716 nodes has its own media id (InfoEntry.media at +0x1C of the 0x28-byte
 * g_node_table entries). g_player_done is 0 while it plays, and
 * g_player_progress_step stays 0 until the first media has started (so the boot
 * state, which also reads "not done, media 0", doesn't count). */
extern int g_media_id;
extern short g_player_done;
extern int g_player_progress_step;
extern unsigned char g_node_table[];
static const char *playing_node_name(void) {
    static int cached_media = -1;
    static char name[9];
    if (g_player_done != 0 || g_player_progress_step == 0) {
        return NULL;
    }
    if (g_media_id != cached_media) {
        cached_media = g_media_id;
        name[0] = 0;
        for (int i = 0; i < 0x2CC; i++) {
            const unsigned char *e = g_node_table + i * 0x28;
            if ((short)(e[0x1C] | e[0x1D] << 8) == g_media_id) {
                memcpy(name, e, 8);
                name[8] = 0;
                break;
            }
        }
    }
    return name[0] ? name : NULL;
}

/* Other names a disc track goes by in subtitle sets, built from the game's own
 * tables: each voice track (g_media_table: XA file number, channel) under the
 * name of the node that plays it, so voices played outside the node player (the
 * idle "network voices", the clip after the ending) find their subtitles too.
 * Site B's idle voices (media 0x2BB-0x2C4, listed under the GaTE/P2 nodes) are
 * byte-identical copies of Site A's (0xA9-0xB2, the Env nodes). */
extern unsigned char g_media_table[]; /* 8-byte entries: u8 xa_file, u8, s16 channel/file, s32 size */
#define MEDIA_COUNT (0x1700 / 8)
typedef struct {
    char track[24];
    char names[2][12];
} TrackAlias;
static TrackAlias *track_aliases;
static int track_alias_count;

static void node_name_for_media(int media, char *out, size_t cap) {
    out[0] = 0;
    for (int i = 0; i < 0x2CC; i++) {
        const unsigned char *e = g_node_table + i * 0x28;
        if ((short)(e[0x1C] | e[0x1D] << 8) == media) {
            snprintf(out, cap, "%.8s", (const char *)e);
            return;
        }
    }
}

static void build_track_aliases(void) {
    track_aliases = calloc(MEDIA_COUNT + 1, sizeof *track_aliases);
    for (int m = 0; m < MEDIA_COUNT; m++) {
        const unsigned char *e = g_media_table + m * 8;
        int xa = e[0], chan = (short)(e[2] | e[3] << 8);
        if (xa == 0 || chan < 0 || chan > 31) {
            continue; /* a movie, or unused */
        }
        TrackAlias *a = &track_aliases[track_alias_count];
        snprintf(a->track, sizeof a->track, "LAIN%02d.XA.ch%02d", xa, chan);
        node_name_for_media(m, a->names[0], sizeof a->names[0]);
        if (m >= 0x2BB && m <= 0x2C4) {
            node_name_for_media(0xA9 + (m - 0x2BB), a->names[1], sizeof a->names[1]); /* Env0xx */
        } else if (m == 0x2DD) {
            snprintf(a->names[1], sizeof a->names[1], "Xa0001"); /* after the ending */
        }
        if (a->names[0][0] || a->names[1][0]) {
            track_alias_count++;
        }
    }
    /* The ending's credits movie. */
    TrackAlias *a = &track_aliases[track_alias_count++];
    snprintf(a->track, sizeof a->track, "ENDROLL1.STR");
    snprintf(a->names[0], sizeof a->names[0], "Endroll");
}

/* The name a pack uses for the track playing now: the disc track itself, one of the
 * names the game's tables give it (above), or the node the media player is playing.
 * `has` says whether the pack (subtitles or dubs) has a given name. */
static const char *resolve_track_name(const char *track, int (*has)(const char *)) {
    if (has(track)) {
        return track;
    }
    if (!track_aliases) {
        build_track_aliases();
    }
    for (int i = 0; i < track_alias_count; i++) {
        if (strcmp(track_aliases[i].track, track) != 0) {
            continue;
        }
        for (int k = 0; k < 2; k++) {
            const char *name = track_aliases[i].names[k];
            if (name[0] && has(name)) {
                return name;
            }
        }
        break;
    }
    const char *node = playing_node_name();
    return node && has(node) ? node : NULL;
}

/* Subtitles: text for the voice track or movie playing now (if a pack is loaded).
 * A pack names its tracks by disc track (LAIN01.XA.ch00, F001.STR) or by node
 * (Cou001): the disc track is tried first, then the names above, then the node
 * the media player is playing (movies). */
static int subtitles_loaded;
static void update_subtitle(void) {
    char track[48];
    long ms;
    const char *text = NULL;
    int playing = tracks_current(inserted_disc, track, sizeof track, &ms);
    /* A track can keep streaming after the game has silenced it (leaving the voice
     * player by a keyword jump mutes CD audio but lets the drive run on): subtitles and
     * dubs only follow what is audible. */
    SpuCommonAttr cd;
    SpuGetCommonAttr(&cd);
    if (!cd.cd.mix || (cd.cd.volume.left == 0 && cd.cd.volume.right == 0)) {
        playing = 0;
    }
    if (subtitles_loaded && playing) {
        const char *name = resolve_track_name(track, subtitles_has_track);
        text = name ? subtitles_lookup(name, ms) : NULL;
    }
    overlay_set_subtitle(text);
    if (frame_log) { /* LAIN_FRAME_LOG: each subtitle line as it appears (tests) */
        static char last[160];
        if (text && strncmp(text, last, sizeof last - 1) != 0) {
            printf("[lain] subtitle (%s at %ld ms): %.120s\n", track, ms, text);
        }
        snprintf(last, sizeof last, "%s", text ? text : "");
    }
    /* Dub packs name their files the same ways (LAIN01.XA.ch00.ogg or Cou001.ogg). */
    dub_player_update(playing ? resolve_track_name(track, dub_player_has) : NULL, ms);
}

/* Loads <data dir>/lang/<code>/ (or $LAIN_SUBTITLE_PACK) unless the text language
 * is the original Japanese. A pack's font.ttf is used for its subtitles. */
static void load_subtitle_pack(const Settings *settings) {
    char dir[1024], font[1100];
    const char *override = getenv("LAIN_SUBTITLE_PACK");
    if (override) {
        snprintf(dir, sizeof dir, "%s", override);
    } else if (strcmp(settings->text_lang, "ja") != 0) {
        snprintf(dir, sizeof dir, "%slang/%s", settings_data_dir(), settings->text_lang);
    } else {
        subtitles_unload();
        subtitles_loaded = 0;
        overlay_set_font(NULL);
        return;
    }
    int n = subtitles_load_pack(dir);
    subtitles_loaded = n > 0;
    printf("[lain] subtitles: %s (%d tracks)\n", dir, n);
    snprintf(font, sizeof font, "%s/font.ttf", dir);
    FILE *f = fopen(font, "rb");
    if (f) {
        fclose(f);
    }
    overlay_set_font(f ? font : NULL);
}

/* Loads <data dir>/dub/<code>/ (or $LAIN_DUB_PACK) unless the voices are the originals. */
static void load_dub_pack(const Settings *settings) {
    char dir[1024];
    const char *override = getenv("LAIN_DUB_PACK");
    if (override) {
        snprintf(dir, sizeof dir, "%s", override);
    } else if (strcmp(settings->voice_lang, "ja") != 0) {
        snprintf(dir, sizeof dir, "%sdub/%s", settings_data_dir(), settings->voice_lang);
    } else {
        dub_player_set_pack(NULL);
        return;
    }
    dub_player_set_pack(dir);
    printf("[lain] dubs: %s\n", dir);
}

/* An in-between frame (frame interpolation) is about to be shown: overlays go
 * on it too. LAIN_SCREENSHOT_INTERP=1 (tests) also saves the in-between frames
 * leading up to each screenshot frame, as <prefix>-<frame>-i<k>.bmp. */
static int interp_count;
static void on_interp_present(void) {
    static int last_frame = -1, k;
    overlay_draw();
    interp_count++;
    k = frame_count == last_frame ? k + 1 : 1;
    last_frame = frame_count;
    for (int i = 0; shot_path && getenv("LAIN_SCREENSHOT_INTERP") && i < shot_count; i++) {
        if (shot_frames[i] == frame_count + 1) {
            char path[1024];
            snprintf(path, sizeof path, "%s-%d-i%d.bmp", shot_path, frame_count + 1, k);
            save_window(path);
        }
    }
}

/* The settings the game runs with; the F1 menu edits them live. */
static Settings live_settings;

/* Turbo cheat: while Tab is held (and the menu is closed) the emulated vblank
 * clock runs `turbo` times faster, so the whole game speeds up. */
static void update_turbo(void) {
    const Uint8 *keys = SDL_GetKeyboardState(NULL);
    int on = live_settings.turbo >= 2 && !overlay_menu_is_open() && keys[SDL_SCANCODE_TAB];
    static int was_on;
    g_lain_speed = on ? (float)live_settings.turbo : 1.0f;
    if (on != was_on) {
        /* Display vsync would cap turbo at the monitor's refresh rate. */
        PsyX_EnableSwapInterval(on ? 0 : live_settings.vsync != 0);
        was_on = on;
    }
    if (on) {
        char msg[32];
        snprintf(msg, sizeof msg, ">> %dx", live_settings.turbo);
        overlay_toast(msg, 0.25f);
    }
}

/* Whether any key mapped to a pad button, or any controller button, is held. */
static int pad_input_held(void) {
    const PsyXKeyboardMapping *m = &g_cfg_keyboardMapping;
    const int keys[] = {m->kc_square, m->kc_circle, m->kc_triangle, m->kc_cross, m->kc_l1, m->kc_l2, m->kc_l3,
                        m->kc_r1, m->kc_r2, m->kc_r3, m->kc_start, m->kc_select, m->kc_dpad_left,
                        m->kc_dpad_right, m->kc_dpad_up, m->kc_dpad_down};
    int numkeys = 0;
    const Uint8 *state = SDL_GetKeyboardState(&numkeys);
    for (size_t i = 0; i < sizeof keys / sizeof keys[0]; i++) {
        if (keys[i] > 0 && keys[i] < numkeys && state[keys[i]]) {
            return 1;
        }
    }
    for (int j = 0; j < SDL_NumJoysticks(); j++) {
        SDL_GameController *gc = SDL_IsGameController(j)
                                     ? SDL_GameControllerFromInstanceID(SDL_JoystickGetDeviceInstanceID(j))
                                     : NULL;
        for (int b = 0; gc && b < SDL_CONTROLLER_BUTTON_MAX; b++) {
            if (SDL_GameControllerGetButton(gc, (SDL_GameControllerButton)b)) {
                return 1;
            }
        }
    }
    return 0;
}

/* The game gets no pad input while the F1 menu is open, and after it closes until the
 * key or button that closed it (Enter = Start, gamepad A = Cross...) is released, so
 * closing the menu doesn't also press a button in the game. */
static void update_pad_block(void) {
    static int was_open, wait_frames;
    const int open = overlay_menu_is_open();
    if (was_open && !open) {
        wait_frames = 60; /* at most a second */
    }
    was_open = open;
    if (wait_frames > 0 && (!pad_input_held() || --wait_frames == 0)) {
        wait_frames = 0;
    }
    g_lain_padBlocked = open || wait_frames > 0;
}

/* LAIN_ANIM_LOG=1 (tests): Lain's idle animation slot as it changes: when a pack
 * starts loading, when it is ready, when it plays. g_lain_idle_anims[0] is the port's
 * SoundSlot (port/game/8001D114.c): void *handle, s16 id, s16, s16 state (0 free,
 * 1 loading, 2 ready), ... (state at +12 with the 8-byte pointer). */
static void log_lain_anim(void) {
    static int on = -1, last_state = -1, last_cur = -2;
    extern unsigned char g_lain_idle_anims[];
    extern short g_lain_idle_anim_cur;
    if (on < 0) on = getenv("LAIN_ANIM_LOG") != NULL;
    if (!on) return;
    {
        extern int g_anim_last_frame_size, g_anim_decode_count;
        static int last_count = -1;
        if (g_anim_decode_count != last_count) {
            printf("[anim] frame %d vsync %d: decoded size %d\n", frame_count, VSync(-1), g_anim_last_frame_size);
            last_count = g_anim_decode_count;
        }
    }
    const size_t so = sizeof(void *) + 4;
    int state = (short)(g_lain_idle_anims[so] | g_lain_idle_anims[so + 1] << 8);
    int file = 0;
    if (state != last_state || g_lain_idle_anim_cur != last_cur) {
        (void)file;
        printf("[anim] frame %d vsync %d: slot %s, playing %s\n", frame_count, VSync(-1),
               state == 0 ? "free" : state == 1 ? "LOADING" : state == 2 ? "ready" : "failed",
               g_lain_idle_anim_cur >= 0 ? "idle pick" : "default loop");
        last_state = state;
        last_cur = g_lain_idle_anim_cur;
    }
}

static void on_end_scene(void) {
    frame_count++;
    modrt_frame();
    log_lain_anim();
    check_disc_swap();
    update_subtitle();
    overlay_draw();
    apply_keys();
    /* PsyCross leaves pad polling to the game loop; on the PS1 the pad buffers
     * refresh every frame by themselves. */
    update_pad_block(); /* the menu has the input */
    update_turbo();
    PsyX_UpdateInput();
    if (frame_log && frame_count % 60 == 0) {
        char track[48];
        long track_ms;
        if (tracks_current(inserted_disc, track, sizeof track, &track_ms)) {
            printf("[lain] frame %d: track %s at %ld ms\n", frame_count, track, track_ms);
        }
        static Uint64 last_t;
        static int last_vbl, last_interp;
        Uint64 now = SDL_GetPerformanceCounter();
        int vbl = VSync(-1);
        double secs = last_t ? (double)(now - last_t) / (double)SDL_GetPerformanceFrequency() : 0;
        {
            extern int g_anim_decode_count, g_vsync_counter; /* the game's own counters */
            printf("[lain] game counters: anim frames decoded %d, vsync %d\n", g_anim_decode_count, g_vsync_counter);
        }
        printf("[lain] frame %d (%.1f fps, %.1f shown/s, %.2f vblank/s; XA frames played %lld, cd lba %d%s)\n",
               frame_count, secs > 0 ? 60.0 / secs : 0.0, secs > 0 ? (60 + interp_count - last_interp) / secs : 0.0,
               secs > 0 ? (vbl - last_vbl) / secs : 0.0,
               LainXA_GetPlayedFrames(), LainCD_GetPosition(), LainCD_IsReading() ? ", reading" : "");
        last_t = now;
        last_vbl = vbl;
        last_interp = interp_count;
        fflush(stdout);
    }
    for (int i = 0; shot_path && i < shot_count; i++) {
        if (shot_frames[i] == frame_count) {
            char path[1024];
            snprintf(path, sizeof path, "%s-%d.bmp", shot_path, frame_count);
            save_window(path);
            if (getenv("LAIN_OT_DUMP")) {
                dump_ot();
            }
            if (getenv("LAIN_VRAM_DUMP")) {
                snprintf(path, sizeof path, "%s-%d-vram.tga", shot_path, frame_count);
                GR_SaveVRAM(path, 0, 0, 1024, 512, 1);
            }
        }
    }
    if (exit_frame && frame_count >= exit_frame) {
        printf("[lain] exit at frame %d\n", frame_count);
        fflush(stdout);
        exit(0);
    }
}

/* Prints the faulting address and a backtrace on a crash (lldb isn't always
 * usable in batch runs). Symbolize with atos -o lain -l <image base> <pc>. */
#ifndef _WIN32
#include <sys/ucontext.h>
static void crash_handler(int sig, siginfo_t *info, void *uctx) {
    void *frames[64];
    int n = backtrace(frames, 64);
    void *pc = NULL;
#if defined(__APPLE__) && defined(__aarch64__)
    pc = (void *)((ucontext_t *)uctx)->uc_mcontext->__ss.__pc;
#elif defined(__APPLE__) && defined(__x86_64__)
    pc = (void *)((ucontext_t *)uctx)->uc_mcontext->__ss.__rip;
#endif
    fprintf(stderr, "[lain] signal %d at pc %p, fault address %p, image base %p\n", sig, pc,
            info ? info->si_addr : NULL,
#ifdef __APPLE__
            (void *)_dyld_get_image_header(0)
#else
            NULL
#endif
    );
    backtrace_symbols_fd(frames, n, 2);
    _exit(128 + sig);
}
#endif /* !_WIN32 */

static void debug_hooks_init(void) {
#ifndef _WIN32
    struct sigaction sa;
    memset(&sa, 0, sizeof sa);
    sa.sa_sigaction = crash_handler;
    sa.sa_flags = SA_SIGINFO;
    int sigs[] = {SIGSEGV, SIGBUS, SIGABRT, SIGILL, SIGFPE};
    for (int i = 0; i < 5; i++) {
        sigaction(sigs[i], &sa, NULL);
    }
#endif
    shot_path = getenv("LAIN_SCREENSHOT");
    const char *frames = getenv("LAIN_SCREENSHOT_FRAMES");
    if (!frames) {
        frames = "300";
    }
    for (const char *p = frames; *p && shot_count < 64;) {
        shot_frames[shot_count++] = atoi(p);
        p = strchr(p, ',');
        if (!p) {
            break;
        }
        p++;
    }
    if (getenv("LAIN_EXIT_FRAME")) {
        exit_frame = atoi(getenv("LAIN_EXIT_FRAME"));
    }
    frame_log = getenv("LAIN_FRAME_LOG") != NULL;
    if (getenv("LAIN_KEYS")) {
        parse_keys(getenv("LAIN_KEYS"));
    }
    g_lain_onEndScene = on_end_scene;
}

/* The keyboard mapping from settings.ini (SDL key names) into PsyCross's pad emulation. */
static void apply_keyboard_mapping(const Settings *s) {
    PsyXKeyboardMapping *m = &g_cfg_keyboardMapping;
    int *dst[SETTINGS_NUM_KEYS] = {
        &m->kc_dpad_up, &m->kc_dpad_down, &m->kc_dpad_left, &m->kc_dpad_right,
        &m->kc_cross, &m->kc_circle, &m->kc_square, &m->kc_triangle,
        &m->kc_l1, &m->kc_l2, &m->kc_l3, &m->kc_r1, &m->kc_r2, &m->kc_r3,
        &m->kc_start, &m->kc_select,
    };
    for (int i = 0; i < SETTINGS_NUM_KEYS; i++) {
        SDL_Scancode sc = SDL_GetScancodeFromName(s->keys[i]);
        if (sc != SDL_SCANCODE_UNKNOWN) {
            *dst[i] = sc;
        } else if (s->keys[i][0]) {
            fprintf(stderr, "lain: unknown key '%s' for %s, keeping the default\n", s->keys[i],
                    SETTINGS_KEY_NAMES[i]);
        }
    }
}

int port_data_relocate(void);
int lain_game_main(void);

extern int port_cheat_open_nodes, port_cheat_genome_title; /* game/port_libc.c */


static void apply_video(const Settings *s) {
    g_cfg_renderScale = s->render_scale;
    g_cfg_interpolate = s->smooth;
    PsyX_EnableSwapInterval(s->vsync ? 1 : 0);
    SDL_Window *win = SDL_GL_GetCurrentWindow();
    int is_full = win && (SDL_GetWindowFlags(win) & SDL_WINDOW_FULLSCREEN) != 0;
    if (win && is_full != (s->fullscreen != 0)) {
        PsyX_ToggleFullscreen();
    }
}

static void apply_menu_change(int what) {
    Settings *s = &live_settings;
    if (what & OVERLAY_APPLY_VIDEO) {
        apply_video(s);
    }
    if (what & OVERLAY_APPLY_VOLUME) {
        LainSpu_SetOutputGain(s->volume / 100.0f);
    }
    if (what & OVERLAY_APPLY_TEXT) {
        load_subtitle_pack(s);
    }
    if (what & OVERLAY_APPLY_VOICE) {
        load_dub_pack(s);
    }
    if (what & OVERLAY_APPLY_CHEATS) {
        port_cheat_open_nodes = s->cheat_open_nodes;
        port_cheat_genome_title = s->cheat_genome;
    }
    if (what & OVERLAY_APPLY_KEYS) {
        apply_keyboard_mapping(s);
    }
    if (what & OVERLAY_APPLY_QUIT) {
        settings_save(s);
        exit(0);
    }
}

/* Loads the EXE image into the arena, opens the game window and runs the game. */
static int boot_game(const Settings *initial, const char *disc1, const char *disc2) {
    live_settings = *initial;
    const Settings *settings = &live_settings;
    char err[256];
    if (psx_mem_load(disc1, err, sizeof err) != 0) {
        fprintf(stderr, "lain: %s\n", err);
        return 1;
    }
    int bad = port_data_relocate();
    if (bad != 0) {
        fprintf(stderr, "lain: %d pointer words outside PS1 RAM\n", bad);
    }
    /* Player mods: file tables in the arena and the disc reader (client/mods.cpp). */
    mods_apply(settings->mods, disc1, disc2);
    mods_apply_data(settings->mods);
    /* Mod scripts and plugins: hooks on game functions, events (client/modrt.cpp). */
    modrt_start(settings->mods);
    /* HD texture packs, and the texture dump for making them (client/hdtex.cpp). */
    hdtex_start(settings->mods, getenv("LAIN_DUMP_TEXTURES") ? atoi(getenv("LAIN_DUMP_TEXTURES")) : settings->dump_textures);
    /* Internal resolution: 320x240 times render_scale, scaled to the window;
     * 0 renders at the window's own (HiDPI) resolution. LAIN_RENDER_SCALE (tests)
     * overrides it without saving to settings.ini. */
    g_cfg_renderScale = getenv("LAIN_RENDER_SCALE") ? atoi(getenv("LAIN_RENDER_SCALE")) : settings->render_scale;
    PsyX_Initialise("Serial Experiments Lain", settings->width, settings->height,
                    settings->fullscreen);
    if (!SDL_GL_GetCurrentWindow()) {
        /* Typically no OpenGL 3.2 driver (old GPU, remote desktop, VM). */
        char msg[512];
        snprintf(msg, sizeof msg,
                 "Could not open the game window with OpenGL 3.2.\n\n%s\n\n"
                 "Update your graphics driver; the game needs OpenGL 3.2 or newer.",
                 SDL_GetError());
        fprintf(stderr, "lain: %s\n", msg);
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Serial Experiments Lain", msg, NULL);
        return 1;
    }
    app_icon_apply(SDL_GL_GetCurrentWindow());
    /* F1: settings menu over the running game. */
    overlay_menu_init(&live_settings, apply_menu_change);
    port_cheat_open_nodes = settings->cheat_open_nodes;
    port_cheat_genome_title = settings->cheat_genome;
    g_cfg_interpolate = getenv("LAIN_SMOOTH") ? atoi(getenv("LAIN_SMOOTH")) : settings->smooth;
    g_lain_onInterpPresent = on_interp_present; /* subtitles and the F1 menu on in-between frames too */
    g_lain_onEvent = overlay_handle_event;
    LainSpu_SetOutputGain(settings->volume / 100.0f);
    /* Display vsync (swap interval) from the settings. The game's own timing
     * comes from the emulated 59.94 Hz vblank, so it doesn't depend on this. */
    apply_keyboard_mapping(settings);
    g_cfg_swapInterval = settings->vsync ? 1 : 0;
    PsyX_SetSwapInterval(1);
    PsyX_EnableSwapInterval(settings->vsync ? 1 : 0);
    /* TMDs in PS1 RAM keep PS1 addresses when mapped: the game reads them itself. */
    GsSetMapBase(psx_arena, PSX_ARENA_SIZE, PSX_RAM_BASE);
    /* libcd (and the lain_xa drive, through PsyCross's image) read the imported disc.
     * LAIN_DISC=2 (debug) starts with disc 2 in the drive; see check_disc_swap. */
    const char *disc = disc1;
    if (getenv("LAIN_DISC") && atoi(getenv("LAIN_DISC")) == 2 && disc2) {
        disc = disc2;
    }
    PsyX_CDFS_Init(disc, 0, 0);
    tracks_init(disc1, disc2); /* subtitles/dubs: which voice track or movie is playing */
    load_subtitle_pack(settings);
    load_dub_pack(settings);
    disc_paths[0] = disc1;
    disc_paths[1] = disc2;
    inserted_disc = disc == disc2 ? 1 : 0;
    /* Memory card slot 1: a raw 128 KiB card image in the user data folder. */
    {
        char card[1100];
        snprintf(card, sizeof card, "%smemcard1.mcd", settings_data_dir());
        if (getenv("LAIN_MEMCARD")) { /* debug/tests: another card image */
            snprintf(card, sizeof card, "%s", getenv("LAIN_MEMCARD"));
        }
        LainMcrd_SetCardPath(0, card);
    }
    debug_hooks_init();
    atexit(modrt_quit);
    /* Stop the CD drive thread before the process tears down (it runs game callbacks). */
    atexit(LainCD_Shutdown);
    lain_game_main(); /* never returns */
    return 0;
}

int main(int argc, char **argv) {
    /* `lain --make-subtitle-pack <code> [name]`: template SRT files for translators. */
    if (argc >= 3 && strcmp(argv[1], "--make-subtitle-pack") == 0) {
        char d1[1024], d2[1024], dir[1100], base[1100];
        const char *discs[2];
        int n = 0;
        SDL_Init(SDL_INIT_EVENTS);
        if (discs_imported(settings_data_dir(), DISC_1, d1, sizeof d1)) discs[n++] = d1;
        if (discs_imported(settings_data_dir(), DISC_2, d2, sizeof d2)) discs[n++] = d2;
        if (n == 0) {
            fprintf(stderr, "import your discs first (lain --import disc1.cue disc2.cue)\n");
            return 1;
        }
        snprintf(base, sizeof base, "%slang", settings_data_dir());
        snprintf(dir, sizeof dir, "%s/%s", base, argv[2]);
        int tracks = subkit_write_templates(discs, n, dir, argc >= 4 ? argv[3] : argv[2]);
        printf("%d subtitle templates in %s\n", tracks, dir);
        SDL_Quit();
        return tracks > 0 ? 0 : 1;
    }
    if (argc >= 2 && strcmp(argv[1], "--selftest") == 0) {
        return selftest();
    }
    /* `lain --check-mods`: the installed mods and the state of each file.
     * `lain --export-originals [dir]`: every archive entry as mod files. */
    if (argc >= 2 && (strcmp(argv[1], "--check-mods") == 0 || strcmp(argv[1], "--export-originals") == 0)) {
        Settings s;
        char d1[1024], d2[1024], msg[256] = "", out[1100];
        settings_load(&s);
        int have1 = discs_imported(settings_data_dir(), DISC_1, d1, sizeof d1);
        int have2 = discs_imported(settings_data_dir(), DISC_2, d2, sizeof d2);
        if (!have1) {
            fprintf(stderr, "import your discs first (lain --import disc1.cue disc2.cue)\n");
            return 1;
        }
        if (strcmp(argv[1], "--check-mods") == 0) {
            return mods_check_cli(s.mods, d1, have2 ? d2 : NULL);
        }
        snprintf(out, sizeof out, "%s", argc >= 3 ? argv[2] : "");
        if (!out[0]) {
            snprintf(out, sizeof out, "%smods/_originals", settings_data_dir());
        }
        int n = mods_export_originals(d1, have2 ? d2 : NULL, out, NULL, msg, sizeof msg);
        printf("%s: %s\n", out, msg);
        return n > 0 ? 0 : 1;
    }
    if (argc >= 3 && strcmp(argv[1], "--import") == 0) {
        SDL_Init(SDL_INIT_EVENTS);
        int rc = import_cli(argc - 2, argv + 2);
        SDL_Quit();
        return rc;
    }
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS) != 0) {
        fprintf(stderr, "SDL_Init: %s\n", SDL_GetError());
        return 1;
    }
    Settings settings;
    settings_load(&settings);

    /* `lain --play`: skip the launcher and boot with the saved settings/discs. */
    int play = argc >= 2 && strcmp(argv[1], "--play") == 0;

    /* The launcher is also where discs get imported. */
    if (!play && !setup_ui_run(&settings)) {
        SDL_Quit();
        return 0;
    }

    char disc1[1024], disc2[1024];
    int have1 = discs_imported(settings_data_dir(), DISC_1, disc1, sizeof disc1);
    int have2 = discs_imported(settings_data_dir(), DISC_2, disc2, sizeof disc2);
    printf("data dir: %s\ndisc 1: %s\ndisc 2: %s\n", settings_data_dir(),
           have1 ? disc1 : "(missing)", have2 ? disc2 : "(missing)");
    printf("display: %s %dx%d, render scale %dx; text %s, voices %s\n",
           settings.fullscreen ? "fullscreen" : "windowed", settings.width, settings.height,
           settings.render_scale, settings.text_lang, settings.voice_lang);
    if (!have1) {
        fprintf(stderr, "lain: import disc 1 first\n");
        SDL_Quit();
        return 1;
    }
    /* The launcher's window is gone; PsyCross opens the game window. */
    SDL_Quit();
    return boot_game(&settings, disc1, have2 ? disc2 : NULL);
}
