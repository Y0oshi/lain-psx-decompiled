# Standalone test for the port's CD-XA ADPCM audio (port/psx/src/lain_xa):
#   cmake -S port/tests/xa_decode -B build/tests/xa_decode -G Ninja
#   cmake --build build/tests/xa_decode
#   build/tests/xa_decode/xa_decode            decode one channel to WAV + stats
#   build/tests/xa_decode/xa_decode --drive    same channel through libcd (paced drive), offline
#   build/tests/xa_decode/xa_decode --live 8   play it through libcd + the SPU for 8 s
# Reads extract/disc1/XA/LAIN01.XA (the user's own disc dump) at run time and
# writes under the build directory; no game data is part of the source tree.
cmake_minimum_required(VERSION 3.20)
project(xa_decode C CXX)

# Same settings as port/CMakeLists.txt.
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
if(APPLE)
    list(APPEND CMAKE_PREFIX_PATH /opt/homebrew/opt/openal-soft)
endif()

add_subdirectory(../../psx psx)

get_filename_component(LAIN_REPO_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../../.." ABSOLUTE)
add_executable(xa_decode main.c)
target_link_libraries(xa_decode PRIVATE psx)
target_compile_definitions(xa_decode PRIVATE
    LAIN_REPO_ROOT="${LAIN_REPO_ROOT}"
    XA_DECODE_OUT="${CMAKE_CURRENT_BINARY_DIR}/out")
