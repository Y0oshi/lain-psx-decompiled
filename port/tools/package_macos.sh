#!/bin/bash
# Builds a self-contained Lain.app (Release) with its Homebrew libraries inside.
#   port/tools/package_macos.sh            -> dist/Lain.app, dist/Lain-macos.zip
# The app contains no game data: players import their own discs on first launch.
set -euo pipefail
cd "$(dirname "$0")/../.."
ROOT=$PWD
BUILD=$ROOT/build/port-static
APP=$ROOT/dist/Lain.app

# SDL2 and OpenAL are linked statically (LAIN_STATIC_DEPS): no Homebrew needed to run.
cmake -S port -B "$BUILD" -G Ninja -DCMAKE_BUILD_TYPE=Release -DLAIN_STATIC_DEPS=ON -Wno-dev >/dev/null
cmake --build "$BUILD"

rm -rf "$APP"
mkdir -p "$APP/Contents/MacOS" "$APP/Contents/Frameworks" "$APP/Contents/Resources"
cp "$BUILD/lain" "$APP/Contents/MacOS/lain"
cp "$ROOT/LICENSE" "$APP/Contents/Resources/LICENSE.txt"
cp -R "$ROOT/thirdparty" "$APP/Contents/Resources/thirdparty"

# Safety net: copy any non-system dylib the binary still needs (recursively) and
# point the references at the bundle. With static deps there should be none.
is_external() { [[ "$1" == /opt/homebrew/* || "$1" == /usr/local/* ]]; }
queue=("$APP/Contents/MacOS/lain")
seen=" "   # copied dylib names (macOS bash 3.2 has no associative arrays)
while ((${#queue[@]})); do
    file=${queue[0]}
    queue=("${queue[@]:1}")
    while read -r dep; do
        is_external "$dep" || continue
        name=$(basename "$dep")
        if [[ "$seen" != *" $name "* ]]; then
            seen="$seen$name "
            cp "$(realpath "$dep")" "$APP/Contents/Frameworks/$name"
            chmod u+w "$APP/Contents/Frameworks/$name"
            install_name_tool -id "@executable_path/../Frameworks/$name" "$APP/Contents/Frameworks/$name"
            queue+=("$APP/Contents/Frameworks/$name")
        fi
        install_name_tool -change "$dep" "@executable_path/../Frameworks/$name" "$file"
    done < <(otool -L "$file" | tail -n +2 | awk '{print $1}')
done

# App icon: port/src/lain.png scaled into an iconset (nearest neighbour keeps the pixels sharp).
ICONSET=$(mktemp -d)/lain.iconset
mkdir -p "$ICONSET" "$APP/Contents/Resources"
for sz in 16 32 128 256 512; do
    sips -z $sz $sz "$ROOT/port/src/lain.png" --out "$ICONSET/icon_${sz}x${sz}.png" >/dev/null
    sips -z $((sz * 2)) $((sz * 2)) "$ROOT/port/src/lain.png" --out "$ICONSET/icon_${sz}x${sz}@2x.png" >/dev/null
done
iconutil -c icns "$ICONSET" -o "$APP/Contents/Resources/lain.icns"

cat > "$APP/Contents/Info.plist" <<'EOF'
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
    <key>CFBundleName</key><string>Serial Experiments Lain</string>
    <key>CFBundleDisplayName</key><string>Serial Experiments Lain</string>
    <key>CFBundleExecutable</key><string>lain</string>
    <key>CFBundleIconFile</key><string>lain</string>
    <key>CFBundleIdentifier</key><string>io.github.y0oshi.lain-psx-decompiled</string>
    <key>CFBundlePackageType</key><string>APPL</string>
    <key>CFBundleShortVersionString</key><string>0.1</string>
    <key>CFBundleVersion</key><string>1</string>
    <key>LSMinimumSystemVersion</key><string>12.0</string>
    <key>NSHighResolutionCapable</key><true/>
</dict>
</plist>
EOF

# Apple Silicon requires a signature; ad-hoc is enough to run locally. Copied
# files carry extended attributes (provenance, Finder info) codesign rejects.
# iCloud-synced folders can re-add Finder info right after a write: clear and retry.
for attempt in 1 2 3; do
    xattr -cr "$APP"
    codesign --force --deep --sign - "$APP" && break
    [ $attempt = 3 ] && exit 1
    sleep 1
done

if otool -L "$APP/Contents/MacOS/lain" $(ls "$APP"/Contents/Frameworks/*.dylib 2>/dev/null) | grep -qE "/opt/homebrew|/usr/local"; then
    echo "error: bundle still references Homebrew paths" >&2
    exit 1
fi
(cd "$ROOT/dist" && rm -f Lain-macos.zip && ditto -c -k --keepParent Lain.app Lain-macos.zip)
echo "built $APP and dist/Lain-macos.zip"
