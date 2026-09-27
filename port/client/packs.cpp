#include "packs.h"

#include <stdio.h>

#include <algorithm>

#ifdef _WIN32
#include <windows.h>
#else
#include <dirent.h>
#include <sys/stat.h>
#endif

#include "settings.h"

namespace {

// Language packs are folders under <data dir>/<kind>/<code>/ (kind = "lang" or
// "dub"). Any folder there is offered; its pack.txt "name = ..." gives the label.
std::vector<std::string> list_subdirs(const std::string &dir) {
    std::vector<std::string> out;
#ifdef _WIN32
    WIN32_FIND_DATAA fd;
    HANDLE h = FindFirstFileA((dir + "\\*").c_str(), &fd);
    if (h != INVALID_HANDLE_VALUE) {
        do {
            if ((fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) && fd.cFileName[0] != '.') {
                out.push_back(fd.cFileName);
            }
        } while (FindNextFileA(h, &fd));
        FindClose(h);
    }
#else
    if (DIR *d = opendir(dir.c_str())) {
        while (struct dirent *e = readdir(d)) {
            std::string full = dir + "/" + e->d_name;
            struct stat st;
            if (e->d_name[0] != '.' && stat(full.c_str(), &st) == 0 && S_ISDIR(st.st_mode)) {
                out.push_back(e->d_name);
            }
        }
        closedir(d);
    }
#endif
    std::sort(out.begin(), out.end());
    return out;
}

std::string pack_label(const std::string &pack_dir, const std::string &code) {
    std::string label = code;
    FILE *f = fopen((pack_dir + "/pack.txt").c_str(), "r");
    if (!f) {
        return label;
    }
    char line[256];
    while (fgets(line, sizeof line, f)) {
        char name[200];
        if (sscanf(line, " name = %199[^\r\n]", name) == 1 && name[0]) {
            label = name;
        }
    }
    fclose(f);
    return label;
}

}  // namespace

std::vector<PackOption> find_packs(const char *kind, const char *original_label) {
    std::vector<PackOption> out = {{"ja", original_label}};
    std::string base = std::string(settings_data_dir()) + kind;
    for (const std::string &code : list_subdirs(base)) {
        if (code != "ja") {
            out.push_back({code, pack_label(base + "/" + code, code)});
        }
    }
    return out;
}
