/* The interface native mod plugins get (port/MODDING.md, "Plugins").
 *
 * A plugin is a shared library in a mod's plugins/ folder (.dll on Windows,
 * .dylib on macOS, .so on Linux) that exports
 *
 *     int lain_mod_init(const LainModAPI *api);    returns 0 on success
 *     void lain_mod_quit(void);                    optional
 *
 * lain_mod_init runs once, before the game starts. The plugin keeps `api`; it
 * stays valid until the game quits. Everything here runs on the game's main
 * thread unless noted. This header has no dependencies; copy it into a plugin.
 */
#ifndef LAIN_MOD_API_H
#define LAIN_MOD_API_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define LAIN_MOD_API_VERSION 1

typedef struct LainModAPI {
    int version;            /* LAIN_MOD_API_VERSION the game was built with */
    const char *mod_dir;    /* this mod's folder (UTF-8, no trailing separator) */
    const char *data_dir;   /* the game's user data folder (trailing separator) */

    /* Prints to the game's log and the F1 menu's Mods tab: api->log(api, ...). */
    void (*log)(const struct LainModAPI *self, const char *fmt, ...);

    /* PS1 memory. Game variables keep their PS1 addresses (0x80000000 up);
     * ps1_ptr gives the native pointer to one (NULL outside PS1 RAM). */
    void *(*ps1_ptr)(uint32_t addr);
    uint32_t (*ps1_addr)(const void *ptr);  /* 0 if the pointer isn't in PS1 RAM */

    /* A game variable by name (game/generated/symbols.txt): its PS1 address and
     * size, and the native variable for the few that live outside PS1 RAM.
     * Returns 0 if unknown. */
    int (*symbol)(const char *name, uint32_t *addr, uint32_t *size, void **host);

    /* Game functions by name (e.g. "site_move_cursor"; function_count and
     * function_at list them all). `function` returns what the game calls, so
     * calling it runs every hook. */
    void *(*function)(const char *name);
    int (*function_count)(void);
    const char *(*function_at)(int index, const char **signature);

    /* Replaces a game function with `replacement`, which must have the same C
     * signature. *next receives what to call for the previous behavior (the
     * original or an earlier plugin's replacement). Plugin hooks run after all
     * script hooks. A replacement runs on the caller's thread: the few CD
     * callbacks run on the CD drive thread. Returns 0 on success. May be called
     * from lain_mod_init on. */
    int (*hook)(const char *name, void *replacement, void **next);

    /* Called once per game frame. */
    void (*on_frame)(void (*fn)(void *user), void *user);

    /* Called inside the game's ImGui frame, for windows and overlays. Use the
     * ImGui context from imgui_context (Dear ImGui 1.92.9) with
     * ImGui::SetCurrentContext and ImGui::SetAllocatorFunctions. */
    void (*on_draw)(void (*fn)(void *user), void *user);
    void *(*imgui_context)(void);
    void (*imgui_allocators)(void **alloc_fn, void **free_fn, void **user);

    /* A function mod scripts can call as lain.plugin(name, ...): integer
     * arguments in, one integer out. */
    void (*add_command)(const char *name, int64_t (*fn)(const int64_t *args, int nargs));

    /* Shows a short message over the game. */
    void (*toast)(const char *message, float seconds);

    /* Frames presented since the game started. */
    int (*frame)(void);
} LainModAPI;

typedef int (*LainModInitFn)(const LainModAPI *api);
typedef void (*LainModQuitFn)(void);

#ifdef __cplusplus
}
#endif

#endif
