/* PlayStation SDK functions the game calls that PsyCross (port/psx) doesn't provide. */
#include <stddef.h>

#include <assert.h> /* libgpu.h uses static_assert */

#include "libgte.h"
#include "libgpu.h"

/* libgpu: TIM parsing (OpenTIM / ReadTIM)
 * A TIM is: ID word 0x10, flags word, [CLUT block], pixel block. Each block is
 * a length word (bytes, including itself), x/y, w/h, then the data. ReadTIM
 * fills in pointers into the caller's (host) TIM data and moves on to the
 * next TIM in memory, as libgpu does. */
static u_int *tim_cursor;

int OpenTIM(u_int *addr) {
    tim_cursor = addr;
    return 0;
}

TIM_IMAGE *ReadTIM(TIM_IMAGE *timimg) {
    u_int *p = tim_cursor;

    if (p == NULL || (p[0] & 0xFF) != 0x10) {
        return NULL;
    }
    timimg->mode = p[1];
    p += 2;
    if (timimg->mode & 8) {
        timimg->crect = (RECT16 *)&p[1];
        timimg->caddr = &p[3];
        p += p[0] / 4;
    } else {
        timimg->crect = NULL;
        timimg->caddr = NULL;
    }
    timimg->prect = (RECT16 *)&p[1];
    timimg->paddr = &p[3];
    p += p[0] / 4;
    tim_cursor = p;
    return timimg;
}
