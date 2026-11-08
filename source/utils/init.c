/*
 * Copyright (C) 2021      Andy Nguyen
 * Copyright (C) 2021-2022 Rinnegatamante
 * Copyright (C) 2022-2024 Volodymyr Atamanenko
 *
 * This software may be modified and distributed under the terms
 * of the MIT license. See the LICENSE file for details.
 */

#include "utils/init.h"

#include "utils/dialog.h"
#include "utils/glutil.h"
#include "utils/logger.h"
#include "utils/trophies.h"
#include "utils/utils.h"

#include <string.h>
#include <stdlib.h>

#include <psp2/appmgr.h>
#include <psp2/apputil.h>
#include <psp2/kernel/clib.h>
#include <psp2/power.h>

#include <falso_jni/FalsoJNI.h>
#include <so_util/so_util.h>
#include <kubridge.h>

#include "game/apk.h"
#include "game/audio.h"
#include "game/coco.h"
#include "game/prefs.h"
#include "game/save_import.h"
#include "game/settings.h"
#include "patch.h"

// Base address for the Android .so to be loaded at
#define LOAD_ADDRESS 0x98000000

extern so_module so_mod;

static void check_kubridge(void) {
    if (!module_loaded("kubridge")) {
        l_fatal("kubridge is not loaded.");
        fatal_error("You need to install kubridge.skprx to play this game. "
                    "You can download it at https://github.com/bythos14/kubridge/releases");
    }

    // so_util needs kuKernelAllocMemBlock with a fixed address, which older
    // kubridge versions don't support.
    char *kubridge_hash = file_sha1sum("ux0:/tai/kubridge.skprx");
    if (!kubridge_hash) kubridge_hash = file_sha1sum("ur0:/tai/kubridge.skprx");

    if (!kubridge_hash) {
        fatal_error("Could not find kubridge.skprx file despite the plugin "
                    "itself being active. Please put it in either ur0:/tai or "
                    "ux0:/tai folder.");
    }

    const char *outdated = NULL;
    if (strcmp(kubridge_hash, "E033D76A90C9B8F2D496735C2692AFD8C3ED32FE") == 0)
        outdated = "v0.1";
    else if (strcmp(kubridge_hash, "6CFC985904F9BBE3A4F54DD96197F5DF3E523DCB") == 0)
        outdated = "v0.2";
    else if (strcmp(kubridge_hash, "AFAC6077618245D87CFF9ED2819223E6BB2DE5F8") == 0)
        outdated = "v0.3";

    free(kubridge_hash);

    if (outdated) {
        l_fatal("kubridge check failed.");
        fatal_error("You need to update kubridge.skprx to version v0.3.1 or "
                    "higher to play this game. Currently installed version: %s. "
                    "You can download the update at "
                    "https://github.com/bythos14/kubridge/releases", outdated);
    }

    l_success("kubridge check passed.");
}

static void check_data_files(void) {
    if (!file_exists(SO_PATH)) {
        l_fatal("SO could not be located.");
        fatal_error("Looks like you haven't installed the data files for this "
                    "port, or they are in an incorrect location. Please make "
                    "sure that you have %s file exactly at that path.", SO_PATH);
    }

    if (!file_exists(APK_PATH)) {
        l_fatal("APK could not be located.");
        fatal_error("The game's .apk file is missing. Please copy your "
                    "Bloons 2 .apk to %s (rename it if needed).", APK_PATH);
    }
}

// Needs vitaGL to be initialized (for the warning dialog).
static void check_so_version(void) {
#ifndef SKIP_SO_HASH_CHECK
    char *so_hash = file_sha1sum(SO_PATH);
    if (so_hash && strcmp(so_hash, SO_SHA1) != 0) {
        l_warn("Unexpected libbloons2.so hash %s", so_hash);
        warning("Your libbloons2.so does not match the supported version "
                "of the game (Bloons 2 v1.0.0). The game may crash or "
                "misbehave.");
    }
    free(so_hash);
#endif
}

void soloader_init_all() {
    // Launch `app0:configurator.bin` on `-config` init param
    sceAppUtilInit(&(SceAppUtilInitParam){}, &(SceAppUtilBootParam){});
    SceAppUtilAppEventParam eventParam;
    sceClibMemset(&eventParam, 0, sizeof(SceAppUtilAppEventParam));
    sceAppUtilReceiveAppEvent(&eventParam);
    if (eventParam.type == 0x05) {
        char buffer[2048];
        sceAppUtilAppEventParseLiveArea(&eventParam, buffer);
        if (strstr(buffer, "-config"))
            sceAppMgrLoadExec("app0:/configurator.bin", NULL, NULL);
    }

    // Default clocks. Bloons 2 is light, but texture decoding at load time
    // benefits from the extra CPU speed.
    scePowerSetArmClockFrequency(444);
    scePowerSetBusClockFrequency(222);
    scePowerSetGpuClockFrequency(222);
    scePowerSetGpuXbarClockFrequency(166);

    check_kubridge();
    check_data_files();

    file_mkpath(DATA_PATH "files/Contents/Resources/", 0777);
    file_mkpath(DATA_PATH "external/Contents/Resources/", 0777);

    if (so_file_load(&so_mod, SO_PATH, LOAD_ADDRESS) < 0) {
        l_fatal("SO could not be loaded.");
        fatal_error("Error: could not load %s.", SO_PATH);
    }

    so_relocate(&so_mod);
    l_success("SO relocated.");

    resolve_imports(&so_mod);
    l_success("SO imports resolved.");

    so_patch();
    l_success("SO patched.");

    so_flush_caches(&so_mod);
    l_success("SO caches flushed.");

    gl_preload();
    l_success("OpenGL preloaded.");

    jni_init();
    l_success("FalsoJNI initialized.");

    gl_init();
    l_success("OpenGL initialized.");

    check_so_version();

    if (trophies_init() < 0) {
        l_warn("Trophies initialization failed.");
        // Only nag once; the game is fully playable without trophies.
        if (!file_exists(DATA_PATH "notrpdrm_warned")) {
            warning("This game features unlockable trophies but NoTrpDrm is "
                    "not installed. If you want to be able to unlock trophies, "
                    "please install it.");
            file_save(DATA_PATH "notrpdrm_warned", (const uint8_t *)"1", 1);
        }
    } else {
        l_success("Trophies initialized.");
    }

    // Static constructors may already touch the filesystem, so run them only
    // after everything above is in place.
    so_initialize(&so_mod);
    l_success("SO initialized.");

    coco_resolve();

    prefs_load(DATA_PATH "prefs.txt");
    save_import_run();
    settings_load();

    if (apk_open(APK_PATH) < 0)
        fatal_error("Error: could not open %s. Is it a valid .apk file?", APK_PATH);

    if (audio_init() < 0)
        l_warn("Audio initialization failed, the game will be silent.");
    else
        l_success("Audio initialized.");
}
