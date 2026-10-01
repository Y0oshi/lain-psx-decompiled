/* Game data changes from mods: the JSON files in a mod's data folder
 * (port/MODDING.md, "Game data"). */
#pragma once

#include <stdint.h>
#include <string>
#include <vector>

struct ModInfo;

/* Checks (apply = false, against the program image `exe` as loaded from disc 1)
 * or applies (apply = true, to the running game's memory) one data file.
 * Returns the level (0 ok, 1 notes, 2 errors) and the messages. */
int moddata_file(const std::string &path, bool apply, const std::vector<uint8_t> *exe, std::string &messages);

/* Applies every enabled mod's data files in load order, before the game starts. */
void moddata_apply(const std::vector<ModInfo> &mods, const std::string &mods_dir);
