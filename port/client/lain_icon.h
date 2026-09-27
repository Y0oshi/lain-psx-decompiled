/* The client icon: frame 0 of the game's memory card icon (g_mcard_header_template
 * at 0x8009D490), with a mouth and a lighter nose. 16x16 palette indices; palette
 * entries are RGBA8 packed as 0xAABBGGRR. Entry 0 is transparent. Entry 15 is 0x0000 in
 * the game too (transparent on the console, over the dark card screen); here it is the
 * black outline and pupils it stands for. */
#pragma once

#include <stdint.h>

static const uint32_t lain_icon_palette[17] = {
    0x00000000, 0xFF395A8B, 0xFF839CC5, 0xFFB4C5E6, 0xFF000041, 0xFF00006A,
    0xFF525AD5, 0xFF8B5A39, 0xFFC59C83, 0xFFE6C5B4, 0xFFF600F6, 0xFF940000,
    0xFFEEC5AC, 0xFFF6F6EE, 0xFFEE7300, 0xFF000000, 0xFF4E50B4,
};

static const uint8_t lain_icon_pixels[16][16] = {
    { 0,  0,  0,  0,  0,  0, 15, 15, 15, 15,  0,  0,  0,  0,  0,  0},
    { 0,  0,  0, 15, 15,  4,  4,  5,  5,  4,  4, 15, 15,  0,  0,  0},
    { 0,  0, 15,  4,  4,  5,  5,  5,  5,  5,  5,  4,  4, 15,  0,  0},
    { 0, 15,  4,  5,  5,  5,  5,  5,  5,  5,  5,  5,  5,  4, 15,  0},
    { 0, 15,  5,  6,  6,  5,  5,  6,  5,  6,  5,  6,  5,  5, 15,  0},
    { 0, 15,  6,  3,  5,  5,  5,  3,  6,  3,  3,  5,  3,  5, 15,  0},
    { 0,  4,  6,  5,  1,  2,  5,  5,  6,  5,  6,  5,  6,  5,  4,  0},
    {15,  4,  5,  4, 15, 15,  1,  5,  1,  5, 15, 15,  5,  5,  4, 15},
    {15,  5,  5,  4,  3,  3,  3,  5,  3,  5,  5,  3,  4,  5,  5, 15},
    {15,  5,  5,  1, 11, 15,  1,  3,  3, 11, 15,  1,  1,  5,  5, 15},
    {15,  5,  5,  1, 15, 15, 13,  3,  3, 15, 15, 13,  1,  4, 14, 15},
    {15,  5,  5,  1, 11, 15, 12,  3,  3, 11, 15, 12,  1, 11, 14, 15},
    {15,  4,  5,  1,  3,  3,  3,  3,  2,  3,  3,  3,  1,  5,  6, 15},
    { 0, 15,  5, 15,  1,  2,  3, 16, 16,  3,  2,  1, 15,  5,  6, 15},
    { 0,  0, 15,  0, 15,  4,  1,  2,  2,  1,  4, 15,  0,  4,  5, 15},
    { 0,  0,  0,  0,  0,  0, 15, 15, 15, 15,  0,  0,  0, 15,  5,  0},
};
