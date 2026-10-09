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
  const LDKAssetKeyframeAnimationData *data = source && assets
      ? ldk_asset_manager_keyframe_animation_get_const(assets, source->animation)
      : NULL;
  return data ? &data->clip : NULL;
}

LDKKeyFrameAnimationSource ldk_keyframe_animation_source_make_default(void)
{
  LDKKeyFrameAnimationSource result = {0};
  result.animation = ldk_asset_keyframe_animation_null();
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
  *source = initial ? *(const LDKKeyFrameAnimationSource *)initial
                    : ldk_keyframe_animation_source_make_default();
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
