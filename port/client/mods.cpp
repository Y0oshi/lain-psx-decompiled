#include "mods.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <filesystem>
#include <map>
#include <memory>
#include <set>
#include <unordered_map>

#include "game_hooks.h"
#include "iso.h"
#include "lain_xa.h"
#include "moddata.h"
#include "napk.h"
#include "png.h"
#include "psx_arena.h"
#include "settings.h"
#include "tim.h"
#include "tracks.h"

extern "C" int PsyX_CD_GetImageInfo(const char **fileName, const unsigned char **mem, int *memSize, int *sectorSize);

namespace fs = std::filesystem;

namespace {

/* The archives on the discs and their entry tables in the game program
 * (arrays of {start sector in the archive, byte size}). */
struct Archive {
    const char *name;
    uint32_t table;
    int count;
    bool on_disc[2];
};

const Archive kArchives[] = {
    {"BIN.BIN", 0x80098EE8u, 44, {true, true}},    {"LAPKS.BIN", 0x80098D10u, 59, {true, true}},
    {"SND.BIN", 0x80099048u, 4, {true, true}},     {"VOICE.BIN", 0x80099068u, 639, {true, true}},
    {"SITEA.BIN", 0x8009A460u, 790, {true, false}}, {"SITEB.BIN", 0x8009BD10u, 558, {false, true}},
};

constexpr uint32_t kExeBase = 0x80010000u - 0x800u; /* SLPS_016.03 loads at 0x80010000 after its header */
constexpr int kVoiceRate = 7914;                    /* VOICE.BIN: 16-bit PCM at SPU pitch 0x2DF */
constexpr size_t kSector = 2048;

std::string lower(std::string s) {
    for (char &c : s) c = (char)tolower((unsigned char)c);
    return s;
}

std::string upper(std::string s) {
    for (char &c : s) c = (char)toupper((unsigned char)c);
    return s;
}

const Archive *find_archive(const std::string &name) {
    for (const Archive &a : kArchives) {
        if (upper(name) == a.name) return &a;
    }
    return nullptr;
}

uint32_t rd32(const uint8_t *p) {
    return p[0] | p[1] << 8 | p[2] << 16 | (uint32_t)p[3] << 24;
}

bool read_file(const fs::path &p, std::vector<uint8_t> &out) {
    FILE *f = fopen(p.u8string().c_str(), "rb");
    if (!f) return false;
    fseek(f, 0, SEEK_END);
    long n = ftell(f);
    fseek(f, 0, SEEK_SET);
    out.resize(n > 0 ? (size_t)n : 0);
    bool ok = n >= 0 && fread(out.data(), 1, out.size(), f) == out.size();
    fclose(f);
    return ok;
}

bool write_file(const fs::path &p, const void *data, size_t n) {
    FILE *f = fopen(p.u8string().c_str(), "wb");
    if (!f) return false;
    bool ok = fwrite(data, 1, n, f) == n;
    return fclose(f) == 0 && ok;
}

/* Both discs and the game program. */
struct Context {
    FILE *disc[2] = {nullptr, nullptr};
    std::vector<uint8_t> exe;
    std::string error;

    ~Context() {
        for (FILE *f : disc) {
            if (f) fclose(f);
        }
    }

    bool open(const char *d1, const char *d2) {
        const char *p[2] = {d1, d2};
        for (int i = 0; i < 2; i++) {
            if (p[i] && p[i][0]) {
                disc[i] = fopen(p[i], "rb");
            }
        }
        uint32_t lba, size;
        if (!disc[0] || iso_find_root_file(disc[0], "SLPS_016.03", &lba, &size) != 0) {
            error = "import disc 1 first";
            return false;
        }
        uint8_t *e = iso_read_file(disc[0], lba, size);
        if (!e) {
            error = "can't read the game program from disc 1";
            return false;
        }
        exe.assign(e, e + size);
        free(e);
        return true;
    }

    /* The first imported disc that has the archive, or -1. */
    int disc_for(const Archive &a) const {
        for (int d = 0; d < 2; d++) {
            if (a.on_disc[d] && disc[d]) return d;
        }
        return -1;
    }

    bool entry_loc(const Archive &a, int i, uint32_t &sector, uint32_t &size) const {
        size_t t = a.table - kExeBase + (size_t)i * 8;
        if (i < 0 || i >= a.count || t + 8 > exe.size()) return false;
        sector = rd32(&exe[t]);
        size = rd32(&exe[t + 4]);
        return true;
    }

    bool archive_lba(int d, const Archive &a, uint32_t &lba) const {
        uint32_t size;
        return disc[d] && iso_find_root_file(disc[d], a.name, &lba, &size) == 0;
    }

    std::vector<uint8_t> entry(const Archive &a, int i) const {
        std::vector<uint8_t> out;
        uint32_t sector, size, lba;
        int d = disc_for(a);
        if (d < 0 || !entry_loc(a, i, sector, size) || !archive_lba(d, a, lba) || size == 0) return out;
        uint8_t *p = iso_read_file(disc[d], lba + sector, size);
        if (p) {
            out.assign(p, p + size);
            free(p);
        }
        return out;
    }
};

/* WAV (PCM 8/16/24/32-bit or 32-bit float, any rate and channel count) as mono
 * 16-bit PCM at `rate`. */
bool wav_to_pcm(const std::vector<uint8_t> &w, int rate, std::vector<int16_t> &out, std::string &err) {
    if (w.size() < 12 || memcmp(w.data(), "RIFF", 4) || memcmp(w.data() + 8, "WAVE", 4)) {
        err = "not a WAV file";
        return false;
    }
    int fmt = 0, channels = 0, src_rate = 0, bits = 0;
    const uint8_t *data = nullptr;
    size_t data_n = 0;
    for (size_t p = 12; p + 8 <= w.size();) {
        uint32_t n = rd32(&w[p + 4]);
        const uint8_t *body = w.data() + p + 8;
        if (n > w.size() - p - 8) n = (uint32_t)(w.size() - p - 8);
        if (!memcmp(&w[p], "fmt ", 4) && n >= 16) {
            fmt = body[0] | body[1] << 8;
            channels = body[2] | body[3] << 8;
            src_rate = (int)rd32(body + 4);
            bits = body[14] | body[15] << 8;
            if (fmt == 0xFFFE && n >= 26) fmt = body[24] | body[25] << 8; /* WAVE_FORMAT_EXTENSIBLE */
        } else if (!memcmp(&w[p], "data", 4)) {
            data = body;
            data_n = n;
        }
        p += 8 + n + (n & 1);
    }
    bool pcm = fmt == 1 && (bits == 8 || bits == 16 || bits == 24 || bits == 32);
    bool flt = fmt == 3 && bits == 32;
    if (!data || channels < 1 || src_rate < 1000 || (!pcm && !flt)) {
        err = "unsupported WAV format (use 16-bit PCM)";
        return false;
    }
    const size_t bps = (size_t)bits / 8, frames = data_n / (bps * channels);
    std::vector<float> mono(frames);
    for (size_t f = 0; f < frames; f++) {
        float sum = 0;
        for (int c = 0; c < channels; c++) {
            const uint8_t *s = data + (f * channels + c) * bps;
            float v;
            if (flt) {
                uint32_t u = rd32(s);
                memcpy(&v, &u, 4);
            } else if (bits == 8) {
                v = (s[0] - 128) / 128.0f;
            } else if (bits == 16) {
                v = (int16_t)(s[0] | s[1] << 8) / 32768.0f;
            } else if (bits == 24) {
                v = (int32_t)((uint32_t)s[0] << 8 | (uint32_t)s[1] << 16 | (uint32_t)s[2] << 24) / 2147483648.0f;
            } else {
                v = (int32_t)rd32(s) / 2147483648.0f;
            }
            sum += v;
        }
        mono[f] = sum / channels;
    }
    const size_t n_out = frames ? (size_t)((double)frames * rate / src_rate) : 0;
    out.resize(n_out);
    for (size_t i = 0; i < n_out; i++) {
        double pos = (double)i * src_rate / rate;
        size_t k = (size_t)pos;
        double t = pos - k;
        float a = mono[std::min(k, frames - 1)], b = mono[std::min(k + 1, frames - 1)];
        long v = lrint((a + (b - a) * t) * 32768.0);
        out[i] = (int16_t)(v > 32767 ? 32767 : v < -32768 ? -32768 : v);
    }
    return true;
}

std::vector<uint8_t> pcm_to_wav(const uint8_t *pcm, size_t bytes, int rate) {
    std::vector<uint8_t> w(44 + bytes);
    auto put32 = [&](size_t o, uint32_t v) {
        for (int k = 0; k < 4; k++) w[o + k] = (uint8_t)(v >> (8 * k));
    };
    memcpy(&w[0], "RIFF", 4);
    put32(4, (uint32_t)(36 + bytes));
    memcpy(&w[8], "WAVEfmt ", 8);
    put32(16, 16);
    w[20] = 1;  /* PCM */
    w[22] = 1;  /* mono */
    put32(24, (uint32_t)rate);
    put32(28, (uint32_t)rate * 2);
    w[32] = 2;
    w[34] = 16;
    memcpy(&w[36], "data", 4);
    put32(40, (uint32_t)bytes);
    memcpy(&w[44], pcm, bytes);
    return w;
}

/* Plain text the game reads (ASCII lines, CR or CRLF). */
bool looks_like_text(const std::vector<uint8_t> &d) {
    if (d.size() < 4) return false;
    size_t lines = 0;
    for (uint8_t c : d) {
        if (c == '\n' || c == '\r') lines++;
        else if (c == '\t' || (c >= 0x20 && c < 0x7F)) continue;
        else return false;
    }
    return lines > 0;
}

/* The bytes the game will read for a replacement ("stored": packed if the
 * original is). Returns the level: 0 ok, 1 warning (still used), 2 error. */
int build(const Context &ctx, const Archive &a, int entry, const fs::path &file, const std::string &ext,
          std::vector<uint8_t> &stored, std::string &note) {
    std::vector<uint8_t> orig = ctx.entry(a, entry);
    if (orig.empty()) {
        note = ctx.disc_for(a) < 0 ? (a.on_disc[0] ? "import disc 1 first" : "import disc 2 to use this") :
                                     "can't read the original from the disc";
        return a.on_disc[0] || ctx.disc[1] ? 2 : 1; /* disc 2 not imported: only a warning */
    }
    const bool packed = napk_is_packed(orig.data(), orig.size());
    std::vector<uint8_t> unpacked;
    if (packed) {
        size_t n = 0;
        uint8_t *u = napk_unpack(orig.data(), orig.size(), &n);
        if (!u) {
            note = "the original entry is damaged on the disc";
            return 2;
        }
        unpacked.assign(u, u + n);
        free(u);
    } else {
        unpacked = orig;
    }

    std::vector<uint8_t> content;
    int level = 0;
    char msg[256] = "";
    if (ext == "png" || ext == "tim") {
        TimInfo ti;
        if (!tim_parse(unpacked.data(), unpacked.size(), &ti)) {
            note = "this entry isn't a picture (use .bin" + std::string(looks_like_text(unpacked) ? " or .txt)" : ")");
            return 2;
        }
        content = unpacked;
        if (ext == "png") {
            int w = 0, h = 0;
            char err[128];
            uint8_t *rgba = png_load(file.u8string().c_str(), &w, &h, err, sizeof err);
            if (!rgba) {
                note = std::string("can't read the PNG: ") + err;
                return 2;
            }
            if (w != ti.w || h != ti.h) {
                free(rgba);
                snprintf(msg, sizeof msg, "must be %dx%d pixels like the original (this one is %dx%d)", ti.w, ti.h, w, h);
                note = msg;
                return 2;
            }
            int r = tim_from_rgba(content.data(), content.size(), &ti, rgba, msg, sizeof msg);
            free(rgba);
            if (r < 0) {
                note = msg;
                return 2;
            }
            if (r == 1) {
                note = msg;
                level = 1;
            }
        } else {
            std::vector<uint8_t> t;
            TimInfo ni;
            if (!read_file(file, t) || !tim_parse(t.data(), t.size(), &ni)) {
                note = "not a TIM image";
                return 2;
            }
            if (ni.bpp != ti.bpp || ni.w != ti.w || ni.h != ti.h || ni.clut_w != ti.clut_w || ni.clut_h != ti.clut_h) {
                snprintf(msg, sizeof msg, "must be a %d-bit %dx%d TIM like the original", ti.bpp, ti.w, ti.h);
                note = msg;
                return 2;
            }
            if (ti.clut_w) {
                memcpy(&content[ti.clut_data], &t[ni.clut_data], (size_t)ti.clut_w * ti.clut_h * 2);
            }
            memcpy(&content[ti.pix_data], &t[ni.pix_data], (size_t)ti.pix_stride * ti.h);
        }
    } else if (ext == "wav") {
        if (strcmp(a.name, "VOICE.BIN") != 0) {
            note = "WAV files replace VOICE.BIN entries only";
            return 2;
        }
        std::vector<uint8_t> w;
        std::vector<int16_t> pcm;
        std::string err;
        if (!read_file(file, w) || !wav_to_pcm(w, kVoiceRate, pcm, err)) {
            note = err.empty() ? "can't read the WAV" : err;
            return 2;
        }
        content.resize(pcm.size() * 2);
        for (size_t i = 0; i < pcm.size(); i++) {
            content[i * 2] = (uint8_t)pcm[i];
            content[i * 2 + 1] = (uint8_t)(pcm[i] >> 8);
        }
    } else if (ext == "txt") {
        std::vector<uint8_t> t;
        if (!read_file(file, t)) {
            note = "can't read the file";
            return 2;
        }
        if (t.size() >= 3 && t[0] == 0xEF && t[1] == 0xBB && t[2] == 0xBF) t.erase(t.begin(), t.begin() + 3);
        /* the game splits lines at CR: keep the original's line endings */
        bool crlf = std::search(unpacked.begin(), unpacked.end(), std::begin("\r\n"), std::begin("\r\n") + 2) !=
                    unpacked.end();
        bool cr_only = !crlf && std::find(unpacked.begin(), unpacked.end(), '\r') != unpacked.end();
        for (size_t i = 0; i < t.size(); i++) {
            uint8_t c = t[i];
            if (c >= 0x80) level = 1;
            if (c == '\r') {
                if (i + 1 < t.size() && t[i + 1] == '\n') i++;
                c = '\n';
            }
            if (c == '\n') {
                if (crlf || cr_only) content.push_back('\r');
                if (crlf || !cr_only) content.push_back('\n');
            } else {
                content.push_back(c);
            }
        }
        if (level) note = "has non-ASCII characters; the game shows only plain ASCII here";
    } else {
        if (!read_file(file, content)) {
            note = "can't read the file";
            return 2;
        }
        if (napk_is_packed(content.data(), content.size())) {
            /* already packed: used as is, but it must unpack */
            size_t n = 0;
            uint8_t *u = napk_unpack(content.data(), content.size(), &n);
            if (!u) {
                note = "damaged napk data";
                return 2;
            }
            free(u);
            if (!packed) {
                note = "packed, but the original entry isn't";
                return 2;
            }
            stored = content;
            if (n > unpacked.size()) {
                snprintf(msg, sizeof msg, "too big: %zu bytes unpacked, the original is %zu", n, unpacked.size());
                note = msg;
                return 2;
            }
            goto check_size;
        }
    }
    if (packed) {
        if (content.size() > unpacked.size()) {
            snprintf(msg, sizeof msg, "too big: %zu bytes, the original is %zu", content.size(), unpacked.size());
            note = msg;
            return 2;
        }
        size_t n = 0;
        uint8_t *p = napk_pack(content.data(), content.size(), &n);
        if (!p) {
            note = "out of memory";
            return 2;
        }
        stored.assign(p, p + n);
        free(p);
    } else {
        stored = content;
    }
check_size:
    if (stored.size() > orig.size()) {
        snprintf(msg, sizeof msg, "too big: %zu bytes on the disc, the original takes %zu (%.0f%% over)", stored.size(),
                 orig.size(), 100.0 * ((double)stored.size() / orig.size() - 1));
        note = msg;
        if (ext == "png") note += "; fewer details or colors pack smaller";
        return 2;
    }
    if (note.empty() && (stored == orig || content == unpacked)) note = "same as the original";
    return level;
}

/* A picture or model built into the program (PROGRAM/<variable>.png|.tim|.tmd|.bin):
 * the bytes to write at the variable's PS1 address. */
int build_program(const Context &ctx, const std::string &rel, const fs::path &file, uint32_t &addr,
                  std::vector<uint8_t> &out, std::string &note) {
    std::string fname = rel.substr(rel.rfind('/') + 1);
    size_t dot = fname.rfind('.');
    std::string sym = fname.substr(0, dot), ext = lower(fname.substr(dot + 1));
    const LainSymbol *s = nullptr;
    for (int i = 0; i < lain_symbol_count && !s; i++) {
        if (sym == lain_symbols[i].name) s = &lain_symbols[i];
    }
    if (!s || s->host) {
        note = "no picture or model named " + sym + " in the program (see mods/_originals/PROGRAM)";
        return 2;
    }
    addr = s->addr;
    size_t off = (size_t)(s->addr - kExeBase);
    if (off >= ctx.exe.size()) {
        note = sym + " isn't in the program image";
        return 2;
    }
    const uint8_t *orig = &ctx.exe[off];
    size_t avail = ctx.exe.size() - off;
    char msg[256] = "";
    if (ext == "png" || ext == "tim") {
        TimInfo ti;
        if (!tim_parse(orig, avail, &ti) || ti.start != 0) {
            note = sym + " isn't a picture";
            return 2;
        }
        size_t len = ti.pix_data + (size_t)ti.pix_stride * ti.h;
        out.assign(orig, orig + len);
        if (ext == "png") {
            int w = 0, h = 0;
            char err[128];
            uint8_t *rgba = png_load(file.u8string().c_str(), &w, &h, err, sizeof err);
            if (!rgba) {
                note = std::string("can't read the PNG: ") + err;
                return 2;
            }
            if (w != ti.w || h != ti.h) {
                free(rgba);
                snprintf(msg, sizeof msg, "must be %dx%d pixels like the original (this one is %dx%d)", ti.w, ti.h, w, h);
                note = msg;
                return 2;
            }
            int r = tim_from_rgba(out.data(), out.size(), &ti, rgba, msg, sizeof msg);
            free(rgba);
            if (r < 0) {
                note = msg;
                return 2;
            }
            if (r == 1) note = msg;
            if (note.empty() && memcmp(out.data(), orig, len) == 0) note = "same as the original";
            return r == 1 ? 1 : 0;
        }
        std::vector<uint8_t> t;
        TimInfo ni;
        if (!read_file(file, t) || !tim_parse(t.data(), t.size(), &ni)) {
            note = "not a TIM image";
            return 2;
        }
        if (ni.bpp != ti.bpp || ni.w != ti.w || ni.h != ti.h || ni.clut_w != ti.clut_w || ni.clut_h != ti.clut_h) {
            snprintf(msg, sizeof msg, "must be a %d-bit %dx%d TIM like the original", ti.bpp, ti.w, ti.h);
            note = msg;
            return 2;
        }
        if (ti.clut_w) memcpy(&out[ti.clut_data], &t[ni.clut_data], (size_t)ti.clut_w * ti.clut_h * 2);
        memcpy(&out[ti.pix_data], &t[ni.pix_data], (size_t)ti.pix_stride * ti.h);
        return 0;
    }
    if (ext == "tmd" || ext == "bin") {
        if (!read_file(file, out)) {
            note = "can't read the file";
            return 2;
        }
        if (s->size == 0) {
            note = "the size of " + sym + " isn't known, so it can't be replaced safely";
            return 2;
        }
        if (out.size() > s->size) {
            snprintf(msg, sizeof msg, "too big: %zu bytes, %s has %u", out.size(), sym.c_str(), s->size);
            note = msg;
            return 2;
        }
        if (ext == "tmd" && (out.size() < 12 || rd32(out.data()) != 0x41)) {
            note = "not a TMD model";
            return 2;
        }
        return 0;
    }
    note = "program files are .png or .tim (pictures), .tmd (models) or .bin";
    return 2;
}

/* A movie (.STR) or voice file (.XA) replacing a whole file on the disc, as raw
 * 2352-byte sectors. */
struct Stream {
    std::vector<uint8_t> raw;
    uint32_t sectors = 0;
    uint32_t frames = 0; /* STR: highest frame number */
};

/* Reads Mode 2 sectors of 2352 bytes (with sync and header) or 2336 (from the
 * subheader on). */
int load_stream(const fs::path &file, bool is_xa, Stream &st, std::string &note) {
    std::vector<uint8_t> d;
    if (!read_file(file, d) || d.empty()) {
        note = "can't read the file";
        return 2;
    }
    static const uint8_t sync[12] = {0, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0};
    const bool raw = d.size() % 2352 == 0 && memcmp(d.data(), sync, 12) == 0;
    if (!raw && d.size() % 2336 != 0) {
        note = "not Mode 2 CD sectors (2352 or 2336 bytes each), e.g. from psxavenc";
        return 2;
    }
    st.sectors = (uint32_t)(d.size() / (raw ? 2352 : 2336));
    st.raw.assign((size_t)st.sectors * 2352, 0);
    uint32_t audio = 0;
    for (uint32_t i = 0; i < st.sectors; i++) {
        uint8_t *o = &st.raw[(size_t)i * 2352];
        if (raw) {
            memcpy(o, &d[(size_t)i * 2352], 2352);
        } else {
            memcpy(o, sync, 12);
            o[15] = 2;
            memcpy(o + 16, &d[(size_t)i * 2336], 2336);
        }
        const uint8_t submode = o[18];
        if (submode & 0x04) audio++;
        /* STR video sector: 0x0160 magic, frame number at +8 */
        if (!is_xa && (submode & 0x04) == 0 && o[24] == 0x60 && o[25] == 0x01) {
            uint32_t f = rd32(o + 24 + 8);
            if (f > st.frames && f < 100000) st.frames = f;
        }
    }
    if (is_xa && audio == 0) {
        note = "no XA audio sectors in the file";
        return 2;
    }
    if (!is_xa && st.frames == 0) {
        note = "no video frames found (not a PS1 STR movie)";
        return 2;
    }
    char msg[160];
    if (is_xa) snprintf(msg, sizeof msg, "%u sectors; set the media sizes of its channels in data/media.json if they changed", st.sectors);
    else snprintf(msg, sizeof msg, "%u frames, %u sectors", st.frames, st.sectors);
    note = msg;
    return 0;
}

/* A file in a folder of the disc ("MOVIE/F001.STR"): its LBA and size, or false. */
bool find_disc_file(FILE *img, const std::string &rel, uint32_t &lba, uint32_t &size) {
    if (!img) return false;
    size_t slash = rel.find('/');
    struct Want {
        std::string name;
        uint32_t lba, size;
        bool found;
    } w{upper(rel.substr(slash + 1)), 0, 0, false};
    iso_list_dir(img, upper(rel.substr(0, slash)).c_str(),
                 [](const char *name, uint32_t l, uint32_t sz, void *user) {
                     Want *x = (Want *)user;
                     if (x->name == name) {
                         x->lba = l;
                         x->size = sz;
                         x->found = true;
                     }
                 },
                 &w);
    lba = w.lba;
    size = w.size;
    return w.found;
}

/* A mod file name: leading decimal digits and an extension. */
bool parse_entry_name(const std::string &fname, int &entry, std::string &ext) {
    size_t i = 0;
    while (i < fname.size() && isdigit((unsigned char)fname[i])) i++;
    size_t dot = fname.rfind('.');
    if (i == 0 || i > 6 || dot == std::string::npos) return false;
    entry = atoi(fname.substr(0, i).c_str());
    ext = lower(fname.substr(dot + 1));
    return ext == "png" || ext == "tim" || ext == "bin" || ext == "txt" || ext == "wav";
}

bool is_doc_file(const std::string &name) {
    std::string l = lower(name);
    return l == "mod.ini" || l == ".ds_store" || l == "thumbs.db" || l == "desktop.ini" || l.rfind("readme", 0) == 0 ||
           l.rfind("license", 0) == 0 || l.rfind("preview", 0) == 0 ||
           (l.size() > 3 && (l.compare(l.size() - 3, 3, ".md") == 0));
}

void read_ini(const fs::path &p, ModInfo &m) {
    std::vector<uint8_t> d;
    if (!read_file(p, d)) return;
    std::string s(d.begin(), d.end()), line;
    if (s.size() >= 3 && (uint8_t)s[0] == 0xEF && (uint8_t)s[1] == 0xBB && (uint8_t)s[2] == 0xBF) s.erase(0, 3);
    auto trim = [](std::string t) {
        size_t a = t.find_first_not_of(" \t\r"), b = t.find_last_not_of(" \t\r");
        return a == std::string::npos ? std::string() : t.substr(a, b - a + 1);
    };
    size_t pos = 0;
    while (pos <= s.size()) {
        size_t nl = s.find('\n', pos);
        line = s.substr(pos, nl == std::string::npos ? std::string::npos : nl - pos);
        pos = nl == std::string::npos ? s.size() + 1 : nl + 1;
        line = trim(line);
        if (line.empty() || line[0] == ';' || line[0] == '#') continue;
        size_t eq = line.find('=');
        if (eq == std::string::npos) continue;
        std::string key = lower(trim(line.substr(0, eq))), val = trim(line.substr(eq + 1));
        if (key == "name") m.name = val;
        else if (key == "author") m.author = val;
        else if (key == "version") m.version = val;
        else if (key == "description") m.description += (m.description.empty() ? "" : "\n") + val;
    }
}

ModInfo read_mod(const fs::path &dir) {
    ModInfo m;
    m.folder = dir.filename().u8string();
    read_ini(dir / "mod.ini", m);
    if (m.name.empty()) m.name = m.folder;
    std::error_code ec;
    std::vector<std::string> paths;
    for (auto it = fs::recursive_directory_iterator(dir, ec); !ec && it != fs::recursive_directory_iterator();
         it.increment(ec)) {
        if (!it->is_regular_file(ec)) continue;
        paths.push_back(fs::relative(it->path(), dir, ec).generic_u8string());
    }
    std::sort(paths.begin(), paths.end());
    for (const std::string &rel : paths) {
        size_t slash = rel.find('/');
        std::string top = rel.substr(0, slash), rest = slash == std::string::npos ? "" : rel.substr(slash + 1);
        const Archive *a = slash == std::string::npos ? nullptr : find_archive(top);
        int entry;
        std::string ext;
        const std::string lrel = lower(rel);
        if (lrel.size() > 4 && lrel.compare(lrel.size() - 4, 4, ".lua") == 0) {
            if (lrel == "main.lua") m.has_script = true;
            m.scripts.push_back(rel);
            continue;
        }
        const std::string ltop = lower(top), lrest = lower(rest);
        if ((ltop == "movie" || ltop == "movie2" || ltop == "xa") && slash != std::string::npos &&
            rest.find('/') == std::string::npos &&
            (lrest.size() > 4 && (lrest.compare(lrest.size() - 4, 4, ".str") == 0 || lrest.compare(lrest.size() - 3, 3, ".xa") == 0))) {
            ModFile f;
            f.path = rel;
            f.archive = "DISC";
            m.files.push_back(f);
            continue;
        }
        if (ltop == "src" && slash != std::string::npos) continue; /* a plugin's source */
        if (ltop == "program" && slash != std::string::npos && rest.find('/') == std::string::npos) {
            ModFile f;
            f.path = rel;
            f.archive = "PROGRAM";
            m.files.push_back(f);
            continue;
        }
        if (ltop == "textures" && slash != std::string::npos) {
            if (lower(rel.substr(rel.rfind('.') + 1)) == "png") m.textures++;
            else if (!is_doc_file(rel.substr(rel.rfind('/') + 1))) m.ignored.push_back(rel);
            continue;
        }
        if (ltop == "plugins" && slash != std::string::npos) {
            const std::string e = lower(rel.substr(rel.rfind('.') + 1));
            if (e == "dll" || e == "dylib" || e == "so") m.plugins.push_back(rel);
            else if (!is_doc_file(rel.substr(rel.rfind('/') + 1))) m.ignored.push_back(rel);
            continue;
        }
        if (ltop == "data" && slash != std::string::npos) {
            m.data.push_back(rel);
            ModFile f;
            f.path = rel;
            f.archive = "DATA";
            m.files.push_back(f);
            continue;
        }
        if (!a || rest.find('/') != std::string::npos || !parse_entry_name(rest, entry, ext)) {
            if (!is_doc_file(rel.substr(rel.rfind('/') + 1))) m.ignored.push_back(rel);
            continue;
        }
        ModFile f;
        f.path = rel;
        f.archive = a->name;
        f.entry = entry;
        if (entry >= a->count) {
            f.level = 2;
            f.note = std::string(a->name) + " has entries 0 to " + std::to_string(a->count - 1);
        }
        m.files.push_back(f);
    }
    /* two files for one entry: neither is used */
    for (size_t i = 0; i < m.files.size(); i++) {
        for (size_t j = i + 1; j < m.files.size(); j++) {
            const ModFile &x = m.files[i], &y = m.files[j];
            auto stem = [](const std::string &p) { return p.substr(0, p.rfind('.')); };
            bool same = x.archive != y.archive || x.archive == "DATA" ? false :
                        x.archive == "PROGRAM" ? stem(x.path) == stem(y.path) :
                        x.archive == "DISC"    ? upper(x.path) == upper(y.path) : x.entry == y.entry;
            if (same) {
                m.files[i].level = m.files[j].level = 2;
                m.files[i].note = m.files[j].note = "two files replace this entry; keep one";
            }
        }
    }
    return m;
}

/* What the reader serves: whole sectors of user data, by disc and LBA. */
struct Overlay {
    std::unordered_map<int, std::array<uint8_t, kSector>> sectors[2]; /* user data of Form 1 sectors */
    struct Raw {
        std::shared_ptr<std::vector<uint8_t>> data;
        size_t offset;
    };
    std::unordered_map<int, Raw> raw[2];  /* whole sectors past the end of the disc */
    uint32_t next_virtual[2] = {400000, 400000};
    std::string disc2_path;  /* to tell which disc is inserted */
};
Overlay *g_overlay;

int mods_reader(int lba, unsigned char *raw, void *) {
    const char *name = nullptr;
    const unsigned char *mem = nullptr;
    int mem_size = 0, sector_size = 0;
    int d = PsyX_CD_GetImageInfo(&name, &mem, &mem_size, &sector_size) && name && g_overlay->disc2_path == name ? 1 : 0;
    auto r = g_overlay->raw[d].find(lba);
    if (r != g_overlay->raw[d].end()) {
        memcpy(raw, r->second.data->data() + r->second.offset, 2352);
        /* the header gives the sector's own position (MSF, BCD) */
        const int a = lba + 150;
        const int mm = a / 4500, ss = a / 75 % 60, ff = a % 75;
        raw[12] = (unsigned char)(mm / 10 << 4 | mm % 10);
        raw[13] = (unsigned char)(ss / 10 << 4 | ss % 10);
        raw[14] = (unsigned char)(ff / 10 << 4 | ff % 10);
        return 1;
    }
    int ok = LainCD_ReadImageSector(lba, raw);
    if (!ok) return ok;
    auto it = g_overlay->sectors[d].find(lba);
    if (it != g_overlay->sectors[d].end()) {
        memcpy(raw + 24, it->second.data(), kSector); /* Mode 2 Form 1 user data */
    }
    return ok;
}

std::vector<ModActive> g_active;

} // namespace

const std::vector<ModActive> &mods_active() {
    return g_active;
}

std::string mods_dir() {
    fs::path p = fs::u8path(settings_data_dir()) / "mods";
    std::error_code ec;
    fs::create_directories(p, ec);
    return p.u8string();
}

std::vector<ModInfo> mods_scan(const char *setting) {
    std::vector<ModInfo> out;
    std::error_code ec;
    std::vector<std::string> folders;
    for (auto &e : fs::directory_iterator(fs::u8path(mods_dir()), ec)) {
        std::string n = e.path().filename().u8string();
        if (e.is_directory(ec) && !n.empty() && n[0] != '_' && n[0] != '.' && n.find('|') == std::string::npos) {
            folders.push_back(n);
        }
    }
    std::sort(folders.begin(), folders.end());
    std::vector<std::pair<std::string, bool>> order;
    std::string s = setting ? setting : "";
    for (size_t pos = 0; pos < s.size();) {
        size_t bar = s.find('|', pos);
        std::string tok = s.substr(pos, bar == std::string::npos ? std::string::npos : bar - pos);
        pos = bar == std::string::npos ? s.size() : bar + 1;
        if (tok.size() > 1 && (tok[0] == '+' || tok[0] == '-')) order.push_back({tok.substr(1), tok[0] == '+'});
    }
    for (auto &o : order) {
        if (std::find(folders.begin(), folders.end(), o.first) != folders.end()) {
            ModInfo m = read_mod(fs::u8path(mods_dir()) / fs::u8path(o.first));
            m.enabled = o.second;
            out.push_back(m);
        }
    }
    for (const std::string &f : folders) {
        bool known = std::any_of(order.begin(), order.end(), [&](auto &o) { return o.first == f; });
        if (!known) out.push_back(read_mod(fs::u8path(mods_dir()) / fs::u8path(f)));
    }
    return out;
}

void mods_validate(std::vector<ModInfo> &mods, const char *disc1, const char *disc2) {
    Context ctx;
    bool ok = ctx.open(disc1, disc2);
    std::map<std::pair<std::string, int>, std::pair<size_t, size_t>> winner;
    for (size_t mi = 0; mi < mods.size(); mi++) {
        ModInfo &m = mods[mi];
        for (size_t fi = 0; fi < m.files.size(); fi++) {
            ModFile &f = m.files[fi];
            if (f.level == 2) continue;
            if (!ok) {
                f.level = 2;
                f.note = ctx.error;
                continue;
            }
            if (f.archive == "DATA") {
                f.level = moddata_file((fs::u8path(mods_dir()) / fs::u8path(m.folder) / fs::u8path(f.path)).u8string(),
                                       false, &ctx.exe, f.note);
                continue;
            }
            const Archive *a = find_archive(f.archive);
            std::string ext = lower(f.path.substr(f.path.rfind('.') + 1));
            std::vector<uint8_t> stored;
            const fs::path file = fs::u8path(mods_dir()) / fs::u8path(m.folder) / fs::u8path(f.path);
            if (f.archive == "DISC") {
                uint32_t lba, size;
                const std::string disc_name = upper(f.path);
                bool on1 = find_disc_file(ctx.disc[0], disc_name, lba, size), on2 = find_disc_file(ctx.disc[1], disc_name, lba, size);
                if (!on1 && !on2) {
                    f.level = 2;
                    f.note = "no " + disc_name + " on your imported discs";
                    continue;
                }
                Stream st;
                f.level = load_stream(file, lower(f.path).find(".xa") != std::string::npos, st, f.note);
                continue;
            }
            if (f.archive == "PROGRAM") {
                uint32_t addr;
                f.level = build_program(ctx, f.path, file, addr, stored, f.note);
                f.entry = (int)addr; /* one per variable, for overrides below */
            } else {
                f.level = build(ctx, *a, f.entry, file, ext, stored, f.note);
            }
            if (m.enabled && f.level < 2) {
                auto key = std::make_pair(f.archive, f.entry);
                auto it = winner.find(key);
                if (it != winner.end()) {
                    ModFile &old = mods[it->second.first].files[it->second.second];
                    old.level = std::max(old.level, 1);
                    old.note = "replaced by " + m.name + " (later in the list)";
                }
                winner[key] = {mi, fi};
            }
        }
    }
    for (ModInfo &m : mods) {
        m.errors = m.warnings = 0;
        for (const ModFile &f : m.files) {
            m.errors += f.level == 2;
            m.warnings += f.level == 1;
        }
    }
}

std::string mods_setting_string(const std::vector<ModInfo> &mods) {
    std::string s;
    for (const ModInfo &m : mods) {
        if (!s.empty()) s += '|';
        s += (m.enabled ? '+' : '-') + m.folder;
    }
    return s;
}

bool mods_create(const std::string &name, std::string &folder, std::string &err) {
    std::string f;
    for (char c : name) {
        if (strchr("/\\:*?\"<>|", c) || (unsigned char)c < 0x20) continue;
        f += c;
    }
    while (!f.empty() && (f.back() == ' ' || f.back() == '.')) f.pop_back();
    while (!f.empty() && (f[0] == ' ' || f[0] == '.' || f[0] == '_')) f.erase(0, 1);
    if (f.empty()) {
        err = "enter a name";
        return false;
    }
    fs::path dir = fs::u8path(mods_dir()) / fs::u8path(f);
    std::error_code ec;
    if (fs::exists(dir, ec)) {
        err = "a mod folder with that name already exists";
        return false;
    }
    fs::create_directories(dir, ec);
    std::string ini = "; Shown in the launcher's Mods tab\nname = " + name +
                      "\nauthor = \nversion = 1.0\ndescription = \n\n"
                      "; Replacement files go in folders named after the disc archive,\n"
                      "; named after the entry: SITEA.BIN/0631.png, BIN.BIN/0020.txt.\n"
                      "; Export originals (Mods tab) writes every entry to start from.\n";
    if (ec || !write_file(dir / "mod.ini", ini.data(), ini.size())) {
        err = "can't create the folder";
        return false;
    }
    folder = f;
    return true;
}

extern "C" int mods_apply(const char *setting, const char *disc1, const char *disc2) {
    std::vector<ModInfo> mods = mods_scan(setting);
    if (std::none_of(mods.begin(), mods.end(), [](const ModInfo &m) { return m.enabled && !m.files.empty(); })) {
        return 0;
    }
    Context ctx;
    if (!ctx.open(disc1, disc2)) {
        printf("[mods] not applied: %s\n", ctx.error.c_str());
        return 0;
    }
    struct Rep {
        const Archive *a;
        int entry;
        std::vector<uint8_t> stored;
        size_t mod;
    };
    std::vector<int> ok_files(mods.size(), 0), bad_files(mods.size(), 0);
    std::map<std::pair<std::string, int>, Rep> reps;
    std::map<std::string, std::pair<size_t, fs::path>> disc_files; /* "MOVIE/F001.STR" -> mod, file */
    for (size_t mi = 0; mi < mods.size(); mi++) {
        const ModInfo &m = mods[mi];
        if (!m.enabled) continue;
        int &used = ok_files[mi], &bad = bad_files[mi];
        for (const ModFile &f : m.files) {
            if (f.archive == "DATA") continue; /* mods_apply_data */
            if (f.archive == "DISC") {
                if (f.level == 2) {
                    bad++;
                    continue;
                }
                disc_files[upper(f.path)] = {mi, fs::u8path(mods_dir()) / fs::u8path(m.folder) / fs::u8path(f.path)};
                used++;
                continue;
            }
            std::string note;
            std::vector<uint8_t> stored;
            const Archive *a = find_archive(f.archive);
            const fs::path file = fs::u8path(mods_dir()) / fs::u8path(m.folder) / fs::u8path(f.path);
            uint32_t addr = 0;
            int level = f.level == 2 ? 2 :
                        f.archive == "PROGRAM" ? build_program(ctx, f.path, file, addr, stored, note) :
                        build(ctx, *a, f.entry, file, lower(f.path.substr(f.path.rfind('.') + 1)), stored, note);
            if (level == 2 || stored.empty()) {
                printf("[mods] %s: %s skipped: %s\n", m.name.c_str(), f.path.c_str(),
                       (f.level == 2 ? f.note : note).c_str());
                bad++;
                continue;
            }
            reps[{f.archive, a ? f.entry : (int)addr}] = Rep{a, a ? f.entry : (int)addr, std::move(stored), mi};
            used++;
        }
    }
    /* whole movie and voice files: placed past the end of the disc, the game's
     * file tables pointed there */
    static Overlay overlay;
    std::vector<size_t> disc_file_mods;
    for (auto &kv : disc_files) {
        Stream st;
        std::string note;
        const bool is_xa = kv.first.find(".XA") != std::string::npos;
        if (load_stream(kv.second.second, is_xa, st, note) == 2) {
            printf("[mods] %s skipped: %s\n", kv.first.c_str(), note.c_str());
            bad_files[kv.second.first]++;
            ok_files[kv.second.first]--;
            continue;
        }
        auto shared = std::make_shared<std::vector<uint8_t>>(std::move(st.raw));
        for (int d = 0; d < 2; d++) {
            uint32_t lba, size;
            if (!find_disc_file(ctx.disc[d], kv.first, lba, size)) continue;
            const uint32_t at = overlay.next_virtual[d];
            overlay.next_virtual[d] += st.sectors + 150; /* a gap like between files */
            for (uint32_t i = 0; i < st.sectors; i++) overlay.raw[d][(int)(at + i)] = {shared, (size_t)i * 2352};
            /* the game's tables: disc 1 (69 files), disc 2 (57), and the working copy */
            const uint32_t tables[2] = {d == 0 ? 0x80071678u : 0x800718A0u, 0x80071358u};
            const int counts[2] = {d == 0 ? 69 : 57, 100};
            int index = -1;
            for (int tb = 0; tb < 2; tb++) {
                if (tb == 1 && d != 0) break; /* the working copy starts as disc 1's */
                for (int i = 0; i < counts[tb]; i++) {
                    uint8_t *e = (uint8_t *)PSX_PTR(tables[tb] + (uint32_t)i * 8);
                    if (rd32(e) != lba) continue;
                    uint32_t nsize = st.sectors * 2336;
                    for (int k = 0; k < 4; k++) {
                        e[k] = (uint8_t)(at >> (8 * k));
                        e[4 + k] = (uint8_t)(nsize >> (8 * k));
                    }
                    if (tb == 0) index = i;
                }
            }
            const std::string base = kv.first.substr(kv.first.find('/') + 1);
            tracks_move(d, base.c_str(), at, st.sectors);
            /* a movie's length in frames: the media of this disc's site that play it */
            if (!is_xa && index >= 0) {
                std::set<int> media_of_site;
                for (int n = 0; n < 0x2CC; n++) {
                    const uint8_t *node = (const uint8_t *)PSX_PTR(0x80073F98u + (uint32_t)n * 0x28);
                    if (((node[0x1F] >> 7) & 1) == d) media_of_site.insert((int16_t)(node[0x1C] | node[0x1D] << 8));
                }
                /* and the media the site plays without a node */
                for (int media = d == 0 ? 0x2C5 : 0x2D4; media <= (d == 0 ? 0x2DA : 0x2DC); media++) media_of_site.insert(media);
                for (int media : media_of_site) {
                    if (media < 0 || media >= 0x2E0) continue;
                    uint8_t *me = (uint8_t *)PSX_PTR(0x80071A68u + (uint32_t)media * 8);
                    if (me[0] == 0 && (int16_t)(me[2] | me[3] << 8) == index) {
                        for (int k = 0; k < 4; k++) me[4 + k] = (uint8_t)(st.frames >> (8 * k));
                    }
                }
            }
            printf("[mods] %s on disc %d: %u sectors%s\n", kv.first.c_str(), d + 1, st.sectors,
                   is_xa ? "" : (", " + std::to_string(st.frames) + " frames").c_str());
        }
        disc_file_mods.push_back(kv.second.first);
    }
    /* what each mod ended up providing, after later mods override earlier ones */
    for (size_t mi = 0; mi < mods.size(); mi++) {
        if (!mods[mi].enabled) continue;
        ModActive act;
        act.name = mods[mi].name;
        act.version = mods[mi].version;
        for (auto &kv : reps) act.files += kv.second.mod == mi;
        for (size_t dm : disc_file_mods) act.files += dm == mi;
        act.overridden = ok_files[mi] - act.files;
        act.skipped = bad_files[mi];
        if (act.files || act.overridden || act.skipped) {
            printf("[mods] %s: %d file(s) used", act.name.c_str(), act.files);
            if (act.overridden) printf(", %d replaced by later mods", act.overridden);
            if (act.skipped) printf(", %d skipped", act.skipped);
            printf("\n");
        }
        g_active.push_back(act);
    }
    overlay.disc2_path = disc2 ? disc2 : "";
    for (auto &kv : reps) {
        const Rep &r = kv.second;
        if (!r.a) { /* built into the program: straight into memory */
            memcpy(PSX_PTR((uint32_t)r.entry), r.stored.data(), r.stored.size());
            continue;
        }
        uint32_t sector, size;
        if (!ctx.entry_loc(*r.a, r.entry, sector, size)) continue;
        for (int d = 0; d < 2; d++) {
            uint32_t lba;
            if (!r.a->on_disc[d] || !ctx.archive_lba(d, *r.a, lba)) continue;
            for (size_t off = 0; off < r.stored.size(); off += kSector) {
                std::array<uint8_t, kSector> sec{};
                memcpy(sec.data(), &r.stored[off], std::min(kSector, r.stored.size() - off));
                overlay.sectors[d][(int)(lba + sector + off / kSector)] = sec;
            }
        }
        /* the game reads and unpacks exactly the new size */
        uint8_t *t = (uint8_t *)PSX_PTR(r.a->table + (uint32_t)r.entry * 8 + 4);
        uint32_t n = (uint32_t)r.stored.size();
        t[0] = (uint8_t)n;
        t[1] = (uint8_t)(n >> 8);
        t[2] = (uint8_t)(n >> 16);
        t[3] = (uint8_t)(n >> 24);
    }
    g_overlay = &overlay;
    LainCD_SetSectorReader(mods_reader, nullptr);
    if (!reps.empty() || !disc_file_mods.empty()) printf("[mods] %zu entries replaced\n", reps.size() + disc_file_mods.size());
    return (int)(reps.size() + disc_file_mods.size());
}

extern "C" int mods_check_cli(const char *setting, const char *disc1, const char *disc2) {
    std::vector<ModInfo> mods = mods_scan(setting);
    mods_validate(mods, disc1, disc2);
    int errors = 0;
    printf("mods folder: %s\n", mods_dir().c_str());
    if (mods.empty()) printf("no mods installed\n");
    for (const ModInfo &m : mods) {
        printf("\n%s %s%s%s (%s)\n", m.enabled ? "[on] " : "[off]", m.name.c_str(), m.version.empty() ? "" : " ",
               m.version.c_str(), m.folder.c_str());
        for (const ModFile &f : m.files) {
            std::string note = f.note;
            for (size_t p = note.find('\n'); p != std::string::npos; p = note.find('\n', p + 10)) note.replace(p, 1, "\n         ");
            printf("  %-6s %s%s%s\n", f.level == 2 ? "ERROR" : f.level == 1 ? "note" : "ok", f.path.c_str(),
                   note.empty() ? "" : ": ", note.c_str());
        }
        for (const std::string &g : m.ignored) printf("  ignored %s (not a replacement file)\n", g.c_str());
        if (m.enabled) errors += m.errors;
    }
    return errors ? 1 : 0;
}

extern "C" int mods_export_originals(const char *disc1, const char *disc2, const char *out_dir,
                                     volatile float *progress, char *msg, size_t msg_cap) {
    auto say = [&](const std::string &s) {
        if (msg && msg_cap) snprintf(msg, msg_cap, "%s", s.c_str());
    };
    Context ctx;
    if (!ctx.open(disc1, disc2)) {
        say(ctx.error);
        return -1;
    }
    int total = 0, done = 0, written = 0;
    for (const Archive &a : kArchives) {
        if (ctx.disc_for(a) >= 0) total += a.count;
    }
    fs::path root = fs::u8path(out_dir);
    std::error_code ec;
    fs::create_directories(root, ec);
    for (const Archive &a : kArchives) {
        if (ctx.disc_for(a) < 0) continue;
        fs::path dir = root / a.name;
        fs::create_directories(dir, ec);
        for (int i = 0; i < a.count; i++, done++) {
            if (progress) *progress = total ? (float)done / total : 1.0f;
            std::vector<uint8_t> e = ctx.entry(a, i);
            if (e.empty()) continue;
            if (napk_is_packed(e.data(), e.size())) {
                size_t n = 0;
                uint8_t *u = napk_unpack(e.data(), e.size(), &n);
                if (!u) continue;
                e.assign(u, u + n);
                free(u);
            }
            char base[16];
            snprintf(base, sizeof base, "%04d", i);
            TimInfo ti;
            bool ok;
            if (tim_parse(e.data(), e.size(), &ti)) {
                uint8_t *rgba = tim_to_rgba(e.data(), e.size(), &ti, 0);
                ok = rgba && png_save((dir / (std::string(base) + ".png")).u8string().c_str(), rgba, ti.w, ti.h) == 0;
                free(rgba);
            } else if (strcmp(a.name, "VOICE.BIN") == 0) {
                std::vector<uint8_t> w = pcm_to_wav(e.data(), e.size() & ~(size_t)1, kVoiceRate);
                ok = write_file(dir / (std::string(base) + ".wav"), w.data(), w.size());
            } else if (looks_like_text(e)) {
                ok = write_file(dir / (std::string(base) + ".txt"), e.data(), e.size());
            } else {
                ok = write_file(dir / (std::string(base) + ".bin"), e.data(), e.size());
            }
            written += ok;
        }
    }
    /* pictures and models built into the program, named after their variable */
    fs::path prog = root / "PROGRAM";
    fs::create_directories(prog, ec);
    for (int i = 0; i < lain_symbol_count; i++) {
        const LainSymbol &sy = lain_symbols[i];
        std::string n = sy.name;
        size_t off = (size_t)(sy.addr - kExeBase);
        if (sy.host || off >= ctx.exe.size() || n.size() < 5) continue;
        std::string tail = n.substr(n.size() - 4);
        if (tail == "_tim") {
            TimInfo ti;
            if (!tim_parse(&ctx.exe[off], ctx.exe.size() - off, &ti) || ti.start != 0) continue;
            uint8_t *rgba = tim_to_rgba(&ctx.exe[off], ctx.exe.size() - off, &ti, 0);
            written += rgba && png_save((prog / (n + ".png")).u8string().c_str(), rgba, ti.w, ti.h) == 0;
            free(rgba);
        } else if (tail == "_tmd" && sy.size >= 12 && off + sy.size <= ctx.exe.size() && rd32(&ctx.exe[off]) == 0x41) {
            written += write_file(prog / (n + ".tmd"), &ctx.exe[off], sy.size);
        }
    }
    const char *readme =
        "Every entry of the game's archives, from your own discs, as the files a mod uses.\n"
        "These are the originals to start from; this folder is not a mod and is never loaded.\n\n"
        "To change one: copy it into your mod with the same folder and number, edit it, and\n"
        "keep only the files you changed, e.g. mods/My Mod/SITEA.BIN/0631.png.\n\n"
        "PNG  pictures. Keep the width and height; colors are converted for the PS1.\n"
        "WAV  voice clips (VOICE.BIN), 16-bit mono at 7914 Hz; other formats are converted.\n"
        "TXT  text the game reads (plain ASCII).\n"
        "BIN  everything else, unpacked; edit with a hex editor.\n"
        "PROGRAM  pictures and 3D models built into the game program, named after their variable.\n\n"
        "A replacement can't be bigger than the original. See MODDING.md.\n";
    write_file(root / "README.txt", readme, strlen(readme));
    if (progress) *progress = 1.0f;
    say(std::to_string(written) + " files written");
    return written;
}
