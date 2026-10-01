/* Mods: player-installed folders <data dir>/mods/<folder>/ with an optional
 * mod.ini (name, author, version, description). A mod can replace archive entries
 * (SITEA.BIN/0631.png, BIN.BIN/0020.txt), pictures and models in the program
 * (PROGRAM/) and whole movie and voice files (MOVIE/, XA/), change game data
 * (data/), add HD textures (textures/) and run code (main.lua, plugins/). Before the
 * game starts, every enabled mod is checked and turned into the bytes the game would
 * read; the disc reader then serves those bytes in place of the disc's.
 * See port/MODDING.md.
 */
#pragma once
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Applies the enabled mods (Settings.mods) before the game starts: the game's
 * file tables and the disc reader. disc2 may be NULL. Prints a summary and
 * returns the number of entries and files replaced. */
int mods_apply(const char *mods_setting, const char *disc1, const char *disc2);

/* Applies the enabled mods' game data changes (their data folder's JSON files)
 * to the game's memory, after mods_apply and before the game starts. */
void mods_apply_data(const char *mods_setting);

/* Prints every installed mod and the state of its files (lain --check-mods).
 * Returns 0 if no enabled mod has an error. */
int mods_check_cli(const char *mods_setting, const char *disc1, const char *disc2);

/* Writes every entry of the discs' archives to out_dir as the files a mod
 * uses (pictures as PNG, voices as WAV, text as TXT, the rest as BIN).
 * progress (0..1) may be NULL. Returns the number of files written, -1 on error. */
int mods_export_originals(const char *disc1, const char *disc2, const char *out_dir, volatile float *progress,
                          char *msg, size_t msg_cap);

#ifdef __cplusplus
}

#include <string>
#include <vector>

struct ModFile {
    std::string path;     /* relative to the mod folder, e.g. "SITEA.BIN/0631.png" */
    std::string archive;  /* "SITEA.BIN" */
    int entry = -1;
    int level = 0;        /* 0 ok, 1 warning (still used), 2 error (skipped) */
    std::string note;
};

struct ModInfo {
    std::string folder;   /* folder name under mods/ */
    std::string name, author, version, description;
    bool enabled = true;
    std::vector<ModFile> files;
    std::vector<std::string> ignored;  /* files the mod system doesn't use */
    bool has_script = false;           /* main.lua */
    std::vector<std::string> scripts;  /* .lua files */
    std::vector<std::string> plugins;  /* plugins/ */
    std::vector<std::string> data;     /* data/ (game data changes) */
    int textures = 0;                  /* HD textures in textures/ */
    int errors = 0, warnings = 0;
};

/* <data dir>/mods, created if needed (no trailing separator). */
std::string mods_dir();

/* The installed mods in load order with their on/off state from the setting. */
std::vector<ModInfo> mods_scan(const char *mods_setting);

/* Checks every file of every mod against the discs (fills level/note and the
 * counts; later enabled mods override earlier ones). disc2 may be NULL. */
void mods_validate(std::vector<ModInfo> &mods, const char *disc1, const char *disc2);

/* The setting string for this order and on/off state. */
std::string mods_setting_string(const std::vector<ModInfo> &mods);

/* The mods mods_apply used, in load order (for the in-game menu). */
struct ModActive {
    std::string name, version;
    int files = 0;      /* replacement files used */
    int overridden = 0; /* files a later mod replaces */
    int skipped = 0;    /* files with problems */
};
const std::vector<ModActive> &mods_active();

/* Creates mods/<folder>/ with a mod.ini template. Returns false (reason in err)
 * if the name is empty or taken. */
bool mods_create(const std::string &name, std::string &folder, std::string &err);

#endif
