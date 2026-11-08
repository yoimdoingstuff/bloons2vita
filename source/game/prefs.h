/*
 * prefs.h
 *
 * Minimal persistent key/value store standing in for the game's
 * SharedPreferences("GAME_INFO").
 *
 * This software may be modified and distributed under the terms
 * of the GPLv3 license. See the LICENSE file for details.
 */

#ifndef BLOONS2_PREFS_H
#define BLOONS2_PREFS_H

#ifdef __cplusplus
extern "C" {
#endif

void prefs_load(const char *path);

int prefs_get_int(const char *key, int def);
void prefs_set_int(const char *key, int value);

/** Returned pointer stays valid until the key is overwritten. */
const char *prefs_get_string(const char *key, const char *def);
void prefs_set_string(const char *key, const char *value);

#ifdef __cplusplus
};
#endif

#endif // BLOONS2_PREFS_H
