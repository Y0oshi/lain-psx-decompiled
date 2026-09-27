#!/bin/bash
# Builds a self-contained Windows release (x86_64) in an Ubuntu container with
# MinGW-w64: SDL2, OpenAL and the MinGW runtime are all linked statically, so
# lain.exe needs only DLLs that ship with Windows.
#   port/tools/package_windows.sh          -> dist/Lain-windows.zip
# The zip contains no game data: players import their own discs on first launch.
set -euo pipefail
cd "$(dirname "$0")/../.."
mkdir -p dist
# Build from a staged copy: Docker's file sharing can't read files macOS has offloaded
# to the cloud ("dataless", e.g. iCloud Desktop & Documents with Optimize Storage), while
# a normal copy on the host brings their contents back.
STAGE=$(mktemp -d)
trap 'rm -rf "$STAGE"' EXIT
cp -R port "$STAGE/port"
cp LICENSE "$STAGE/LICENSE"
cp -R thirdparty "$STAGE/thirdparty"
docker run --rm -v "$STAGE/port":/src:ro -v "$STAGE/LICENSE":/lic/LICENSE:ro -v "$STAGE/thirdparty":/lic/thirdparty:ro -v "$PWD/dist":/out ubuntu:24.04 bash -euc '
export DEBIAN_FRONTEND=noninteractive
apt-get update -qq >/dev/null
apt-get install -y -qq mingw-w64 cmake ninja-build git zip >/dev/null
cat > /opt/tc.cmake <<EOF
set(CMAKE_SYSTEM_NAME Windows)
set(CMAKE_C_COMPILER x86_64-w64-mingw32-gcc-posix)
set(CMAKE_CXX_COMPILER x86_64-w64-mingw32-g++-posix)
set(CMAKE_RC_COMPILER x86_64-w64-mingw32-windres)
set(CMAKE_FIND_ROOT_PATH /usr/x86_64-w64-mingw32)
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)
EOF
cmake -S /src -B /tmp/w -G Ninja -DCMAKE_BUILD_TYPE=Release -DLAIN_STATIC_DEPS=ON \
  -DCMAKE_TOOLCHAIN_FILE=/opt/tc.cmake -Wno-dev >/dev/null
cmake --build /tmp/w >/tmp/build.log 2>&1 || { tail -30 /tmp/build.log; exit 1; }
x86_64-w64-mingw32-strip /tmp/w/lain.exe
if x86_64-w64-mingw32-objdump -p /tmp/w/lain.exe | grep -iE "DLL Name: (SDL2|OpenAL|libstdc|libgcc|libwinpthread)"; then
    echo "error: lain.exe still depends on a non-system DLL" >&2; exit 1
fi
mkdir -p /tmp/pkg/Lain && cp /tmp/w/lain.exe /tmp/pkg/Lain/
cp /lic/LICENSE /tmp/pkg/Lain/LICENSE.txt && cp -R /lic/thirdparty /tmp/pkg/Lain/thirdparty
cat > /tmp/pkg/Lain/README.txt <<EOF
Serial Experiments Lain (native client)

Run lain.exe. On first launch, add your own disc images (.cue/.bin) of the
Japanese PlayStation release; nothing from the game is included here.
Subtitle and dub packs go in the data folder shown in the setup window.
EOF
cd /tmp/pkg && rm -f /out/Lain-windows.zip && zip -qr /out/Lain-windows.zip Lain
'
ls -la dist/Lain-windows.zip
