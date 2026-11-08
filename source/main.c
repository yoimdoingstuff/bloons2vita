/*
 * main.c
 *
 * Bloons 2 PS Vita port: entry point and game lifecycle.
 *
 * The startup sequence mirrors what the original Java code does:
 *   cocojava.firstRun()             -> MsetMainActivity, MsetCountryCode,
 *                                      MsetAppVersionNumber, MsetFilesPath,
 *                                      MsetDensity
 *   ClearGLSurfaceView.<init>       -> MpreserveContext
 *   ClearRenderer.onSurfaceChanged  -> MrestoreContext, MsetIsRetina,
 *                                      MrealDisplayInfo, MdisplayInfo
 *   ClearRenderer.onDrawFrame (1st) -> createResources() (unpack
 *                                      assets/unpack/, MsetAPKPath), Mrun
 *   ClearRenderer.onDrawFrame       -> Mrender
 *
 * Input (touchscreen and the button controls) lives in game/input.c.
 *
 * This software may be modified and distributed under the terms
 * of the GPLv3 license. See the LICENSE file for details.
 */

#include <pthread.h>
#include <stdbool.h>
#include <string.h>

#include <psp2/apputil.h>
#include <psp2/kernel/processmgr.h>
#include <psp2/kernel/threadmgr.h>
#include <psp2/system_param.h>

#include <falso_jni/FalsoJNI.h>
#include <so_util/so_util.h>

#include "game/apk.h"
#include "game/coco.h"
#include "game/input.h"
#include "java.h"
#include "reimpl/io.h"
#include "utils/glutil.h"
#include "utils/init.h"
#include "utils/logger.h"
#include "utils/utils.h"

#define SCREEN_W 960
#define SCREEN_H 544

// Leave the rest of the memory to vitaGL (textures live outside the heap).
int _newlib_heap_size_user = 192 * 1024 * 1024;

so_module so_mod;

static jstring jstr(const char *s) {
    return jni->NewStringUTF(&jni, s);
}

static const char *system_language_code(void) {
    static const char *codes[] = {
        "ja", "en", "fr", "es", "de", "it", "nl", "pt", "ru", "ko",
        "zh", "zh", "fi", "sv", "da", "no", "pl", "pt", "en", "tr",
    };
    int lang = 1;
    sceAppUtilSystemParamGetInt(SCE_SYSTEM_PARAM_ID_LANG, &lang);
    if (lang < 0 || lang >= (int)(sizeof(codes) / sizeof(codes[0])))
        return "en";
    return codes[lang];
}

static void game_setup(void) {
    int (*JNI_OnLoad)(JavaVM *vm, void *reserved) = (void *)so_symbol(&so_mod, "JNI_OnLoad");
    if (JNI_OnLoad) {
        JNI_OnLoad(&jvm, NULL);
        l_success("JNI_OnLoad passed.");
    }

    // cocojava.firstRun()
    coco.MsetMainActivity(&jni, COCO_CLASS, COCO_ACTIVITY);
    if (coco.MsetCountryCode)
        coco.MsetCountryCode(&jni, COCO_CLASS, jstr(system_language_code()));
    if (coco.MsetAppVersionNumber)
        coco.MsetAppVersionNumber(&jni, COCO_CLASS, jstr("1.0.0"));
    // (getFilesDir(), getExternalStorageDirectory()); the game appends
    // "/Contents/Resources/" to these itself. We pass the real Android paths
    // and let reimpl/io.c map them to DATA_PATH.
    coco.MsetFilesPath(&jni, COCO_CLASS, jstr(ANDROID_FILES_PATH), jstr(ANDROID_EXTERNAL_PATH));
    if (coco.MsetDensity)
        coco.MsetDensity(&jni, COCO_CLASS, 1.5f); // hdpi
    l_success("firstRun sequence passed.");

    // ClearGLSurfaceView.<init>
    if (coco.MpreserveContext)
        coco.MpreserveContext(&jni, COCO_CLASS, 1);

    // ClearRenderer.onSurfaceChanged(960, 544). A display wider than 799px
    // makes the Java side pick the high-resolution ("retina") assets.
    if (coco.MrestoreContext)
        coco.MrestoreContext(&jni, COCO_CLASS);
    if (coco.MsetIsRetina)
        coco.MsetIsRetina(&jni, COCO_CLASS, USE_HIGHRES_ASSETS);
    glViewport(0, 0, SCREEN_W, SCREEN_H);
    if (coco.MrealDisplayInfo)
        coco.MrealDisplayInfo(&jni, COCO_CLASS, SCREEN_W, SCREEN_H);
    coco.MdisplayInfo(&jni, COCO_CLASS, SCREEN_W, SCREEN_H);
    l_success("onSurfaceChanged sequence passed.");

    // ClearRenderer.onDrawFrame, first frame: cocojava.createResources()
    // unpacks assets/unpack/* into <files>/Contents/Resources/.
    int n = apk_extract_dir("assets/unpack/", DATA_PATH "files/Contents/Resources");
    if (n < 0)
        l_error("Failed to unpack game resources from the apk.");
    coco.MsetAPKPath(&jni, COCO_CLASS, jstr(ANDROID_APK_PATH));

    coco.Mrun(&jni, COCO_CLASS);
    l_success("Mrun passed.");
}

static void *game_thread(void *arg) {
    game_setup();
    input_init();

    bool dialog_was_active = false;
    uint64_t last_frame = sceKernelGetProcessTimeWide();

    while (1) {
        if (!dialog_was_active)
            input_update();

        coco.Mrender(&jni, COCO_CLASS);

        bool dialog_active = java_ui_update();
        if (dialog_active && !dialog_was_active)
            input_release_all();
        else if (!dialog_active && dialog_was_active)
            input_resume();
        dialog_was_active = dialog_active;

        gl_swap(dialog_active ? GL_TRUE : GL_FALSE);

        // vsync already caps us at 60 FPS; honour lower rates set by setFPS()
        float fps = java_target_fps;
        if (fps > 0.0f && fps < 59.0f) {
            uint64_t frame_us = (uint64_t)(1000000.0f / fps);
            uint64_t elapsed = sceKernelGetProcessTimeWide() - last_frame;
            if (elapsed < frame_us)
                sceKernelDelayThread((SceUInt)(frame_us - elapsed));
        }
        last_frame = sceKernelGetProcessTimeWide();
    }

    return NULL;
}

int main() {
    soloader_init_all();

    // Run the game in a thread with an enlarged stack, as Android's GL
    // thread would have.
    pthread_t t;
    pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_attr_setstacksize(&attr, 8 * 1024 * 1024);
    pthread_create(&t, &attr, game_thread, NULL);
    pthread_join(t, NULL);

    return 0;
}
