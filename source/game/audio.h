/*
 * audio.h
 *
 * Re-implementation of the game's Java audio classes (cocosound, a SoundPool
 * wrapper, and cocomusic, a MediaPlayer wrapper) on top of SceAudioOut.
 *
 * This software may be modified and distributed under the terms
 * of the GPLv3 license. See the LICENSE file for details.
 */

#ifndef BLOONS2_AUDIO_H
#define BLOONS2_AUDIO_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

int audio_init(void);

/* Sound effects (cocosound). Sound IDs are > 0; -1 means failure. */
int audio_preload_effect(const char *path);
void audio_unload_effect(const char *path);
void audio_play_effect(int sound_id, float pitch, float pan, bool loop, float gain);
void audio_stop_effect(int sound_id);
void audio_pause_effect(int sound_id);
void audio_set_effect_looping(int sound_id, bool loop);
void audio_set_effect_pitch(int sound_id, float pitch);
void audio_set_effect_gain(int sound_id, float gain);
void audio_pause_all_effects(void);
void audio_resume_all_effects(void);
void audio_set_effects_volume(float volume);
float audio_get_effects_volume(void);

/* Background music (cocomusic). */
void audio_play_music(const char *path, bool loop);
void audio_stop_music(void);
void audio_pause_music(void);
void audio_resume_music(void);
void audio_rewind_music(void);
bool audio_is_music_playing(void);
void audio_set_music_volume(float volume);
float audio_get_music_volume(void);

/** Duration of a sound/music asset in milliseconds, or -1. */
int audio_get_duration_ms(const char *path);

#ifdef __cplusplus
};
#endif

#endif // BLOONS2_AUDIO_H
