/* Hookable game functions (port/tools/gen_hooks.py): every game function can be
 * replaced at run time by mod scripts and plugins. */
#ifndef GAME_HOOKS_H
#define GAME_HOOKS_H

#include <stdint.h>

typedef struct {
    const char *name;
    const char *sig;    /* C signature, e.g. "s32 (u8 *, s32)" */
    const char *kinds;  /* result then arguments: v void, p pointer, i signed, u unsigned */
    int nargs;
    void **hook;        /* the replacement the game calls; NULL = the original */
    void *entry;        /* what the game calls: the replacement if set, else the original */
    void *orig;         /* the original function */
    void *adapter;      /* a replacement that passes the call to lain_game_dispatch_fn */
    /* calls fn (any function with this signature) with integer arguments */
    int64_t (*call)(void *fn, const int64_t *args);
} LainGameFn;

extern const LainGameFn lain_game_fns[];
extern const int lain_game_fn_count;

/* Game variables by name (config/symbol_addrs.txt). Most live in the PS1 RAM
 * arena at their PS1 address; host globals (pointer tables, game/PORT_TYPES.md)
 * are native variables at `host` instead. */
typedef struct {
    const char *name;
    uint32_t addr;      /* PS1 address */
    uint32_t size;      /* bytes on the PS1, 0 if unknown */
    void *host;         /* the native variable for host globals, else NULL */
} LainSymbol;

extern const LainSymbol lain_symbols[];
extern const int lain_symbol_count;

/* Where adapter replacements send calls: the function's index in lain_game_fns
 * and its arguments as integers (pointers as addresses). Returns the result. */
extern int64_t (*lain_game_dispatch_fn)(int index, const int64_t *args, int nargs);

#endif
