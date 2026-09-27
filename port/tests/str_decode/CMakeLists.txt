# Standalone test: decodes frames of a real STR movie through the port's
# libpress (software MDEC) and St* streaming, writing PNGs to out/.
#   cmake -S port/tests/str_decode -B build/tests/str_decode -G Ninja
#   cmake --build build/tests/str_decode && build/tests/str_decode/str_decode
# Reads extract/disc1/MOVIE/F001.STR (the user's own disc dump) at run time;
# no game data is part of the source tree.
cmake_minimum_required(VERSION 3.20)
project(str_decode C CXX ASM)

# Same settings as port/CMakeLists.txt.
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
if(APPLE)
    list(APPEND CMAKE_PREFIX_PATH /opt/homebrew/opt/openal-soft)
endif()

add_subdirectory(../../psx psx)

get_filename_component(LAIN_REPO_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../../.." ABSOLUTE)
add_executable(str_decode main.c lain_vlc.c png.c)
target_link_libraries(str_decode PRIVATE psx)
target_compile_definitions(str_decode PRIVATE
    LAIN_REPO_ROOT="${LAIN_REPO_ROOT}"
    STR_DECODE_OUT="${CMAKE_CURRENT_BINARY_DIR}/out")
