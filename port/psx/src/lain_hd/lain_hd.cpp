/* lain: HD texture replacement: pictures in VRAM and their replacements. */
#include "lain_hd.h"

#include <string.h>

#include <unordered_map>
#include <vector>

extern unsigned short vram[]; /* PsyX_render.cpp: 1024 x 512 */

namespace {

struct Picture {
    int x, y, w, h;
    uint64_t hash;
};

LainHDProvider g_provider;
std::vector<Picture> g_pictures;
int g_last_hit = -1;

struct KeyHash {
    size_t operator()(const std::pair<uint64_t, uint64_t> &k) const {
        return (size_t)(k.first * 0x9E3779B97F4A7C15ull ^ k.second);
    }
};
std::unordered_map<std::pair<uint64_t, uint64_t>, unsigned int, KeyHash> g_textures; /* 0 = none */

uint64_t fnv(uint64_t h, const uint16_t *p, int n) {
    for (int i = 0; i < n; i++) {
        h ^= p[i] & 0xFF;
        h *= 0x100000001B3ull;
        h ^= p[i] >> 8;
        h *= 0x100000001B3ull;
    }
    return h;
}

bool overlaps(const Picture &p, int x, int y, int w, int h) {
    return x < p.x + p.w && p.x < x + w && y < p.y + p.h && p.y < y + h;
}

void drop(int x, int y, int w, int h) {
    for (size_t i = 0; i < g_pictures.size();) {
        if (overlaps(g_pictures[i], x, y, w, h)) {
            g_pictures[i] = g_pictures.back();
            g_pictures.pop_back();
        } else {
            i++;
        }
    }
    g_last_hit = -1;
}

} // namespace

extern "C" void LainHD_SetProvider(LainHDProvider provider) {
    g_provider = provider;
    g_textures.clear();
}

extern "C" void LainHD_NoteWrite(int x, int y, int w, int h) {
    if (g_provider) drop(x, y, w, h);
}

extern "C" void LainHD_NoteUpload(int x, int y, int w, int h) {
    if (!g_provider) return;
    drop(x, y, w, h);
    /* palettes (one row) and the display buffers are not pictures */
    if (h < 2 || w < 1 || (x < 320 && y < 480) || x + w > 1024 || y + h > 512) return;
    uint64_t hash = 0xCBF29CE484222325ull ^ ((uint64_t)w << 32 | (uint64_t)h);
    for (int r = 0; r < h; r++) hash = fnv(hash, vram + (y + r) * 1024 + x, w);
    g_pictures.push_back(Picture{x, y, w, h, hash});
}

extern "C" void LainHD_Match(int tpage, int clut, int umin, int vmin, int umax, int vmax, LainHDMatch *out) {
    out->texture = 0;
    if (!g_provider || g_pictures.empty()) return;
    const int format = (tpage >> 7) & 3;
    if (format == 3) return;
    const int ppw = format == 0 ? 4 : format == 1 ? 2 : 1; /* texels per word */
    const int page_x = (tpage & 15) * 64, page_y = ((tpage >> 4) & 1) * 256;
    const int x0 = page_x + umin / ppw, x1 = page_x + umax / ppw, y0 = page_y + vmin, y1 = page_y + vmax;
    int hit = -1;
    auto contains = [&](const Picture &p) { return x0 >= p.x && x1 < p.x + p.w && y0 >= p.y && y1 < p.y + p.h; };
    if (g_last_hit >= 0 && g_last_hit < (int)g_pictures.size() && contains(g_pictures[g_last_hit])) {
        hit = g_last_hit;
    } else {
        for (int i = 0; i < (int)g_pictures.size() && hit < 0; i++) {
            if (contains(g_pictures[i])) hit = i;
        }
    }
    if (hit < 0) return;
    g_last_hit = hit;
    const Picture &p = g_pictures[hit];
    LainHDKey key;
    memset(&key, 0, sizeof key);
    key.image = p.hash;
    key.bpp = format == 0 ? 4 : format == 1 ? 8 : 16;
    if (format != 2) {
        key.clut_x = (clut & 0x3F) * 16;
        key.clut_y = (clut >> 6) & 0x1FF;
        const int n = format == 0 ? 16 : 256;
        if (key.clut_x + n > 1024) return;
        key.palette = fnv(0xCBF29CE484222325ull ^ (uint64_t)n, vram + key.clut_y * 1024 + key.clut_x, n) | 1;
    }
    auto it = g_textures.find({key.image, key.palette});
    unsigned int tex;
    if (it != g_textures.end()) {
        tex = it->second;
    } else {
        key.x = p.x;
        key.y = p.y;
        key.w = p.w;
        key.h = p.h;
        key.width = p.w * ppw;
        key.vram = vram;
        tex = g_provider(&key);
        g_textures[{key.image, key.palette}] = tex;
    }
    if (!tex) return;
    out->texture = tex;
    out->origin_x = (float)((p.x - page_x) * ppw);
    out->origin_y = (float)(p.y - page_y);
    out->inv_w = 1.0f / (float)(p.w * ppw);
    out->inv_h = 1.0f / (float)p.h;
}
