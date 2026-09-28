# Decompilation

This guide covers the matching decompilation. The C source in `src/`, compiled with
the original compiler and flags, produces an executable that is byte-for-byte
identical to the retail one. The native client in `port/` is built from the same code.

For the per-function workflow, see [MATCHING.md](MATCHING.md).

## Target

| | |
|---|---|
| Game | Serial Experiments Lain, Pioneer LDC, 1998, Japan only |
| Discs | 2 (Site A, Site B) |
| Executable | `SLPS_016.03` (disc 1) and `SLPS_016.04` (disc 2), byte-identical |
| Executable SHA-1 | `a0012634f82dd7fcc4ee5a3f97ddc052573b4815` |
| Size | 618,496 bytes (0x800 header + 0x96800 text/data) |
| Load address | `0x80010000` |
| Entry point | `0x8001307C` |
| Initial SP | `0x801FFFF0` (SYSTEM.CNF: `STACK = 801fff00`) |
| `$gp` | `0x800A5EE4`, set at runtime by `stup1` (the EXE header says 0). Game code uses small data (`-G8`). |
| SDK | Sony PsyQ 4.3 (signature match: 348 objects), plus a newer standalone libpad (4.5) |
| Compiler | GCC 2.8.x PsyQ build, `-O2 -G8`, ASPSX 2.79 rules (see [Compiler](#compiler)) |
| Overlays | None. All game code is in the one executable. |

There is one program to decompile. The two discs differ only in data (`SITEA.BIN`
vs `SITEB.BIN`, movies, XA voice tracks).

### Memory layout

| Range | Section | Contents |
|---|---|---|
| `80010000-80013074` | `.rodata` | ~12 KB |
| `80013074-80013138` | `.text` | startup (`2MBYTE.OBJ`: `__main`, `stup0-2`) |
| `80013138-8003E8A4` | `.text` | game code, 178 KB (`main` = `0x80013138`) |
| `8003E8A4-80070F50` | `.text` | PsyQ libraries, ~850 functions (named from signatures) |
| `80070F50-800A6800` | `.data` | ~214 KB |

Game `.rodata` is `80010000-80011DC4`; library `.rodata` follows. Each game source
file's rodata is one contiguous slice, in the same order as its code, and no two
files share any (checked for all 72 referenced symbols).

### Disc hashes

Redump format, one MODE2/2352 track per disc. Not yet verified against redump.org.

| Disc | SHA-1 |
|---|---|
| Disc 1 | `6426fbdb45f27e089af650cddb8c41929c7890de` |
| Disc 2 | `409668af454a0e0a33a71c1d22918d765e718608` |

## Data files

`tools/disc.py` extracts these. The formats are partly documented; the decompiled
loaders are the reference.

| File | Magic | Contents |
|---|---|---|
| `SITEA.BIN` / `SITEB.BIN` | `napk` | Site data for each disc: node pictures and tables |
| `LAPKS.BIN` | `lapk` | Header + offset table; Lain's animated sprite packs |
| `BIN.BIN` | TIM (`10 00 00 00`) | Starts with a 4bpp TIM; UI graphics and tables |
| `SND.BIN` | `pBAV` | Sony VAB sound bank (VH/VB) |
| `VOICE.BIN` | none | SPU-ADPCM syllable samples for the name-calling voice |
| `MOVIE/*.STR`, `MOVIE2/*.STR` | | MDEC video interleaved with XA audio, extracted as raw 2352-byte sectors |
| `XA/LAIN*.XA` | | XA-ADPCM voice sessions, extracted raw |
| `LAIN_1.INF` / `LAIN_2.INF` | | Disc identification text |

The ISO directory does not flag stream files with XA attributes, so `disc.py`
decides per file from the sector subheaders (Form 2 and real-time bits).

## Toolchain

| Tool | Role |
|---|---|
| [splat](https://github.com/ethteck/splat) | Splits the executable into asm/data and generates the linker script (`config/slps_016.03.yaml`) |
| [spimdisasm](https://github.com/Decompollaborate/spimdisasm) / rabbitizer | MIPS disassembly (used by splat) |
| GCC 2.x (PsyQ build) + [maspsx](https://github.com/mkst/maspsx) | The original compiler. maspsx emulates PsyQ's `aspsx` assembler on top of GNU as. |
| mips binutils | Assembling and linking, in Docker for reproducible builds (`Dockerfile`, `tools/docker.sh`) |
| [m2c](https://github.com/matt-kempster/m2c) | First-draft MIPS-to-C for each function |
| [decomp.me](https://decomp.me) | Shared scratchpads for matching single functions (has PsyQ presets) |
| [objdiff](https://github.com/encounter/objdiff) / asm-differ | Per-function diffs against the original |

### Compiler

The compiler was identified by compiling candidate C under every PsyQ-era GCC
build (`tools/probe.py`):

- The mask LZ decompressor `anim_decompress_mask` matches exactly under 2.7.2-cdk,
  2.8.0-psx and 2.8.1-psx at `-O2`. At `-O1` it is 34+ instructions off; 2.6.3-psx,
  2.7.2-psx and 2.91.66 are 43+ off.
- 2.7.2-cdk differs on almost every other function, mostly in epilogue scheduling.
  2.8.0-psx and 2.8.1-psx remain tied. The build uses `2.8.1-psx`.
- `-G8`: GCC addresses small globals through `$gp`. maspsx only does this for symbols
  defined in the current file, so `tools/extern_sdata.py` turns cc1's
  `.extern sym,size` hints into common symbols (`--use-comm-section`), which the
  linker resolves to the real addresses.

Pipeline (`tools/cc.sh`): `cpp`, then `cc1 -O2 -G8 -mips1`, then `extern_sdata.py`,
then `maspsx --aspsx-version=2.79 --expand-div --macro-inc --use-comm-section`
(override with `ASPSX_VER`), then GNU `as`.

### Scripts

| Script | Purpose |
|---|---|
| `tools/m2c.sh <func>` | First-draft C for a function |
| `tools/check.sh <func> <file.c>` | Instruction diff count per candidate compiler (0 = match) |
| `tools/fdiff.sh <func> <file.c>` | Side-by-side asm diff |
| `tools/whatdiff.py` | After `make`: which functions differ from retail |
| `tools/progress.py` | Matched / non-matching counts (`--files` for a per-file breakdown) |
| `tools/objdiff_config.py` | objdiff units and target asm for `make report` (decomp.dev progress, see [MATCHING.md](MATCHING.md#progress-report-decompdev)) |
| `tools/psyq_match.py` | PsyQ library identification from signatures |
| `tools/rename.py` | Renames a symbol in `config/`, `src/`, `include/`, the docs and `port/` |
| `tools/permute.sh` | decomp-permuter run for one function |

### Linker

psylink aligned sections to 4 bytes, not 16. `.rodata` ends at `0x80013074`, so
splat's default 16-byte alignment would shift all of `.text` by 12 bytes. The splat
config sets `subalign: 4` and disables end-of-section alignment.

## Workflow

```
disc image --disc.py--> extract/disc1/SLPS_016.03
                               |
                          splat split
                               v
               asm/  (one .s per function)
                               |
          pick a function -> m2c -> edit C in src/ -> compile
                               |                          |
                               +---- diff until 0 <-------+
                               v
     replace INCLUDE_ASM with the C body; the full build SHA-1 must still match
```

### Rules

1. **The build always matches.** `make` produces `build/SLPS_016.03` and checks its
   SHA-1 against `config/slps_016.03.sha1`.
2. **Non-matching functions stay as `INCLUDE_ASM`.** Functionally correct C that does
   not match yet goes in `#ifdef NON_MATCHING`, with the asm as the default.
3. **PsyQ library functions are not hand-decompiled.** They are identified by
   signature, named, and excluded from progress. The native client replaces them.
4. **Names follow understanding.** Names live in `config/symbol_addrs.txt`
   (`name = 0x80XXXXXX; // type:func`). Unknown functions stay `func_80XXXXXX`.
5. **No game data in the repository.** No executable bytes, asm, extracted assets or
   copied text. `asm/`, `assets/` and `extract/` are generated locally and git-ignored.

### Naming

- **Functions:** `subsystem_verb_object` in snake_case (`cd_load_file_by_id`,
  `site_draw_notice`, `loading_anim_reload`). Library code the signature scan missed
  keeps its SDK name (`PCclose`, `GsSetNearClip`) and goes in the hand-written part at
  the top of `config/symbol_addrs.txt`, not in the generated `psyq_symbols.txt`.
- **Globals:** `g_` + subsystem + thing (`g_site_cursor_row`, `g_movie_vlc_table`,
  `g_ring_models_outer_tmd`). Images and models end in `_tim` / `_tmd`. Flat string
  constants are `g_str_*` or `g_<screen>_str_*`. A compiler-generated initializer for
  a local array has no `g_` (`start_menu_flicker_order_init`).
- **Struct fields:** named after their use. PsyQ layouts keep the SDK names
  (`vert_top`/`n_vert` for a TMD object, `vpx`/`vpy`/`vpz` for GsRVIEW2). Unused or
  padding fields stay `unkXX`. A field is renamed in `src/game` and `port/game` together.
- **Renaming:** `tools/rename.py old new` (or a file of `old<TAB>new` lines), then
  `tools/docker.sh make split && tools/docker.sh make` (must print OK), then
  `python3 port/tools/gen_arena.py && python3 port/tools/gen_protos.py`. rename.py
  files a renamed `func_` symbol as a function; if it is data (`g_anim_mask_and`,
  splat's `func_80070F4C`), move the line to the globals section by hand.
- **Sizes in `symbol_addrs.txt`** run to the next symbol splat knows, not to the end
  of the C object. When the original code reaches inside an object through its own
  symbol (`g_vram_move_rect_y` is `g_vram_move_rect.y`), both stay symbols and the
  first one's size only extends to the second.
- **Remaining `D_XXXXXXXX` names** are either addresses inside another named object
  (a TIM + 4, a vector's `.vy`, `g_models[5].coord`), which carry a `/* = ... */`
  comment at their `extern`, or values that are only ever cleared or written, with
  nothing to indicate their purpose. `D_801F96E4` is libcd's `StCdIntrFlag`; it keeps
  the placeholder because the port's PsyQ layer defines that name itself.

## Progress

Game code only (PsyQ libraries excluded). `tools/progress.py --files` prints live numbers.

| Milestone | Status |
|---|---|
| Disc intake and extraction | done |
| Round-trip matching build | done |
| Compiler identified | GCC 2.8.x (`2.8.1-psx`) `-O2 -G8`, ASPSX 2.79 |
| PsyQ libraries identified | done: PsyQ 4.3 + libpad 4.5, 651 symbols |
| `.rodata` per translation unit | done |
| Emitted `.sdata` per translation unit | done |
| Game functions as matching C | 321 / 321 (100% of bytes) |
| Functions with no C | 0 |
| `.data` / `.bss` as C | not started (globals are still asm data) |

### Translation units

The source files correspond to the original object files. Boundaries come from the
original section layout: jump-table alignment in `.rodata`, and `$gp` versus absolute
addressing of globals.

| File | Contents |
|---|---|
| `80013138.c` | boot, `main`, CD file loader, picture decoding, map cursor, main game state machine |
| `80018A38.c` | pad input, menu cursor |
| `80018F48.c` | FMV / music player (based on the PsyQ movie sample), ending credits |
| `8001D114.c` | text windows, font, sound banks |
| `800220CC.c` | file load slots, node map, sound init |
| `80023468.c` | menu/dialog UI, 7x7 item grid, pause/system menu, save/load |
| `800281A8.c` | 3D models and camera (libgs layer), curves |
| `80029610.c` | grid scene setup |
| `8002A344.c` | grid/stage scene controller, camera paths |
| `8002F4DC.c` | scene objects, font sprites, particles |
| `80031378.c` | rain effect, 3D scene updates |
| `80033820.c` | start menu (new/load game), name entry, romaji voice synthesis |
| `800379C8.c` | intro sequence, SPU helpers, animated background |
| `80039734.c` | yes/no dialog, heap allocator, credits, CD lid watchdog, title logo |
| `8003C084.c` | memory card wrappers, save image and checksums |
| `8003CB08.c` | menu frames, VRAM dump (debug) |
| `8003D1B4.c` | disc change screen, site change prompt (`site_change_prompt_run`) |
| `8003D6A8.c` | title screen, background loader, loading animation |
