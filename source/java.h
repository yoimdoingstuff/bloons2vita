/*
 * java.h
 *
 * Bits of the fake Java side that the main loop needs to talk to.
 *
 * This software may be modified and distributed under the terms
 * of the GPLv3 license. See the LICENSE file for details.
 */

#ifndef BLOONS2_JAVA_H
#define BLOONS2_JAVA_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Frame rate requested by the game through cocojava.setFPS(). */
extern volatile float java_target_fps;

/**
 * Service message box / keyboard requests made by the game. Call once per
 * frame on the game thread after Mrender().
 *
 * @return true while a system dialog is on screen.
 */
bool java_ui_update(void);

#ifdef __cplusplus
};
#endif

#endif // BLOONS2_JAVA_H
