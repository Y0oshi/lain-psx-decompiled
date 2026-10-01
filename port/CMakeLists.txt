# Native (Windows/macOS/Linux) client for the Serial Experiments Lain decompilation.
# The game code runs natively; psx/ (PsyCross + Lain additions) replaces the
# PlayStation SDK. Game data and media are read from the player's own disc dump.
cmake_minimum_required(VERSION 3.20)
project(lain_native C CXX ASM)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
if(APPLE)
    list(APPEND CMAKE_PREFIX_PATH /opt/homebrew/opt/openal-soft)
endif()

# The Lain additions (MDEC/STR, XA + CD drive, software SPU/libsnd, libgs) are
# what the game links against.
set(PSX_LAIN_EXT ON CACHE BOOL "Build the Lain additions (src/lain_*)" FORCE)
# Release builds link SDL2 and OpenAL statically from pinned source releases, so
# the executable needs no system libraries (and sdl2-compat's run-time SDL3
# lookup never happens). Development builds use the system packages.
option(LAIN_STATIC_DEPS "Build SDL2 and openal-soft from source as static libraries" OFF)
if(LAIN_STATIC_DEPS)
    include(FetchContent)
    set(SDL_SHARED OFF CACHE BOOL "" FORCE)
    set(SDL_STATIC ON CACHE BOOL "" FORCE)
    set(SDL_TEST OFF CACHE BOOL "" FORCE)
    set(SDL_TESTS OFF CACHE BOOL "" FORCE)
    FetchContent_Declare(sdl2
        URL https://github.com/libsdl-org/SDL/releases/download/release-2.32.10/SDL2-2.32.10.tar.gz)
    set(LIBTYPE STATIC CACHE STRING "" FORCE)
    foreach(opt ALSOFT_UTILS ALSOFT_EXAMPLES ALSOFT_TESTS ALSOFT_INSTALL ALSOFT_INSTALL_CONFIG
                ALSOFT_INSTALL_HRTF_DATA ALSOFT_INSTALL_AMBDEC_PRESETS ALSOFT_INSTALL_EXAMPLES
                ALSOFT_INSTALL_UTILS
                # Optional backends would pick up whatever the build machine has
                # installed (e.g. Homebrew PortAudio); keep only each OS's native one.
                ALSOFT_BACKEND_PORTAUDIO ALSOFT_BACKEND_JACK ALSOFT_BACKEND_SDL2
                ALSOFT_BACKEND_SDL3 ALSOFT_BACKEND_SNDIO)
        set(${opt} OFF CACHE BOOL "" FORCE)
    endforeach()
    FetchContent_Declare(openal
        URL https://github.com/kcat/openal-soft/archive/refs/tags/1.25.2.tar.gz)
    FetchContent_MakeAvailable(sdl2 openal)
    # Expose them under the names the rest of the build uses.
    if(NOT TARGET SDL2::SDL2)  # SDL 2.32 defines it for static-only builds
        add_library(lain_sdl2 INTERFACE)
        target_link_libraries(lain_sdl2 INTERFACE SDL2::SDL2-static)
        add_library(SDL2::SDL2 ALIAS lain_sdl2)
    endif()
    if(NOT TARGET OpenAL::OpenAL)
        add_library(OpenAL::OpenAL ALIAS OpenAL)
    endif()
    target_compile_definitions(OpenAL INTERFACE AL_LIBTYPE_STATIC)
    # openal-soft builds with -Werror; newer clang adds warnings it doesn't expect.
    target_compile_options(OpenAL PRIVATE $<$<CXX_COMPILER_ID:Clang,AppleClang>:-Wno-error=function-effects>)
endif()

add_subdirectory(psx)

# Dear ImGui (MIT) with SDL2 + OpenGL3 backends: launcher/setup and in-game overlays.
add_library(imgui STATIC
    external/imgui/imgui.cpp external/imgui/imgui_draw.cpp external/imgui/imgui_tables.cpp
    external/imgui/imgui_widgets.cpp
    external/imgui/backends/imgui_impl_sdl2.cpp external/imgui/backends/imgui_impl_opengl3.cpp)
target_include_directories(imgui PUBLIC external/imgui external/imgui/backends)
if(NOT TARGET SDL2::SDL2)
    find_package(SDL2 REQUIRED)
endif()
find_package(OpenGL REQUIRED)
target_link_libraries(imgui PUBLIC SDL2::SDL2 OpenGL::GL)

# tinyfiledialogs (zlib): native "open file" dialogs.
add_library(tinyfd STATIC external/tinyfiledialogs/tinyfiledialogs.c)
target_include_directories(tinyfd PUBLIC external/tinyfiledialogs)

# The PS1 RAM arena with every game global at its original offset (generated).
add_library(psx_arena STATIC game/generated/psx_arena.S)

# Lua (MIT): mod scripts.
file(GLOB LUA_SRCS external/lua/*.c)
add_library(lua STATIC ${LUA_SRCS})
target_include_directories(lua PUBLIC external/lua)
if(NOT WIN32)
    target_compile_definitions(lua PRIVATE LUA_USE_POSIX)
endif()

add_library(client STATIC
    client/discs.c client/settings.c client/sha1.c client/setup_ui.cpp
    client/iso.c client/psx_mem.c client/overlay.cpp client/packs.cpp client/sub_download.cpp client/unused_gallery.cpp client/app_icon.c
    client/tracks.c client/subtitles.c client/subtitle_kit.c client/dub.c client/dub_player.c
    client/mods.cpp client/mods_ui.cpp client/modrt.cpp client/moddata.cpp client/hdtex.cpp client/json.cpp client/napk.c client/png.c client/tim.c
    external/stb/stb_vorbis_impl.c)
target_include_directories(client PUBLIC client game/include external/stb)
target_link_libraries(client PUBLIC imgui tinyfd psx lua SDL2::SDL2)

# The ported game code (every function has C; see game/PORT_TYPES.md). It is
# compiled from copies in which every game function can be replaced at run time
# by mods (tools/gen_hooks.py; game/*.c is unchanged).
find_package(Python3 REQUIRED COMPONENTS Interpreter)
file(GLOB GAME_SRCS CONFIGURE_DEPENDS game/*.c)
set(HOOKS_DIR ${CMAKE_CURRENT_BINARY_DIR}/hooks)
set(GAME_HOOKED_SRCS ${HOOKS_DIR}/game_hooks.c)
foreach(src ${GAME_SRCS})
    get_filename_component(name ${src} NAME)
    list(APPEND GAME_HOOKED_SRCS ${HOOKS_DIR}/game/${name})
endforeach()
add_custom_command(OUTPUT ${GAME_HOOKED_SRCS}
    COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tools/gen_hooks.py ${HOOKS_DIR}
    DEPENDS ${GAME_SRCS} tools/gen_hooks.py game/include/game_protos.h game/generated/symbols.txt game/PORT_TYPES.md
    COMMENT "Generating hookable game functions")
add_library(game STATIC ${GAME_HOOKED_SRCS})
target_include_directories(game PUBLIC game/include)
target_compile_definitions(game PRIVATE NON_MATCHING NEEDS_RODATA NEEDS_SDATA)
target_link_libraries(game PUBLIC psx)
# The game's main() becomes lain_game_main(), called by src/main.c.
set_source_files_properties(${HOOKS_DIR}/game/80013138.c PROPERTIES COMPILE_DEFINITIONS main=lain_game_main)

add_executable(lain src/main.c src/platform.c src/sdk_extra.c)
target_link_libraries(lain PRIVATE client game psx psx_arena)
if(WIN32)
    # UTF-8 code page for fopen/system/*A calls (see src/lain.manifest).
    target_sources(lain PRIVATE src/lain.rc)
    # SDL renames main() to SDL_main; SDL2main provides the Windows entry point.
    target_link_libraries(lain PRIVATE SDL2::SDL2main)
endif()
if(LAIN_STATIC_DEPS AND CMAKE_SYSTEM_NAME STREQUAL "Linux")
    # Release: no dependency on the build machine's (newer) libstdc++.
    target_link_options(lain PRIVATE -static-libgcc -static-libstdc++)
endif()
if(MINGW AND LAIN_STATIC_DEPS)
    # Release: no MinGW runtime DLLs (libstdc++, libgcc, winpthread) next to lain.exe.
    target_link_options(lain PRIVATE -static -static-libgcc -static-libstdc++)
endif()

# Debug aid: print every game function the first time it runs (src/trace.c).
option(LAIN_TRACE "Trace game function entries" OFF)
if(LAIN_TRACE)
    target_compile_options(game PRIVATE -finstrument-functions)
    target_sources(lain PRIVATE src/trace.c)
    target_link_options(lain PRIVATE -rdynamic)
endif()
