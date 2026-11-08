/*
 * apk.h
 *
 * Read-only access to the game's original .apk (a zip archive), standing in
 * for Android's AssetManager.
 *
 * This software may be modified and distributed under the terms
 * of the GPLv3 license. See the LICENSE file for details.
 */

#ifndef BLOONS2_APK_H
#define BLOONS2_APK_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Open the apk. Returns 0 on success. */
int apk_open(const char *path);

void apk_close(void);

/**
 * Load an asset into a newly malloc'ed buffer (caller frees).
 *
 * `name` is resolved the same way the game's Java side resolves it through
 * AssetManager: relative to `assets/` inside the apk. As fallbacks, an
 * `.../Contents/Resources/` prefix or the directory part is stripped, and
 * a plain file on the memory card with that exact path is accepted too.
 */
bool asset_load(const char *name, uint8_t **buf, size_t *size);

/**
 * Extract every apk entry under `prefix` (e.g. "assets/unpack/") into
 * `dest_dir`, skipping files that already exist with the right size.
 * Returns the number of files written, or -1 on error.
 */
int apk_extract_dir(const char *prefix, const char *dest_dir);

#ifdef __cplusplus
};
#endif

#endif // BLOONS2_APK_H
