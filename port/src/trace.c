/* Function-entry trace of the game code (debug builds with -DLAIN_TRACE=ON).
 *
 * The game library is built with -finstrument-functions; this prints each game
 * function the first time it runs, with the frame number, so a stall shows
 * where the game got to. LAIN_TRACE_ALL=1 prints every call (very verbose);
 * LAIN_TRACE_HOT=N prints, every N frames, the functions called in that window
 * with their call counts. */
#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#define NO_INSTR __attribute__((no_instrument_function))

extern int lain_frame_count(void) NO_INSTR;

#define SEEN_CAP 4096
static void *seen[SEEN_CAP];
static unsigned hits[SEEN_CAP];
static int trace_all = -1;
static int hot_every;
static int hot_last_frame;

NO_INSTR static const char *fn_name(void *fn) {
    Dl_info info;
    return dladdr(fn, &info) && info.dli_sname ? info.dli_sname : "?";
}

NO_INSTR static int mark_seen(void *fn) {
    unsigned h = (unsigned)(((uintptr_t)fn >> 2) * 2654435761u) % SEEN_CAP;
    while (seen[h] != NULL) {
        if (seen[h] == fn) {
            hits[h]++;
            return 0;
        }
        h = (h + 1) % SEEN_CAP;
    }
    seen[h] = fn;
    hits[h] = 1;
    return 1;
}

NO_INSTR static void hot_report(int frame) {
    fprintf(stderr, "[hot] frames %d-%d:", hot_last_frame, frame);
    for (int i = 0; i < SEEN_CAP; i++) {
        if (seen[i] && hits[i]) {
            fprintf(stderr, " %s:%u", fn_name(seen[i]), hits[i]);
            hits[i] = 0;
        }
    }
    fprintf(stderr, "\n");
    hot_last_frame = frame;
}

NO_INSTR void __cyg_profile_func_enter(void *fn, void *caller) {
    (void)caller;
    if (trace_all < 0) {
        trace_all = getenv("LAIN_TRACE_ALL") != NULL;
        hot_every = getenv("LAIN_TRACE_HOT") ? atoi(getenv("LAIN_TRACE_HOT")) : 0;
    }
    int frame = lain_frame_count();
    if (hot_every > 0 && frame - hot_last_frame >= hot_every) {
        hot_report(frame);
    }
    if (mark_seen(fn) || trace_all) {
        fprintf(stderr, "[trace] frame %d: %s\n", frame, fn_name(fn));
    }
}

NO_INSTR void __cyg_profile_func_exit(void *fn, void *caller) {
    (void)fn;
    (void)caller;
}
