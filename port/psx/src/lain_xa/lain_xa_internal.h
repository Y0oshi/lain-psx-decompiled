/* lain: hooks between PsyCross' LIBCD.C / LIBSPU.C and the lain_xa drive + XA output. */
#ifndef LAIN_XA_INTERNAL_H
#define LAIN_XA_INTERNAL_H

#include "psx/types.h"
#include "psx/libcd.h"
#include "psx/libspu.h"

#if defined(__cplusplus)
extern "C" {
#endif

/* LIBCD.C */
#define LAINCD_CTL_F    0   /* CdControlF: issue, don't wait */
#define LAINCD_CTL      1   /* CdControl: issue, wait for acknowledge */
#define LAINCD_CTL_B    2   /* CdControlB: issue, wait for completion */

int   LainCD_Control(u_char com, u_char *param, u_char *result, int kind);
int   LainCD_Sync(int mode, u_char *result);
CdlCB LainCD_SetReadyCallback(CdlCB cb);
CdlCB LainCD_SetSyncCallback(CdlCB cb);
void *LainCD_SetDataCallback(void (*cb)(void));
int   LainCD_GetSector(void *madr, int words);
int   LainCD_LastCom(void);
void  LainCD_Reset(void);

/* Provided by LIBCD.C: where the PsyCross disc image is. */
int PsyX_CD_GetImageInfo(const char **fileName, const u_char **mem, int *memSize, int *sectorSize);

#if defined(__cplusplus)
}
#endif

#endif
