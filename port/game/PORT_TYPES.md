# Host representation changes in port/game

The game C in `port/game` runs on 64-bit little-endian hosts. Anything that holds an
address is a real pointer there (8 bytes), so a few globals and structs do not
have the PS1 layout. This file lists them for the data loader and for
`port/tools/gen_arena.py`.

Conventions:

- **PS1 size**: bytes the symbol occupies in the original EXE/RAM (the arena).
- **Host type**: the type the host definition must have, named after the
  declaration in the file given in brackets. Every other file's declaration of the
  symbol has the same layout (see "Views" below).
- **Initial value is a pointer**: `yes` means the symbol lives in `.data` and its
  initial words are PS1 addresses. The loader must read each 32-bit word from the
  EXE and store `PSX_PTR(word)` if `0x80000000 <= word < 0x80200000`, or `NULL` if
  the word is 0. `no (data)` means it is in `.data` but its initial pointer
  words are 0, and any non-pointer members are copied as they are. `no (bss)` means
  it starts zeroed.
- Every source change is marked `/* port: ... */` in the code.

## Host-typed globals

Rows start with the symbol so the generator can pick them up. These symbols are
kept out of the byte arena and defined as host globals.

| Symbol | PS1 size | Host type | Initial value is a pointer | Notes |
|---|---|---|---|---|
| g_node_text_strings | 0x684 | `char *[417]` [80013138.c] | yes (all 417 words; 211 symbolized, the rest literal 0x8001xxxx) | Node/text string table |
| g_node_label_strings | 0xCC | `char *[51]` [80013138.c] | yes (all 51 words) | Option string table |
| g_site_node_tmds | 0x40 | `u32 *[16]` [800281A8.c] | yes | TMD table (grid models) |
| g_site_node_tims | 0x40 | `u32 *[16]` [8002A344.c] | yes | TIM table (grid textures) |
| g_site_open_shard_tmds | 0x10 | `void *[4]` [8002A344.c] | yes | TMD table |
| g_ring_burst_tmds | 0x80 | `void *[32]` [8002A344.c] | yes | TMD table |
| g_view_preset_map | 0x20 | `GsRVIEW2` [800281A8.c] | no (data): `super` is 0; copy vpx to rz | 8002A344.c/8002F4DC.c read it as `s32[]` (words 0-2 only, same offsets) |
| g_view_preset_menu | 0x20 | `GsRVIEW2` [800281A8.c] | no (data), as above | as above |
| g_view_preset_intro | 0x20 | `GsRVIEW2` [800281A8.c] | no (data), as above | as above |
| g_gate_model_tmds | 0x14 | `u32 *[5]` [80031378.c] | yes | Splat splits the table into g_gate_model_tmds (4 words) and D_800955C0 (1 word). The code reads `g_gate_model_tmds[4]`, so the host array has 5 entries and entry 4 comes from D_800955C0 |
| D_800955C0 | 0x4 | (element 4 of g_gate_model_tmds) | yes | Not referenced by name. Relocate its word into `g_gate_model_tmds[4]` |
| g_sjis_digit_strings | 0x28 | `char *[10]` [8003C084.c] | yes | Digit strings |
| g_menu_sprites | 0x4 | `SpriteEntry *` [80023468.c] | no (data, 0) | malloc'd sprite table |
| g_movie_model_tmds | 0x8 | `u32 *[2]` [80031378.c] | yes (literal words 0x80082F90, 0x80082CE0) | TMD pointers |
| g_movie_model_tmds_hi | 0x8 | `u32 *[2]` [80031378.c] | yes (literal words 0x800830A0, 0x80082E38) | TMD pointers |
| g_menu_scroll_tim | 0x4 | `u32 *` [8003D6A8.c] | no (data, 0) | Kept TIM |
| g_lz_magic | 0x4 | `char *` [800379C8.c] | yes (`D_800A675C`, "napk") | Compressed-file magic. The next 8 bytes (two 0xFFFFFFFF words) are other, unreferenced data and stay in the arena |
| g_heap_start | 0x4 | `HeapBlock *` [80039734.c] | no (data, 0) | Heap start |
| g_heap_rover | 0x4 | `HeapBlock *` [80039734.c] | no (data, 0) | Heap free rover |
| g_anim_data | 0x4 | `u8 *` [80013138.c] | no (data, 0) | `s32` in the decomp: archive data start |
| g_anim_entry | 0x4 | `ArchiveEntry *` [80013138.c] | no (data, 0) | Current archive entry |
| g_cd_read_dest | 0x4 | `u8 *` [80013138.c] | no (bss) | CD read destination |
| g_movie_ring_buf | 0x4 | `void *` [80018F48.c] | no (bss) | |
| g_site_file_tables | 0x8 | `FileEntry *[2]` [80018F48.c] | no (bss) | Covers the alias D_800A689C (see below) |
| g_spu_decode_buf | 0x4 | `void *` [all files] | no (bss) | `s32` in the decomp's 80018A38.c/80018F48.c, `s16 *` in its 8001D114.c: SPU decode buffer |
| g_player_load_buf | 0x4 | `void *` [80018F48.c] | no (bss) | |
| g_ending_card_tim | 0x4 | `u32 *` [80018F48.c] | no (bss) | |
| g_font_pixels | 0x4 | `u8 *` [8001D114.c] | no (bss) | |
| g_menu_save_buf | 0x4 | `u8 *` [80023468.c] | no (bss) | Save buffer |
| g_light_fade_target | 0x4 | `FlatLight *` [80023468.c] | no (bss) | `s32` in the decomp's 800281A8.c/80029610.c (declared `void *` there) |
| g_voice_pcm | 0x4 | `u8 *` [80033820.c] | no (bss) | |
| g_voice_name_table | 0x4 | `u8 *` [80033820.c] | no (bss) | |
| g_audio_node_file_buf | 0x4 | `u32 *` [80039734.c] | no (bss) | |
| g_bg_curve_polyline | 0x4 | `Point *` [80039734.c] | no (bss) | |
| g_gfx_load_buf | 0x4 | `void *` [8003D6A8.c] | no (bss) | `s32` in the decomp: async load buffer |
| g_cd_slots | 0x480 | `FileSlot[24]` [80013138.c] | no (bss) | `FileSlot.dest` is a pointer |
| g_level_meter | 0x3C | `MeterColumn *[15]` [80018F48.c] | no (bss) | |
| g_movie_decenv | 0x38 | `DECENV` [80018F48.c] | no (bss) | MDEC decode env: `vlcbuf[2]` and `imgbuf[2]` are pointers |
| g_tim_info | 0x1C | `GsIMAGE` [80018F48.c] | no (bss) | |
| g_slideshow_sprite | 0x4C | `ImageSprite` [80018F48.c] | no (bss) | Embeds a host `POLY_FT4`. 800379C8.c/80039734.c use its start as a `GsSPRITE` (same offset 0 on both) |
| g_lain_anim_default | 0x10 | `SoundSlot` [8001D114.c] | no (bss) | `SoundSlot.handle` is `void *` (`s32` in the decomp) |
| g_lain_idle_anims | 0x10 | `SoundSlot[1]` [8001D114.c] | no (bss) | 800220CC.c's `LoadSlot` view has the same layout (`buf` is `s32 fileId` in the decomp) |
| g_lain_action_anims | 0x60 | `SoundSlot[6]` [8001D114.c] | no (bss) | as above |
| g_msgbox_bars | 0x260 | `SpriteEntry[8]` [80023468.c] | no (bss) | Embeds a host `POLY_FT4` |
| g_ot_tags0 | 0x10000 | `GsOT_TAG[1 << 14]` [80023468.c] | no (bss) | OT tags of g_ot[0] (length 14). GsOT_TAG is 12 bytes on the host |
| g_ot_tags1 | 0x10000 | `GsOT_TAG[1 << 14]` [80023468.c] | no (bss) | OT tags of g_ot[1] |
| g_models | 0x171C | `Model[51]` [800281A8.c] | no (bss) | Scene models. Aliases D_801E4944, D_801E4B14, D_801E5B54 rewritten (below) |
| g_site_node_models | 0x2EAC | `Model[103]` [800281A8.c] | no (bss) | Grid models. Aliases D_801E6054, D_801E8D28 rewritten (below) |
| g_site_level_models | 0x984 | `Model[21]` [800281A8.c] | no (bss) | |
| g_ot_2d | 0x28 | `GsOT[2]` [80023468.c] | no (bss) | |
| g_ot | 0x28 | `GsOT[2]` [80023468.c] | no (bss) | |
| g_ot_2d_tags | 0x800 | `GsOT_TAG[2][1 << 8]` [80023468.c] | no (bss) | OT tags of g_ot_2d[0] and [1] (length 8) |
| g_view | 0x20 | `GsRVIEW2` [800281A8.c] | no (bss) | Aliases D_801EAEE4/D_801EAEE8 rewritten (below). 80018A38.c/80018F48.c declare it as the host `GsRVIEW2` too (`u8[]` in the decomp); 8002A344.c/8002F4DC.c read words 0-2 through `Struct801EAEE0` (same offsets) |
| g_site_open_lines | 0xD20 | `RotCoordPair[20]` [800281A8.c] (`LineObj` in 8002A344.c) | no (bss) | |
| g_site_open_spark_coords | 0x6E0 | `RotCoord[20]` [800281A8.c] (`Obj58` in 8002A344.c) | no (bss) | |
| g_orb_poly | 0x28 | `POLY_FT4` [8002F4DC.c] | no (bss) | Built by the game and passed to GsSortPoly |
| g_streaks | 0x52D0 | `Streak[100]` [80031378.c] (`StructD4` in 8002A344.c) | no (bss) | |
| g_voice_clips | 0x160 | `FileEntry[17]` [80033820.c] (u8 *data, ...) | no (bss) | 20 bytes per entry on PS1. 0x160 bytes up to g_bg_curve_history; the code indexes it by g_voice_clip_count |
| g_bg_curve_history | 0x40 | `Point *[16]` [800379C8.c] | no (bss) | |
| g_bg_curve_lines | 0x40 | `GsGLINE *[16]` [800379C8.c] | no (bss) | |
| g_disc_change_text | 0x720 | `MenuObject[24]` [8003D1B4.c] | no (bss) | Embeds a host `POLY_FT4` |
| g_site_prompt_ring | 0x4C | `MenuObject` [8003D1B4.c] | no (bss) | as above |

Method: every `extern` D_ symbol in port/game was compiled with
`sizeof` probes for arm64 and for i386. The symbols whose size differs are the rows
above. Relocations were checked against `asm/data/*.s`. No other D_ global changes
size, and no other `.data` symbol that the port references holds PS1 pointers.

## Struct types whose host layout differs from the PS1 layout

All of them are only used by the globals above, by heap blocks, or on the stack.

| Type (file) | PS1 size | Pointer members | Views of the same memory, with identical host layout |
|---|---|---|---|
| `Model` (800281A8.c, 80029610.c, 80031378.c, 80033820.c) | 0x74 | via `GsDOBJ2`/`GsCOORDINATE2`, `tmd` | `Obj74` (8002A344.c, 8002F4DC.c). A byte overlay in the decomp (`unk4[0xC]`, `unk10[0x18]`, `unk34[0x2C]`, ...), spelled out as the real members here; `_Static_assert` on the size |
| `RotCoordPair` (800281A8.c, 80031378.c, 80033820.c) | 0xA8 | `GsCOORDINATE2` x2 | `LineObj` (8002A344.c, 8002F4DC.c), spelled out the same way |
| `RotCoord` (800281A8.c) | 0x58 | `GsCOORDINATE2` | `Obj58` (8002A344.c, 8002F4DC.c) |
| `Streak` (80031378.c) | 0xD4 | via `RotCoordPair` | `StructD4` (8002A344.c, 8002F4DC.c): `u8[0xD4]` in the decomp; indexed as an array, so it has Streak's members |
| `GsCOORDINATE2`, `GsDOBJ2`, `GsRVIEW2`, `GsOT`, `GsIMAGE` (local PsyQ copies in many files) | 0x50, 0x10, 0x20, 0x14, 0x1C | yes | Layout must match port/psx/include/psx/libgs.h (see "Open issues") |
| `FileSlot` (80013138.c) | 0x30 | `dest` | |
| `SoundSlot` (8001D114.c) / `LoadSlot` (800220CC.c) | 0x10 | `handle` / `buf` (`s32` in the decomp) | |
| `DECENV` (80018F48.c) | 0x38 | `vlcbuf[2]`, `imgbuf[2]` | |
| `FileEntry` (80033820.c only: `u8 *data`, ...) | 0x14 | `data` | The other files' `FileEntry` is a pointer-free {sector/offset, size} pair |
| `HeapBlock` (80039734.c) | 0x10 | `next`, `prev` | Heap block header (see "Open issues") |

## Offset aliases rewritten as base[index]

The PS1 code uses symbols that point into the middle of another object. When the
base contains pointers, the alias no longer names the same bytes on the host, so
each use is written against the base.

| Alias | Was | Now | Files |
|---|---|---|---|
| D_801E4944 | `GsCOORDINATE2` | `g_models[1].coord` | 80031378.c, 80033820.c |
| D_801E4B14 | `GsCOORDINATE2` | `g_models[5].coord` | 800281A8.c, 80029610.c |
| D_801E5B54 | `Model[]` | `g_models[41 + i]` | 80031378.c, 80033820.c |
| D_801E6054 | `Obj74[]` | `g_site_node_models[1 + i]` | 8002A344.c (declared, unused, in 8002F4DC.c) |
| D_801E8D28 | `ModelInfo *[]` (only `[0]` used) | `g_site_node_models[99].model` | 8002A344.c (declared, unused, in 8002F4DC.c) |
| D_801EAEE4 | `s32[]` | `g_view.vpy` | 80031378.c (declared in 80033820.c) |
| D_801EAEE8 | `s32[]` | `g_view.vpz` | 80031378.c (declared in 80033820.c) |
| D_800A689C | `FileEntry *` | `g_site_file_tables[1]` | 80018F48.c (declared in 80018A38.c) |

Aliases into pointer-free data (for example g_view_path_mid_menu/g_view_path_mid_intro, read as `s32[]`
next to the GsRVIEW2 tables) are unchanged and stay in the arena.

## Hardware access

Declared in `include/port_hw.h`, implemented by the platform layer (port/src/platform.c):

| PS1 access | Helper | Where |
|---|---|---|
| Scratchpad `(u32 *)0x1F800000`, passed to `GsSortObject4` as the packet work area | `u32 *port_scratchpad(void)` | 800281A8.c, model_sort (draw a model's sub-objects) |

BIOS-level calls go through the libapi prototypes: `EnterCriticalSection`/
`ExitCriticalSection` (80013138.c, CD request queue) and `SetMemSize` come from
port/src/platform.c (the critical section takes the CD/VSync "IRQ" lock),
`ResetCallback` from PsyCross. There is no inline asm, no
GTE macro and no 0x1F80xxxx I/O register access in port/game.

## Function signature changes

Callers match the new signatures; `include/game_protos.h` is generated from the definitions.

| Function | Change | Why |
|---|---|---|
| cd_load_archive_entry (80013138.c) | `u8 *dest` becomes `void *dest`; every file's local prototype is `(s32, s32, <table> *, void *)` returning `s32` | Callers passed buffers as `s32` or other pointer types. Some local prototypes declared an `s16` return or a `u16` first argument; slot ids are in -1 to 23, so the values are the same |
| lz_decompress (800379C8.c) | returns `void *` (`u8 *` in the decomp) | Callers store the result as `u32 *` |
| debug_log_open (800379C8.c) | takes `char *logName` (`void` in the decomp) | The PS1 callers pass the log name, and the stub ignores it |
| spinner_init (80023468.c) | first parameter is `const void *` (`s32` in the decomp) | Callers pass a string or byte array. The parameter is unused |
| voice_load_clip_to (80033820.c) | `s32 dst` becomes `void *dst` | It is a load destination |
| light_fade_start (800281A8.c) | `s32 arg1` becomes `void *arg1` | Stored in g_light_fade_target (`FlatLight *`) |
| lain_anim_play_move, lain_anim_play_level_move, lain_anim_play_select, lain_anim_play_save, lain_anim_play_site_enter (800220CC.c) | `void` becomes `s32 ... { return lain_anim_play_sequence(n); }` | The PS1 callers (80013138.c) test `$v0`, which is lain_anim_play_sequence's result |
| gs_sprite_setup callers in 80018F48.c and 800379C8.c | Given the definition's real prototype (unprototyped or all-`s32` in the decomp) | |
| cd_sync_callback, cd_ready_callback (80013138.c) | `(u8 status, u8 *result)` (`(s32, u8 *)` / `(s32)` in the decomp) | libcd's `CdlCB` callback type |
| 8002A344.c / 8002F4DC.c | The 800281A8.c/80031378.c helpers, unprototyped `f()` in the decomp, have real prototypes that use the layout-identical views | |

## Open issues (behaviour that depended on 32-bit pointers or PS1 layouts)

1. **PsyQ types come from port/psx.** The files have no local copies of the
   PsyQ types, macros and prototypes; `include/psx_sdk.h` (pulled in by
   `common.h`) includes the PsyCross headers, and `RECT` is `RECT16`. The structs
   that embed a `POLY_FT4` (`SpriteEntry`, `ImageSprite`, `MenuObject`, `Glyph`,
   `Unk8001D350`) embed PsyCross's (12-byte tag on 64-bit hosts, 0x30 bytes);
   every file's view of them uses the same host `POLY_FT4`, so the views
   agree, and the objects in RAM are host globals (g_slideshow_sprite, g_msgbox_bars,
   g_orb_poly, g_disc_change_text, g_site_prompt_ring, table above). Hard-coded sizes of the
   malloc'd sprite tables (0x2600, 0x1D18) are `n * sizeof(SpriteEntry)`. The
   game sets tags with libgpu's `setlen`/`setPolyFT4`. PsyCross is built without
   PGXP (psx/CMakeLists.txt), so primitive coordinates are plain shorts, as on the
   PS1. Library functions the signature scan missed are named after the SDK in
   config/symbol_addrs.txt, and the port calls
   them by those names (GsSetNearClip/GsSetFarClip/GsSetProjection/GsGetProjection/
   GsSetWorkBase/PCclose/SsUtReverbOn; RotMatrix_gte is RotMatrix).
2. **Heap (80039734.c).** `HeapBlock` grows from 16 to 24 bytes, and allocations
   are only rounded to 4 bytes, so host blocks and headers can be 4-byte aligned
   only (arm64 and x86-64 allow this for ordinary loads and stores). The heap does
   not use the 0xD4000 bytes at g_heap_area: `lain_anim_init` puts it in a
   2 MB host-only extension of the arena at "address" 0x80200000
   (`PSX_HEAP_ADDR`, include/psx_arena.h), so it has room for the bigger host
   blocks and heap data still has 32-bit addresses (see 3). `heap_check_block`
   (unused) finds the header at `ptr - 8`, which on the PS1 is the middle of the
   16-byte header; the port keeps that. `menu_load_texture` calls the BIOS `free()` on
   a game-heap block; the port skips that call (the block stays allocated, as on
   the PS1).
3. **Data that libgs relocates in place.** `GsMapModelingData` rewrites the TMD
   object table's `vert_top`/`normal_top`/`primitive_top`. The game reads them
   itself (tmd_set_vertex and friends, through copies of the object entry), so the
   client calls the port's `GsSetMapBase(psx_arena, PSX_ARENA_SIZE, 0x80000000)`:
   TMDs inside the arena are mapped with PS1 addresses, and the game resolves them
   with `PSX_PTR`. TMDs are only ever in the arena (EXE data or the heap). TIMs:
   `GsGetTimInfo` and the port's `OpenTIM`/`ReadTIM` (src/sdk_extra.c) return host
   pointers into the TIM data and don't modify it.
4. **LineObj.end.** The PS1 C overlay put `end` at 0x78, but the original binary
   (asm node_open_anim_effect, a NON_MATCHING function) stores it at 0x68,
   `coord1.coord.t`. The port's `LineObj` follows the binary. This is the one
   place where the port's behaviour differs from the decomp's C (and matches the
   original game).
5. **Unprototyped calls.** Calls that went through `f()` declarations passed
   `int`, and the callee truncated to its parameter types (`s16`/`u8`). The real
   prototypes do the same conversion at the call site. Values that are out of
   range for the parameter give the same result.
6. `SpuReadDecodedData` is declared locally as `void (void *, s32)`. The real
   signature is `long SpuReadDecodedData(SpuDecodedData *, long)` (the result is
   unused). `SsUtReverbOn` (0x8006179C, called in 800220CC.c) is libsnd's.
7. **libc `rand()`.** PsyQ's `rand()` returns 0-0x7FFF; the host's goes up to 2^31 - 1. The
   game relies on the small range (e.g. the music player's level meter uses `rand() / 8191` as
   a bar count; host values write far past its sprite arrays and garble the player screen
   into "Cou001"). `common.h` maps `rand`/`srand` to `port_rand`/`port_srand`
   (game/port_libc.c), the EXE's own LCG (0x41C64E6D, 12345, seed starts at 0).
