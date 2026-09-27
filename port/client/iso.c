#include "iso.h"

#include <stdlib.h>
#include <string.h>

#define RAW_SECTOR 2352

static uint32_t le32(const uint8_t *p) {
    return p[0] | p[1] << 8 | p[2] << 16 | (uint32_t)p[3] << 24;
}

int iso_read_sector(FILE *img, uint32_t lba, uint8_t out[2048]) {
    uint8_t raw[RAW_SECTOR];
    if (fseek(img, (long)lba * RAW_SECTOR, SEEK_SET) != 0 || fread(raw, 1, RAW_SECTOR, img) != RAW_SECTOR) {
        return -1;
    }
    memcpy(out, raw + (raw[15] == 1 ? 16 : 24), 2048);
    return 0;
}

int iso_find_root_file(FILE *img, const char *name, uint32_t *lba, uint32_t *size) {
    uint8_t sec[2048];
    if (iso_read_sector(img, 16, sec) != 0 || memcmp(sec + 1, "CD001", 5) != 0) {
        return -1;
    }
    uint32_t root_lba = le32(sec + 156 + 2), root_size = le32(sec + 156 + 10);
    size_t name_len = strlen(name);
    for (uint32_t s = 0; s < (root_size + 2047) / 2048; s++) {
        if (iso_read_sector(img, root_lba + s, sec) != 0) {
            return -1;
        }
        for (int pos = 0; pos < 2048 && sec[pos];) {
            const uint8_t *rec = sec + pos;
            int len = rec[32];
            /* Names are stored as "NAME.EXT;1". */
            if ((size_t)len >= name_len && memcmp(rec + 33, name, name_len) == 0 &&
                ((size_t)len == name_len || rec[33 + name_len] == ';')) {
                *lba = le32(rec + 2);
                *size = le32(rec + 10);
                return 0;
            }
            pos += rec[0];
        }
    }
    return -1;
}

uint8_t *iso_read_file(FILE *img, uint32_t lba, uint32_t size) {
    uint8_t *buf = malloc(size + 2048);
    if (!buf) {
        return NULL;
    }
    for (uint32_t off = 0; off < size; off += 2048) {
        if (iso_read_sector(img, lba + off / 2048, buf + off) != 0) {
            free(buf);
            return NULL;
        }
    }
    return buf;
}

/* Locates a directory's extent by walking the path from the root. */
static int find_dir(FILE *img, const char *path, uint32_t *lba, uint32_t *size) {
    uint8_t sec[2048];
    if (iso_read_sector(img, 16, sec) != 0 || memcmp(sec + 1, "CD001", 5) != 0) {
        return -1;
    }
    *lba = le32(sec + 156 + 2);
    *size = le32(sec + 156 + 10);
    while (*path) {
        const char *slash = strchr(path, '/');
        size_t n = slash ? (size_t)(slash - path) : strlen(path);
        int found = 0;
        for (uint32_t s = 0; s < (*size + 2047) / 2048 && !found; s++) {
            if (iso_read_sector(img, *lba + s, sec) != 0) {
                return -1;
            }
            for (int pos = 0; pos < 2048 && sec[pos];) {
                const uint8_t *rec = sec + pos;
                if ((rec[25] & 2) && rec[32] == n && memcmp(rec + 33, path, n) == 0) {
                    *lba = le32(rec + 2);
                    *size = le32(rec + 10);
                    found = 1;
                    break;
                }
                pos += rec[0];
            }
        }
        if (!found) {
            return -1;
        }
        path += n + (slash ? 1 : 0);
    }
    return 0;
}

int iso_list_dir(FILE *img, const char *path, IsoDirFn fn, void *user) {
    uint32_t lba, size;
    uint8_t sec[2048];
    if (find_dir(img, path, &lba, &size) != 0) {
        return -1;
    }
    for (uint32_t s = 0; s < (size + 2047) / 2048; s++) {
        if (iso_read_sector(img, lba + s, sec) != 0) {
            return -1;
        }
        for (int pos = 0; pos < 2048 && sec[pos];) {
            const uint8_t *rec = sec + pos;
            int len = rec[32];
            if (!(rec[25] & 2) && len > 0 && !(len == 1 && rec[33] <= 1)) {
                char name[64];
                int n = 0;
                while (n < len && n < 63 && rec[33 + n] != ';') {
                    name[n] = (char)rec[33 + n];
                    n++;
                }
                name[n] = 0;
                fn(name, le32(rec + 2), le32(rec + 10), user);
            }
            pos += rec[0];
        }
    }
    return 0;
}
