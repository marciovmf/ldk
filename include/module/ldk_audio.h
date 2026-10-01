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

  typedef struct LDKAudioVoice
  {
    u32 index;
    u32 version;
  } LDKAudioVoice;

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

  LDK_API LDKAudioVoice ldk_audio_voice_null(void);
  LDK_API bool ldk_audio_voice_is_null(LDKAudioVoice voice);
  LDK_API bool ldk_audio_voice_is_alive(
      LDKAudio *audio, LDKAudioVoice voice);
  LDK_API bool ldk_audio_voice_is_playing(
      LDKAudio *audio, LDKAudioVoice voice);
  LDK_API void ldk_audio_voice_stop(LDKAudio *audio, LDKAudioVoice voice);

  /**
   * Plays an audio asset once and returns a versioned playback handle.
   * The path is resolved through LDKAssetSource and may refer to loose RunTree
   * content or an entry in an open .box package.
   *
   * Audio asset bytes are cached by path after the first successful load and
   * remain resident until the audio module terminates. Each active voice owns
   * a reference to its cached asset until that voice ends or is stopped.
   */
  LDK_API LDKAudioVoice ldk_audio_sound_play(
      LDKAudio *audio, const char *path);

#ifdef __cplusplus
}
#endif

#endif // LDK_AUDIO_H
