#include "psx/libetc.h"

#include "../PsyX_main.h"
#include "PsyX/PsyX_public.h"

#include <SDL_timer.h>
#include <SDL.h> /* lain: performance counter */

#ifdef DEBUG
char scratchData[4096 + 8];
char* _scratchData = scratchData + 4;
#else
char scratchData[4096];
char* _scratchData = scratchData;
#endif

void(*vsync_callback)(void) = NULL;

int StopCallback(void)
{
	PSYX_UNIMPLEMENTED();
	return 0;
}

int ResetCallback(void)
{
	int old = (int)vsync_callback;
	vsync_callback = NULL;
	return old;
}

// lain: PsyQ semantics (NTSC timing from PsyX_main.cpp's vblank thread):
//   VSync(-1)  vblank count since boot, no wait
//   VSync(1)   horizontal lines since the last vblank, no wait
//   VSync(0)   wait for the next vblank
//   VSync(n>1) wait until n vblanks have passed since the previous VSync() wait
// The waits return the line count too. Lain's menus and players pace their
// loops with VSync(2) or VSync(5).

static int VSync_HCount(void)
{
	const Uint64 freq = SDL_GetPerformanceFrequency();
	const Uint64 now = SDL_GetPerformanceCounter();
	const Uint64 last = g_lain_lastVBlankTicks;
	const double lines = PsyX_Sys_VBlankRate() > 55.0 ? 262.5 : 312.5; // lines per field
	if (now <= last)
		return 0;
	return (int)((double)(now - last) / (double)freq * PsyX_Sys_VBlankRate() * lines) & 0xFFFF;
}

#ifdef __cplusplus
extern "C"
#endif
int LainInterp_Present(float t); // lain: PsyX_GPU.cpp

int VSync(int mode)
{
	static int lastWait = -1;

	if (mode < 0)
		return PsyX_Sys_GetVBlankCount();
	if (mode == 1)
		return VSync_HCount();

	if (lastWait < 0)
		lastWait = PsyX_Sys_GetVBlankCount();

	const int target = lastWait + (mode > 1 ? mode : 1);
	int count = PsyX_Sys_GetVBlankCount();
	if (mode == 0 && count >= target)
	{
		// VSync(0) always waits for the next vblank
		const int next = count + 1;
		while (!g_skipSwapInterval && PsyX_Sys_GetVBlankCount() < next)
			SDL_Delay(0);
	}
	else
	{
		// lain: a wait of several vblanks (a 12/30 fps screen) can show
		// in-between frames at each vblank (frame interpolation, PsyX_GPU.cpp).
		// A vblank that passed while the game worked still gets its frame.
		const int start = lastWait;
		int shown = start;
		while (!g_skipSwapInterval && (count = PsyX_Sys_GetVBlankCount()) < target)
		{
			if (count != shown && mode > 1)
			{
				shown = count;
				LainInterp_Present((float)(count - start) / (float)(target - start));
			}
			SDL_Delay(0);
		}
	}
	lastWait = PsyX_Sys_GetVBlankCount();
	return VSync_HCount();
}

int VSyncCallback(void(*f)(void))
{
	int old = (int)vsync_callback;
	vsync_callback = f;
	return old;
}

int SetVideoMode(int mode)
{
#ifdef DEBUG
	// debug marks for overflow cheks
	*(uint*)&scratchData[0] = 0xdeadb33f;
	*(uint*)&scratchData[4096 + 4] = 0xdeadb33f;
#endif
	
	return PsyX_Sys_SetVMode(mode);
}

int GetVideoMode()
{
	return g_vmode;
}

void PadInit(int mode)
{
	PSYX_UNIMPLEMENTED();

	// TODO: call PadInitDirect
}

u_int PadRead(int id)
{
	PSYX_UNIMPLEMENTED();
	
	// TODO: return pad data as u_int
	return 0;
}

void PadStop(void)
{
	PSYX_UNIMPLEMENTED();

	// TODO: stop pad reads
}