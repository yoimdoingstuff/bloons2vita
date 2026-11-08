/*
 * achievements.h
 *
 * Maps the game's Game Center achievements (inherited from the iOS version,
 * com.digitalgoldfish.bloons2.*) to Vita trophies.
 *
 * This software may be modified and distributed under the terms
 * of the GPLv3 license. See the LICENSE file for details.
 */

#ifndef BLOONS2_ACHIEVEMENTS_H
#define BLOONS2_ACHIEVEMENTS_H

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Report achievement progress (0-100) as sent by the game. The matching
 * trophy is unlocked once progress reaches 100%.
 */
void achievement_report(const char *id, float percent);

#ifdef __cplusplus
};
#endif

#endif // BLOONS2_ACHIEVEMENTS_H
