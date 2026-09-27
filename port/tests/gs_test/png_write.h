#ifndef GS_TEST_PNG_WRITE_H
#define GS_TEST_PNG_WRITE_H

/* rgb: w*h*3 bytes, top row first. Returns 0 on success. */
int png_write_rgb(const char *path, const unsigned char *rgb, int w, int h);

#endif
