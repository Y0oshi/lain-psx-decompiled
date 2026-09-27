/* In-game overlay drawn over the game's frame (Dear ImGui on the game's GL context). */
#pragma once

#include "settings.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Shows a short message ("Disc 2 inserted") for `seconds`. */
void overlay_toast(const char *message, float seconds);

/* Sets the subtitle line shown at the bottom of the frame (NULL or "" hides it).
 * Lines are separated by '\n'. */
void overlay_set_subtitle(const char *text);

/* TTF font for subtitles (e.g. a pack's font.ttf, needed for non-Latin scripts).
 * Must be called before the first overlay_draw; NULL keeps the built-in font. */
void overlay_set_font(const char *ttf_path);

/* The F1 settings menu edits `settings` in place (saved when it closes) and calls
 * `apply` with OVERLAY_APPLY_* bits for what changed. */
enum {
    OVERLAY_APPLY_VIDEO = 1,  /* fullscreen, vsync, render scale */
    OVERLAY_APPLY_VOLUME = 2,
    OVERLAY_APPLY_TEXT = 4,   /* subtitle pack */
    OVERLAY_APPLY_VOICE = 8,  /* dub pack */
    OVERLAY_APPLY_QUIT = 16,
    OVERLAY_APPLY_KEYS = 32,  /* keyboard mapping */
    OVERLAY_APPLY_CHEATS = 64,
};
typedef void (*OverlayApplyFn)(int what);
void overlay_menu_init(Settings *settings, OverlayApplyFn apply);
int overlay_menu_is_open(void);

/* Gives the overlay an SDL event first (install as g_lain_onEvent). Returns 1
 * when the event is used by the menu and should not reach the game. */
union SDL_Event;
int overlay_handle_event(const union SDL_Event *event);

/* Draws the overlay into the current back buffer; call once per frame before
 * the swap. Does nothing (and costs nothing) while nothing is showing. */
void overlay_draw(void);

#ifdef __cplusplus
}
#endif
