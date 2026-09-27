# Hidden and unused content

Findings from the decompilation of *Serial Experiments Lain* (PlayStation, Japan:
SLPS-01603/01604): rare events, stats the game tracks but never displays, and disc
content the game never uses. Each entry cites the code or data it rests on, so it
can be checked against the decompiled source in `src/`.

## Sources and prior work

Searches of lainTSX, lain.wiki, laingame.net, jpsxdec's laintools, es.wikipedia and
Japanese fan wikis turned up none of items 1-4 and 7-10. The fan remake lainTSX
(github.com/ad044/lainTSX) differs from the original game on items 7, 8, 9, 10 and
on the name-voice bug in item 4. The Cutting Room Floor page
(https://tcrf.net/Serial_Experiments_Lain_(PlayStation)) has not been compared.

Images and sound clips are cited by path under `img/` and `audio/` next to this
file. They are extracted from the game discs and are therefore not committed (see
`.gitignore`). The native client's launcher decodes much of the same content from
the player's own discs in its "Unused" tab (`port/client/unused_gallery.cpp`).

Addresses are for `SLPS_016.03`; symbol names are those in `config/symbol_addrs.txt`.

## 1. The "Genome" save title and four hidden stats

`save_build_image` (`0x8003C40C`, `src/game/8003C084.c`, matching C) builds the
memory-card save title:

```c
if (rand() % 70 != 0) {
    *(SaveTitle *)(buf + 4) = g_save_title;        /* 連続ゲイム小説［ｌａｉｎ］　第 */
} else {
    *(SaveTitle *)(buf + 4) = g_save_title_rare;   /* 連続ゲノム小説［ｌａｉｎ］　第 */
}
```

- The title normally reads **連続ゲイム小説［lain］ 第N回** ("Serial Game Novel
  [lain], part N"). One save in 70 reads **連続ゲノム小説**, "Serial **Genome**
  Novel", a one-kana pun (イ to ノ).
- After the part number, `rand() % 4` appends one of four stats that the game counts
  but shows nowhere else:

| Title text | Variable | Increment |
|---|---|---|
| ときめき度 NNN％ ("heart-flutter") | `g_stat_tokimeki` / `0x800A6848` | +1 each time a node is opened (up to 100) |
| 毒電波 N つ ("N poison radio waves") | `g_screensaver_count` / `0x800A6852` | +1 each time the idle screensaver starts (up to 9); shown only when not 0 |
| はるまげ度 NNN％ | `g_stat_harumage` / `0x800A6846` | +1 each time ○ is pressed on an empty cell of the node map (`site_handle_command` / `0x8001598C`) |
| がっくり度 NNN％ ("dejection") | `g_stat_gakkuri` / `0x800A683E` | +1 per frame while Lain's node-selection animation runs, including successful opens (`site_run` / `0x80016430`, up to 100; one ordinary open gave 83%) |

- The four stats are **not saved**. `save_build_image` does not write them, and they
  reset to 0 on a new game and on every load, so the percentage in a title counts
  only since the last load. Nothing else reads them: they have no effect on
  animations, endings or the screensaver.
- The part number **第N回 counts loads, not saves.** `g_save_count` increases only
  when a save is loaded (`save_apply_image`), so two saves in one session carry the
  same number. In the native client, a 第1回 card loaded once and saved became 第2回.

**Observation.** The title appears in the console's memory-card manager or any
memory-card editor. A save made in the native client read
`連続ゲイム小説［ｌａｉｎ］　第１回：がっくり度　　０％`. The Genome title takes
about 70 saves on average.

## 2. Eleven Lain animations the game never plays

`LAPKS.BIN` (identical on both discs) holds 59 packs of Lain's animated sprite,
indexed through `g_lapks_file_table` / `0x80098D10`. The program loads packs from
these lists only:

| Source | Packs |
|---|---|
| start-up | 8, 9 |
| idle pool `g_lain_idle_anim_files` / `0x800735E8` | 38 entries, picked with `rand() % 38` |
| page change `g_lain_anim_sets3` / `0x80073634` | two "move" sets (called with 0 or 1) |
| reactions `g_lain_anim_sets4` / `0x80073658` | five "select" sets (`node_pick_select_anim` returns 0-4, or -1 for an empty cell, checked before loading) |
| bank change `g_lain_level_anim_files` / `0x800A5FD8` | two level packs |
| save menu `D_800A5FE4`, `D_800A5FEC` | two packs |
| talking `g_lain_anim_set6` / `0x800736B8` | six packs |

Nothing else reads `g_lapks_file_table`. Together the lists name 48 packs; the
remaining eleven are never loaded:

| Pack | Frames | Content | Image |
|---|---|---|---|
| 6 | 31 | fidgeting with her hands, then a wink | `img/lapks/06.png` |
| 10-13 | 15 each | four head-turn / look-around variants | `img/lapks/10.png` to `13.png` |
| 17 | 17 | a full turn-around | `img/lapks/17.png` |
| 19 | | another full turn-around | `img/lapks/19.png` |
| 23 | | looking aside with her eyes closed | `img/lapks/23.png` |
| 26, 28 | | gestures with raised hands | `img/lapks/26.png`, `28.png` |
| 51 | 39 | raising her arm, surprised, then pointing | `img/lapks/51.png` |

Overview sheets: `img/unused_a.png` (6, 10-13, 17) and `img/unused_b.png` (19, 23,
26, 28, 51). The frames were decoded with a reimplementation of the game's VLC/MDEC
picture format.

**Evidence.** Only `8001D114.c` loads from `LAPKS.BIN` (archive 4); the other loaders
that take a variable archive number all use archive 6. The pack lists above were read
from the program with their exact sizes. A 10-minute scripted session of the native
client with every disc read logged (`LAIN_LOG_READS`: idle, moving between levels,
opening and cancelling nodes, the menu) loaded 25 different packs from `LAPKS.BIN`,
none of them among the eleven.

**Overlap with used animations.** Byte comparison of the compressed frames shows that
no unused pack is a copy of a used one, but some frames are shared. Every pack starts
and ends on Lain's standing pose, and packs 17, 19, 23, 26 and 28 share about 40-60%
of their frames with animations the game plays (26 with the raised-hands pack 27, 23
with 24). Packs 6 (29 of 31 frames unseen) and 51 (37 of 39) are almost entirely
unseen; 10-13 each have 12 of 15 unseen frames.

lainTSX extracted every frame of `LAPKS.BIN` for its remake, so some of these frames
may have been seen before. The new result is that the original game never plays them.

## 3. Lain's eyes on the end/continue screen

On the screen after an ending (`site_change_prompt_run` / `0x8003D1B4`,
`src/game/8003D1B4.c`), every frame (30 fps) has a 1-in-500 chance
(`rand() % 500 == 0`) of fading in a semi-transparent strip of Lain's eyes (sprite
`D_8009D9C0`, 184x16, from `BIN.BIN` entry 0x1F) for 180 frames (6 seconds). On
average it appears about every 17 seconds of waiting. The texture page containing
the strip: `img/bin31.png`. Not yet observed in the native client, since it requires
a save made after an ending.

## 4. Unused sounds in the name-calling voice

The voice that calls the player's name is assembled from syllable samples in
`VOICE.BIN` (`voice_synth_romaji` / `0x800371C4`; the name table is `BIN.BIN` entry
0x14, 640 file names). The code only requests names of the forms `V.WAV`, `p_V.WAV`,
`p_p.WAV`, `SYL.WAV` and `p_SYL.WAV`, so these samples are never played:

- `A/E/I/N/O/U_CAVE.WAV` (about 160 bytes each) and `A/E/I/N/O/U_END.WAV`
  (0.2-0.3 s). Extracted as 16-bit PCM at about 7.9 kHz in `audio/`.
- The lookup stops after 0x27E (638) entries, so the last name, `U_ZU.WAV`, is never
  found; the voice falls back to plain `ZU.WAV`.
- Most `A_K*` transitions are missing from the table (only `O_KO` and `U_KO` exist).
- **ピ, プ, ペ and ポ are dropped from the name.** `name_entry_to_romaji`
  (`src/game/80033820.c`) handles voiced codes 49-69 only. The handakuten row gives
  69-73, so パ is voiced ("PA") but ピ/プ/ペ/ポ (and ピャ/ピュ/ピョ) match nothing and
  are silently left out of the spoken name. `PI/PU/PE/PO.WAV` and `PYA/PYU/PYO.WAV`
  are therefore never played. lainTSX voices them.
- **The "JY" transitions are never found.** The base samples are named
  `ZYA/ZYU/ZYO.WAV`, but the transitions are named `A_JYA.WAV`, `I_JYU.WAV`, and so on
  (18 files). The code requests `A_ZYA.WAV`, finds nothing and falls back to the
  plain sample.
- ヂ and ヅ cannot be entered on the name screen (the dakuten step skips them), and
  the `SHA/CHA` branch of `voice_synth_romaji` never runs.

In all, 38 of the 639 samples are unreachable.

## 5. Developer leftovers

None of these is reachable.

- Debug text `"MEMORY TOO FEW"` and `"PurgeIdle"` is printed with `FntPrint`, but
  `FntFlush` is never called, so it never appears.
- `"1left memory %d"` and `"2left memory %d"` go to `debug_printf` / `0x800395C8`,
  and the `d:\usr\tmp\psx.log` log file setup is `debug_log_open` / `0x800395E0`.
  Both are empty stubs in the release.
- `vram_dump_tim` / `0x8003CF38` is a VRAM-to-TIM screenshot dumper that writes to
  the development kit's host PC (`PCcreat`). It is never called.
- `mcard_unformat` / `0x8003C0D0` unformats a memory card. It is never called.
- The picture decoder records its timings (`g_anim_decode_vblanks` / `0x800A6800`,
  `D_800A67F4`-`D_800A67FA`), but nothing displays them.
- There is no debug menu, button code or special name. The pad reader
  `pad_read_command` / `0x80018C40` reduces input to one command per frame, so button
  combinations cannot be detected at all.

## 6. Corrections to fan documentation

- **Site B screensaver movies.** `PO1.STR` and `PO2.STR` play only once all six
  Polytan parts are collected (`g_polytan_parts` / `0x800A5F12 == 0x3F`), with a
  chance of about 1 in 6 per screensaver (`idle_play_random_movie` / `0x8001B77C`,
  `idle_play_random_media` / `0x8001CD08`). Without the parts there is still a
  1-in-65,536 chance of `PO1` (when `rand()` returns 32767). lainTSX uses 30%.
- **Endings needed for the last nodes.** Each ending increments
  `g_media_played_count` / `0x800A5F16`. A node is *visible* when its depth is at
  most endings + 1 (`node_is_displayable` / `0x80022710`), but it *opens* only when
  its depth is at most endings (`node_pick_select_anim`). Each layer of nodes
  therefore appears one ending before it can be opened: the 17 depth-5 TaK nodes
  appear after four endings and need a **fifth** to open. The ending nodes (Cou053,
  Dia048, Lda237, Tda092) can be replayed. Guides state that nothing new appears
  after the third ending; lainTSX has no "visible but locked" state.
- **Env nodes.** Env001-012 have grid cell 0, so they are never placed on the map
  (previously known).
- **Files.** Every file on both discs is referenced by the program; there are no
  orphan files. `BIN.BIN` entries 18, 19 and 44 (a gradient strip, a noise texture and
  UI bars, `img/bin_unused.png`) are never loaded by a fixed index.

## 7. The credits replace unwatched clips with a "NO DATA" card

The end credits movie (`ENDROLL1.STR`) is a montage of the game's 57 short "Dc"
movies. While it plays, `ending_movie_play` (`src/game/80018F48.c`) checks each clip
against the save. For every Dc movie not watched to the end (node flag bit 26), the
credits show a still card in place of that clip's section of the montage:
**"NO DATA  ????.??.????? authorised_il active_file:lv. ftp/tsk.S_server"**
(`SITEB.BIN` entry 0x22A). The audio continues underneath.

- Cue table `g_ending_cues` (`0x80073270`): 57 entries of {show, start frame, end frame}.
- Bugs: the cue index stops at 0x36, so the last two clips (Dc1057/F098,
  Dc1058/F103) are never replaced. Their cue data is broken regardless (one has
  start > end, the other copies the entry before it), and the first two cues overlap.
- Reproduced in the native client with a save in which only Dc1001-Dc1010 were
  watched: real clips early in the credits, the NO DATA card for the rest
  (`img/round2/e4_sheet.png`, `endroll_1990.png`, `endroll_2590.png`; the card
  itself: `img/round2/endroll_card.png`).
- lainTSX plays the credits movie unchanged, so the card never appears there.

## 8. The screensaver's tenth voice: 1 in 4,096

After about 42 seconds without input on the node map, a screensaver starts: a movie
or, half the time, one of ten "network voice" clips over a slideshow. The clip is
chosen with `g_idle_voice_base[site] + rand() / 3640` (`idle_play_random_voice` /
`0x8001BA60`). `rand()` returns 0-32767, so `rand() / 3640` is 0-8 almost always and
9 only when `rand()` is 32760 or more. Clips 1-9 each come up about 1 time in 9;
**clip 10 comes up 1 time in 4,096** (1 in 8,192 screensavers). Clip 10 is Env012
(`LAIN13.XA` channel 28), the longest at 46.5 s
(`audio/round2/idle_music_10_LAIN13_c28.wav`).

Site B plays the same ten clips. Its table points at media 0x2BB-0x2C4, which are
listed under the GaTE and P2 nodes (which never play them) and are byte-identical
copies on `LAIN21.XA` channels 5-14. lainTSX picks the ten clips with equal odds on
Site A and uses eight unrelated nodes on Site B. lain.wiki describes the Env clips as
extra content outside the game.

## 9. Site A's first movie unlocks a Site B movie

A node's `parent` field (+0x24) names the node whose viewing unlocks it. Only two
links cross the sites: Dc1055 (Site B) is unlocked by Cou016 (Site A), and
**Dc1058** (Site B, `F103.STR`, hidden at the start) by **Dc1001** (Site A,
`F001.STR`). `node_unlock_children` makes the child visible on the other site too.
Reproduced in the native client: after watching F001 on Site A and saving, Dc1058's
flag byte changed from 0x00 to 0x01 (`img/round2/d3_sheet.png`).

lainTSX's node data has 715 of the 716 nodes. **Dc1001 is missing**, so Dc1058
(whose `unlocked_by` points at it) never appears there.

## 10. Three photos nothing shows

Collecting every image index the program can load from `SITEA.BIN`/`SITEB.BIN`
(node pictures, the screensaver slideshows, the name-calling screen, the ending card)
leaves three photos that are never shown (`img/round2/unref_sheet.png`):

| Index | Content | Image |
|---|---|---|
| 631 | two boys on a sports field, one in a #3 baseball shirt | `img/round2/s_0_631.png` |
| 651 | a building entrance with stairs at night | `img/round2/s_0_651.png` |
| 744 | an orange building under power lines | `img/round2/s_0_744.png` |

There is also a spare mouth frame for Lain's name-calling face (SITEA 787 =
SITEB 552, `img/round2/s_0_787.png`).

## 11. Two sound effects nothing plays

In the sound bank (`SND.BIN`, a VAB with 32 programs), programs 2 and 3 (both sample
VAG 3, 0.42 s) and program 22 (VAG 23, 1.63 s) are never triggered by any
sound-effect call, Lain's animation sounds or the two music sequences
(`audio/round2/orphan_vag3.wav`, `orphan_vag23.wav`).

## 12. Minor findings

- **Build dates.** Both discs were mastered on 1998-09-30, but the program
  (`SLPS_016.03/04`) and `BIN.BIN` are dated **1998-10-21**, three weeks later: they
  were replaced late in production. The oldest file is `VOICE.BIN` (1998-08-13); the
  last movie, `F079.STR`, is dated eight minutes before disc 1's volume.
- **Text.** There is no hidden text (all 417 keywords and 51 node labels are used),
  but there are typos and quirks: "cO.unconsious"; keyword pairs that differ only by a
  trailing space ("decision" / "decision "), which their links treat as different
  words; "open the nExt." appears on one node only (Dc1058), so its keyword link
  never finds anything; the NO DATA card spells "authorised_il" while node labels
  say "authorized_il".
- **Dead ends.** Every voice channel and every movie on the discs is used (the only
  unreferenced channels are filler copies of one clip). There are no play-time,
  save-count or disc-swap unlocks, and no New Game+.
