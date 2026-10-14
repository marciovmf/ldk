/**
 * # Audio engine
 *
 * Audio playback module.
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

  /** @group ldk.module.audio */
  //@enum
  typedef enum LDKAudioPriority
  {
    LDK_AUDIO_PRIORITY_LOW = 0,
    LDK_AUDIO_PRIORITY_NORMAL,
    LDK_AUDIO_PRIORITY_HIGH,
    LDK_AUDIO_PRIORITY_CRITICAL
  } LDKAudioPriority;

  /** @group ldk.module.audio */
  typedef struct LDKAudioVoice
  {
    u32 index;
    u32 version;
  } LDKAudioVoice;

  /** @group ldk.module.audio */
  typedef struct LDKAudioPlayDesc
  {
    LDKAudioPriority priority;
    float volume;
    float range;
    Vec3 position;
    bool loop;
    bool spatialized;
  } LDKAudioPlayDesc;

  /** @group ldk.module.audio */
  typedef struct LDKAudio
  {
    void *internal;
    bool is_initialized;
  } LDKAudio;

  // -------------------------------------------------------------------------
  // Audio lifecycle
  // -------------------------------------------------------------------------

  /**
   * @brief Initialize the audio.
   * @param audio Audio used by this operation.
   * @param assets Asset manager used to resolve referenced assets.
   * @return True if the operation succeeds; false otherwise.
   * @group ldk.module.audio
   */
  LDK_API bool ldk_audio_initialize(
      LDKAudio *audio, LDKAssetManager *assets);
  /**
   * @brief Update the audio.
   * @param audio Audio used by this operation.
   * @group ldk.module.audio
   */
  LDK_API void ldk_audio_update(LDKAudio *audio);
  /**
   * @brief Shut down the audio.
   * @param audio Audio used by this operation.
   * @group ldk.module.audio
   */
  LDK_API void ldk_audio_terminate(LDKAudio *audio);

  // -------------------------------------------------------------------------
  // Master volume
  // -------------------------------------------------------------------------

  /**
   * Set the master output volume for all voices (0.0 = silent, 1.0 = full).
   * Returns false if audio is not initialized or volume is outside [0, 1].
   * @param audio Audio used by this operation.
   * @param volume Volume used by this operation.
   * @return True if the operation succeeds; false otherwise.
   * @group ldk.module.audio
   */
  LDK_API bool ldk_audio_master_volume_set(LDKAudio *audio, float volume);

  /** Return the current master volume, or 1.0 if audio is unavailable.
   * @param audio Audio used by this operation.
   * @return The resulting value.
   * @group ldk.module.audio
   */
  LDK_API float ldk_audio_master_volume_get(LDKAudio *audio);

  // -------------------------------------------------------------------------
  // Listener
  // -------------------------------------------------------------------------

  /**
   * @brief Set the audio listener.
   * @param audio Audio used by this operation.
   * @param position Position used by the operation.
   * @param direction Direction used by this operation.
   * @param world_up World up used by this operation.
   * @group ldk.module.audio
   */
  LDK_API void ldk_audio_listener_set(LDKAudio *audio, Vec3 position,
      Vec3 direction, Vec3 world_up);

  // -------------------------------------------------------------------------
  // Playback
  // -------------------------------------------------------------------------

  /**
   * @brief Initialize an audio playback descriptor with defaults.
   * @return A descriptor for the requested component or resource.
   * @group ldk.module.audio
   */
  LDK_API LDKAudioPlayDesc ldk_audio_play_desc_default(void);

  /**
   * @brief Return an invalid handle for the audio voice.
   * @return An invalid or empty handle.
   * @group ldk.module.audio
   */
  LDK_API LDKAudioVoice ldk_audio_voice_null(void);
  /**
   * @brief Check whether an audio voice handle is null.
   * @param voice Voice used by this operation.
   * @return True when the queried condition holds; false otherwise.
   * @group ldk.module.audio
   */
  LDK_API bool ldk_audio_voice_is_null(LDKAudioVoice voice);
  /**
   * @brief Check whether the audio voice refers to a live resource.
   * @param audio Audio used by this operation.
   * @param voice Voice used by this operation.
   * @return True when the queried condition holds; false otherwise.
   * @group ldk.module.audio
   */
  LDK_API bool ldk_audio_voice_is_alive(
      LDKAudio *audio, LDKAudioVoice voice);
  /**
   * @brief Check whether the audio voice is playing.
   * @param audio Audio used by this operation.
   * @param voice Voice used by this operation.
   * @return True when the queried condition holds; false otherwise.
   * @group ldk.module.audio
   */
  LDK_API bool ldk_audio_voice_is_playing(
      LDKAudio *audio, LDKAudioVoice voice);
  /**
   * @brief Stop the audio voice.
   * @param audio Audio used by this operation.
   * @param voice Voice used by this operation.
   * @group ldk.module.audio
   */
  LDK_API void ldk_audio_voice_stop(LDKAudio *audio, LDKAudioVoice voice);
  /**
   * @brief Set the audio voice position.
   * @param audio Audio used by this operation.
   * @param voice Voice used by this operation.
   * @param position Position used by the operation.
   * @return True if the operation succeeds; false otherwise.
   * @group ldk.module.audio
   */
  LDK_API bool ldk_audio_voice_position_set(
      LDKAudio *audio, LDKAudioVoice voice, Vec3 position);
  /**
   * @brief Set the audio voice volume.
   * @param audio Audio used by this operation.
   * @param voice Voice used by this operation.
   * @param volume Volume used by this operation.
   * @return True if the operation succeeds; false otherwise.
   * @group ldk.module.audio
   */
  LDK_API bool ldk_audio_voice_volume_set(
      LDKAudio *audio, LDKAudioVoice voice, float volume);
  /**
   * @brief Set the audio voice range.
   * @param audio Audio used by this operation.
   * @param voice Voice used by this operation.
   * @param range Range used by this operation.
   * @return True if the operation succeeds; false otherwise.
   * @group ldk.module.audio
   */
  LDK_API bool ldk_audio_voice_range_set(
      LDKAudio *audio, LDKAudioVoice voice, float range);
  /**
   * @brief Set the audio voice loop.
   * @param audio Audio used by this operation.
   * @param voice Voice used by this operation.
   * @param loop Loop used by this operation.
   * @return True if the operation succeeds; false otherwise.
   * @group ldk.module.audio
   */
  LDK_API bool ldk_audio_voice_loop_set(
      LDKAudio *audio, LDKAudioVoice voice, bool loop);
  /**
   * @brief Set the audio voice priority.
   * @param audio Audio used by this operation.
   * @param voice Voice used by this operation.
   * @param priority Priority used by this operation.
   * @return True if the operation succeeds; false otherwise.
   * @group ldk.module.audio
   */
  LDK_API bool ldk_audio_voice_priority_set(
      LDKAudio *audio, LDKAudioVoice voice, LDKAudioPriority priority);

  /**
   * Plays a shared audio asset and returns a versioned playback handle. Small
   * assets may be cached in memory; larger assets stream from RunTree or an
   * open .box package. Active voices retain a reference to the asset data.
   * @param audio Audio used by this operation.
   * @param asset Asset handle to inspect or modify.
   * @param desc Description or configuration for this operation.
   * @return The requested value.
   * @group ldk.module.audio
   */
  LDK_API LDKAudioVoice ldk_audio_asset_play(LDKAudio *audio,
      LDKAssetAudio asset, const LDKAudioPlayDesc *desc);

  /**
   * Convenience path-based playback. The path is resolved through the asset
   * manager and then played as a shared LDKAssetAudio.
   * @param audio Audio used by this operation.
   * @param path Path identifying the file or resource.
   * @return The requested value.
   * @group ldk.module.audio
   */
  LDK_API LDKAudioVoice ldk_audio_sound_play(
      LDKAudio *audio, const char *path);
  /**
   * @brief Play a sound using the supplied playback options.
   * @param audio Audio used by this operation.
   * @param path Path identifying the file or resource.
   * @param desc Description or configuration for this operation.
   * @return The requested value.
   * @group ldk.module.audio
   */
  LDK_API LDKAudioVoice ldk_audio_sound_play_ex(
      LDKAudio *audio, const char *path, const LDKAudioPlayDesc *desc);

#ifdef __cplusplus
}
#endif

#endif // LDK_AUDIO_H
