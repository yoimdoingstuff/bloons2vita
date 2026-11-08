/*
 * coco.h
 *
 * Native entry points exported by libbloons2.so for the Java class
 * com.miniclip.nativeJNI.CocoJNI, resolved once after the .so is loaded.
 *
 * This software may be modified and distributed under the terms
 * of the GPLv3 license. See the LICENSE file for details.
 */

#ifndef BLOONS2_COCO_H
#define BLOONS2_COCO_H

#include <falso_jni/FalsoJNI.h>

#ifdef __cplusplus
extern "C" {
#endif

// Dummy handles we hand to the game in place of real Java objects
#define COCO_CLASS    ((jclass)0x42424242)
#define COCO_ACTIVITY ((jobject)0x41414141)

typedef struct {
    // Lifecycle / rendering
    void (*Mrun)(JNIEnv *env, jclass clazz);
    void (*Mrender)(JNIEnv *env, jclass clazz);
    void (*Mend)(JNIEnv *env, jclass clazz);
    void (*Mpause)(JNIEnv *env, jclass clazz);
    void (*Mresume)(JNIEnv *env, jclass clazz);
    void (*MlowMemory)(JNIEnv *env, jclass clazz);
    void (*MpreserveContext)(JNIEnv *env, jclass clazz, jint preserve);
    void (*MrestoreContext)(JNIEnv *env, jclass clazz);
    void (*MsetGamePause)(JNIEnv *env, jclass clazz, jint paused);

    // Setup
    void (*MsetMainActivity)(JNIEnv *env, jclass clazz, jobject activity);
    void (*MsetCountryCode)(JNIEnv *env, jclass clazz, jstring code);
    void (*MsetAppVersionNumber)(JNIEnv *env, jclass clazz, jstring version);
    void (*MsetFilesPath)(JNIEnv *env, jclass clazz, jstring files, jstring external);
    void (*MsetAPKPath)(JNIEnv *env, jclass clazz, jstring apk);
    void (*MsetDensity)(JNIEnv *env, jclass clazz, jfloat density);
    void (*MsetIsRetina)(JNIEnv *env, jclass clazz, jint retina);
    void (*MdisplayInfo)(JNIEnv *env, jclass clazz, jint w, jint h);
    void (*MrealDisplayInfo)(JNIEnv *env, jclass clazz, jint w, jint h);

    // Input
    void (*MtouchesBegin)(JNIEnv *env, jclass clazz, jint id, jfloat x, jfloat y);
    void (*MtouchesEnd)(JNIEnv *env, jclass clazz, jint id, jfloat x, jfloat y);
    void (*MtouchesMove)(JNIEnv *env, jclass clazz, jintArray ids, jfloatArray xs, jfloatArray ys);
    void (*MtouchesCancel)(JNIEnv *env, jclass clazz, jintArray ids, jfloatArray xs, jfloatArray ys);
    void (*MpressedBackButton)(JNIEnv *env, jclass clazz);
    void (*MpressedKey)(JNIEnv *env, jclass clazz, jint keycode, jint down);

    // Java -> native callbacks
    void (*MnativeInitBitmap)(JNIEnv *env, jclass clazz, jint w, jint h, jbyteArray pixels);
    void (*MonMessageBoxButtonPressed)(JNIEnv *env, jclass clazz, jint id, jint button);
    void (*MkeyboardInputResponse)(JNIEnv *env, jclass clazz, jstring text);
} CocoJNI;

extern CocoJNI coco;

/** Resolve all natives from the loaded module. Fatal error if a required one is missing. */
void coco_resolve(void);

#ifdef __cplusplus
};
#endif

#endif // BLOONS2_COCO_H
