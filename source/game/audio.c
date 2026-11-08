/*
 * audio.c
 *
 * Re-implementation of the game's Java audio classes (cocosound, a SoundPool
 * wrapper, and cocomusic, a MediaPlayer wrapper) on top of SceAudioOut.
 *
 * Effects are decoded to PCM once on preload; music is streamed from memory.
 * Everything is mixed in software on a dedicated thread at 44.1 kHz stereo.
 *
 * This software may be modified and distributed under the terms
 * of the GPLv3 license. See the LICENSE file for details.
 */

#include "game/audio.h"

#include <malloc.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <psp2/audioout.h>
#include <psp2/kernel/threadmgr.h>

#define DR_MP3_IMPLEMENTATION
#define DR_MP3_NO_STDIO
#include <dr_libs/dr_mp3.h>

#include "game/apk.h"
#include "utils/logger.h"

#define MIX_RATE     44100
#define MIX_GRAIN    512    // frames per SceAudioOut call
#define MAX_SOUNDS   128
#define MUSIC_CHUNK  2048   // frames decoded per music refill

typedef struct {
    char *path;
    int16_t *pcm;          // interleaved
    uint32_t frames;
    int channels;
    int rate;

    // Playback state. Like SoundPool as the game drives it (playEffect always
    // stops the previous stream of the same sound first), each sound has at
    // most one voice.
    bool playing;
    bool paused;
    bool loop;
    double pos;            // in source frames
    float pitch;
    float gain;
    float pan;
} Sound;

typedef struct {
    uint8_t *file;         // whole mp3 kept in memory for streaming
    size_t file_size;
    drmp3 mp3;
    bool open;
    bool playing;
    bool paused;
    bool loop;
    char path[256];

    // Decoded chunk; index 0 holds the last frame of the previous chunk so
    // linear interpolation works across chunk boundaries.
    int16_t buf[(MUSIC_CHUNK + 1) * 2];
    uint32_t buf_frames;   // valid frames including the carried-over one
    double pos;
} Music;

static Sound sounds[MAX_SOUNDS];
static Music music;
static float effects_volume = 1.0f;
static float music_volume = 1.0f;
static bool effects_paused_all = false;

static SceKernelLwMutexWork audio_mutex;

static inline void lock(void)   { sceKernelLockLwMutex(&audio_mutex, 1, NULL); }
static inline void unlock(void) { sceKernelUnlockLwMutex(&audio_mutex, 1); }

static inline float clampf(float v, float lo, float hi) {
    return v < lo ? lo : (v > hi ? hi : v);
}

static inline int16_t sample_at(const int16_t *pcm, int channels, uint32_t frame, int ch) {
    return pcm[frame * channels + (channels == 1 ? 0 : ch)];
}

/*
 * Decoding
 */

static bool decode_asset(const char *path, int16_t **pcm, uint32_t *frames,
                         int *channels, int *rate) {
    uint8_t *data;
    size_t size;

    if (!asset_load(path, &data, &size)) {
        // cocosound retries with the extension swapped to .wav / .mp3
        char alt[256];
        snprintf(alt, sizeof(alt), "%s", path);
        char *dot = strrchr(alt, '.');
        if (!dot || strcasecmp(dot, ".mp3") == 0) return false;
        snprintf(dot, sizeof(alt) - (dot - alt), ".mp3");
        if (!asset_load(alt, &data, &size)) return false;
    }

    drmp3_config cfg;
    drmp3_uint64 n = 0;
    drmp3_int16 *out = drmp3_open_memory_and_read_pcm_frames_s16(data, size, &cfg, &n, NULL);
    free(data);

    if (!out || n == 0) {
        l_error("audio: failed to decode %s", path);
        if (out) drmp3_free(out, NULL);
        return false;
    }

    *pcm = out;
    *frames = (uint32_t)n;
    *channels = (int)cfg.channels;
    *rate = (int)cfg.sampleRate;
    return true;
}

/*
 * Effects
 */

static int find_sound_by_path(const char *path) {
    for (int i = 0; i < MAX_SOUNDS; i++)
        if (sounds[i].path && strcmp(sounds[i].path, path) == 0)
            return i;
    return -1;
}

static Sound *get_sound(int sound_id) {
    int i = sound_id - 1;
    if (i < 0 || i >= MAX_SOUNDS || !sounds[i].pcm)
        return NULL;
    return &sounds[i];
}

int audio_preload_effect(const char *path) {
    if (!path) return -1;

    lock();
    int existing = find_sound_by_path(path);
    unlock();
    if (existing >= 0)
        return existing + 1;

    int16_t *pcm;
    uint32_t frames;
    int channels, rate;
    if (!decode_asset(path, &pcm, &frames, &channels, &rate))
        return -1;

    lock();
    int slot = -1;
    for (int i = 0; i < MAX_SOUNDS; i++) {
        if (!sounds[i].path) { slot = i; break; }
    }
    if (slot < 0) {
        unlock();
        drmp3_free(pcm, NULL);
        l_error("audio: too many sounds loaded, can't load %s", path);
        return -1;
    }

    Sound *s = &sounds[slot];
    memset(s, 0, sizeof(*s));
    s->path = strdup(path);
    s->pcm = pcm;
    s->frames = frames;
    s->channels = channels;
    s->rate = rate;
    s->pitch = 1.0f;
    s->gain = 1.0f;
    unlock();

    l_info("audio: loaded effect %s as #%i (%u frames, %i ch, %i Hz)",
           path, slot + 1, frames, channels, rate);
    return slot + 1;
}

void audio_unload_effect(const char *path) {
    if (!path) return;
    lock();
    int i = find_sound_by_path(path);
    if (i >= 0) {
        drmp3_free(sounds[i].pcm, NULL);
        free(sounds[i].path);
        memset(&sounds[i], 0, sizeof(Sound));
    }
    unlock();
}

void audio_play_effect(int sound_id, float pitch, float pan, bool loop, float gain) {
    lock();
    Sound *s = get_sound(sound_id);
    if (s) {
        s->pos = 0.0;
        s->pitch = clampf(pitch, 0.5f, 2.0f); // same clamp as cocosound
        s->pan = clampf(pan, -1.0f, 1.0f);
        s->gain = clampf(gain, 0.0f, 1.0f);
        s->loop = loop;
        s->paused = false;
        s->playing = true;
    }
    unlock();
}

void audio_stop_effect(int sound_id) {
    lock();
    Sound *s = get_sound(sound_id);
    if (s) s->playing = false;
    unlock();
}

void audio_pause_effect(int sound_id) {
    lock();
    Sound *s = get_sound(sound_id);
    if (s) s->paused = true;
    unlock();
}

void audio_set_effect_looping(int sound_id, bool loop) {
    lock();
    Sound *s = get_sound(sound_id);
    if (s) s->loop = loop;
    unlock();
}

void audio_set_effect_pitch(int sound_id, float pitch) {
    lock();
    Sound *s = get_sound(sound_id);
    if (s) s->pitch = clampf(pitch, 0.5f, 2.0f);
    unlock();
}

void audio_set_effect_gain(int sound_id, float gain) {
    lock();
    Sound *s = get_sound(sound_id);
    if (s) s->gain = clampf(gain, 0.0f, 1.0f);
    unlock();
}

void audio_pause_all_effects(void) {
    lock();
    effects_paused_all = true;
    unlock();
}

void audio_resume_all_effects(void) {
    lock();
    effects_paused_all = false;
    for (int i = 0; i < MAX_SOUNDS; i++)
        sounds[i].paused = false;
    unlock();
}

void audio_set_effects_volume(float volume) {
    lock();
    effects_volume = clampf(volume, 0.0f, 1.0f);
    unlock();
}

float audio_get_effects_volume(void) {
    return effects_volume;
}

/*
 * Music
 */

// Must be called with the lock held.
static void music_close(void) {
    if (music.open) {
        drmp3_uninit(&music.mp3);
        music.open = false;
    }
    free(music.file);
    music.file = NULL;
    music.file_size = 0;
    music.playing = false;
    music.paused = false;
    music.path[0] = '\0';
}

// Must be called with the lock held. Returns false at end of stream.
static bool music_refill(void) {
    uint32_t keep = 0;
    if (music.buf_frames > 0) {
        // carry over the last frame for interpolation
        int ch = (int)music.mp3.channels;
        for (int c = 0; c < ch && c < 2; c++)
            music.buf[c] = music.buf[(music.buf_frames - 1) * ch + c];
        keep = 1;
    }

    drmp3_uint64 got = drmp3_read_pcm_frames_s16(&music.mp3, MUSIC_CHUNK,
                                                 music.buf + keep * music.mp3.channels);
    if (got == 0 && music.loop) {
        drmp3_seek_to_pcm_frame(&music.mp3, 0);
        got = drmp3_read_pcm_frames_s16(&music.mp3, MUSIC_CHUNK,
                                        music.buf + keep * music.mp3.channels);
    }

    if (got == 0)
        return false;

    music.pos -= (double)(music.buf_frames > 0 ? music.buf_frames - 1 : 0);
    if (music.pos < 0.0) music.pos = 0.0;
    music.buf_frames = (uint32_t)got + keep;
    return true;
}

void audio_play_music(const char *path, bool loop) {
    if (!path) return;

    // cocomusic opens the asset by its file name only
    const char *name = strrchr(path, '/');
    name = name ? name + 1 : path;

    lock();
    if (music.open && strcmp(music.path, name) == 0) {
        // same track: restart, as MediaPlayer.seekTo(0) + start() would
        drmp3_seek_to_pcm_frame(&music.mp3, 0);
        music.buf_frames = 0;
        music.pos = 0.0;
        music.loop = loop;
        music.paused = false;
        music.playing = true;
        unlock();
        return;
    }
    music_close();
    unlock();

    uint8_t *data;
    size_t size;
    if (!asset_load(name, &data, &size) && !asset_load(path, &data, &size)) {
        l_error("audio: music %s not found", path);
        return;
    }

    lock();
    music.file = data;
    music.file_size = size;
    if (!drmp3_init_memory(&music.mp3, music.file, music.file_size, NULL)) {
        l_error("audio: failed to open music %s", path);
        music_close();
        unlock();
        return;
    }
    music.open = true;
    snprintf(music.path, sizeof(music.path), "%s", name);
    music.buf_frames = 0;
    music.pos = 0.0;
    music.loop = loop;
    music.paused = false;
    music.playing = true;
    unlock();

    l_info("audio: playing music %s (%i ch, %i Hz, loop=%i)", name,
           (int)music.mp3.channels, (int)music.mp3.sampleRate, loop);
}

void audio_stop_music(void) {
    lock();
    music.playing = false;
    if (music.open) {
        drmp3_seek_to_pcm_frame(&music.mp3, 0);
        music.buf_frames = 0;
        music.pos = 0.0;
    }
    unlock();
}

void audio_pause_music(void) {
    lock();
    music.paused = true;
    unlock();
}

void audio_resume_music(void) {
    lock();
    music.paused = false;
    unlock();
}

void audio_rewind_music(void) {
    lock();
    if (music.open) {
        drmp3_seek_to_pcm_frame(&music.mp3, 0);
        music.buf_frames = 0;
        music.pos = 0.0;
        music.playing = true;
        music.paused = false;
    }
    unlock();
}

bool audio_is_music_playing(void) {
    return music.open && music.playing && !music.paused;
}

void audio_set_music_volume(float volume) {
    lock();
    music_volume = clampf(volume, 0.0f, 1.0f);
    unlock();
}

float audio_get_music_volume(void) {
    return music_volume;
}

int audio_get_duration_ms(const char *path) {
    if (!path) return -1;

    lock();
    int i = find_sound_by_path(path);
    if (i >= 0) {
        int ms = (int)((uint64_t)sounds[i].frames * 1000 / sounds[i].rate);
        unlock();
        return ms;
    }
    unlock();

    const char *name = strrchr(path, '/');
    name = name ? name + 1 : path;

    uint8_t *data;
    size_t size;
    if (!asset_load(name, &data, &size))
        return -1;

    drmp3 mp3;
    int ms = -1;
    if (drmp3_init_memory(&mp3, data, size, NULL)) {
        drmp3_uint64 frames = drmp3_get_pcm_frame_count(&mp3);
        if (mp3.sampleRate)
            ms = (int)(frames * 1000 / mp3.sampleRate);
        drmp3_uninit(&mp3);
    }
    free(data);
    return ms;
}

/*
 * Mixer
 */

static int32_t mixbuf[MIX_GRAIN * 2];
static int16_t outbuf[2][MIX_GRAIN * 2];

// Must be called with the lock held.
static void mix_effects(void) {
    if (effects_paused_all) return;

    for (int i = 0; i < MAX_SOUNDS; i++) {
        Sound *s = &sounds[i];
        if (!s->pcm || !s->playing || s->paused) continue;

        float g = s->gain * effects_volume;
        // SoundPool-style pan: attenuate the opposite side only
        int32_t gl = (int32_t)(g * (s->pan > 0.0f ? 1.0f - s->pan : 1.0f) * 256.0f);
        int32_t gr = (int32_t)(g * (s->pan < 0.0f ? 1.0f + s->pan : 1.0f) * 256.0f);
        double step = (double)s->pitch * (double)s->rate / (double)MIX_RATE;

        for (int f = 0; f < MIX_GRAIN; f++) {
            uint32_t i0 = (uint32_t)s->pos;
            if (i0 >= s->frames) {
                if (s->loop) {
                    s->pos -= (double)s->frames;
                    i0 = (uint32_t)s->pos;
                    if (i0 >= s->frames) { s->pos = 0; i0 = 0; }
                } else {
                    s->playing = false;
                    break;
                }
            }
            uint32_t i1 = i0 + 1;
            if (i1 >= s->frames) i1 = s->loop ? 0 : i0;
            int32_t frac = (int32_t)((s->pos - (double)i0) * 256.0);

            int32_t l0 = sample_at(s->pcm, s->channels, i0, 0);
            int32_t l1 = sample_at(s->pcm, s->channels, i1, 0);
            int32_t r0 = sample_at(s->pcm, s->channels, i0, 1);
            int32_t r1 = sample_at(s->pcm, s->channels, i1, 1);
            int32_t l = l0 + (((l1 - l0) * frac) >> 8);
            int32_t r = r0 + (((r1 - r0) * frac) >> 8);

            mixbuf[f * 2]     += (l * gl) >> 8;
            mixbuf[f * 2 + 1] += (r * gr) >> 8;

            s->pos += step;
        }
    }
}

// Must be called with the lock held.
static void mix_music(void) {
    if (!music.open || !music.playing || music.paused) return;

    int ch = (int)music.mp3.channels;
    if (ch < 1) return;
    int32_t g = (int32_t)(music_volume * 256.0f);
    double step = (double)music.mp3.sampleRate / (double)MIX_RATE;

    for (int f = 0; f < MIX_GRAIN; f++) {
        while ((uint32_t)music.pos + 1 >= music.buf_frames) {
            if (!music_refill()) {
                music.playing = false;
                drmp3_seek_to_pcm_frame(&music.mp3, 0);
                music.buf_frames = 0;
                music.pos = 0.0;
                return;
            }
        }

        uint32_t i0 = (uint32_t)music.pos;
        int32_t frac = (int32_t)((music.pos - (double)i0) * 256.0);
        int32_t l0 = sample_at(music.buf, ch, i0, 0);
        int32_t l1 = sample_at(music.buf, ch, i0 + 1, 0);
        int32_t r0 = sample_at(music.buf, ch, i0, 1);
        int32_t r1 = sample_at(music.buf, ch, i0 + 1, 1);

        mixbuf[f * 2]     += ((l0 + (((l1 - l0) * frac) >> 8)) * g) >> 8;
        mixbuf[f * 2 + 1] += ((r0 + (((r1 - r0) * frac) >> 8)) * g) >> 8;

        music.pos += step;
    }
}

static int audio_thread(SceSize args, void *argp) {
    int port = sceAudioOutOpenPort(SCE_AUDIO_OUT_PORT_TYPE_BGM, MIX_GRAIN,
                                   MIX_RATE, SCE_AUDIO_OUT_MODE_STEREO);
    if (port < 0) {
        l_error("audio: sceAudioOutOpenPort failed: 0x%08X", port);
        return 0;
    }

    int vol[2] = { SCE_AUDIO_VOLUME_0DB, SCE_AUDIO_VOLUME_0DB };
    sceAudioOutSetVolume(port, SCE_AUDIO_VOLUME_FLAG_L_CH | SCE_AUDIO_VOLUME_FLAG_R_CH, vol);

    int cur = 0;
    while (1) {
        memset(mixbuf, 0, sizeof(mixbuf));

        lock();
        mix_music();
        mix_effects();
        unlock();

        int16_t *out = outbuf[cur];
        for (int i = 0; i < MIX_GRAIN * 2; i++) {
            int32_t v = mixbuf[i];
            out[i] = (int16_t)(v > 32767 ? 32767 : (v < -32768 ? -32768 : v));
        }

        sceAudioOutOutput(port, out); // blocks until the hardware wants more
        cur ^= 1;
    }

    return 0;
}

int audio_init(void) {
    memset(sounds, 0, sizeof(sounds));
    memset(&music, 0, sizeof(music));

    sceKernelCreateLwMutex(&audio_mutex, "audio_mutex", 0, 0, NULL);

    SceUID thid = sceKernelCreateThread("audio_mixer", audio_thread, 0x10000100 - 10,
                                        64 * 1024, 0, 0, NULL);
    if (thid < 0) {
        l_error("audio: failed to create mixer thread: 0x%08X", thid);
        return -1;
    }
    sceKernelStartThread(thid, 0, NULL);
    return 0;
}
