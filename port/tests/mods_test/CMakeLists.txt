# Tests for the mod support (napk, PNG, TIM, the mods disc reader):
#   cmake -S port/tests/mods_test -B build/tests/mods_test -G Ninja
#   cmake --build build/tests/mods_test
#   build/tests/mods_test/mods_test
#   python3 port/tests/mods_test/gen_pngs.py build/tests/mods_test/pngs
#   build/tests/mods_test/mods_test --pngs build/tests/mods_test/pngs
#   build/tests/mods_test/mods_test --reader <data dir with discs and mods>
# The reader test uses the player's own imported discs; no game data is part of
# the source tree.
cmake_minimum_required(VERSION 3.20)
project(mods_test C CXX ASM)

# Same settings as port/CMakeLists.txt.
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
if(APPLE)
    list(APPEND CMAKE_PREFIX_PATH /opt/homebrew/opt/openal-soft)
endif()
set(PSX_LAIN_EXT ON CACHE BOOL "" FORCE)

add_subdirectory(../../psx psx)
set(PORT ${CMAKE_CURRENT_SOURCE_DIR}/../..)
add_executable(mods_test main.c stubs.c
    ${PORT}/client/mods.cpp ${PORT}/client/moddata.cpp ${PORT}/client/json.cpp ${PORT}/client/tracks.c
    ${PORT}/client/napk.c ${PORT}/client/png.c ${PORT}/client/tim.c
    ${PORT}/client/iso.c ${PORT}/client/settings.c ${PORT}/game/generated/psx_arena.S)
target_include_directories(mods_test PRIVATE ${PORT}/client ${PORT}/game/include)
target_link_libraries(mods_test PRIVATE psx)
