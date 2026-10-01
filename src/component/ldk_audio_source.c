#include <component/ldk_audio_source.h>

#include <component/ldk_transform.h>
#include <ldk.h>
#include <module/ldk_ecs.h>

#include <string.h>

static LDKAudioSource *s_audio_source_get(LDKEntity entity)
{
  return (LDKAudioSource *)ldk_ecs_component_get(
      entity, LDK_COMPONENT_TYPE_AUDIO_SOURCE);
}

static Vec3 s_audio_source_world_position(LDKEntity entity)
{
  Mat4 world = mat4_identity();

  if (!ldk_transform_get_world_matrix(entity, &world))
  {
    return vec3_make(0.0f, 0.0f, 0.0f);
  }

  return vec3_make(world.m[12], world.m[13], world.m[14]);
}

LDKAudioSource ldk_audio_source_make_default(void)
{
  LDKAudioSource source;

  memset(&source, 0, sizeof(source));
  source.audio = ldk_asset_audio_null();
  source.priority = LDK_AUDIO_PRIORITY_NORMAL;
  source.range = 10.0f;
  source.volume = 1.0f;
  source.loop = false;
  source.play_on_start = true;
  source.voice = ldk_audio_voice_null();
  source.runtime_started = false;
  return source;
}

bool ldk_audio_source_play(LDKEntity entity)
{
  LDKAudioSource *source = s_audio_source_get(entity);
  LDKAudio *audio = (LDKAudio *)ldk_module_get(LDK_MODULE_AUDIO);
  LDKAudioPlayDesc desc;

  if (!source || !audio || x_handle_is_null(source->audio.h) ||
      source->range <= 0.0f || source->volume < 0.0f)
  {
    return false;
  }

  if (ldk_audio_voice_is_alive(audio, source->voice))
  {
    ldk_audio_voice_stop(audio, source->voice);
  }
  source->voice = ldk_audio_voice_null();

  desc = ldk_audio_play_desc_default();
  desc.priority = source->priority;
  desc.volume = source->volume;
  desc.range = source->range;
  desc.position = s_audio_source_world_position(entity);
  desc.loop = source->loop;
  desc.spatialized = true;

  source->voice = ldk_audio_asset_play(audio, source->audio, &desc);
  return !ldk_audio_voice_is_null(source->voice);
}

void ldk_audio_source_stop(LDKEntity entity)
{
  LDKAudioSource *source = s_audio_source_get(entity);
  LDKAudio *audio = (LDKAudio *)ldk_module_get(LDK_MODULE_AUDIO);

  if (!source)
  {
    return;
  }

  if (audio && ldk_audio_voice_is_alive(audio, source->voice))
  {
    ldk_audio_voice_stop(audio, source->voice);
  }

  source->voice = ldk_audio_voice_null();
}

bool ldk_audio_source_is_playing(LDKEntity entity)
{
  LDKAudioSource *source = s_audio_source_get(entity);
  LDKAudio *audio = (LDKAudio *)ldk_module_get(LDK_MODULE_AUDIO);

  return source && audio &&
      ldk_audio_voice_is_playing(audio, source->voice);
}

#ifdef LDK_ENGINE
static bool s_audio_source_attach(LDKEntityRegistry *entities,
    LDKComponentRegistry *components, LDKEntity entity, void *component,
    u32 index, const void *initial_value, void *user)
{
  LDKAudioSource *source = (LDKAudioSource *)component;

  (void)entities;
  (void)components;
  (void)entity;
  (void)index;
  (void)user;

  if (!source)
  {
    return false;
  }

  *source = initial_value ? *(const LDKAudioSource *)initial_value
                          : ldk_audio_source_make_default();
  source->voice = ldk_audio_voice_null();
  source->runtime_started = false;
  return true;
}

static void s_audio_source_destroy(LDKEntityRegistry *entities,
    LDKComponentRegistry *components, LDKEntity entity, void *component,
    u32 index, void *user)
{
  LDKAudioSource *source = (LDKAudioSource *)component;
  LDKAudio *audio = (LDKAudio *)ldk_module_get(LDK_MODULE_AUDIO);

  (void)entities;
  (void)components;
  (void)entity;
  (void)index;
  (void)user;

  if (source && audio && ldk_audio_voice_is_alive(audio, source->voice))
  {
    ldk_audio_voice_stop(audio, source->voice);
  }
}

LDKComponentDesc ldk_audio_source_component_desc(u32 initial_capacity)
{
  static const u32 required_components[] = {
      LDK_COMPONENT_TYPE_TRANSFORM};
  LDKComponentDesc desc = {0};

  desc.name = "AudioSource";
  desc.type = LDK_COMPONENT_TYPE_AUDIO_SOURCE;
  desc.entry_size = sizeof(LDKAudioSource);
  desc.initial_capacity = initial_capacity;
  desc.required_components = required_components;
  desc.required_component_count =
      (u32)(sizeof(required_components) / sizeof(required_components[0]));
  desc.attach = s_audio_source_attach;
  desc.destroy = s_audio_source_destroy;
  return desc;
}

void ldk_audio_source_update_all(void)
{
  LDKComponentRegistry *components = ldk_ecs_component_registry_get();
  LDKAudio *audio = (LDKAudio *)ldk_module_get(LDK_MODULE_AUDIO);
  XArray *sources;
  XArray *owners;
  u32 count;

  if (!components || !audio)
  {
    return;
  }

  sources = ldk_component_store_get(
      components, LDK_COMPONENT_TYPE_AUDIO_SOURCE);
  owners = ldk_component_owners_get(
      components, LDK_COMPONENT_TYPE_AUDIO_SOURCE);
  if (!sources || !owners)
  {
    return;
  }

  count = x_array_count(sources);
  for (u32 i = 0; i < count; ++i)
  {
    LDKAudioSource *source = (LDKAudioSource *)x_array_get(sources, i);
    LDKEntity *entity = (LDKEntity *)x_array_get(owners, i);
    bool game_started;

    if (!source || !entity)
    {
      continue;
    }

    game_started = ldk_game_instance_is_started();
    if (!game_started)
    {
      if (source->runtime_started &&
          ldk_audio_voice_is_alive(audio, source->voice))
      {
        ldk_audio_voice_stop(audio, source->voice);
      }

      source->voice = ldk_audio_voice_null();
      source->runtime_started = false;
      continue;
    }

    if (!source->runtime_started)
    {
      source->runtime_started = true;
      if (source->play_on_start)
      {
        (void)ldk_audio_source_play(*entity);
      }
    }

    if (!ldk_audio_voice_is_alive(audio, source->voice))
    {
      source->voice = ldk_audio_voice_null();
      continue;
    }

    (void)ldk_audio_voice_position_set(
        audio, source->voice, s_audio_source_world_position(*entity));
    (void)ldk_audio_voice_volume_set(
        audio, source->voice, source->volume);
    (void)ldk_audio_voice_range_set(
        audio, source->voice, source->range);
    (void)ldk_audio_voice_loop_set(audio, source->voice, source->loop);
    (void)ldk_audio_voice_priority_set(
        audio, source->voice, source->priority);
  }
}
#endif
