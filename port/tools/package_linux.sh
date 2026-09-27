#!/bin/bash
# Builds a self-contained Linux release (x86_64) in an Ubuntu 22.04 container
# (glibc 2.35, so it runs on most current distributions). SDL2 and OpenAL are
# linked statically; SDL loads X11/Wayland and the sound system at run time.
#   port/tools/package_linux.sh            -> dist/Lain-linux-x86_64.tar.gz
# The archive contains no game data: players import their own discs on first launch.
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
docker run --rm --platform linux/amd64 -v "$STAGE/port":/src:ro -v "$STAGE/LICENSE":/lic/LICENSE:ro -v "$STAGE/thirdparty":/lic/thirdparty:ro -v "$PWD/dist":/out ubuntu:22.04 bash -euc '
export DEBIAN_FRONTEND=noninteractive
apt-get update -qq >/dev/null
apt-get install -y -qq build-essential python3-pip ninja-build git pkg-config libgl-dev \
    libx11-dev libxext-dev libxrandr-dev libxcursor-dev libxi-dev libxss-dev libxkbcommon-dev \
    libwayland-dev wayland-protocols libdecor-0-dev libasound2-dev libpulse-dev libgtk-3-dev >/dev/null
# openal-soft needs CMake >= 3.26 and C++20 ranges (GCC 13); 22.04 ships 3.22 and GCC 11.
# libstdc++ is linked statically, so the newer compiler adds no runtime dependency.
pip3 install -q "cmake>=3.26" >/dev/null 2>&1
apt-get install -y -qq software-properties-common >/dev/null
add-apt-repository -y ppa:ubuntu-toolchain-r/test >/dev/null 2>&1
apt-get update -qq >/dev/null && apt-get install -y -qq gcc-13 g++-13 >/dev/null
export CC=gcc-13 CXX=g++-13
cmake -S /src -B /tmp/l -G Ninja -DCMAKE_BUILD_TYPE=Release -DLAIN_STATIC_DEPS=ON -Wno-dev >/dev/null
cmake --build /tmp/l >/tmp/build.log 2>&1 || { tail -30 /tmp/build.log; exit 1; }
strip /tmp/l/lain
echo "== shared libraries (glibc symbol versions: $(objdump -T /tmp/l/lain | grep -o "GLIBC_[0-9.]*" | sort -uV | tail -1))"
ldd /tmp/l/lain | sed "s/ (0x.*//"
if ldd /tmp/l/lain | grep -qiE "libSDL2|libopenal|libstdc"; then
    echo "error: lain still links SDL2/OpenAL dynamically" >&2; exit 1
fi
mkdir -p /tmp/pkg/Lain && cp /tmp/l/lain /tmp/pkg/Lain/
cp /src/src/lain.png /tmp/pkg/Lain/lain.png && cp /lic/LICENSE /tmp/pkg/Lain/LICENSE.txt && cp -R /lic/thirdparty /tmp/pkg/Lain/thirdparty
cat > /tmp/pkg/Lain/README.txt <<EOF
Serial Experiments Lain (native client)

Run ./lain. On first launch, add your own disc images (.cue/.bin) of the
Japanese PlayStation release; nothing from the game is included here.
Subtitle and dub packs go in the data folder shown in the setup window
(~/.local/share/LainNative/lain-native/). Needs OpenGL 3.2.
EOF
tar -C /tmp/pkg -czf /out/Lain-linux-x86_64.tar.gz Lain
'
ls -la dist/Lain-linux-x86_64.tar.gz
