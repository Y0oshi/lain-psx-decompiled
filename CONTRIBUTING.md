# Contributing

There are two parts to work on: the matching decompilation in `src/`, and the native
client in `port/`. Both need your own dumps of the two discs; nothing from the game is
ever committed.

## Ground rules

- **No game data.** No disc images, extracted files, executable bytes, generated asm or
  copied game text. `asm/`, `assets/`, `extract/` and `build/` stay git-ignored.
- **The build always matches.** After any change to `src/`, `include/` or `config/`,
  `tools/docker.sh make` must end with `build/SLPS_016.03: OK`.
- **`src/` is the original game.** It only ever holds code that reproduces the retail
  executable. Fixes, features and anything that changes behaviour go in `port/`.
- **One topic per pull request.** A matched function, a batch of renames or a client
  fix; not all three.

## Decompilation

Setup is in the [README](README.md#building-the-decompilation). Then:

1. Pick a function under `#ifdef NON_MATCHING` in `src/game/`
   (`grep -rn "ifdef NON_MATCHING" src/game`; `tools/progress.py --files` shows the
   totals per file).
2. Edit its C, then compare it with the original:

   ```sh
   tools/docker.sh tools/check.sh <function> src/game/<file>.c   # 0 = match
   tools/docker.sh tools/fdiff.sh <function> src/game/<file>.c   # side-by-side diff
   ```
3. When it matches, move the C out of `#ifdef NON_MATCHING` so it replaces the
   `INCLUDE_ASM`, and run the full build.
4. Refresh the progress report:

   ```sh
   tools/docker.sh make report
   cp build/report.json progress/SLPS_016.03_report.json
   ```

Techniques, compiler quirks and the permuter are covered in
[docs/MATCHING.md](docs/MATCHING.md).

### Naming

Follow the scheme in [docs/DECOMPILATION.md](docs/DECOMPILATION.md#naming)
(`subsystem_verb_object` for functions, `g_` for globals). Rename with the tool, never
by hand, so `src/`, `port/`, `config/` and the docs stay in step:

```sh
tools/rename.py old_name new_name
tools/docker.sh make split && tools/docker.sh make
python3 port/tools/gen_arena.py && python3 port/tools/gen_protos.py
```

Leave a name as `func_XXXXXXXX` or `D_XXXXXXXX` until its purpose is known.

## Native client

Build it as described in the [README](README.md#building-the-client), then check that
the game data still lines up and that it boots:

```sh
build/port/lain --selftest
build/port/lain
```

- `port/game/` is a 64-bit copy of the game code. A change made to a function there
  does not carry over to `src/`, and the reverse is also true.
- `port/psx/` is based on PsyCross. Mark changes to upstream files with a `lain:`
  comment so they stay easy to find.
- Test on the platforms you have, and say in the pull request which ones those were.
  Gamepads and Linux on real hardware need testing most.

[port/PORTING.md](port/PORTING.md) documents the client's structure, pack formats and
the `LAIN_*` debug options.

## Style

- Match the surrounding code. Types come from `include/types.h` (`s32`, `u16`, ...).
- Comments state what is not obvious from the code: a hardware quirk, why an odd
  construct matches, a measured value. Keep them short.
- Commit messages: a short summary line, then details if needed.

## Reporting bugs

Open an issue with:

- your system and the release version (or commit)
- what you did and what happened
- the terminal output: run `lain` (`lain.exe` on Windows,
  `Lain.app/Contents/MacOS/lain` on macOS) from a terminal to see it

Do not attach disc images or other game files.

## License

Contributions are released under the [MIT License](LICENSE).
