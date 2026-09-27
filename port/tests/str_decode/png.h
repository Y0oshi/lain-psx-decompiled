#ifndef STR_PNG_H
#define STR_PNG_H
/* Writes 8-bit RGB pixels (w*h*3 bytes) as an uncompressed PNG. */
int write_png_rgb(const char *path, const unsigned char *rgb, int w, int h);
#endif
