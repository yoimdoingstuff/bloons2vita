/*
 * java.c
 *
 * FalsoJNI implementation of the Java side of Bloons 2: the static methods of
 * com.miniclip.nativeJNI.cocojava that libbloons2.so looks up with
 * GetStaticMethodID and calls back into.
 *
 * Everything that only makes sense on a phone (ads, Tapjoy, GetJar, Flurry,
 * OpenFeint, Facebook, in-app billing, e-mail/SMS, ...) is implemented as a
 * harmless no-op that reports "not available".
 *
 * This software may be modified and distributed under the terms
 * of the GPLv3 license. See the LICENSE file for details.
 */

#include <falso_jni/FalsoJNI_Impl.h>
#include <falso_jni/FalsoJNI_Logger.h>
#include <falso_jni/FalsoJNI.h>

#include <malloc.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <psp2/kernel/processmgr.h>
#include <psp2/message_dialog.h>

#include "game/achievements.h"
#include "game/apk.h"
#include "game/audio.h"
#include "game/coco.h"
#include "game/prefs.h"
#include "game/texture.h"
#include "java.h"
#include "utils/dialog.h"
#include "utils/logger.h"
#include "utils/utils.h"

/*
 * Helpers
 */

static char *jstr_dup(jstring s) {
    if (!s) return NULL;
    const char *c = jni->GetStringUTFChars(&jni, s, NULL);
    char *r = c ? strdup(c) : NULL;
    if (c) jni->ReleaseStringUTFChars(&jni, s, (char *)c);
    return r;
}

// jfloat arguments arrive promoted to double through the va_list
#define VA_FLOAT(args) ((jfloat)va_arg((args), double))
#define VA_BOOL(args)  ((jboolean)va_arg((args), int))

/*
 * Frame pacing (cocojava.setFPS -> ClearRenderer.animationInterval)
 */

volatile float java_target_fps = 60.0f;

/*
 * UI requests from the game thread. They are serviced from java_ui_update(),
 * called once per frame by the main loop right after Mrender().
 */

typedef enum {
    UI_NONE = 0,
    UI_MSGBOX_PENDING,
    UI_MSGBOX_ACTIVE,
    UI_IME_PENDING,
    UI_IME_ACTIVE,
} UiState;

static UiState ui_state = UI_NONE;

static int msgbox_id;
static char *msgbox_title;
static char *msgbox_msg;
static char *msgbox_btn[3];
static int msgbox_nbtn;
static bool msgbox_dismissed;

static char *ime_initial;

static void msgbox_free(void) {
    free(msgbox_title); msgbox_title = NULL;
    free(msgbox_msg); msgbox_msg = NULL;
    for (int i = 0; i < 3; i++) { free(msgbox_btn[i]); msgbox_btn[i] = NULL; }
    msgbox_nbtn = 0;
}

bool java_ui_update(void) {
    switch (ui_state) {
        case UI_MSGBOX_PENDING:
            if (msgbox_start(msgbox_title, msgbox_msg,
                             (const char **)msgbox_btn, msgbox_nbtn) < 0) {
                l_error("Failed to open message box %i", msgbox_id);
                msgbox_free();
                ui_state = UI_NONE;
                return false;
            }
            ui_state = UI_MSGBOX_ACTIVE;
            return true;

        case UI_MSGBOX_ACTIVE: {
            int btn = msgbox_poll();
            if (btn < 0) return true;
            ui_state = UI_NONE;
            if (!msgbox_dismissed && coco.MonMessageBoxButtonPressed)
                coco.MonMessageBoxButtonPressed(&jni, COCO_CLASS, msgbox_id, btn);
            msgbox_free();
            return false;
        }

        case UI_IME_PENDING:
            if (init_ime_dialog("", ime_initial ? ime_initial : "") < 0) {
                ui_state = UI_NONE;
                return false;
            }
            ui_state = UI_IME_ACTIVE;
            return true;

        case UI_IME_ACTIVE: {
            char *text = get_ime_dialog_result();
            if (!text) return true;
            ui_state = UI_NONE;
            const char *answer = (*text || !ime_initial) ? text : ime_initial;
            if (coco.MkeyboardInputResponse)
                coco.MkeyboardInputResponse(&jni, COCO_CLASS, jni->NewStringUTF(&jni, answer));
            free(ime_initial);
            ime_initial = NULL;
            return false;
        }

        default:
            return false;
    }
}

/*
 * Generic stubs for phone-only features
 */

void stub_void(jmethodID id, va_list args) { }
jint stub_int0(jmethodID id, va_list args) { return 0; }
jboolean stub_true(jmethodID id, va_list args) { return JNI_TRUE; }
jobject stub_empty_string(jmethodID id, va_list args) { return jni->NewStringUTF(&jni, ""); }

/*
 * Device info
 */

jint getAndroidVersion(jmethodID id, va_list args) {
    return 10; // Android 2.3.3, the era this build targets
}

jint getAppVersionCode(jmethodID id, va_list args) {
    return 1;
}

jint DeviceSupportsMultitouch(jmethodID id, va_list args) {
    return 1;
}

jfloat availableMemoryOnDevice(jmethodID id, va_list args) {
    return 512.0f;
}

jfloat availableMemoryOnSDcard(jmethodID id, va_list args) {
    return 1024.0f;
}

jobject getDeviceId(jmethodID id, va_list args) {
    // Same scheme as the Java side: a random id persisted in preferences.
    const char *stored = prefs_get_string("Device Id", "");
    if (stored && *stored)
        return jni->NewStringUTF(&jni, stored);

    static const char alphabet[] = "abcdefghijmnopqrstuvxz0123456789";
    char id_str[64];
    int n = snprintf(id_str, sizeof(id_str), "%llu-", (unsigned long long)time(NULL) * 1000ULL);
    srand((unsigned)time(NULL));
    for (int i = 0; i < 10 && n < (int)sizeof(id_str) - 1; i++)
        id_str[n++] = alphabet[rand() % (sizeof(alphabet) - 1)];
    id_str[n] = '\0';

    prefs_set_string("Device Id", id_str);
    return jni->NewStringUTF(&jni, id_str);
}

/*
 * SharedPreferences("GAME_INFO")
 */

void SharedPreferences_setInt(jmethodID id, va_list args) {
    char *key = jstr_dup(va_arg(args, jstring));
    jint value = va_arg(args, jint);
    if (key) prefs_set_int(key, value);
    free(key);
}

jint SharedPreferences_getInt(jmethodID id, va_list args) {
    char *key = jstr_dup(va_arg(args, jstring));
    jint ret = key ? prefs_get_int(key, 0) : 0;
    free(key);
    return ret;
}

void SharedPreferences_setString(jmethodID id, va_list args) {
    char *key = jstr_dup(va_arg(args, jstring));
    char *value = jstr_dup(va_arg(args, jstring));
    if (key) prefs_set_string(key, value ? value : "");
    free(key);
    free(value);
}

jobject SharedPreferences_getString(jmethodID id, va_list args) {
    char *key = jstr_dup(va_arg(args, jstring));
    jobject ret = jni->NewStringUTF(&jni, key ? prefs_get_string(key, "") : "");
    free(key);
    return ret;
}

jint isFullVersion(jmethodID id, va_list args) {
    return prefs_get_int("FULL_VERSION", 0);
}

void setFullVersion(jmethodID id, va_list args) {
    prefs_set_int("FULL_VERSION", 1);
}

jint getAdsDisabled(jmethodID id, va_list args) {
    return 1; // There's no ad network on the Vita
}

jint getPendingPurchaseAmount(jmethodID id, va_list args) {
    char *key = jstr_dup(va_arg(args, jstring));
    jint ret = key ? prefs_get_int(key, 0) : 0;
    free(key);
    return ret;
}

void setPendingPurchaseAmount(jmethodID id, va_list args) {
    char *key = jstr_dup(va_arg(args, jstring));
    jint value = va_arg(args, jint);
    if (key) prefs_set_int(key, value);
    free(key);
}

jobject getPurchasedOwnedItems(jmethodID id, va_list args) {
    return jni->NewObjectArray(&jni, 0, NULL, NULL);
}

/*
 * Textures (cocotexture)
 */

void loadTexture(jmethodID id, va_list args) {
    char *path = jstr_dup(va_arg(args, jstring));
    texture_load(path);
    free(path);
}

void copyAsset(jmethodID id, va_list args) {
    // Java copies the asset to /sdcard/<name>; our IO layer maps /sdcard/
    // to DATA_PATH.
    char *name = jstr_dup(va_arg(args, jstring));
    if (!name) return;

    uint8_t *buf;
    size_t size;
    if (asset_load(name, &buf, &size)) {
        char out[512];
        snprintf(out, sizeof(out), DATA_PATH "%s", name);
        file_mkpath(out, 0777);
        file_save(out, buf, size);
        free(buf);
    }
    free(name);
}

/*
 * Sound effects (cocosound)
 */

jint preloadEffect(jmethodID id, va_list args) {
    char *path = jstr_dup(va_arg(args, jstring));
    jint ret = audio_preload_effect(path);
    free(path);
    return ret;
}

void unloadEffect(jmethodID id, va_list args) {
    char *path = jstr_dup(va_arg(args, jstring));
    audio_unload_effect(path);
    free(path);
}

// playEffect(int soundId, float pitch, float pan, int loop, float gain)
void playEffect(jmethodID id, va_list args) {
    jint sound = va_arg(args, jint);
    jfloat pitch = VA_FLOAT(args);
    jfloat pan = VA_FLOAT(args);
    jint loop = va_arg(args, jint);
    jfloat gain = VA_FLOAT(args);
    audio_play_effect(sound, pitch, pan, loop != 0, gain);
}

void stopEffect(jmethodID id, va_list args) {
    audio_stop_effect(va_arg(args, jint));
}

void pauseEffect(jmethodID id, va_list args) {
    audio_pause_effect(va_arg(args, jint));
}

void setEffectLooping(jmethodID id, va_list args) {
    jint sound = va_arg(args, jint);
    jboolean loop = VA_BOOL(args);
    audio_set_effect_looping(sound, loop);
}

void setEffectPitch(jmethodID id, va_list args) {
    jint sound = va_arg(args, jint);
    audio_set_effect_pitch(sound, VA_FLOAT(args));
}

void setEffectGain(jmethodID id, va_list args) {
    jint sound = va_arg(args, jint);
    audio_set_effect_gain(sound, VA_FLOAT(args));
}

void pauseAllSounds(jmethodID id, va_list args) {
    audio_pause_all_effects();
}

void resumeAllSounds(jmethodID id, va_list args) {
    audio_resume_all_effects();
}

void setEffectsVolume(jmethodID id, va_list args) {
    audio_set_effects_volume(VA_FLOAT(args));
}

jfloat getEffectsVolume(jmethodID id, va_list args) {
    return audio_get_effects_volume();
}

/*
 * Background music (cocomusic)
 */

void playBackgroundMusic(jmethodID id, va_list args) {
    char *path = jstr_dup(va_arg(args, jstring));
    jboolean loop = VA_BOOL(args);
    audio_play_music(path, loop);
    free(path);
}

void stopBackgroundMusic(jmethodID id, va_list args) {
    audio_stop_music();
}

void pauseBackgroundMusic(jmethodID id, va_list args) {
    audio_pause_music();
}

void resumeBackgroundMusic(jmethodID id, va_list args) {
    audio_resume_music();
}

void rewindBackgroundMusic(jmethodID id, va_list args) {
    audio_rewind_music();
}

jboolean isBackgroundMusicPlaying(jmethodID id, va_list args) {
    return audio_is_music_playing() ? JNI_TRUE : JNI_FALSE;
}

jfloat getBackgroundMusicVolume(jmethodID id, va_list args) {
    return audio_get_music_volume();
}

void setBackgroundMusicVolume(jmethodID id, va_list args) {
    audio_set_music_volume(VA_FLOAT(args));
}

jint getDurationForSound(jmethodID id, va_list args) {
    char *path = jstr_dup(va_arg(args, jstring));
    jint ret = audio_get_duration_ms(path);
    free(path);
    return ret;
}

/*
 * Achievements -> trophies. The game's Game Center layer reports through
 * these; IDs are the iOS ones (com.digitalgoldfish.bloons2.*).
 */

// openFeint_UnlockAchievement(String id)
void openFeint_UnlockAchievement(jmethodID id, va_list args) {
    char *ach = jstr_dup(va_arg(args, jstring));
    achievement_report(ach, 100.0f);
    free(ach);
}

// openFeint_UpdateAchievement(String id, float percent) and
// custom_updateAchievement(String id, float percent[, Object])
void updateAchievement(jmethodID id, va_list args) {
    char *ach = jstr_dup(va_arg(args, jstring));
    jfloat percent = VA_FLOAT(args);
    achievement_report(ach, percent);
    free(ach);
}

void setGameState(jmethodID id, va_list args) {
    jint state = va_arg(args, jint);
    l_info("setGameState(%i)", state);
}

/*
 * Rendering / app control
 */

void setFPS(jmethodID id, va_list args) {
    jfloat fps = VA_FLOAT(args);
    if (fps > 0.0f) java_target_fps = fps;
    l_info("setFPS(%f)", fps);
}

void exitApplication(jmethodID id, va_list args) {
    l_info("exitApplication()");
    sceKernelExitProcess(0);
}

/*
 * Dialogs
 */

// showMessageBox(int id, String title, String message, String[] buttons)
void showMessageBox(jmethodID id, va_list args) {
    jint box_id = va_arg(args, jint);
    jstring title = va_arg(args, jstring);
    jstring msg = va_arg(args, jstring);
    jobjectArray buttons = va_arg(args, jobjectArray);

    if (ui_state != UI_NONE) {
        l_warn("showMessageBox(%i): another dialog is already open, ignoring", box_id);
        return;
    }

    msgbox_free();
    msgbox_id = box_id;
    msgbox_title = jstr_dup(title);
    msgbox_msg = jstr_dup(msg);
    msgbox_dismissed = false;

    int n = buttons ? jni->GetArrayLength(&jni, buttons) : 0;
    if (n > 3) n = 3;
    for (int i = 0; i < n; i++)
        msgbox_btn[i] = jstr_dup(jni->GetObjectArrayElement(&jni, buttons, i));
    if (n == 0) {
        msgbox_btn[0] = strdup("OK");
        n = 1;
    }
    msgbox_nbtn = n;

    l_info("showMessageBox(%i, \"%s\", \"%s\", %i buttons)", box_id,
           msgbox_title ? msgbox_title : "", msgbox_msg ? msgbox_msg : "", n);
    ui_state = UI_MSGBOX_PENDING;
}

void closeMessageBox(jmethodID id, va_list args) {
    jint box_id = va_arg(args, jint);
    if (box_id != msgbox_id) return;

    if (ui_state == UI_MSGBOX_PENDING) {
        msgbox_free();
        ui_state = UI_NONE;
    } else if (ui_state == UI_MSGBOX_ACTIVE) {
        // closed by the game, not by the player: don't report a button
        msgbox_dismissed = true;
        sceMsgDialogClose();
    }
}

void keyboardInput_Show(jmethodID id, va_list args) {
    char *initial = jstr_dup(va_arg(args, jstring));
    if (ui_state != UI_NONE) {
        free(initial);
        return;
    }
    free(ime_initial);
    ime_initial = initial;
    ui_state = UI_IME_PENDING;
}

/*
 * Method table
 */

enum {
    M_getAndroidVersion = 1,
    M_getAppVersionCode,
    M_DeviceSupportsMultitouch,
    M_availableMemoryOnDevice,
    M_availableMemoryOnSDcard,
    M_isSDcardPresent,
    M_getDeviceId,

    M_SharedPreferences_setInt,
    M_SharedPreferences_getInt,
    M_SharedPreferences_setString,
    M_SharedPreferences_getString,
    M_isFullVersion,
    M_setFullVersion,
    M_getAdsDisabled,
    M_getPendingPurchaseAmount,
    M_setPendingPurchaseAmount,
    M_getPurchasedOwnedItems,
    M_isPurchaseOwned,

    M_loadTexture,
    M_copyAsset,

    M_preloadEffect,
    M_unloadEffect,
    M_playEffect,
    M_stopEffect,
    M_pauseEffect,
    M_setEffectLooping,
    M_setEffectPitch,
    M_setEffectGain,
    M_pauseAllSounds,
    M_resumeAllSounds,
    M_setEffectsVolume,
    M_getEffectsVolume,

    M_playBackgroundMusic,
    M_stopBackgroundMusic,
    M_pauseBackgroundMusic,
    M_resumeBackgroundMusic,
    M_rewindBackgroundMusic,
    M_isBackgroundMusicPlaying,
    M_getBackgroundMusicVolume,
    M_setBackgroundMusicVolume,
    M_getDurationForSound,

    M_setFPS,
    M_exitApplication,
    M_showMessageBox,
    M_hideMessageBox,
    M_closeMessageBox,
    M_keyboardInput_Show,
    M_keyboardInput_Show_withMaxLengthLocked,

    // phone-only features: void no-ops
    M_keyboardInput_Hide,
    M_setKeyboardInputPosition,
    M_setKeyboardInputStyle,
    M_setKeyboardInputVisible,
    M_setKeyboardInputVisibleDelayed,
    M_setKeyboardInputText,
    M_setKeyboardInputMaxLength,
    M_setKeyboardInputSingleLine,
    M_setKeyboardInputTypePassword,
    M_callInAppGetJar,
    M_pushInterstitial,
    M_showInterstitialMopubView,
    M_showMiniclipView,
    M_enableAdsWithPositionForGameplay,
    M_enableAdsWithPositionForGameplayGivenWidth,
    M_enableAdsWithPositionForMenu,
    M_enableAdsWithPositionForMenuGivenWidth,
    M_enableAds,
    M_disableAds,
    M_hideAds,
    M_showTapJoyOffers,
    M_spendTapPoints,
    M_setGameState,
    M_openFeint_login,
    M_openFeint_showDashboard,
    M_openFeint_showAchievements,
    M_openFeint_showLeaderboards,
    M_openFeint_showLeaderboard,
    M_openFeint_UnlockAchievement,
    M_openFeint_UpdateAchievement,
    M_openFeint_PostScore,
    M_openFeint_PostScoreWithString,
    M_startFlurrySession,
    M_logFlurryEvent,
    M_endFlurryTimedEvent,
    M_custom_logEvent,
    M_flashScreen,
    M_updateNotificationStatus,
    M_vibrateForMS,
    M_showBigAd,
    M_loadFullScreenAd,
    M_showFullScreenAd,
    M_enableNewsfeed,
    M_disableNewsfeed,
    M_showSimpleTapjoyFeatureAd,
    M_showTJOfferDialog,
    M_showRotatedBanner,
    M_hideRotatedBanner,
    M_showHorizontalBanner,
    M_hideHorizontalBanner,
    M_faceBook_dialog,
    M_faceBook_like,
    M_faceBook_request,
    M_faceBook_logout,
    M_faceBook_authorize,
    M_showHtmlDialog,
    M_setClipboardText,
    M_hideAdsInSeconds,
    M_permanentlyRemoveAds,
    M_Newsfeed_showDialog,
    M_sendEmail,
    M_sendSMS,
    M_sendGoogleEvent,
    M_custom_updateScore,
    M_custom_showLeaderboard,
    M_custom_showLeaderboards,
    M_custom_updateAchievement,
    M_custom_showAchievements,
    M_NF_showBoard,
    M_NF_showUrgentBoard,
    M_NF_dismissBoard,
    M_NF_setSandBox,
    M_NF_setShowBadge,
    M_showCheckpoint,
    M_showDatePickerDialog_JNI,
    M_openLink,
    M_openExternalURL,

    // phone-only features: int-returning "not available"
    M_getRecommendedPriceGetJar,
    M_callInAppPurchase,
    M_callInAppPurchaseManaged,
    M_callInAppPurchaseRemoveAds,
    M_callInAppPurchaseRemoveAdsManaged,
    M_getPendingPurchaseSignatureAmount,
    M_callLastPendingPurchaseSignature,
    M_removeLastPendingPurchaseSignature,
    M_getNotificationStatus,
    M_faceBook_isSessionValid,
    M_Newsfeed_getNumber,
    M_isAppInstalled,
    M_checkForInstalledApp,

    M_faceBook_getAccessToken,
};

NameToMethodID nameToMethodId[] = {
    { M_getAndroidVersion, "getAndroidVersion", METHOD_TYPE_INT },
    { M_getAppVersionCode, "getAppVersionCode", METHOD_TYPE_INT },
    { M_DeviceSupportsMultitouch, "DeviceSupportsMultitouch", METHOD_TYPE_INT },
    { M_availableMemoryOnDevice, "availableMemoryOnDevice", METHOD_TYPE_FLOAT },
    { M_availableMemoryOnSDcard, "availableMemoryOnSDcard", METHOD_TYPE_FLOAT },
    { M_isSDcardPresent, "isSDcardPresent", METHOD_TYPE_BOOLEAN },
    { M_getDeviceId, "getDeviceId", METHOD_TYPE_OBJECT },

    { M_SharedPreferences_setInt, "SharedPreferences_setInt", METHOD_TYPE_VOID },
    { M_SharedPreferences_getInt, "SharedPreferences_getInt", METHOD_TYPE_INT },
    { M_SharedPreferences_setString, "SharedPreferences_setString", METHOD_TYPE_VOID },
    { M_SharedPreferences_getString, "SharedPreferences_getString", METHOD_TYPE_OBJECT },
    { M_isFullVersion, "isFullVersion", METHOD_TYPE_INT },
    { M_setFullVersion, "setFullVersion", METHOD_TYPE_VOID },
    { M_getAdsDisabled, "getAdsDisabled", METHOD_TYPE_INT },
    { M_getPendingPurchaseAmount, "getPendingPurchaseAmount", METHOD_TYPE_INT },
    { M_setPendingPurchaseAmount, "setPendingPurchaseAmount", METHOD_TYPE_VOID },
    { M_getPurchasedOwnedItems, "getPurchasedOwnedItems", METHOD_TYPE_OBJECT },
    { M_isPurchaseOwned, "isPurchaseOwned", METHOD_TYPE_INT },

    { M_loadTexture, "loadTexture", METHOD_TYPE_VOID },
    { M_copyAsset, "copyAsset", METHOD_TYPE_VOID },

    { M_preloadEffect, "preloadEffect", METHOD_TYPE_INT },
    { M_unloadEffect, "unloadEffect", METHOD_TYPE_VOID },
    { M_playEffect, "playEffect", METHOD_TYPE_VOID },
    { M_stopEffect, "stopEffect", METHOD_TYPE_VOID },
    { M_pauseEffect, "pauseEffect", METHOD_TYPE_VOID },
    { M_setEffectLooping, "setEffectLooping", METHOD_TYPE_VOID },
    { M_setEffectPitch, "setEffectPitch", METHOD_TYPE_VOID },
    { M_setEffectGain, "setEffectGain", METHOD_TYPE_VOID },
    { M_pauseAllSounds, "pauseAllSounds", METHOD_TYPE_VOID },
    { M_resumeAllSounds, "resumeAllSounds", METHOD_TYPE_VOID },
    { M_setEffectsVolume, "setEffectsVolume", METHOD_TYPE_VOID },
    { M_getEffectsVolume, "getEffectsVolume", METHOD_TYPE_FLOAT },

    { M_playBackgroundMusic, "playBackgroundMusic", METHOD_TYPE_VOID },
    { M_stopBackgroundMusic, "stopBackgroundMusic", METHOD_TYPE_VOID },
    { M_pauseBackgroundMusic, "pauseBackgroundMusic", METHOD_TYPE_VOID },
    { M_resumeBackgroundMusic, "resumeBackgroundMusic", METHOD_TYPE_VOID },
    { M_rewindBackgroundMusic, "rewindBackgroundMusic", METHOD_TYPE_VOID },
    { M_isBackgroundMusicPlaying, "isBackgroundMusicPlaying", METHOD_TYPE_BOOLEAN },
    { M_getBackgroundMusicVolume, "getBackgroundMusicVolume", METHOD_TYPE_FLOAT },
    { M_setBackgroundMusicVolume, "setBackgroundMusicVolume", METHOD_TYPE_VOID },
    { M_getDurationForSound, "getDurationForSound", METHOD_TYPE_INT },

    { M_setFPS, "setFPS", METHOD_TYPE_VOID },
    { M_exitApplication, "exitApplication", METHOD_TYPE_VOID },
    { M_showMessageBox, "showMessageBox", METHOD_TYPE_VOID },
    { M_hideMessageBox, "hideMessageBox", METHOD_TYPE_VOID },
    { M_closeMessageBox, "closeMessageBox", METHOD_TYPE_VOID },
    { M_keyboardInput_Show, "keyboardInput_Show", METHOD_TYPE_VOID },
    { M_keyboardInput_Show_withMaxLengthLocked, "keyboardInput_Show_withMaxLengthLocked", METHOD_TYPE_VOID },

    { M_keyboardInput_Hide, "keyboardInput_Hide", METHOD_TYPE_VOID },
    { M_setKeyboardInputPosition, "setKeyboardInputPosition", METHOD_TYPE_VOID },
    { M_setKeyboardInputStyle, "setKeyboardInputStyle", METHOD_TYPE_VOID },
    { M_setKeyboardInputVisible, "setKeyboardInputVisible", METHOD_TYPE_VOID },
    { M_setKeyboardInputVisibleDelayed, "setKeyboardInputVisibleDelayed", METHOD_TYPE_VOID },
    { M_setKeyboardInputText, "setKeyboardInputText", METHOD_TYPE_VOID },
    { M_setKeyboardInputMaxLength, "setKeyboardInputMaxLength", METHOD_TYPE_VOID },
    { M_setKeyboardInputSingleLine, "setKeyboardInputSingleLine", METHOD_TYPE_VOID },
    { M_setKeyboardInputTypePassword, "setKeyboardInputTypePassword", METHOD_TYPE_VOID },
    { M_callInAppGetJar, "callInAppGetJar", METHOD_TYPE_VOID },
    { M_pushInterstitial, "pushInterstitial", METHOD_TYPE_VOID },
    { M_showInterstitialMopubView, "showInterstitialMopubView", METHOD_TYPE_VOID },
    { M_showMiniclipView, "showMiniclipView", METHOD_TYPE_VOID },
    { M_enableAdsWithPositionForGameplay, "enableAdsWithPositionForGameplay", METHOD_TYPE_VOID },
    { M_enableAdsWithPositionForGameplayGivenWidth, "enableAdsWithPositionForGameplayGivenWidth", METHOD_TYPE_VOID },
    { M_enableAdsWithPositionForMenu, "enableAdsWithPositionForMenu", METHOD_TYPE_VOID },
    { M_enableAdsWithPositionForMenuGivenWidth, "enableAdsWithPositionForMenuGivenWidth", METHOD_TYPE_VOID },
    { M_enableAds, "enableAds", METHOD_TYPE_VOID },
    { M_disableAds, "disableAds", METHOD_TYPE_VOID },
    { M_hideAds, "hideAds", METHOD_TYPE_VOID },
    { M_showTapJoyOffers, "showTapJoyOffers", METHOD_TYPE_VOID },
    { M_spendTapPoints, "spendTapPoints", METHOD_TYPE_VOID },
    { M_setGameState, "setGameState", METHOD_TYPE_VOID },
    { M_openFeint_login, "openFeint_login", METHOD_TYPE_VOID },
    { M_openFeint_showDashboard, "openFeint_showDashboard", METHOD_TYPE_VOID },
    { M_openFeint_showAchievements, "openFeint_showAchievements", METHOD_TYPE_VOID },
    { M_openFeint_showLeaderboards, "openFeint_showLeaderboards", METHOD_TYPE_VOID },
    { M_openFeint_showLeaderboard, "openFeint_showLeaderboard", METHOD_TYPE_VOID },
    { M_openFeint_UnlockAchievement, "openFeint_UnlockAchievement", METHOD_TYPE_VOID },
    { M_openFeint_UpdateAchievement, "openFeint_UpdateAchievement", METHOD_TYPE_VOID },
    { M_openFeint_PostScore, "openFeint_PostScore", METHOD_TYPE_VOID },
    { M_openFeint_PostScoreWithString, "openFeint_PostScoreWithString", METHOD_TYPE_VOID },
    { M_startFlurrySession, "startFlurrySession", METHOD_TYPE_VOID },
    { M_logFlurryEvent, "logFlurryEvent", METHOD_TYPE_VOID },
    { M_endFlurryTimedEvent, "endFlurryTimedEvent", METHOD_TYPE_VOID },
    { M_custom_logEvent, "custom_logEvent", METHOD_TYPE_VOID },
    { M_flashScreen, "flashScreen", METHOD_TYPE_VOID },
    { M_updateNotificationStatus, "updateNotificationStatus", METHOD_TYPE_VOID },
    { M_vibrateForMS, "vibrateForMS", METHOD_TYPE_VOID },
    { M_showBigAd, "showBigAd", METHOD_TYPE_VOID },
    { M_loadFullScreenAd, "loadFullScreenAd", METHOD_TYPE_VOID },
    { M_showFullScreenAd, "showFullScreenAd", METHOD_TYPE_VOID },
    { M_enableNewsfeed, "enableNewsfeed", METHOD_TYPE_VOID },
    { M_disableNewsfeed, "disableNewsfeed", METHOD_TYPE_VOID },
    { M_showSimpleTapjoyFeatureAd, "showSimpleTapjoyFeatureAd", METHOD_TYPE_VOID },
    { M_showTJOfferDialog, "showTJOfferDialog", METHOD_TYPE_VOID },
    { M_showRotatedBanner, "showRotatedBanner", METHOD_TYPE_VOID },
    { M_hideRotatedBanner, "hideRotatedBanner", METHOD_TYPE_VOID },
    { M_showHorizontalBanner, "showHorizontalBanner", METHOD_TYPE_VOID },
    { M_hideHorizontalBanner, "hideHorizontalBanner", METHOD_TYPE_VOID },
    { M_faceBook_dialog, "faceBook_dialog", METHOD_TYPE_VOID },
    { M_faceBook_like, "faceBook_like", METHOD_TYPE_VOID },
    { M_faceBook_request, "faceBook_request", METHOD_TYPE_VOID },
    { M_faceBook_logout, "faceBook_logout", METHOD_TYPE_VOID },
    { M_faceBook_authorize, "faceBook_authorize", METHOD_TYPE_VOID },
    { M_showHtmlDialog, "showHtmlDialog", METHOD_TYPE_VOID },
    { M_setClipboardText, "setClipboardText", METHOD_TYPE_VOID },
    { M_hideAdsInSeconds, "hideAdsInSeconds", METHOD_TYPE_VOID },
    { M_permanentlyRemoveAds, "permanentlyRemoveAds", METHOD_TYPE_VOID },
    { M_Newsfeed_showDialog, "Newsfeed_showDialog", METHOD_TYPE_VOID },
    { M_sendEmail, "sendEmail", METHOD_TYPE_VOID },
    { M_sendSMS, "sendSMS", METHOD_TYPE_VOID },
    { M_sendGoogleEvent, "sendGoogleEvent", METHOD_TYPE_VOID },
    { M_custom_updateScore, "custom_updateScore", METHOD_TYPE_VOID },
    { M_custom_showLeaderboard, "custom_showLeaderboard", METHOD_TYPE_VOID },
    { M_custom_showLeaderboards, "custom_showLeaderboards", METHOD_TYPE_VOID },
    { M_custom_updateAchievement, "custom_updateAchievement", METHOD_TYPE_VOID },
    { M_custom_showAchievements, "custom_showAchievements", METHOD_TYPE_VOID },
    { M_NF_showBoard, "NF_showBoard", METHOD_TYPE_VOID },
    { M_NF_showUrgentBoard, "NF_showUrgentBoard", METHOD_TYPE_VOID },
    { M_NF_dismissBoard, "NF_dismissBoard", METHOD_TYPE_VOID },
    { M_NF_setSandBox, "NF_setSandBox", METHOD_TYPE_VOID },
    { M_NF_setShowBadge, "NF_setShowBadge", METHOD_TYPE_VOID },
    { M_showCheckpoint, "showCheckpoint", METHOD_TYPE_VOID },
    { M_showDatePickerDialog_JNI, "showDatePickerDialog_JNI", METHOD_TYPE_VOID },
    { M_openLink, "openLink", METHOD_TYPE_VOID },
    { M_openExternalURL, "openExternalURL", METHOD_TYPE_VOID },

    { M_getRecommendedPriceGetJar, "getRecommendedPriceGetJar", METHOD_TYPE_INT },
    { M_callInAppPurchase, "callInAppPurchase", METHOD_TYPE_INT },
    { M_callInAppPurchaseManaged, "callInAppPurchaseManaged", METHOD_TYPE_INT },
    { M_callInAppPurchaseRemoveAds, "callInAppPurchaseRemoveAds", METHOD_TYPE_INT },
    { M_callInAppPurchaseRemoveAdsManaged, "callInAppPurchaseRemoveAdsManaged", METHOD_TYPE_INT },
    { M_getPendingPurchaseSignatureAmount, "getPendingPurchaseSignatureAmount", METHOD_TYPE_INT },
    { M_callLastPendingPurchaseSignature, "callLastPendingPurchaseSignature", METHOD_TYPE_INT },
    { M_removeLastPendingPurchaseSignature, "removeLastPendingPurchaseSignature", METHOD_TYPE_INT },
    { M_getNotificationStatus, "getNotificationStatus", METHOD_TYPE_INT },
    { M_faceBook_isSessionValid, "faceBook_isSessionValid", METHOD_TYPE_INT },
    { M_Newsfeed_getNumber, "Newsfeed_getNumber", METHOD_TYPE_INT },
    { M_isAppInstalled, "isAppInstalled", METHOD_TYPE_INT },
    { M_checkForInstalledApp, "checkForInstalledApp", METHOD_TYPE_INT },

    { M_faceBook_getAccessToken, "faceBook_getAccessToken", METHOD_TYPE_OBJECT },
};

MethodsBoolean methodsBoolean[] = {
    { M_isSDcardPresent, stub_true },
    { M_isBackgroundMusicPlaying, isBackgroundMusicPlaying },
};

MethodsByte methodsByte[] = {};
MethodsChar methodsChar[] = {};
MethodsDouble methodsDouble[] = {};

MethodsFloat methodsFloat[] = {
    { M_availableMemoryOnDevice, availableMemoryOnDevice },
    { M_availableMemoryOnSDcard, availableMemoryOnSDcard },
    { M_getEffectsVolume, getEffectsVolume },
    { M_getBackgroundMusicVolume, getBackgroundMusicVolume },
};

MethodsInt methodsInt[] = {
    { M_getAndroidVersion, getAndroidVersion },
    { M_getAppVersionCode, getAppVersionCode },
    { M_DeviceSupportsMultitouch, DeviceSupportsMultitouch },
    { M_SharedPreferences_getInt, SharedPreferences_getInt },
    { M_isFullVersion, isFullVersion },
    { M_getAdsDisabled, getAdsDisabled },
    { M_getPendingPurchaseAmount, getPendingPurchaseAmount },
    { M_isPurchaseOwned, stub_int0 },
    { M_preloadEffect, preloadEffect },
    { M_getDurationForSound, getDurationForSound },

    { M_getRecommendedPriceGetJar, stub_int0 },
    { M_callInAppPurchase, stub_int0 },
    { M_callInAppPurchaseManaged, stub_int0 },
    { M_callInAppPurchaseRemoveAds, stub_int0 },
    { M_callInAppPurchaseRemoveAdsManaged, stub_int0 },
    { M_getPendingPurchaseSignatureAmount, stub_int0 },
    { M_callLastPendingPurchaseSignature, stub_int0 },
    { M_removeLastPendingPurchaseSignature, stub_int0 },
    { M_getNotificationStatus, stub_int0 },
    { M_faceBook_isSessionValid, stub_int0 },
    { M_Newsfeed_getNumber, stub_int0 },
    { M_isAppInstalled, stub_int0 },
    { M_checkForInstalledApp, stub_int0 },
};

MethodsLong methodsLong[] = {};

MethodsObject methodsObject[] = {
    { M_getDeviceId, getDeviceId },
    { M_SharedPreferences_getString, SharedPreferences_getString },
    { M_getPurchasedOwnedItems, getPurchasedOwnedItems },
    { M_faceBook_getAccessToken, stub_empty_string },
};

MethodsShort methodsShort[] = {};

MethodsVoid methodsVoid[] = {
    { M_SharedPreferences_setInt, SharedPreferences_setInt },
    { M_SharedPreferences_setString, SharedPreferences_setString },
    { M_setFullVersion, setFullVersion },
    { M_setPendingPurchaseAmount, setPendingPurchaseAmount },
    { M_loadTexture, loadTexture },
    { M_copyAsset, copyAsset },
    { M_unloadEffect, unloadEffect },
    { M_playEffect, playEffect },
    { M_stopEffect, stopEffect },
    { M_pauseEffect, pauseEffect },
    { M_setEffectLooping, setEffectLooping },
    { M_setEffectPitch, setEffectPitch },
    { M_setEffectGain, setEffectGain },
    { M_pauseAllSounds, pauseAllSounds },
    { M_resumeAllSounds, resumeAllSounds },
    { M_setEffectsVolume, setEffectsVolume },
    { M_playBackgroundMusic, playBackgroundMusic },
    { M_stopBackgroundMusic, stopBackgroundMusic },
    { M_pauseBackgroundMusic, pauseBackgroundMusic },
    { M_resumeBackgroundMusic, resumeBackgroundMusic },
    { M_rewindBackgroundMusic, rewindBackgroundMusic },
    { M_setBackgroundMusicVolume, setBackgroundMusicVolume },
    { M_setFPS, setFPS },
    { M_exitApplication, exitApplication },
    { M_showMessageBox, showMessageBox },
    { M_hideMessageBox, closeMessageBox },
    { M_closeMessageBox, closeMessageBox },
    { M_keyboardInput_Show, keyboardInput_Show },
    { M_keyboardInput_Show_withMaxLengthLocked, keyboardInput_Show },

    { M_keyboardInput_Hide, stub_void },
    { M_setKeyboardInputPosition, stub_void },
    { M_setKeyboardInputStyle, stub_void },
    { M_setKeyboardInputVisible, stub_void },
    { M_setKeyboardInputVisibleDelayed, stub_void },
    { M_setKeyboardInputText, stub_void },
    { M_setKeyboardInputMaxLength, stub_void },
    { M_setKeyboardInputSingleLine, stub_void },
    { M_setKeyboardInputTypePassword, stub_void },
    { M_callInAppGetJar, stub_void },
    { M_pushInterstitial, stub_void },
    { M_showInterstitialMopubView, stub_void },
    { M_showMiniclipView, stub_void },
    { M_enableAdsWithPositionForGameplay, stub_void },
    { M_enableAdsWithPositionForGameplayGivenWidth, stub_void },
    { M_enableAdsWithPositionForMenu, stub_void },
    { M_enableAdsWithPositionForMenuGivenWidth, stub_void },
    { M_enableAds, stub_void },
    { M_disableAds, stub_void },
    { M_hideAds, stub_void },
    { M_showTapJoyOffers, stub_void },
    { M_spendTapPoints, stub_void },
    { M_setGameState, setGameState },
    { M_openFeint_login, stub_void },
    { M_openFeint_showDashboard, stub_void },
    { M_openFeint_showAchievements, stub_void },
    { M_openFeint_showLeaderboards, stub_void },
    { M_openFeint_showLeaderboard, stub_void },
    { M_openFeint_UnlockAchievement, openFeint_UnlockAchievement },
    { M_openFeint_UpdateAchievement, updateAchievement },
    { M_openFeint_PostScore, stub_void },
    { M_openFeint_PostScoreWithString, stub_void },
    { M_startFlurrySession, stub_void },
    { M_logFlurryEvent, stub_void },
    { M_endFlurryTimedEvent, stub_void },
    { M_custom_logEvent, stub_void },
    { M_flashScreen, stub_void },
    { M_updateNotificationStatus, stub_void },
    { M_vibrateForMS, stub_void },
    { M_showBigAd, stub_void },
    { M_loadFullScreenAd, stub_void },
    { M_showFullScreenAd, stub_void },
    { M_enableNewsfeed, stub_void },
    { M_disableNewsfeed, stub_void },
    { M_showSimpleTapjoyFeatureAd, stub_void },
    { M_showTJOfferDialog, stub_void },
    { M_showRotatedBanner, stub_void },
    { M_hideRotatedBanner, stub_void },
    { M_showHorizontalBanner, stub_void },
    { M_hideHorizontalBanner, stub_void },
    { M_faceBook_dialog, stub_void },
    { M_faceBook_like, stub_void },
    { M_faceBook_request, stub_void },
    { M_faceBook_logout, stub_void },
    { M_faceBook_authorize, stub_void },
    { M_showHtmlDialog, stub_void },
    { M_setClipboardText, stub_void },
    { M_hideAdsInSeconds, stub_void },
    { M_permanentlyRemoveAds, stub_void },
    { M_Newsfeed_showDialog, stub_void },
    { M_sendEmail, stub_void },
    { M_sendSMS, stub_void },
    { M_sendGoogleEvent, stub_void },
    { M_custom_updateScore, stub_void },
    { M_custom_showLeaderboard, stub_void },
    { M_custom_showLeaderboards, stub_void },
    { M_custom_updateAchievement, updateAchievement },
    { M_custom_showAchievements, stub_void },
    { M_NF_showBoard, stub_void },
    { M_NF_showUrgentBoard, stub_void },
    { M_NF_dismissBoard, stub_void },
    { M_NF_setSandBox, stub_void },
    { M_NF_setShowBadge, stub_void },
    { M_showCheckpoint, stub_void },
    { M_showDatePickerDialog_JNI, stub_void },
    { M_openLink, stub_void },
    { M_openExternalURL, stub_void },
};

/*
 * Fields. The game doesn't read any Java fields as far as we know; keep the
 * Android SDK level around in case it ever asks for Build.VERSION.SDK_INT.
 */

NameToFieldID nameToFieldId[] = {
    { 1, "SDK_INT", FIELD_TYPE_INT },
};

FieldsBoolean fieldsBoolean[] = {};
FieldsByte fieldsByte[] = {};
FieldsChar fieldsChar[] = {};
FieldsDouble fieldsDouble[] = {};
FieldsFloat fieldsFloat[] = {};
FieldsInt fieldsInt[] = {
    { 1, 10 },
};
FieldsObject fieldsObject[] = {};
FieldsLong fieldsLong[] = {};
FieldsShort fieldsShort[] = {};

__FALSOJNI_IMPL_CONTAINER_SIZES
