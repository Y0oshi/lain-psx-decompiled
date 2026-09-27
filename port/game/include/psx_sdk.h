/* The PlayStation SDK as seen by the ported game code: PsyCross (port/psx) headers.
 *
 * port: the game files have no private copies of the PsyQ types and
 * prototypes, so every structure the game hands to the SDK has the host
 * layout the SDK expects.
 */
#ifndef PSX_SDK_H
#define PSX_SDK_H

#include <assert.h>

#include "libgte.h"
#include "libgpu.h"
#include "libgs.h"
#include "libetc.h"
#include "libapi.h"
#include "libcd.h"
#include "libpress.h"
#include "libspu.h"
#include "libsnd.h"
#include "libpad.h"
#include "libmcrd.h"
#include "libsn.h"

/* PsyCross names the VRAM rectangle RECT16 (RECT clashes with Windows). */
typedef RECT16 RECT;

#endif /* PSX_SDK_H */
