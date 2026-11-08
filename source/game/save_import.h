/*
 * save_import.h
 *
 * One-time import of the player's own Android save data (progress and any
 * purchases their copy of the game recorded) into the port.
 *
 * This software may be modified and distributed under the terms
 * of the GPLv3 license. See the LICENSE file for details.
 */

#ifndef BLOONS2_SAVE_IMPORT_H
#define BLOONS2_SAVE_IMPORT_H

#ifdef __cplusplus
extern "C" {
#endif

/**
 * If DATA_PATH "import/" exists, import its contents and rename the folder
 * to "import_done". Call after prefs_load() and before the game starts.
 */
void save_import_run(void);

#ifdef __cplusplus
};
#endif

#endif // BLOONS2_SAVE_IMPORT_H
