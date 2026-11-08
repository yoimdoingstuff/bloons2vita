/*
 * input.c
 *
 * Touchscreen passthrough plus native-style button controls (see input.h).
 *
 * Bloons 2 computes a throw from the drag between where the finger first
 * touches and where it is released (the player code logs "First touch",
 * "Last touch", "Delta", "Angle" and "Power"). So inside a level the left
 * stick drives a virtual finger as a slingshot: tilting it presses a finger
 * down at a fixed anchor and pulls it by the stick vector, and the game draws
 * its own aiming arrow as on Android. Cross lets go (throws). Cancelling
 * eases the finger back onto the anchor before lifting it, i.e. a drag of
 * zero length, which throws nothing.
 *
 * game/context.c tells us whether a level is showing, so the stick never
 * presses anything in menus.
 *
 * This software may be modified and distributed under the terms
 * of the GPLv3 license. See the LICENSE file for details.
 */

#include "game/input.h"

#include <math.h>
#include <stdbool.h>
#include <string.h>

#include <psp2/ctrl.h>
#include <psp2/kernel/processmgr.h>
#include <psp2/touch.h>

#include <falso_jni/FalsoJNI.h>
#include <falso_jni/FalsoJNI_ImplBridge.h>

#include "game/coco.h"
#include "game/context.h"
#include "game/settings.h"
#include "utils/logger.h"

#define SCREEN_W 960.0f
#define SCREEN_H 544.0f

/*
 * Tuning
 */

#define STICK_DEADZONE     0.20f
#define AIM_ANCHOR_X       (SCREEN_W / 2)
#define AIM_ANCHOR_Y       (SCREEN_H / 2)
#define AIM_RADIUS         250.0f   // px pulled by a full stick tilt at 100% strength
#define AIM_MIN_PULL       12.0f    // px; shorter pulls are treated as "no aim"
#define AIM_SMOOTHING      0.45f    // fraction of the way to the stick target per frame
#define FINE_ANGLE_STEP    0.5f     // degrees per d-pad tick
#define FINE_POWER_STEP    0.02f    // fraction of the full pull per d-pad tick
#define FINE_ANGLE_SPEED   35.0f    // degrees/s, right stick fully tilted
#define FINE_POWER_SPEED   0.45f    // full pulls/s, right stick fully tilted
#define DPAD_REPEAT_DELAY  0.30f    // s
#define DPAD_REPEAT_RATE   0.05f    // s
#define SWIPE_DISTANCE     320.0f   // px
#define SWIPE_FRAMES       9

/*
 * Pointers
 *
 * Android pointer IDs are small reused integers; the Vita reports arbitrary
 * 0-127 IDs, so we hand out our own. The last slot is the virtual finger.
 */

#define MAX_POINTERS 6
#define VIRTUAL_SLOT (MAX_POINTERS - 1)

typedef struct {
    bool active;
    int vita_id;
    float x, y;
} Pointer;

static Pointer pointers[MAX_POINTERS];
static bool pointers_moved = false;

static void pointer_down(int slot, float x, float y) {
    pointers[slot].active = true;
    pointers[slot].x = x;
    pointers[slot].y = y;
    coco.MtouchesBegin(&jni, COCO_CLASS, slot, x, y);
}

static void pointer_move(int slot, float x, float y) {
    if (pointers[slot].x != x || pointers[slot].y != y) {
        pointers[slot].x = x;
        pointers[slot].y = y;
        pointers_moved = true;
    }
}

static void flush_moves(void) {
    if (!pointers_moved) return;
    pointers_moved = false;

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

static void pointer_up(int slot) {
    if (!pointers[slot].active) return;
    flush_moves(); // the game must see the final position before the release
    pointers[slot].active = false;
    coco.MtouchesEnd(&jni, COCO_CLASS, slot, pointers[slot].x, pointers[slot].y);
}

/*
 * Aiming state
 */

typedef enum {
    AIM_IDLE,       // no virtual finger
    AIM_HOLDING,    // finger down at the anchor, pulled by aim_x/aim_y
    AIM_QUICK_DOWN, // Precise mode: Cross without aiming re-throws the last
    AIM_QUICK_PULL, //   shot: down -> pull -> release over three frames
    AIM_CANCEL,     // finger moved back onto the anchor, lift next frame
} AimPhase;

static AimPhase aim_phase = AIM_IDLE;
static float aim_x = 0.0f, aim_y = 0.0f;   // current pull (px, before inversion)
static float last_x = 0.0f, last_y = 0.0f; // Precise mode: last shot
static bool have_last = false;

static int swipe_frame = -1;
static float swipe_dir = 0.0f;

static uint32_t old_buttons = 0;
static float dpad_hold = 0.0f, dpad_next = 0.0f;
static uint64_t last_time = 0;

static float clampf(float v, float lo, float hi) {
    return v < lo ? lo : (v > hi ? hi : v);
}

static float aim_radius(void) {
    return AIM_RADIUS * (float)settings.aim_strength / 100.0f;
}

static void finger_position(float *x, float *y) {
    float s = settings.invert_aim ? -1.0f : 1.0f;
    *x = clampf(AIM_ANCHOR_X + aim_x * s, 0.0f, SCREEN_W - 1.0f);
    *y = clampf(AIM_ANCHOR_Y + aim_y * s, 0.0f, SCREEN_H - 1.0f);
}

static bool aim_allowed(void) {
    return context_get() != CONTEXT_MENU && !context_paused();
}

static void aim_begin(float x, float y) {
    aim_x = x;
    aim_y = y;
    pointer_down(VIRTUAL_SLOT, AIM_ANCHOR_X, AIM_ANCHOR_Y);
    aim_phase = AIM_HOLDING;
}

static void aim_cancel(void) {
    if (aim_phase == AIM_IDLE || aim_phase == AIM_CANCEL) return;
    if (!pointers[VIRTUAL_SLOT].active) {
        aim_phase = AIM_IDLE;
        return;
    }
    // A zero-length drag doesn't throw anything
    aim_x = aim_y = 0.0f;
    pointer_move(VIRTUAL_SLOT, AIM_ANCHOR_X, AIM_ANCHOR_Y);
    aim_phase = AIM_CANCEL;
}

static void aim_throw(void) {
    float len = hypotf(aim_x, aim_y);
    if (len < AIM_MIN_PULL) {
        aim_cancel();
        return;
    }

    float fx, fy;
    finger_position(&fx, &fy);
    pointer_move(VIRTUAL_SLOT, fx, fy);
    pointer_up(VIRTUAL_SLOT);

    if (settings.aim_mode == AIM_MODE_PRECISE) {
        last_x = aim_x;
        last_y = aim_y;
        have_last = true;
    }
    aim_x = aim_y = 0.0f;
    aim_phase = AIM_IDLE;
}

// Rotate the pull by `deg` degrees and scale its length by `dlen` px.
static void aim_adjust(float deg, float dlen) {
    float len = hypotf(aim_x, aim_y);
    float ang = atan2f(aim_y, aim_x);
    if (len < AIM_MIN_PULL) {
        // Nothing to adjust yet: start from a gentle pull to the left
        len = aim_radius() * 0.5f;
        ang = (float)M_PI;
    }
    ang += deg * (float)M_PI / 180.0f;
    len = clampf(len + dlen, AIM_MIN_PULL, aim_radius());
    aim_x = cosf(ang) * len;
    aim_y = sinf(ang) * len;
}

static float stick_axis(uint8_t raw) {
    return clampf(((float)raw - 128.0f) / 127.0f, -1.0f, 1.0f);
}

// Radial deadzone; returns the remapped vector (length 0..1).
static void stick_vector(uint8_t rx, uint8_t ry, float *x, float *y) {
    float vx = stick_axis(rx), vy = stick_axis(ry);
    float m = hypotf(vx, vy);
    if (m < STICK_DEADZONE) {
        *x = *y = 0.0f;
        return;
    }
    float k = clampf((m - STICK_DEADZONE) / (1.0f - STICK_DEADZONE), 0.0f, 1.0f) / m;
    *x = vx * k;
    *y = vy * k;
}

/*
 * Touchscreen
 */

static void poll_touch(void) {
    SceTouchData touch;
    if (sceTouchPeek(SCE_TOUCH_PORT_FRONT, &touch, 1) < 0)
        return;

    // Releases
    for (int i = 0; i < VIRTUAL_SLOT; i++) {
        if (!pointers[i].active) continue;
        bool still_down = false;
        for (int r = 0; r < touch.reportNum; r++) {
            if (touch.report[r].id == pointers[i].vita_id) {
                still_down = true;
                break;
            }
        }
        if (!still_down) pointer_up(i);
    }

    // Presses and moves
    for (int r = 0; r < touch.reportNum; r++) {
        float x = (float)touch.report[r].x * SCREEN_W / 1920.0f;
        float y = (float)touch.report[r].y * SCREEN_H / 1088.0f;

        int slot = -1;
        for (int i = 0; i < VIRTUAL_SLOT; i++) {
            if (pointers[i].active && pointers[i].vita_id == touch.report[r].id) {
                slot = i;
                break;
            }
        }

        if (slot >= 0) {
            pointer_move(slot, x, y);
            continue;
        }

        for (int i = 0; i < VIRTUAL_SLOT; i++) {
            if (!pointers[i].active) {
                slot = i;
                break;
            }
        }
        if (slot < 0) continue;

        // A real finger takes over from a stick aim in progress
        aim_cancel();
        pointers[slot].vita_id = touch.report[r].id;
        pointer_down(slot, x, y);
    }
}

/*
 * Buttons
 */

static void back(void) {
    if (aim_phase != AIM_IDLE) {
        aim_cancel();
        return;
    }
    if (coco.MpressedBackButton) {
        context_back_pressed();
        coco.MpressedBackButton(&jni, COCO_CLASS);
    }
}

static void poll_pad(float dt) {
    SceCtrlData pad;
    if (sceCtrlPeekBufferPositive(0, &pad, 1) < 0)
        return;

    uint32_t now = pad.buttons;
    uint32_t pressed = now & ~old_buttons;
    old_buttons = now;

    // Back (Start / Circle). While aiming, the first press only cancels the
    // aim, so the cancelled finger can never land on the pause menu.
    if (pressed & (SCE_CTRL_START | SCE_CTRL_CIRCLE))
        back();

    // L / R: swipe through zones / spin the planet. Only on screens known to
    // be menus: inside a level a drag would throw a dart.
    if (swipe_frame < 0 && aim_phase == AIM_IDLE && context_get() == CONTEXT_MENU) {
        if (pressed & SCE_CTRL_LTRIGGER) { swipe_dir = 1.0f;  swipe_frame = 0; }
        if (pressed & SCE_CTRL_RTRIGGER) { swipe_dir = -1.0f; swipe_frame = 0; }
    }

    if (swipe_frame >= 0)
        return;

    bool allowed = aim_allowed();
    if (!allowed) {
        aim_cancel();
        return;
    }

    bool precise = settings.aim_mode == AIM_MODE_PRECISE;

    // Left stick: the pull. Letting go keeps the current aim.
    float sx, sy;
    stick_vector(pad.lx, pad.ly, &sx, &sy);
    if (sx != 0.0f || sy != 0.0f) {
        float tx = sx * aim_radius(), ty = sy * aim_radius();
        if (aim_phase == AIM_IDLE) {
            aim_begin(tx, ty);
        } else if (aim_phase == AIM_HOLDING) {
            aim_x += (tx - aim_x) * AIM_SMOOTHING;
            aim_y += (ty - aim_y) * AIM_SMOOTHING;
        }
    }

    if (precise) {
        // D-pad: fine tuning with key repeat
        float da = 0.0f, dp = 0.0f;
        if (now & SCE_CTRL_LEFT)  da -= FINE_ANGLE_STEP;
        if (now & SCE_CTRL_RIGHT) da += FINE_ANGLE_STEP;
        if (now & SCE_CTRL_UP)    dp += FINE_POWER_STEP;
        if (now & SCE_CTRL_DOWN)  dp -= FINE_POWER_STEP;

        uint32_t dpad = SCE_CTRL_LEFT | SCE_CTRL_RIGHT | SCE_CTRL_UP | SCE_CTRL_DOWN;
        if (da != 0.0f || dp != 0.0f) {
            bool fire = false;
            if (pressed & dpad) {
                fire = true;
                dpad_hold = 0.0f;
                dpad_next = DPAD_REPEAT_DELAY;
            } else {
                dpad_hold += dt;
                if (dpad_hold >= dpad_next) {
                    fire = true;
                    dpad_next += DPAD_REPEAT_RATE;
                }
            }

            if (fire) {
                if (aim_phase == AIM_IDLE)
                    aim_begin(have_last ? last_x : 0.0f, have_last ? last_y : 0.0f);
                if (aim_phase == AIM_HOLDING)
                    aim_adjust(da, dp * aim_radius());
            }
        }

        // Right stick: analog fine tuning
        float fx, fy;
        stick_vector(pad.rx, pad.ry, &fx, &fy);
        if (fx != 0.0f || fy != 0.0f) {
            if (aim_phase == AIM_IDLE)
                aim_begin(have_last ? last_x : 0.0f, have_last ? last_y : 0.0f);
            if (aim_phase == AIM_HOLDING)
                aim_adjust(fx * FINE_ANGLE_SPEED * dt, -fy * FINE_POWER_SPEED * aim_radius() * dt);
        }
    }

    // Cross: throw
    if (pressed & SCE_CTRL_CROSS) {
        if (aim_phase == AIM_HOLDING) {
            aim_throw();
        } else if (aim_phase == AIM_IDLE && precise && have_last) {
            // Repeat the last shot
            aim_x = last_x;
            aim_y = last_y;
            pointer_down(VIRTUAL_SLOT, AIM_ANCHOR_X, AIM_ANCHOR_Y);
            aim_phase = AIM_QUICK_DOWN;
        }
    }
}

static void update_aim(void) {
    switch (aim_phase) {
        case AIM_HOLDING: {
            float fx, fy;
            finger_position(&fx, &fy);
            pointer_move(VIRTUAL_SLOT, fx, fy);
            break;
        }
        case AIM_QUICK_DOWN: {
            float fx, fy;
            finger_position(&fx, &fy);
            pointer_move(VIRTUAL_SLOT, fx, fy);
            aim_phase = AIM_QUICK_PULL;
            break;
        }
        case AIM_QUICK_PULL:
            aim_phase = AIM_HOLDING;
            aim_throw();
            break;
        case AIM_CANCEL:
            pointer_up(VIRTUAL_SLOT);
            aim_phase = AIM_IDLE;
            break;
        default:
            break;
    }
}

static void update_swipe(void) {
    if (swipe_frame < 0) return;

    float start_x = SCREEN_W / 2 - swipe_dir * SWIPE_DISTANCE / 2;
    float y = SCREEN_H / 2;
    float t = (float)swipe_frame / (float)SWIPE_FRAMES;
    float x = start_x + swipe_dir * SWIPE_DISTANCE * t;

    if (swipe_frame == 0) {
        pointer_down(VIRTUAL_SLOT, x, y);
    } else if (swipe_frame <= SWIPE_FRAMES) {
        pointer_move(VIRTUAL_SLOT, x, y);
    } else {
        pointer_up(VIRTUAL_SLOT);
        swipe_frame = -1;
        return;
    }
    swipe_frame++;
}

/*
 * Public API
 */

void input_init(void) {
    sceCtrlSetSamplingMode(SCE_CTRL_MODE_ANALOG_WIDE);
    sceTouchSetSamplingState(SCE_TOUCH_PORT_FRONT, SCE_TOUCH_SAMPLING_STATE_START);
    memset(pointers, 0, sizeof(pointers));
    last_time = sceKernelGetProcessTimeWide();
}

void input_update(void) {
    uint64_t now = sceKernelGetProcessTimeWide();
    float dt = (float)(now - last_time) / 1000000.0f;
    last_time = now;
    if (dt > 0.1f) dt = 0.1f;

    poll_touch();
    poll_pad(dt);
    update_aim();
    update_swipe();
    flush_moves();
}

void input_release_all(void) {
    // Never throw because a dialog popped up: cancel the aim first
    if (aim_phase != AIM_IDLE && pointers[VIRTUAL_SLOT].active) {
        pointer_move(VIRTUAL_SLOT, AIM_ANCHOR_X, AIM_ANCHOR_Y);
        flush_moves();
    }
    aim_phase = AIM_IDLE;
    aim_x = aim_y = 0.0f;
    swipe_frame = -1;
    for (int i = 0; i < MAX_POINTERS; i++)
        pointer_up(i);
}

void input_resume(void) {
    // Buttons still held from closing a system dialog must not count as new
    // presses in the game.
    SceCtrlData pad;
    if (sceCtrlPeekBufferPositive(0, &pad, 1) >= 0)
        old_buttons = pad.buttons;
    last_time = sceKernelGetProcessTimeWide();
}
