#ifndef COMMON_H
#define COMMON_H

#include "types.h"

/*
 * INCLUDE_ASM for maspsx with non-zero -G: cc1 can move top-level __asm__
 * blocks after data, so each block is wrapped in a dummy function that
 * maspsx unwraps (see the maspsx README, "INCLUDE_ASM reordering workaround").
 */
#if !defined(M2CTX) && !defined(PERMUTER)
#if defined(SKIP_ASM)
/* objdiff base objects (make objdiff): the C only, no original functions. */
#define INCLUDE_ASM(FOLDER, NAME)
#else
#define INCLUDE_ASM(FOLDER, NAME)                                  \
    void __maspsx_include_asm_hack_##NAME(void) {                  \
        __asm__(".text # maspsx-keep\n"                            \
                "\t.align\t2 # maspsx-keep\n"                      \
                "\t.set noreorder # maspsx-keep\n"                 \
                "\t.set noat # maspsx-keep\n"                      \
                ".include \"" FOLDER "/" #NAME ".s\" # maspsx-keep\n" \
                "\t.set reorder # maspsx-keep\n"                   \
                "\t.set at # maspsx-keep\n");                      \
    }
#endif
/* Unmigrated .rodata (orphan strings/tables), placed in address order in the file. */
#define INCLUDE_RODATA(FOLDER, NAME)                               \
    void __maspsx_include_asm_hack_rodata_##NAME(void) {           \
        __asm__(".section .rodata # maspsx-keep\n"                 \
                ".include \"" FOLDER "/" #NAME ".s\" # maspsx-keep\n" \
                ".section .text # maspsx-keep\n");                 \
    }
#else
#define INCLUDE_ASM(FOLDER, NAME)
#define INCLUDE_RODATA(FOLDER, NAME)
#endif

/*
 * NO_GP: declare a small global that this translation unit addressed absolutely
 * (lui/%lo) rather than via $gp. The original TU declared it without a size, so
 * ASPSX didn't use $gp; putting the extern in .data has the same effect: cc1 still
 * treats it as small but stops emitting the `.extern sym,size` hint.
 *     extern s16 g_current_site NO_GP;
 */
#ifndef NO_GP
#define NO_GP __attribute__((section(".data")))
#endif

#endif /* COMMON_H */
