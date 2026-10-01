/* Example native plugin: counts the cursor moves on the node map and lets
 * scripts read the count.
 *
 * Build from this folder (the header is port/client/lain_mod_api.h):
 *   mkdir -p ../plugins
 *   macOS:   cc -shared -fPIC -I ../../../client example.c -o ../plugins/example.dylib
 *   Linux:   cc -shared -fPIC -I ../../../client example.c -o ../plugins/example.so
 *   Windows: x86_64-w64-mingw32-gcc -shared -I ../../../client example.c -o ../plugins/example.dll
 */
#include <stdint.h>

#include "lain_mod_api.h"

#ifdef _WIN32
#define EXPORT __declspec(dllexport)
#else
#define EXPORT __attribute__((visibility("default")))
#endif

static const LainModAPI *api;
static int32_t (*next_move)(int32_t dx, int32_t dy);
static int64_t moves;

/* Same C signature as the game's site_move_cursor (s32 (s32, s32)). */
static int32_t on_move(int32_t dx, int32_t dy) {
    moves++;
    return next_move(dx, dy);
}

static void every_frame(void *user) {
    (void)user;
    if (api->frame() % 600 == 0) {
        api->log(api, "%lld cursor moves so far", (long long)moves);
    }
}

/* lain.plugin("moves") in a script */
static int64_t cmd_moves(const int64_t *args, int nargs) {
    (void)args;
    (void)nargs;
    return moves;
}

EXPORT int lain_mod_init(const LainModAPI *a) {
    if (a->version < LAIN_MOD_API_VERSION) {
        return -1;
    }
    api = a;
    if (api->hook("site_move_cursor", (void *)on_move, (void **)&next_move) != 0) {
        return -1;
    }
    api->on_frame(every_frame, 0);
    api->add_command("moves", cmd_moves);
    api->log(api, "example plugin ready (mod folder %s)", api->mod_dir);
    return 0;
}
