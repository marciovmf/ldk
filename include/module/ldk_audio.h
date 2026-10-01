/**
 * @file   ldk_audio.h
 * @brief  Audio playback module.
 */

#ifndef LDK_AUDIO_H
#define LDK_AUDIO_H

#include <ldk_common.h>
#include <module/ldk_asset_source.h>

#ifdef __cplusplus
extern "C"
{
#endif

  typedef struct LDKAudio
  {
    void *internal;
    bool is_initialized;
  } LDKAudio;

  // -------------------------------------------------------------------------
  // Audio lifecycle
  // -------------------------------------------------------------------------

  LDK_API bool ldk_audio_initialize(
      LDKAudio *audio, LDKAssetSource *asset_source);
  LDK_API void ldk_audio_update(LDKAudio *audio);
  LDK_API void ldk_audio_terminate(LDKAudio *audio);

  // -------------------------------------------------------------------------
  // Playback
  // -------------------------------------------------------------------------

  /**
   * Plays an audio asset once and releases it automatically when it finishes.
   * The path is resolved through LDKAssetSource and may refer to loose RunTree
   * content or an entry in an open .box package.
   */
  LDK_API bool ldk_audio_sound_play(LDKAudio *audio, const char *path);

#ifdef __cplusplus
}
#endif

#endif // LDK_AUDIO_H
