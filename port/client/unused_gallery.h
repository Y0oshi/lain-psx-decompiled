/* The launcher's "Unused" tab: pictures and animations on the discs that the game
 * never shows, decoded from the player's own disc images when the tab is opened.
 * Nothing of the game is stored in this program; see docs/findings/README.md. */
#pragma once

#include <cstdint>
#include <string>
#include <vector>

struct GalleryItem {
    std::string title;    // short name
    std::string caption;  // what it is and why it's unused
    int w = 0, h = 0;
    std::vector<std::vector<uint32_t>> frames;  // RGBA8 (R in the low byte), w*h each
    float fps = 0;        // > 0 for animations
    // Sounds (no frames): 16-bit PCM, interleaved when stereo.
    std::vector<int16_t> pcm;
    int rate = 0, channels = 1;
};

struct GallerySection {
    std::string title, intro;
    std::vector<GalleryItem> items;
};

/* Decodes everything from the imported disc images (disc2 may be NULL: the items
 * from Site B are then left out). Returns false with `err` if disc 1 can't be read. */
bool gallery_load(const char *disc1_bin, const char *disc2_bin, std::vector<GallerySection> &out, std::string &err);
