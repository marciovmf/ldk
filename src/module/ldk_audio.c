#include <module/ldk_audio.h>

#include <ldk.h>

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define MA_NO_ENCODING
#define MA_NO_GENERATION
#define MA_NO_RESOURCE_MANAGER
#define MINIAUDIO_IMPLEMENTATION
#include "../depend/miniaudio/miniaudio.h"

typedef struct LDKAudioSound
{
  ma_decoder decoder;
  ma_sound sound;
  void *encoded_data;
  struct LDKAudioSound *next;
} LDKAudioSound;

typedef struct LDKAudioInternal
{
  ma_engine engine;
  LDKAssetSource *asset_source;
  LDKAudioSound *sounds;
} LDKAudioInternal;

static void s_audio_sound_destroy(LDKAudioSound *sound)
{
  if (!sound)
  {
    return;
  }

  ma_sound_uninit(&sound->sound);
  ma_decoder_uninit(&sound->decoder);
  free(sound->encoded_data);
  free(sound);
}

static void s_audio_finished_sounds_collect(LDKAudioInternal *internal)
{
  LDKAudioSound **link;

  if (!internal)
  {
    return;
  }

  link = &internal->sounds;
  while (*link)
  {
    LDKAudioSound *sound = *link;
    if (ma_sound_at_end(&sound->sound))
    {
      *link = sound->next;
      s_audio_sound_destroy(sound);
      continue;
    }

    link = &sound->next;
  }
}

static bool s_audio_asset_read(LDKAudioInternal *internal, const char *path,
    void **out_data, size_t *out_size)
{
  LDKAssetSourceFile file;
  u64 size;
  void *data;

  if (!internal || !internal->asset_source || !path || !path[0] ||
      !out_data || !out_size ||
      !ldk_asset_source_find(internal->asset_source, path, &file))
  {
    return false;
  }

  size = ldk_asset_source_file_size(&file);
  if (size == 0 || size > (u64)SIZE_MAX)
  {
    return false;
  }

  data = malloc((size_t)size);
  if (!data)
  {
    return false;
  }

  if (!ldk_asset_source_file_read(&file, data, size))
  {
    free(data);
    return false;
  }

  *out_data = data;
  *out_size = (size_t)size;
  return true;
}

bool ldk_audio_initialize(LDKAudio *audio, LDKAssetSource *asset_source)
{
  LDKAudioInternal *internal;
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

  s_audio_finished_sounds_collect((LDKAudioInternal *)audio->internal);
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
    LDKAudioSound *sound = internal->sounds;
    while (sound)
    {
      LDKAudioSound *next = sound->next;
      s_audio_sound_destroy(sound);
      sound = next;
    }

    ma_engine_uninit(&internal->engine);
    free(internal);
  }

  memset(audio, 0, sizeof(*audio));
}

bool ldk_audio_sound_play(LDKAudio *audio, const char *path)
{
  LDKAudioInternal *internal;
  LDKAudioSound *sound;
  void *encoded_data;
  size_t encoded_size;
  ma_result result;

  if (!audio || !audio->is_initialized || !audio->internal || !path ||
      !path[0])
  {
    return false;
  }

  internal = (LDKAudioInternal *)audio->internal;
  s_audio_finished_sounds_collect(internal);

  encoded_data = NULL;
  encoded_size = 0;
  if (!s_audio_asset_read(internal, path, &encoded_data, &encoded_size))
  {
    ldk_log_error("Failed to read audio asset '%s'.\n", path);
    return false;
  }

  sound = (LDKAudioSound *)calloc(1, sizeof(*sound));
  if (!sound)
  {
    free(encoded_data);
    return false;
  }

  result = ma_decoder_init_memory(
      encoded_data, encoded_size, NULL, &sound->decoder);
  if (result != MA_SUCCESS)
  {
    ldk_log_error("Failed to decode audio asset '%s': %s.\n", path,
        ma_result_description(result));
    free(encoded_data);
    free(sound);
    return false;
  }

  result = ma_sound_init_from_data_source(&internal->engine,
      (ma_data_source *)&sound->decoder, 0, NULL, &sound->sound);
  if (result != MA_SUCCESS)
  {
    ldk_log_error("Failed to initialize audio asset '%s': %s.\n", path,
        ma_result_description(result));
    ma_decoder_uninit(&sound->decoder);
    free(encoded_data);
    free(sound);
    return false;
  }

  result = ma_sound_start(&sound->sound);
  if (result != MA_SUCCESS)
  {
    ldk_log_error("Failed to play audio asset '%s': %s.\n", path,
        ma_result_description(result));
    ma_sound_uninit(&sound->sound);
    ma_decoder_uninit(&sound->decoder);
    free(encoded_data);
    free(sound);
    return false;
  }

  sound->encoded_data = encoded_data;
  sound->next = internal->sounds;
  internal->sounds = sound;
  return true;
}
