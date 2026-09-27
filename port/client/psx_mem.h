/* Fills the PS1 RAM arena (game/include/psx_arena.h) from the player's disc. */
#pragma once

#ifdef __cplusplus
extern "C" {
#endif

/* Copies SLPS_016.03 (the game EXE, identical on both discs) from the imported
 * disc image into the arena at its load address and clears the rest.
 * Returns 0 on success; on failure writes a message to err. */
int psx_mem_load(const char *disc_bin, char *err, int err_cap);

#ifdef __cplusplus
}
#endif
