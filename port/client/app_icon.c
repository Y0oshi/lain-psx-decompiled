#include "app_icon.h"

#include "lain_icon.h"

#define ICON_SCALE 16 /* 16x16 -> 256x256, whole pixels */

void app_icon_apply(SDL_Window *win) {
    if (!win) {
        return;
    }
    const int n = 16 * ICON_SCALE;
    SDL_Surface *s = SDL_CreateRGBSurfaceWithFormat(0, n, n, 32, SDL_PIXELFORMAT_RGBA32);
    if (!s) {
        return;
    }
    for (int y = 0; y < n; y++) {
        uint32_t *row = (uint32_t *)((uint8_t *)s->pixels + (size_t)y * s->pitch);
        for (int x = 0; x < n; x++) {
            uint32_t p = lain_icon_palette[lain_icon_pixels[y / ICON_SCALE][x / ICON_SCALE]];
            uint8_t *o = (uint8_t *)&row[x];
            o[0] = (uint8_t)p;
            o[1] = (uint8_t)(p >> 8);
            o[2] = (uint8_t)(p >> 16);
            o[3] = (uint8_t)(p >> 24);
        }
    }
    SDL_SetWindowIcon(win, s);
    SDL_FreeSurface(s);
}
