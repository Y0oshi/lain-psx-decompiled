# Native client

A native Windows, macOS and Linux build of *Serial Experiments Lain*, built from the
decompiled game code. Nothing is emulated: the game's C runs natively, and a
reimplementation of the PlayStation SDK (`psx/`, based on PsyCross) turns its
graphics, sound, CD and pad calls into OpenGL, OpenAL and SDL.

The matching decompilation in `../src` is the reference and stays unchanged. The
client has its own copy of the game code (`game/`, forked from `../src/game`), made
portable: 64-bit clean, no hardware addresses. Ship of Harkinian follows the same
model with the Ocarina of Time decompilation.

## Player setup

1. **Download the client.** It contains no Sony or Pioneer data.
2. **First launch opens the setup window:**
   - **Discs:** select dumps of disc 1 and disc 2 (BIN/CUE or CHD). Each is checked
     against its known SHA-1; only the hashes ship with the client.
   - **Import:** each verified disc's raw data track (2352-byte sectors) is copied into
     the user data folder as `disc1.bin` / `disc2.bin`. The original dumps are no
     longer needed afterwards, and the client swaps discs itself.
   - **Display:** window size, fullscreen, internal render scale, vsync.
   - **Language:** text/subtitles (Japanese original, English, or any installed
     subtitle pack) and voices (Japanese original, or an installed dub pack).
3. **Play.** Settings stay reachable from the in-game F1 menu. Saves go to the user
   data folder.

| OS | User data folder |
|---|---|
| Windows | `%APPDATA%\LainNative\lain-native\` |
| macOS | `~/Library/Application Support/LainNative/lain-native/` |
| Linux | `~/.local/share/LainNative/lain-native/` |

## Copyright

| Content | Source |
|---|---|
| Game logic | The decompiled C in this repository |
| Program data, movies (STR), voice (XA), sounds (VAB, `VOICE.BIN`), images (TIM, `BIN.BIN`), node and keyword tables | The player's own discs, imported on their machine |
| Hashes of the retail discs | Shipped with the client (a hash is not copyrighted content) |
| Subtitle and dub packs | Separate packs, from translators and voice actors who hold the rights |

The repository and release builds never contain game data. `.gitignore` blocks disc
images, `extract/`, `asm/` and `build/`.

English fan translations of Lain belong to their authors and are not in this
repository or its releases. The launcher can fetch a published set on the player's
machine, from the translators' own repository, when the player requests it (see
[Getting subtitles](#getting-subtitles)).

## Subtitles and dubs

The original game has no subtitles. The client draws them as an overlay and can
replace voice audio with a dub. Both come as **packs**: folders in the user data
folder. The launcher lists every pack folder it finds.

Tracks are named after the disc file and, for voice files, the XA channel:
`LAIN01.XA.ch00` is channel 0 of `XA/LAIN01.XA`, and `F001.STR` is a movie. The client
identifies the playing track from the drive position and channel filter. The time
within a track is `(sector - file start) / 150` seconds (double speed).

### Subtitle packs: `lang/<code>/`

```
lang/en/pack.txt                  name = English / author = ... / version = 1
lang/en/font.ttf                  optional; needed for non-Latin scripts
lang/en/subtitles/index.txt       one .srt file name per line
lang/en/subtitles/LAIN01.XA.ch00.srt
lang/en/subtitles/F001.STR.srt
```

- Standard SRT, timed from the start of the track. Subtitle Edit, Aegisub and
  similar editors work; time the lines against the track's audio.
- `lain --make-subtitle-pack <code> [name]` writes a template pack from the player's
  imported discs: all 754 tracks (672 voice channels, 82 movies), each with one
  placeholder cue giving the track's length. Placeholder cues are never shown, so a
  partly translated pack only shows subtitles for finished tracks.
- Selected with the launcher's "Text / subtitles" option (`text_lang` in
  `settings.ini`), or `LAIN_SUBTITLE_PACK=<folder>` for testing.

**Folder packs.** A pack without `subtitles/index.txt` is a plain folder of subtitle
files in any layout. Every `.srt` or `.ass`/`.ssa` file under it is loaded, and the
file name without extension names the track. This is the layout fan subtitle projects
use, so their folders work unchanged. Besides disc tracks, a name can be:

- **A node name** (`Cou001.ass`). While the media player plays a node's voice file or
  movie, the client finds the node whose media id matches `g_media_id`
  (`g_node_table[i].media`; 716 nodes, each with its own media) and uses its name when
  the pack has no entry for the disc track.
- **The node that owns a voice in the game's media table** (`g_media_table`: XA file
  number and channel), so voices played outside the node player also find their
  subtitles:
  - the idle "network voices" (Env nodes; Site B's copies are listed under the GaTE/P2
    nodes but are byte-identical to Site A's, so they use the Env subtitles),
  - the clip after the ending (media 0x2DD, `Xa0001`),
  - the credits movie (`ENDROLL1.STR`, `Endroll`).

### Getting subtitles

The launcher's Language section offers:

- **Download English subtitles:** fetches the laingame.net fan subtitles
  (https://github.com/laingame-net/lainass, timed `.ass` files per node) as a
  `.tar.gz` with the system's `curl`, unpacks it with `tar` (both built into macOS,
  Windows 10+ and Linux) into `lang/en-laingame/`, and selects it. Sources are listed
  in `kSubSources` (`client/sub_download.cpp`).
- **Add subtitle folder...:** copies a folder of `.srt`/`.ass` files into
  `lang/<folder name>/`.

### Dub packs: `dub/<code>/`

```
dub/en/pack.txt                   name = English dub / ...
dub/en/LAIN01.XA.ch00.ogg         Ogg Vorbis, any sample rate, mono or stereo
```

- Every `.ogg` under the pack folder is indexed, named by disc track
  (`LAIN01.XA.ch00.ogg`) or by node (`Cou001.ogg`). The playing track resolves to a
  name as for subtitles: disc track, then the names from the game's media table, then
  the node being played.
- For each track with a file, the dub plays in place of the disc's voice audio, sample
  for sample and paced by the disc stream. It starts, pauses and stops with the
  original, and the game's volume and level meter still apply. Tracks without a file
  keep the original audio.
- A dub should match the original track's length; audio past the original's end is cut.
- Selected with the launcher's "Voices" option (`voice_lang`), or
  `LAIN_DUB_PACK=<folder>`.

Packs never contain game data.

## Launcher extras

### Icon

The client icon is frame 0 of the game's memory card icon (`g_mcard_header_template`,
16x16, 4-bit) with a mouth and a lighter nose, stored as palette data in
`client/lain_icon.h`. `client/app_icon.c` sets it as the window icon, enlarged by whole
pixels. `tools/gen_icon.py` writes `src/lain.png` and `src/lain.ico` from the header: the
Windows build embeds the `.ico` (`src/lain.rc`), `package_macos.sh` builds `lain.icns`
for the app bundle, and the Linux package ships `lain.png`.

### "Unused" tab

Shows the content described in [docs/findings/README.md](../docs/findings/README.md):

- the 11 Lain animations the game never plays (animated),
- the unused photos, the credits' NO DATA card and Lain's eye strip,
- sounds with Play buttons: two sound-bank samples nothing triggers, name-voice
  samples the code never reaches (the word endings; pi/pu/pe/po, dropped by a bug),
  and the rarest screensaver voice (1 in 4,096).

Nothing is stored in the program. `client/unused_gallery.cpp` reads the files from the
player's imported disc images the first time the tab opens, using the game's own file
tables in `SLPS_016.03`, and decodes them: napk LZ, TIM, Lain's MDEC animation frames
with the game's run/level table, and SPU/XA ADPCM to 16-bit PCM, played through a plain
SDL audio device while the launcher is open.

`LAIN_SETUP_TAB=unused` opens the launcher on that tab; `LAIN_SETUP_SCREENSHOT=<file>`
saves the launcher window.

## Source layout

```
port/
  CMakeLists.txt
  PORTING.md          this file
  compat/             small portability shims (e.g. <malloc.h> on macOS)
  psx/                PsyCross (vendored, MIT; see psx/UPSTREAM.txt) + additions
  game/               portable copy of the decompiled game C (from ../src/game)
  client/             launcher/setup UI, disc import, settings, subtitle/dub overlay
  src/                entry point, platform glue
  tests/              standalone tests (gs_test, snd_test, str_decode, xa_decode)
  tools/              code generators and packaging scripts
```

### Global data

The client loads the player's `SLPS_016.03` from the imported disc 1 into a PS1 RAM
arena at its load address (`client/psx_mem.c`). Every game global keeps its original
address: `tools/gen_arena.py` generates `game/generated/psx_arena.S` with one label per
global. Globals that hold pointers cannot keep the PS1 layout; they are host globals
(listed in `game/PORT_TYPES.md`) initialized from the arena's 32-bit words by
`game/port_data.c`. `lain --selftest` loads the arena and checks the labels.

## Status

| Component | Status |
|---|---|
| PsyCross built natively (macOS arm64, Windows, Linux) | done |
| `psx/lain_press`: MDEC (15/24bpp) and STR streaming (`St*`, `CdRead2`), verified on F001/F002/F020/F043 | done |
| `psx/lain_xa`: XA-ADPCM (bit-exact against ffmpeg) and a paced CD drive (CdControl/Sync/Ready callbacks, RT/SF routing, 75/150 sectors/s) feeding the SPU's CD input | done |
| `psx/lain_snd`: software SPU (24 voices, ADSR, CD input, capture + IRQ) and libsnd (VAB, SEQ sequencer at 240 Hz, voice allocation), replacing PsyCross's OpenAL-per-voice SPU | done |
| `psx/lain_gs`: libgs (ordering tables, double buffering, sprites, lines, TMD models with lighting/fog, coordinates, camera) on PsyCross libgpu/libgte | done |
| Lain's movie/picture VLC decoder (`vlc_decode_frame`, `vlc_build_table`, in the EXE's library region) in C (`game/port_vlc.c`; reference: `tests/str_decode/lain_vlc.c`). Its code table `g_movie_vlc_table` comes from the player's EXE. | done |
| `client/`: disc verification and import | done |
| `client/`: setup window (resolution, fullscreen, language, voice) | done |
| `game/`: 64-bit clean C, `NON_MATCHING` code paths | done |
| Global data from the player's EXE (see [Global data](#global-data)) | done |
| Boot through the warning/logo screens, title menu, name entry and into Site A (3D node map, Lain animation, XA audio), node menus | done |
| Media: sound-file player (XA, slideshow, level meter), in-game STR movies (15-bit), the 24-bit ending movie, disc 1/2 swaps through the game's "DISC Change Request" screen | done |
| Subtitle overlay and dub playback | done |
| Windows (MinGW), macOS and Linux CI ([port.yml](../.github/workflows/port.yml)) | done |
| Release packages ([release.yml](../.github/workflows/release.yml)) | written, not yet run |

Open items are tracked in [docs/ROADMAP.md](../docs/ROADMAP.md).

### Changes to PsyCross

Marked `lain:` in the source.

**Headers and build**

- `libgpu.h`: 64-bit primitive tags on arm64 (upstream only checked x86-64).
- `libgpu.h`: `LoadTPage` takes `u_int*` (`u_long` is 64-bit on LP64).
- `libgpu.h`/`LIBGPU.C`: `LoadImage`/`LoadImage2`/`StoreImage`/`StoreImage2`/
  `LoadClut`/`LoadClut2`/`OpenTIM` take `u_int*` (LP64). `TIM_IMAGE` fields are
  `crect`/`prect` (PsyQ names).
- `libapi.h`: forward-declares `struct EXEC` before the prototypes that use it.
- `libpress.h`, `libcd.h`: declarations for the MDEC and streaming additions (`lain_press/`).
- `LIBGTE.C`: `fst_min`/`fst_max` are `static inline` (plain C99 `inline` emitted no
  definition).
- Built with `USE_PGXP=0` (`psx/CMakeLists.txt`): the game fills primitives itself
  with plain short coordinates.

**Sound, CD and memory card**

- `src/psx/LIBSPU.C` is compiled out (`#if 0`). libspu comes from the software SPU in
  `src/lain_snd/` (24 ADPCM voices, ADSR, reverb, CD input, capture/IRQ, one SDL audio
  stream), which also provides libsnd (`include/psx/libsnd.h`). `libspu.h` gains a
  `lain:` block: `EncSPU`, `LainSpu_SetCdSource`/`LainSpu_PushCd` (CD/XA input),
  `LainSpu_Render` (offline), `LainSpu_SetOutputGain`, `LainSpu_ReadRam`. Lain's
  `SpuRead` uploads (in the binary it is libspu's write routine). Test:
  `port/tests/snd_test`.
- `LIBCD.C`: CdControl/CdSync/CdGetSector/callbacks forward to the paced drive in
  `lain_xa/cd_drive.c`.
- Platform glue maps `EnterCriticalSection`/`ExitCriticalSection` to
  `LainCD_EnterIRQ`/`LainCD_ExitIRQ` (`port/src/platform.c`).
- Disc swaps: `PsyX_CDFS_SwapImage` (`LIBCD.C`) and `LainCD_OpenShell` (`lain_xa`,
  shell-open status for the game's lid watchdog). The client swaps when the disc the
  game wants (`g_current_site`) is not the inserted one.
- `src/lain_mcrd/`: libmcrd on card image files (`LainMcrd_SetCardPath`); `LIBMCRD.C`
  is compiled out.

**Graphics**

- `LIBGPU.C`: `ClearImage` clears the window only when the rectangle touches the
  display area, not for every off-screen VRAM clear.
- `PsyX_GPU.cpp`: vertex coordinates wrap to 11 bits as on the GPU; polygons wider
  than 1023 or taller than 511 pixels are dropped as on the GPU; raw-texture sprites
  (0x65/0x67) are handled.
- `PsyX_render.cpp`: `GR_DrawVRAMBackground` starts each frame from the display area
  of VRAM (15-bit, or 24-bit when `isrgb24`), so pictures and movie frames uploaded
  with LoadImage show. `ClearVRAM` colour packing is corrected. `PsyX_BeginScene`
  calls it; `lain_gs` `GsSwapDispBuff` presents frames that drew nothing (movie frames).
- 24-bit display: while `isrgb24`, `PsyX_EndScene` does not store the window back into
  VRAM and `GR_ReadFramebufferDataToVRAM` does not read it back, so the uploaded
  24-bit picture is preserved.
- `lain_gs`: `GsSetMapBase` (TMDs in the emulated PS1 RAM get PS1 addresses when mapped).

**Timing, window and input**

- `PsyX_main.cpp`: the VSync callback runs under the CD "IRQ" lock;
  `g_lain_onEndScene` client hook and `g_lain_scenesPresented` counter.
- Timing (`PsyX_main.cpp`, `LIBETC.C`): the vblank thread runs on an absolute
  59.94 Hz schedule (50 Hz PAL if selected) and sleeps between ticks. `VSync()`
  implements `-1`/`1`/`0`/`n` as PsyQ does.
- Window (`PsyX_main.cpp`, `PsyX_render.cpp`): desktop fullscreen,
  `PsyX_ToggleFullscreen` (Alt+Enter, Cmd+Ctrl+F), a 4:3 view rectangle
  (`GR_GetViewRect`) for viewport, scissor, frame store and the VRAM background.
  `GR_Clear` clears only the covered part of the window.
- `PsyX_pad.cpp`: gamepad hotplug by joystick instance id (first free port).

## Playing

### Controls

Keyboard defaults. Each binding can be changed in the F1 menu under "Keyboard
controls", or in `settings.ini` as `key_<button> = <SDL key name>` (buttons: `up down
left right cross circle square triangle l1 l2 l3 r1 r2 r3 start select`).

| PS1 button | Key | In Lain |
|---|---|---|
| D-pad | arrow keys | move the cursor, select a node |
| ○ circle | V | confirm, open a node |
| × cross | C | cancel, back |
| △ triangle | Z | open the menu (Load, Save, Change, ...) |
| □ square | X | |
| Start | Return | continue, skip |
| Select | Space | |
| L1 / L2 / L3 | Left Shift / Left Ctrl / [ | |
| R1 / R2 / R3 | Right Shift / Right Ctrl / ] | |

Any gamepad in SDL's GameController database (Xbox, PlayStation, Switch Pro, ...)
works as pad 1, whether connected before or during play. Face buttons map by
position (bottom = ×, right = ○, left = □, top = △); shoulders and triggers map to
L1/R1/L2/R2; sticks work in analog mode (hold Select + Start to switch). The keyboard
stays active alongside it.

### Window

`settings.ini` (written by the launcher):

| Key | Meaning |
|---|---|
| `fullscreen` | desktop fullscreen |
| `width`, `height` | window size |
| `render_scale` | the game draws into an offscreen target of 320x240 times this value, then scales it into the window. 1 = original pixels, 0 = auto (the window's full HiDPI resolution) |
| `vsync` | display swap interval |
| `smooth` | 60 fps in-between frames (see [Smooth motion](#smooth-motion)) |
| `volume` | 0-100, scales the final SPU mix (XA voices, movie audio and dubs all pass through it) |
| `text_lang`, `voice_lang` | subtitle and dub pack |

The picture keeps its 4:3 shape, with black bars in other window shapes; whole-number
magnifications stay sharp. Overlays (subtitles, messages) are drawn after scaling, at
the window's resolution. Alt+Enter, and Cmd+Ctrl+F on macOS, toggle fullscreen.

### In-game menu

F1 (or a gamepad's Guide/Home button) opens a menu over the running game with three
tabs: Settings, Controls and Cheats. It covers fullscreen, vsync, render scale, master
volume, subtitle and voice language, keyboard bindings (click a button, press a key),
cheats and quit. Changes apply at once and are saved to `settings.ini` when the menu
closes (Esc, F1 or Resume). The game keeps running underneath but receives no pad input
while the menu is open. The menu responds to mouse, keyboard (arrows, Space/Return) and
gamepad. `LAIN_MENU_TAB=cheats` opens the menu on the Cheats tab.

### Cheats

Saved in `settings.ini`.

| Cheat | Effect |
|---|---|
| Open any node (`cheat_open_nodes`) | Skips the progress check in `node_pick_select_anim`, like `LAIN_UNLOCK_ALL=1`. Saves are unchanged. |
| Turbo (`turbo`) | Runs the emulated vblank clock N times faster while Tab is held (`g_lain_speed` in PsyCross), with display vsync off meanwhile. Movies and voices keep normal speed because the CD drive paces them; 60 fps screens are limited by how fast the host draws. |
| Genome save title (`cheat_genome`) | Every save takes the rare "連続ゲノム小説" title. The port's `save_build_image` still calls `rand()` first, so the game's random sequence is unchanged. |

The tab also shows Lain's four hidden stats live (see
[docs/findings/README.md](../docs/findings/README.md)).

### Node map pacing

On the PS1 the node map (Site A/B) is CPU-bound. Every frame, Lain's animation is
VLC-decoded in software, MDEC-decoded and uploaded, and the 3D map is drawn, so a frame
takes 4-8 vblanks depending on the size of her current animation frame (about 12 fps;
large emote frames run slower, the standing pose slightly faster).

| Vblanks per frame | PS1 (Mednafen, 3.5 min idle) | Native client |
|---|---|---|
| 4 | 3.5% | 5.3% |
| 5 | 86.9% | 82.0% |
| 6 | 7.2% | 9.2% |
| 7-9 | 2.5% | 3.5% (7-8) |
| mean | 5.10 | 5.13 |

The client does that work almost instantly, so the map's frame end (`gfx_frame_end`)
waits `VSync(n)` with n = ceil(-0.336 + size / 2602), clamped to 4-8, where size is the
compressed size of the frame just decoded (`g_anim_last_frame_size`).
`LAIN_SITE_VBLANKS=N` fixes n (0 = unpaced). The title menus run at 60 fps on the PS1
and are unchanged. `LAIN_ANIM_LOG=1` logs Lain's idle-animation loads and each decoded
frame's size.

### Smooth motion

Optional 60 fps presentation, on by default (F1 > Settings, `smooth` in `settings.ini`,
or `LAIN_SMOOTH=0/1`). Screens the game draws at 12 or 30 fps keep their original
speed; the game logic is untouched.

While the game waits several vblanks in `VSync(n)`, the renderer shows an in-between
frame at every vblank:

1. Each scene's draw batches are recorded: vertices, splits, a GPU copy of VRAM as
   that batch saw it, and the scene's starting image.
2. The current scene is redrawn into a separate framebuffer, with every primitive
   moved part of the way from its position in the previous scene.
3. Primitives are matched through tags the port's libgs gives each packet
   (`LainGs_PacketTag`: a hash of the TMD object and polygon, or of the sprite/line
   struct, plus how often it was sorted this frame); untagged primitives match by
   address. Primitives that appeared, vanished or moved more than 96 pixels are drawn
   where they are.

The real frame is still presented unchanged at the end of the wait, and the screen
framebuffer the game reads back into VRAM is never touched. Overlays (subtitles, F1
menu) are drawn on in-between frames too. `LAIN_SCREENSHOT_INTERP=1` also saves the
in-between frames before each screenshot frame. Code: `LainInterp_*` in
`psx/src/gpu/PsyX_GPU.cpp`, `GR_Interp*` in `psx/src/render/PsyX_render.cpp`, and the
wait loop in `VSync` (`LIBETC.C`).

### Timing

The game runs on an emulated NTSC vertical blank (59.94 Hz, on its own thread), so game
speed does not depend on the display's refresh rate or on vsync. `VSync()` has PsyQ's
semantics (`VSync(n)` waits n vblanks since the previous wait), which paces Lain's menus
as on the console (e.g. the media player menus at 12 fps, the node map at 30 fps).

### Saves

Memory card 1 is `memcard1.mcd` in the user data folder: a raw 128 KiB card image (the
common `.mcd`/`.mcr` format, usable with emulators and card managers). The first save
offers to format it.

### Discs

When the game asks for the other disc, the client swaps the imported image and shows
"Disc N inserted".

## Running and debugging

`lain --play` boots straight into the game with the saved settings and imported discs,
skipping the launcher. A crash prints the faulting pc and a backtrace (symbolize with
`atos -o lain -l <image base> <pc>`). Configure with `-DLAIN_TRACE=ON` to log each game
function the first time it runs.

Debug environment variables (`src/main.c` unless noted):

| Variable | Effect |
|---|---|
| `LAIN_SCREENSHOT=path`, `LAIN_SCREENSHOT_FRAMES=N,M` | save `path-N.bmp` at the listed frames |
| `LAIN_VRAM_DUMP=1` | also save `path-N-vram.tga` |
| `LAIN_SCREENSHOT_INTERP=1` | also save the in-between frames before each screenshot frame |
| `LAIN_OT_DUMP=1` | print the 3D ordering tables |
| `LAIN_EXIT_FRAME=N` | exit at frame N |
| `LAIN_FRAME_LOG=1` | log frames; also prints frames/s and vblanks/s |
| `LAIN_KEYS=F:Key[:N],...` | scripted key presses (arrows, C = cross, V = circle, X = square, Z = triangle, Return = start, Space = select) |
| `LAIN_TRACE_HOT=N` | with `LAIN_TRACE`: call counts every N frames |
| `LAIN_UNLOCK_ALL=1` | every node opens regardless of progress |
| `LAIN_MEMCARD=path` | use another card image |
| `LAIN_RENDER_SCALE=N` | override the render scale |
| `LAIN_DATA_DIR=<folder>` | use another data folder (settings, discs, memory card, packs) |
| `LAIN_MENU_DEBUG=1` | log which F1-menu control has focus |
| `LAIN_DISC=2` | start with disc 2 in the drive |
| `LAIN_DISC_SWAP_DELAY=N` | wait N frames before the client swaps discs, so the game's change-request screen shows |
| `LAIN_SITE_VBLANKS=N` | fixed node-map frame length (0 = unpaced) |
| `LAIN_ANIM_LOG=1` | log Lain's animation loads and frame sizes |
| `LAIN_LOG_READS` | log every disc read |
| `LAIN_SUBTITLE_PACK`, `LAIN_DUB_PACK` | load a pack folder directly |
| `LAIN_SETUP_TAB`, `LAIN_SETUP_SCREENSHOT`, `LAIN_MENU_TAB` | open the launcher or F1 menu on a tab; save the launcher window |

Scripted keys also arrive as SDL key events, so they drive the F1 menu. Hold them for
one frame there (`F:Down:1`): at the game's 12 fps screens, a longer hold passes
ImGui's key-repeat delay.

### Repro scripts

Run from the repository root after `cmake --build build/port`. Frame numbers count
presented frames, so the timings assume a normal-speed build (a `LAIN_TRACE` build is
slower).

**Boot sequence** (warning, logo, title, name entry, Site A, node menu). V at 700 picks
"Authorize User", V at 760 enters one character, Return at 820 finishes the name. Site A
settles by about frame 900 with Cou001 selected; V at 1000 opens its menu.

```
LAIN_KEYS="700:V,760:V,820:Return,1000:V" LAIN_EXIT_FRAME=1300 LAIN_SCREENSHOT=build/port/shots/final LAIN_SCREENSHOT_FRAMES=100,240,600,800,980,1250 build/port/lain --play
```

**Cou001 sound-file player** (the node selected when Site A appears; V at 1250 chooses Play):

```
LAIN_KEYS="700:V,760:V,820:Return,1000:V,1250:V" LAIN_EXIT_FRAME=2000 LAIN_SCREENSHOT=build/port/shots/cou LAIN_SCREENSHOT_FRAMES=1300,1500,1800 build/port/lain --play
```

**Save and load.** A new game: Site A, △ menu, Down Down to "Save", ○, Left to "Yes",
○, Start to format and save ("SAVE successful"):

```
LAIN_MEMCARD=build/port/test.mcd LAIN_KEYS="700:V,760:V,820:Return,1000:Z,1200:Down,1300:Down,1400:V,1500:Left,1600:V,1700:Return" LAIN_EXIT_FRAME=1900 build/port/lain --play
```

Then load it (title: Down to "Load Data", ○, Left to "Yes", ○; it goes straight to Site A):

```
LAIN_MEMCARD=build/port/test.mcd LAIN_KEYS="700:Down,780:V,900:Left,960:V" LAIN_EXIT_FRAME=1300 build/port/lain --play
```

**Dc1015 movie "weather break"** (Down at 1000 moves to it; it is locked in a new game,
hence `LAIN_UNLOCK_ALL`):

```
LAIN_UNLOCK_ALL=1 LAIN_KEYS="700:V,760:V,820:Return,1000:Down,1150:V,1400:V" LAIN_EXIT_FRAME=1700 LAIN_SCREENSHOT=build/port/shots/movie LAIN_SCREENSHOT_FRAMES=1440,1500,1560,1620 build/port/lain --play
```
