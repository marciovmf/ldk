/**
 * @file   ldk_audio.h
 * @brief  Audio playback module.
 */

#ifndef LDK_AUDIO_H
#define LDK_AUDIO_H

#include <ldk_common.h>
#include <module/ldk_asset_manager.h>
#include <stdx/stdx_math.h>

#ifdef __cplusplus
extern "C"
{
#endif

  //@enum
  typedef enum LDKAudioPriority
  {
    LDK_AUDIO_PRIORITY_LOW = 0,
    LDK_AUDIO_PRIORITY_NORMAL,
    LDK_AUDIO_PRIORITY_HIGH,
    LDK_AUDIO_PRIORITY_CRITICAL
  } LDKAudioPriority;

  typedef struct LDKAudioVoice
  {
    u32 index;
    u32 version;
  } LDKAudioVoice;

  typedef struct LDKAudioPlayDesc
  {
    LDKAudioPriority priority;
    float volume;
    float range;
    Vec3 position;
    bool loop;
    bool spatialized;
  } LDKAudioPlayDesc;

  typedef struct LDKAudio
  {
    void *internal;
    bool is_initialized;
  } LDKAudio;

  // -------------------------------------------------------------------------
  // Audio lifecycle
  // -------------------------------------------------------------------------

  LDK_API bool ldk_audio_initialize(
      LDKAudio *audio, LDKAssetManager *assets);
  LDK_API void ldk_audio_update(LDKAudio *audio);
  LDK_API void ldk_audio_terminate(LDKAudio *audio);

  // -------------------------------------------------------------------------
  // Listener
  // -------------------------------------------------------------------------

  LDK_API void ldk_audio_listener_set(LDKAudio *audio, Vec3 position,
      Vec3 direction, Vec3 world_up);

  // -------------------------------------------------------------------------
  // Playback
  // -------------------------------------------------------------------------

  LDK_API LDKAudioPlayDesc ldk_audio_play_desc_default(void);

  LDK_API LDKAudioVoice ldk_audio_voice_null(void);
  LDK_API bool ldk_audio_voice_is_null(LDKAudioVoice voice);
  LDK_API bool ldk_audio_voice_is_alive(
      LDKAudio *audio, LDKAudioVoice voice);
  LDK_API bool ldk_audio_voice_is_playing(
      LDKAudio *audio, LDKAudioVoice voice);
  LDK_API void ldk_audio_voice_stop(LDKAudio *audio, LDKAudioVoice voice);
  LDK_API bool ldk_audio_voice_position_set(
      LDKAudio *audio, LDKAudioVoice voice, Vec3 position);
  LDK_API bool ldk_audio_voice_volume_set(
      LDKAudio *audio, LDKAudioVoice voice, float volume);
  LDK_API bool ldk_audio_voice_range_set(
      LDKAudio *audio, LDKAudioVoice voice, float range);
  LDK_API bool ldk_audio_voice_loop_set(
      LDKAudio *audio, LDKAudioVoice voice, bool loop);
  LDK_API bool ldk_audio_voice_priority_set(
      LDKAudio *audio, LDKAudioVoice voice, LDKAudioPriority priority);

  /**
   * Plays a shared audio asset and returns a versioned playback handle.
   * Audio bytes are owned by LDKAssetManager and may originate from RunTree or
   * an open .box package. Active voices retain a reference to the asset data.
   */
  LDK_API LDKAudioVoice ldk_audio_asset_play(LDKAudio *audio,
      LDKAssetAudio asset, const LDKAudioPlayDesc *desc);

  /**
   * Convenience path-based playback. The path is resolved through the asset
   * manager and then played as a shared LDKAssetAudio.
   */
  LDK_API LDKAudioVoice ldk_audio_sound_play(
      LDKAudio *audio, const char *path);
  LDK_API LDKAudioVoice ldk_audio_sound_play_ex(
      LDKAudio *audio, const char *path, const LDKAudioPlayDesc *desc);

#ifdef __cplusplus
}
#endif

#endif // LDK_AUDIO_H
