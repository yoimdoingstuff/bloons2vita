/*
 * context.c
 *
 * Tracks which kind of screen the game is showing (inside a level or in a
 * menu) from the game's own log output, so button controls only act where
 * they make sense.
 *
 * The game's C++ screen classes log when they become active, e.g.
 * "finished CGameScreen::SetActive in %fs" (entering a level) or
 * "virtual void CLevelSelectScreen::SetActive(void *)", and the pause screen
 * logs "Pause Screen Inactive" when it closes. All of that reaches us through
 * our __android_log_* / printf replacements.
 *
 * If the game never logs any of this, the context simply stays UNKNOWN and
 * every control is allowed.
 *
 * This software may be modified and distributed under the terms
 * of the GPLv3 license. See the LICENSE file for details.
 */

#include "game/context.h"

#include <string.h>

#include "utils/logger.h"

static volatile GameContext context = CONTEXT_UNKNOWN;
static volatile bool paused = false;

static void set_context(GameContext c, const char *why) {
    if (context != c) {
        context = c;
        l_info("context: %s (%s)", c == CONTEXT_LEVEL ? "level" : "menu", why);
    }
}

void context_feed(const char *line) {
    if (!line) return;

    if (strstr(line, "CPauseScreen")) {
        if (strstr(line, "SetActive")) paused = true;
        return;
    }

    if (strstr(line, "Pause Screen Inactive")) {
        paused = false;
        return;
    }

    if (strstr(line, "CGameScreen::SetActive")) {
        paused = false;
        set_context(CONTEXT_LEVEL, "CGameScreen");
        return;
    }

    if (strstr(line, "FIRING DART") || strstr(line, "Firing boomerang")) {
        set_context(CONTEXT_LEVEL, "dart fired");
        return;
    }

    // Any other screen becoming active: a menu (main menu, zone/level
    // select, level complete, store, help...)
    const char *sa = strstr(line, "Screen::SetActive");
    if (sa) {
        paused = false;
        set_context(CONTEXT_MENU, line);
    }
}

void context_back_pressed(void) {
    if (context == CONTEXT_LEVEL)
        paused = !paused; // Back opens the pause menu, and closes it again
}

GameContext context_get(void) {
    return context;
}

bool context_paused(void) {
    return paused;
}
