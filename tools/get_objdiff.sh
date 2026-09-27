#!/bin/bash
# Download objdiff-cli for this host into tools/bin/ (git-ignored) and print its path.
#   tools/get_objdiff.sh
set -eo pipefail
VERSION=${OBJDIFF_VERSION:-v3.8.1}
cd "$(dirname "$0")/.."
case "$(uname -s)-$(uname -m)" in
    Linux-x86_64)                 plat=linux-x86_64 ;;
    Linux-aarch64 | Linux-arm64)  plat=linux-aarch64 ;;
    Darwin-arm64)                 plat=macos-arm64 ;;
    Darwin-x86_64)                plat=macos-x86_64 ;;
    *) echo "unsupported host: $(uname -s)-$(uname -m)" >&2; exit 1 ;;
esac
bin=tools/bin/objdiff-cli-$VERSION-$plat
if [ ! -x "$bin" ]; then
    mkdir -p tools/bin
    echo "downloading objdiff-cli $VERSION ($plat)" >&2
    curl -sSfL -o "$bin.tmp" \
        "https://github.com/encounter/objdiff/releases/download/$VERSION/objdiff-cli-$plat"
    chmod +x "$bin.tmp"
    mv "$bin.tmp" "$bin"
fi
echo "$bin"
