#!/bin/bash
# Cross-compiles the native client for Windows (x86_64, MinGW-w64) inside an
# Ubuntu container, to catch Windows build problems from a Mac/Linux machine:
#   docker run --rm -v "$PWD/port":/src:ro -v "$PWD/port/tools":/scr ubuntu:24.04 /scr/cross_windows_docker.sh
# Uses the official SDL2 MinGW kit and openal-soft Windows binaries.
set -u
export DEBIAN_FRONTEND=noninteractive
apt-get update -qq >/dev/null 2>&1
apt-get install -y -qq mingw-w64 cmake ninja-build curl unzip python3 >/dev/null 2>&1
cd /opt
curl -sSL https://github.com/libsdl-org/SDL/releases/download/release-2.32.10/SDL2-devel-2.32.10-mingw.tar.gz | tar xz
curl -sSL -o oal.zip https://github.com/kcat/openal-soft/releases/download/1.25.2/openal-soft-1.25.2-bin.zip && unzip -q oal.zip
OAL=/opt/openal-soft-1.25.2-bin
cat > /opt/tc.cmake <<'EOF'
set(CMAKE_SYSTEM_NAME Windows)
set(CMAKE_C_COMPILER x86_64-w64-mingw32-gcc-posix)
set(CMAKE_CXX_COMPILER x86_64-w64-mingw32-g++-posix)
set(CMAKE_RC_COMPILER x86_64-w64-mingw32-windres)
set(CMAKE_FIND_ROOT_PATH /usr/x86_64-w64-mingw32 /opt/SDL2-2.32.10/x86_64-w64-mingw32)
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)
EOF
echo "== configure"
cmake -S /src -B /tmp/w -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_TOOLCHAIN_FILE=/opt/tc.cmake \
  -DSDL2_DIR=/opt/SDL2-2.32.10/x86_64-w64-mingw32/lib/cmake/SDL2 \
  -DOPENAL_INCLUDE_DIR=$OAL/include/AL -DOPENAL_LIBRARY=$OAL/libs/Win64/libOpenAL32.dll.a -Wno-dev 2>&1 | tail -6
echo "== build"
cmake --build /tmp/w -- -k 0 > /tmp/build.log 2>&1
grep -E " error|FAILED|undefined reference" /tmp/build.log | sed -E "s|/src/||" | sort | uniq -c | sort -rn | head -30
ls -la /tmp/w/lain.exe 2>/dev/null || echo "no lain.exe"
