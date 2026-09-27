/* The PS1's 2 MB of main RAM, recreated on the host (port/client/psx_mem.c).
 *
 * The original EXE's text/rodata/data are copied into it from the player's
 * disc at startup, and each game global (D_xxxxxxxx) is a label at its original
 * offset (port/game/generated/psx_arena.S), so layout-dependent code keeps working.
 */
#ifndef PSX_ARENA_H
#define PSX_ARENA_H

#include <stdint.h>

#define PSX_RAM_BASE 0x80000000u
#define PSX_RAM_SIZE 0x00200000u

/* Host-only extension of the arena, right after PS1 RAM, that holds the game's
 * heap (heap_init). Host heap blocks are bigger than the PS1's (24-byte
 * headers, host-sized structs), so the original 0xD4000-byte heap at
 * g_heap_area could run out; this one is twice as big. It stays inside the
 * arena so heap data (TMDs, TIMs) keeps 32-bit "PS1" addresses
 * (GsSetMapBase, PSX_PTR). */
#define PSX_HEAP_ADDR 0x80200000u
#define PSX_HEAP_SIZE 0x00200000u
#define PSX_ARENA_SIZE (PSX_RAM_SIZE + PSX_HEAP_SIZE)

extern uint8_t psx_arena[];

/* PS1 address -> host pointer, and back (only valid for addresses inside the arena). */
#define PSX_PTR(addr) ((void *)(psx_arena + ((uint32_t)(addr) - PSX_RAM_BASE)))
#define PSX_ADDR(ptr) ((uint32_t)((uint8_t *)(ptr) - psx_arena) + PSX_RAM_BASE)

#endif
