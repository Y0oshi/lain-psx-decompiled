# Matching guide

How to turn an `INCLUDE_ASM` function into C that compiles to identical
instructions. Every game function is matched; the same techniques apply when editing
matched code. [DECOMPILATION.md](DECOMPILATION.md) covers the setup and toolchain.

All commands run inside the container, through `tools/docker.sh <cmd>`.

## Loop

1. Pick a function, e.g. `INCLUDE_ASM("asm/nonmatchings/game/80013138", game_vsync_callback);`
2. Generate a first draft: `tools/m2c.sh game_vsync_callback`
3. Replace the `INCLUDE_ASM` line with the C (plus any `extern` declarations it needs).
4. Check it: `tools/check.sh game_vsync_callback src/game/80013138.c`
   - `0 diffs` on `2.8.1-psx` means the instructions match.
   - Otherwise, show the differences: `tools/fdiff.sh game_vsync_callback src/game/80013138.c`
5. Iterate on the C until it matches (see [Techniques](#techniques)).
6. Run `make` for a full build. The SHA-1 check also covers symbol references,
   which `check.sh` masks.

## Toolchain facts

- ASPSX 2.79 rules (`$gp`-relative `sym+offset` on small externs). 2.56 and 2.86
  match fewer functions.
- 2.8.0-psx and 2.8.1-psx are still tied. A function that matches on only one of
  them settles the question: record it.
- `$gp` = `0x800A5EE4`. Small data (8 bytes or less) is addressed `%gp_rel(sym)($gp)`.

## Globals: `$gp` or absolute

The declaration of a global decides how GCC addresses it:

| asm shows | declare as |
|---|---|
| `lw $v0, %gp_rel(g_anim_data)($gp)` | a scalar, pointer or small struct: `extern s32 g_anim_data;` |
| `lui`/`%hi(g_anim_mask_buf)` + `%lo(...)` | an array or a struct over 8 bytes: `extern u8 g_anim_mask_buf[];` |

Accesses like `%lo(D_800B0000+0x10)` are struct fields or array elements. Use the
base symbol with an offset; do not invent a new `D_` symbol for the field.

Use the exact symbol names from the asm. `check.sh` cannot tell whether the wrong
symbol is referenced; only `make` can.

Other addressing patterns:

- **Absolute access to a small global** (`lui $at,%hi(sym)` + `lw r,%lo(sym)($at)`
  where other functions use `$gp`): the original file declared it without a size.
  Use `extern s32 D_X NO_GP;` (`NO_GP` is in `common.h`).
- **Arrays indexed from 1:** `addiu r, sym, -SIZE` together with `(i+1)*SIZE` means
  `arr[i + 1]` on the real base (`sym - SIZE`, which usually exists in
  `config/undefined_syms_auto.txt`).
- **Address-only symbols:** a 2-instruction `la` that GCC will not put in a delay
  slot. Declare the symbol as a small scalar (`extern u32 D_X;`) and pass `&D_X`.
- **Mixed addressing in one file:** if one function addresses a symbol absolutely
  while others use `$gp`, they came from different original C files (each had its
  own declarations). The file splits at that point.

## .rodata, .sdata and .data

**`.rodata` is per file.** Each C file owns its slice of `.rodata` (the `.rodata`
subsegments in the config). A function that uses string literals, switch jump
tables or const tables writes them in C, and the compiler emits them into the
file's slice in function order. Rodata that no single function owns (shared
strings, tables referenced from data) comes in through
`INCLUDE_RODATA("asm/nonmatchings/game/<file>", D_xxxxxxxx);`, placed just before
the first function whose rodata follows it (`tools/place_rodata.py`). If a literal
in the C is also pulled in by `INCLUDE_RODATA`, delete the include. GCC merges
identical literals within a file, as the original did.

**`.sdata` emitted by the C is per file too.** When a function emits small data
(string literals of 8 bytes or less, local array initializers), the file gets a
`.sdata` slice in the config, sized to exactly what the C emits. Find it by
compiling the file and searching for its `.sdata` bytes in the retail EXE. The
rest of `.data`/`.sdata`/`.bss` (globals) is still asm. The linker reports a
missing slice as "`.sdata` referenced in section `.text` ... defined in discarded
section".

**Jump tables and alignment.** cc1 emits `.align 3` before jump tables, and it
stays: the alignment is relative to each object's `.rodata` start, and the linker
places objects 4-aligned (`SUBALIGN(4)`), as psylink did. A table that is not
8-aligned relative to its file's rodata start means the file is two original
translation units. This located the boundaries at 80033820, 80039734, 8003CB08,
8003D1B4 and 8003D6A8.

## Functions that do not match

When a function is functionally correct but a few instructions still differ, keep
the C for the native client and the asm for the build:

```c
#ifdef NON_MATCHING
/* 3 diffs: register swap in the inner loop */
void func_80012345(...) { ... }
#else
INCLUDE_ASM("asm/nonmatchings/game/80012300", func_80012345);
#endif
```

The comment states the diff count and what has been tried.

## Techniques

### Basics (GCC 2.8, `-O2`)

- **Statement order.** GCC keeps much of the source order: `a = x; b = y;` and
  `b = y; a = x;` can schedule differently.
- **Split or merge expressions.** `len = *p++; len += 3;` allocates registers
  differently from `len = *p++ + 3;` (`anim_decompress_mask`).
- **Pointer vs array.** `buf + i` (pointer first) versus `arr[i]` changes operand order.
- **Local temporaries.** Adding or removing a local like `u8 *d = &buf[i];` shifts
  scheduling.
- **Loop shapes.** `for`, `while` and `do {} while` produce different guards.
- **Types.** `s16` vs `s32` locals, signed vs unsigned (look for `sltu` vs `slt`,
  `lhu` vs `lh`, `andi 0xFF` truncations).
- **`addu $v0, $zero, $zero` and `move`** are equivalent; ignore them in fdiff.
- **Early `return` vs `break`** changes the block layout.
- **Prototype every called function.** Implicit declarations change argument handling.

### Control flow

- **Cross-jumping.** GCC merges identical tails and keeps the last copy. If the
  original has one shared store after branches that each end in a call, write the
  store in every branch. In large state machines, `goto` to shared blocks and a
  common `goto end; ... end: return 0;` exit reproduce merged tails.
- **Blocking jump threading.** An empty `do {} while (0);` between a label and a jump
  (often a compiled-out debug macro) stops GCC from threading it before reload, which
  changes which duplicate blocks merge (`menu_run`).
- **Inline functions defined before their callers** are inlined there, and GCC emits
  their out-of-line copy at the end of the file. Use this only when the original
  copy is last in the TU (`site_end_frame`/`site_draw_frame`). Otherwise use a
  separately named `static inline` helper.
- **Switch case values.** `check.sh` compares instructions only, so a switch with the
  right range but wrong case labels still shows 0 diffs. Only `make` sees the
  jump-table contents (`name_entry_press_cell` had its dakuten cases off by one).

### Registers and scheduling

- **Split temporaries** control register choice and scheduling:
  `z = ...; b = z * 4; ...; b = b - 7; idx = a + b;`
- **Stopping constant folding.** Assign a variable twice
  (`top = info & 7; ... top = top * 8 + C;`), or pass a constant through a variable
  (`tpX = 0x2C0`) so combine cannot fold it.
- **Operand order.** `a = x + a` gives `addu a,a,x`. Write into a new variable to get
  `x` first.
- **Association of sums.** `x - 236 + n * 236 / 8` and `x + (n * 236 / 8 - 236)`
  compile differently (`addu x, x, t` after `addiu t, -236`, vs `addiu` last). Try
  every association before blaming the scheduler.
- **Adjacent s16 globals may be a struct** (SVECTOR/DVECTOR): stores to struct members
  keep their order around pointer stores; scalar stores do not. `lui/addiu sym+4`
  followed by `-4($reg)` accesses point to the same object.
- **Signed compare on a u8.** Load into an `s32` temporary first (`slti` vs `sltiu`).
  For an s16 compared with a byte: `step >= (s16)table[i]`.
- **A constant kept in a register across a branch** (`li` in a delay slot, reused
  later) means the original had a local variable holding that constant.
- **Duplicate a constant in both branches** (`if ... x = 0x48; else x = 0x48;`) when
  the original hoists it into a saved register.
- **Inline-helper parameters.** A constant argument copied into a pseudo can be
  hoisted before a branch. Derive it from a small index parameter (`0xC9 + n`).
- **`volatile`** on globals shared with CD, SPU or VSync callbacks, or on on-stack
  loop counters, fixes store order and can decide whether cross-jumping happens
  (`cd_sync_callback`, `anim_decode_frame`).
- **Unused locals** (`s32 unused[2];`, `RECT rect_unused;`) reproduce the original
  frame size.

### Loops

- **Loop hoists.** `cc1 ... -dL` writes a `.loop` dump that explains every invariant
  hoist ("moved" / "not desirable"). Hoist order follows first use in the loop.
- **`move a1,t5` copying a hoisted base in the preheader** comes from loop strength
  reduction: write `array[len++] = x`, not `*out++ = x`.
- **cse knows branch conditions.** In the `else` of `if (i != 5)`,
  `g_jump_menu_sprite_pos[i]` compiles as the constant `&g_jump_menu_sprite_pos[5]`
  (`0x14(a3)` off the hoisted base). Constant offsets like that mean the original
  indexed with the loop variable, not a constant or a second pointer
  (`jump_menu_slide_in`).

### Reading compiler dumps

- **Assignment vs emission order.** `cc1 ... -dg` prints "Register dispositions". If
  every pseudo already lands in the target's register, the diff is emission order,
  not allocation: changing types or register pressure will not help, only statement
  or first-use order. `-dL` "moved to insn N" lines give the preheader hoist order: a
  lower insn number is hoisted, and emitted, first.
- **Scheduler "birthing" priority.** In the first scheduling pass, an insn that sets
  a variable assigned only once (and still live) gets top priority (`0x7f000001` in
  `-dS`) and lands last before the branch. If two independent computations come out
  swapped and source order does not help, check how often each variable is
  assigned; the original often reuses one variable for two purposes.
- **`sllv` where the original has `sll`.** A post-reload pass reused a register that
  held the constant, because a `li` was scheduled earlier. Read `-dR` (`.sched2`) to
  separate delay-slot filling from earlier passes.
- For large functions, a unified diff of `objdump` output is easier to read than
  `fdiff.sh`.

### Brute force

- **Permutation batches.** For a stubborn block of 4-6 statements, generate every
  order (or move one statement to every position) and score all variants in one
  container call. About 120 variants take a few minutes (`ending_movie_play`).
- **Last resort, marked `FAKE MATCH`:** `__asm__ volatile("");` as a scheduling
  barrier. Only with a comment; replace it when the real source shape is found.

## decomp-permuter

For a function stuck on register allocation or scheduling,
[decomp-permuter](https://github.com/simonlindholm/decomp-permuter) (in the image at
`/opt/permuter`) tries random source rewrites and keeps the ones that score better:

    tools/docker.sh tools/permute.sh <func> src/game/<file>.c
    tools/docker.sh env PERMUTE_TIME=45m PERMUTE_JOBS=4 tools/permute.sh <func> src/game/<file>.c

`tools/permute_import.py` sets up `build/permuter/<func>/`:

| File | Contents |
|---|---|
| `base.c` | the source file preprocessed with `-DPERMUTER -DNON_MATCHING -DNEEDS_RODATA -DNEEDS_SDATA`, pruned to what the function uses |
| `target.o` | the function's asm, assembled with `include/macro.inc` |
| `compile.sh` | calls `tools/cc.sh` |

`__attribute__` lines (`NO_GP`) are kept verbatim as `#pragma _permuter b64literal`
so the addressing matches. The run stops after `PERMUTE_TIME` (default 20m).
Improvements land in `build/permuter/<func>/output-<score>-<n>/` (`source.c` and
`diff.txt`). `PERMUTE_IMPORT=0` reuses an existing `base.c`, e.g. one with
hand-added `PERM_` macros to steer the search (see the permuter's README).

- The permuter weights register, reordering and insertion differences differently
  from `check.sh`, so a lower permuter score can mean more `check.sh` diffs.
  `permute.sh` ends by ranking the outputs by `check.sh` diffs
  (`tools/permute_import.py --rank <func> build/permuter/<func>` does it on demand).
- Only port changes that read as plausible source, then re-check with `check.sh`
  and the full `make`.
- It is slow here (cc1 runs under emulation): about 4 compiles/s with `-j4`, so a
  20-minute run covers ~5000 candidates, far fewer for functions of 1000+ instructions.
- It can emit code that behaves differently (reusing a variable before its last use,
  hoisting a store out of a conditional path). Check every candidate by hand.
- It finds statement orders and extra temporaries (`gate_scene_run`, the `k` in
  `movie_models_init`) and extra loop levels that fix an allocation
  (`movie_models_update`). It does not solve pure tie-breaks (hoist order,
  local-alloc priority, reorg slot choice); hand-written variant batches (every
  association of a sum, every statement order) do as well there.

## Progress report (decomp.dev)

`make report` writes `build/report.json`, an [objdiff](https://github.com/encounter/objdiff)
report with one unit per game code segment (the PsyQ SDK objects are left out):

    tools/docker.sh make report

- **target** objects (`build/objdiff/target/`): the original code of each unit. For a C
  file, `tools/objdiff_config.py` concatenates its functions from `asm/{non,}matchings`
  with its `.rodata`/`.sdata` slices.
- **base** objects (`build/objdiff/base/`): the C files compiled with `-DSKIP_ASM`, which
  turns `INCLUDE_ASM` into nothing, so `NON_MATCHING` functions count as unmatched.
- `objdiff.json` at the repo root lists the units and is regenerated by the same target.
- objdiff-cli is downloaded to `tools/bin/` by `tools/get_objdiff.sh` on first use.

The `game` totals match `tools/progress.py`. To publish, refresh the committed copy:

    cp build/report.json progress/SLPS_016.03_report.json

and commit it. On `main`, `.github/workflows/progress.yml` uploads it as the
`SLPS_016.03_report` artifact that decomp.dev reads. The report holds only names,
sizes and percentages, no code or data.

## Conventions

- Put a one-line comment above a function once its purpose is known. Renames go
  through `tools/rename.py` (see [Naming](DECOMPILATION.md#naming)).
- `extern` declarations and prototypes go at the top of the C file that uses them.
- Types: `s8 u8 s16 u16 s32 u32` from `include/types.h`.
