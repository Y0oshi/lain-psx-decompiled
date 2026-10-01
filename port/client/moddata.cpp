#include "moddata.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <algorithm>
#include <functional>
#include <map>

#include "game_hooks.h"
#include "json.h"
#include "modrt.h"
#include "mods.h"
#include "psx_arena.h"

/* The host string tables (game/PORT_TYPES.md): native pointers. */
extern "C" char *g_node_text_strings[417];
extern "C" char *g_node_label_strings[51];

namespace {

/* the PS-X EXE's 0x800-byte header sits before its text at 0x80010000 */
constexpr uint32_t kExeBase = 0x80010000u - 0x800u;
constexpr uint32_t kNodeTable = 0x80073F98u, kNodeSize = 0x28;
constexpr int kNodes = 0x2CC;
constexpr uint32_t kMediaTable = 0x80071A68u;
constexpr int kMedia = 0x2E0;
constexpr uint32_t kTextTable = 0x80073848u, kLabelTable = 0x80073ECCu;
constexpr int kTexts = 417, kLabels = 51, kLapks = 59;

uint32_t rd32(const uint8_t *p) {
    return p ? (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24 : 0;
}

/* Where the file is checked or applied: the program image from the disc, or
 * the running game's memory. */
struct Target {
    bool apply;
    const std::vector<uint8_t> *exe;
    std::vector<uint8_t> scratch; /* a writable copy of exe when checking */
    std::vector<std::string> texts, labels;

    uint8_t *ram(uint32_t addr, uint32_t n) {
        if (addr < PSX_RAM_BASE || (size_t)(addr - PSX_RAM_BASE) + n > PSX_ARENA_SIZE) return nullptr;
        const size_t off = addr - PSX_RAM_BASE;
        if (apply) return psx_arena + off;
        /* all of PS1 RAM, with the program where it loads (the rest reads as zero) */
        if (scratch.empty()) {
            scratch.assign(PSX_ARENA_SIZE, 0);
            size_t at = kExeBase - PSX_RAM_BASE;
            memcpy(&scratch[at], exe->data(), std::min(exe->size(), scratch.size() - at));
        }
        return &scratch[off];
    }

    std::string c_string(uint32_t ps1) {
        const uint8_t *p = ram(ps1, 1);
        std::string s;
        while (p && *p && s.size() < 256) {
            s += (char)*p++;
            if (!ram(ps1 + (uint32_t)s.size(), 1)) break;
        }
        return s;
    }

    void load_strings() {
        if (!texts.empty()) return;
        for (int i = 0; i < kTexts; i++) {
            if (apply) texts.push_back(g_node_text_strings[i] ? g_node_text_strings[i] : "");
            else texts.push_back(c_string(rd32(ram(kTextTable + i * 4, 4))));
        }
        for (int i = 0; i < kLabels; i++) {
            if (apply) labels.push_back(g_node_label_strings[i] ? g_node_label_strings[i] : "");
            else labels.push_back(c_string(rd32(ram(kLabelTable + i * 4, 4))));
        }
    }

    void set_string(bool label, int i, const std::string &s) {
        load_strings();
        (label ? labels : texts)[i] = s;
        if (apply) {
            /* a new native string; the old one stays (it may be in the program image) */
            char *copy = strdup(s.c_str());
            (label ? g_node_label_strings : g_node_text_strings)[i] = copy;
        }
    }

};

struct Report {
    std::string text;
    int level = 0;
    void error(const Json &at, const std::string &m) {
        text += "line " + std::to_string(at.line) + ": " + m + "\n";
        level = 2;
    }
    void note(const Json &at, const std::string &m) {
        text += "line " + std::to_string(at.line) + ": " + m + "\n";
        if (level < 1) level = 1;
    }
};

void wr16(uint8_t *p, uint32_t v) {
    p[0] = (uint8_t)v;
    p[1] = (uint8_t)(v >> 8);
}

void wr32(uint8_t *p, uint32_t v) {
    for (int k = 0; k < 4; k++) p[k] = (uint8_t)(v >> (8 * k));
}

bool get_int(const Json &j, long long &out) {
    if (!j.is_int()) return false;
    out = (long long)j.num;
    return true;
}

/* A node given by name ("Lda112") or index. */
int node_index(Target &t, const Json &j) {
    long long n;
    if (get_int(j, n)) return n >= 0 && n < kNodes ? (int)n : -1;
    if (j.type != Json::String) return -1;
    for (int i = 0; i < kNodes; i++) {
        const uint8_t *p = t.ram(kNodeTable + i * kNodeSize, 8);
        if (p && strncmp((const char *)p, j.str.c_str(), 8) == 0 && j.str.size() <= 7) return i;
    }
    return -1;
}

/* A string given as an index or as the text itself (it must exist in the table). */
int string_index(Target &t, bool label, const Json &j, Report &r) {
    long long n;
    int count = label ? kLabels : kTexts;
    if (get_int(j, n)) {
        if (n == -1 && !label) return -1;
        if (n < 0 || n >= count) {
            r.error(j, (label ? "label " : "text ") + std::to_string(n) + " doesn't exist (0 to " +
                           std::to_string(count - 1) + ")");
            return -2;
        }
        return (int)n;
    }
    if (j.type == Json::Null && !label) return -1;
    if (j.type == Json::String) {
        t.load_strings();
        const auto &list = label ? t.labels : t.texts;
        /* the game shows '_' as a space: either matches */
        auto same = [](const std::string &a, const std::string &b, bool any_case) {
            if (a.size() != b.size()) return false;
            for (size_t k = 0; k < a.size(); k++) {
                char x = a[k] == '_' ? ' ' : a[k], y = b[k] == '_' ? ' ' : b[k];
                if (any_case) {
                    x = (char)tolower((unsigned char)x);
                    y = (char)tolower((unsigned char)y);
                }
                if (x != y) return false;
            }
            return true;
        };
        for (int i = 0; i < count; i++) {
            if (same(list[i], j.str, false)) return i;
        }
        std::string hint;
        for (int i = 0; i < count && hint.empty(); i++) {
            if (same(list[i], j.str, true)) hint = " (did you mean \"" + list[i] + "\", number " + std::to_string(i) + "?)";
        }
        r.error(j, std::string("no ") + (label ? "label" : "text") + " reads \"" + j.str + "\"" + hint +
                       "; strings.json changes an existing one");
        return -2;
    }
    r.error(j, "expected a number or a text");
    return -2;
}

int kind_value(const Json &j) {
    static const std::map<std::string, int> names = {
        {"lda", 0}, {"env", 0}, {"tda", 2}, {"cou", 3}, {"dc", 4},   {"movie", 4}, {"dia", 5},
        {"eda", 6}, {"ekm", 6}, {"ere", 6}, {"tak", 6}, {"sskn", 7}, {"gate", 8},  {"polytan", 9}};
    long long n;
    if (get_int(j, n)) return n >= 0 && n <= 11 ? (int)n : -1;
    if (j.type == Json::String) {
        std::string l;
        for (char c : j.str) l += (char)tolower((unsigned char)c);
        auto it = names.find(l);
        if (it != names.end()) return it->second;
    }
    return -1;
}

/* Node fields the game keeps in saves; a save load puts back these values for
 * the nodes mods changed (see the save hook below). */
constexpr uint32_t kStaticFlags = 0xFAFF0000u; /* unlock level, kind, initially visible, ending, depth */
struct NodeFix {
    uint32_t flags;
    uint16_t parent;
};
std::map<int, NodeFix> g_node_fixes;

void nodes_file(Target &t, const Json &root, Report &r) {
    const Json *list = root.get("nodes");
    if (!list || list->type != Json::Array) {
        r.error(root, "expected {\"nodes\": [ ... ]}");
        return;
    }
    for (const Json &e : list->arr) {
        if (e.type != Json::Object) {
            r.error(e, "each node change is an object {\"node\": ..., ...}");
            continue;
        }
        const Json *which = e.get("node");
        int ni = which ? node_index(t, *which) : -1;
        if (ni < 0) {
            r.error(which ? *which : e, !which ? "\"node\" (name or number) is missing" :
                                        which->type == Json::String ? "no node named \"" + which->str + "\"" :
                                                                      "no node number " + std::to_string((long long)which->num) + " (0 to 715)");
            continue;
        }
        uint8_t *n = t.ram(kNodeTable + ni * kNodeSize, kNodeSize);
        uint32_t flags = rd32(n + 0x20);
        uint16_t pos = (uint16_t)(n[0x1E] | n[0x1F] << 8);
        bool placed = false;
        long long v;
        for (const auto &kv : e.obj) {
            const std::string &k = kv.first;
            const Json &val = kv.second;
            if (k == "node") continue;
            if (k == "name") {
                if (val.type != Json::String || val.str.empty() || val.str.size() > 7) {
                    r.error(val, "\"name\" must be 1 to 7 characters");
                    continue;
                }
                memset(n, 0, 8);
                memcpy(n, val.str.data(), val.str.size());
            } else if (k == "lines" || k == "keywords" || k == "slideshow") {
                const size_t want = k == "lines" ? 4 : 3;
                const uint32_t off = k == "lines" ? 0x08 : k == "keywords" ? 0x10 : 0x16;
                if (val.type != Json::Array || val.arr.size() != want) {
                    r.error(val, "\"" + k + "\" needs " + std::to_string(want) + " values");
                    continue;
                }
                for (size_t i = 0; i < want; i++) {
                    int idx;
                    if (k == "slideshow") {
                        if (!get_int(val.arr[i], v) || v < -1 || v > 789) {
                            r.error(val.arr[i], "a slideshow picture is an entry of SITEA.BIN/SITEB.BIN, or -1");
                            continue;
                        }
                        idx = (int)v;
                    } else {
                        idx = string_index(t, k == "lines", val.arr[i], r);
                        if (idx == -2) continue;
                    }
                    wr16(n + off + i * 2, (uint16_t)(int16_t)idx);
                }
            } else if (k == "media") {
                int mi = -1;
                if (get_int(val, v)) mi = v >= 0 && v < kMedia ? (int)v : -1;
                else if (val.type == Json::String) { /* another node's media */
                    int other = node_index(t, val);
                    if (other >= 0) {
                        const uint8_t *om = t.ram(kNodeTable + other * kNodeSize + 0x1C, 2);
                        mi = (int16_t)(om[0] | om[1] << 8);
                    }
                }
                if (mi < 0) {
                    r.error(val, "\"media\" is a media number (0 to 735) or a node name");
                    continue;
                }
                wr16(n + 0x1C, (uint16_t)mi);
                if (mi != ni) r.note(val, "the game marks node " + std::to_string(mi) + " seen when this plays (it indexes nodes by media)");
            } else if (k == "site" || k == "level" || k == "row" || k == "column") {
                placed = true; /* combined below */
            } else if (k == "hidden_from_map") {
                if (val.type != Json::Bool) { r.error(val, "\"hidden_from_map\" is true or false"); continue; }
                if (val.b) pos &= 0x8007; /* cell 0: never placed */
            } else if (k == "unlock_level") {
                if (!get_int(val, v) || v < 0 || v > 15) { r.error(val, "\"unlock_level\" is 0 to 15 (the game's levels are 0 to 5)"); continue; }
                flags = (flags & ~0x000F0000u) | (uint32_t)v << 16;
            } else if (k == "kind") {
                int kd = kind_value(val);
                if (kd < 0) { r.error(val, "\"kind\" is 0 to 11 or lda, tda, cou, dc, dia, eda, sskn, gate, polytan"); continue; }
                flags = (flags & ~0x00F00000u) | (uint32_t)kd << 20;
            } else if (k == "visible") {
                if (val.type != Json::Bool) { r.error(val, "\"visible\" is true or false"); continue; }
                flags = val.b ? flags | 0x03000000u : flags & ~0x03000000u;
            } else if (k == "ending") {
                if (val.type != Json::Bool) { r.error(val, "\"ending\" is true or false"); continue; }
                flags = val.b ? flags | 0x08000000u : flags & ~0x08000000u;
            } else if (k == "depth") {
                if (!get_int(val, v) || v < 0 || v > 15) { r.error(val, "\"depth\" is 0 to 15"); continue; }
                flags = (flags & ~0xF0000000u) | (uint32_t)v << 28;
            } else if (k == "parent") {
                int pi = val.type == Json::Null ? 0xFFFF : node_index(t, val);
                if (pi < 0) { r.error(val, "\"parent\" is a node name, number or null"); continue; }
                wr16(n + 0x24, (uint16_t)pi);
            } else {
                r.error(val, "unknown node field \"" + k + "\"");
            }
        }
        if (placed) {
            bool ok = true; /* a bad field keeps the old place; the other fields still apply */
            const Json *js = e.get("site"), *jl = e.get("level"), *jr = e.get("row"), *jc = e.get("column");
            int site = (pos >> 15) & 1, cell = (pos >> 3) & 0xFFF;
            int level = cell ? (cell - 1) / 3 : 0, row = cell ? (cell - 1) % 3 : 0, col = pos & 7;
            if (js) {
                if (js->type == Json::String && (js->str == "A" || js->str == "a")) site = 0;
                else if (js->type == Json::String && (js->str == "B" || js->str == "b")) site = 1;
                else { r.error(*js, "\"site\" is \"A\" or \"B\""); ok = false; }
            }
            if (jl && (!get_int(*jl, v) || v < 0 || v >= (site ? 13 : 22))) { r.error(*jl, std::string("\"level\" is 0 to ") + (site ? "12 on site B" : "21 on site A")); ok = false; }
            else if (jl) level = (int)v;
            if (jr && (!get_int(*jr, v) || v < 0 || v > 2)) { r.error(*jr, "\"row\" is 0 to 2 (the three rows of a level)"); ok = false; }
            else if (jr) row = (int)v;
            if (jc && (!get_int(*jc, v) || v < 0 || v > 7)) { r.error(*jc, "\"column\" is 0 to 7"); ok = false; }
            else if (jc) col = (int)v;
            if (ok) pos = (uint16_t)(col | (level * 3 + row + 1) << 3 | site << 15);
        }
        wr16(n + 0x1E, pos);
        wr32(n + 0x20, flags);
        if (t.apply) {
            g_node_fixes[ni] = NodeFix{flags & kStaticFlags, (uint16_t)(n[0x24] | n[0x25] << 8)};
        }
    }
}

void strings_file(Target &t, const Json &root, Report &r) {
    for (const auto &kv : root.obj) {
        bool label = kv.first == "labels";
        if (kv.first != "text" && !label) {
            r.error(kv.second, "strings.json has \"text\" and \"labels\"");
            continue;
        }
        if (kv.second.type != Json::Object) {
            r.error(kv.second, "\"" + kv.first + "\" is an object: {\"number or old text\": \"new text\"}");
            continue;
        }
        for (const auto &e : kv.second.obj) {
            Json key;
            key.line = e.second.line;
            char *end = nullptr;
            long idx = strtol(e.first.c_str(), &end, 10);
            if (end && *end == 0 && !e.first.empty()) {
                key.type = Json::Number;
                key.num = (double)idx;
            } else {
                key.type = Json::String;
                key.str = e.first;
            }
            int i = string_index(t, label, key, r);
            if (i < 0) continue;
            if (e.second.type != Json::String) {
                r.error(e.second, "the new text is a string");
                continue;
            }
            const std::string &s = e.second.str;
            for (unsigned char c : s) {
                if (c >= 0x80) {
                    r.note(e.second, "non-ASCII characters aren't drawn by the game's font");
                    break;
                }
            }
            size_t max = label ? 15 : 17;
            if (s.size() > max) r.note(e.second, "longer than the " + std::to_string(max) + " characters that fit on screen");
            t.set_string(label, i, s);
        }
    }
}

void media_file(Target &t, const Json &root, Report &r) {
    const Json *list = root.get("media");
    if (!list || list->type != Json::Array) {
        r.error(root, "expected {\"media\": [ ... ]}");
        return;
    }
    long long v;
    for (const Json &e : list->arr) {
        const Json *which = e.get("media");
        if (!which || !get_int(*which, v) || v < 0 || v >= kMedia) {
            r.error(which ? *which : e, "\"media\" is a media number, 0 to 735");
            continue;
        }
        uint8_t *m = t.ram(kMediaTable + (uint32_t)v * 8, 8);
        if (const Json *same = e.get("same_as")) {
            long long o;
            if (!get_int(*same, o) || o < 0 || o >= kMedia) { r.error(*same, "\"same_as\" is a media number"); continue; }
            memcpy(m, t.ram(kMediaTable + (uint32_t)o * 8, 8), 8);
        }
        for (const auto &kv : e.obj) {
            const std::string &k = kv.first;
            if (k == "media" || k == "same_as") continue;
            if (!get_int(kv.second, v)) { r.error(kv.second, "\"" + k + "\" is a number"); continue; }
            if (k == "xa_file") {
                if (v < 0 || v > 21) { r.error(kv.second, "\"xa_file\" is 0 (a movie) or 1 to 21"); continue; }
                m[0] = (uint8_t)v;
            } else if (k == "file" || k == "channel") {
                if (k == "channel" && v > 31) { r.error(kv.second, "\"channel\" is 0 to 31"); continue; }
                if (v < 0 || v > 99) { r.error(kv.second, "\"" + k + "\" is 0 to 99"); continue; }
                wr16(m + 2, (uint16_t)v);
            } else if (k == "size" || k == "frames") {
                if (v < 0) { r.error(kv.second, "\"" + k + "\" can't be negative"); continue; }
                wr32(m + 4, (uint32_t)v);
            } else {
                r.error(kv.second, "unknown media field \"" + k + "\" (xa_file, file, channel, size, frames, same_as)");
            }
        }
    }
}

/* Lain's animation tables: LAPKS.BIN entries (0 to 58). */
void lain_file(Target &t, const Json &root, Report &r) {
    struct Table {
        const char *key;
        uint32_t addr;
        int count, stride; /* values, bytes per value */
    };
    static const Table tables[] = {
        {"idle", 0x800735E8u, 38, 2},         {"move", 0x80073634u, 6, 4},
        {"move_sfx", 0x8007364Cu, 3, 4},      {"select", 0x80073658u, 20, 4},
        {"select_sfx", 0x800736A8u, 4, 4},    {"audio_node", 0x800736B8u, 6, 4},
        {"audio_node_sfx", 0x800736D0u, 6, 4}, {"level", 0x800A5FD8u, 2, 4},
        {"save", 0x800A5FE4u, 1, 4},          {"site_enter", 0x800A5FECu, 1, 4},
    };
    long long v;
    for (const auto &kv : root.obj) {
        const Table *tb = nullptr;
        for (const Table &x : tables) {
            if (kv.first == x.key) tb = &x;
        }
        if (!tb) {
            r.error(kv.second, "unknown table \"" + kv.first +
                                   "\" (idle, move, move_sfx, select, select_sfx, audio_node, audio_node_sfx, level, save, site_enter)");
            continue;
        }
        std::vector<const Json *> vals;
        std::function<void(const Json &)> flat = [&](const Json &j) {
            if (j.type == Json::Array) for (const Json &x : j.arr) flat(x);
            else vals.push_back(&j);
        };
        flat(kv.second);
        if ((int)vals.size() != tb->count) {
            r.error(kv.second, "\"" + kv.first + "\" has " + std::to_string(tb->count) + " values");
            continue;
        }
        bool sfx = strstr(tb->key, "sfx") != nullptr;
        for (int i = 0; i < tb->count; i++) {
            if (!get_int(*vals[i], v) || (!sfx && (v < 0 || v >= kLapks)) || (sfx && (v < -32768 || v > 32767))) {
                r.error(*vals[i], sfx ? "a sound number" : "an animation is a LAPKS.BIN entry, 0 to 58");
                continue;
            }
            uint8_t *p = t.ram(tb->addr + (uint32_t)(i * tb->stride), (uint32_t)tb->stride);
            if (tb->stride == 2 || sfx) wr16(p, (uint16_t)v);
            else wr32(p, (uint32_t)v);
        }
    }
}

std::vector<uint8_t> parse_hex(const std::string &s, bool &ok) {
    std::vector<uint8_t> out;
    int nib = -1;
    ok = true;
    for (char c : s) {
        int d = c >= '0' && c <= '9' ? c - '0' : c >= 'a' && c <= 'f' ? c - 'a' + 10 : c >= 'A' && c <= 'F' ? c - 'A' + 10 : -1;
        if (d < 0) {
            if (c != ' ' && c != '\n' && c != '\t') ok = false;
            continue;
        }
        if (nib < 0) nib = d;
        else {
            out.push_back((uint8_t)(nib << 4 | d));
            nib = -1;
        }
    }
    if (nib >= 0) ok = false;
    return out;
}

/* Raw changes to any game variable or address. */
void memory_file(Target &t, const Json &root, Report &r) {
    const Json *list = root.get("patches");
    if (!list || list->type != Json::Array) {
        r.error(root, "expected {\"patches\": [ ... ]}");
        return;
    }
    for (const Json &e : list->arr) {
        uint32_t addr = 0, size = 0;
        void *host = nullptr;
        const Json *sym = e.get("symbol"), *ad = e.get("address");
        long long v;
        if (sym && sym->type == Json::String) {
            int found = -1;
            for (int i = 0; i < lain_symbol_count && found < 0; i++) {
                if (sym->str == lain_symbols[i].name) found = i;
            }
            if (found < 0) { r.error(*sym, "no game variable named " + sym->str); continue; }
            addr = lain_symbols[found].addr;
            size = lain_symbols[found].size;
            host = lain_symbols[found].host;
        } else if (ad && (get_int(*ad, v) || (ad->type == Json::String && (v = strtoll(ad->str.c_str(), nullptr, 0)) != 0)) &&
                   v >= 0x80000000LL && v < 0x80200000LL) {
            addr = (uint32_t)v;
        } else {
            r.error(e, "a patch needs \"symbol\" (a game variable) or \"address\" (0x80000000 to 0x801FFFFF)");
            continue;
        }
        long long off = 0;
        if (const Json *o = e.get("offset")) {
            if (!get_int(*o, off) || off < 0 || off >= PSX_ARENA_SIZE) { r.error(*o, "\"offset\" is a byte count"); continue; }
        }
        std::vector<uint8_t> bytes;
        const Json *type = e.get("type"), *values = e.get("values"), *hex = e.get("bytes"), *str = e.get("string");
        if (hex && hex->type == Json::String) {
            bool ok;
            bytes = parse_hex(hex->str, ok);
            if (!ok) { r.error(*hex, "\"bytes\" is hex, e.g. \"00 1F A0\""); continue; }
        } else if (str && str->type == Json::String) {
            bytes.assign(str->str.begin(), str->str.end());
            bytes.push_back(0);
        } else if (type && values && type->type == Json::String) {
            int w = type->str == "u8" || type->str == "s8" ? 1 : type->str == "u16" || type->str == "s16" ? 2 :
                    type->str == "u32" || type->str == "s32" ? 4 : 0;
            if (!w) { r.error(*type, "\"type\" is u8, s8, u16, s16, u32 or s32"); continue; }
            std::vector<const Json *> vals;
            if (values->type == Json::Array) for (const Json &x : values->arr) vals.push_back(&x);
            else vals.push_back(values);
            bool bad = false;
            for (const Json *x : vals) {
                if (!get_int(*x, v)) { r.error(*x, "values are numbers"); bad = true; break; }
                for (int k = 0; k < w; k++) bytes.push_back((uint8_t)((uint64_t)v >> (8 * k)));
            }
            if (bad) continue;
        } else {
            r.error(e, "a patch needs \"bytes\", \"string\", or \"type\" with \"values\"");
            continue;
        }
        if (size && (uint64_t)off + bytes.size() > size) {
            r.note(e, "writes past the end of " + (sym ? sym->str : std::string("the variable")) + " (" +
                          std::to_string(size) + " bytes)");
        }
        if (host) {
            if (t.apply) memcpy((uint8_t *)host + off, bytes.data(), bytes.size());
            continue;
        }
        uint8_t *p = t.ram(addr + (uint32_t)off, (uint32_t)bytes.size());
        if (!p) { r.error(e, "outside the game's memory"); continue; }
        memcpy(p, bytes.data(), bytes.size());
    }
}

int32_t (*g_next_save_apply)(uint8_t *buf);

/* After a save loads: the node fields mods set, back in place of the save's. */
int32_t on_save_apply(uint8_t *buf) {
    int32_t r = g_next_save_apply(buf);
    for (const auto &kv : g_node_fixes) {
        uint8_t *n = psx_arena + (kNodeTable - PSX_RAM_BASE) + kv.first * kNodeSize;
        uint32_t flags = rd32(n + 0x20);
        /* visible and seen come from the save: they are the player's progress */
        wr32(n + 0x20, (flags & ~kStaticFlags) | kv.second.flags);
        wr16(n + 0x24, kv.second.parent);
    }
    return r;
}

} // namespace

int moddata_file(const std::string &path, bool apply, const std::vector<uint8_t> *exe, std::string &messages) {
    Report r;
    FILE *f = fopen(path.c_str(), "rb");
    std::string text;
    if (f) {
        char buf[65536];
        size_t n;
        while ((n = fread(buf, 1, sizeof buf, f)) > 0) text.append(buf, n);
        fclose(f);
    } else {
        messages = "can't read the file";
        return 2;
    }
    Json root;
    std::string err;
    if (!json_parse(text, root, err)) {
        messages = err;
        return 2;
    }
    if (root.type != Json::Object) {
        messages = "the file must be a JSON object { ... }";
        return 2;
    }
    Target t{apply, exe, {}, {}, {}};
    std::string name = path.substr(path.find_last_of("/\\") + 1);
    for (char &c : name) c = (char)tolower((unsigned char)c);
    if (name == "nodes.json") nodes_file(t, root, r);
    else if (name == "strings.json") strings_file(t, root, r);
    else if (name == "media.json") media_file(t, root, r);
    else if (name == "lain.json") lain_file(t, root, r);
    else if (name == "memory.json") memory_file(t, root, r);
    else {
        messages = "unknown data file (nodes, strings, media, lain or memory .json)";
        return 2;
    }
    messages = r.text;
    while (!messages.empty() && messages.back() == '\n') messages.pop_back();
    return r.level;
}

extern "C" void mods_apply_data(const char *mods_setting) {
    moddata_apply(mods_scan(mods_setting), mods_dir());
}

void moddata_apply(const std::vector<ModInfo> &mods, const std::string &dir) {
    for (const ModInfo &m : mods) {
        if (!m.enabled) continue;
        for (const std::string &rel : m.data) {
            std::string msg;
            int level = moddata_file(dir + "/" + m.folder + "/" + rel, true, nullptr, msg);
            printf("[mods] %s: %s %s%s%s\n", m.name.c_str(), rel.c_str(), level == 2 ? "has problems" : "applied",
                   msg.empty() ? "" : ":\n  ", msg.c_str());
        }
    }
    if (!g_node_fixes.empty()) {
        modrt_hook("save_apply_image", (void *)on_save_apply, (void **)&g_next_save_apply);
    }
}
