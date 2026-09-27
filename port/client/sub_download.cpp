#include "sub_download.h"

#include <stdio.h>
#include <stdlib.h>

#include <filesystem>
#include <system_error>
#include <thread>

#include "settings.h"

namespace fs = std::filesystem;

/* Fan-made subtitle sets published by their authors. Nothing of theirs is part of
 * this program: the player's own client fetches them from the source. */
const SubSource kSubSources[] = {
    {"Download English subtitles", "en-laingame", "English (laingame.net fan subtitles)",
     "https://github.com/laingame-net/lainass/archive/refs/heads/main.tar.gz",
     "fan translation by laingame.net (github.com/laingame-net/lainass)"},
};
const int kSubSourceCount = sizeof kSubSources / sizeof kSubSources[0];

namespace {

fs::path lang_dir() {
    return fs::u8path(settings_data_dir()) / "lang";
}

/* Quotes a path for the shell (cmd.exe on Windows, sh elsewhere). */
std::string quoted(const fs::path &p) {
    std::string s = p.u8string();
#ifdef _WIN32
    return "\"" + s + "\"";
#else
    std::string out = "'";
    for (char c : s) {
        out += c == '\'' ? std::string("'\\''") : std::string(1, c);
    }
    return out + "'";
#endif
}

int run(const std::string &cmd) {
#ifdef _WIN32
    /* cmd.exe strips the outer quotes of the whole line when it starts with one. */
    return system(("\"" + cmd + "\"").c_str());
#else
    return system(cmd.c_str());
#endif
}

/* The system's own curl/tar. On Windows use the ones in System32 (Windows 10 1803+):
 * a Git/MSYS tar earlier in PATH reads "C:\..." as a remote host. */
std::string tool(const char *name) {
#ifdef _WIN32
    const char *root = getenv("SystemRoot");
    fs::path p = fs::u8path(root ? root : "C:\\Windows") / "System32" / (std::string(name) + ".exe");
    std::error_code ec;
    return fs::exists(p, ec) ? quoted(p) : std::string();
#else
    std::string check = std::string("command -v ") + name + " >/dev/null 2>&1";
    return system(check.c_str()) == 0 ? std::string(name) : std::string();
#endif
}

void finish(SubJob *job, bool ok, const std::string &msg) {
    job->message = msg;
    job->state = ok ? SubJob::DONE : SubJob::FAILED;
}

bool is_sub_file(const fs::path &p) {
    std::string ext = p.extension().u8string();
    for (char &c : ext) c = (char)tolower((unsigned char)c);
    return ext == ".ass" || ext == ".ssa" || ext == ".srt";
}

/* Counts the subtitle files a folder holds (bounded: any depth, at most 20000 entries). */
int count_subs(const fs::path &dir) {
    int n = 0, seen = 0;
    std::error_code ec;
    for (auto it = fs::recursive_directory_iterator(dir, ec); !ec && it != fs::recursive_directory_iterator() && seen < 20000;
         it.increment(ec), seen++) {
        std::error_code fe;
        if (it->is_regular_file(fe) && is_sub_file(it->path())) n++;
    }
    return n;
}

void write_pack_txt(const fs::path &dir, const std::string &name, const std::string &note) {
    FILE *f = fopen((dir / "pack.txt").u8string().c_str(), "w");
    if (f) {
        fprintf(f, "name = %s\n; %s\n", name.c_str(), note.c_str());
        fclose(f);
    }
}

/* A folder name usable as a pack code: letters, digits, '-', '_' and spaces, at most
 * 40 bytes (Settings.text_lang holds 64), never "ja" (the original Japanese). */
std::string pack_code(const std::string &name) {
    std::string out;
    for (unsigned char c : name) {
        if (isalnum(c) || c == '-' || c == '_') out += (char)c;
        else if (c == ' ' || c == '.') out += '-';
        if (out.size() >= 40) break;
    }
    while (!out.empty() && out.back() == '-') out.pop_back();
    if (out.empty()) out = "subtitles";
    if (out == "ja" || out == "JA") out += "-subs";
    return out;
}

/* Puts `from` in place as lang/<code>, replacing an older copy only once the new one
 * is in: the old one is moved aside first and restored if the move fails. */
bool install(const fs::path &from, const std::string &code, std::string &err) {
    std::error_code ec;
    fs::path dest = lang_dir() / fs::u8path(code);
    fs::path old = lang_dir() / fs::u8path("." + code + ".old");
    fs::remove_all(old, ec);
    bool had_old = fs::exists(dest, ec);
    if (had_old) {
        fs::rename(dest, old, ec);
        if (ec) {
            err = ec.message();
            return false;
        }
    }
    fs::rename(from, dest, ec);
    if (ec) {
        err = ec.message();
        if (had_old) {
            std::error_code back;
            fs::rename(old, dest, back);
        }
        return false;
    }
    fs::remove_all(old, ec);
    return true;
}

/* Is `inner` the same as, or inside, `outer`? */
bool within(const fs::path &inner, const fs::path &outer) {
    std::error_code ec;
    fs::path a = fs::weakly_canonical(inner, ec), b = fs::weakly_canonical(outer, ec);
    auto ai = a.begin(), bi = b.begin();
    for (; bi != b.end(); ++ai, ++bi) {
        if (ai == a.end() || *ai != *bi) return false;
    }
    return true;
}

}  // namespace

void sub_cleanup_leftovers(void) {
    std::error_code ec;
    fs::path lang = lang_dir();
    fs::remove_all(lang / ".download", ec);
    fs::remove_all(lang / ".import", ec);
    for (auto it = fs::directory_iterator(lang, ec); !ec && it != fs::directory_iterator(); it.increment(ec)) {
        std::string n = it->path().filename().u8string();
        if (n.size() > 5 && n[0] == '.' && n.compare(n.size() - 4, 4, ".old") == 0) {
            std::error_code rm;
            fs::remove_all(it->path(), rm);
        }
    }
}

void sub_download_start(SubJob *job, const SubSource &src) {
    job->state = SubJob::RUNNING;
    job->message.clear();
    job->code = src.code;
    SubSource s = src;
    std::thread([job, s] {
        std::error_code ec;
        const std::string curl = tool("curl"), tar = tool("tar");
        if (curl.empty() || tar.empty()) {
            finish(job, false, std::string("This needs the system's ") + (curl.empty() ? "curl" : "tar") +
                                   " (built into macOS and Windows 10+; on Linux install it with the package manager).");
            return;
        }
        fs::path work = lang_dir() / ".download";
        fs::remove_all(work, ec);
        fs::create_directories(work, ec);
        if (ec) {
            finish(job, false, "Can't create " + work.u8string() + ": " + ec.message());
            return;
        }
        fs::path archive = work / "subs.tar.gz";
        std::string get = curl + " -fsSL --retry 2 -o " + quoted(archive) + " \"" + s.url + "\"";
        if (run(get) != 0 || !fs::exists(archive, ec)) {
            fs::remove_all(work, ec);
            finish(job, false, "Download failed. Check the internet connection.");
            return;
        }
        std::string unpack = tar + " -xzf " + quoted(archive) + " -C " + quoted(work);
        if (run(unpack) != 0) {
            fs::remove_all(work, ec);
            finish(job, false, "Couldn't unpack the download.");
            return;
        }
        fs::remove(archive, ec);
        /* GitHub archives hold one top folder (lainass-main/). */
        fs::path top;
        for (auto it = fs::directory_iterator(work, ec); !ec && it != fs::directory_iterator(); it.increment(ec)) {
            std::error_code de;
            if (it->is_directory(de)) top = it->path();
        }
        int n = top.empty() ? 0 : count_subs(top);
        if (n == 0) {
            fs::remove_all(work, ec);
            finish(job, false, "The download has no subtitle files.");
            return;
        }
        write_pack_txt(top, s.name, std::string("Downloaded from ") + s.url);
        std::string err;
        bool ok = install(top, s.code, err);
        fs::remove_all(work, ec);
        if (!ok) {
            finish(job, false, "Couldn't install the subtitles: " + err);
            return;
        }
        finish(job, true, std::to_string(n) + " subtitle files installed: " + s.name + ".");
    }).detach();
}

void sub_import_folder_start(SubJob *job, const std::string &dir) {
    job->state = SubJob::RUNNING;
    job->message.clear();
    std::thread([job, dir] {
        std::error_code ec;
        fs::path from = fs::u8path(dir);
        fs::path lang = lang_dir();
        if (within(from, lang) || within(lang, from)) {
            finish(job, false, "Pick a folder outside the program's data folder.");
            return;
        }
        int n = count_subs(from);
        if (n == 0) {
            finish(job, false, "No .ass, .ssa or .srt subtitle files in that folder.");
            return;
        }
        const std::string code = pack_code(from.filename().u8string());
        fs::path work = lang / ".import";
        fs::remove_all(work, ec);
        fs::create_directories(work, ec);
        /* Only subtitle files (and a pack's font.ttf / pack.txt), keeping their folders. */
        int copied = 0;
        for (auto it = fs::recursive_directory_iterator(from, ec); !ec && it != fs::recursive_directory_iterator();
             it.increment(ec)) {
            std::error_code fe;
            if (!it->is_regular_file(fe)) continue;
            const fs::path &p = it->path();
            std::string name = p.filename().u8string();
            if (!is_sub_file(p) && name != "font.ttf" && name != "pack.txt") continue;
            fs::path rel = p.lexically_relative(from), to = work / rel;
            fs::create_directories(to.parent_path(), fe);
            fs::copy_file(p, to, fs::copy_options::overwrite_existing, fe);
            if (!fe && is_sub_file(p)) copied++;
        }
        if (copied == 0) {
            fs::remove_all(work, ec);
            finish(job, false, "Couldn't copy the subtitle files.");
            return;
        }
        if (!fs::exists(work / "pack.txt", ec)) {
            write_pack_txt(work, from.filename().u8string(), "Added from " + from.u8string());
        }
        std::string err;
        if (!install(work, code, err)) {
            fs::remove_all(work, ec);
            finish(job, false, "Couldn't install the subtitles: " + err);
            return;
        }
        job->code = code;
        finish(job, true, std::to_string(copied) + " subtitle files added as \"" + code + "\".");
    }).detach();
}
