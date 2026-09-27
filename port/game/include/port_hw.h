/* Host stand-ins for PS1 hardware the game code touched directly.
 *
 * Declarations only: each helper replaces a hard-coded hardware address in the
 * game C (see port/game/PORT_TYPES.md, "Hardware access"). Implementations belong
 * to the platform layer, not to port/game.
 */
#ifndef PORT_HW_H
#define PORT_HW_H

#include "types.h"

/* The 1 KB scratchpad (D-cache RAM) at 0x1F800000. The game only hands it to
 * libgs as GsSortObject4's packet work area.
 * TODO(platform): return a static, 8-byte aligned buffer (>= 1 KB; PsyCross may
 * need more for host-sized packets). */
u32 *port_scratchpad(void);

/* Vblanks per node-map frame, as on the PS1. The node map is CPU-bound there:
 * Lain's animation is VLC-decoded in software every frame, so a frame takes
 * 4-8 vblanks depending on the size of the animation frame (her emotes are
 * bigger, so they play slower). The host does that work instantly, so the frame
 * end waits VSync(n) with this value instead of VSync(0). LAIN_SITE_VBLANKS=N
 * fixes it to N (0 = unpaced). */
s32 port_site_frame_vblanks(void);

#endif /* PORT_HW_H */
