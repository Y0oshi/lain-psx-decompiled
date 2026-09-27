/* The PsyQ C library functions whose host versions behave differently.
 *
 * rand(): the game expects PsyQ's generator, which returns 0..0x7FFF (the host's
 * returns up to 2^31 - 1). Code such as the music player's level meter
 * (media_player_run: rand() / 8191 as a bar count) overruns its arrays with host
 * values. Same LCG as the EXE's libc2 rand/srand (seed in bss, starts at 0). */
#include "common.h"

static u32 port_rand_seed;

int port_rand(void) {
    port_rand_seed = port_rand_seed * 0x41C64E6D + 12345;
    return (port_rand_seed >> 16) & 0x7FFF;
}

void port_srand(unsigned int seed) {
    port_rand_seed = seed;
}

/* Cheat "open any node" (F1 menu), or LAIN_UNLOCK_ALL=1: every node is openable
 * regardless of progress (node_pick_select_anim). Only the check changes; saves don't. */
int port_cheat_open_nodes;

/* Cheat "Genome save title": save_build_image always uses the rare title. */
int port_cheat_genome_title;

int port_unlock_all(void) {
    static int env = -1;
    if (env < 0) {
        env = getenv("LAIN_UNLOCK_ALL") != NULL;
    }
    return env || port_cheat_open_nodes;
}
