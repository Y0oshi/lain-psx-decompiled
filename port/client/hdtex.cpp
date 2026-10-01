/* HD texture packs: the textures/ folders of the enabled mods, and the texture
 * dump that names each picture the game draws (port/MODDING.md, "Texture packs"). */
#include "hdtex.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>

#include <filesystem>
#include <map>
#include <string>
#include <vector>

#include "lain_hd.h"
#include "mods.h"
#include "png.h"
#include "settings.h"

namespace fs = std::filesystem;

namespace {

std::map<std::pair<uint64_t, uint64_t>, std::string> g_files; /* (image, palette or 0) -> PNG */
bool g_dump;
std::string g_dump_dir;

std::string hex(uint64_t v) {
    char b[17];
    snprintf(b, sizeof b, "%016llx", (unsigned long long)v);
    return b;
}

/* "<image>.png" or "<image>-<palette>.png", 16 hex digits each */
bool parse_name(const std::string &stem, uint64_t &image, uint64_t &palette) {
    auto h = [](const std::string &s, uint64_t &v) {
        if (s.size() != 16) return false;
        char *end = nullptr;
        v = strtoull(s.c_str(), &end, 16);
        return end && *end == 0;
    };
    palette = 0;
    if (stem.size() == 16) return h(stem, image);
    return stem.size() == 33 && stem[16] == '-' && h(stem.substr(0, 16), image) && h(stem.substr(17), palette);
}

/* The picture as the game draws it with this palette (color 0x0000 transparent). */
std::vector<uint8_t> decode(const LainHDKey *k) {
    std::vector<uint8_t> out((size_t)k->width * k->h * 4);
    for (int y = 0; y < k->h; y++) {
        const uint16_t *row = k->vram + (k->y + y) * 1024 + k->x;
        for (int x = 0; x < k->width; x++) {
            uint16_t c;
            if (k->bpp == 16) {
                c = row[x];
            } else {
                int idx = k->bpp == 8 ? (row[x / 2] >> ((x & 1) * 8)) & 0xFF : (row[x / 4] >> ((x & 3) * 4)) & 0xF;
                c = k->vram[k->clut_y * 1024 + k->clut_x + idx];
            }
            uint8_t *p = &out[((size_t)y * k->width + x) * 4];
            p[0] = (uint8_t)((c & 31) << 3);
            p[1] = (uint8_t)(((c >> 5) & 31) << 3);
            p[2] = (uint8_t)(((c >> 10) & 31) << 3);
            p[3] = c == 0 ? 0 : 255;
        }
    }
    return out;
}

unsigned int provide(const LainHDKey *k) {
    if (g_dump) {
        std::string name = hex(k->image) + (k->bpp == 16 ? "" : "-" + hex(k->palette)) + ".png";
        fs::path p = fs::u8path(g_dump_dir) / name;
        std::error_code ec;
        if (!fs::exists(p, ec)) {
            std::vector<uint8_t> px = decode(k);
            png_save(p.u8string().c_str(), px.data(), k->width, k->h);
        }
    }
    auto it = g_files.find({k->image, k->palette});
    if (it == g_files.end()) it = g_files.find({k->image, 0}); /* any palette */
    if (it == g_files.end()) return 0;
    int w = 0, h = 0;
    char err[128];
    uint8_t *rgba = png_load(it->second.c_str(), &w, &h, err, sizeof err);
    if (!rgba) {
        printf("[mods] texture %s: %s\n", it->second.c_str(), err);
        return 0;
    }
    unsigned int tex = GR_CreateHDTexture(w, h, rgba);
    free(rgba);
    return tex;
}

} // namespace

extern "C" void hdtex_start(const char *mods_setting, int dump) {
    std::error_code ec;
    for (const ModInfo &m : mods_scan(mods_setting)) {
        if (!m.enabled) continue;
        fs::path dir = fs::u8path(mods_dir()) / fs::u8path(m.folder) / "textures";
        if (!fs::is_directory(dir, ec)) continue;
        for (auto it = fs::recursive_directory_iterator(dir, ec); !ec && it != fs::recursive_directory_iterator();
             it.increment(ec)) {
            fs::path p = it->path();
            std::string ext = p.extension().u8string();
            for (char &c : ext) c = (char)tolower((unsigned char)c);
            uint64_t image, palette;
            if (ext == ".png" && parse_name(p.stem().u8string(), image, palette)) {
                g_files[{image, palette}] = p.u8string(); /* later mods win */
            }
        }
    }
    g_dump = dump != 0;
    if (g_dump) {
        g_dump_dir = (fs::u8path(settings_data_dir()) / "texture_dump").u8string();
        fs::create_directories(fs::u8path(g_dump_dir), ec);
        printf("[mods] saving the game's textures to %s\n", g_dump_dir.c_str());
    }
    if (!g_files.empty() || g_dump) {
        LainHD_SetProvider(provide);
        if (!g_files.empty()) printf("[mods] %zu HD textures available\n", g_files.size());
    }
}
