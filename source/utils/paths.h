/*
 * paths.h
 *
 * Location of the game data on the Vita. The port accepts its files in
 * either ux0:data/bloons2/ or ur0:data/bloons2/ and picks the folder that
 * contains libbloons2.so at startup.
 *
 * This software may be modified and distributed under the terms
 * of the GPLv3 license. See the LICENSE file for details.
 */

#ifndef BLOONS2_PATHS_H
#define BLOONS2_PATHS_H

#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Data folder with a trailing '/', e.g. "ur0:data/bloons2/". */
extern char data_path[64];
extern char so_path[96];
extern char apk_path[96];

/**
 * Pick the data folder. Call first thing at startup.
 * Returns false if libbloons2.so wasn't found in any candidate folder
 * (data_path then points at the first candidate, for error messages).
 */
bool paths_init(void);

/** out = data_path + rel. Returns out. */
char *data_file(char *out, size_t out_size, const char *rel);

/** Human-readable list of the folders that are searched. */
const char *paths_searched(void);

#ifdef __cplusplus
};
#endif

#endif // BLOONS2_PATHS_H
