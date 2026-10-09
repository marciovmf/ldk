/*
 * @File  ldk_keyframe_animation.h
 * @brief Transform keyframe clips and relative hierarchy bindings.
 *
 * Clips own mutable tracks/keyframes. They do not own ECS entities. A path
 * is a hash relative to the animation root, not an entity identity.
 */
#ifndef LDK_KEYFRAME_ANIMATION_H
#define LDK_KEYFRAME_ANIMATION_H

#include <ldk_common.h>
#include <module/ldk_entity.h>
#include <stdx/stdx_math.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum LDKKeyframeTransformChannel
{
  LDK_KEYFRAME_TRANSFORM_POSITION = 0,
  LDK_KEYFRAME_TRANSFORM_ROTATION,
  LDK_KEYFRAME_TRANSFORM_SCALE
} LDKKeyframeTransformChannel;

typedef struct LDKKeyframe
{
  float time;
  Vec4 value; /* XYZ for position/scale, XYZW for quaternion rotation. */
} LDKKeyframe;

typedef struct LDKKeyframeTrack
{
  u64 target_path;
  LDKKeyframeTransformChannel channel;
  LDKKeyframe *keys;
  u32 count;
  u32 capacity;
} LDKKeyframeTrack;

typedef enum LDKKeyframeEventValueType
{
  LDK_KEYFRAME_EVENT_INTEGER = 0,
  LDK_KEYFRAME_EVENT_STRING
} LDKKeyframeEventValueType;

typedef struct LDKKeyframeEvent
{
  float time;
  LDKKeyframeEventValueType type;
  i32 number;
  char *text; /* Owned by the clip, NULL for integer events. */
} LDKKeyframeEvent;

typedef struct LDKKeyframeAnimation
{
  float duration;
  LDKKeyframeTrack *tracks;
  u32 track_count;
  u32 track_capacity;
  LDKKeyframeEvent *events;
  u32 event_count;
  u32 event_capacity;
} LDKKeyframeAnimation;

typedef void (*LDKKeyframeEventFn)(
    LDKEntity root, const LDKKeyframeEvent *event, void *user);

/* The root path is not derived from the root entity's name. */
LDK_API u64 ldk_keyframe_path_root(void);
LDK_API u64 ldk_keyframe_path_child(u64 parent_path, u64 name_hash);
LDK_API bool ldk_keyframe_entity_path(
    LDKEntity root, LDKEntity target, u64 *out_path);

LDK_API void ldk_keyframe_animation_init(LDKKeyframeAnimation *clip);
LDK_API void ldk_keyframe_animation_clear(LDKKeyframeAnimation *clip);
LDK_API i32 ldk_keyframe_animation_track_find(
    const LDKKeyframeAnimation *clip, u64 path, LDKKeyframeTransformChannel channel);
LDK_API i32 ldk_keyframe_animation_track_add(
    LDKKeyframeAnimation *clip, u64 path, LDKKeyframeTransformChannel channel);
LDK_API bool ldk_keyframe_animation_track_remove(LDKKeyframeAnimation *clip, u32 track);
/* Inserting at an existing time replaces its value. Keys remain sorted. */
LDK_API bool ldk_keyframe_animation_key_set(
    LDKKeyframeAnimation *clip, u32 track, float time, Vec4 value);
LDK_API bool ldk_keyframe_animation_key_remove(
    LDKKeyframeAnimation *clip, u32 track, u32 key);
LDK_API bool ldk_keyframe_animation_sample(
    const LDKKeyframeAnimation *clip, u32 track, float time, Vec4 *out_value);
/* Apply sampled values to a root or descendant using Transform setters. */
LDK_API bool ldk_keyframe_animation_apply(
    const LDKKeyframeAnimation *clip, LDKEntity root, float time);
LDK_API bool ldk_keyframe_animation_value_from_transform(
    LDKEntity entity, LDKKeyframeTransformChannel channel, Vec4 *out_value);

LDK_API bool ldk_keyframe_animation_event_add_integer(
    LDKKeyframeAnimation *clip, float time, i32 number);
LDK_API bool ldk_keyframe_animation_event_add_string(
    LDKKeyframeAnimation *clip, float time, const char *text);
/* Emits all events crossed moving forward, in timeline order. */
LDK_API void ldk_keyframe_animation_events_dispatch(
    const LDKKeyframeAnimation *clip, LDKEntity root, float previous_time,
    float current_time, LDKKeyframeEventFn fn, void *user);

/* TML source / asset-source paths (packages may be read but not overwritten). */
LDK_API bool ldk_keyframe_animation_from_tml(
    LDKKeyframeAnimation *clip, const char *source);
LDK_API bool ldk_keyframe_animation_to_tml(
    const LDKKeyframeAnimation *clip, char **out_source);
LDK_API bool ldk_keyframe_animation_load(
    LDKKeyframeAnimation *clip, const char *asset_path);
LDK_API bool ldk_keyframe_animation_save(
    const LDKKeyframeAnimation *clip, const char *asset_path);

#ifdef __cplusplus
}
#endif
#endif /* LDK_KEYFRAME_ANIMATION_H */
