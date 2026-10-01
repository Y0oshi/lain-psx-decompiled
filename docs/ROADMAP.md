# Roadmap

## Completed

| Area | Milestone |
|---|---|
| Intake | Disc tool (BIN/CUE/CHD reading, hashing, ISO9660 + XA-aware extraction); SHA-1 of both discs and the shared executable |
| Split | splat config for the executable (1,181 functions); round-trip build to a byte-identical executable |
| Toolchain | GCC 2.8.1-psx + maspsx in Docker; `tools/progress.py` progress tracking |
| PsyQ | Libraries identified by signature (PsyQ 4.3 + libpad 4.5) and excluded from decomp work |
| Decompilation | All 321 game functions match byte for byte; `.rodata` and emitted `.sdata` split per translation unit |
| Naming | All game functions named except 6 empty stubs; all but 58 globals named |
| Native client | Game C built natively (64-bit clean); PsyQ replaced (GPU, GTE, SPU/XA, MDEC, CD, pad, memory card); disc import with SHA-1 check; setup window and F1 menu |
| Media | STR movies, XA voices, the 24-bit ending movie, disc 1/2 swaps through the game's own change-request screen |
| Presentation | Render scale, fullscreen, node map paced like the PS1, optional 60 fps in-between frames |
| Packs | Subtitle packs (SRT/ASS, node-name lookup, one-click download of the laingame.net fan set), dub packs |
| Extras | Cheats tab (open any node, turbo, Genome save title, live hidden stats); launcher "Unused" tab |
| Mods | Mods folder managed in the launcher's Mods tab: archive and program files, movies and voice files, game data (JSON), HD texture packs, Lua scripts hooking any of the 276 game functions, native plugins; export of every original ([MODDING.md](../port/MODDING.md)) |
| Platforms | Windows, macOS and Linux builds in CI ([port.yml](../.github/workflows/port.yml)) |

## Open work

### Decompilation

- **`.data` / `.bss` as C.** Globals are still asm data.
- **Remaining names.** 6 empty stub functions and 58 globals: 30 point inside other
  named objects (and carry a comment), about 26 are only ever written.
- **Known symbol issues.** The library symbol `PCread` at `0x8005C254` is `PCwrite`.
  `g_scene_wait_frames` is declared `u16` in two files and `s32` in two others.
- **Asset formats.** Document the node/level tables, TIM/image banks, STR video, XA
  audio, fonts and save format.
- **Disc hashes.** Verify against redump.org.

### Native client

- **Full playthrough test** in one run: Site A, disc swap, Site B, ending.
- **Gamepads.** SDL GameController support (Xbox, PlayStation, Switch Pro, hotplug,
  Guide button for the F1 menu) is untested with real controllers, including on Windows.
  The Windows smoke test under Wine is `port/tools/test_windows_wine.sh`.
- **Linux on real hardware.** Tested only in a container (Xvfb + Mesa).
- **Frame pacing of other screens.** Title, menus (60 fps) and the node map (12 fps)
  are measured against the PS1 (Mednafen) and match. Not yet measured: opening a node,
  menus inside the site, Site B, the ending.
- **Smooth motion gaps.** In-between frames need the game to wait in one `VSync(n)`
  call. Screens that pace themselves with two `VSync(0)` calls per frame (30 fps) are
  not smoothed.
- **UI text translation** (menus, node keywords) in language packs. Low priority:
  most UI text in the original is English.
- **More cheats** from the hidden-content research (debug modes, progress or site
  unlocks that do not touch saves).
- **App icon file.** The running windows use the game's save icon read from the
  player's disc; the app bundle and `.exe` still have the default icon.
- **Widescreen** rendering option.
- **Release workflow.** [release.yml](../.github/workflows/release.yml) builds and
  attaches the three packages when a `v*` tag is pushed; it has not run yet.

## Not planned

- Code signing and notarization (macOS) and Authenticode (Windows): releases are unsigned.
