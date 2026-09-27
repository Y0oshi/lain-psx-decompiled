/* First-run / settings window: disc import, display and language options. */
#pragma once
#include "settings.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Shows the setup window until the player presses Play (returns 1) or quits (0).
 * Creates and destroys its own SDL window; SDL video must be initialised. */
int setup_ui_run(Settings *settings);

#ifdef __cplusplus
}
#endif
