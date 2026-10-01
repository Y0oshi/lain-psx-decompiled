#include "mods_ui.h"

#include <ctype.h>
#include <stdio.h>
#include <string.h>

#include <atomic>
#include <memory>
#include <string>
#include <thread>
#include <vector>

#include <SDL.h>

#include "discs.h"
#include "imgui.h"
#include "mods.h"

namespace {

/* A background check of the mods (building every replacement takes a while). */
struct CheckJob {
    std::atomic<bool> done{false};
    std::vector<ModInfo> mods;
};

struct ExportJob {
    std::atomic<int> state{0}; /* 0 idle, 1 running, 2 done */
    volatile float progress = 0;
    int written = 0;
    char msg[256] = "";
    std::string dir;
};

struct Tab {
    bool loaded = false;
    std::vector<ModInfo> mods;
    bool checked = false;
    std::shared_ptr<CheckJob> check;
    std::shared_ptr<ExportJob> exp;
    int selected = 0;
    char new_name[64] = "";
    bool naming = false;
    std::string message;
};

Tab g_tab;

bool disc_paths(std::string &d1, std::string &d2) {
    char a[1024], b[1024];
    bool have1 = discs_imported(settings_data_dir(), DISC_1, a, sizeof a);
    d1 = have1 ? a : "";
    d2 = discs_imported(settings_data_dir(), DISC_2, b, sizeof b) ? b : "";
    return have1;
}

/* Rescans the folder and starts checking in the background. */
void refresh(Settings *s) {
    g_tab.mods = mods_scan(s->mods);
    g_tab.loaded = true;
    g_tab.checked = false;
    if (g_tab.selected >= (int)g_tab.mods.size()) g_tab.selected = (int)g_tab.mods.size() - 1;
    if (g_tab.selected < 0) g_tab.selected = 0;
    auto job = std::make_shared<CheckJob>();
    job->mods = g_tab.mods;
    g_tab.check = job;
    std::string d1, d2;
    disc_paths(d1, d2);
    std::thread([job, d1, d2] {
        mods_validate(job->mods, d1.empty() ? nullptr : d1.c_str(), d2.empty() ? nullptr : d2.c_str());
        job->done = true;
    }).detach();
}

void save_order(Settings *s) {
    snprintf(s->mods, sizeof s->mods, "%s", mods_setting_string(g_tab.mods).c_str());
    settings_save(s);
    refresh(s); /* overrides depend on order and on/off */
}

/* A folder path as a file:// URL for SDL_OpenURL. */
void open_folder(const std::string &path) {
    std::string url = "file://";
    std::string p = path;
#ifdef _WIN32
    for (char &c : p) {
        if (c == '\\') c = '/';
    }
    url += "/";
#endif
    for (unsigned char c : p) {
        if (isalnum(c) || strchr("/-_.~:", c)) {
            url += (char)c;
        } else {
            char hex[4];
            snprintf(hex, sizeof hex, "%%%02X", c);
            url += hex;
        }
    }
    SDL_OpenURL(url.c_str());
}

ImVec4 level_color(int level) {
    return level == 2 ? ImVec4(1.0f, 0.45f, 0.4f, 1) : level == 1 ? ImVec4(1.0f, 0.85f, 0.4f, 1) : ImVec4(0.55f, 0.9f, 0.55f, 1);
}

void draw_details(const ModInfo &m) {
    ImGui::TextUnformatted(m.name.c_str());
    if (!m.version.empty()) {
        ImGui::SameLine();
        ImGui::TextDisabled("%s", m.version.c_str());
    }
    if (!m.author.empty()) ImGui::TextDisabled("by %s", m.author.c_str());
    ImGui::TextDisabled("Folder: mods/%s", m.folder.c_str());
    if (!m.description.empty()) {
        ImGui::Spacing();
        ImGui::TextWrapped("%s", m.description.c_str());
    }
    ImGui::Spacing();
    ImGui::SeparatorText("Files");
    if (m.files.empty() && !m.has_script && m.plugins.empty() && m.data.empty()) {
        ImGui::TextDisabled("Nothing in this mod yet. Replacement files go in folders named after the");
        ImGui::TextDisabled("archive (SITEA.BIN/0631.png), a script in main.lua. See MODDING.md.");
    }
    if (ImGui::BeginTable("files", 2, ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_RowBg)) {
        for (const ModFile &f : m.files) {
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::TextUnformatted(f.path.c_str());
            ImGui::TableNextColumn();
            if (!g_tab.checked && f.level != 2) {
                ImGui::TextDisabled("checking...");
            } else {
                const char *what = f.level == 2 ? "not used" : "used";
                ImGui::TextColored(level_color(f.level), "%s", what);
                if (!f.note.empty()) {
                    ImGui::SameLine();
                    ImGui::TextWrapped("%s", f.note.c_str());
                }
            }
        }
        ImGui::EndTable();
    }
    if (m.textures) {
        ImGui::Spacing();
        ImGui::BulletText("Texture pack: %d HD texture%s", m.textures, m.textures == 1 ? "" : "s");
    }
    if (m.has_script || !m.plugins.empty()) {
        ImGui::Spacing();
        ImGui::SeparatorText("Code");
        if (m.has_script) ImGui::BulletText("Script: main.lua (%d Lua file%s)", (int)m.scripts.size(), m.scripts.size() == 1 ? "" : "s");
        else if (!m.scripts.empty()) ImGui::BulletText("Lua files without a main.lua (not run)");
        for (const std::string &p : m.plugins) ImGui::BulletText("Plugin: %s", p.c_str());
        ImGui::TextDisabled("Script and plugin messages appear in the F1 menu's Mods tab.");
    }
    if (!m.ignored.empty()) {
        ImGui::Spacing();
        ImGui::TextDisabled("Ignored (not named like a replacement):");
        for (const std::string &g : m.ignored) ImGui::BulletText("%s", g.c_str());
    }
}

} // namespace

void mods_tab_draw(Settings *s) {
    Tab &t = g_tab;
    if (!t.loaded) refresh(s);
    if (t.check && t.check->done) {
        /* the check ran on a copy: keep it only if the list hasn't changed since */
        if (t.check->mods.size() == t.mods.size()) {
            bool same = true;
            for (size_t i = 0; i < t.mods.size() && same; i++) {
                same = t.check->mods[i].folder == t.mods[i].folder && t.check->mods[i].enabled == t.mods[i].enabled;
            }
            if (same) {
                t.mods = t.check->mods;
                t.checked = true;
            }
        }
        t.check.reset();
    }
    std::string d1, d2;
    const bool have1 = disc_paths(d1, d2);

    ImGui::TextWrapped("Mods replace pictures, text, voices and other game files with your own. "
                       "They are applied when you press Play; your discs are never changed.");
    if (ImGui::Button("Open mods folder")) open_folder(mods_dir());
    ImGui::SameLine();
    if (ImGui::Button("Refresh")) refresh(s);
    ImGui::SameLine();
    if (!t.naming) {
        if (ImGui::Button("New mod...")) {
            t.naming = true;
            t.new_name[0] = 0;
        }
    } else {
        ImGui::SetNextItemWidth(180);
        if (ImGui::IsWindowAppearing() || !ImGui::IsAnyItemActive()) ImGui::SetKeyboardFocusHere();
        bool enter = ImGui::InputTextWithHint("##name", "Mod name", t.new_name, sizeof t.new_name,
                                              ImGuiInputTextFlags_EnterReturnsTrue);
        ImGui::SameLine();
        if (ImGui::Button("Create") || enter) {
            std::string folder, err;
            if (mods_create(t.new_name, folder, err)) {
                t.message = "Created mods/" + folder + ". Add replacement files to it.";
                t.naming = false;
                t.mods = mods_scan(s->mods); /* save_order checks them */
                for (size_t i = 0; i < t.mods.size(); i++) {
                    if (t.mods[i].folder == folder) t.selected = (int)i;
                }
                save_order(s);
                open_folder(mods_dir() + "/" + folder);
            } else {
                t.message = err;
            }
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel")) t.naming = false;
    }
    ImGui::SameLine();
    const bool exporting = t.exp && t.exp->state == 1;
    ImGui::BeginDisabled(!have1 || exporting);
    if (ImGui::Button("Export originals")) {
        auto job = std::make_shared<ExportJob>();
        job->dir = mods_dir() + "/_originals";
        job->state = 1;
        t.exp = job;
        std::string a = d1, b = d2;
        std::thread([job, a, b] {
            job->written = mods_export_originals(a.c_str(), b.empty() ? nullptr : b.c_str(), job->dir.c_str(),
                                                 &job->progress, job->msg, sizeof job->msg);
            job->state = 2;
        }).detach();
    }
    ImGui::EndDisabled();
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
        ImGui::SetTooltip("Writes every picture, voice and file of your discs to mods/_originals,\n"
                          "as files a mod can replace. Copy the ones you want to change into your mod.");
    }

    if (exporting) {
        ImGui::ProgressBar(t.exp->progress, ImVec2(-1, 0), "Exporting originals...");
    } else if (t.exp && t.exp->state == 2) {
        if (t.exp->written > 0) {
            ImGui::Text("Exported %d files to mods/_originals.", t.exp->written);
            ImGui::SameLine();
            if (ImGui::SmallButton("Open")) open_folder(t.exp->dir);
        } else {
            ImGui::TextColored(level_color(2), "Export failed: %s", t.exp->msg);
        }
    } else if (!t.message.empty()) {
        ImGui::TextWrapped("%s", t.message.c_str());
    } else if (!have1) {
        ImGui::TextDisabled("Import disc 1 on the Setup tab to check and use mods.");
    }
    bool dump = s->dump_textures != 0;
    if (ImGui::Checkbox("Save the game's textures while playing", &dump)) {
        s->dump_textures = dump;
        settings_save(s);
    }
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Every picture the game draws is saved to the texture_dump folder, named for a\n"
                          "texture pack: upscale one, keep its name, put it in a mod's textures folder.");
    }
    ImGui::Separator();

    if (t.mods.empty()) {
        ImGui::Spacing();
        ImGui::TextDisabled("No mods installed. Put a mod's folder in the mods folder, or make one with New mod.");
        ImGui::TextDisabled("port/MODDING.md explains how mods work.");
        return;
    }

    /* Left: the list in load order. Right: the selected mod. */
    ImGui::BeginChild("modlist", ImVec2(ImGui::GetContentRegionAvail().x * 0.42f, 0), true);
    ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
    ImGui::TextWrapped("Load order: when two mods change the same file, the lower one wins.");
    ImGui::PopStyleColor();
    for (size_t i = 0; i < t.mods.size(); i++) {
        ModInfo &m = t.mods[i];
        ImGui::PushID((int)i);
        if (ImGui::Checkbox("##on", &m.enabled)) save_order(s);
        ImGui::SameLine();
        std::string label = m.name;
        if (!m.version.empty()) label += "  " + m.version;
        if (ImGui::Selectable(label.c_str(), t.selected == (int)i)) t.selected = (int)i;
        if (t.checked && (m.errors || m.warnings)) {
            ImGui::SameLine();
            ImGui::TextColored(level_color(m.errors ? 2 : 1), m.errors ? "%d problem%s" : "%d note%s",
                               m.errors ? m.errors : m.warnings, (m.errors ? m.errors : m.warnings) == 1 ? "" : "s");
        }
        ImGui::PopID();
    }
    ImGui::EndChild();
    ImGui::SameLine();
    ImGui::BeginChild("moddetail", ImVec2(0, 0), true);
    if (t.selected >= 0 && t.selected < (int)t.mods.size()) {
        const int i = t.selected;
        ImGui::BeginDisabled(i == 0);
        if (ImGui::Button("Move up")) {
            std::swap(t.mods[i], t.mods[i - 1]);
            t.selected = i - 1;
            save_order(s);
        }
        ImGui::EndDisabled();
        ImGui::SameLine();
        ImGui::BeginDisabled(i + 1 >= (int)t.mods.size());
        if (ImGui::Button("Move down")) {
            std::swap(t.mods[i], t.mods[i + 1]);
            t.selected = i + 1;
            save_order(s);
        }
        ImGui::EndDisabled();
        ImGui::SameLine();
        if (ImGui::Button("Open folder")) open_folder(mods_dir() + "/" + t.mods[t.selected].folder);
        ImGui::Separator();
        if (t.selected < (int)t.mods.size()) draw_details(t.mods[t.selected]);
    }
    ImGui::EndChild();
}
