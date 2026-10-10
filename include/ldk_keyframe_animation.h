/* Reflected component-property keyframe clips with relative hierarchy bindings. */
#ifndef LDK_KEYFRAME_ANIMATION_H
#define LDK_KEYFRAME_ANIMATION_H

#include <ldk_common.h>
#include <ldk_property.h>
#include <module/ldk_entity.h>

#ifdef __cplusplus
extern "C" {
#endif

#define LDK_KEYFRAME_PROPERTY_NAME_CAPACITY 96

typedef struct LDKKeyframe
{
  float time;
  LDKPropertyValue value;
} LDKKeyframe;

typedef struct LDKKeyframeTrack
{
  u64 target_path;
  u32 component_type;
  char property_name[LDK_KEYFRAME_PROPERTY_NAME_CAPACITY];
  LDKFieldType value_type;
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
  char *text;
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

LDK_API u64 ldk_keyframe_path_root(void);
LDK_API u64 ldk_keyframe_path_child(u64 parent_path, u64 name_hash);
LDK_API bool ldk_keyframe_entity_path(
    LDKEntity root, LDKEntity target, u64 *out_path);
LDK_API bool ldk_keyframe_target_resolve(
    LDKEntity root, u64 path, LDKEntity *out);

LDK_API void ldk_keyframe_animation_init(LDKKeyframeAnimation *clip);
LDK_API void ldk_keyframe_animation_clear(LDKKeyframeAnimation *clip);
/* Deep copy, including keyframe arrays and event strings. Destination must
 * have been initialized; on failure it is left unchanged. */
LDK_API bool ldk_keyframe_animation_copy(
    LDKKeyframeAnimation *destination, const LDKKeyframeAnimation *source);
/* A shorter duration is rejected if it would leave keys/events outside the clip. */
LDK_API bool ldk_keyframe_animation_duration_set(
    LDKKeyframeAnimation *clip, float duration);
LDK_API i32 ldk_keyframe_animation_track_find(const LDKKeyframeAnimation *clip,
    u64 path, u32 component_type, const char *property_name);
LDK_API i32 ldk_keyframe_animation_track_add(LDKKeyframeAnimation *clip,
    u64 path, u32 component_type, const char *property_name);
LDK_API bool ldk_keyframe_animation_track_remove(LDKKeyframeAnimation *clip, u32 track);
LDK_API bool ldk_keyframe_animation_key_set(LDKKeyframeAnimation *clip,
    u32 track, float time, LDKPropertyValue value);
LDK_API bool ldk_keyframe_animation_key_remove(
    LDKKeyframeAnimation *clip, u32 track, u32 key);
/* Move a key atomically, without overwriting another key or allocating. */
LDK_API bool ldk_keyframe_animation_key_move(
    LDKKeyframeAnimation *clip, u32 track, u32 key, float new_time,
    u32 *out_index);
LDK_API bool ldk_keyframe_animation_sample(const LDKKeyframeAnimation *clip,
    u32 track, float time, LDKPropertyValue *out_value);
LDK_API bool ldk_keyframe_animation_apply(
    const LDKKeyframeAnimation *clip, LDKEntity root, float time);
LDK_API bool ldk_keyframe_animation_value_from_entity(
    LDKEntity entity, const LDKKeyframeTrack *track, LDKPropertyValue *out_value);

LDK_API bool ldk_keyframe_animation_event_add_integer(
    LDKKeyframeAnimation *clip, float time, i32 number);
LDK_API bool ldk_keyframe_animation_event_add_string(
    LDKKeyframeAnimation *clip, float time, const char *text);
/* Mutating event operations preserve chronological ordering. */
LDK_API bool ldk_keyframe_animation_event_remove(
    LDKKeyframeAnimation *clip, u32 event_index);
LDK_API bool ldk_keyframe_animation_event_move(
    LDKKeyframeAnimation *clip, u32 event_index, float new_time,
    u32 *out_index);
LDK_API bool ldk_keyframe_animation_event_set_integer(
    LDKKeyframeAnimation *clip, u32 event_index, i32 number);
LDK_API bool ldk_keyframe_animation_event_set_string(
    LDKKeyframeAnimation *clip, u32 event_index, const char *text);
LDK_API void ldk_keyframe_animation_events_dispatch(
    const LDKKeyframeAnimation *clip, LDKEntity root, float previous_time,
    float current_time, LDKKeyframeEventFn fn, void *user);
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
#endif
