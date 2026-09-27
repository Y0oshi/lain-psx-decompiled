#include "settings.h"

#include <SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

const char *settings_data_dir(void) {
    static char *dir;
    if (!dir && getenv("LAIN_DATA_DIR")) {
        /* tests: a separate data folder (settings, discs, memory card, packs) */
        const char *d = getenv("LAIN_DATA_DIR");
        size_t n = strlen(d);
        dir = SDL_malloc(n + 2);
        memcpy(dir, d, n + 1);
        if (n && d[n - 1] != '/' && d[n - 1] != '\\') {
            dir[n] = '/';
            dir[n + 1] = 0;
        }
    }
    if (!dir) {
        /* %APPDATA%\LainNative\lain-native\, ~/Library/Application Support/LainNative/lain-native/, ... */
        dir = SDL_GetPrefPath("LainNative", "lain-native");
        if (!dir) {
            dir = SDL_strdup("./");
        }
    }
    return dir;
}

const char *const SETTINGS_KEY_NAMES[SETTINGS_NUM_KEYS] = {
    "up", "down", "left", "right", "cross", "circle", "square", "triangle",
    "l1", "l2", "l3", "r1", "r2", "r3", "start", "select",
};

static const char *const DEFAULT_KEYS[SETTINGS_NUM_KEYS] = {
    "Up", "Down", "Left", "Right", "C", "V", "X", "Z",
    "Left Shift", "Left Ctrl", "[", "Right Shift", "Right Ctrl", "]", "Return", "Space",
};

const char *settings_default_key(int button) {
    return button >= 0 && button < SETTINGS_NUM_KEYS ? DEFAULT_KEYS[button] : "";
}

void settings_defaults(Settings *s) {
    memset(s, 0, sizeof *s);
    s->fullscreen = 0;
    s->width = 1280;
    s->height = 960;
    s->render_scale = 0; /* auto: the window's resolution */
    s->vsync = 1;
    s->smooth = 1;
    s->volume = 100;
    strcpy(s->text_lang, "ja");
    strcpy(s->voice_lang, "ja");
    for (int i = 0; i < SETTINGS_NUM_KEYS; i++) {
        snprintf(s->keys[i], sizeof s->keys[i], "%s", DEFAULT_KEYS[i]);
    }
}

static void settings_path(char *out, size_t cap) {
    snprintf(out, cap, "%ssettings.ini", settings_data_dir());
}

void settings_load(Settings *s) {
    settings_defaults(s);
    char path[1024], line[256];
    settings_path(path, sizeof path);
    FILE *f = fopen(path, "r");
    if (!f) {
        return;
    }
    while (fgets(line, sizeof line, f)) {
        char key[64], val[128];
        if (sscanf(line, " %63[^= ] = %127[^\r\n]", key, val) != 2) {
            continue;
        }
        if (!strcmp(key, "fullscreen")) s->fullscreen = atoi(val);
        else if (!strcmp(key, "width")) s->width = atoi(val);
        else if (!strcmp(key, "height")) s->height = atoi(val);
        else if (!strcmp(key, "render_scale")) s->render_scale = atoi(val);
        else if (!strcmp(key, "vsync")) s->vsync = atoi(val);
        else if (!strcmp(key, "smooth")) s->smooth = atoi(val);
        else if (!strcmp(key, "volume")) s->volume = atoi(val);
        else if (!strcmp(key, "cheat_open_nodes")) s->cheat_open_nodes = atoi(val);
        else if (!strcmp(key, "turbo")) s->turbo = atoi(val);
        else if (!strcmp(key, "cheat_genome")) s->cheat_genome = atoi(val);
        else if (!strcmp(key, "text_lang")) snprintf(s->text_lang, sizeof s->text_lang, "%s", val);
        else if (!strcmp(key, "voice_lang")) snprintf(s->voice_lang, sizeof s->voice_lang, "%s", val);
        else if (!strcmp(key, "setup_done")) s->setup_done = atoi(val);
        else if (!strncmp(key, "key_", 4)) {
            for (int i = 0; i < SETTINGS_NUM_KEYS; i++) {
                if (!strcmp(key + 4, SETTINGS_KEY_NAMES[i])) {
                    snprintf(s->keys[i], sizeof s->keys[i], "%s", val);
                }
            }
        }
    }
    fclose(f);
    if (s->width < 320) s->width = 320;
    if (s->height < 240) s->height = 240;
    if (s->render_scale < 0 || s->render_scale > 8) s->render_scale = 0;
    if (s->volume < 0 || s->volume > 100) s->volume = 100;
    if (s->turbo < 0 || s->turbo > 8 || s->turbo == 1) s->turbo = 0;
}

void settings_save(const Settings *s) {
    char path[1024];
    settings_path(path, sizeof path);
    FILE *f = fopen(path, "w");
    if (!f) {
        return;
    }
    fprintf(f, "; Lain native client settings\n");
    fprintf(f, "fullscreen = %d\nwidth = %d\nheight = %d\nrender_scale = %d\nvsync = %d\n"
               "smooth = %d\nvolume = %d\n",
            s->fullscreen, s->width, s->height, s->render_scale, s->vsync, s->smooth, s->volume);
    fprintf(f, "; cheats\ncheat_open_nodes = %d\nturbo = %d\ncheat_genome = %d\n", s->cheat_open_nodes, s->turbo,
            s->cheat_genome);
    fprintf(f, "text_lang = %s\nvoice_lang = %s\nsetup_done = %d\n", s->text_lang, s->voice_lang,
            s->setup_done);
    fprintf(f, "; keyboard: SDL key names\n");
    for (int i = 0; i < SETTINGS_NUM_KEYS; i++) {
        fprintf(f, "key_%s = %s\n", SETTINGS_KEY_NAMES[i], s->keys[i]);
    }
    fclose(f);
}
