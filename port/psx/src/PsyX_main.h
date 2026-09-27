#ifndef EMULATOR_SETUP_H
#define EMULATOR_SETUP_H

#include "platform.h"

//Disc image filename to load for disc image builds
#define DISC_CUE_FILENAME "IMAGE.CUE"

#if defined(_LANGUAGE_C_PLUS_PLUS)||defined(__cplusplus)||defined(c_plusplus)
extern "C" {
#endif

extern unsigned int g_swapTime;
extern int g_vmode;
extern int g_activeKeyboardControllers;

extern int PsyX_Sys_GetVBlankCount();
extern int PsyX_Sys_SetVMode(int mode);

/* lain: vblank timing for VSync() (LIBETC.C) */
extern double PsyX_Sys_VBlankRate(void);
extern volatile unsigned long long g_lain_lastVBlankTicks;
extern int g_skipSwapInterval;

#if defined(_LANGUAGE_C_PLUS_PLUS)||defined(__cplusplus)||defined(c_plusplus)
}
#endif

#endif