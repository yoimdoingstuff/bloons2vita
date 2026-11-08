/*
 * Copyright (C) 2023      Rinnegatamante
 * Copyright (C) 2023-2024 Volodymyr Atamanenko
 *
 * This software may be modified and distributed under the terms
 * of the MIT license. See the LICENSE file for details.
 */

/**
 * @file  trophies.h
 * @brief Trophy support through sceNpTrophy. Requires the NoTrpDrm plugin
 *        (https://github.com/Rinnegatamante/NoTrpDrm) on the console.
 */

#ifndef SOLOADER_TROPHIES_H
#define SOLOADER_TROPHIES_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Must be called after vitaGL is initialized (shows the system trophy setup
 * dialog). Returns < 0 if trophies are unavailable (e.g. no NoTrpDrm).
 */
int trophies_init();

bool trophies_is_unlocked(uint32_t id);

/** Non-blocking; does nothing if the trophy is already unlocked. */
void trophies_unlock(uint32_t id);

#ifdef __cplusplus
};
#endif

#endif // SOLOADER_TROPHIES_H
