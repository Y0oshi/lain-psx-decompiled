#!/bin/bash
# Smoke-tests the Windows release (dist/Lain-windows.zip) under Wine, Xvfb and
# Mesa's software OpenGL, in an x86_64 Ubuntu container:
#   port/tools/test_windows_wine.sh <folder with disc1.bin/disc2.bin>
# Runs --selftest, then boots the game for 700 frames and saves
# dist/wine-test/win-{300,700}.bmp and play.log. No sound device in the container.
set -euo pipefail
cd "$(dirname "$0")/../.."
DISCS=$(cd "$1" && pwd)
mkdir -p dist/wine-test
docker run --rm --platform linux/amd64 -v "$DISCS":/discs:ro -v "$PWD/dist":/dist:ro \
    -v "$PWD/dist/wine-test":/out ubuntu:24.04 bash -uc '
export DEBIAN_FRONTEND=noninteractive
apt-get update -qq >/dev/null 2>&1
apt-get install -y -qq wine64 xvfb unzip libgl1-mesa-dri libgl1 libopengl0 libglx-mesa0 \
    libxrender1 libxcomposite1 libxi6 libxrandr2 libxcursor1 libxinerama1 libxext6 libxfixes3 \
    libfreetype6 libfontconfig1 >/dev/null 2>&1
Xvfb :7 -screen 0 1280x1024x24 >/dev/null 2>&1 &
sleep 2
export DISPLAY=:7 WINEDEBUG=err+all WINEPREFIX=/root/.wine
wineboot -i >/dev/null 2>&1
D="$WINEPREFIX/drive_c/users/root/AppData/Roaming/LainNative/lain-native"
mkdir -p "$D" /tmp/app
ln -sf /discs/disc1.bin "$D/disc1.bin"
[ -f /discs/disc2.bin ] && ln -sf /discs/disc2.bin "$D/disc2.bin"
cd /tmp/app && unzip -q /dist/Lain-windows.zip
echo "== selftest"
wine Lain/lain.exe --selftest 2>&1 | tail -1
echo "== play (700 frames)"
LAIN_SCREENSHOT="Z:\\out\\win" LAIN_SCREENSHOT_FRAMES=300,700 LAIN_EXIT_FRAME=702 LAIN_FRAME_LOG=1 \
    timeout 900 wine Lain/lain.exe --play > /out/play.log 2>&1
echo "exit $?"
grep -E "OpenGL version|frame 700|saved|lain:" /out/play.log
'
