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

typedef struct LDKAudioAsset
{
  LDKAssetPath path;
  void *encoded_data;
  size_t encoded_size;
  u32 ref_count;
  struct LDKAudioAsset *next;
} LDKAudioAsset;

typedef struct LDKAudioVoiceData
{
  ma_decoder decoder;
  ma_sound sound;
  LDKAudioAsset *asset;
  bool decoder_initialized;
  bool sound_initialized;
} LDKAudioVoiceData;

typedef struct LDKAudioInternal
{
  ma_engine engine;
  LDKAssetSource *asset_source;
  LDKAudioAsset *assets;
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

static void s_audio_voice_destroy(void *user, void *item)
{
  LDKAudioVoiceData *voice = (LDKAudioVoiceData *)item;

  (void)user;

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

  if (voice->asset)
  {
    if (voice->asset->ref_count > 0)
    {
      voice->asset->ref_count--;
    }
    voice->asset = NULL;
  }

  memset(voice, 0, sizeof(*voice));
}

static LDKAudioAsset *s_audio_asset_find(
    LDKAudioInternal *internal, const LDKAssetPath *path)
{
  LDKAudioAsset *asset;

  if (!internal || !path)
  {
    return NULL;
  }

  asset = internal->assets;
  while (asset)
  {
    if (asset->path.length == path->length &&
        memcmp(asset->path.buf, path->buf, path->length + 1u) == 0)
    {
      return asset;
    }
    asset = asset->next;
  }

  return NULL;
}

static LDKAudioAsset *s_audio_asset_load(
    LDKAudioInternal *internal, const LDKAssetPath *path)
{
  LDKAssetSourceFile file;
  LDKAudioAsset *asset;
  u64 size;
  void *data;

  if (!internal || !internal->asset_source || !path ||
      !ldk_asset_source_find(internal->asset_source, path->buf, &file))
  {
    return NULL;
  }

  size = ldk_asset_source_file_size(&file);
  if (size == 0 || size > (u64)SIZE_MAX)
  {
    return NULL;
  }

  data = malloc((size_t)size);
  if (!data)
  {
    return NULL;
  }

  if (!ldk_asset_source_file_read(&file, data, size))
  {
    free(data);
    return NULL;
  }

  asset = (LDKAudioAsset *)calloc(1, sizeof(*asset));
  if (!asset)
  {
    free(data);
    return NULL;
  }

  asset->path = *path;
  asset->encoded_data = data;
  asset->encoded_size = (size_t)size;
  asset->next = internal->assets;
  internal->assets = asset;
  return asset;
}

static LDKAudioAsset *s_audio_asset_get(
    LDKAudioInternal *internal, const char *path)
{
  LDKAssetPath asset_path;
  LDKAudioAsset *asset;

  if (!internal || !path || !path[0] ||
      !ldk_asset_path_set(&asset_path, path))
  {
    return NULL;
  }

  asset = s_audio_asset_find(internal, &asset_path);
  if (asset)
  {
    return asset;
  }

  return s_audio_asset_load(internal, &asset_path);
}

static void s_audio_assets_destroy(LDKAudioInternal *internal)
{
  LDKAudioAsset *asset;

  if (!internal)
  {
    return;
  }

  asset = internal->assets;
  while (asset)
  {
    LDKAudioAsset *next = asset->next;

    if (asset->ref_count != 0)
    {
      ldk_log_warning("Audio asset '%s' still has %u active reference(s) "
                      "during shutdown.\n",
          asset->path.buf, asset->ref_count);
    }

    free(asset->encoded_data);
    free(asset);
    asset = next;
  }

  internal->assets = NULL;
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

bool ldk_audio_initialize(LDKAudio *audio, LDKAssetSource *asset_source)
{
  LDKAudioInternal *internal;
  XHPoolConfig voice_pool_config;
  ma_result result;

  if (!audio || !asset_source)
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

  internal->asset_source = asset_source;
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
    s_audio_assets_destroy(internal);
    ma_engine_uninit(&internal->engine);
    free(internal);
  }

  memset(audio, 0, sizeof(*audio));
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
  voice_data = (LDKAudioVoiceData *)x_hpool_get(
      &internal->voices, s_audio_voice_to_x(voice));
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
  voice_data = (LDKAudioVoiceData *)x_hpool_get(
      &internal->voices, s_audio_voice_to_x(voice));
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

LDKAudioVoice ldk_audio_sound_play(LDKAudio *audio, const char *path)
{
  XHandle handle;
  LDKAudioInternal *internal;
  LDKAudioAsset *asset;
  LDKAudioVoiceData *voice;
  ma_result result;

  handle = x_handle_null();

  if (!audio || !audio->is_initialized || !audio->internal || !path ||
      !path[0])
  {
    return s_audio_voice_from_x(handle);
  }

  internal = (LDKAudioInternal *)audio->internal;
  s_audio_finished_voices_collect(internal);

  asset = s_audio_asset_get(internal, path);
  if (!asset)
  {
    ldk_log_error("Failed to read audio asset '%s'.\n", path);
    return s_audio_voice_from_x(handle);
  }

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

  result = ma_decoder_init_memory(
      asset->encoded_data, asset->encoded_size, NULL, &voice->decoder);
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

  result = ma_sound_start(&voice->sound);
  if (result != MA_SUCCESS)
  {
    ldk_log_error("Failed to play audio asset '%s': %s.\n", path,
        ma_result_description(result));
    x_hpool_free(&internal->voices, handle);
    return ldk_audio_voice_null();
  }

  voice->asset = asset;
  asset->ref_count++;
  return s_audio_voice_from_x(handle);
}
