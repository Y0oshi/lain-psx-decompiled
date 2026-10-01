/* HD texture packs (port/MODDING.md, "Texture packs"). */
#pragma once

#ifdef __cplusplus
extern "C" {
#endif

/* Indexes the enabled mods' textures/ folders and, when dump is set, saves every
 * picture the game draws to <data dir>/texture_dump/, named for texture packs. */
void hdtex_start(const char *mods_setting, int dump);

#ifdef __cplusplus
}
#endif
