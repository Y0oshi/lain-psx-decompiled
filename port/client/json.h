/* A small JSON reader for mod data files, with line numbers in errors. */
#pragma once

#include <string>
#include <utility>
#include <vector>

struct Json {
    enum Type { Null, Bool, Number, String, Array, Object } type = Null;
    bool b = false;
    double num = 0;
    std::string str;
    std::vector<Json> arr;
    std::vector<std::pair<std::string, Json>> obj; /* in file order */
    int line = 0;

    const Json *get(const std::string &key) const {
        for (const auto &kv : obj) {
            if (kv.first == key) return &kv.second;
        }
        return nullptr;
    }
    bool is_int() const {
        return type == Number && num == (double)(long long)num;
    }
};

/* Parses text; on failure returns false with "line N: reason" in err. */
bool json_parse(const std::string &text, Json &out, std::string &err);
