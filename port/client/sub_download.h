/* Getting subtitle packs onto the player's machine without shipping any:
 * download a published fan subtitle set, or copy a folder the player already has,
 * into <data dir>/lang/<code>/. Runs in the background; poll the job. */
#pragma once

#include <atomic>
#include <string>

struct SubJob {
    enum State { IDLE, RUNNING, DONE, FAILED };
    std::atomic<int> state{IDLE};
    std::string message; /* result text, set before state becomes DONE/FAILED */
    std::string code;    /* pack folder name on success */
};

/* A subtitle set the launcher offers to download. */
struct SubSource {
    const char *label;   /* button text */
    const char *code;    /* folder under lang/ */
    const char *name;    /* pack name shown in the language list */
    const char *url;     /* .tar.gz of the whole set */
    const char *credit;  /* where it comes from, shown next to the button */
};

extern const SubSource kSubSources[];
extern const int kSubSourceCount;

/* Downloads and unpacks `src` into <data dir>/lang/<src.code>/ (replacing it).
 * Uses the system's curl and tar (macOS, Windows 10+, Linux). */
void sub_download_start(SubJob *job, const SubSource &src);

/* Removes temporary folders a download or import left behind (e.g. the program was
 * closed in the middle). Call once at startup. */
void sub_cleanup_leftovers(void);

/* Copies the folder `dir` (subtitle files, any layout) to <data dir>/lang/<its name>/. */
void sub_import_folder_start(SubJob *job, const std::string &dir);
