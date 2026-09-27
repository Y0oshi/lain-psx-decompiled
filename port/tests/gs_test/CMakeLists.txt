# Standalone test of the port's libgs (port/psx/src/lain_gs): opens a window,
# draws sprites/lines/polygons from a TIM and TMD models through PsyCross,
# and saves a screenshot.
#   cmake -S port/tests/gs_test -B build/tests/gs_test -G Ninja
#   cmake --build build/tests/gs_test && build/tests/gs_test/gs_test
# Reads the TIM from extract/disc1/BIN.BIN and a TMD from the EXE
# (extract/disc1/SLPS_016.03) of the user's own disc dump at run time; no
# game data is part of the source tree. Output: build/tests/gs_test/out/.
cmake_minimum_required(VERSION 3.20)
project(gs_test C CXX)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
if(APPLE)
    list(APPEND CMAKE_PREFIX_PATH /opt/homebrew/opt/openal-soft)
endif()

add_subdirectory(../../psx psx)

get_filename_component(LAIN_REPO_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../../.." ABSOLUTE)
add_executable(gs_test main.c png_write.c)
target_link_libraries(gs_test PRIVATE psx)
target_compile_definitions(gs_test PRIVATE
    LAIN_REPO_ROOT="${LAIN_REPO_ROOT}"
    GS_TEST_OUT="${CMAKE_CURRENT_BINARY_DIR}/out")
