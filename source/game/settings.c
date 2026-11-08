/*
 * settings.c
 *
 * Port settings, read from DATA_PATH "settings.txt" (created with defaults
 * and explanations on first launch). Simple "key = value" lines; anything
 * after '#' is a comment.
 *
 * This software may be modified and distributed under the terms
 * of the GPLv3 license. See the LICENSE file for details.
 */

#include "game/settings.h"

#include <ctype.h>
#include <malloc.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "utils/logger.h"
#include "utils/utils.h"

#define SETTINGS_PATH DATA_PATH "settings.txt"

Settings settings = {
    .aim_mode = AIM_MODE_PRECISE,
    .invert_aim = false,
    .aim_strength = 100,
};

static const char default_file[] =
    "# Bloons 2 for PS Vita - settings\n"
    "# Edit this file on your memory card, then restart the game.\n"
    "\n"
    "# How the analog stick aims a throw inside a level:\n"
    "#   0 = Classic: the stick is the pull, like Angry Birds. Tilt to aim,\n"
    "#       let go of the stick to keep the aim, press Cross to throw.\n"
    "#   1 = Precise: Classic, plus the d-pad fine-tunes the shot\n"
    "#       (left/right: angle, up/down: power) and your last aim is kept\n"
    "#       after every throw, so you can repeat or adjust a near miss.\n"
    "aim_mode = 1\n"
    "\n"
    "# 1 = flip the aiming direction (if the dart flies the opposite way to\n"
    "#     what you expect).\n"
    "invert_aim = 0\n"
    "\n"
    "# How far a fully tilted stick pulls, in percent (50 - 200). Raise it if\n"
    "# you can't reach full power, lower it for finer control.\n"
    "aim_strength = 100\n";

static int clampi(int v, int lo, int hi) {
    return v < lo ? lo : (v > hi ? hi : v);
}

static char *trim(char *s) {
    while (isspace((unsigned char)*s)) s++;
    char *e = s + strlen(s);
    while (e > s && isspace((unsigned char)e[-1])) *--e = '\0';
    return s;
}

void settings_load(void) {
    if (!file_exists(SETTINGS_PATH)) {
        file_save(SETTINGS_PATH, (const uint8_t *)default_file, sizeof(default_file) - 1);
        l_info("settings: wrote defaults to %s", SETTINGS_PATH);
        return;
    }

    uint8_t *buf;
    size_t size;
    if (!file_load(SETTINGS_PATH, &buf, &size))
        return;

    char *data = realloc(buf, size + 1);
    data[size] = '\0';

    char *saveptr = NULL;
    for (char *line = strtok_r(data, "\n", &saveptr); line;
         line = strtok_r(NULL, "\n", &saveptr)) {
        char *hash = strchr(line, '#');
        if (hash) *hash = '\0';
        char *eq = strchr(line, '=');
        if (!eq) continue;
        *eq = '\0';
        char *key = trim(line);
        int value = atoi(trim(eq + 1));

        if (strcmp(key, "aim_mode") == 0)
            settings.aim_mode = value == 0 ? AIM_MODE_CLASSIC : AIM_MODE_PRECISE;
        else if (strcmp(key, "invert_aim") == 0)
            settings.invert_aim = value != 0;
        else if (strcmp(key, "aim_strength") == 0)
            settings.aim_strength = clampi(value, 50, 200);
    }

    free(data);
    l_success("settings: aim_mode=%i invert_aim=%i aim_strength=%i",
              settings.aim_mode, settings.invert_aim, settings.aim_strength);
}
