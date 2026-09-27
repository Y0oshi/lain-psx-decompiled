/* Host implementations of the BIOS services the game calls directly
 * (on the PS1 these are A-table/syscall stubs; see config/symbol_addrs.txt). */
#include "lain_xa.h"

#include <stdlib.h>

/* Named Psx* on the host (see game/include/common.h): Windows' kernel32 has its
 * own EnterCriticalSection/ExitCriticalSection. */

/* The game brackets its CD request queue with critical sections so the CD
 * interrupt callbacks can't run in the middle. On the host those callbacks come
 * from the drive thread under its "IRQ" lock, so a critical section takes it. */
int PsxEnterCriticalSection(void) {
    LainCD_EnterIRQ();
    return 1;
}

void PsxExitCriticalSection(void) {
    LainCD_ExitIRQ();
}

/* BIOS heap and RAM-size setup: the host has real memory, nothing to do. */
void InitHeap(unsigned long *head, unsigned long size) {
    (void)head;
    (void)size;
}

void SetMemSize(int megabytes) {
    (void)megabytes;
}

/* The PS1's 1 KB data-cache scratchpad (0x1F800000). The game only uses it as
 * GsSortObject4's packet work area, which the port's libgs ignores. */
#include "port_hw.h"

u32 *port_scratchpad(void) {
    static u32 scratchpad[1024 / 4] __attribute__((aligned(16)));
    return scratchpad;
}

/* How long the PS1 takes for one node-map frame. Most of the frame is the software
 * decode of Lain's animation frame, which grows with the frame's compressed size
 * (anim_decode_frame leaves it in g_anim_last_frame_size), so her big emote frames
 * run slower than her standing pose. Fitted to a recording of the node map idling
 * on hardware-accurate emulation (Mednafen): frames there take 4 vblanks 3.5% of
 * the time, 5 87%, 6 7%, 7-9 2.5% (mean 5.10); this model gives 5.12. */
s32 port_site_frame_vblanks(void) {
    extern s32 g_anim_last_frame_size;
    static s32 fixed = -2;
    if (fixed == -2) {
        const char *env = getenv("LAIN_SITE_VBLANKS"); /* tests: a fixed value, 0 = unpaced */
        fixed = env ? atoi(env) : -1;
    }
    if (fixed >= 0) {
        return fixed;
    }
    s32 size = g_anim_last_frame_size;
    if (size <= 0) {
        return 5;
    }
    /* ceil(-0.336 + size / 2602) in fixed point, clamped to the observed 4..8 */
    s32 n = (size * 1000 / 2602 - 336 + 999) / 1000;
    return n < 4 ? 4 : n > 8 ? 8 : n;
}
