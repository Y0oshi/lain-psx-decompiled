/* Player-supplied disc dumps: identification and one-time import.
 *
 * The game reads files by fixed sector numbers, so disc images are kept whole:
 * import verifies a dump and copies its raw (2352-byte sector) data track into
 * the user data folder as disc1.bin / disc2.bin.
 */
#pragma once
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

enum { DISC_UNKNOWN = 0, DISC_1 = 1, DISC_2 = 2 };

typedef enum {
    IMPORT_IDLE,
    IMPORT_RUNNING,
    IMPORT_DONE,
    IMPORT_FAILED,
} ImportState;

typedef struct {
    volatile ImportState state;
    volatile float progress;   /* 0..1 */
    int disc;                  /* DISC_1 / DISC_2 once identified */
    int verified;              /* 1 if the SHA-1 matches a known retail dump */
    char sha1[41];
    char message[256];         /* error or result, for the UI */
} ImportJob;

/* Resolves a .cue (or a bare .bin) to the path of its raw data track.
 * Returns 0 on success, writes an error to err otherwise. */
int discs_resolve_track(const char *path, char *bin_out, int bin_cap, char *err, int err_cap);

/* Which disc a raw image is, from the LAIN_n.INF file on it (DISC_UNKNOWN if none). */
int discs_identify_by_contents(const char *bin_path);

/* Starts importing `path` (.cue/.bin) into data_dir on a background thread.
 * Poll job->state / job->progress from the UI thread. */
void discs_import_start(ImportJob *job, const char *path, const char *data_dir);

/* Path of an imported disc (data_dir/discN.bin) if it exists; returns 1 if present. */
int discs_imported(const char *data_dir, int disc, char *out, int cap);

#ifdef __cplusplus
}
#endif
