/*
 * paths.c
 *
 * Location of the game data on the Vita. The port accepts its files in
 * either ux0:data/bloons2/ or ur0:data/bloons2/ and picks the folder that
 * contains libbloons2.so at startup.
 *
 * This software may be modified and distributed under the terms
 * of the GPLv3 license. See the LICENSE file for details.
 */

#include "utils/paths.h"

#include <stdio.h>
#include <string.h>

#include <psp2/io/stat.h>

char data_path[64];
char so_path[96];
char apk_path[96];

// DATA_PATH (CMake option, default ux0:data/bloons2/) is checked first.
static const char *candidates[] = {
    DATA_PATH,
    "ux0:data/bloons2/",
    "ur0:data/bloons2/",
};

static bool exists(const char *path) {
    SceIoStat st;
    return sceIoGetstat(path, &st) >= 0;
}

static void use(const char *dir) {
    snprintf(data_path, sizeof(data_path), "%s", dir);
    snprintf(so_path, sizeof(so_path), "%slibbloons2.so", dir);
    snprintf(apk_path, sizeof(apk_path), "%sgame.apk", dir);
}

bool paths_init(void) {
    for (size_t i = 0; i < sizeof(candidates) / sizeof(candidates[0]); i++) {
        char so[96];
        snprintf(so, sizeof(so), "%slibbloons2.so", candidates[i]);
        if (exists(so)) {
            use(candidates[i]);
            return true;
        }
    }

    // Not installed yet. Prefer a folder that at least exists, so the error
    // message (and the log file) point somewhere sensible.
    for (size_t i = 0; i < sizeof(candidates) / sizeof(candidates[0]); i++) {
        char dir[64];
        snprintf(dir, sizeof(dir), "%s", candidates[i]);
        size_t l = strlen(dir);
        if (l && dir[l - 1] == '/') dir[l - 1] = '\0';
        if (exists(dir)) {
            use(candidates[i]);
            return false;
        }
    }

    use(candidates[0]);
    return false;
}

char *data_file(char *out, size_t out_size, const char *rel) {
    snprintf(out, out_size, "%s%s", data_path, rel);
    return out;
}

const char *paths_searched(void) {
    return "ux0:data/bloons2/ or ur0:data/bloons2/";
}
