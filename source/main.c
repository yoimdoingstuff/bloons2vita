/*
 * main.c
 *
 * Bloons 2 PS Vita port: entry point, game lifecycle and input.
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
 * This software may be modified and distributed under the terms
 * of the GPLv3 license. See the LICENSE file for details.
 */

#include <pthread.h>
#include <stdbool.h>
#include <string.h>

#include <psp2/apputil.h>
#include <psp2/ctrl.h>
#include <psp2/kernel/clib.h>
#include <psp2/kernel/processmgr.h>
#include <psp2/kernel/threadmgr.h>
#include <psp2/system_param.h>
#include <psp2/touch.h>

#include <falso_jni/FalsoJNI.h>
#include <falso_jni/FalsoJNI_ImplBridge.h>
#include <so_util/so_util.h>

#include "game/apk.h"
#include "game/coco.h"
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

/*
 * Helpers
 */

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

/*
 * Touch input
 *
 * Android pointer IDs are small integers that get reused, while the Vita
 * reports arbitrary 0-127 IDs, so we hand out our own slots.
 */

#define MAX_POINTERS 6

typedef struct {
    bool active;
    int vita_id;
    float x, y;
} Pointer;

static Pointer pointers[MAX_POINTERS];

static int pointer_find(int vita_id) {
    for (int i = 0; i < MAX_POINTERS; i++)
        if (pointers[i].active && pointers[i].vita_id == vita_id)
            return i;
    return -1;
}

static int pointer_alloc(int vita_id) {
    for (int i = 0; i < MAX_POINTERS; i++) {
        if (!pointers[i].active) {
            pointers[i].active = true;
            pointers[i].vita_id = vita_id;
            return i;
        }
    }
    return -1;
}

static void send_touches_move(void) {
    int n = 0;
    for (int i = 0; i < MAX_POINTERS; i++)
        if (pointers[i].active) n++;
    if (n == 0) return;

    jintArray ids = jni->NewIntArray(&jni, n);
    jfloatArray xs = jni->NewFloatArray(&jni, n);
    jfloatArray ys = jni->NewFloatArray(&jni, n);

    if (ids && xs && ys) {
        jint *pid = ((JavaDynArray *)ids)->array;
        jfloat *px = ((JavaDynArray *)xs)->array;
        jfloat *py = ((JavaDynArray *)ys)->array;

        int k = 0;
        for (int i = 0; i < MAX_POINTERS; i++) {
            if (!pointers[i].active) continue;
            pid[k] = i;
            px[k] = pointers[i].x;
            py[k] = pointers[i].y;
            k++;
        }

        coco.MtouchesMove(&jni, COCO_CLASS, ids, xs, ys);
    }

    if (ids) jda_free((JavaDynArray *)ids);
    if (xs) jda_free((JavaDynArray *)xs);
    if (ys) jda_free((JavaDynArray *)ys);
}

static void poll_touch(void) {
    SceTouchData touch;
    if (sceTouchPeek(SCE_TOUCH_PORT_FRONT, &touch, 1) < 0)
        return;

    bool moved = false;

    // Releases
    for (int i = 0; i < MAX_POINTERS; i++) {
        if (!pointers[i].active) continue;
        bool still_down = false;
        for (int r = 0; r < touch.reportNum; r++) {
            if (touch.report[r].id == pointers[i].vita_id) {
                still_down = true;
                break;
            }
        }
        if (!still_down) {
            pointers[i].active = false;
            coco.MtouchesEnd(&jni, COCO_CLASS, i, pointers[i].x, pointers[i].y);
        }
    }

    // Presses and moves
    for (int r = 0; r < touch.reportNum; r++) {
        float x = (float)touch.report[r].x * (float)SCREEN_W / 1920.0f;
        float y = (float)touch.report[r].y * (float)SCREEN_H / 1088.0f;

        int slot = pointer_find(touch.report[r].id);
        if (slot < 0) {
            slot = pointer_alloc(touch.report[r].id);
            if (slot < 0) continue;
            pointers[slot].x = x;
            pointers[slot].y = y;
            coco.MtouchesBegin(&jni, COCO_CLASS, slot, x, y);
        } else if (pointers[slot].x != x || pointers[slot].y != y) {
            pointers[slot].x = x;
            pointers[slot].y = y;
            moved = true;
        }
    }

    if (moved)
        send_touches_move();
}

static void release_all_touches(void) {
    for (int i = 0; i < MAX_POINTERS; i++) {
        if (pointers[i].active) {
            pointers[i].active = false;
            coco.MtouchesEnd(&jni, COCO_CLASS, i, pointers[i].x, pointers[i].y);
        }
    }
}

/*
 * Buttons: the game is touch-only; the Android Back key opens the pause menu
 * in-game and the quit prompt in the main menu.
 */

static uint32_t old_buttons = 0;

static void poll_buttons(void) {
    SceCtrlData pad;
    if (sceCtrlPeekBufferPositive(0, &pad, 1) < 0)
        return;

    uint32_t pressed = pad.buttons & ~old_buttons;
    old_buttons = pad.buttons;

    if ((pressed & (SCE_CTRL_START | SCE_CTRL_CIRCLE)) && coco.MpressedBackButton)
        coco.MpressedBackButton(&jni, COCO_CLASS);
}

/*
 * Game thread
 */

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
    sceCtrlSetSamplingMode(SCE_CTRL_MODE_DIGITAL);
    sceTouchSetSamplingState(SCE_TOUCH_PORT_FRONT, SCE_TOUCH_SAMPLING_STATE_START);

    game_setup();

    bool dialog_was_active = false;
    uint64_t last_frame = sceKernelGetProcessTimeWide();

    while (1) {
        if (!dialog_was_active) {
            poll_touch();
            poll_buttons();
        }

        coco.Mrender(&jni, COCO_CLASS);

        bool dialog_active = java_ui_update();
        if (dialog_active && !dialog_was_active)
            release_all_touches();
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
