#include "setup_ui.h"

#include <SDL.h>
#include <SDL_opengl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <algorithm>
#include <atomic>
#include <thread>
#include <string>
#include <vector>

#include "discs.h"
#include "packs.h"
#include "sub_download.h"
#include "mods_ui.h"
#include "unused_gallery.h"
#include "app_icon.h"
#include "imgui.h"
#include "imgui_impl_opengl3.h"
#include "imgui_impl_sdl2.h"
#include "tinyfiledialogs.h"

namespace {

const char *disc_status(int disc) {
    char path[1024];
    return discs_imported(settings_data_dir(), disc, path, sizeof path) ? "ready" : "not imported";
}

struct WindowSize {
    int w, h;
};
const WindowSize kSizes[] = {{960, 720}, {1280, 960}, {1440, 1080}, {1600, 1200}, {1920, 1440}, {2560, 1920}};

}  // namespace

namespace {

// "Unused" tab: decoded from the player's discs when first opened
struct Gallery {
    std::atomic<int> state{0};  // 0 not loaded, 1 loading, 2 ready, 3 failed
    std::vector<GallerySection> sections;
    std::string error;
    std::vector<std::vector<GLuint>> textures;  // per item (flattened over sections), per frame
    bool uploaded = false;
};

void gallery_start(Gallery &g) {
    char d1[1024], d2[1024];
    bool have1 = discs_imported(settings_data_dir(), DISC_1, d1, sizeof d1);
    bool have2 = discs_imported(settings_data_dir(), DISC_2, d2, sizeof d2);
    if (!have1) {
        g.error = "Import disc 1 first (Setup tab).";
        g.state = 3;
        return;
    }
    g.state = 1;
    std::string p1 = d1, p2 = have2 ? d2 : "";
    std::thread([&g, p1, p2] {
        bool ok = gallery_load(p1.c_str(), p2.empty() ? nullptr : p2.c_str(), g.sections, g.error);
        g.state = ok ? 2 : 3;
    }).detach();
}

void gallery_upload(Gallery &g) {
    for (auto &sec : g.sections) {
        for (auto &it : sec.items) {
            std::vector<GLuint> tex(it.frames.size());
            glGenTextures((GLsizei)tex.size(), tex.data());
            for (size_t f = 0; f < tex.size(); f++) {
                glBindTexture(GL_TEXTURE_2D, tex[f]);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
                glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, it.w, it.h, 0, GL_RGBA, GL_UNSIGNED_BYTE, it.frames[f].data());
            }
            g.textures.push_back(std::move(tex));
            it.frames.clear();  // on the GPU now
            it.frames.shrink_to_fit();
        }
    }
    glBindTexture(GL_TEXTURE_2D, 0);
    g.uploaded = true;
}

// Playing the gallery's sounds (a plain SDL audio device, launcher only)
SDL_AudioDeviceID g_audio;
const GalleryItem *g_playing;

void sound_stop() {
    if (g_audio) {
        SDL_ClearQueuedAudio(g_audio);
    }
    g_playing = nullptr;
}

void sound_play(const GalleryItem &it) {
    if (!g_audio) {
        if (!SDL_WasInit(SDL_INIT_AUDIO) && SDL_InitSubSystem(SDL_INIT_AUDIO) != 0) return;
        SDL_AudioSpec want = {}, have;
        want.freq = 44100;
        want.format = AUDIO_S16SYS;
        want.channels = 2;
        want.samples = 1024;
        g_audio = SDL_OpenAudioDevice(nullptr, 0, &want, &have, 0);
        if (!g_audio) return;
        SDL_PauseAudioDevice(g_audio, 0);
    }
    // Linear resampling to 44.1 kHz stereo.
    const size_t in_frames = it.pcm.size() / it.channels;
    const size_t out_frames = (size_t)((double)in_frames * 44100 / it.rate);
    std::vector<int16_t> out(out_frames * 2);
    for (size_t i = 0; i < out_frames; i++) {
        double pos = (double)i * it.rate / 44100;
        size_t a = (size_t)pos, b = std::min(a + 1, in_frames - 1);
        double f = pos - a;
        for (int c = 0; c < 2; c++) {
            int ch = it.channels == 2 ? c : 0;
            double v = it.pcm[a * it.channels + ch] * (1 - f) + it.pcm[b * it.channels + ch] * f;
            out[i * 2 + c] = (int16_t)v;
        }
    }
    SDL_ClearQueuedAudio(g_audio);
    SDL_QueueAudio(g_audio, out.data(), (Uint32)(out.size() * sizeof(int16_t)));
    g_playing = &it;
}

void sound_close() {
    if (g_audio) {
        SDL_CloseAudioDevice(g_audio);
        g_audio = 0;
    }
    g_playing = nullptr;
}

void gallery_draw(Gallery &g) {
    if (g.state == 0) {
        gallery_start(g);
    }
    if (g.state == 1) {
        ImGui::TextDisabled("Reading your discs...");
        return;
    }
    if (g.state == 3) {
        ImGui::TextWrapped("%s", g.error.c_str());
        if (ImGui::Button("Try again")) {
            g.state = 0; // e.g. after importing disc 1
        }
        return;
    }
    if (!g.uploaded) {
        gallery_upload(g);
    }
    ImGui::TextWrapped("Things on the Serial Experiments Lain discs that the game never shows, "
                       "decoded from your own discs. Found while decompiling the game.");
    const double now = ImGui::GetTime();
    size_t n = 0;
    for (auto &sec : g.sections) {
        ImGui::SeparatorText(sec.title.c_str());
        ImGui::TextWrapped("%s", sec.intro.c_str());
        const float avail = ImGui::GetContentRegionAvail().x;
        float x = 0;
        for (size_t i = 0; i < sec.items.size(); i++, n++) {
            const GalleryItem &it = sec.items[i];
            const std::vector<GLuint> &tex = g.textures[n];
            if (tex.empty()) {
                if (it.pcm.empty()) continue;
                // A sound: Play/Stop, its length, what it is.
                if (g_playing == &it && g_audio && SDL_GetQueuedAudioSize(g_audio) == 0) g_playing = nullptr;
                ImGui::PushID((int)n);
                if (ImGui::Button(g_playing == &it ? "Stop" : "Play", ImVec2(70, 0))) {
                    if (g_playing == &it) sound_stop();
                    else sound_play(it);
                }
                ImGui::PopID();
                ImGui::SameLine();
                ImGui::Text("%s", it.title.c_str());
                ImGui::SameLine();
                ImGui::TextDisabled("(%.1f s)", it.pcm.size() / (double)it.channels / it.rate);
                ImGui::TextWrapped("%s", it.caption.c_str());
                x = 0;
                continue;
            }
            // Small pictures are enlarged (whole pixels), big ones fit a 256-pixel box.
            float scale = std::max(1.0f, std::floor(160.0f / std::max(it.w, it.h)));
            if (it.w * scale > 256 || it.h * scale > 256) scale = 256.0f / std::max(it.w, it.h);
            const ImVec2 size(it.w * scale, it.h * scale);
            const float cell = std::max(size.x, 170.0f);
            if (i > 0 && x + cell <= avail) {
                ImGui::SameLine();
            } else {
                x = 0;
            }
            x += cell + ImGui::GetStyle().ItemSpacing.x;
            size_t frame = it.fps > 0 ? (size_t)(now * it.fps) % tex.size() : 0;
            ImGui::BeginGroup();
            ImGui::Image((ImTextureID)(intptr_t)tex[frame], size);
            ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + cell);
            ImGui::TextUnformatted(it.title.c_str());
            if (it.fps > 0) {
                ImGui::SameLine();
                ImGui::TextDisabled("(%zu frames)", tex.size());
            }
            ImGui::TextDisabled("%s", it.caption.c_str());
            ImGui::PopTextWrapPos();
            ImGui::EndGroup();
        }
    }
}

}  // namespace

extern "C" int setup_ui_run(Settings *s) {
    const char *glsl = "#version 150";
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, SDL_GL_CONTEXT_FORWARD_COMPATIBLE_FLAG);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 2);
    SDL_Window *win = SDL_CreateWindow("Serial Experiments Lain - Setup", SDL_WINDOWPOS_CENTERED,
                                       SDL_WINDOWPOS_CENTERED, 720, 640,
                                       SDL_WINDOW_OPENGL | SDL_WINDOW_ALLOW_HIGHDPI);
    if (!win) {
        fprintf(stderr, "setup window: %s\n", SDL_GetError());
        return 0;
    }
    app_icon_apply(win);
    SDL_GLContext gl = SDL_GL_CreateContext(win);
    SDL_GL_MakeCurrent(win, gl);
    SDL_GL_SetSwapInterval(1);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO &io = ImGui::GetIO();
    io.IniFilename = nullptr;
    ImGui::StyleColorsDark();
    ImGui::GetStyle().WindowRounding = 0;
    ImGui_ImplSDL2_InitForOpenGL(win, gl);
    ImGui_ImplOpenGL3_Init(glsl);

    std::vector<PackOption> text_langs = find_packs("lang", "Japanese (original)");
    std::vector<PackOption> voice_langs = find_packs("dub", "Japanese (original)");
    // Static: their worker threads write into them and may still be running when the
    // window closes (Play, Quit), so they must outlive this function.
    static ImportJob job;
    static SubJob subjob;
    job = ImportJob{};
    sub_cleanup_leftovers();
    static Gallery gallery; // static: its loader thread may outlive a quick close
    std::string sub_message;
    std::vector<std::string> queue;  // discs waiting to import
    std::string last_message;
    int result = -1;

    while (result < 0) {
        SDL_Event ev;
        while (SDL_PollEvent(&ev)) {
            ImGui_ImplSDL2_ProcessEvent(&ev);
            if (ev.type == SDL_QUIT) {
                result = 0;
            } else if (ev.type == SDL_DROPFILE) {
                queue.push_back(ev.drop.file);
                SDL_free(ev.drop.file);
            }
        }
        if (job.state != IMPORT_RUNNING && !queue.empty()) {
            discs_import_start(&job, queue.front().c_str(), settings_data_dir());
            queue.erase(queue.begin());
        }
        if (job.state == IMPORT_DONE || job.state == IMPORT_FAILED) {
            last_message = job.message;
            job.state = IMPORT_IDLE;
        }

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplSDL2_NewFrame();
        ImGui::NewFrame();
        ImGui::SetNextWindowPos(ImVec2(0, 0));
        ImGui::SetNextWindowSize(io.DisplaySize);
        ImGui::Begin("setup", nullptr,
                     ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings);

        ImGui::TextUnformatted("Serial Experiments Lain");
        ImGui::TextDisabled("Native client. Game data comes from your own discs.");
        ImGui::Separator();

        const bool busy = job.state == IMPORT_RUNNING;
        // A finished subtitle download/import (polled here, whatever tab is open).
        if (subjob.state == SubJob::DONE || subjob.state == SubJob::FAILED) {
            sub_message = subjob.message;
            if (subjob.state == SubJob::DONE) {
                text_langs = find_packs("lang", "Japanese (original)");
                snprintf(s->text_lang, sizeof s->text_lang, "%s", subjob.code.c_str());
            }
            subjob.state = SubJob::IDLE;
        }
        const bool sub_busy = subjob.state == SubJob::RUNNING;
        // Tabs; Play/Quit stay below them.
        const float footer = 64.0f;
        ImGui::BeginTabBar("launcher");
        const bool setup_tab = ImGui::BeginTabItem("Setup");
        if (setup_tab) {
            ImGui::BeginChild("setup_page", ImVec2(0, -footer));
        }
        if (setup_tab) {
        // Discs
        ImGui::SeparatorText("Discs");
        ImGui::Text("Disc 1 (Site A): %s", disc_status(DISC_1));
        ImGui::Text("Disc 2 (Site B): %s", disc_status(DISC_2));
        ImGui::BeginDisabled(busy);
        if (ImGui::Button("Add disc...")) {
            const char *filters[] = {"*.cue", "*.bin"};
            const char *picked = tinyfd_openFileDialog("Choose a Serial Experiments Lain disc", "", 2, filters,
                                                       "Disc image (.cue, .bin)", 1);
            if (picked) {
                // Multiple selections come back separated by '|'.
                std::string all = picked;
                size_t start = 0, bar;
                while ((bar = all.find('|', start)) != std::string::npos) {
                    queue.push_back(all.substr(start, bar - start));
                    start = bar + 1;
                }
                queue.push_back(all.substr(start));
            }
        }
        ImGui::EndDisabled();
        ImGui::SameLine();
        ImGui::TextDisabled("or drop .cue files on this window. Either disc, any order.");
        if (busy) {
            ImGui::ProgressBar(job.progress, ImVec2(-1, 0), "Importing and verifying...");
        } else if (!last_message.empty()) {
            ImGui::TextWrapped("%s", last_message.c_str());
        }

        // Display
        ImGui::SeparatorText("Display");
        ImGui::RadioButton("Windowed", &s->fullscreen, 0);
        ImGui::SameLine();
        ImGui::RadioButton("Fullscreen", &s->fullscreen, 1);
        ImGui::BeginDisabled(s->fullscreen);
        char cur[32];
        snprintf(cur, sizeof cur, "%d x %d", s->width, s->height);
        if (ImGui::BeginCombo("Window size", cur)) {
            for (const WindowSize &ws : kSizes) {
                char label[32];
                snprintf(label, sizeof label, "%d x %d", ws.w, ws.h);
                if (ImGui::Selectable(label, ws.w == s->width && ws.h == s->height)) {
                    s->width = ws.w;
                    s->height = ws.h;
                }
            }
            ImGui::EndCombo();
        }
        ImGui::EndDisabled();
        ImGui::SliderInt("Render scale", &s->render_scale, 0, 8, s->render_scale ? "%dx" : "Auto");
        ImGui::SameLine();
        if (s->render_scale) {
            ImGui::TextDisabled("(%d x %d internal)", 320 * s->render_scale, 240 * s->render_scale);
        } else {
            ImGui::TextDisabled("(window resolution; 1x = original pixels)");
        }
        bool vsync = s->vsync != 0;
        if (ImGui::Checkbox("VSync", &vsync)) {
            s->vsync = vsync;
        }
        ImGui::SameLine();
        bool smooth = s->smooth != 0;
        if (ImGui::Checkbox("Smooth motion (60 fps)", &smooth)) {
            s->smooth = smooth;
        }
        ImGui::SameLine();
        ImGui::TextDisabled("12/30 fps screens get in-between frames; game speed unchanged");

        // Language
        ImGui::SeparatorText("Language");
        auto lang_combo = [](const char *label, std::vector<PackOption> &opts, char *code, size_t cap) {
            const char *shown = opts[0].label.c_str();
            for (auto &o : opts) {
                if (o.code == code) shown = o.label.c_str();
            }
            if (ImGui::BeginCombo(label, shown)) {
                for (auto &o : opts) {
                    if (ImGui::Selectable(o.label.c_str(), o.code == code)) {
                        snprintf(code, cap, "%s", o.code.c_str());
                    }
                }
                ImGui::EndCombo();
            }
        };
        lang_combo("Text / subtitles", text_langs, s->text_lang, sizeof s->text_lang);
        lang_combo("Voices", voice_langs, s->voice_lang, sizeof s->voice_lang);

        // Subtitles are not part of this program: players fetch a published fan set
        // with one click, or add a folder of .ass/.srt files they already have.
        ImGui::BeginDisabled(sub_busy);
        for (int i = 0; i < kSubSourceCount; i++) {
            ImGui::PushID(i);
            if (ImGui::Button(kSubSources[i].label)) {
                sub_download_start(&subjob, kSubSources[i]);
            }
            ImGui::SameLine();
            ImGui::TextDisabled("%s", kSubSources[i].credit);
            ImGui::PopID();
        }
        if (ImGui::Button("Add subtitle folder...")) {
            const char *dir = tinyfd_selectFolderDialog("Folder with .ass or .srt subtitle files", "");
            if (dir) {
                sub_import_folder_start(&subjob, dir);
            }
        }
        ImGui::EndDisabled();
        ImGui::SameLine();
        if (sub_busy) {
            ImGui::TextDisabled("Working...");
        } else if (!sub_message.empty()) {
            ImGui::TextWrapped("%s", sub_message.c_str());
        } else {
            ImGui::TextDisabled("Packs live in: %slang", settings_data_dir());
        }

        }
        if (setup_tab) {
            ImGui::EndChild();
            ImGui::EndTabItem();
        }
        // Tests/docs: LAIN_SETUP_TAB=mods / unused opens the launcher on that tab.
        static bool first_frame = true;
        const char *want_tab = first_frame ? getenv("LAIN_SETUP_TAB") : nullptr;
        const bool open_unused = want_tab && !strcmp(want_tab, "unused");
        const bool open_mods = want_tab && !strcmp(want_tab, "mods");
        first_frame = false;
        if (ImGui::BeginTabItem("Mods", nullptr, open_mods ? ImGuiTabItemFlags_SetSelected : 0)) {
            ImGui::BeginChild("mods_page", ImVec2(0, -footer));
            mods_tab_draw(s);
            ImGui::EndChild();
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Unused", nullptr, open_unused ? ImGuiTabItemFlags_SetSelected : 0)) {
            ImGui::BeginChild("unused_page", ImVec2(0, -footer));
            gallery_draw(gallery);
            ImGui::EndChild();
            ImGui::EndTabItem();
        }
        ImGui::EndTabBar();

        // Play
        ImGui::Separator();
        char p1[1024];
        bool have_disc1 = discs_imported(settings_data_dir(), DISC_1, p1, sizeof p1);
        ImGui::BeginDisabled(!have_disc1 || busy || sub_busy);
        if (ImGui::Button("Play", ImVec2(160, 40))) {
            s->setup_done = 1;
            settings_save(s);
            result = 1;
        }
        ImGui::EndDisabled();
        ImGui::SameLine();
        if (ImGui::Button("Quit", ImVec2(100, 40))) {
            result = 0;
        }
        if (!have_disc1) {
            ImGui::SameLine();
            ImGui::TextDisabled("Import disc 1 to play. Disc 2 is needed for Site B.");
        }

        ImGui::End();
        ImGui::Render();
        int fbw, fbh;
        SDL_GL_GetDrawableSize(win, &fbw, &fbh);
        glViewport(0, 0, fbw, fbh);
        glClearColor(0.05f, 0.05f, 0.07f, 1);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        // Tests/docs: LAIN_SETUP_SCREENSHOT=<file.bmp> saves the launcher after a few frames and quits.
        static int frames = 0;
        const char *shot = getenv("LAIN_SETUP_SCREENSHOT");
        if (shot && ++frames == 90) {
            std::vector<unsigned char> px((size_t)fbw * fbh * 4);
            glReadPixels(0, 0, fbw, fbh, GL_RGBA, GL_UNSIGNED_BYTE, px.data());
            SDL_Surface *surf = SDL_CreateRGBSurfaceWithFormat(0, fbw, fbh, 32, SDL_PIXELFORMAT_RGBA32);
            for (int y = 0; y < fbh; y++) {
                memcpy((unsigned char *)surf->pixels + (size_t)y * surf->pitch, &px[(size_t)(fbh - 1 - y) * fbw * 4],
                       (size_t)fbw * 4);
            }
            SDL_SaveBMP(surf, shot);
            SDL_FreeSurface(surf);
            result = 0;
        }
        SDL_GL_SwapWindow(win);
    }

    settings_save(s);
    sound_close(); // the game opens its own audio
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplSDL2_Shutdown();
    ImGui::DestroyContext();
    SDL_GL_DeleteContext(gl);
    SDL_DestroyWindow(win);
    return result;
}
