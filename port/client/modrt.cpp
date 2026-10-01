#include "modrt.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include <algorithm>
#include <filesystem>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include <SDL.h>

#include "game_hooks.h"
#include "imgui.h"
#include "lain_mod_api.h"
#include "mods.h"
#include "overlay.h"
#include "psx_arena.h"
#include "settings.h"

extern "C" {
#include "lauxlib.h"
#include "lua.h"
#include "lualib.h"
}

namespace fs = std::filesystem;

extern "C" int lain_frame_count(void);

namespace {

struct Handler {
    struct Mod *mod;
    int ref;          /* Lua function in the mod's registry */
    bool dead = false; /* stopped after an error */
};

struct Mod {
    std::string name, folder, dir;
    lua_State *L = nullptr;
    std::vector<Handler> frame, draw, menu, quit;
    std::vector<std::string> log;
    std::vector<void *> plugins;       /* SDL_LoadObject handles */
    std::vector<LainModQuitFn> plugin_quit;
};

std::vector<std::unique_ptr<Mod>> g_mods;
std::vector<std::vector<Handler>> g_lua_hooks;  /* per game function, in load order */
std::vector<void *> g_typed_top;                /* per game function: top of the plugin chain */
SDL_threadID g_main_thread;
bool g_drawing;                                 /* inside modrt_draw/menu: lain.ui works */
struct Callback {
    void (*fn)(void *);
    void *user;
};
std::vector<Callback> g_plugin_frame, g_plugin_draw;
std::map<std::string, int64_t (*)(const int64_t *, int)> g_commands;
std::map<std::string, int> g_fn_index;
std::map<std::string, int> g_sym_index;

void mod_log(Mod *m, const char *text) {
    printf("[mod %s] %s\n", m ? m->name.c_str() : "?", text);
    if (m) {
        m->log.push_back(text);
        if (m->log.size() > 200) m->log.erase(m->log.begin());
    }
}

/* PS1 address (any RAM mirror) -> native pointer; other values are native pointers. */
void *resolve(int64_t v) {
    uint64_t a = (uint64_t)v & 0xFFFFFFFFu;
    if ((uint64_t)v == a) {
        uint32_t off = a & 0x1FFFFFFFu;
        if ((a >> 29 == 0 || a >> 29 == 4 || a >> 29 == 5) && off < PSX_ARENA_SIZE) {
            return psx_arena + off;
        }
    }
    return (void *)(intptr_t)v;
}

/* A native pointer as scripts see it: its PS1 address when it's in PS1 RAM. */
int64_t to_script(int64_t v) {
    uint8_t *p = (uint8_t *)(intptr_t)v;
    if (p >= psx_arena && p < psx_arena + PSX_ARENA_SIZE) {
        return (int64_t)(PSX_RAM_BASE + (uint32_t)(p - psx_arena));
    }
    return v;
}

void refresh_hook(int i) {
    const LainGameFn &f = lain_game_fns[i];
    if (!g_lua_hooks[i].empty()) {
        *f.hook = f.adapter;
    } else {
        *f.hook = g_typed_top[i] == f.orig ? nullptr : g_typed_top[i];
    }
}

/* Calls the hook chain of function i from script handler `level` down. */
int64_t run_level(int i, int level, int64_t *args, int n);

struct NextState {
    int index, level, nargs;
    int64_t args[16];
    bool called;    /* the hook called next */
    int64_t result; /* next's result */
};

int lua_next_call(lua_State *L) {
    NextState *st = (NextState *)lua_touserdata(L, lua_upvalueindex(1));
    const LainGameFn &f = lain_game_fns[st->index];
    int64_t args[16];
    memcpy(args, st->args, sizeof args);
    int given = lua_gettop(L);
    for (int k = 0; k < st->nargs && k < given; k++) {
        int64_t v = lua_isboolean(L, k + 1) ? lua_toboolean(L, k + 1) : (int64_t)luaL_checkinteger(L, k + 1);
        args[k] = f.kinds[k + 1] == 'p' ? (int64_t)(intptr_t)resolve(v) : v;
    }
    int64_t r = run_level(st->index, st->level, args, st->nargs);
    st->called = true;
    st->result = r;
    if (f.kinds[0] == 'v') return 0;
    lua_pushinteger(L, f.kinds[0] == 'p' ? to_script(r) : r);
    return 1;
}

int64_t run_level(int i, int level, int64_t *args, int n) {
    const LainGameFn &f = lain_game_fns[i];
    while (level >= 0 && g_lua_hooks[i][level].dead) level--;
    if (level < 0) {
        return f.call(g_typed_top[i], args);
    }
    /* copied: the hook may add hooks, which moves the vector */
    const Handler h = g_lua_hooks[i][level];
    lua_State *L = h.mod->L;
    int top = lua_gettop(L);
    /* the state stays on the stack below the call, so it outlives next */
    NextState *st = (NextState *)lua_newuserdatauv(L, sizeof(NextState), 0);
    st->index = i;
    st->level = level - 1;
    st->nargs = n;
    st->called = false;
    st->result = 0;
    memcpy(st->args, args, sizeof(int64_t) * (size_t)n);
    lua_rawgeti(L, LUA_REGISTRYINDEX, h.ref);
    lua_pushvalue(L, top + 1);
    lua_pushcclosure(L, lua_next_call, 1);
    for (int k = 0; k < n; k++) {
        lua_pushinteger(L, f.kinds[k + 1] == 'p' ? to_script(args[k]) : args[k]);
    }
    if (lua_pcall(L, n + 1, 1, 0) != LUA_OK) {
        char msg[512];
        snprintf(msg, sizeof msg, "error in hook %s (hook turned off): %s", f.name, lua_tostring(L, -1));
        mod_log(h.mod, msg);
        g_lua_hooks[i][level].dead = true;
        const bool called = st->called;
        const int64_t result = st->result;
        lua_settop(L, top);
        return called ? result : run_level(i, level - 1, args, n);
    }
    int64_t r = 0;
    if (f.kinds[0] != 'v') {
        if (lua_isnil(L, -1)) {
            r = st->result; /* no result given: next's, or 0 when next wasn't called */
        } else {
            int64_t v = lua_isboolean(L, -1) ? lua_toboolean(L, -1) : (int64_t)lua_tointeger(L, -1);
            r = f.kinds[0] == 'p' ? (int64_t)(intptr_t)resolve(v) : v;
        }
    }
    lua_settop(L, top);
    return r;
}

int64_t dispatch(int i, const int64_t *a, int n) {
    int64_t args[16] = {0};
    memcpy(args, a, sizeof(int64_t) * (size_t)n);
    if (SDL_ThreadID() != g_main_thread) {
        /* a CD callback on the drive thread: scripts only run on the main thread */
        return lain_game_fns[i].call(g_typed_top[i], args);
    }
    return run_level(i, (int)g_lua_hooks[i].size() - 1, args, n);
}

Mod *mod_of(lua_State *L) {
    return *(Mod **)lua_getextraspace(L);
}

int fn_index(const char *name) {
    auto it = g_fn_index.find(name);
    return it == g_fn_index.end() ? -1 : it->second;
}

/* lain.* functions */

int l_hook(lua_State *L) {
    const char *name = luaL_checkstring(L, 1);
    luaL_checktype(L, 2, LUA_TFUNCTION);
    int i = fn_index(name);
    if (i < 0) return luaL_error(L, "no game function named %s", name);
    lua_pushvalue(L, 2);
    g_lua_hooks[i].push_back(Handler{mod_of(L), luaL_ref(L, LUA_REGISTRYINDEX)});
    refresh_hook(i);
    return 0;
}

int l_call(lua_State *L) {
    const char *name = luaL_checkstring(L, 1);
    int i = fn_index(name);
    if (i < 0) return luaL_error(L, "no game function named %s", name);
    const LainGameFn &f = lain_game_fns[i];
    int64_t args[16] = {0};
    for (int k = 0; k < f.nargs; k++) {
        int64_t v = lua_isboolean(L, k + 2) ? lua_toboolean(L, k + 2) : (int64_t)luaL_optinteger(L, k + 2, 0);
        args[k] = f.kinds[k + 1] == 'p' ? (int64_t)(intptr_t)resolve(v) : v;
    }
    int64_t r = f.call(f.entry, args);
    if (f.kinds[0] == 'v') return 0;
    lua_pushinteger(L, f.kinds[0] == 'p' ? to_script(r) : r);
    return 1;
}

int l_functions(lua_State *L) {
    lua_createtable(L, lain_game_fn_count, 0);
    for (int i = 0; i < lain_game_fn_count; i++) {
        lua_createtable(L, 0, 2);
        lua_pushstring(L, lain_game_fns[i].name);
        lua_setfield(L, -2, "name");
        lua_pushstring(L, lain_game_fns[i].sig);
        lua_setfield(L, -2, "signature");
        lua_rawseti(L, -2, i + 1);
    }
    return 1;
}

int l_sym(lua_State *L) {
    const char *name = luaL_checkstring(L, 1);
    auto it = g_sym_index.find(name);
    if (it == g_sym_index.end()) {
        int i = fn_index(name);
        if (i < 0) return 0;
        lua_pushinteger(L, (lua_Integer)(intptr_t)lain_game_fns[i].entry);
        lua_pushinteger(L, 0);
        lua_pushstring(L, "function");
        return 3;
    }
    const LainSymbol &s = lain_symbols[it->second];
    lua_pushinteger(L, s.host ? (lua_Integer)(intptr_t)s.host : (lua_Integer)s.addr);
    lua_pushinteger(L, s.size);
    lua_pushstring(L, s.host ? "host" : "ps1");
    return 3;
}

template <typename T> int l_read(lua_State *L) {
    T v;
    memcpy(&v, resolve(luaL_checkinteger(L, 1)), sizeof v);
    lua_pushinteger(L, (lua_Integer)v);
    return 1;
}

template <typename T> int l_write(lua_State *L) {
    T v = (T)luaL_checkinteger(L, 2);
    memcpy(resolve(luaL_checkinteger(L, 1)), &v, sizeof v);
    return 0;
}

int l_read_bytes(lua_State *L) {
    lua_Integer n = luaL_checkinteger(L, 2);
    luaL_argcheck(L, n >= 0 && n <= (1 << 24), 2, "bad length");
    lua_pushlstring(L, (const char *)resolve(luaL_checkinteger(L, 1)), (size_t)n);
    return 1;
}

int l_write_bytes(lua_State *L) {
    size_t n;
    const char *s = luaL_checklstring(L, 2, &n);
    memcpy(resolve(luaL_checkinteger(L, 1)), s, n);
    return 0;
}

int l_read_string(lua_State *L) {
    const char *p = (const char *)resolve(luaL_checkinteger(L, 1));
    lua_Integer max = luaL_optinteger(L, 2, 4096);
    size_t n = 0;
    while ((lua_Integer)n < max && p[n]) n++;
    lua_pushlstring(L, p, n);
    return 1;
}

int l_address(lua_State *L) { /* PS1 address of a native pointer, or the value itself */
    lua_pushinteger(L, to_script(luaL_checkinteger(L, 1)));
    return 1;
}

int l_on(lua_State *L) {
    const char *ev = luaL_checkstring(L, 1);
    luaL_checktype(L, 2, LUA_TFUNCTION);
    Mod *m = mod_of(L);
    std::vector<Handler> *list = !strcmp(ev, "frame") ? &m->frame : !strcmp(ev, "draw") ? &m->draw :
                                 !strcmp(ev, "menu")  ? &m->menu  : !strcmp(ev, "quit") ? &m->quit : nullptr;
    if (!list) return luaL_error(L, "unknown event %s (frame, draw, menu, quit)", ev);
    lua_pushvalue(L, 2);
    list->push_back(Handler{m, luaL_ref(L, LUA_REGISTRYINDEX)});
    return 0;
}

int l_log(lua_State *L) {
    const int n = lua_gettop(L); /* the buffer uses stack slots above the arguments */
    luaL_Buffer b;
    luaL_buffinit(L, &b);
    for (int i = 1; i <= n; i++) {
        if (i > 1) luaL_addchar(&b, ' ');
        luaL_tolstring(L, i, nullptr);
        luaL_addvalue(&b);
    }
    luaL_pushresult(&b);
    mod_log(mod_of(L), lua_tostring(L, -1));
    return 0;
}

int l_toast(lua_State *L) {
    overlay_toast(luaL_checkstring(L, 1), (float)luaL_optnumber(L, 2, 3.0));
    return 0;
}

int l_frame(lua_State *L) {
    lua_pushinteger(L, lain_frame_count());
    return 1;
}

int l_time(lua_State *L) {
    lua_pushnumber(L, SDL_GetTicks64() / 1000.0);
    return 1;
}

std::string mod_path(lua_State *L, const char *rel) {
    fs::path p = fs::u8path(rel);
    return p.is_absolute() ? p.u8string() : (fs::u8path(mod_of(L)->dir) / p).u8string();
}

bool read_all(const std::string &p, std::string &data) {
    FILE *f = fopen(p.c_str(), "rb");
    if (!f) return false;
    char buf[65536];
    size_t n;
    while ((n = fread(buf, 1, sizeof buf, f)) > 0) data.append(buf, n);
    fclose(f);
    return true;
}

bool write_all(const std::string &p, const char *s, size_t n) {
    std::error_code ec;
    fs::create_directories(fs::u8path(p).parent_path(), ec);
    FILE *f = fopen(p.c_str(), "wb");
    bool ok = f && fwrite(s, 1, n, f) == n;
    if (f) ok = fclose(f) == 0 && ok;
    return ok;
}

int push_file(lua_State *L, const std::string &p) {
    std::string data;
    if (!read_all(p, data)) return 0;
    lua_pushlstring(L, data.data(), data.size());
    return 1;
}

int l_read_file(lua_State *L) {
    return push_file(L, mod_path(L, luaL_checkstring(L, 1)));
}

int l_write_file(lua_State *L) {
    size_t n;
    const char *s = luaL_checklstring(L, 2, &n);
    lua_pushboolean(L, write_all(mod_path(L, luaL_checkstring(L, 1)), s, n));
    return 1;
}

/* Per-mod saved data: <data dir>/mods_data/<folder>/<key> */
std::string data_path(lua_State *L, const char *key) {
    std::string k;
    for (const char *c = key; *c; c++) k += strchr("/\\:*?\"<>|", *c) ? '_' : *c;
    return (fs::u8path(settings_data_dir()) / "mods_data" / fs::u8path(mod_of(L)->folder) / fs::u8path(k)).u8string();
}

int l_save(lua_State *L) {
    size_t n;
    const char *s = luaL_checklstring(L, 2, &n);
    lua_pushboolean(L, write_all(data_path(L, luaL_checkstring(L, 1)), s, n));
    return 1;
}

int l_load(lua_State *L) {
    return push_file(L, data_path(L, luaL_checkstring(L, 1)));
}

int l_plugin(lua_State *L) {
    const char *name = luaL_checkstring(L, 1);
    auto it = g_commands.find(name);
    if (it == g_commands.end()) return luaL_error(L, "no plugin command named %s", name);
    int64_t args[16] = {0};
    int n = lua_gettop(L) - 1;
    if (n > 16) n = 16;
    for (int k = 0; k < n; k++) args[k] = lua_isboolean(L, k + 2) ? lua_toboolean(L, k + 2) : luaL_checkinteger(L, k + 2);
    lua_pushinteger(L, it->second(args, n));
    return 1;
}

/* lain.ui: Dear ImGui, inside draw and menu events only */

void ui_check(lua_State *L) {
    if (!g_drawing) luaL_error(L, "lain.ui works only in draw and menu events");
}

int ui_window(lua_State *L) {
    ui_check(L);
    const char *title = luaL_checkstring(L, 1);
    luaL_checktype(L, 2, LUA_TFUNCTION);
    bool visible = ImGui::Begin(title);
    int r = LUA_OK;
    if (visible) {
        lua_pushvalue(L, 2);
        r = lua_pcall(L, 0, 0, 0);
    }
    ImGui::End();
    if (r != LUA_OK) return lua_error(L);
    return 0;
}

int ui_text(lua_State *L) {
    ui_check(L);
    ImGui::TextUnformatted(luaL_checkstring(L, 1));
    return 0;
}

int ui_text_colored(lua_State *L) {
    ui_check(L);
    ImGui::TextColored(ImVec4((float)luaL_checknumber(L, 1), (float)luaL_checknumber(L, 2), (float)luaL_checknumber(L, 3), 1),
                       "%s", luaL_checkstring(L, 4));
    return 0;
}

int ui_button(lua_State *L) {
    ui_check(L);
    lua_pushboolean(L, ImGui::Button(luaL_checkstring(L, 1)));
    return 1;
}

int ui_checkbox(lua_State *L) {
    ui_check(L);
    bool v = lua_toboolean(L, 2);
    ImGui::Checkbox(luaL_checkstring(L, 1), &v);
    lua_pushboolean(L, v);
    return 1;
}

int ui_slider(lua_State *L) {
    ui_check(L);
    if (lua_isinteger(L, 2) && lua_isinteger(L, 3) && lua_isinteger(L, 4)) {
        int v = (int)lua_tointeger(L, 2);
        ImGui::SliderInt(luaL_checkstring(L, 1), &v, (int)lua_tointeger(L, 3), (int)lua_tointeger(L, 4));
        lua_pushinteger(L, v);
    } else {
        float v = (float)luaL_checknumber(L, 2);
        ImGui::SliderFloat(luaL_checkstring(L, 1), &v, (float)luaL_checknumber(L, 3), (float)luaL_checknumber(L, 4));
        lua_pushnumber(L, v);
    }
    return 1;
}

int ui_input(lua_State *L) {
    ui_check(L);
    char buf[1024];
    snprintf(buf, sizeof buf, "%s", luaL_optstring(L, 2, ""));
    ImGui::InputText(luaL_checkstring(L, 1), buf, sizeof buf);
    lua_pushstring(L, buf);
    return 1;
}

int ui_same_line(lua_State *L) {
    ui_check(L);
    ImGui::SameLine();
    return 0;
}

int ui_separator(lua_State *L) {
    ui_check(L);
    ImGui::Separator();
    return 0;
}

int ui_progress(lua_State *L) {
    ui_check(L);
    ImGui::ProgressBar((float)luaL_checknumber(L, 1), ImVec2(-1, 0), luaL_optstring(L, 2, nullptr));
    return 0;
}

/* Text on the screen without a window: x, y in 0..1 of the screen, color 0..1,
 * size in PS1 pixels (the screen is 240 high; default 12). */
int ui_draw_text(lua_State *L) {
    ui_check(L);
    const ImGuiViewport *vp = ImGui::GetMainViewport();
    float x = vp->WorkPos.x + (float)luaL_checknumber(L, 1) * vp->WorkSize.x;
    float y = vp->WorkPos.y + (float)luaL_checknumber(L, 2) * vp->WorkSize.y;
    ImU32 col = IM_COL32((int)(luaL_optnumber(L, 4, 1) * 255), (int)(luaL_optnumber(L, 5, 1) * 255),
                         (int)(luaL_optnumber(L, 6, 1) * 255), 255);
    float size = (float)luaL_optnumber(L, 7, 12) * vp->WorkSize.y / 240.0f;
    ImGui::GetForegroundDrawList()->AddText(ImGui::GetFont(), size, ImVec2(x, y), col, luaL_checkstring(L, 3));
    return 0;
}

int ui_draw_rect(lua_State *L) {
    ui_check(L);
    const ImGuiViewport *vp = ImGui::GetMainViewport();
    auto sx = [&](int i) { return vp->WorkPos.x + (float)luaL_checknumber(L, i) * vp->WorkSize.x; };
    auto sy = [&](int i) { return vp->WorkPos.y + (float)luaL_checknumber(L, i) * vp->WorkSize.y; };
    ImU32 col = IM_COL32((int)(luaL_optnumber(L, 5, 1) * 255), (int)(luaL_optnumber(L, 6, 1) * 255),
                         (int)(luaL_optnumber(L, 7, 1) * 255), (int)(luaL_optnumber(L, 8, 1) * 255));
    ImGui::GetForegroundDrawList()->AddRectFilled(ImVec2(sx(1), sy(2)), ImVec2(sx(3), sy(4)), col);
    return 0;
}

void open_lain_lib(lua_State *L) {
    static const luaL_Reg lib[] = {
        {"hook", l_hook},         {"call", l_call},         {"functions", l_functions},
        {"sym", l_sym},           {"on", l_on},             {"log", l_log},
        {"toast", l_toast},       {"frame", l_frame},       {"time", l_time},       {"read_file", l_read_file},
        {"write_file", l_write_file}, {"save", l_save},     {"load", l_load},
        {"plugin", l_plugin},     {"address", l_address},
        {"read8", l_read<uint8_t>},   {"read16", l_read<uint16_t>},  {"read32", l_read<uint32_t>},
        {"read8s", l_read<int8_t>},   {"read16s", l_read<int16_t>},  {"read32s", l_read<int32_t>},
        {"write8", l_write<uint8_t>}, {"write16", l_write<uint16_t>}, {"write32", l_write<uint32_t>},
        {"read_bytes", l_read_bytes}, {"write_bytes", l_write_bytes}, {"read_string", l_read_string},
        {nullptr, nullptr}};
    static const luaL_Reg ui[] = {
        {"window", ui_window},       {"text", ui_text},     {"text_colored", ui_text_colored},
        {"button", ui_button},       {"checkbox", ui_checkbox}, {"slider", ui_slider},
        {"input", ui_input},         {"same_line", ui_same_line}, {"separator", ui_separator},
        {"progress", ui_progress},   {"draw_text", ui_draw_text}, {"draw_rect", ui_draw_rect},
        {nullptr, nullptr}};
    luaL_newlib(L, lib);
    luaL_newlib(L, ui);
    lua_setfield(L, -2, "ui");
    lua_pushstring(L, mod_of(L)->dir.c_str());
    lua_setfield(L, -2, "dir");
    lua_pushinteger(L, LAIN_MOD_API_VERSION);
    lua_setfield(L, -2, "version");
    lua_setglobal(L, "lain");
    /* print goes to the mod's log */
    lua_pushcfunction(L, l_log);
    lua_setglobal(L, "print");
}

void run_handlers(std::vector<Handler> &list) {
    /* by index: a handler may add handlers, which moves the vector */
    for (size_t k = 0; k < list.size(); k++) {
        if (list[k].dead) continue;
        Mod *mod = list[k].mod;
        lua_State *L = mod->L;
        lua_rawgeti(L, LUA_REGISTRYINDEX, list[k].ref);
        if (lua_pcall(L, 0, 0, 0) != LUA_OK) {
            char msg[512];
            snprintf(msg, sizeof msg, "error (handler turned off): %s", lua_tostring(L, -1));
            mod_log(mod, msg);
            lua_pop(L, 1);
            list[k].dead = true;
        }
    }
}

/* Plugin API */

std::map<const LainModAPI *, Mod *> g_api_mod;

void api_log(const LainModAPI *self, const char *fmt, ...) {
    char buf[1024];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof buf, fmt, ap);
    va_end(ap);
    auto it = g_api_mod.find(self);
    mod_log(it == g_api_mod.end() ? nullptr : it->second, buf);
}

void *api_ps1_ptr(uint32_t addr) {
    uint32_t off = addr & 0x1FFFFFFFu;
    return off < PSX_ARENA_SIZE ? psx_arena + off : nullptr;
}

uint32_t api_ps1_addr(const void *p) {
    const uint8_t *b = (const uint8_t *)p;
    return b >= psx_arena && b < psx_arena + PSX_ARENA_SIZE ? PSX_RAM_BASE + (uint32_t)(b - psx_arena) : 0;
}

int api_symbol(const char *name, uint32_t *addr, uint32_t *size, void **host) {
    auto it = g_sym_index.find(name);
    if (it == g_sym_index.end()) return 0;
    const LainSymbol &s = lain_symbols[it->second];
    if (addr) *addr = s.addr;
    if (size) *size = s.size;
    if (host) *host = s.host;
    return 1;
}

void *api_function(const char *name) {
    int i = fn_index(name);
    return i < 0 ? nullptr : lain_game_fns[i].entry;
}

int api_function_count(void) {
    return lain_game_fn_count;
}

const char *api_function_at(int i, const char **sig) {
    if (i < 0 || i >= lain_game_fn_count) return nullptr;
    if (sig) *sig = lain_game_fns[i].sig;
    return lain_game_fns[i].name;
}

int api_hook(const char *name, void *replacement, void **next) {
    int i = fn_index(name);
    if (i < 0 || !replacement) return -1;
    if (next) *next = g_typed_top[i];
    g_typed_top[i] = replacement;
    refresh_hook(i);
    return 0;
}

void api_on_frame(void (*fn)(void *), void *user) {
    g_plugin_frame.push_back({fn, user});
}

void api_on_draw(void (*fn)(void *), void *user) {
    g_plugin_draw.push_back({fn, user});
}

void *api_imgui_context(void) {
    return ImGui::GetCurrentContext();
}

void api_imgui_allocators(void **alloc_fn, void **free_fn, void **user) {
    ImGuiMemAllocFunc a;
    ImGuiMemFreeFunc f;
    void *u;
    ImGui::GetAllocatorFunctions(&a, &f, &u);
    if (alloc_fn) *alloc_fn = (void *)a;
    if (free_fn) *free_fn = (void *)f;
    if (user) *user = u;
}

void api_add_command(const char *name, int64_t (*fn)(const int64_t *, int)) {
    g_commands[name] = fn;
}

void api_toast(const char *m, float s) {
    overlay_toast(m, s);
}

int api_frame(void) {
    return lain_frame_count();
}

void load_plugins(Mod &m) {
#if defined(_WIN32)
    const char *ext = ".dll";
#elif defined(__APPLE__)
    const char *ext = ".dylib";
#else
    const char *ext = ".so";
#endif
    std::error_code ec;
    fs::path dir = fs::u8path(m.dir) / "plugins";
    if (!fs::is_directory(dir, ec)) return;
    std::vector<fs::path> libs;
    for (auto &e : fs::directory_iterator(dir, ec)) {
        if (e.is_regular_file(ec) && e.path().extension() == ext) libs.push_back(e.path());
    }
    std::sort(libs.begin(), libs.end());
    for (const fs::path &p : libs) {
        void *h = SDL_LoadObject(p.u8string().c_str());
        if (!h) {
            mod_log(&m, (std::string("can't load plugin ") + p.filename().u8string() + ": " + SDL_GetError()).c_str());
            continue;
        }
        LainModInitFn init = (LainModInitFn)SDL_LoadFunction(h, "lain_mod_init");
        if (!init) {
            mod_log(&m, (p.filename().u8string() + " has no lain_mod_init").c_str());
            SDL_UnloadObject(h);
            continue;
        }
        /* one API struct per mod (mod_dir differs); it lives until the game quits */
        LainModAPI *api = new LainModAPI{};
        api->version = LAIN_MOD_API_VERSION;
        api->mod_dir = strdup(m.dir.c_str());
        api->data_dir = settings_data_dir();
        api->log = api_log;
        api->ps1_ptr = api_ps1_ptr;
        api->ps1_addr = api_ps1_addr;
        api->symbol = api_symbol;
        api->function = api_function;
        api->function_count = api_function_count;
        api->function_at = api_function_at;
        api->hook = api_hook;
        api->on_frame = api_on_frame;
        api->on_draw = api_on_draw;
        api->imgui_context = api_imgui_context;
        api->imgui_allocators = api_imgui_allocators;
        api->add_command = api_add_command;
        api->toast = api_toast;
        api->frame = api_frame;
        g_api_mod[api] = &m;
        int r = init(api);
        if (r != 0) {
            mod_log(&m, (p.filename().u8string() + ": lain_mod_init failed").c_str());
            /* not unloaded: init may already have installed hooks or callbacks */
            continue;
        }
        m.plugins.push_back(h);
        if (auto q = (LainModQuitFn)SDL_LoadFunction(h, "lain_mod_quit")) m.plugin_quit.push_back(q);
        mod_log(&m, (std::string("plugin ") + p.filename().u8string() + " loaded").c_str());
    }
}

void load_script(Mod &m) {
    fs::path main = fs::u8path(m.dir) / "main.lua";
    std::error_code ec;
    if (!fs::exists(main, ec)) return;
    lua_State *L = luaL_newstate();
    *(Mod **)lua_getextraspace(L) = &m;
    m.L = L;
    luaL_openlibs(L);
    open_lain_lib(L);
    /* require() finds modules in the mod folder */
    lua_getglobal(L, "package");
    std::string path = (fs::u8path(m.dir) / "?.lua").u8string() + ";" + (fs::u8path(m.dir) / "?" / "init.lua").u8string();
    lua_pushstring(L, path.c_str());
    lua_setfield(L, -2, "path");
    lua_pop(L, 1);
    if (luaL_loadfile(L, main.u8string().c_str()) != LUA_OK || lua_pcall(L, 0, 0, 0) != LUA_OK) {
        mod_log(&m, (std::string("main.lua: ") + lua_tostring(L, -1)).c_str());
        lua_pop(L, 1);
    }
}

void init_tables() {
    if (!g_typed_top.empty()) return;
    g_main_thread = SDL_ThreadID();
    g_lua_hooks.assign(lain_game_fn_count, {});
    g_typed_top.resize(lain_game_fn_count);
    for (int i = 0; i < lain_game_fn_count; i++) {
        g_typed_top[i] = lain_game_fns[i].orig;
        g_fn_index[lain_game_fns[i].name] = i;
    }
    for (int i = 0; i < lain_symbol_count; i++) g_sym_index[lain_symbols[i].name] = i;
    lain_game_dispatch_fn = dispatch;
}

} // namespace

extern "C" int modrt_hook(const char *name, void *replacement, void **next) {
    init_tables();
    return api_hook(name, replacement, next);
}

extern "C" void modrt_start(const char *setting) {
    init_tables();
    for (const ModInfo &info : mods_scan(setting)) {
        if (!info.enabled) continue;
        auto m = std::make_unique<Mod>();
        m->name = info.name;
        m->folder = info.folder;
        m->dir = (fs::u8path(mods_dir()) / fs::u8path(info.folder)).u8string();
        Mod &ref = *m;
        g_mods.push_back(std::move(m));
        load_plugins(ref);
        load_script(ref);
    }
}

extern "C" void modrt_frame(void) {
    for (auto &m : g_mods) run_handlers(m->frame);
    for (const Callback &c : g_plugin_frame) c.fn(c.user);
}

extern "C" int modrt_wants_draw(void) {
    if (!g_plugin_draw.empty()) return 1;
    for (auto &m : g_mods) {
        for (const Handler &h : m->draw) {
            if (!h.dead) return 1;
        }
    }
    return 0;
}

extern "C" void modrt_draw(void) {
    g_drawing = true;
    for (auto &m : g_mods) run_handlers(m->draw);
    for (const Callback &c : g_plugin_draw) c.fn(c.user);
    g_drawing = false;
}

extern "C" void modrt_menu(void) {
    g_drawing = true;
    for (auto &m : g_mods) {
        if (m->menu.empty() && m->log.empty()) continue;
        if (ImGui::CollapsingHeader(m->name.c_str())) {
            ImGui::PushID(m.get());
            run_handlers(m->menu);
            if (!m->log.empty()) {
                ImGui::TextDisabled("Log");
                ImGui::BeginChild("log", ImVec2(0, 120), true);
                for (const std::string &line : m->log) ImGui::TextWrapped("%s", line.c_str());
                if (ImGui::GetScrollY() >= ImGui::GetScrollMaxY()) ImGui::SetScrollHereY(1.0f); /* follow new lines at the bottom */
                ImGui::EndChild();
            }
            ImGui::PopID();
        }
    }
    g_drawing = false;
}

extern "C" void modrt_quit(void) {
    for (auto &m : g_mods) {
        run_handlers(m->quit);
        for (LainModQuitFn q : m->plugin_quit) q();
    }
}
