#include "unused_gallery.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <algorithm>
#include <string>
#include <exception>

#include "iso.h"

namespace {

/* The game program's file tables
 * SLPS_016.03 is loaded at 0x80010000 after a 0x800-byte header. Archive tables
 * are arrays of {start sector, byte size} relative to the archive file. */
constexpr uint32_t kExeBase = 0x80010000u - 0x800u;
constexpr uint32_t kBinTable = 0x80098EE8u;    // BIN.BIN
constexpr uint32_t kLapksTable = 0x80098D10u;  // LAPKS.BIN (Lain's animations)
constexpr uint32_t kSiteATable = 0x8009A460u;  // SITEA.BIN (disc 1 pictures)
constexpr uint32_t kSiteBTable = 0x8009BD10u;  // SITEB.BIN (disc 2 pictures)
constexpr uint32_t kRunLevelTable = 0x800A5CE4u;  // 111 run/level codes of the picture decoder

struct Disc {
    FILE *f = nullptr;
    ~Disc() {
        if (f) fclose(f);
    }
    std::vector<uint8_t> file(const char *name) {
        uint32_t lba, size;
        std::vector<uint8_t> out;
        if (!f || iso_find_root_file(f, name, &lba, &size) != 0) {
            return out;
        }
        uint8_t *p = iso_read_file(f, lba, size);
        if (p) {
            out.assign(p, p + size);
            free(p);
        }
        return out;
    }
};

uint32_t rd32(const std::vector<uint8_t> &d, size_t o) {
    return o + 4 <= d.size() ? d[o] | d[o + 1] << 8 | d[o + 2] << 16 | (uint32_t)d[o + 3] << 24 : 0;
}
uint16_t rd16(const std::vector<uint8_t> &d, size_t o) {
    return o + 2 <= d.size() ? (uint16_t)(d[o] | d[o + 1] << 8) : 0;
}

/* Entry i of an archive table in the program: the bytes of that entry. */
std::vector<uint8_t> entry(const std::vector<uint8_t> &exe, uint32_t table, const std::vector<uint8_t> &archive,
                           int i) {
    size_t t = table - kExeBase + (size_t)i * 8;
    uint32_t sector = rd32(exe, t), size = rd32(exe, t + 4);
    size_t start = (size_t)sector * 2048;
    if (start + size > archive.size() || size == 0) {
        return {};
    }
    return std::vector<uint8_t>(archive.begin() + start, archive.begin() + start + size);
}

/* "napk" LZ: flag byte per 8 items, MSB first; set bit = copy (offset+1, length+3). */
std::vector<uint8_t> unnapk(const std::vector<uint8_t> &src) {
    if (src.size() < 8 || memcmp(src.data(), "napk", 4) != 0) {
        return src;
    }
    size_t remaining = rd32(src, 4);
    if (remaining > (16u << 20)) return {};  // no archive entry is this big: corrupt
    std::vector<uint8_t> out;
    out.reserve(remaining);
    size_t p = 8;
    while (p < src.size() && remaining > 0) {
        uint8_t flags = src[p++];
        for (int bit = 0; bit < 8 && p < src.size() && remaining > 0; bit++) {
            if (flags & (0x80 >> bit)) {
                if (p + 1 >= src.size()) break;
                size_t back = src[p] + 1u, len = src[p + 1] + 3u;
                p += 2;
                if (back > out.size()) return out;  // corrupt: reference before the start
                for (size_t k = 0; k < len; k++) out.push_back(out[out.size() - back]);
                remaining -= std::min(remaining, len);
            } else {
                out.push_back(src[p++]);
                remaining -= remaining > 0;
            }
        }
    }
    return out;
}

uint32_t rgba15(uint16_t v, bool transparent_black) {
    uint32_t r = (v & 31) << 3, g = ((v >> 5) & 31) << 3, b = ((v >> 10) & 31) << 3;
    uint32_t a = (v == 0 && transparent_black) ? 0 : 255;
    return r | g << 8 | b << 16 | a << 24;
}

/* PlayStation TIM (4/8/16/24-bit) -> RGBA; the image may follow a small header. */
bool decode_tim(std::vector<uint8_t> t, GalleryItem &item, int clut_index = 0) {
    for (size_t off : {0u, 4u, 8u}) {
        if (rd32(t, off) == 0x10) {
            t.erase(t.begin(), t.begin() + off);
            break;
        }
    }
    if (rd32(t, 0) != 0x10) return false;
    uint32_t flag = rd32(t, 4);
    size_t p = 8;
    std::vector<uint16_t> clut;
    if ((flag & 7) == 3) {  // 24-bit, stored as three bytes per pixel
        uint16_t pw = rd16(t, 16), ph = rd16(t, 18);
        int w = pw * 2 / 3, h = ph;
        if ((size_t)20 + (size_t)w * h * 3 > t.size()) return false;
        std::vector<uint32_t> px((size_t)w * h);
        for (size_t i = 0; i < px.size(); i++) {
            const uint8_t *c = &t[20 + i * 3];
            px[i] = c[0] | c[1] << 8 | c[2] << 16 | 0xFFu << 24;
        }
        item.w = w;
        item.h = h;
        item.frames.push_back(std::move(px));
        return true;
    }
    if (flag & 8) {
        uint32_t len = rd32(t, p);
        uint16_t cw = rd16(t, p + 8), ch = rd16(t, p + 10);
        size_t n = (size_t)cw * ch;
        if (n > 4096 || p + 12 + n * 2 > t.size() || len < 12) return false;
        for (size_t i = 0; i < n; i++) clut.push_back(rd16(t, p + 12 + 2 * i));
        p += len;
    }
    uint32_t len = rd32(t, p);
    uint16_t pw = rd16(t, p + 8), ph = rd16(t, p + 10);
    size_t data = p + 12;
    if (data + (size_t)len - 12 > t.size() || len < 12) return false;
    int bpp = flag & 3;
    int w = bpp == 0 ? pw * 4 : bpp == 1 ? pw * 2 : pw;
    if (w == 0 || ph == 0 || w > 1024 || ph > 512 || data + (size_t)pw * ph * 2 > t.size()) return false;
    std::vector<uint32_t> px((size_t)w * ph);
    for (int y = 0; y < ph; y++) {
        for (int x = 0; x < w; x++) {
            uint16_t v;
            if (bpp == 0) {
                uint8_t b = t[data + (size_t)y * pw * 2 + x / 2];
                size_t ci = (size_t)clut_index * 16 + ((x & 1) ? b >> 4 : b & 15);
                v = ci < clut.size() ? clut[ci] : 0;
            } else if (bpp == 1) {
                size_t ci = (size_t)clut_index * 256 + t[data + (size_t)y * pw * 2 + x];
                v = ci < clut.size() ? clut[ci] : 0;
            } else {
                v = rd16(t, data + ((size_t)y * pw + x) * 2);
            }
            px[(size_t)y * w + x] = rgba15(v, true);
        }
    }
    item.w = w;
    item.h = ph;
    item.frames.push_back(std::move(px));
    return true;
}

/* Lain's animation frames (LAPKS.BIN)
 * Each frame is an MDEC picture: a Huffman-style bit stream of run/level codes
 * (the game's own table of 111 codes), 16x16 macroblocks (Cr, Cb, 4 x Y) in
 * column order, dequantised with the standard PlayStation matrix. */
const int kZigzag[64] = {0,  1,  8,  16, 9,  2,  3,  10, 17, 24, 32, 25, 18, 11, 4,  5,  12, 19, 26, 33, 40, 48,
                         41, 34, 27, 20, 13, 6,  7,  14, 21, 28, 35, 42, 49, 56, 57, 50, 43, 36, 29, 22, 15, 23,
                         30, 37, 44, 51, 58, 59, 52, 45, 38, 31, 39, 46, 53, 60, 61, 54, 47, 55, 62, 63};
const int kQuant[64] = {2,  16, 19, 22, 26, 27, 29, 34, 16, 16, 22, 24, 27, 29, 34, 37, 19, 22, 26, 27, 29, 34,
                        34, 38, 22, 22, 26, 27, 29, 34, 37, 40, 22, 26, 27, 29, 32, 35, 40, 48, 26, 27, 29, 32,
                        35, 40, 48, 58, 26, 27, 29, 34, 38, 46, 56, 69, 27, 29, 35, 38, 46, 56, 69, 83};

struct VlcEntry {
    uint8_t len;
    int16_t value;  // >= 0: index into the run/level table; -1 end of block; -2 escape; -3 invalid
};

struct Vlc {
    std::vector<VlcEntry> lut = std::vector<VlcEntry>(65536, VlcEntry{0, -3});
    uint16_t runlevel[111];

    void add(const char *bits, int extra, int base, bool skip_first = false) {
        int plen = (int)strlen(bits), prefix = 0;
        for (int i = 0; i < plen; i++) prefix = prefix << 1 | (bits[i] == '1');
        for (int i = skip_first ? 1 : 0; i < (1 << extra); i++) {
            int len = plen + extra, code = prefix << extra | i;
            for (int k = 0; k < (1 << (16 - len)); k++) lut[(code << (16 - len)) | k] = {(uint8_t)len, (int16_t)(base + i)};
        }
    }
    void add_special(const char *bits, int16_t v) {
        int len = (int)strlen(bits), code = 0;
        for (int i = 0; i < len; i++) code = code << 1 | (bits[i] == '1');
        for (int k = 0; k < (1 << (16 - len)); k++) lut[(code << (16 - len)) | k] = {(uint8_t)len, v};
    }
    explicit Vlc(const std::vector<uint8_t> &exe) {
        for (int i = 0; i < 111; i++) runlevel[i] = rd16(exe, kRunLevelTable - kExeBase + i * 2);
        add("11", 0, 0);
        add("011", 0, 1);
        add("010", 1, 2);
        add("001", 2, 3, true);  // 00100 starts the next group
        add("0001", 2, 7);
        add("00001", 2, 11);
        add("00100", 3, 15);
        add("0000001", 3, 23);
        add("00000001", 4, 31);
        add("000000001", 4, 47);
        add("0000000001", 4, 63);
        add("00000000001", 4, 79);
        add("000000000001", 4, 95);
        add_special("10", -1);
        add_special("000001", -2);
    }
};

struct BitReader {
    const uint8_t *d;
    size_t n, pos = 0;  // pos in bits, MSB first
    uint32_t peek(int k) const {
        uint32_t v = 0;
        for (int i = 0; i < k; i++) {
            size_t b = pos + i;
            v = v << 1 | ((b / 8 < n) ? (d[b / 8] >> (7 - b % 8)) & 1 : 0);
        }
        return v;
    }
    uint32_t get(int k) {
        uint32_t v = peek(k);
        pos += k;
        return v;
    }
};

void idct8x8(const float in[64], float out[64]) {
    static float c[8][8];
    static bool init;
    if (!init) {
        for (int u = 0; u < 8; u++)
            for (int x = 0; x < 8; x++)
                c[u][x] = (u == 0 ? sqrtf(0.5f) : 1.0f) * cosf((2 * x + 1) * u * 3.14159265f / 16) / 2;
        init = true;
    }
    float tmp[64];
    for (int v = 0; v < 8; v++)
        for (int x = 0; x < 8; x++) {
            float s = 0;
            for (int u = 0; u < 8; u++) s += in[v * 8 + u] * c[u][x];
            tmp[v * 8 + x] = s;
        }
    for (int y = 0; y < 8; y++)
        for (int x = 0; x < 8; x++) {
            float s = 0;
            for (int v = 0; v < 8; v++) s += c[v][y] * tmp[v * 8 + x];
            out[y * 8 + x] = s;
        }
}

std::vector<uint32_t> decode_frame(const Vlc &vlc, const uint8_t *bits, size_t nbytes, int w, int h, int qy,
                                   int qc) {
    std::vector<uint32_t> px((size_t)w * h, 0xFF000000u);
    BitReader br{bits, nbytes};
    int mbx = 0, mby = 0;
    const int count = (w / 16) * (h / 16);
    for (int m = 0; m < count; m++) {
        float blocks[6][64];
        for (int bi = 0; bi < 6; bi++) {
            float coef[64] = {0};
            const int q = bi < 2 ? qc : qy;
            int dc = (int)br.get(10);
            if (dc & 512) dc -= 1024;
            coef[0] = (float)(dc * kQuant[0]);
            int k = 0;
            for (;;) {
                const VlcEntry &e = vlc.lut[br.peek(16)];
                if (e.value == -3) return px;  // corrupt stream
                br.pos += e.len;
                if (e.value == -1) break;
                int run, level;
                if (e.value == -2) {
                    run = (int)br.get(6);
                    level = (int)br.get(8);
                    if (level == 0) level = (int)br.get(8);
                    else if (level == 0x80) level = (int)br.get(8) - 256;
                    else if (level & 0x80) level -= 256;
                } else {
                    uint16_t rl = vlc.runlevel[e.value];
                    run = rl >> 10;
                    level = rl & 0x3FF;
                    if (br.get(1)) level = -level;
                }
                k += run + 1;
                if (k > 63) break;
                coef[kZigzag[k]] = (float)level * q * kQuant[k] / 8.0f;
            }
            idct8x8(coef, blocks[bi]);
        }
        // Cr, Cb (8x8, upsampled), then Y0 Y1 / Y2 Y3
        for (int y = 0; y < 16; y++) {
            for (int x = 0; x < 16; x++) {
                const float *yb = blocks[2 + (y / 8) * 2 + (x / 8)];
                float Y = yb[(y % 8) * 8 + (x % 8)];
                float cr = blocks[0][(y / 2) * 8 + x / 2], cb = blocks[1][(y / 2) * 8 + x / 2];
                float r = Y + 1.402f * cr + 128, g = Y - 0.3437f * cb - 0.7143f * cr + 128, b = Y + 1.772f * cb + 128;
                auto c8 = [](float v) { return (uint32_t)std::min(255.0f, std::max(0.0f, v)); };
                int X = mbx * 16 + x, Yp = mby * 16 + y;
                if (X < w && Yp < h) px[(size_t)Yp * w + X] = c8(r) | c8(g) << 8 | c8(b) << 16 | 0xFFu << 24;
            }
        }
        if (++mby * 16 >= h) {
            mby = 0;
            mbx++;
        }
    }
    return px;
}

/* One LAPK pack ("lapk", size, frame count, frame table, frame data). Frames can
 * differ in size; each has an anchor point (low half: x from its left edge, high
 * half: y from its top) that stays fixed on screen, so they are composed onto one
 * canvas around it. */
bool decode_lapk(const Vlc &vlc, const std::vector<uint8_t> &e, GalleryItem &item) {
    if (e.size() < 12 || memcmp(e.data(), "lapk", 4) != 0) return false;
    uint32_t count = std::min<uint32_t>(rd32(e, 8), 200);
    size_t dat = 12 + (size_t)count * 12;
    struct Frame { int w, h, ax, ay; std::vector<uint32_t> px; };
    std::vector<Frame> frames;
    int left = 0, right = 0, up = 0, down = 0;
    for (uint32_t k = 0; k < count; k++) {
        size_t f = dat + rd32(e, 12 + 12 * k);
        uint32_t anchor = rd32(e, 16 + 12 * k);
        if (f + 16 >= e.size()) return false;
        int w = (int16_t)rd16(e, f), h = (int16_t)rd16(e, f + 2);
        int qy = rd16(e, f + 4), qc = rd16(e, f + 6);
        if (w <= 0 || h <= 0 || w > 640 || h > 480) return false;
        int ax = (int16_t)(anchor & 0xFFFF), ay = (int16_t)(anchor >> 16);
        left = std::max(left, ax);
        right = std::max(right, w - ax);
        up = std::max(up, ay);
        down = std::max(down, h - ay);
        frames.push_back({w, h, ax, ay, decode_frame(vlc, &e[f + 16], e.size() - (f + 16), w, h, qy, qc)});
    }
    if (frames.empty()) return false;
    item.w = left + right;
    item.h = up + down;
    if (item.w <= 0 || item.h <= 0 || item.w > 1024 || item.h > 1024) return false;  // bad anchors
    for (const Frame &fr : frames) {
        std::vector<uint32_t> canvas((size_t)item.w * item.h, 0); // transparent around smaller frames
        int ox = left - fr.ax, oy = up - fr.ay;
        for (int y = 0; y < fr.h; y++)
            for (int x = 0; x < fr.w; x++)
                canvas[(size_t)(oy + y) * item.w + ox + x] = fr.px[(size_t)y * fr.w + x];
        item.frames.push_back(std::move(canvas));
    }
    return true;
}

/* Sounds
 * The PlayStation's 4-bit ADPCM, used both by the sound bank (SPU "VAG" samples:
 * 16-byte blocks of 28 samples) and by XA voice sectors (18 groups of 128 bytes,
 * 8 blocks of 28 samples each). */
const int kAdpcmK[5][2] = {{0, 0}, {60, 0}, {115, -52}, {98, -55}, {122, -60}};

int16_t adpcm_sample(int nibble, int shift, int filter, int &h1, int &h2) {
    int v = nibble >= 8 ? nibble - 16 : nibble;
    const int *k = kAdpcmK[filter < 5 ? filter : 0];
    int s = ((v << 12) >> shift) + ((h1 * k[0] + h2 * k[1] + 32) >> 6);
    s = s < -32768 ? -32768 : s > 32767 ? 32767 : s;
    h2 = h1;
    h1 = s;
    return (int16_t)s;
}

std::vector<int16_t> decode_vag(const uint8_t *d, size_t n) {
    std::vector<int16_t> out;
    int h1 = 0, h2 = 0;
    for (size_t i = 0; i + 16 <= n; i += 16) {
        const int shift = d[i] & 15, filter = (d[i] >> 4) & 7;
        for (int j = 0; j < 28; j++) {
            out.push_back(adpcm_sample((d[i + 2 + j / 2] >> ((j & 1) * 4)) & 15, shift, filter, h1, h2));
        }
    }
    return out;
}

/* One sample of the sound bank (SND.BIN entry 0 = VAB header, entry 1 = body). */
bool sound_bank_sample(const std::vector<uint8_t> &exe, const std::vector<uint8_t> &snd, int vag, GalleryItem &it) {
    constexpr uint32_t kSndTable = 0x80099048u;
    std::vector<uint8_t> head = entry(exe, kSndTable, snd, 0), body = entry(exe, kSndTable, snd, 1);
    if (head.size() < 0x820 || memcmp(head.data(), "pBAV", 4) != 0) return false;
    int programs = 0;
    for (int p = 0; p < 128; p++) programs += head[0x20 + p * 16] != 0;
    const size_t sizes = 0x820 + (size_t)programs * 512;  // after the tone attributes
    if (sizes + 512 > head.size() || vag < 1 || vag > 255) return false;
    size_t off = 0;
    for (int i = 0; i < vag; i++) off += (size_t)rd16(head, sizes + i * 2) * 8;
    size_t len = (size_t)rd16(head, sizes + vag * 2) * 8;
    if (len == 0 || off + len > body.size()) return false;
    it.pcm = decode_vag(&body[off], len);
    it.rate = 22050;
    return !it.pcm.empty();
}

/* One channel of an XA voice file (raw 2352-byte sectors of the disc image). */
bool xa_channel(FILE *disc, const char *file, int channel, GalleryItem &it) {
    struct Find { const char *name; uint32_t lba = 0, size = 0; } find{file};
    iso_list_dir(disc, "XA", [](const char *name, uint32_t lba, uint32_t size, void *user) {
        Find *f = (Find *)user;
        if (strcmp(name, f->name) == 0) { f->lba = lba; f->size = size; }
    }, &find);
    if (!find.lba) return false;
    uint32_t sectors = (find.size + 2047) / 2048;
    int hist[2][2] = {{0, 0}, {0, 0}};
    std::vector<int16_t> ch[2];
    uint8_t sec[2352];
    for (uint32_t s = 0; s < sectors && s < 200000; s++) {
        if (fseek(disc, (long)(find.lba + s) * 2352, SEEK_SET) != 0 || fread(sec, 1, 2352, disc) != 2352) break;
        const uint8_t *sub = sec + 16;
        if (sub[1] != channel || !(sub[2] & 4)) continue;  // other channel / not audio
        const bool stereo = sub[3] & 1;
        it.rate = (sub[3] & 4) ? 18900 : 37800;
        it.channels = stereo ? 2 : 1;
        const uint8_t *data = sec + 24;
        for (int g = 0; g < 18; g++) {
            const uint8_t *grp = data + g * 128;
            for (int blk = 0; blk < 8; blk++) {
                const int hdr = grp[4 + blk], shift = hdr & 15, filter = (hdr >> 4) & 7;
                const int c = stereo ? (blk & 1) : 0;
                for (int i = 0; i < 28; i++) {
                    int nib = (grp[16 + i * 4 + blk / 2] >> ((blk & 1) * 4)) & 15;
                    ch[c].push_back(adpcm_sample(nib, shift, filter, hist[c][0], hist[c][1]));
                }
            }
        }
    }
    if (ch[0].empty()) return false;
    if (it.channels == 2) {
        size_t n = std::min(ch[0].size(), ch[1].size());
        it.pcm.resize(n * 2);
        for (size_t i = 0; i < n; i++) { it.pcm[2 * i] = ch[0][i]; it.pcm[2 * i + 1] = ch[1][i]; }
    } else {
        it.pcm = std::move(ch[0]);
    }
    return true;
}

/* Name-voice samples: VOICE.BIN entries named in BIN.BIN 0x14 (one name per line),
 * 16-bit PCM played at pitch 0x2DF (7914 Hz). Joined with a short pause between. */
bool name_voice(const std::vector<uint8_t> &exe, const std::vector<uint8_t> &bin, const std::vector<uint8_t> &voice,
                const std::vector<std::string> &names, GalleryItem &it) {
    constexpr uint32_t kVoiceTable = 0x80099068u;
    std::vector<uint8_t> list = unnapk(entry(exe, kBinTable, bin, 0x14));
    std::vector<std::string> all;
    std::string cur;
    for (uint8_t c : list) {
        if (c == '\r') continue;
        if (c == '\n') { all.push_back(cur); cur.clear(); } else cur += (char)c;
    }
    it.rate = 7914;
    for (const std::string &want : names) {
        for (size_t i = 0; i < all.size() && i < 639; i++) {
            if (all[i] != want) continue;
            std::vector<uint8_t> e = entry(exe, kVoiceTable, voice, (int)i);
            for (size_t k = 0; k + 1 < e.size(); k += 2) it.pcm.push_back((int16_t)(e[k] | e[k + 1] << 8));
            it.pcm.insert(it.pcm.end(), 7914 / 3, 0);  // a third of a second of silence
        }
    }
    return !it.pcm.empty();
}

}  // namespace

static bool gallery_load_unchecked(const char *disc1_bin, const char *disc2_bin, std::vector<GallerySection> &out,
                                   std::string &err);

bool gallery_load(const char *disc1_bin, const char *disc2_bin, std::vector<GallerySection> &out, std::string &err) {
    // Runs on a background thread: never let a damaged disc image end the program.
    try {
        return gallery_load_unchecked(disc1_bin, disc2_bin, out, err);
    } catch (const std::exception &e) {
        out.clear();
        err = std::string("Couldn't read the discs: ") + e.what();
        return false;
    }
}

static bool gallery_load_unchecked(const char *disc1_bin, const char *disc2_bin, std::vector<GallerySection> &out,
                                   std::string &err) {
    Disc d1, d2;
    d1.f = disc1_bin ? fopen(disc1_bin, "rb") : nullptr;
    d2.f = disc2_bin ? fopen(disc2_bin, "rb") : nullptr;
    std::vector<uint8_t> exe = d1.file("SLPS_016.03");
    if (exe.size() < 0x90000) {
        err = "Couldn't read disc 1 (import it first).";
        return false;
    }
    out.clear();

    // Lain's animations that nothing plays.
    {
        GallerySection sec;
        sec.title = "Lain's unused animations";
        sec.intro = "LAPKS.BIN holds 59 packs of Lain's animated sprite. No code path of the game ever loads "
                    "these 11. Their first and last frames are her usual standing pose, and some share "
                    "frames with animations the game does play; the rest of each is never seen.";
        std::vector<uint8_t> lapks = d1.file("LAPKS.BIN");
        Vlc vlc(exe);
        const struct { int pack; const char *caption; } anims[] = {
            {6, "Fidgets with her hands, then winks."},
            {10, "Head turn / look around (1 of 4)."},
            {11, "Head turn / look around (2 of 4)."},
            {12, "Head turn / look around (3 of 4)."},
            {13, "Head turn / look around (4 of 4)."},
            {17, "A full turn-around. About 40% of its frames also appear in animations the game plays."},
            {19, "Another full turn-around. About 40% of its frames also appear in animations the game plays."},
            {23, "Looks aside with her eyes closed. About half its frames also appear in animations the game plays."},
            {26, "A gesture with raised hands. About 40% of its frames also appear in a similar gesture the game plays."},
            {28, "Another raised-hands gesture. About 40% of its frames also appear in animations the game plays."},
            {51, "Raises her arm, surprised, then points."},
        };
        for (const auto &a : anims) {
            GalleryItem it;
            it.title = "Pack " + std::to_string(a.pack);
            it.caption = a.caption;
            it.fps = 12.0f;
            if (decode_lapk(vlc, entry(exe, kLapksTable, lapks, a.pack), it)) sec.items.push_back(std::move(it));
        }
        out.push_back(std::move(sec));
    }

    // Photos nothing shows.
    {
        GallerySection sec;
        sec.title = "Unused photos";
        sec.intro = "Pictures in SITEA.BIN that no node, slideshow or screen of the game ever loads.";
        std::vector<uint8_t> sitea = d1.file("SITEA.BIN");
        const struct { int index; const char *title, *caption; } pics[] = {
            {631, "SITEA 631", "Two boys on a sports field, one in a #3 baseball shirt."},
            {651, "SITEA 651", "A building entrance with stairs, at night."},
            {744, "SITEA 744", "An orange building under power lines."},
            {787, "SITEA 787", "A spare mouth frame for Lain's name-calling face."},
        };
        for (const auto &p : pics) {
            GalleryItem it;
            it.title = p.title;
            it.caption = p.caption;
            if (decode_tim(unnapk(entry(exe, kSiteATable, sitea, p.index)), it)) sec.items.push_back(std::move(it));
        }
        out.push_back(std::move(sec));
    }

    // Sounds nothing plays.
    {
        GallerySection sec;
        sec.title = "Unused sounds";
        sec.intro = "Sounds on the disc that the game never plays.";
        std::vector<uint8_t> snd = d1.file("SND.BIN");
        struct { int vag; const char *title, *caption; } sfx[] = {
            {3, "Sound effect (bank sample 3)", "In the sound bank as programs 2 and 3; no sound call, animation or music uses them."},
            {23, "Sound effect (bank sample 23)", "Program 22 of the sound bank; never triggered."},
        };
        for (const auto &x : sfx) {
            GalleryItem it;
            it.title = x.title;
            it.caption = x.caption;
            if (sound_bank_sample(exe, snd, x.vag, it)) sec.items.push_back(std::move(it));
        }
        std::vector<uint8_t> bin = d1.file("BIN.BIN"), voice = d1.file("VOICE.BIN");
        {
            GalleryItem it;
            it.title = "Name voice: word endings";
            it.caption = "A_END ... U_END: samples for the voice that calls your name, which the code never asks for.";
            if (name_voice(exe, bin, voice, {"A_END.WAV", "E_END.WAV", "I_END.WAV", "N_END.WAV", "O_END.WAV", "U_END.WAV"}, it))
                sec.items.push_back(std::move(it));
        }
        {
            GalleryItem it;
            it.title = "Name voice: pi pu pe po";
            it.caption = "A bug drops pi, pu, pe and po from the name, so Lain never says these.";
            if (name_voice(exe, bin, voice, {"PI.WAV", "PU.WAV", "PE.WAV", "PO.WAV"}, it)) sec.items.push_back(std::move(it));
        }
        if (!sec.items.empty()) out.push_back(std::move(sec));
    }

    // Rarely seen: shown only under special conditions.
    {
        GallerySection sec;
        sec.title = "Rarely seen";
        sec.intro = "Used by the game, but most players never see them.";
        if (d2.f) {
            std::vector<uint8_t> siteb = d2.file("SITEB.BIN");
            GalleryItem it;
            it.title = "NO DATA card";
            it.caption = "Shown in the ending credits in place of every short Dc movie you never "
                         "watched to the end.";
            if (decode_tim(unnapk(entry(exe, kSiteBTable, siteb, 0x22A)), it)) sec.items.push_back(std::move(it));
        }
        {
            std::vector<uint8_t> bin = d1.file("BIN.BIN");
            GalleryItem it;
            it.title = "Lain's eyes";
            it.caption = "The top strip fades in on the \"end / continue\" screen with a 1-in-500 "
                         "chance per frame, for six seconds.";
            if (decode_tim(unnapk(entry(exe, kBinTable, bin, 0x1F)), it)) sec.items.push_back(std::move(it));
        }
        {
            GalleryItem it;
            it.title = "Screensaver voice 10 of 10";
            it.caption = "The longest of the idle screensaver's ten voice clips, and the rarest: picked only "
                         "1 time in 4,096 (Env012, LAIN13.XA channel 28).";
            if (xa_channel(d1.f, "LAIN13.XA", 28, it)) sec.items.push_back(std::move(it));
        }
        if (!sec.items.empty()) out.push_back(std::move(sec));
    }
    return true;
}
