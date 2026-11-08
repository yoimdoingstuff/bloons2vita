/*
 * context.h
 *
 * Tracks which kind of screen the game is showing (inside a level or in a
 * menu) from the game's own log output, so button controls only act where
 * they make sense.
 *
 * This software may be modified and distributed under the terms
 * of the GPLv3 license. See the LICENSE file for details.
 */

#ifndef BLOONS2_CONTEXT_H
#define BLOONS2_CONTEXT_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    CONTEXT_UNKNOWN = 0, // no information yet: allow everything
    CONTEXT_MENU,
    CONTEXT_LEVEL,
} GameContext;

/** Feed one line of game log output (any thread). */
void context_feed(const char *line);

/** Note that we asked the game to go "back" (opens the pause menu in a level). */
void context_back_pressed(void);

GameContext context_get(void);

/** True while the pause screen is (probably) open over a level. */
bool context_paused(void);

#ifdef __cplusplus
};
#endif

#endif // BLOONS2_CONTEXT_H
