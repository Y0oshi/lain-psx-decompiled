#include "png.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const uint8_t sig[8] = {137, 80, 78, 71, 13, 10, 26, 10}; /* the PNG signature */

/* CRC-32 and Adler-32 */

static uint32_t crc_table[256];

static uint32_t crc32_update(uint32_t c, const uint8_t *p, size_t n) {
    if (!crc_table[1]) {
        for (uint32_t k = 0; k < 256; k++) {
            uint32_t v = k;
            for (int j = 0; j < 8; j++) {
                v = (v & 1) ? 0xEDB88320u ^ (v >> 1) : v >> 1;
            }
            crc_table[k] = v;
        }
    }
    for (size_t i = 0; i < n; i++) {
        c = crc_table[(c ^ p[i]) & 0xFF] ^ (c >> 8);
    }
    return c;
}

static uint32_t adler32(const uint8_t *p, size_t n) {
    uint32_t a = 1, b = 0;
    while (n > 0) {
        size_t k = n < 5552 ? n : 5552;
        n -= k;
        while (k--) {
            a += *p++;
            b += a;
        }
        a %= 65521;
        b %= 65521;
    }
    return b << 16 | a;
}

static uint32_t rd_be32(const uint8_t *p) {
    return (uint32_t)p[0] << 24 | (uint32_t)p[1] << 16 | (uint32_t)p[2] << 8 | p[3];
}

static void wr_be32(uint8_t *p, uint32_t v) {
    p[0] = (uint8_t)(v >> 24);
    p[1] = (uint8_t)(v >> 16);
    p[2] = (uint8_t)(v >> 8);
    p[3] = (uint8_t)v;
}

/* Inflate (RFC 1951) */

typedef struct {
    const uint8_t *src;
    size_t src_size, pos;
    uint32_t bitbuf;
    int bitcnt;
    uint8_t *out;
    size_t out_size, out_pos;
    int error;
} Inflate;

typedef struct {
    uint16_t count[16];  /* codes of each length */
    uint16_t symbol[288];
} Huffman;

static int bits(Inflate *s, int need) {
    uint32_t v = s->bitbuf;
    while (s->bitcnt < need) {
        if (s->pos >= s->src_size) {
            s->error = 1;
            return 0;
        }
        v |= (uint32_t)s->src[s->pos++] << s->bitcnt;
        s->bitcnt += 8;
    }
    s->bitbuf = v >> need;
    s->bitcnt -= need;
    return (int)(v & ((1u << need) - 1));
}

/* Canonical Huffman decoding, one bit at a time (as in zlib's puff). */
static int decode(Inflate *s, const Huffman *h) {
    int code = 0, first = 0, index = 0;
    for (int len = 1; len < 16; len++) {
        code |= bits(s, 1);
        if (s->error) {
            return -1;
        }
        int count = h->count[len];
        if (code - count < first) {
            return h->symbol[index + (code - first)];
        }
        index += count;
        first += count;
        first <<= 1;
        code <<= 1;
    }
    s->error = 1;
    return -1;
}

/* Builds a table from code lengths; 0 on success. Incomplete codes are allowed
 * (a single distance code is legal). */
static int build(Huffman *h, const uint8_t *length, int n) {
    uint16_t offs[16];
    memset(h->count, 0, sizeof h->count);
    for (int i = 0; i < n; i++) {
        h->count[length[i]]++;
    }
    if (h->count[0] == n) {
        return 0;
    }
    int left = 1;
    for (int len = 1; len < 16; len++) {
        left <<= 1;
        left -= h->count[len];
        if (left < 0) {
            return -1; /* over-subscribed */
        }
    }
    offs[1] = 0;
    for (int len = 1; len < 15; len++) {
        offs[len + 1] = offs[len] + h->count[len];
    }
    for (int i = 0; i < n; i++) {
        if (length[i]) {
            h->symbol[offs[length[i]]++] = (uint16_t)i;
        }
    }
    return 0;
}

static const uint16_t len_base[29] = {3,  4,  5,  6,  7,  8,  9,  10, 11,  13,  15,  17,  19,  23, 27,
                                      31, 35, 43, 51, 59, 67, 83, 99, 115, 131, 163, 195, 227, 258};
static const uint8_t len_extra[29] = {0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2,
                                      2, 3, 3, 3, 3, 4, 4, 4, 4, 5, 5, 5, 5, 0};
static const uint16_t dist_base[30] = {1,   2,   3,   4,   5,   7,    9,    13,   17,   25,   33,   49,   65,    97,    129,
                                       193, 257, 385, 513, 769, 1025, 1537, 2049, 3073, 4097, 6145, 8193, 12289, 16385, 24577};
static const uint8_t dist_extra[30] = {0, 0, 0, 0, 1, 1, 2, 2,  3,  3,  4,  4,  5,  5,  6,
                                       6, 7, 7, 8, 8, 9, 9, 10, 10, 11, 11, 12, 12, 13, 13};

static int codes(Inflate *s, const Huffman *lit, const Huffman *dist) {
    for (;;) {
        int sym = decode(s, lit);
        if (sym < 0) {
            return -1;
        }
        if (sym < 256) {
            if (s->out_pos >= s->out_size) {
                return -1;
            }
            s->out[s->out_pos++] = (uint8_t)sym;
        } else if (sym == 256) {
            return 0;
        } else {
            sym -= 257;
            if (sym >= 29) {
                return -1;
            }
            size_t len = len_base[sym] + bits(s, len_extra[sym]);
            int dsym = decode(s, dist);
            if (dsym < 0 || dsym >= 30) {
                return -1;
            }
            size_t d = dist_base[dsym] + bits(s, dist_extra[dsym]);
            if (s->error || d > s->out_pos || s->out_pos + len > s->out_size) {
                return -1;
            }
            for (size_t k = 0; k < len; k++, s->out_pos++) {
                s->out[s->out_pos] = s->out[s->out_pos - d];
            }
        }
    }
}

static int block_stored(Inflate *s) {
    s->bitbuf = 0;
    s->bitcnt = 0;
    if (s->pos + 4 > s->src_size) {
        return -1;
    }
    size_t len = s->src[s->pos] | s->src[s->pos + 1] << 8;
    size_t nlen = s->src[s->pos + 2] | s->src[s->pos + 3] << 8;
    s->pos += 4;
    if (len != (~nlen & 0xFFFF) || s->pos + len > s->src_size || s->out_pos + len > s->out_size) {
        return -1;
    }
    memcpy(s->out + s->out_pos, s->src + s->pos, len);
    s->pos += len;
    s->out_pos += len;
    return 0;
}

static int block_fixed(Inflate *s) {
    static Huffman lit, dist;
    static int built;
    if (!built) {
        uint8_t l[288];
        int i = 0;
        for (; i < 144; i++) l[i] = 8;
        for (; i < 256; i++) l[i] = 9;
        for (; i < 280; i++) l[i] = 7;
        for (; i < 288; i++) l[i] = 8;
        build(&lit, l, 288);
        for (i = 0; i < 30; i++) l[i] = 5;
        build(&dist, l, 30);
        built = 1;
    }
    return codes(s, &lit, &dist);
}

static int block_dynamic(Inflate *s) {
    static const uint8_t order[19] = {16, 17, 18, 0, 8, 7, 9, 6, 10, 5, 11, 4, 12, 3, 13, 2, 14, 1, 15};
    uint8_t lengths[320];
    Huffman lencode, lit, dist;
    int nlen = bits(s, 5) + 257, ndist = bits(s, 5) + 1, ncode = bits(s, 4) + 4;
    if (s->error || nlen > 286 || ndist > 30) {
        return -1;
    }
    memset(lengths, 0, sizeof lengths);
    for (int i = 0; i < ncode; i++) {
        lengths[order[i]] = (uint8_t)bits(s, 3);
    }
    if (s->error || build(&lencode, lengths, 19) != 0) {
        return -1;
    }
    int index = 0;
    while (index < nlen + ndist) {
        int sym = decode(s, &lencode);
        if (sym < 0) {
            return -1;
        }
        if (sym < 16) {
            lengths[index++] = (uint8_t)sym;
            continue;
        }
        int len = 0, rep;
        if (sym == 16) {
            if (index == 0) {
                return -1;
            }
            len = lengths[index - 1];
            rep = 3 + bits(s, 2);
        } else if (sym == 17) {
            rep = 3 + bits(s, 3);
        } else {
            rep = 11 + bits(s, 7);
        }
        if (s->error || index + rep > nlen + ndist) {
            return -1;
        }
        while (rep--) {
            lengths[index++] = (uint8_t)len;
        }
    }
    if (lengths[256] == 0 || build(&lit, lengths, nlen) != 0 || build(&dist, lengths + nlen, ndist) != 0) {
        return -1;
    }
    return codes(s, &lit, &dist);
}

/* Inflates a zlib stream into out (out_size bytes expected). Returns bytes written or -1. */
static long zlib_inflate(const uint8_t *src, size_t src_size, uint8_t *out, size_t out_size) {
    if (src_size < 2 || (src[0] & 0x0F) != 8 || ((src[0] << 8) | src[1]) % 31 != 0 || (src[1] & 0x20)) {
        return -1;
    }
    Inflate s = {src, src_size, 2, 0, 0, out, out_size, 0, 0};
    int last;
    do {
        last = bits(&s, 1);
        int type = bits(&s, 2);
        if (s.error) {
            return -1;
        }
        int r = type == 0 ? block_stored(&s) : type == 1 ? block_fixed(&s) : type == 2 ? block_dynamic(&s) : -1;
        if (r != 0 || s.error) {
            return -1;
        }
    } while (!last);
    return (long)s.out_pos;
}

/* Deflate: LZ77 with hash chains, fixed Huffman codes */

typedef struct {
    uint8_t *buf;
    size_t size, cap;
    uint32_t bitbuf;
    int bitcnt;
} BitOut;

static void put_byte(BitOut *o, uint8_t b) {
    if (o->size == o->cap) {
        o->cap = o->cap ? o->cap * 2 : 4096;
        o->buf = (uint8_t *)realloc(o->buf, o->cap);
        if (!o->buf) abort(); /* out of memory */
    }
    o->buf[o->size++] = b;
}

static void put_bits(BitOut *o, uint32_t v, int n) {
    o->bitbuf |= v << o->bitcnt;
    o->bitcnt += n;
    while (o->bitcnt >= 8) {
        put_byte(o, (uint8_t)o->bitbuf);
        o->bitbuf >>= 8;
        o->bitcnt -= 8;
    }
}

/* Huffman codes go out most significant bit first. */
static void put_code(BitOut *o, uint32_t code, int n) {
    uint32_t r = 0;
    for (int i = 0; i < n; i++) {
        r |= ((code >> i) & 1) << (n - 1 - i);
    }
    put_bits(o, r, n);
}

static void put_lit(BitOut *o, int sym) {
    if (sym < 144) put_code(o, 0x30 + sym, 8);
    else if (sym < 256) put_code(o, 0x190 + sym - 144, 9);
    else if (sym < 280) put_code(o, sym - 256, 7);
    else put_code(o, 0xC0 + sym - 280, 8);
}

static void put_match(BitOut *o, int len, int dist) {
    int i = 28;
    while (len_base[i] > len) i--;
    put_lit(o, 257 + i);
    put_bits(o, (uint32_t)(len - len_base[i]), len_extra[i]);
    int d = 29;
    while (dist_base[d] > dist) d--;
    put_code(o, (uint32_t)d, 5);
    put_bits(o, (uint32_t)(dist - dist_base[d]), dist_extra[d]);
}

#define HASH_BITS 15
#define WINDOW 32768
#define MAX_CHAIN 64

static uint32_t hash3(const uint8_t *p) {
    return ((uint32_t)p[0] << 10 ^ (uint32_t)p[1] << 5 ^ p[2]) & ((1u << HASH_BITS) - 1);
}

/* zlib stream of `data`, compressed (malloc'd; size in *out_size). */
static uint8_t *zlib_compress(const uint8_t *data, size_t size, size_t *out_size) {
    BitOut o = {0};
    put_byte(&o, 0x78);
    put_byte(&o, 0x9C);
    int32_t *head = (int32_t *)malloc(sizeof(int32_t) << HASH_BITS);
    int32_t *prev = (int32_t *)malloc(sizeof(int32_t) * (size ? size : 1));
    if (!head || !prev) {
        free(head);
        free(prev);
        free(o.buf);
        return NULL;
    }
    for (size_t i = 0; i < ((size_t)1 << HASH_BITS); i++) head[i] = -1;
    put_bits(&o, 1, 1); /* final block */
    put_bits(&o, 1, 2); /* fixed Huffman */
    size_t i = 0;
    while (i < size) {
        int best_len = 0, best_dist = 0;
        if (i + 3 <= size) {
            uint32_t h = hash3(data + i);
            int32_t cand = head[h];
            int chain = MAX_CHAIN;
            size_t max_len = size - i < 258 ? size - i : 258;
            while (cand >= 0 && i - (size_t)cand <= WINDOW && chain--) {
                size_t l = 0;
                while (l < max_len && data[cand + l] == data[i + l]) l++;
                if ((int)l > best_len) {
                    best_len = (int)l;
                    best_dist = (int)(i - (size_t)cand);
                    if (l == max_len) break;
                }
                cand = prev[cand];
            }
            prev[i] = head[h];
            head[h] = (int32_t)i;
        }
        if (best_len >= 3) {
            put_match(&o, best_len, best_dist);
            /* index the skipped positions too */
            for (size_t k = i + 1; k < i + (size_t)best_len && k + 3 <= size; k++) {
                uint32_t h = hash3(data + k);
                prev[k] = head[h];
                head[h] = (int32_t)k;
            }
            i += (size_t)best_len;
        } else {
            put_lit(&o, data[i]);
            i++;
        }
    }
    put_lit(&o, 256);
    if (o.bitcnt > 0) {
        put_bits(&o, 0, 8 - o.bitcnt);
    }
    uint32_t a = adler32(data, size);
    put_byte(&o, (uint8_t)(a >> 24));
    put_byte(&o, (uint8_t)(a >> 16));
    put_byte(&o, (uint8_t)(a >> 8));
    put_byte(&o, (uint8_t)a);
    free(head);
    free(prev);
    *out_size = o.size;
    return o.buf;
}

/* PNG */

static void set_err(char *err, size_t cap, const char *msg) {
    if (err && cap) {
        snprintf(err, cap, "%s", msg);
    }
}

static int paeth(int a, int b, int c) {
    int p = a + b - c, pa = abs(p - a), pb = abs(p - b), pc = abs(p - c);
    return pa <= pb && pa <= pc ? a : pb <= pc ? b : c;
}

/* Undoes the filters of one pass in place; bpp is bytes per pixel (at least 1). */
static int unfilter(uint8_t *p, size_t rowbytes, int rows, int bpp) {
    uint8_t *prev = NULL;
    for (int y = 0; y < rows; y++) {
        uint8_t type = p[0], *row = p + 1;
        for (size_t x = 0; x < rowbytes; x++) {
            int a = x >= (size_t)bpp ? row[x - bpp] : 0;
            int b = prev ? prev[x] : 0;
            int c = prev && x >= (size_t)bpp ? prev[x - bpp] : 0;
            switch (type) {
            case 0: break;
            case 1: row[x] = (uint8_t)(row[x] + a); break;
            case 2: row[x] = (uint8_t)(row[x] + b); break;
            case 3: row[x] = (uint8_t)(row[x] + ((a + b) >> 1)); break;
            case 4: row[x] = (uint8_t)(row[x] + paeth(a, b, c)); break;
            default: return -1;
            }
        }
        prev = row;
        p += rowbytes + 1;
    }
    return 0;
}

typedef struct {
    int depth, ctype, channels;
    uint8_t palette[256][4];
    int npal;
    int has_trns;
    uint16_t trns[3]; /* gray or RGB key, raw sample values */
} Format;

static unsigned sample(const uint8_t *row, int depth, size_t idx) {
    if (depth == 16) return (unsigned)(row[idx * 2] << 8 | row[idx * 2 + 1]);
    if (depth == 8) return row[idx];
    size_t bit = idx * (size_t)depth;
    return (unsigned)(row[bit / 8] >> (8 - depth - bit % 8)) & ((1u << depth) - 1);
}

static uint8_t to8(unsigned v, int depth) {
    if (depth == 16) return (uint8_t)(v >> 8);
    if (depth == 8) return (uint8_t)v;
    return (uint8_t)(v * 255 / ((1u << depth) - 1));
}

static void put_pixel(const Format *f, const uint8_t *row, size_t x, uint8_t *dst) {
    const int d = f->depth;
    size_t s = x * (size_t)f->channels;
    switch (f->ctype) {
    case 0: {
        unsigned g = sample(row, d, s);
        dst[0] = dst[1] = dst[2] = to8(g, d);
        dst[3] = f->has_trns && g == f->trns[0] ? 0 : 255;
        break;
    }
    case 2: {
        unsigned r = sample(row, d, s), g = sample(row, d, s + 1), b = sample(row, d, s + 2);
        dst[0] = to8(r, d);
        dst[1] = to8(g, d);
        dst[2] = to8(b, d);
        dst[3] = f->has_trns && r == f->trns[0] && g == f->trns[1] && b == f->trns[2] ? 0 : 255;
        break;
    }
    case 3: {
        unsigned i = sample(row, d, s);
        const uint8_t *c = i < (unsigned)f->npal ? f->palette[i] : f->palette[0];
        memcpy(dst, c, 4);
        break;
    }
    case 4:
        dst[0] = dst[1] = dst[2] = to8(sample(row, d, s), d);
        dst[3] = to8(sample(row, d, s + 1), d);
        break;
    default:
        for (int k = 0; k < 4; k++) dst[k] = to8(sample(row, d, s + k), d);
        break;
    }
}

uint8_t *png_decode(const uint8_t *data, size_t size, int *out_w, int *out_h, char *err, size_t err_cap) {
    if (size < 8 || memcmp(data, sig, 8) != 0) {
        set_err(err, err_cap, "not a PNG file");
        return NULL;
    }
    Format f;
    memset(&f, 0, sizeof f);
    uint32_t w = 0, h = 0;
    int interlace = 0, have_ihdr = 0;
    uint8_t *idat = NULL;
    size_t idat_size = 0, pos = 8;
    while (pos + 12 <= size) {
        uint32_t len = rd_be32(data + pos);
        const uint8_t *type = data + pos + 4, *body = data + pos + 8;
        if (len > size - pos - 12) {
            break;
        }
        if (!memcmp(type, "IHDR", 4) && len >= 13) {
            w = rd_be32(body);
            h = rd_be32(body + 4);
            f.depth = body[8];
            f.ctype = body[9];
            interlace = body[12];
            have_ihdr = 1;
        } else if (!memcmp(type, "PLTE", 4)) {
            f.npal = (int)(len / 3 > 256 ? 256 : len / 3);
            for (int i = 0; i < f.npal; i++) {
                memcpy(f.palette[i], body + i * 3, 3);
                f.palette[i][3] = 255;
            }
        } else if (!memcmp(type, "tRNS", 4)) {
            if (f.ctype == 3) {
                for (uint32_t i = 0; i < len && i < 256; i++) f.palette[i][3] = body[i];
            } else if (f.ctype == 0 && len >= 2) {
                f.trns[0] = (uint16_t)(body[0] << 8 | body[1]);
                f.has_trns = 1;
            } else if (f.ctype == 2 && len >= 6) {
                for (int k = 0; k < 3; k++) f.trns[k] = (uint16_t)(body[k * 2] << 8 | body[k * 2 + 1]);
                f.has_trns = 1;
            }
        } else if (!memcmp(type, "IDAT", 4)) {
            uint8_t *n = (uint8_t *)realloc(idat, idat_size + len + 1);
            if (!n) {
                free(idat);
                set_err(err, err_cap, "out of memory");
                return NULL;
            }
            idat = n;
            memcpy(idat + idat_size, body, len);
            idat_size += len;
        } else if (!memcmp(type, "IEND", 4)) {
            break;
        }
        pos += 12 + len;
    }
    static const int chan[7] = {1, 0, 3, 1, 2, 0, 4};
    int ok_depth = f.ctype == 3 ? (f.depth == 1 || f.depth == 2 || f.depth == 4 || f.depth == 8)
                 : f.ctype == 0 ? (f.depth == 1 || f.depth == 2 || f.depth == 4 || f.depth == 8 || f.depth == 16)
                                : (f.depth == 8 || f.depth == 16);
    if (!have_ihdr || w == 0 || h == 0 || w > 16384 || h > 16384 || f.ctype > 6 || chan[f.ctype] == 0 || !ok_depth ||
        interlace > 1 || !idat || (f.ctype == 3 && f.npal == 0)) {
        free(idat);
        set_err(err, err_cap, "unsupported or damaged PNG");
        return NULL;
    }
    f.channels = chan[f.ctype];
    const int bits_pp = f.channels * f.depth, bpp = bits_pp < 8 ? 1 : bits_pp / 8;

    static const int x0[7] = {0, 4, 0, 2, 0, 1, 0}, y0[7] = {0, 0, 4, 0, 2, 0, 1};
    static const int dx[7] = {8, 8, 4, 4, 2, 2, 1}, dy[7] = {8, 8, 8, 4, 4, 2, 2};
    const int passes = interlace ? 7 : 1;
    size_t raw_size = 0;
    for (int p = 0; p < passes; p++) {
        size_t pw = interlace ? (w - x0[p] + dx[p] - 1) / dx[p] : w;
        size_t ph = interlace ? (h - y0[p] + dy[p] - 1) / dy[p] : h;
        if (w <= (uint32_t)x0[p] || h <= (uint32_t)y0[p]) pw = ph = 0;
        if (pw && ph) raw_size += ph * (1 + (pw * bits_pp + 7) / 8);
    }
    uint8_t *raw = (uint8_t *)malloc(raw_size ? raw_size : 1);
    uint8_t *rgba = (uint8_t *)malloc((size_t)w * h * 4);
    if (!raw || !rgba || zlib_inflate(idat, idat_size, raw, raw_size) != (long)raw_size) {
        free(idat);
        free(raw);
        free(rgba);
        set_err(err, err_cap, "damaged PNG image data");
        return NULL;
    }
    free(idat);
    uint8_t *p = raw;
    for (int pass = 0; pass < passes; pass++) {
        size_t pw = interlace ? (w - x0[pass] + dx[pass] - 1) / dx[pass] : w;
        size_t ph = interlace ? (h - y0[pass] + dy[pass] - 1) / dy[pass] : h;
        if (w <= (uint32_t)x0[pass] || h <= (uint32_t)y0[pass] || !pw || !ph) continue;
        size_t rowbytes = (pw * bits_pp + 7) / 8;
        if (unfilter(p, rowbytes, (int)ph, bpp) != 0) {
            free(raw);
            free(rgba);
            set_err(err, err_cap, "damaged PNG filter data");
            return NULL;
        }
        for (size_t y = 0; y < ph; y++) {
            const uint8_t *row = p + y * (rowbytes + 1) + 1;
            size_t oy = interlace ? y0[pass] + y * dy[pass] : y;
            for (size_t x = 0; x < pw; x++) {
                size_t ox = interlace ? x0[pass] + x * dx[pass] : x;
                put_pixel(&f, row, x, rgba + (oy * w + ox) * 4);
            }
        }
        p += ph * (rowbytes + 1);
    }
    free(raw);
    *out_w = (int)w;
    *out_h = (int)h;
    return rgba;
}

uint8_t *png_load(const char *path, int *w, int *h, char *err, size_t err_cap) {
    FILE *fp = fopen(path, "rb");
    if (!fp) {
        set_err(err, err_cap, "can't open the file");
        return NULL;
    }
    fseek(fp, 0, SEEK_END);
    long n = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    uint8_t *buf = n > 0 ? (uint8_t *)malloc((size_t)n) : NULL;
    if (!buf || fread(buf, 1, (size_t)n, fp) != (size_t)n) {
        fclose(fp);
        free(buf);
        set_err(err, err_cap, "can't read the file");
        return NULL;
    }
    fclose(fp);
    uint8_t *rgba = png_decode(buf, (size_t)n, w, h, err, err_cap);
    free(buf);
    return rgba;
}

static void chunk(FILE *fp, const char *type, const uint8_t *body, size_t n) {
    uint8_t b[4];
    wr_be32(b, (uint32_t)n);
    fwrite(b, 1, 4, fp);
    fwrite(type, 1, 4, fp);
    if (n) fwrite(body, 1, n, fp);
    uint32_t c = crc32_update(0xFFFFFFFFu, (const uint8_t *)type, 4);
    wr_be32(b, crc32_update(c, body, n) ^ 0xFFFFFFFFu);
    fwrite(b, 1, 4, fp);
}

int png_save(const char *path, const uint8_t *rgba, int w, int h) {
    const size_t stride = (size_t)w * 4;
    uint8_t *raw = (uint8_t *)malloc((stride + 1) * (size_t)h), *line = (uint8_t *)malloc(stride);
    if (!raw || !line) {
        free(raw);
        free(line);
        return -1;
    }
    /* Per row, the filter with the smallest sum of absolute values. */
    for (int y = 0; y < h; y++) {
        const uint8_t *cur = rgba + y * stride, *up = y ? cur - stride : NULL;
        uint8_t *out = raw + y * (stride + 1);
        long best = -1;
        for (int type = 0; type < 5; type++) {
            long sum = 0;
            for (size_t x = 0; x < stride; x++) {
                int a = x >= 4 ? cur[x - 4] : 0, b = up ? up[x] : 0, c = up && x >= 4 ? up[x - 4] : 0;
                int pred = type == 0 ? 0 : type == 1 ? a : type == 2 ? b : type == 3 ? (a + b) >> 1 : paeth(a, b, c);
                line[x] = (uint8_t)(cur[x] - pred);
                sum += line[x] < 128 ? line[x] : 256 - line[x];
            }
            if (best < 0 || sum < best) {
                best = sum;
                out[0] = (uint8_t)type;
                memcpy(out + 1, line, stride);
            }
        }
    }
    free(line);
    size_t zn = 0;
    uint8_t *z = zlib_compress(raw, (stride + 1) * (size_t)h, &zn);
    free(raw);
    FILE *fp = z ? fopen(path, "wb") : NULL;
    if (!fp) {
        free(z);
        return -1;
    }
    uint8_t ihdr[13];
    wr_be32(ihdr, (uint32_t)w);
    wr_be32(ihdr + 4, (uint32_t)h);
    ihdr[8] = 8;  /* bit depth */
    ihdr[9] = 6;  /* RGBA */
    ihdr[10] = ihdr[11] = ihdr[12] = 0;
    fwrite(sig, 1, 8, fp);
    chunk(fp, "IHDR", ihdr, 13);
    chunk(fp, "IDAT", z, zn);
    chunk(fp, "IEND", NULL, 0);
    free(z);
    return fclose(fp) == 0 ? 0 : -1;
}
