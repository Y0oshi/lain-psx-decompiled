/* Minimal PNG writer (stored/uncompressed deflate blocks), no dependencies. */
#include "png.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static unsigned int crc_table[256];

static unsigned int crc32_update(unsigned int c, const unsigned char *p, size_t n)
{
    size_t i;
    if (!crc_table[1]) {
        unsigned int k, j;
        for (k = 0; k < 256; k++) {
            unsigned int v = k;
            for (j = 0; j < 8; j++)
                v = (v & 1) ? 0xEDB88320u ^ (v >> 1) : v >> 1;
            crc_table[k] = v;
        }
    }
    for (i = 0; i < n; i++)
        c = crc_table[(c ^ p[i]) & 0xFF] ^ (c >> 8);
    return c;
}

static void be32(unsigned char *p, unsigned int v)
{
    p[0] = (unsigned char)(v >> 24); p[1] = (unsigned char)(v >> 16);
    p[2] = (unsigned char)(v >> 8);  p[3] = (unsigned char)v;
}

static void chunk(FILE *f, const char *type, const unsigned char *data, size_t n)
{
    unsigned char b[4];
    unsigned int c;
    be32(b, (unsigned int)n);
    fwrite(b, 1, 4, f);
    fwrite(type, 1, 4, f);
    if (n)
        fwrite(data, 1, n, f);
    c = crc32_update(0xFFFFFFFFu, (const unsigned char *)type, 4);
    c = crc32_update(c, data, n) ^ 0xFFFFFFFFu;
    be32(b, c);
    fwrite(b, 1, 4, f);
}

int write_png_rgb(const char *path, const unsigned char *rgb, int w, int h)
{
    static const unsigned char sig[8] = {137, 80, 78, 71, 13, 10, 26, 10};
    size_t raw_n = (size_t)h * (1 + (size_t)w * 3), nblk = (raw_n + 65534) / 65535;
    unsigned char *raw = malloc(raw_n), *z = malloc(raw_n + nblk * 5 + 6), ihdr[13];
    size_t zi = 0, off = 0;
    unsigned int a = 1, bsum = 0;
    int y;
    FILE *f;

    if (!raw || !z)
        return 0;
    for (y = 0; y < h; y++) {
        raw[(size_t)y * (1 + w * 3)] = 0;
        memcpy(raw + (size_t)y * (1 + w * 3) + 1, rgb + (size_t)y * w * 3, (size_t)w * 3);
    }
    z[zi++] = 0x78;
    z[zi++] = 0x01;
    while (off < raw_n) {
        size_t n = raw_n - off > 65535 ? 65535 : raw_n - off, i;
        z[zi++] = off + n == raw_n;
        z[zi++] = (unsigned char)n;
        z[zi++] = (unsigned char)(n >> 8);
        z[zi++] = (unsigned char)~n;
        z[zi++] = (unsigned char)(~n >> 8);
        memcpy(z + zi, raw + off, n);
        for (i = 0; i < n; i++) {
            a = (a + raw[off + i]) % 65521;
            bsum = (bsum + a) % 65521;
        }
        zi += n;
        off += n;
    }
    be32(z + zi, (bsum << 16) | a);
    zi += 4;

    be32(ihdr, (unsigned int)w);
    be32(ihdr + 4, (unsigned int)h);
    ihdr[8] = 8; ihdr[9] = 2; ihdr[10] = ihdr[11] = ihdr[12] = 0;

    f = fopen(path, "wb");
    if (!f) {
        free(raw);
        free(z);
        return 0;
    }
    fwrite(sig, 1, 8, f);
    chunk(f, "IHDR", ihdr, 13);
    chunk(f, "IDAT", z, zi);
    chunk(f, "IEND", NULL, 0);
    fclose(f);
    free(raw);
    free(z);
    return 1;
}
