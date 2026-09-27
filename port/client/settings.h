/* Player settings, stored as settings.ini in the user data folder. */
#pragma once

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    int fullscreen;       /* 0 windowed, 1 fullscreen (desktop resolution) */
    int width, height;    /* window size when windowed */
    int render_scale;     /* internal resolution: 320x240 times this; 0 = window resolution */
    int vsync;
    int smooth;           /* 60 Hz with in-between frames on 12/30 fps screens (1), or original (0) */
    int volume;           /* master volume, 0..100 */
    int cheat_open_nodes; /* cheat: every node opens regardless of progress */
    int turbo;            /* cheat: speed while Tab is held (2..8), 0 = off */
    int cheat_genome;     /* cheat: saves always get the rare "Genome" title */
    char text_lang[64];   /* "ja" (original) or a subtitle/UI pack code, e.g. "en" */
    char voice_lang[64];  /* "ja" (original) or an installed dub pack code */
    int setup_done;       /* setup window completed at least once */
    /* Keyboard mapping: SDL key names (e.g. "Return", "C", "Up") for the pad
     * buttons, in SETTINGS_KEY_NAMES order; stored as key_<button> = <name>. */
    char keys[16][32];
} Settings;

enum { SETTINGS_NUM_KEYS = 16 };
/* up, down, left, right, cross, circle, square, triangle, l1, l2, l3, r1, r2, r3, start, select */
extern const char *const SETTINGS_KEY_NAMES[SETTINGS_NUM_KEYS];
/* The default SDL key name for a button (SETTINGS_KEY_NAMES order). */
const char *settings_default_key(int button);

/* The per-user data folder (created if needed), with a trailing separator. */
const char *settings_data_dir(void);

void settings_defaults(Settings *s);
void settings_load(Settings *s);
void settings_save(const Settings *s);

#ifdef __cplusplus
}
#endif
