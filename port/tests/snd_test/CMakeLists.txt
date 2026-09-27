# Standalone test for the lain_snd software SPU + libsnd.
#   cmake -S port/tests/snd_test -B build/tests/snd_test -G Ninja
#   cmake --build build/tests/snd_test && build/tests/snd_test/snd_test [extract/disc1]
# Reads the player's extracted disc at runtime; writes WAVs to
# build/tests/snd_test/out/ (never commit them).
cmake_minimum_required(VERSION 3.20)
project(snd_test C CXX)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
if(APPLE)
    list(APPEND CMAKE_PREFIX_PATH /opt/homebrew/opt/openal-soft)
endif()

add_subdirectory(../../psx psx)

add_executable(snd_test main.c)
target_link_libraries(snd_test PRIVATE psx)
target_compile_definitions(snd_test PRIVATE
    LAIN_REPO_ROOT="${CMAKE_CURRENT_SOURCE_DIR}/../../.."
    SND_TEST_OUT="${CMAKE_CURRENT_BINARY_DIR}/out")
