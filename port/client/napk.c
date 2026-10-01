#include "napk.h"

#include <stdlib.h>
#include <string.h>

#define MAX_DIST 256
#define MIN_LEN 3
#define MAX_LEN 258

static uint32_t rd32(const uint8_t *p) {
    return p[0] | p[1] << 8 | p[2] << 16 | (uint32_t)p[3] << 24;
}

int napk_is_packed(const uint8_t *data, size_t size) {
    return size >= 8 && memcmp(data, "napk", 4) == 0;
}

/* Same steps as the game's lz_decompress, with bounds checks. */
uint8_t *napk_unpack(const uint8_t *data, size_t size, size_t *out_size) {
    if (!napk_is_packed(data, size)) {
        return NULL;
    }
    size_t remaining = rd32(data + 4);
    if (remaining > (64u << 20)) {
        return NULL;
    }
    uint8_t *out = (uint8_t *)malloc(remaining ? remaining : 1);
    if (!out) {
        return NULL;
    }
    size_t n = 0, p = 8;
    while (p < size && remaining > 0) {
        uint8_t flags = data[p++];
        for (int bit = 0; bit < 8 && p < size && remaining > 0; bit++) {
            if (flags & (0x80 >> bit)) {
                if (p + 2 > size) {
                    free(out);
                    return NULL;
                }
                size_t back = data[p] + 1u, len = data[p + 1] + 3u;
                p += 2;
                if (back > n || len > remaining) {
                    free(out);
                    return NULL;
                }
                for (size_t k = 0; k < len; k++, n++) {
                    out[n] = out[n - back];
                }
                remaining -= len;
            } else {
                out[n++] = data[p++];
                remaining--;
            }
        }
    }
    if (remaining != 0) {
        free(out);
        return NULL;
    }
    *out_size = n;
    return out;
}

uint8_t *napk_pack(const uint8_t *data, size_t size, size_t *out_size) {
    /* Longest match at each position (distance 1-256), from the end backward:
     * run[d] is the match length at distance d for the position after this one. */
    uint16_t *best_len = (uint16_t *)calloc(size + 1, sizeof(uint16_t));
    uint16_t *best_dist = (uint16_t *)calloc(size + 1, sizeof(uint16_t));
    uint32_t *cost = (uint32_t *)malloc((size + 1) * sizeof(uint32_t));
    uint16_t *choice = (uint16_t *)calloc(size + 1, sizeof(uint16_t));
    uint16_t run[MAX_DIST + 1];
    memset(run, 0, sizeof run);
    if (!best_len || !best_dist || !cost || !choice) {
        free(best_len);
        free(best_dist);
        free(cost);
        free(choice);
        return NULL;
    }
    for (size_t i = size; i-- > 0;) {
        for (int d = 1; d <= MAX_DIST; d++) {
            if ((size_t)d > i || data[i] != data[i - d]) {
                run[d] = 0;
                continue;
            }
            size_t l = (size_t)run[d] + 1;
            run[d] = (uint16_t)(l > MAX_LEN ? MAX_LEN : l);
            /* run[d] can't pass the end: it grows by one per byte from the end */
            if (run[d] > best_len[i]) {
                best_len[i] = run[d];
                best_dist[i] = (uint16_t)d;
            }
        }
    }
    /* Cheapest encoding in bits: a literal is 9 (byte + flag), a match 17. */
    cost[size] = 0;
    for (size_t i = size; i-- > 0;) {
        cost[i] = cost[i + 1] + 9;
        choice[i] = 1;
        for (size_t l = MIN_LEN; l <= best_len[i]; l++) {
            uint32_t c = cost[i + l] + 17;
            if (c < cost[i]) {
                cost[i] = c;
                choice[i] = (uint16_t)l;
            }
        }
    }
    size_t cap = 8 + size + size / 8 + 2;
    uint8_t *out = (uint8_t *)malloc(cap);
    if (!out) {
        free(best_len);
        free(best_dist);
        free(cost);
        free(choice);
        return NULL;
    }
    memcpy(out, "napk", 4);
    out[4] = (uint8_t)size;
    out[5] = (uint8_t)(size >> 8);
    out[6] = (uint8_t)(size >> 16);
    out[7] = (uint8_t)(size >> 24);
    size_t o = 8, flag_pos = 0;
    int bit = 8;
    for (size_t i = 0; i < size;) {
        if (bit == 8) {
            flag_pos = o++;
            out[flag_pos] = 0;
            bit = 0;
        }
        size_t l = choice[i];
        if (l >= MIN_LEN) {
            out[flag_pos] |= (uint8_t)(0x80 >> bit);
            out[o++] = (uint8_t)(best_dist[i] - 1);
            out[o++] = (uint8_t)(l - MIN_LEN);
            i += l;
        } else {
            out[o++] = data[i++];
        }
        bit++;
    }
    free(best_len);
    free(best_dist);
    free(cost);
    free(choice);
    *out_size = o;
    return out;
}
