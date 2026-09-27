/* In-game overlay: toasts, subtitles and the F1 settings menu, drawn with Dear ImGui
 * on the game window's GL context (PsyCross's). ImGui's OpenGL3 backend saves
 * and restores the GL state it touches, so PsyCross's rendering is unaffected. */
#include "overlay.h"

#include <algorithm>

#include <SDL.h>
#include <float.h>
#include <string.h>

#include "imgui.h"
#include "imgui_impl_opengl3.h"
#include "imgui_impl_sdl2.h"
#include "packs.h"

#include <string>
#include <utility>
#include <vector>

static bool s_ready;
static char s_text[256];
static Uint64 s_until;
static char s_sub[1024];
static char s_font_path[1024];
static ImFont *s_sub_font;
static const float SUB_FONT_PX = 48.0f; /* rasterised size; scaled to the window */
static bool s_font_dirty;

/* The game's hidden stats (see the Cheats tab), read live from its memory. */
extern "C" short g_stat_tokimeki, g_stat_harumage, g_stat_gakkuri, g_screensaver_count;

/* F1 menu */
static bool s_menu;
static bool s_menu_focus; /* put keyboard/gamepad focus on the first control */
static Settings *s_settings;
static OverlayApplyFn s_apply;
static std::vector<PackOption> s_text_packs, s_voice_packs;
static Uint64 s_mouse_moved_at;  /* last mouse motion (performance counter) */
static int s_rebind = -1;       /* button waiting for a key, or -1 */
static bool s_keys_changed;

static bool overlay_init(void) {
    SDL_Window *win = SDL_GL_GetCurrentWindow();
    SDL_GLContext gl = SDL_GL_GetCurrentContext();
    if (!win || !gl) {
        return false;
    }
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO &io = ImGui::GetIO();
    io.IniFilename = nullptr;
    io.ConfigFlags |= ImGuiConfigFlags_NoMouseCursorChange | ImGuiConfigFlags_NavEnableKeyboard |
                      ImGuiConfigFlags_NavEnableGamepad;
    ImGui::StyleColorsDark();
    ImGui::GetStyle().WindowRounding = 4;
    ImGui_ImplSDL2_InitForOpenGL(win, gl);
    ImGui_ImplOpenGL3_Init("#version 150");
    io.Fonts->AddFontDefault();
    s_font_dirty = true;
    return true;
}

/* The subtitle font can change while playing (a pack chosen in the menu); ImGui
 * 1.92's dynamic atlas accepts new fonts at any time. */
static void load_sub_font(void) {
    /* Each font file is added to the atlas once, however often packs are switched. */
    static std::vector<std::pair<std::string, ImFont *>> loaded;
    s_font_dirty = false;
    s_sub_font = nullptr;
    if (!s_font_path[0]) {
        return;
    }
    for (auto &f : loaded) {
        if (f.first == s_font_path) {
            s_sub_font = f.second;
            return;
        }
    }
    s_sub_font = ImGui::GetIO().Fonts->AddFontFromFileTTF(s_font_path, SUB_FONT_PX);
    if (s_sub_font) {
        loaded.emplace_back(s_font_path, s_sub_font);
    }
}

extern "C" void overlay_set_subtitle(const char *text) {
    SDL_strlcpy(s_sub, text ? text : "", sizeof s_sub);
}

extern "C" void overlay_set_font(const char *ttf_path) {
    SDL_strlcpy(s_font_path, ttf_path ? ttf_path : "", sizeof s_font_path);
    s_font_dirty = true;
}

extern "C" void overlay_menu_init(Settings *settings, OverlayApplyFn apply) {
    s_settings = settings;
    s_apply = apply;
}

extern "C" int overlay_menu_is_open(void) {
    return s_menu;
}

static void set_menu(bool open) {
    if (open == s_menu || !s_settings) {
        return;
    }
    s_menu = open;
    s_rebind = -1;
    if (open) {
        s_menu_focus = true;
        if (s_ready) {
            ImGui::GetIO().ClearInputKeys(); /* nothing carried over from before */
        }
        /* Packs installed while the game runs show up the next time the menu opens. */
        s_text_packs = find_packs("lang", "Japanese (original)");
        s_voice_packs = find_packs("dub", "Japanese (original)");
    } else {
        settings_save(s_settings);
    }
}

extern "C" int overlay_handle_event(const union SDL_Event *ev) {
    const SDL_Event *e = (const SDL_Event *)ev;
    if (e->type == SDL_MOUSEMOTION) {
        s_mouse_moved_at = SDL_GetPerformanceCounter();
    }
    if (s_menu && s_rebind >= 0 && e->type == SDL_KEYDOWN && !e->key.repeat) {
        SDL_Scancode sc = e->key.keysym.scancode;
        if (sc != SDL_SCANCODE_ESCAPE && sc != SDL_SCANCODE_F1) {
            SDL_strlcpy(s_settings->keys[s_rebind], SDL_GetScancodeName(sc), sizeof s_settings->keys[0]);
            s_keys_changed = true;
        }
        s_rebind = -1; /* Esc/F1 cancel */
        return 1;
    }
    bool toggle = (e->type == SDL_KEYDOWN && !e->key.repeat && e->key.keysym.scancode == SDL_SCANCODE_F1) ||
                  (e->type == SDL_CONTROLLERBUTTONDOWN && e->cbutton.button == SDL_CONTROLLER_BUTTON_GUIDE);
    if (toggle) {
        set_menu(!s_menu);
        return 1;
    }
    if (!s_menu) {
        /* Releases still go to the menu, so a key held when it closed isn't stuck down
         * the next time it opens. */
        if (s_ready && (e->type == SDL_KEYUP || e->type == SDL_MOUSEBUTTONUP || e->type == SDL_CONTROLLERBUTTONUP)) {
            ImGui_ImplSDL2_ProcessEvent(e);
        }
        return 0;
    }
    if (e->type == SDL_KEYDOWN && e->key.keysym.scancode == SDL_SCANCODE_ESCAPE) {
        set_menu(false);
        return 1;
    }
    if (s_ready) {
        ImGui_ImplSDL2_ProcessEvent(e);
    }
    /* Keyboard, mouse and text go to the menu; window and device events still
     * reach PsyCross (resizes, controller hotplug). */
    switch (e->type) {
    case SDL_KEYDOWN: case SDL_KEYUP: case SDL_TEXTINPUT: case SDL_TEXTEDITING:
    case SDL_MOUSEMOTION: case SDL_MOUSEBUTTONDOWN: case SDL_MOUSEBUTTONUP: case SDL_MOUSEWHEEL:
    case SDL_CONTROLLERBUTTONDOWN: case SDL_CONTROLLERBUTTONUP: case SDL_CONTROLLERAXISMOTION:
        return 1;
    default:
        return 0;
    }
}

static bool pack_combo(const char *label, const std::vector<PackOption> &opts, char *code, size_t cap) {
    const char *shown = opts.empty() ? code : opts[0].label.c_str();
    for (const PackOption &o : opts) {
        if (o.code == code) shown = o.label.c_str();
    }
    bool changed = false;
    if (ImGui::BeginCombo(label, shown)) {
        for (const PackOption &o : opts) {
            if (ImGui::Selectable(o.label.c_str(), o.code == code) && o.code != code) {
                SDL_strlcpy(code, o.code.c_str(), cap);
                changed = true;
            }
        }
        ImGui::EndCombo();
    }
    return changed;
}

/* LAIN_MENU_DEBUG: log which control has keyboard/gamepad focus (tests). */
static void note_focus(const char *label) {
    static const char *last;
    if (ImGui::IsItemFocused() && last != label && SDL_getenv("LAIN_MENU_DEBUG")) {
        last = label;
        SDL_Log("menu focus: %s (frame %d)", label, ImGui::GetFrameCount());
    }
}

static void draw_menu(const ImGuiViewport *vp) {
    Settings *s = s_settings;
    int apply = 0;
    float ui = std::min(vp->WorkSize.y / 540.0f, vp->WorkSize.x / 640.0f); /* sized to the window */
    ui = ui < 1.0f ? 1.0f : ui;
    const float width = std::min(600.0f * ui, vp->WorkSize.x * 0.98f);
    ImGui::PushFont(nullptr, 15.0f * ui);
    ImGui::SetNextWindowPos(ImVec2(vp->WorkPos.x + vp->WorkSize.x * 0.5f, vp->WorkPos.y + vp->WorkSize.y * 0.5f),
                            ImGuiCond_Always, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSizeConstraints(ImVec2(width, 0.0f), ImVec2(width, vp->WorkSize.y * 0.95f));
    ImGui::SetNextWindowSize(ImVec2(width, 0.0f), ImGuiCond_Always);
    ImGui::SetNextWindowBgAlpha(0.92f);
    bool open = true;
    ImGui::Begin("Settings (F1)", &open, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoMove |
                                             ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoResize);
    ImGui::PushItemWidth(-160.0f * ui);
    if (ImGui::BeginTabBar("tabs")) {
        if (ImGui::BeginTabItem("Settings")) {
            ImGui::SeparatorText("Display");
            bool fs = s->fullscreen != 0;
            if (s_menu_focus) {
                ImGui::SetKeyboardFocusHere();
                s_menu_focus = false;
            }
            if (ImGui::Checkbox("Fullscreen", &fs)) {
                s->fullscreen = fs;
                apply |= OVERLAY_APPLY_VIDEO;
            }
            note_focus("Fullscreen");
            ImGui::SameLine(0, 24.0f * ui);
            bool vsync = s->vsync != 0;
            if (ImGui::Checkbox("VSync", &vsync)) {
                s->vsync = vsync;
                apply |= OVERLAY_APPLY_VIDEO;
            }
            note_focus("VSync");
            bool smooth = s->smooth != 0;
            if (ImGui::Checkbox("Smooth motion (60 fps)", &smooth)) {
                s->smooth = smooth;
                apply |= OVERLAY_APPLY_VIDEO;
            }
            note_focus("Smooth motion");
            ImGui::SameLine();
            ImGui::TextDisabled(s->smooth ? "(in-between frames; game speed unchanged)" : "(original frame rate)");
            if (ImGui::SliderInt("Render scale", &s->render_scale, 0, 8, s->render_scale ? "%dx" : "Auto")) {
                apply |= OVERLAY_APPLY_VIDEO;
            }
            note_focus("Render scale");
            ImGui::TextDisabled(s->render_scale ? "%d x %d internal; 1x = original pixels" : "window resolution",
                                320 * s->render_scale, 240 * s->render_scale);

            ImGui::SeparatorText("Sound");
            if (ImGui::SliderInt("Volume", &s->volume, 0, 100, "%d%%")) {
                apply |= OVERLAY_APPLY_VOLUME;
            }
            note_focus("Volume");

            ImGui::SeparatorText("Language");
            if (pack_combo("Text / subtitles", s_text_packs, s->text_lang, sizeof s->text_lang)) {
                apply |= OVERLAY_APPLY_TEXT;
            }
            note_focus("Text / subtitles");
            if (pack_combo("Voices", s_voice_packs, s->voice_lang, sizeof s->voice_lang)) {
                apply |= OVERLAY_APPLY_VOICE;
            }
            note_focus("Voices");
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Controls")) {
            static const char *const labels[SETTINGS_NUM_KEYS] = {
                "Up", "Down", "Left", "Right", "Cross", "Circle", "Square", "Triangle",
                "L1", "L2", "L3", "R1", "R2", "R3", "Start", "Select",
            };
            ImGui::TextDisabled("Click a button, then press a key (Esc cancels).");
            if (ImGui::BeginTable("keys", 4, ImGuiTableFlags_SizingStretchSame)) {
                for (int b = 0; b < SETTINGS_NUM_KEYS; b++) {
                    ImGui::TableNextColumn();
                    ImGui::TextUnformatted(labels[b]);
                    ImGui::TableNextColumn();
                    ImGui::PushID(b);
                    const char *shown = s_rebind == b ? "press a key..." : s->keys[b];
                    if (ImGui::Button(shown, ImVec2(-FLT_MIN, 0))) {
                        s_rebind = b;
                    }
                    ImGui::PopID();
                }
                ImGui::EndTable();
            }
            if (ImGui::Button("Default keys")) {
                for (int b = 0; b < SETTINGS_NUM_KEYS; b++) {
                    SDL_strlcpy(s->keys[b], settings_default_key(b), sizeof s->keys[b]);
                }
                s_keys_changed = true;
            }
            ImGui::EndTabItem();
        }
        // Tests/docs: LAIN_MENU_TAB=cheats opens the menu on that tab.
        static bool tab_forced;
        const bool force_cheats = !tab_forced && SDL_getenv("LAIN_MENU_TAB") && !strcmp(SDL_getenv("LAIN_MENU_TAB"), "cheats");
        tab_forced = true;
        if (ImGui::BeginTabItem("Cheats", nullptr, force_cheats ? ImGuiTabItemFlags_SetSelected : 0)) {
            ImGui::TextDisabled("Cheats change how the game runs, not your saves.");
            bool nodes = s->cheat_open_nodes != 0;
            if (ImGui::Checkbox("Open any node", &nodes)) {
                s->cheat_open_nodes = nodes;
                apply |= OVERLAY_APPLY_CHEATS;
            }
            note_focus("Open any node");
            ImGui::SameLine();
            ImGui::TextDisabled("(ignore the progress a node needs)");
            if (ImGui::SliderInt("Turbo (hold Tab)", &s->turbo, 0, 8, s->turbo >= 2 ? "%dx" : "Off")) {
                if (s->turbo == 1) {
                    s->turbo = 0;
                }
                apply |= OVERLAY_APPLY_CHEATS;
            }
            note_focus("Turbo");
            ImGui::TextDisabled("Movies and voices keep their normal speed.");
            bool genome = s->cheat_genome != 0;
            if (ImGui::Checkbox("Genome save title", &genome)) {
                s->cheat_genome = genome;
                apply |= OVERLAY_APPLY_CHEATS;
            }
            note_focus("Genome save title");
            ImGui::SameLine();
            ImGui::TextDisabled("(normally 1 save in 70)");

            // The four stats the game counts but only ever shows in a save's title
            // (docs/findings/README.md, item 1). Read live from the game.
            ImGui::SeparatorText("Lain's hidden stats");
            ImGui::TextDisabled("Since the last load; save titles show one.");
            ImGui::Text("Heart-flutter  %3d%%", g_stat_tokimeki);
            ImGui::SameLine();
            ImGui::TextDisabled("nodes opened");
            ImGui::Text("Dejection      %3d%%", g_stat_gakkuri);
            ImGui::SameLine();
            ImGui::TextDisabled("Lain's reactions");
            ImGui::Text("Harumage       %3d%%", g_stat_harumage);
            ImGui::SameLine();
            ImGui::TextDisabled("empty cells pressed");
            ImGui::Text("Poison waves   %3d ", g_screensaver_count);
            ImGui::SameLine();
            ImGui::TextDisabled("screensavers");
            ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
    }
    if (s_keys_changed) {
        s_keys_changed = false;
        apply |= OVERLAY_APPLY_KEYS;
    }

    ImGui::Separator();
    if (ImGui::Button("Resume")) {
        open = false;
    }
    note_focus("Resume");
    ImGui::SameLine();
    if (ImGui::Button("Quit game")) {
        apply |= OVERLAY_APPLY_QUIT;
    }
    note_focus("Quit game");
    ImGui::SameLine();
    ImGui::TextDisabled("Esc / F1 to close");
    ImGui::PopItemWidth();
    ImGui::End();
    ImGui::PopFont();

    if (!open) {
        set_menu(false);
    }
    if (apply && s_apply) {
        s_apply(apply);
    }
}

/* Bottom-centred subtitle lines with a dark outline, sized to the window. */
static void draw_subtitle(const ImGuiViewport *vp) {
    ImFont *font = s_sub_font ? s_sub_font : ImGui::GetFont();
    float size = vp->WorkSize.y / 18.0f;
    float wrap = vp->WorkSize.x * 0.8f;
    ImDrawList *dl = ImGui::GetForegroundDrawList();
    /* Lay out bottom-up so multi-line cues grow upwards. */
    const char *lines[8];
    int n = 0;
    static char buf[1024];
    SDL_strlcpy(buf, s_sub, sizeof buf);
    for (char *p = buf; p && n < 8;) {
        lines[n++] = p;
        p = strchr(p, '\n');
        if (p) {
            *p++ = 0;
        }
    }
    /* A translucent band behind all lines keeps them readable over the game's UI. */
    float total = 0, widest = 0;
    for (int i = 0; i < n; i++) {
        ImVec2 sz = font->CalcTextSizeA(size, FLT_MAX, wrap, lines[i]);
        total += sz.y + size * 0.15f;
        widest = sz.x > widest ? sz.x : widest;
    }
    float bottom = vp->WorkPos.y + vp->WorkSize.y * 0.975f;
    float pad = size * 0.3f;
    dl->AddRectFilled(ImVec2(vp->WorkPos.x + (vp->WorkSize.x - widest) * 0.5f - pad, bottom - total - pad),
                      ImVec2(vp->WorkPos.x + (vp->WorkSize.x + widest) * 0.5f + pad, bottom + pad * 0.5f),
                      IM_COL32(0, 0, 0, 150), size * 0.25f);
    float y = bottom;
    for (int i = n - 1; i >= 0; i--) {
        ImVec2 sz = font->CalcTextSizeA(size, FLT_MAX, wrap, lines[i]);
        y -= sz.y;
        ImVec2 pos(vp->WorkPos.x + (vp->WorkSize.x - sz.x) * 0.5f, y);
        float o = size / 16.0f;
        for (int dx = -1; dx <= 1; dx++) {
            for (int dy = -1; dy <= 1; dy++) {
                if (dx || dy) {
                    dl->AddText(font, size, ImVec2(pos.x + dx * o, pos.y + dy * o), IM_COL32(0, 0, 0, 220),
                                lines[i], nullptr, wrap);
                }
            }
        }
        dl->AddText(font, size, pos, IM_COL32(245, 245, 235, 255), lines[i], nullptr, wrap);
        y -= size * 0.15f;
    }
}

extern "C" void overlay_toast(const char *message, float seconds) {
    SDL_strlcpy(s_text, message, sizeof s_text);
    s_until = SDL_GetPerformanceCounter() + (Uint64)(seconds * (double)SDL_GetPerformanceFrequency());
}

/* The mouse cursor: shown while the F1 menu is open and for a moment after the mouse
 * moves, hidden otherwise so it doesn't sit on the picture (the game ignores the mouse). */
static void update_cursor(void) {
    const Uint64 now = SDL_GetPerformanceCounter();
    const bool recent = s_mouse_moved_at && now - s_mouse_moved_at < SDL_GetPerformanceFrequency() * 5 / 2;
    const int want = (s_menu || recent) ? SDL_ENABLE : SDL_DISABLE;
    if (SDL_ShowCursor(SDL_QUERY) != want) {
        SDL_ShowCursor(want);
    }
}

extern "C" void overlay_draw(void) {
    update_cursor();
    if (s_text[0] && SDL_GetPerformanceCounter() >= s_until) {
        s_text[0] = 0;
    }
    if (!s_text[0] && !s_sub[0] && !s_menu) {
        return;
    }
    if (!s_ready) {
        s_ready = overlay_init();
        if (!s_ready) {
            return;
        }
    }
    if (s_font_dirty) {
        load_sub_font();
    }
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplSDL2_NewFrame();
    ImGui::NewFrame();

    const ImGuiViewport *vp = ImGui::GetMainViewport();
    if (vp->WorkSize.x < 64.0f || vp->WorkSize.y < 64.0f) {
        /* Minimized (or tiny): fonts would be sized from a zero-height window. */
        ImGui::EndFrame();
        return;
    }
    if (s_sub[0]) {
        draw_subtitle(vp);
    }
    if (s_menu) {
        draw_menu(vp);
    }
    if (!s_text[0]) {
        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        return;
    }
    ImGui::SetNextWindowPos(ImVec2(vp->WorkPos.x + vp->WorkSize.x * 0.5f, vp->WorkPos.y + 24.0f), ImGuiCond_Always,
                            ImVec2(0.5f, 0.0f));
    ImGui::SetNextWindowBgAlpha(0.75f);
    ImGui::Begin("##toast", nullptr,
                 ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoInputs |
                     ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing |
                     ImGuiWindowFlags_NoNav);
    ImGui::SetWindowFontScale(vp->WorkSize.y / 360.0f > 1.5f ? vp->WorkSize.y / 360.0f : 1.5f); /* ~1/25 of the height */
    ImGui::TextUnformatted(s_text);
    ImGui::End();

    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}
