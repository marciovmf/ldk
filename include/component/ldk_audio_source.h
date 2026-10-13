/**
 * # Audio source component
 *
 * Spatial audio source component.
 */

#ifndef LDK_AUDIO_SOURCE_H
#define LDK_AUDIO_SOURCE_H

#include <ldk_asset.h>
#include <ldk_common.h>
#include <module/ldk_audio.h>
#include <module/ldk_component.h>
#include <module/ldk_entity.h>

#ifdef __cplusplus
extern "C"
{
#endif

  /** @group components.audio_source */
  //@component
  typedef struct LDKAudioSource
  {
    LDKAssetAudio audio;
    LDKAudioPriority priority;
    float range;
    //@inspect slider min=0.0 max=1.0
    float volume;
    bool loop;
    bool play_on_start;

    //@inspect hidden runtime
    LDKAudioVoice voice;
    //@inspect hidden runtime
    bool runtime_started;
  } LDKAudioSource;

  /**
   * @brief Create default settings for audio source.
   * @return A value initialized with the engine defaults.
   * @group components.audio_source
   */
  LDK_API LDKAudioSource ldk_audio_source_make_default(void);
  /**
   * @brief Start playback of the audio source.
   * @param entity Entity to inspect or modify.
   * @return True if the operation succeeds; false otherwise.
   * @group components.audio_source.playback
   */
  LDK_API bool ldk_audio_source_play(LDKEntity entity);
  /**
   * @brief Stop the audio source.
   * @param entity Entity to inspect or modify.
   * @group components.audio_source.playback
   */
  LDK_API void ldk_audio_source_stop(LDKEntity entity);
  /**
   * @brief Check whether the audio source is playing.
   * @param entity Entity to inspect or modify.
   * @return True when the queried condition holds; false otherwise.
   * @group components.audio_source.playback
   */
  LDK_API bool ldk_audio_source_is_playing(LDKEntity entity);

#ifdef LDK_ENGINE
  /**
   * @brief Build the ECS registration descriptor for the audio source component.
   * @param initial_capacity Initial number of entries reserved in the registry.
   * @return A descriptor for the requested component or resource.
   * @group components.audio_source.registration
   */
  LDK_API LDKComponentDesc ldk_audio_source_component_desc(
      u32 initial_capacity);
  /**
   * @brief Update all registered audio source components.
   * @group components.audio_source
   */
  LDK_API void ldk_audio_source_update_all(void);
#endif

#ifdef __cplusplus
}
#endif

#endif // LDK_AUDIO_SOURCE_H
