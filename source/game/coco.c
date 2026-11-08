/*
 * coco.c
 *
 * Native entry points exported by libbloons2.so for the Java class
 * com.miniclip.nativeJNI.CocoJNI, resolved once after the .so is loaded.
 *
 * This software may be modified and distributed under the terms
 * of the GPLv3 license. See the LICENSE file for details.
 */

#include "game/coco.h"

#include <so_util/so_util.h>

#include "utils/dialog.h"
#include "utils/logger.h"

extern so_module so_mod;

CocoJNI coco;

#define JNI_PREFIX "Java_com_miniclip_nativeJNI_CocoJNI_"

#define RESOLVE_REQUIRED(name) do { \
    coco.name = (void *)so_symbol(&so_mod, JNI_PREFIX #name); \
    if (!coco.name) \
        fatal_error("Error: %s not found in libbloons2.so. Make sure you are " \
                    "using the supported version of the game.", #name); \
} while (0)

#define RESOLVE_OPTIONAL(name) do { \
    coco.name = (void *)so_symbol(&so_mod, JNI_PREFIX #name); \
    if (!coco.name) l_warn("Optional native %s not found", #name); \
} while (0)

void coco_resolve(void) {
    RESOLVE_REQUIRED(Mrun);
    RESOLVE_REQUIRED(Mrender);
    RESOLVE_OPTIONAL(Mend);
    RESOLVE_OPTIONAL(Mpause);
    RESOLVE_OPTIONAL(Mresume);
    RESOLVE_OPTIONAL(MlowMemory);
    RESOLVE_OPTIONAL(MpreserveContext);
    RESOLVE_OPTIONAL(MrestoreContext);
    RESOLVE_OPTIONAL(MsetGamePause);

    RESOLVE_REQUIRED(MsetMainActivity);
    RESOLVE_OPTIONAL(MsetCountryCode);
    RESOLVE_OPTIONAL(MsetAppVersionNumber);
    RESOLVE_REQUIRED(MsetFilesPath);
    RESOLVE_REQUIRED(MsetAPKPath);
    RESOLVE_OPTIONAL(MsetDensity);
    RESOLVE_OPTIONAL(MsetIsRetina);
    RESOLVE_REQUIRED(MdisplayInfo);
    RESOLVE_OPTIONAL(MrealDisplayInfo);

    RESOLVE_REQUIRED(MtouchesBegin);
    RESOLVE_REQUIRED(MtouchesEnd);
    RESOLVE_REQUIRED(MtouchesMove);
    RESOLVE_OPTIONAL(MtouchesCancel);
    RESOLVE_OPTIONAL(MpressedBackButton);
    RESOLVE_OPTIONAL(MpressedKey);

    RESOLVE_REQUIRED(MnativeInitBitmap);
    RESOLVE_OPTIONAL(MonMessageBoxButtonPressed);
    RESOLVE_OPTIONAL(MkeyboardInputResponse);

    l_success("CocoJNI natives resolved.");
}
