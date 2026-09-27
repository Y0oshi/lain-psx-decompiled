#ifndef LIBPRESS_H
#define LIBPRESS_H

#include "types.h" /* lain: u_int etc. */

#if defined(_LANGUAGE_C_PLUS_PLUS)||defined(__cplusplus)||defined(c_plusplus)
extern "C" {
#endif

/* lain: DecDCT* are implemented in src/lain_press/lain_mdec.c (software MDEC).
 * DecDCTin mode: bit 0 = 24bpp output (else 15bpp), bit 1 = set STP (bit 15).
 * DecDCTvlc / DecDCTBufSize (Sony STR v2 bitstreams) are not implemented:
 * Lain decodes its own bitstream format (func_8007038C). */
extern void DecDCTReset(int mode);
extern int DecDCTBufSize(u_int* bs);
extern void DecDCTvlc(u_int* bs, u_int*buf);
extern void DecDCTin(u_int* buf, int mode);
extern void DecDCTout(u_int* buf, int size);
extern int DecDCTinSync( int mode) ;
extern int DecDCToutSync( int mode) ;
extern void DecDCTinCallback(void (*func)(void));  /* lain */
extern void DecDCToutCallback(void (*func)(void)); /* lain */

/* lain: EncSPU is declared in libspu.h (implemented in src/lain_snd/encspu.c). */

#if defined(_LANGUAGE_C_PLUS_PLUS)||defined(__cplusplus)||defined(c_plusplus)
}
#endif

	
#endif
