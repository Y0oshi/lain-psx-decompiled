# Modding

Mods change the game: its pictures, text, voices, movies, data, and code. The client
applies them when the game starts; the imported discs are never changed. A mod holds only
what it changes, so it contains no game data beyond that.

A mod can:

- replace any file in the disc archives (pictures, text, voice clips, Lain's animations,
  sounds),
- replace the pictures and 3D models built into the game program,
- replace movies and voice sessions, longer or shorter than the originals,
- add HD versions of any picture the game draws (texture packs),
- change the game's data: nodes (names, texts, links, map position, unlock rules, media),
  strings, media, Lain's animation choices, or any variable,
- run a Lua script that hooks any of the game's 276 functions, reads and writes game
  memory, and draws its own windows and overlays,
- load a native plugin (C/C++) with the same access.

## Using mods

1. Open the launcher's **Mods** tab and press **Open mods folder**.
2. Put each mod's folder in it (`<data folder>/mods/<mod name>/`).
3. Back in the Mods tab, press **Refresh**. Tick the mods to use and press **Play**.

The tab checks every file of every mod against your discs and shows what is used and
what isn't, with the reason. A file with a problem is skipped; the rest of the mod still
applies. When two mods change the same thing, the one lower in the list wins (**Move up**
and **Move down** set the order).

In the game, the F1 menu's **Mods** tab lists the mods in use, each script's settings and
each mod's messages.

## Making a mod

1. In the Mods tab, press **New mod...** and give it a name. This creates the folder with
   a `mod.ini`.
2. Press **Export originals**. It writes every archive entry, and the pictures and models
   built into the program, to `mods/_originals/` as the files a mod uses (about 50 MB).
3. Copy the files you want to change into your mod, keeping the folder and the name, and
   edit them. Keep only the files you changed.
4. Press **Refresh** to check them, then **Play**.

### Examples

`port/mod_examples/` holds working mods to copy from:

| Folder | Shows |
|---|---|
| `example-data` | Game data: renames a node, moves another, changes an info line |
| `example-script` | A Lua script: a hook, an overlay and a setting in the F1 menu |
| `example-plugin` | A native plugin, with its source and build commands |

### A first script

A mod folder with a `mod.ini` and this `main.lua` shows a message each time a node
opens:

```lua
lain.hook("media_play", function(next, movie, skipped, params)
  lain.toast("Opening media " .. movie)
  return next(movie, skipped, params)
end)
```

`lain.functions()` lists every function a script can hook, with its C signature; the
game's source (`port/game/`) shows what each one does. [Scripts](#scripts) has the full
library.

### What goes where

```
mods/
  My Mod/
    mod.ini               name, author, version, description
    SITEA.BIN/0631.png    a file in a disc archive (entry 631)
    PROGRAM/g_orb_tim.png a picture or model built into the program
    MOVIE/F001.STR        a movie or voice file (MOVIE/, MOVIE2/, XA/)
    data/nodes.json       game data changes
    textures/1f4b39bc115a9785.png  an HD picture (texture pack)
    main.lua              a script (and other .lua files it requires)
    plugins/my.dll        native plugins (.dll, .dylib, .so)
```

### mod.ini

```ini
name = Blue Font
author = Your name
version = 1.0
description = A blue version of the game's font.
description = Each description line adds a line.
```

Every key is optional. Without a name, the folder name is shown.

Files the mod system doesn't use are listed as ignored in the Mods tab; `README`,
`LICENSE`, `preview` and `.md` files, and a `src/` folder (a plugin's source), are
skipped silently.

## Files in the disc archives

The folder is the archive on the disc; the file name starts with the entry number
(decimal, leading zeros optional). Anything after the number is ignored, so
`0631 sports field.png` works too.

| Archive | Disc | Entries | Holds |
|---|---|---|---|
| `BIN.BIN` | 1 and 2 | 0-43 | Fonts, interface pictures, the voice name list |
| `SITEA.BIN` | 1 | 0-789 | Site A pictures |
| `SITEB.BIN` | 2 | 0-557 | Site B pictures |
| `LAPKS.BIN` | 1 and 2 | 0-58 | Lain's animations |
| `VOICE.BIN` | 1 and 2 | 0-638 | Voice clips (Lain saying the player's name) |
| `SND.BIN` | 1 and 2 | 0-3 | Sound bank and music sequences |

| Type | For | Rules |
|---|---|---|
| `.png` | Pictures | Same width and height as the original. Any PNG works (color type, bit depth, interlacing). |
| `.tim` | Pictures | Same depth, size and palette size as the original TIM. |
| `.wav` | `VOICE.BIN` clips | Any rate, channel count and PCM depth, or 32-bit float. Converted to 16-bit mono at 7914 Hz. |
| `.txt` | Text entries | Plain ASCII. Line endings are converted to the original's. |
| `.bin` | Anything | The entry's bytes, unpacked. An entry that is already napk-packed is used as is. |

### Pictures

- **Size and position:** the game places every picture at a fixed spot in video memory,
  so a replacement keeps the original's width and height.
- **Transparency:** transparent pixels (alpha below 128) become the PlayStation's
  transparent color. Opaque pure black stays visible.
- **Colors** are stored with 5 bits per channel, like on the PlayStation.
- **Palettes.** 4-bit and 8-bit pictures use a palette of 16 or 256 colors:
  - If every color of the PNG is already in the original palette, the palette is kept.
  - Otherwise a picture with one palette gets a new palette made from the PNG (reduced
    to 16 or 256 colors when it has more).
  - Some pictures, such as the fonts, have several palettes that share the same pixels.
    Their colors are matched to the nearest color of the first palette, and the Mods tab
    notes it.
- **Semi-transparency** isn't visible in a PNG: it is taken from the original picture.

An unchanged exported PNG converts back to exactly the original bytes.

### Size limit

A replacement can't be bigger than the original entry, packed or unpacked: the game reads
some entries into fixed buffers. Most pictures are packed (napk), so a picture with much
more detail than the original may not fit; the Mods tab says by how much. Fewer colors
and larger flat areas pack smaller.

## Pictures and models in the program

Pictures and 3D models built into the game program go in `PROGRAM/`, named after the game
variable that holds them: `PROGRAM/g_node_icon0_tim.png` (the node icons),
`PROGRAM/g_level_ring_tmd.tmd` (a level ring). Export originals writes all of them.

| Type | Rules |
|---|---|
| `.png`, `.tim` | As for archive pictures: same size, colors converted. |
| `.tmd` | A TMD model no bigger than the original. |
| `.bin` | Raw bytes for any variable of known size, no bigger than it. |

## Movies and voice files

A mod replaces a whole movie or voice file with one named as on the disc:
`MOVIE/F001.STR`, `MOVIE2/...`, `XA/LAIN01.XA`. The file holds Mode 2 CD sectors, 2352
bytes each (with sync and header) or 2336 (from the subheader on), as made by psxavenc or
ripped from a disc. It can be longer or shorter than the original: the client places it
past the end of the disc and points the game's file table at it.

- **Movies:** the frame count of the media that play the movie on that disc's site is set
  from the file.
- **Voice files:** an XA file holds up to 32 channels, each a voice session or a piece of
  music. If the channels' lengths change, set them in `data/media.json` (`size` is the
  channel's length in bytes).
- Subtitles and dubs keep working for replaced files.

## Texture packs

A texture pack replaces the pictures the game draws with larger ones, for any render
scale. Colors come from the HD picture; transparency comes from its alpha, and
semi-transparency still follows the original.

1. In the Mods tab, tick **Save the game's textures while playing**, then play through
   the scenes to change. Every picture the game draws is saved once to
   `<data folder>/texture_dump/`.
2. Upscale or redraw the ones to change. Keep the name and the aspect ratio: any size
   works (2x, 4x, ...), and it is scaled to the original's area.
3. Put them in a mod's `textures/` folder (subfolders are fine) and turn the dump off.

Names are hashes of the picture as uploaded: `<picture>-<palette>.png` for the picture
with one palette, or `<picture>.png` (the first 16 characters) for every palette it is
drawn with. Pictures that change while they are shown, such as the node name labels and
palette fades, get a new name for each state; the palette-free name covers fades.

## Game data

Game data changes are JSON files in the mod's `data/` folder, named for what they change.
Comments (`//`) and trailing commas are allowed. Each problem is reported with its line
number; the rest of the file still applies.

### data/nodes.json

The game has 716 nodes (453 on site A, 263 on site B). A change names the node by its name
(`"Lda112"`) or number (0 to 715) and lists the fields to set:

```json
{ "nodes": [
  { "node": "Cou001", "name": "Mod001", "lines": ["COUNSELING REC.", 0, 0, 0] },
  { "node": "Dia010", "site": "A", "level": 3, "row": 1, "column": 5, "visible": true },
  { "node": "Tda005", "unlock_level": 2, "depth": 1, "parent": "Tda004" }
] }
```

| Field | Meaning |
|---|---|
| `name` | The node's name, 1 to 7 characters. |
| `lines` | The 4 info lines: labels, by number (0 to 50) or text. |
| `keywords` | The 3 keywords shown in the player, which also link nodes that share them: texts, by number (0 to 416), text, or -1 for none. |
| `slideshow` | 3 pictures shown with a voice file: entries of SITEA.BIN/SITEB.BIN, or -1. |
| `media` | What it plays: a media number (0 to 735) or another node's name. |
| `site`, `level`, `row`, `column` | Its place on the map: site `"A"` or `"B"`, level (0 to 21 on A, 0 to 12 on B), row in the level (0 to 2), column (0 to 7). |
| `hidden_from_map` | `true`: not placed on the map at all. |
| `kind` | `lda`, `tda`, `cou`, `dc` (movie), `dia`, `eda`, `sskn` (upgrade), `gate`, `polytan`, or 0 to 11. |
| `visible` | Shown from the start of a new game. |
| `unlock_level` | The SSkn level needed to open it (0 to 5 in the game). |
| `depth` | How many media must have ended before it shows (0 to 15). |
| `parent` | The node whose opening reveals it (name, number or `null`). |
| `ending` | `true`: playing it ends the site, like the four ending nodes. |

Saves store part of every node (unlock level, kind, visibility, depth, parent). When a save
loads, the fields a mod set are put back, and the save keeps what the player has seen and
unlocked.

### data/strings.json

Changes existing texts. `labels` are the 51 info lines (at most 15 characters show),
`text` the 417 keywords (about 17 characters show). Name one by its number or its
current text; spaces and underscores match each other.

```json
{
  "labels": { "COUNSELING REC.": "MODDED RECORD", "37": "diary" },
  "text": { "counselor": "doctor" }
}
```

### data/media.json

What each of the 736 media entries plays.

```json
{ "media": [
  { "media": 12, "same_as": 30 },
  { "media": 0, "frames": 300 },
  { "media": 100, "xa_file": 3, "channel": 7, "size": 1500000 }
] }
```

| Field | Meaning |
|---|---|
| `same_as` | Copy another entry first. |
| `xa_file` | 0 for a movie, or which XA file (1 to 21). |
| `file` | A movie: its index in the disc's file table. |
| `channel` | A voice file: its channel (0 to 31). |
| `frames`, `size` | A movie's frame count, or a voice channel's length in bytes. |

### data/lain.json

Which of Lain's animations (entries of LAPKS.BIN, 0 to 58) the game picks.

| Table | Values | When |
|---|---|---|
| `idle` | 38 | Idle animations, picked at random |
| `move`, `move_sfx` | 2 × 3, 3 | Moving on the map |
| `select`, `select_sfx` | 5 × 4, 4 | Opening a node (sets 0-1) or refusing (2-4) |
| `audio_node`, `audio_node_sfx` | 6, 6 | Before an audio node plays |
| `level` | 2 | Changing level |
| `save`, `site_enter` | 1 each | Saving, entering a site |

```json
{ "idle": [8, 16, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47, 48, 49,
           50, 8, 16, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47, 48] }
```

### data/memory.json

Writes to any game variable by name, or any address, for everything the other files don't
cover. The names are those in `port/game/generated/symbols.txt`, the list `lain.sym`
uses.

```json
{ "patches": [
  { "symbol": "g_node_kind_icons", "offset": 2, "type": "u16", "values": [5, 6] },
  { "address": "0x800A6850", "bytes": "01 00" },
  { "symbol": "g_idle_voice_base", "type": "u16", "values": [169, 699] }
] }
```

`type` is u8, s8, u16, s16, u32 or s32; `values` one number or a list. `bytes` is hex;
`string` is written with a terminating zero.

## Scripts

A mod's `main.lua` runs once before the game starts (Lua 5.4, with the standard
libraries). It sets up hooks and events with the `lain` library; `require` finds other
`.lua` files in the mod folder. Each mod has its own Lua state. An error turns off only
the hook or event it happened in, and is shown in the F1 menu's Mods tab.

### Hooks

```lua
lain.hook("site_move_cursor", function(next, dx, dy)
  lain.log("moving", dx, dy)
  return next(dx, dy)      -- the game's own behavior (and later hooks)
end)
```

`lain.hook(name, fn)` replaces one of the game's functions. `fn` gets `next` and the
arguments; `next(...)` runs the replaced behavior with the given arguments (or the
original ones when called without). Not calling `next` skips the original. The return
value is the function's result; returning nothing gives `next`'s result, or 0 when `next`
wasn't called. `lain.functions()`
lists every function with its C signature. Pointer arguments arrive as PS1 addresses.

Hooks of several mods stack in load order; script hooks run before plugin hooks. The few
functions the CD drive calls from its own thread run script hooks only when called from
the game's thread.

`lain.call(name, ...)` calls a game function (through every hook).

### Events

| Event | Runs |
|---|---|
| `lain.on("frame", fn)` | Once per game frame |
| `lain.on("draw", fn)` | Every frame, inside the overlay: windows and drawing (`lain.ui`) |
| `lain.on("menu", fn)` | In the F1 menu's Mods tab, under the mod's name: settings |
| `lain.on("quit", fn)` | When the game closes |

### Memory

Game variables keep their PS1 addresses (0x80000000 and up).

| Function | Does |
|---|---|
| `lain.sym(name)` | A variable's address, size and kind (`"ps1"`, or `"host"` for the few pointer tables kept outside PS1 memory); for a game function, its native address, 0 and `"function"` |
| `lain.read8/16/32(addr)`, `read8s/16s/32s` | Read unsigned or signed |
| `lain.write8/16/32(addr, v)` | Write |
| `lain.read_bytes(addr, n)`, `write_bytes(addr, s)` | Bytes as a Lua string |
| `lain.read_string(addr[, max])` | A zero-terminated string |
| `lain.address(v)` | The PS1 address of a native pointer, when it has one |

### Drawing (`lain.ui`, in draw and menu events)

| Function | Does |
|---|---|
| `window(title, fn)` | A window; `fn` draws its contents |
| `text(s)`, `text_colored(r, g, b, s)` | Text (colors 0 to 1) |
| `button(label)` | True when pressed |
| `checkbox(label, v)` | Returns the new value |
| `slider(label, v, min, max)` | Integer or number, returns the new value |
| `input(label, s)` | A text field, returns the text |
| `same_line()`, `separator()`, `progress(f[, text])` | Layout |
| `draw_text(x, y, s[, r, g, b, size])` | Text anywhere on the screen: x and y from 0 to 1, size in PS1 pixels |
| `draw_rect(x0, y0, x1, y1[, r, g, b, a])` | A filled rectangle |

### Other functions

| Function | Does |
|---|---|
| `lain.log(...)`, `print(...)` | A message in the F1 menu's Mods tab and the console |
| `lain.toast(s[, seconds])` | A short message over the game |
| `lain.frame()` | Frames presented so far |
| `lain.time()` | Seconds since the game started |
| `lain.read_file(path)`, `write_file(path, s)` | Files, relative to the mod folder |
| `lain.save(key, s)`, `lain.load(key)` | Data kept between sessions, per mod |
| `lain.plugin(name, ...)` | A command a plugin added |
| `lain.dir`, `lain.version` | The mod folder; the API version |

## Plugins

A plugin is a shared library in the mod's `plugins/` folder: `.dll` on Windows, `.dylib` on
macOS, `.so` on Linux; the others are ignored, so one mod can ship all three. It exports

```c
int lain_mod_init(const LainModAPI *api);   /* 0 on success */
void lain_mod_quit(void);                   /* optional */
```

`port/client/lain_mod_api.h` defines `LainModAPI`; copy it into the plugin. It has what
scripts have, in C:

- **Memory:** `ps1_ptr`, `ps1_addr`, `symbol`.
- **Functions:** `function`, `function_count`, `function_at`, and `hook`, which takes a
  replacement with the function's own C signature and gives the previous behavior to
  call. A replacement runs on the caller's thread (the few CD callbacks run on the CD
  drive thread).
- **Events:** `on_frame`, and `on_draw` with `imgui_context` and `imgui_allocators`. A
  plugin that draws uses Dear ImGui 1.92.9 in that context.
- **Scripts:** `add_command` makes a function that scripts call with `lain.plugin`.
- **Other:** `log`, `toast`, `frame`, `mod_dir`, `data_dir`.

`port/mod_examples/example-plugin` builds with one compiler command per system. Plugins run
native code with the game's full access.

## Load order

Mods apply in the Mods tab's order, top to bottom: archive and program files, then game
data, then scripts and plugins. When two mods change the same file, entry or field, the
lower one wins; hooks stack, the lower mod's running first.

## Command line

| Command | Does |
|---|---|
| `lain --check-mods` | Lists the installed mods and the state of each file |
| `lain --export-originals [folder]` | Writes the originals (default `mods/_originals`) |

## Troubleshooting

- **A file isn't used.** The Mods tab and `lain --check-mods` give the reason for each
  file: wrong size, too big to fit, no such entry, a name the mod system doesn't know.
- **A script stops working.** An error turns off the hook or event it happened in. The
  message is in the F1 menu's Mods tab, under the mod's name, and in the console.
- **A text in strings.json isn't found.** Spaces and underscores match each other, but
  the case must match; when only the case differs, the message names the right text.
- **The game looks wrong after a change.** Turn mods off one at a time in the Mods tab
  to find the one responsible; the imported discs are never changed.

Scripts and plugins run code with the game's full access to the computer, like any
program. Install them only from people you trust.

## How it works

- **Files:** the game loads files by sector from its tables of archive entries. Before
  the game starts, the client builds the bytes of every replacement (packing them with
  napk when the original is packed), sets the entry's size in the game's table and serves
  the new bytes through the CD drive's sector reader, on each disc that has the archive.
  Movie and voice files that change size go past the end of the disc, and the disc file
  table points there.
- **Program files and game data** are written into the game's memory before it starts.
- **Texture packs:** each picture uploaded into video memory is hashed; a textured
  polygon or sprite that samples one takes its colors from the matching HD picture
  (`psx/src/lain_hd/`, the shaders in `psx/src/render/PsyX_render.cpp`).
- **Hooks:** the build compiles the game from copies in which every game function can be
  replaced at run time (`tools/gen_hooks.py`); the game's own calls go through the hooks.

Code: `client/mods.cpp` (files), `client/moddata.cpp` (game data), `client/modrt.cpp`
(scripts and plugins), `client/hdtex.cpp` (texture packs), `client/mods_ui.cpp` (the tab), `client/napk.c`, `client/tim.c`,
`client/png.c`, `client/json.cpp`.
