/*
 * settings.h
 *
 * Port settings, read from DATA_PATH "settings.txt" (created with defaults
 * and explanations on first launch).
 *
 * This software may be modified and distributed under the terms
 * of the GPLv3 license. See the LICENSE file for details.
 */

#ifndef BLOONS2_SETTINGS_H
#define BLOONS2_SETTINGS_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    AIM_MODE_CLASSIC = 0, // the stick is the pull (Angry Birds style)
    AIM_MODE_PRECISE = 1, // classic + d-pad fine tuning, aim remembered
} AimMode;

typedef struct {
    AimMode aim_mode;
    bool invert_aim;        // flip the pull direction
    int aim_strength;       // 50..200 (%): how far a full stick tilt pulls
} Settings;

extern Settings settings;

void settings_load(void);

#ifdef __cplusplus
};
#endif

#endif // BLOONS2_SETTINGS_H
