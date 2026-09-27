/* Installed subtitle ("lang") and dub ("dub") packs, for the launcher and the in-game menu. */
#pragma once

#include <string>
#include <vector>

struct PackOption {
    std::string code;   // "ja" or pack folder name
    std::string label;  // shown in the combo
};

// The original Japanese first, then every folder under <data dir>/<kind>/.
std::vector<PackOption> find_packs(const char *kind, const char *original_label);
