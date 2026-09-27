/* Minimal PNG writer (RGB, 8-bit, uncompressed deflate blocks). */
#include "png_write.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static unsigned long crc_table[256];

static void crc_init(void) {
    unsigned long c;
    int n, k;

    for (n = 0; n < 256; n++) {
        c = (unsigned long)n;
        for (k = 0; k < 8; k++) {
            c = (c & 1) ? 0xEDB88320UL ^ (c >> 1) : c >> 1;
        }
        crc_table[n] = c;
    }
}

static unsigned long crc_update(unsigned long crc, const unsigned char *buf, size_t len) {
    size_t i;

    for (i = 0; i < len; i++) {
        crc = crc_table[(crc ^ buf[i]) & 0xFF] ^ (crc >> 8);
    }
    return crc;
}

static void put32(unsigned char *p, unsigned long v) {
    p[0] = (unsigned char)(v >> 24);
    p[1] = (unsigned char)(v >> 16);
    p[2] = (unsigned char)(v >> 8);
    p[3] = (unsigned char)v;
}

static void chunk(FILE *f, const char *type, const unsigned char *data, size_t len) {
    unsigned char hdr[8];
    unsigned char tail[4];
    unsigned long crc;

    put32(hdr, (unsigned long)len);
    memcpy(hdr + 4, type, 4);
    fwrite(hdr, 1, 8, f);
    if (len) {
        fwrite(data, 1, len, f);
    }
    crc = crc_update(0xFFFFFFFFUL, (const unsigned char *)type, 4);
    crc = crc_update(crc, data, len) ^ 0xFFFFFFFFUL;
    put32(tail, crc);
    fwrite(tail, 1, 4, f);
}

int png_write_rgb(const char *path, const unsigned char *rgb, int w, int h) {
    size_t raw_len = (size_t)(w * 3 + 1) * h;
    size_t nblocks = (raw_len + 65534) / 65535;
    size_t z_len = 2 + raw_len + nblocks * 5 + 4;
    unsigned char *raw = malloc(raw_len);
    unsigned char *z = malloc(z_len);
    unsigned char ihdr[13];
    unsigned long a = 1, b = 0;
    size_t i, pos, zp;
    FILE *f;
    int y;

    if (!raw || !z) {
        free(raw);
        free(z);
        return -1;
    }
    crc_init();
    for (y = 0; y < h; y++) {
        raw[(size_t)y * (w * 3 + 1)] = 0;
        memcpy(raw + (size_t)y * (w * 3 + 1) + 1, rgb + (size_t)y * w * 3, (size_t)w * 3);
    }
    zp = 0;
    z[zp++] = 0x78;
    z[zp++] = 0x01;
    for (pos = 0; pos < raw_len;) {
        size_t n = raw_len - pos > 65535 ? 65535 : raw_len - pos;
        z[zp++] = (pos + n == raw_len) ? 1 : 0;
        z[zp++] = (unsigned char)(n & 0xFF);
        z[zp++] = (unsigned char)(n >> 8);
        z[zp++] = (unsigned char)(~n & 0xFF);
        z[zp++] = (unsigned char)((~n >> 8) & 0xFF);
        memcpy(z + zp, raw + pos, n);
        zp += n;
        pos += n;
    }
    for (i = 0; i < raw_len; i++) {
        a = (a + raw[i]) % 65521;
        b = (b + a) % 65521;
    }
    put32(z + zp, (b << 16) | a);
    zp += 4;

    f = fopen(path, "wb");
    if (!f) {
        free(raw);
        free(z);
        return -1;
    }
    fwrite("\x89PNG\r\n\x1a\n", 1, 8, f);
    put32(ihdr, (unsigned long)w);
    put32(ihdr + 4, (unsigned long)h);
    ihdr[8] = 8;  /* bit depth */
    ihdr[9] = 2;  /* RGB */
    ihdr[10] = 0;
    ihdr[11] = 0;
    ihdr[12] = 0;
    chunk(f, "IHDR", ihdr, 13);
    chunk(f, "IDAT", z, zp);
    chunk(f, "IEND", NULL, 0);
    fclose(f);
    free(raw);
    free(z);
    return 0;
}
