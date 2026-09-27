#!/bin/bash
# Run a command inside the build container: tools/docker.sh make
# The image is amd64 because the PsyQ-era GCC builds only exist as x86-64 binaries.
# It is tagged with a hash of its inputs and only rebuilt when they change.
set -e
cd "$(dirname "$0")/.."
tag=lain-psx-decompiled:$(cat Dockerfile requirements.txt | shasum | cut -c1-12)
if ! docker image inspect "$tag" >/dev/null 2>&1; then
    docker build -q --platform linux/amd64 -t "$tag" . >/dev/null
fi
# macOS may have offloaded files to the cloud (iCloud "Optimize Mac Storage" on a
# synced Desktop); Docker's file sharing gets I/O errors on those, while reading them
# on the host brings them back. Cheap when everything is already local.
if [ "$(uname)" = Darwin ]; then
    find Makefile include src config tools asm -type f -print0 2>/dev/null | xargs -0 cat >/dev/null 2>&1 || true
fi
exec docker run --rm -v "$PWD":/lain -w /lain "$tag" "$@" \
    2> >(grep -v "does not match the detected host platform" >&2)
