#include <module/ldk_audio.h>

#include <ldk.h>
#include <stdx/stdx_array.h>
#include <stdx/stdx_hpool.h>

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define MA_NO_ENCODING
#define MA_NO_GENERATION
#define MA_NO_RESOURCE_MANAGER
#define MINIAUDIO_IMPLEMENTATION
#include "../depend/miniaudio/miniaudio.h"

#define LDK_AUDIO_VOICE_PAGE_CAPACITY 64u

typedef struct LDKAudioVoiceData
{
  ma_decoder decoder;
  ma_sound sound;
  LDKAssetAudio asset;
  LDKAudioPriority priority;
  bool decoder_initialized;
  bool sound_initialized;
} LDKAudioVoiceData;

typedef struct LDKAudioInternal
{
  ma_engine engine;
  LDKAssetManager *assets;
  XHPool voices;
  XArray *finished_voices;
} LDKAudioInternal;

static XHandle s_audio_voice_to_x(LDKAudioVoice voice)
{
  XHandle handle;
  handle.index = voice.index;
  handle.version = voice.version;
  return handle;
}

static LDKAudioVoice s_audio_voice_from_x(XHandle handle)
{
  LDKAudioVoice voice;
  voice.index = handle.index;
  voice.version = handle.version;
  return voice;
}

static LDKAudioVoiceData *s_audio_voice_get(
    LDKAudioInternal *internal, LDKAudioVoice voice)
{
  if (!internal)
  {
    return NULL;
  }

  return (LDKAudioVoiceData *)x_hpool_get(
      &internal->voices, s_audio_voice_to_x(voice));
}

static void s_audio_voice_destroy(void *user, void *item)
{
  LDKAudioInternal *internal = (LDKAudioInternal *)user;
  LDKAudioVoiceData *voice = (LDKAudioVoiceData *)item;

  if (!voice)
  {
    return;
  }

  if (voice->sound_initialized)
  {
    ma_sound_uninit(&voice->sound);
  }

  if (voice->decoder_initialized)
  {
    ma_decoder_uninit(&voice->decoder);
  }

  if (internal && internal->assets && !x_handle_is_null(voice->asset.h))
  {
    LDKAssetAudioData *asset_data =
        ldk_asset_manager_audio_get(internal->assets, voice->asset);
    if (asset_data && asset_data->ref_count > 0)
    {
      asset_data->ref_count--;
    }
  }

  memset(voice, 0, sizeof(*voice));
  voice->asset = ldk_asset_audio_null();
}

static const char *s_audio_asset_path(
    LDKAudioInternal *internal, LDKAssetAudio asset)
{
  LDKAssetHandle generic;
  const LDKAssetInfo *info;

  if (!internal || !internal->assets)
  {
    return "";
  }

  generic.h = asset.h;
  info = ldk_asset_get_info_const(internal->assets, generic);
  return info ? info->asset_path.buf : "";
}

static void s_audio_finished_voices_collect(LDKAudioInternal *internal)
{
  XHPoolIter it;
  XHandle handle;
  LDKAudioVoiceData *voice;
  u32 count;

  if (!internal || !internal->finished_voices)
  {
    return;
  }

  x_array_clear(internal->finished_voices);

  for (voice = (LDKAudioVoiceData *)x_hpool_iter_begin(
           &internal->voices, &it, &handle);
       voice;
       voice = (LDKAudioVoiceData *)x_hpool_iter_next(
           &internal->voices, &it, &handle))
  {
    if (voice->sound_initialized && ma_sound_at_end(&voice->sound))
    {
      if (x_array_add(internal->finished_voices, &handle) != XARRAY_OK)
      {
        ldk_log_warning(
            "Failed to queue a finished audio voice for release.\n");
        break;
      }
    }
  }

  count = x_array_count(internal->finished_voices);
  for (u32 i = 0; i < count; ++i)
  {
    XHandle *finished =
        (XHandle *)x_array_get(internal->finished_voices, i);
    if (finished)
    {
      x_hpool_free(&internal->voices, *finished);
    }
  }
}

bool ldk_audio_initialize(LDKAudio *audio, LDKAssetManager *assets)
{
  LDKAudioInternal *internal;
  XHPoolConfig voice_pool_config;
  ma_result result;

  if (!audio || !assets)
  {
    return false;
  }

  memset(audio, 0, sizeof(*audio));

  internal = (LDKAudioInternal *)calloc(1, sizeof(*internal));
  if (!internal)
  {
    return false;
  }

  result = ma_engine_init(NULL, &internal->engine);
  if (result != MA_SUCCESS)
  {
    ldk_log_error("Failed to initialize audio: %s.\n",
        ma_result_description(result));
    free(internal);
    return false;
  }

  memset(&voice_pool_config, 0, sizeof(voice_pool_config));
  voice_pool_config.page_capacity = LDK_AUDIO_VOICE_PAGE_CAPACITY;
  voice_pool_config.initial_pages = 1;

  if (!x_hpool_init(&internal->voices, sizeof(LDKAudioVoiceData),
          voice_pool_config, NULL, s_audio_voice_destroy, internal))
  {
    ma_engine_uninit(&internal->engine);
    free(internal);
    return false;
  }

  internal->finished_voices =
      x_array_create(sizeof(XHandle), LDK_AUDIO_VOICE_PAGE_CAPACITY);
  if (!internal->finished_voices)
  {
    x_hpool_term(&internal->voices);
    ma_engine_uninit(&internal->engine);
    free(internal);
    return false;
  }

  internal->assets = assets;
  audio->internal = internal;
  audio->is_initialized = true;
  return true;
}

void ldk_audio_update(LDKAudio *audio)
{
  if (!audio || !audio->is_initialized || !audio->internal)
  {
    return;
  }

  s_audio_finished_voices_collect((LDKAudioInternal *)audio->internal);
}

void ldk_audio_terminate(LDKAudio *audio)
{
  LDKAudioInternal *internal;

  if (!audio || !audio->is_initialized)
  {
    return;
  }

  internal = (LDKAudioInternal *)audio->internal;
  if (internal)
  {
    x_hpool_term(&internal->voices);
    x_array_destroy(internal->finished_voices);
    ma_engine_uninit(&internal->engine);
    free(internal);
  }

  memset(audio, 0, sizeof(*audio));
}

void ldk_audio_listener_set(
    LDKAudio *audio, Vec3 position, Vec3 direction, Vec3 world_up)
{
  LDKAudioInternal *internal;

  if (!audio || !audio->is_initialized || !audio->internal)
  {
    return;
  }

  internal = (LDKAudioInternal *)audio->internal;
  ma_engine_listener_set_position(
      &internal->engine, 0, position.x, position.y, position.z);
  ma_engine_listener_set_direction(
      &internal->engine, 0, direction.x, direction.y, direction.z);
  ma_engine_listener_set_world_up(
      &internal->engine, 0, world_up.x, world_up.y, world_up.z);
}

LDKAudioPlayDesc ldk_audio_play_desc_default(void)
{
  LDKAudioPlayDesc desc;
  memset(&desc, 0, sizeof(desc));
  desc.priority = LDK_AUDIO_PRIORITY_NORMAL;
  desc.volume = 1.0f;
  desc.range = 10.0f;
  desc.loop = false;
  desc.spatialized = false;
  return desc;
}

LDKAudioVoice ldk_audio_voice_null(void)
{
  return s_audio_voice_from_x(x_handle_null());
}

bool ldk_audio_voice_is_null(LDKAudioVoice voice)
{
  return x_handle_is_null(s_audio_voice_to_x(voice)) != 0;
}

bool ldk_audio_voice_is_alive(LDKAudio *audio, LDKAudioVoice voice)
{
  LDKAudioInternal *internal;

  if (!audio || !audio->is_initialized || !audio->internal)
  {
    return false;
  }

  internal = (LDKAudioInternal *)audio->internal;
  return x_hpool_is_alive(
      &internal->voices, s_audio_voice_to_x(voice)) != 0;
}

bool ldk_audio_voice_is_playing(LDKAudio *audio, LDKAudioVoice voice)
{
  LDKAudioInternal *internal;
  LDKAudioVoiceData *voice_data;

  if (!audio || !audio->is_initialized || !audio->internal)
  {
    return false;
  }

  internal = (LDKAudioInternal *)audio->internal;
  voice_data = s_audio_voice_get(internal, voice);
  if (!voice_data || !voice_data->sound_initialized)
  {
    return false;
  }

  return ma_sound_is_playing(&voice_data->sound) != 0;
}

void ldk_audio_voice_stop(LDKAudio *audio, LDKAudioVoice voice)
{
  LDKAudioInternal *internal;
  LDKAudioVoiceData *voice_data;

  if (!audio || !audio->is_initialized || !audio->internal)
  {
    return;
  }

  internal = (LDKAudioInternal *)audio->internal;
  voice_data = s_audio_voice_get(internal, voice);
  if (!voice_data)
  {
    return;
  }

  if (voice_data->sound_initialized)
  {
    ma_sound_stop(&voice_data->sound);
  }

  x_hpool_free(&internal->voices, s_audio_voice_to_x(voice));
}

bool ldk_audio_voice_position_set(
    LDKAudio *audio, LDKAudioVoice voice, Vec3 position)
{
  LDKAudioInternal *internal;
  LDKAudioVoiceData *voice_data;

  if (!audio || !audio->is_initialized || !audio->internal)
  {
    return false;
  }

  internal = (LDKAudioInternal *)audio->internal;
  voice_data = s_audio_voice_get(internal, voice);
  if (!voice_data || !voice_data->sound_initialized)
  {
    return false;
  }

  ma_sound_set_position(
      &voice_data->sound, position.x, position.y, position.z);
  return true;
}

bool ldk_audio_voice_volume_set(
    LDKAudio *audio, LDKAudioVoice voice, float volume)
{
  LDKAudioInternal *internal;
  LDKAudioVoiceData *voice_data;

  if (!audio || !audio->is_initialized || !audio->internal || volume < 0.0f)
  {
    return false;
  }

  internal = (LDKAudioInternal *)audio->internal;
  voice_data = s_audio_voice_get(internal, voice);
  if (!voice_data || !voice_data->sound_initialized)
  {
    return false;
  }

  ma_sound_set_volume(&voice_data->sound, volume);
  return true;
}

bool ldk_audio_voice_range_set(
    LDKAudio *audio, LDKAudioVoice voice, float range)
{
  LDKAudioInternal *internal;
  LDKAudioVoiceData *voice_data;

  if (!audio || !audio->is_initialized || !audio->internal || range <= 0.0f)
  {
    return false;
  }

  internal = (LDKAudioInternal *)audio->internal;
  voice_data = s_audio_voice_get(internal, voice);
  if (!voice_data || !voice_data->sound_initialized)
  {
    return false;
  }

  ma_sound_set_max_distance(&voice_data->sound, range);
  return true;
}

bool ldk_audio_voice_loop_set(
    LDKAudio *audio, LDKAudioVoice voice, bool loop)
{
  LDKAudioInternal *internal;
  LDKAudioVoiceData *voice_data;

  if (!audio || !audio->is_initialized || !audio->internal)
  {
    return false;
  }

  internal = (LDKAudioInternal *)audio->internal;
  voice_data = s_audio_voice_get(internal, voice);
  if (!voice_data || !voice_data->sound_initialized)
  {
    return false;
  }

  ma_sound_set_looping(&voice_data->sound, loop ? MA_TRUE : MA_FALSE);
  return true;
}

bool ldk_audio_voice_priority_set(
    LDKAudio *audio, LDKAudioVoice voice, LDKAudioPriority priority)
{
  LDKAudioInternal *internal;
  LDKAudioVoiceData *voice_data;

  if (!audio || !audio->is_initialized || !audio->internal ||
      priority < LDK_AUDIO_PRIORITY_LOW ||
      priority > LDK_AUDIO_PRIORITY_CRITICAL)
  {
    return false;
  }

  internal = (LDKAudioInternal *)audio->internal;
  voice_data = s_audio_voice_get(internal, voice);
  if (!voice_data)
  {
    return false;
  }

  voice_data->priority = priority;
  return true;
}

LDKAudioVoice ldk_audio_asset_play(LDKAudio *audio, LDKAssetAudio asset,
    const LDKAudioPlayDesc *desc)
{
  LDKAudioPlayDesc play_desc;
  LDKAudioInternal *internal;
  LDKAssetAudioData *asset_data;
  LDKAudioVoiceData *voice;
  XHandle handle;
  ma_result result;
  const char *path;

  handle = x_handle_null();

  if (!audio || !audio->is_initialized || !audio->internal ||
      x_handle_is_null(asset.h))
  {
    return s_audio_voice_from_x(handle);
  }

  internal = (LDKAudioInternal *)audio->internal;
  s_audio_finished_voices_collect(internal);

  asset_data = ldk_asset_manager_audio_get(internal->assets, asset);
  if (!asset_data || !asset_data->encoded_data ||
      asset_data->encoded_size == 0 ||
      asset_data->encoded_size > (u64)SIZE_MAX)
  {
    return s_audio_voice_from_x(handle);
  }

  play_desc = desc ? *desc : ldk_audio_play_desc_default();
  if (play_desc.volume < 0.0f || play_desc.range <= 0.0f ||
      play_desc.priority < LDK_AUDIO_PRIORITY_LOW ||
      play_desc.priority > LDK_AUDIO_PRIORITY_CRITICAL)
  {
    return s_audio_voice_from_x(handle);
  }

  path = s_audio_asset_path(internal, asset);
  handle = x_hpool_alloc(&internal->voices);
  if (x_handle_is_null(handle))
  {
    ldk_log_error("Failed to allocate an audio voice for '%s'.\n", path);
    return s_audio_voice_from_x(handle);
  }

  voice = (LDKAudioVoiceData *)x_hpool_get(&internal->voices, handle);
  if (!voice)
  {
    x_hpool_free(&internal->voices, handle);
    return ldk_audio_voice_null();
  }

  voice->asset = ldk_asset_audio_null();
  result = ma_decoder_init_memory(asset_data->encoded_data,
      (size_t)asset_data->encoded_size, NULL, &voice->decoder);
  if (result != MA_SUCCESS)
  {
    ldk_log_error("Failed to decode audio asset '%s': %s.\n", path,
        ma_result_description(result));
    x_hpool_free(&internal->voices, handle);
    return ldk_audio_voice_null();
  }
  voice->decoder_initialized = true;

  result = ma_sound_init_from_data_source(&internal->engine,
      (ma_data_source *)&voice->decoder, 0, NULL, &voice->sound);
  if (result != MA_SUCCESS)
  {
    ldk_log_error("Failed to initialize audio asset '%s': %s.\n", path,
        ma_result_description(result));
    x_hpool_free(&internal->voices, handle);
    return ldk_audio_voice_null();
  }
  voice->sound_initialized = true;

  ma_sound_set_volume(&voice->sound, play_desc.volume);
  ma_sound_set_looping(
      &voice->sound, play_desc.loop ? MA_TRUE : MA_FALSE);
  ma_sound_set_spatialization_enabled(
      &voice->sound, play_desc.spatialized ? MA_TRUE : MA_FALSE);
  if (play_desc.spatialized)
  {
    ma_sound_set_position(&voice->sound, play_desc.position.x,
        play_desc.position.y, play_desc.position.z);
    ma_sound_set_attenuation_model(
        &voice->sound, ma_attenuation_model_linear);
    ma_sound_set_min_distance(&voice->sound, 0.0f);
    ma_sound_set_max_distance(&voice->sound, play_desc.range);
  }

  voice->priority = play_desc.priority;

  result = ma_sound_start(&voice->sound);
  if (result != MA_SUCCESS)
  {
    ldk_log_error("Failed to play audio asset '%s': %s.\n", path,
        ma_result_description(result));
    x_hpool_free(&internal->voices, handle);
    return ldk_audio_voice_null();
  }

  voice->asset = asset;
  asset_data->ref_count++;
  return s_audio_voice_from_x(handle);
}

LDKAudioVoice ldk_audio_sound_play(LDKAudio *audio, const char *path)
{
  return ldk_audio_sound_play_ex(audio, path, NULL);
}

LDKAudioVoice ldk_audio_sound_play_ex(
    LDKAudio *audio, const char *path, const LDKAudioPlayDesc *desc)
{
  LDKAudioInternal *internal;
  LDKAssetAudio asset;

  if (!audio || !audio->is_initialized || !audio->internal || !path ||
      !path[0])
  {
    return ldk_audio_voice_null();
  }

  internal = (LDKAudioInternal *)audio->internal;
  asset = ldk_asset_manager_audio_load_shared(internal->assets, path);
  if (x_handle_is_null(asset.h))
  {
    ldk_log_error("Failed to read audio asset '%s'.\n", path);
    return ldk_audio_voice_null();
  }

  return ldk_audio_asset_play(audio, asset, desc);
}
