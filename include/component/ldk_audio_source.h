/**
 * @file ldk_audio_source.h
 * @brief Spatial audio source component.
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

  LDK_API LDKAudioSource ldk_audio_source_make_default(void);
  LDK_API bool ldk_audio_source_play(LDKEntity entity);
  LDK_API void ldk_audio_source_stop(LDKEntity entity);
  LDK_API bool ldk_audio_source_is_playing(LDKEntity entity);

#ifdef LDK_ENGINE
  LDK_API LDKComponentDesc ldk_audio_source_component_desc(
      u32 initial_capacity);
  LDK_API void ldk_audio_source_update_all(void);
#endif

#ifdef __cplusplus
}
#endif

#endif // LDK_AUDIO_SOURCE_H
