/* Host (PC) version of the decompilation's common.h.
 * Every game function has C, so INCLUDE_ASM must never be reached. */
#ifndef COMMON_H
#define COMMON_H

/* port: the game's BIOS critical sections get host names, because Windows'
 * kernel32 exports EnterCriticalSection/ExitCriticalSection with a different
 * meaning (see port/src/platform.c). Defined before the SDK headers so their
 * prototypes are renamed too. */
#define EnterCriticalSection PsxEnterCriticalSection
#define ExitCriticalSection PsxExitCriticalSection

#include "types.h"
#include "psx_sdk.h" /* port: PsyQ types and prototypes from port/psx */

#define INCLUDE_ASM(FOLDER, NAME) _Static_assert(0, "missing C for " #NAME)
#define INCLUDE_RODATA(FOLDER, NAME) /* rodata comes from the player's EXE (see data loader) */

/* $gp vs absolute addressing is a PS1 codegen detail; irrelevant on the host. */
#define NO_GP

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "game_protos.h"

/* port: PsyQ's rand() returns 0-0x7FFF; the game depends on it (port_libc.c). */
int port_rand(void);
void port_srand(unsigned int seed);
#define rand port_rand
#define srand port_srand
/* port: debug switch, see port_libc.c */
int port_unlock_all(void);

#endif /* COMMON_H */
