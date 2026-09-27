# Third-party software

The native client includes or links the following third-party software. Their license
texts are in this folder, and release packages ship a copy of this folder.

| Component | Used for | License | Where | License file |
|---|---|---|---|---|
| [PsyCross](https://github.com/OpenDriver2/PsyCross) | PsyQ SDK re-implementation on SDL2/OpenGL (with local changes marked `lain:`) | MIT | `port/psx/` (vendored) | [PsyCross-LICENSE.txt](PsyCross-LICENSE.txt) |
| [Dear ImGui](https://github.com/ocornut/imgui) 1.92.9 (includes its bundled stb_truetype, stb_rect_pack, stb_textedit) | Launcher and in-game menu | MIT | `port/external/imgui/` (vendored) | [DearImGui-LICENSE.txt](DearImGui-LICENSE.txt) |
| [tinyfiledialogs](https://sourceforge.net/projects/tinyfiledialogs) | Disc file picker | zlib | `port/external/tinyfiledialogs/` (vendored) | [tinyfiledialogs-LICENSE.txt](tinyfiledialogs-LICENSE.txt) |
| [stb_vorbis](https://github.com/nothings/stb) 1.22 | Ogg Vorbis decoding for dub packs | MIT or Public Domain | `port/external/stb/` (vendored) | [stb_vorbis-LICENSE.txt](stb_vorbis-LICENSE.txt) |
| [glad](https://github.com/Dav1dde/glad) 0.1.36 output | OpenGL loader (part of PsyCross) | Public Domain / WTFPL / CC0; Khronos parts Apache-2.0 | `port/psx/src/render/glad.c` | [glad-NOTICE.txt](glad-NOTICE.txt) |
| [SDL2](https://www.libsdl.org/) 2.32.10 | Windowing, input, audio output | zlib | release builds link it statically (fetched at build time, `LAIN_STATIC_DEPS`); dev builds use the system SDL2 | [SDL2-LICENSE.txt](SDL2-LICENSE.txt) |
| [openal-soft](https://github.com/kcat/openal-soft) 1.25.2 | Audio output (PsyCross) | LGPL-2.1-or-later (see below) | release builds link it statically (fetched at build time); dev builds use the system library | [openal-soft-COPYING.txt](openal-soft-COPYING.txt), [openal-soft-BSD-3Clause.txt](openal-soft-BSD-3Clause.txt), [openal-soft-pffft-LICENSE.txt](openal-soft-pffft-LICENSE.txt) |
| [{fmt}](https://github.com/fmtlib/fmt) 11.2.0 | Bundled inside openal-soft | MIT | inside openal-soft | [fmt-LICENSE.txt](fmt-LICENSE.txt) |
| [GSL](https://github.com/microsoft/GSL) | Bundled inside openal-soft | MIT | inside openal-soft | [GSL-LICENSE.txt](GSL-LICENSE.txt) |

## openal-soft (LGPL)

openal-soft is licensed under the GNU Lesser General Public License, version 2.1 or later.
Release builds link it statically. To meet the LGPL's relinking requirement, the complete
source of this project and its build scripts are public, and the build fetches the unmodified
openal-soft 1.25.2 source from https://github.com/kcat/openal-soft (tag `1.25.2`). You can
rebuild the client against a modified openal-soft by changing the `FetchContent` source in
`port/CMakeLists.txt`, or link a system/shared openal-soft by configuring without
`-DLAIN_STATIC_DEPS=ON`.

## Build tools (not distributed)

The decompilation's build runs in a Docker image with tools that are downloaded at build time
and are not part of this repository or of any release: GCC 2.8.1 for the PlayStation
(decompals/old-gcc), maspsx, splat, spimdisasm, binutils, and optionally decomp-permuter.
They keep their own licenses.

## Game data

No part of Serial Experiments Lain (code, audio, video, images, text) is included in this
repository or in any release. The client reads the player's own disc images.
