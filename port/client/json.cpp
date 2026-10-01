#include "json.h"

#include <stdlib.h>
#include <string.h>

namespace {

struct Parser {
    const std::string &s;
    size_t p = 0;
    int line = 1;
    std::string err;

    explicit Parser(const std::string &text) : s(text) {}

    bool fail(const std::string &why) {
        if (err.empty()) err = "line " + std::to_string(line) + ": " + why;
        return false;
    }

    void skip() {
        while (p < s.size()) {
            char c = s[p];
            if (c == '\n') {
                line++;
                p++;
            } else if (c == ' ' || c == '\t' || c == '\r') {
                p++;
            } else if (c == '/' && p + 1 < s.size() && s[p + 1] == '/') {
                /* comments are allowed: mod files are written by hand */
                while (p < s.size() && s[p] != '\n') p++;
            } else {
                break;
            }
        }
    }

    bool literal(const char *word) {
        size_t n = strlen(word);
        if (s.compare(p, n, word) != 0) return false;
        p += n;
        return true;
    }

    static void utf8(std::string &out, unsigned cp) {
        if (cp < 0x80) {
            out += (char)cp;
        } else if (cp < 0x800) {
            out += (char)(0xC0 | cp >> 6);
            out += (char)(0x80 | (cp & 0x3F));
        } else if (cp < 0x10000) {
            out += (char)(0xE0 | cp >> 12);
            out += (char)(0x80 | ((cp >> 6) & 0x3F));
            out += (char)(0x80 | (cp & 0x3F));
        } else {
            out += (char)(0xF0 | cp >> 18);
            out += (char)(0x80 | ((cp >> 12) & 0x3F));
            out += (char)(0x80 | ((cp >> 6) & 0x3F));
            out += (char)(0x80 | (cp & 0x3F));
        }
    }

    bool hex4(unsigned &v) {
        if (p + 4 > s.size()) return false;
        v = 0;
        for (int i = 0; i < 4; i++) {
            char c = s[p++];
            v <<= 4;
            if (c >= '0' && c <= '9') v |= (unsigned)(c - '0');
            else if (c >= 'a' && c <= 'f') v |= (unsigned)(c - 'a' + 10);
            else if (c >= 'A' && c <= 'F') v |= (unsigned)(c - 'A' + 10);
            else return false;
        }
        return true;
    }

    bool string(std::string &out) {
        p++; /* opening quote */
        while (p < s.size() && s[p] != '"') {
            char c = s[p++];
            if (c == '\n') return fail("line break inside a string");
            if (c != '\\') {
                out += c;
                continue;
            }
            if (p >= s.size()) break;
            char e = s[p++];
            switch (e) {
            case '"': out += '"'; break;
            case '\\': out += '\\'; break;
            case '/': out += '/'; break;
            case 'b': out += '\b'; break;
            case 'f': out += '\f'; break;
            case 'n': out += '\n'; break;
            case 'r': out += '\r'; break;
            case 't': out += '\t'; break;
            case 'u': {
                unsigned cp;
                if (!hex4(cp)) return fail("bad \\u escape");
                if (cp >= 0xD800 && cp < 0xDC00 && s.compare(p, 2, "\\u") == 0) {
                    p += 2;
                    unsigned lo;
                    if (!hex4(lo) || lo < 0xDC00 || lo >= 0xE000) return fail("bad surrogate pair");
                    cp = 0x10000 + ((cp - 0xD800) << 10) + (lo - 0xDC00);
                }
                utf8(out, cp);
                break;
            }
            default: return fail(std::string("unknown escape \\") + e);
            }
        }
        if (p >= s.size()) return fail("string not closed");
        p++;
        return true;
    }

    bool value(Json &v, int depth) {
        if (depth > 64) return fail("nested too deep");
        skip();
        v.line = line;
        if (p >= s.size()) return fail("value missing");
        char c = s[p];
        if (c == '{') {
            v.type = Json::Object;
            p++;
            skip();
            if (p < s.size() && s[p] == '}') {
                p++;
                return true;
            }
            for (;;) {
                skip();
                if (p >= s.size() || s[p] != '"') return fail("expected a \"key\"");
                std::string key;
                if (!string(key)) return false;
                skip();
                if (p >= s.size() || s[p] != ':') return fail("expected ':' after \"" + key + "\"");
                p++;
                Json item;
                if (!value(item, depth + 1)) return false;
                v.obj.emplace_back(key, std::move(item));
                skip();
                if (p < s.size() && s[p] == ',') {
                    p++;
                    skip();
                    if (p < s.size() && s[p] == '}') { /* trailing comma */
                        p++;
                        return true;
                    }
                    continue;
                }
                if (p < s.size() && s[p] == '}') {
                    p++;
                    return true;
                }
                return fail("expected ',' or '}'");
            }
        }
        if (c == '[') {
            v.type = Json::Array;
            p++;
            skip();
            if (p < s.size() && s[p] == ']') {
                p++;
                return true;
            }
            for (;;) {
                Json item;
                if (!value(item, depth + 1)) return false;
                v.arr.push_back(std::move(item));
                skip();
                if (p < s.size() && s[p] == ',') {
                    p++;
                    skip();
                    if (p < s.size() && s[p] == ']') {
                        p++;
                        return true;
                    }
                    continue;
                }
                if (p < s.size() && s[p] == ']') {
                    p++;
                    return true;
                }
                return fail("expected ',' or ']'");
            }
        }
        if (c == '"') {
            v.type = Json::String;
            return string(v.str);
        }
        if (literal("true")) {
            v.type = Json::Bool;
            v.b = true;
            return true;
        }
        if (literal("false")) {
            v.type = Json::Bool;
            return true;
        }
        if (literal("null")) return true;
        if (c == '-' || (c >= '0' && c <= '9')) {
            const char *start = s.c_str() + p;
            char *end = nullptr;
            if (s.compare(p, 2, "0x") == 0 || s.compare(p, 3, "-0x") == 0) {
                v.num = (double)strtoll(start, &end, 16); /* hex addresses */
            } else {
                v.num = strtod(start, &end);
            }
            if (end == start) return fail("bad number");
            p += (size_t)(end - start);
            v.type = Json::Number;
            return true;
        }
        return fail(std::string("unexpected '") + c + "'");
    }
};

} // namespace

bool json_parse(const std::string &text, Json &out, std::string &err) {
    Parser ps(text);
    if (text.size() >= 3 && (unsigned char)text[0] == 0xEF && (unsigned char)text[1] == 0xBB && (unsigned char)text[2] == 0xBF) ps.p = 3; /* UTF-8 BOM */
    out = Json();
    if (!ps.value(out, 0)) {
        err = ps.err;
        return false;
    }
    ps.skip();
    if (ps.p != text.size()) {
        err = "line " + std::to_string(ps.line) + ": extra text after the end";
        return false;
    }
    return true;
}
