/*
 * input.h
 *
 * Touchscreen passthrough plus native-style button controls:
 *
 * In a level (the stick is a slingshot, like Angry Birds on PSP):
 *   Left stick       pull to aim; the aim stays when you let go
 *   Cross            throw
 *   Circle           cancel the aim (Back when not aiming)
 *   D-pad            [Precise mode] fine-tune: left/right angle, up/down power
 *   Right stick      [Precise mode] analog fine-tuning
 *
 * Everywhere:
 *   Start            Back (pause menu in a level, quit prompt in menus)
 *   Circle           Back
 *   L / R            swipe left / right in menus (zones, planet)
 *
 * The aiming mode is chosen in settings.txt (see game/settings.h).
 *
 * This software may be modified and distributed under the terms
 * of the GPLv3 license. See the LICENSE file for details.
 */

#ifndef BLOONS2_INPUT_H
#define BLOONS2_INPUT_H

#ifdef __cplusplus
extern "C" {
#endif

/** Call on the game thread after the game has started. */
void input_init(void);

/** Poll input and forward it to the game. Call before Mrender. */
void input_update(void);

/** Lift every finger (e.g. when a system dialog opens). */
void input_release_all(void);

/** Call when a system dialog closes, before the next input_update(). */
void input_resume(void);

#ifdef __cplusplus
};
#endif

#endif // BLOONS2_INPUT_H
