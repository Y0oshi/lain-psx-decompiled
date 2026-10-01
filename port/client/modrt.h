/* The mod runtime: Lua scripts and native plugins of the enabled mods
 * (port/MODDING.md). */
#pragma once

#ifdef __cplusplus
extern "C" {
#endif

/* Loads every enabled mod's scripts (main.lua) and plugins (plugins/) and runs
 * them once, before the game starts; they install their hooks and events. */
void modrt_start(const char *mods_setting);

/* Once per game frame (main thread). */
void modrt_frame(void);

/* Inside the overlay's ImGui frame: script and plugin windows and overlays. */
int modrt_wants_draw(void);
void modrt_draw(void);

/* The F1 menu's Mods tab: each mod's settings (its "menu" event) and log. */
void modrt_menu(void);

void modrt_quit(void);

/* Replaces game function `name` for the client itself (like a plugin hook). */
int modrt_hook(const char *name, void *replacement, void **next);

#ifdef __cplusplus
}
#endif
