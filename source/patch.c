/*
 * patch.c
 *
 * Patching some of the .so internal functions or bridging them to native
 * for better compatibility.
 *
 * libbloons2.so is built for ARMv6+ (it uses LDREX/STREX for atomics) with
 * the soft-float ABI, contains no kernel-user helper calls (0xFFFF0FC0 &
 * co.) and no VFP code, so unlike e.g. Modern Combat 3 it needs no
 * instruction patching to run. This is the place to add hooks
 * (hook_addr(so_mod.text_base + offset, &replacement)) should any game
 * function need to be replaced later.
 *
 * This software may be modified and distributed under the terms
 * of the GPLv3 license. See the LICENSE file for details.
 */

#include "patch.h"

#include <so_util/so_util.h>

#include "utils/logger.h"

extern so_module so_mod;

void so_patch(void) {
    // Nothing to patch for v1.0.0.
}
