/* Native, implicit keyframe animation playback. Built into ldk.c. */
#include <component/ldk_keyframe_animation_source.h>
#include <component/ldk_transform.h>
#include <ldk.h>
#include <module/ldk_asset_manager.h>
#include <module/ldk_ecs.h>

#include <float.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

static LDKKeyframeEventFn s_keyframe_source_event_handler;
static void *s_keyframe_source_event_user;

static LDKKeyFrameAnimationSource *s_keyframe_source_get(LDKEntity entity)
{
  return (LDKKeyFrameAnimationSource *)ldk_ecs_component_get(
      entity, LDK_COMPONENT_TYPE_KEYFRAME_ANIMATION_SOURCE);
}

static const LDKKeyframeAnimation *s_keyframe_source_clip(
    const LDKKeyFrameAnimationSource *source)
{
  LDKAssetManager *assets = (LDKAssetManager *)ldk_module_get(
      LDK_MODULE_ASSET_MANAGER);
  const LDKAssetKeyframeAnimationData *data = source && assets &&
      source->current_animation >= 0 &&
      (u32)source->current_animation < source->animation_count
      ? ldk_asset_manager_keyframe_animation_get_const(assets,
          source->animations[source->current_animation])
      : NULL;
  return data ? &data->clip : NULL;
}

LDKKeyFrameAnimationSource ldk_keyframe_animation_source_make_default(void)
{
  LDKKeyFrameAnimationSource result = {0};
  result.current_animation = -1;
  result.play_on_start = true;
  result.loop = true;
  result.speed = 1.0f;
  return result;
}

static bool s_keyframe_source_attach(LDKEntityRegistry *entities,
    LDKComponentRegistry *components, LDKEntity entity, void *component,
    u32 index, const void *initial, void *user)
{
  LDKKeyFrameAnimationSource *source = component;
  (void)entities;
  (void)components;
  (void)entity;
  (void)index;
  (void)user;
  if (!source)
  {
    return false;
  }
  *source = ldk_keyframe_animation_source_make_default();
  if (initial)
  {
    const LDKKeyFrameAnimationSource *config = initial;
    source->play_on_start = config->play_on_start;
    source->loop = config->loop;
    source->speed = config->speed;
    if (config->animation_count)
    {
      if (!config->animations ||
          config->animation_count > UINT32_MAX / sizeof(*source->animations))
      {
        return false;
      }
      source->animations = malloc((size_t)config->animation_count *
          sizeof(*source->animations));
      if (!source->animations)
      {
        return false;
      }
      memcpy(source->animations, config->animations,
          (size_t)config->animation_count * sizeof(*source->animations));
      source->animation_count = config->animation_count;
      source->animation_capacity = config->animation_count;
      source->current_animation = config->current_animation >= 0 &&
          (u32)config->current_animation < source->animation_count
          ? config->current_animation : 0;
    }
  }
  source->time = 0.0f;
  source->playing = false;
  source->runtime_started = false;
  source->restart_pending = true;
  return true;
}

static void s_keyframe_source_destroy(LDKEntityRegistry *entities,
    LDKComponentRegistry *components, LDKEntity entity, void *component,
    u32 index, void *user)
{
  LDKKeyFrameAnimationSource *source = component;
  (void)entities;
  (void)components;
  (void)entity;
  (void)index;
  (void)user;
  if (source)
  {
    free(source->animations);
    source->animations = NULL;
    source->animation_count = source->animation_capacity = 0;
  }
}

bool ldk_keyframe_animation_source_add(
    LDKEntity root, LDKAssetKeyframeAnimation animation)
{
  LDKKeyFrameAnimationSource *source = s_keyframe_source_get(root);
  if (!source || x_handle_is_null(animation.h) ||
      source->animation_count >= INT32_MAX)
  {
    return false;
  }
  for (u32 i = 0; i < source->animation_count; ++i)
  {
    if (source->animations[i].h.index == animation.h.index &&
        source->animations[i].h.version == animation.h.version)
    {
      return false;
    }
  }
  if (source->animation_count == source->animation_capacity)
  {
    u32 capacity = source->animation_capacity
        ? source->animation_capacity * 2u : 4u;
    if (capacity <= source->animation_capacity ||
        (size_t)capacity > SIZE_MAX / sizeof(*source->animations))
    {
      return false;
    }
    LDKAssetKeyframeAnimation *items = realloc(source->animations,
        (size_t)capacity * sizeof(*items));
    if (!items)
    {
      return false;
    }
    source->animations = items;
    source->animation_capacity = capacity;
  }
  source->animations[source->animation_count++] = animation;
  if (source->current_animation < 0)
  {
    source->current_animation = 0;
  }
  return true;
}

bool ldk_keyframe_animation_source_replace(
    LDKEntity root, u32 index, LDKAssetKeyframeAnimation animation)
{
  LDKKeyFrameAnimationSource *source = s_keyframe_source_get(root);
  if (!source || index >= source->animation_count ||
      x_handle_is_null(animation.h))
  {
    return false;
  }
  for (u32 i = 0; i < source->animation_count; ++i)
  {
    if (i != index && source->animations[i].h.index == animation.h.index &&
        source->animations[i].h.version == animation.h.version)
    {
      return false;
    }
  }
  source->animations[index] = animation;
  if (source->current_animation == (i32)index)
  {
    source->time = 0.0f;
    source->playing = false;
    source->runtime_started = false;
    source->restart_pending = true;
  }
  return true;
}

bool ldk_keyframe_animation_source_set_current(LDKEntity root, i32 index)
{
  LDKKeyFrameAnimationSource *source = s_keyframe_source_get(root);
  if (!source || index < 0 || (u32)index >= source->animation_count)
  {
    return false;
  }
  if (source->current_animation != index)
  {
    source->current_animation = index;
    source->time = 0.0f;
    source->playing = false;
    source->runtime_started = false;
    source->restart_pending = true;
  }
  return true;
}

bool ldk_keyframe_animation_source_remove(LDKEntity root, u32 index)
{
  LDKKeyFrameAnimationSource *source = s_keyframe_source_get(root);
  if (!source || index >= source->animation_count)
  {
    return false;
  }
  if (index + 1 < source->animation_count)
  {
    memmove(&source->animations[index], &source->animations[index + 1],
        (size_t)(source->animation_count - index - 1) *
            sizeof(*source->animations));
  }
  --source->animation_count;
  if (!source->animation_count)
  {
    source->current_animation = -1;
  }
  else if (source->current_animation > (i32)index)
  {
    --source->current_animation;
  }
  else if (source->current_animation == (i32)index ||
      source->current_animation >= (i32)source->animation_count)
  {
    source->current_animation = 0;
  }
  source->time = 0.0f;
  source->playing = false;
  source->runtime_started = false;
  source->restart_pending = true;
  return true;
}

LDKComponentDesc ldk_keyframe_animation_source_component_desc(
    u32 initial_capacity)
{
  static const u32 required[] = {LDK_COMPONENT_TYPE_TRANSFORM};
  LDKComponentDesc desc = {0};
  desc.name = "KeyFrameAnimationSource";
  desc.type = LDK_COMPONENT_TYPE_KEYFRAME_ANIMATION_SOURCE;
  desc.entry_size = sizeof(LDKKeyFrameAnimationSource);
  desc.initial_capacity = initial_capacity;
  desc.required_components = required;
  desc.required_component_count = 1;
  desc.attach = s_keyframe_source_attach;
  desc.destroy = s_keyframe_source_destroy;
  return desc;
}

void ldk_keyframe_animation_source_event_handler_set(
    LDKKeyframeEventFn handler, void *user)
{
  s_keyframe_source_event_handler = handler;
  s_keyframe_source_event_user = user;
}

bool ldk_keyframe_animation_source_play(LDKEntity root)
{
  LDKKeyFrameAnimationSource *source = s_keyframe_source_get(root);
  const LDKKeyframeAnimation *clip = s_keyframe_source_clip(source);
  if (!source || !clip || !ldk_ecs_component_is_enabled(
          root, LDK_COMPONENT_TYPE_KEYFRAME_ANIMATION_SOURCE))
  {
    return false;
  }
  bool restarting = source->restart_pending || !source->runtime_started ||
      source->time >= clip->duration;
  if (restarting)
  {
    source->time = 0.0f;
  }
  source->playing = true;
  source->runtime_started = true;
  source->restart_pending = false;
  (void)ldk_keyframe_animation_apply(clip, root, source->time);
  if (restarting)
  {
    ldk_keyframe_animation_events_dispatch(clip, root, -FLT_EPSILON, 0.0f,
        s_keyframe_source_event_handler, s_keyframe_source_event_user);
  }
  return true;
}

void ldk_keyframe_animation_source_pause(LDKEntity root)
{
  LDKKeyFrameAnimationSource *source = s_keyframe_source_get(root);
  if (source)
  {
    source->playing = false;
    source->runtime_started = true;
  }
}

void ldk_keyframe_animation_source_stop(LDKEntity root)
{
  LDKKeyFrameAnimationSource *source = s_keyframe_source_get(root);
  if (!source)
  {
    return;
  }
  source->runtime_started = true;
  source->playing = false;
  source->restart_pending = true;
  source->time = 0.0f;
  const LDKKeyframeAnimation *clip = s_keyframe_source_clip(source);
  if (clip)
  {
    (void)ldk_keyframe_animation_apply(clip, root, 0.0f);
  }
}

bool ldk_keyframe_animation_source_seek(LDKEntity root, float time)
{
  LDKKeyFrameAnimationSource *source = s_keyframe_source_get(root);
  const LDKKeyframeAnimation *clip = s_keyframe_source_clip(source);
  if (!source || !clip || !isfinite(time))
  {
    return false;
  }
  if (time < 0.0f)
  {
    time = 0.0f;
  }
  if (time > clip->duration)
  {
    time = clip->duration;
  }
  source->time = time;
  source->runtime_started = true;
  source->restart_pending = false;
  return ldk_keyframe_animation_apply(clip, root, time);
}

static void s_keyframe_source_tick(LDKEntity root, float dt)
{
  LDKKeyFrameAnimationSource *source = s_keyframe_source_get(root);
  const LDKKeyframeAnimation *clip = s_keyframe_source_clip(source);
  float duration;
  float next;
  float previous;
  bool loop;

  if (!source || !clip || !isfinite(dt) || dt < 0.0f ||
      !isfinite(source->speed) || source->speed < 0.0f ||
      !ldk_ecs_component_is_enabled(
          root, LDK_COMPONENT_TYPE_KEYFRAME_ANIMATION_SOURCE))
  {
    return;
  }
  if (!source->runtime_started)
  {
    if (!source->play_on_start)
    {
      source->runtime_started = true;
      return;
    }
    if (!ldk_keyframe_animation_source_play(root))
    {
      return;
    }
    /* A synchronous event handler can remove the component. */
    source = s_keyframe_source_get(root);
    if (!source)
    {
      return;
    }
  }
  if (!source->playing || dt == 0.0f || source->speed == 0.0f)
  {
    return;
  }

  duration = clip->duration;
  if (!isfinite(duration) || duration <= 0.0f)
  {
    source->playing = false;
    return;
  }
  previous = source->time;
  if (!isfinite(previous) || previous < 0.0f || previous >= duration)
  {
    previous = 0.0f;
  }
  next = previous + dt * source->speed;
  if (!isfinite(next))
  {
    return;
  }
  loop = source->loop;
  if (!loop || next < duration)
  {
    if (next >= duration)
    {
      next = duration;
      source->playing = false;
    }
    source->time = next;
    (void)ldk_keyframe_animation_apply(clip, root, next);
    ldk_keyframe_animation_events_dispatch(clip, root, previous, next,
        s_keyframe_source_event_handler, s_keyframe_source_event_user);
    return;
  }

  /* Deliver markers from each crossed loop, including those at time zero. */
  float remaining = next - previous;
  float cursor = previous;
  while (remaining >= duration - cursor)
  {
    float segment = duration - cursor;
    ldk_keyframe_animation_events_dispatch(clip, root, cursor, duration,
        s_keyframe_source_event_handler, s_keyframe_source_event_user);
    source = s_keyframe_source_get(root);
    if (!source || !source->playing)
    {
      return;
    }
    remaining -= segment;
    cursor = 0.0f;
    ldk_keyframe_animation_events_dispatch(clip, root, -FLT_EPSILON, 0.0f,
        s_keyframe_source_event_handler, s_keyframe_source_event_user);
    source = s_keyframe_source_get(root);
    if (!source || !source->playing)
    {
      return;
    }
  }
  source = s_keyframe_source_get(root);
  if (!source)
  {
    return;
  }
  source->time = remaining;
  (void)ldk_keyframe_animation_apply(clip, root, remaining);
  ldk_keyframe_animation_events_dispatch(clip, root, 0.0f, remaining,
      s_keyframe_source_event_handler, s_keyframe_source_event_user);
}

void ldk_keyframe_animation_source_update_all(float delta_time)
{
  LDKComponentRegistry *registry = ldk_ecs_component_registry_get();
  XArray *owners;
  u32 count;
  LDKEntity *snapshot;

  if (!registry || !isfinite(delta_time) || delta_time < 0.0f)
  {
    return;
  }
  owners = ldk_component_owners_get(
      registry, LDK_COMPONENT_TYPE_KEYFRAME_ANIMATION_SOURCE);
  if (!owners)
  {
    return;
  }
  count = x_array_count(owners);
  if (!count)
  {
    return;
  }
  snapshot = malloc((size_t)count * sizeof(*snapshot));
  if (!snapshot)
  {
    return;
  }
  for (u32 i = 0; i < count; ++i)
  {
    const LDKEntity *entity = x_array_get(owners, i);
    snapshot[i] = entity ? *entity : x_handle_null();
  }
  for (u32 i = 0; i < count; ++i)
  {
    if (!x_handle_is_null(snapshot[i]))
    {
      s_keyframe_source_tick(snapshot[i], delta_time);
    }
  }
  free(snapshot);
}

void ldk_keyframe_animation_source_reset_all(void)
{
  LDKComponentRegistry *registry = ldk_ecs_component_registry_get();
  XArray *sources = registry ? ldk_component_store_get(
      registry, LDK_COMPONENT_TYPE_KEYFRAME_ANIMATION_SOURCE) : NULL;
  if (!sources)
  {
    return;
  }
  for (u32 i = 0; i < x_array_count(sources); ++i)
  {
    LDKKeyFrameAnimationSource *source = x_array_get(sources, i);
    if (source)
    {
      source->time = 0.0f;
      source->playing = false;
      source->runtime_started = false;
      source->restart_pending = true;
    }
  }
}
