/**
 * @file ldk_keyframe_animation.h
 * @brief Code-authored keyframe clips, relative target bindings, and TML I/O.
 */
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

/**
 * @brief Called synchronously for an animation marker.
 * @param root Entity driving the animation.
 * @param event Marker payload; pointer is valid only during the callback.
 * @param user Opaque callback context.
 */
typedef void (*LDKKeyframeEventFn)(
    LDKEntity root, const LDKKeyframeEvent *event, void *user);

/**
 * @brief Return the reserved path hash of an animation source root.
 * @return Stable root hash used for relative track bindings.
 */
LDK_API u64 ldk_keyframe_path_root(void);
/**
 * @brief Extend a relative hierarchy path with a child name hash.
 * @param parent_path Existing hash relative to the animation source.
 * @param name_hash Nonzero persistent entity name hash.
 * @return Deterministic path hash, or zero for invalid inputs.
 */
LDK_API u64 ldk_keyframe_path_child(u64 parent_path, u64 name_hash);
/**
 * @brief Compute the path of a descendant relative to an animation root.
 * @param root Animation hierarchy root entity.
 * @param target Descendant entity whose relative path is requested.
 * @param out_path Receives the relative hash on success.
 * @return True if target belongs to the transform subtree rooted at root.
 */
LDK_API bool ldk_keyframe_entity_path(
    LDKEntity root, LDKEntity target, u64 *out_path);
/**
 * @brief Resolve a relative path hash to exactly one descendant.
 * @param root Animation hierarchy root entity.
 * @param path Hash relative to the animation source root.
 * @param out Receives the resolved entity.
 * @return True if the path resolves unambiguously. Duplicate matching paths fail.
 */
LDK_API bool ldk_keyframe_target_resolve(
    LDKEntity root, u64 path, LDKEntity *out);

/**
 * @brief Initialize a clip to an empty, one-second animation.
 * @param clip Animation clip instance.
 * @return Nothing. Call before any other clip operation.
 */
LDK_API void ldk_keyframe_animation_init(LDKKeyframeAnimation *clip);
/**
 * @brief Free a clip and reset it to the initialized state.
 * @param clip Animation clip instance.
 * @return Nothing. Invalidates pointers into tracks, keys, and events.
 */
LDK_API void ldk_keyframe_animation_clear(LDKKeyframeAnimation *clip);
/**
 * @brief Validate a clip independently of the ECS and reflected metadata.
 * @param clip Animation clip instance.
 * @return True if the duration, tracks, keys and events are structurally valid.
 */
LDK_API bool ldk_keyframe_animation_validate(const LDKKeyframeAnimation *clip);
/**
 * @brief Deep-copy all tracks, keys, events and owned event strings.
 * @param destination Initialized destination clip.
 * @param source Initialized source clip.
 * @return True on success; otherwise destination remains unchanged.
 */
LDK_API bool ldk_keyframe_animation_copy(
    LDKKeyframeAnimation *destination, const LDKKeyframeAnimation *source);
/**
 * @brief Set duration without discarding keys or events.
 * @param clip Animation clip instance.
 * @param duration Requested clip length in seconds.
 * @return False if invalid or any key/event would lie beyond duration.
 */
LDK_API bool ldk_keyframe_animation_duration_set(
    LDKKeyframeAnimation *clip, float duration);
/**
 * @brief Find a track identified by relative path and reflected field.
 * @param clip Animation clip instance.
 * @param path Hash relative to the animation source root.
 * @param component_type Runtime ECS component type.
 * @param property_name Persistent reflected field identifier.
 * @return Zero-based track index, or -1 if not found.
 */
LDK_API i32 ldk_keyframe_animation_track_find(const LDKKeyframeAnimation *clip,
    u64 path, u32 component_type, const char *property_name);
/**
 * @brief Create a track for an editable reflected component field.
 * @param clip Animation clip instance.
 * @param path Hash relative to the animation source root.
 * @param component_type Runtime ECS component type.
 * @param property_name Persistent reflected field identifier.
 * @return Track index; existing track is reused, -1 if unavailable or unsupported.
 * @note Reflected component metadata must be registered when authoring.
 */
LDK_API i32 ldk_keyframe_animation_track_add(LDKKeyframeAnimation *clip,
    u64 path, u32 component_type, const char *property_name);
/**
 * @brief Remove a track and all of its keyframes.
 * @param clip Animation clip instance.
 * @param track Zero-based track index or track description.
 * @return True if the track existed. Later track indices shift.
 */
LDK_API bool ldk_keyframe_animation_track_remove(LDKKeyframeAnimation *clip, u32 track);
/**
 * @brief Insert or update a typed keyframe at the specified second.
 * @param clip Animation clip instance.
 * @param track Zero-based track index or track description.
 * @param time Time in seconds.
 * @param value Type-tagged property value.
 * @return True on success; may extend clip duration. Requires available component metadata.
 * @note Reflected component metadata must be registered when authoring.
 */
LDK_API bool ldk_keyframe_animation_key_set(LDKKeyframeAnimation *clip,
    u32 track, float time, LDKPropertyValue value);
/**
 * @brief Remove a keyframe from a track.
 * @param clip Animation clip instance.
 * @param track Zero-based track index or track description.
 * @param key Zero-based keyframe index.
 * @return True if removed. Subsequent key indices shift.
 */
LDK_API bool ldk_keyframe_animation_key_remove(
    LDKKeyframeAnimation *clip, u32 track, u32 key);
/**
 * @brief Move a keyframe in time while keeping its value and chronological order.
 * @param clip Animation clip instance.
 * @param track Zero-based track index or track description.
 * @param key Zero-based keyframe index.
 * @param new_time Requested new timestamp in seconds.
 * @param out_index Optional output for the moved item index.
 * @return True on success. Rejects collisions and times beyond duration.
 */
LDK_API bool ldk_keyframe_animation_key_move(
    LDKKeyframeAnimation *clip, u32 track, u32 key, float new_time,
    u32 *out_index);
/**
 * @brief Evaluate a single track at the requested time without changing entities.
 * @param clip Animation clip instance.
 * @param track Zero-based track index or track description.
 * @param time Time in seconds.
 * @param out_value Receives the sampled property value.
 * @return True if the track has keys and its values can be evaluated.
 */
LDK_API bool ldk_keyframe_animation_sample(const LDKKeyframeAnimation *clip,
    u32 track, float time, LDKPropertyValue *out_value);
/**
 * @brief Sample and write all nonempty tracks to the subtree of a root entity.
 * @param clip Animation clip instance.
 * @param root Entity used as the root for relative bindings.
 * @param time Time in seconds.
 * @return True if all tracks applied successfully; missing bindings are reported by false.
 */
LDK_API bool ldk_keyframe_animation_apply(
    const LDKKeyframeAnimation *clip, LDKEntity root, float time);
/**
 * @brief Read the current component value addressed by a track.
 * @param entity Target ECS entity.
 * @param track Zero-based track index or track description.
 * @param out_value Receives the sampled property value.
 * @return True if the editable reflected field exists and can be read.
 */
LDK_API bool ldk_keyframe_animation_value_from_entity(
    LDKEntity entity, const LDKKeyframeTrack *track, LDKPropertyValue *out_value);

/**
 * @brief Insert an integer event marker at the specified second.
 * @param clip Animation clip instance.
 * @param time Time in seconds.
 * @param number Integer event payload.
 * @return True on success; extends duration if needed.
 */
LDK_API bool ldk_keyframe_animation_event_add_integer(
    LDKKeyframeAnimation *clip, float time, i32 number);
/**
 * @brief Insert a string event marker, copying the supplied text.
 * @param clip Animation clip instance.
 * @param time Time in seconds.
 * @param text NUL-terminated event string.
 * @return True on success; extends duration if needed.
 */
LDK_API bool ldk_keyframe_animation_event_add_string(
    LDKKeyframeAnimation *clip, float time, const char *text);
/**
 * @brief Delete an event marker and its owned string.
 * @param clip Animation clip instance.
 * @param event_index Zero-based event index.
 * @return True if removed. Later event indices shift.
 */
LDK_API bool ldk_keyframe_animation_event_remove(
    LDKKeyframeAnimation *clip, u32 event_index);
/**
 * @brief Change an event timestamp while preserving event ordering.
 * @param clip Animation clip instance.
 * @param event_index Zero-based event index.
 * @param new_time Requested new timestamp in seconds.
 * @param out_index Optional output for the moved item index.
 * @return True on success; times beyond duration are rejected.
 */
LDK_API bool ldk_keyframe_animation_event_move(
    LDKKeyframeAnimation *clip, u32 event_index, float new_time,
    u32 *out_index);
/**
 * @brief Replace an event payload with an integer.
 * @param clip Animation clip instance.
 * @param event_index Zero-based event index.
 * @param number Integer event payload.
 * @return True if the event existed.
 */
LDK_API bool ldk_keyframe_animation_event_set_integer(
    LDKKeyframeAnimation *clip, u32 event_index, i32 number);
/**
 * @brief Replace an event payload with a copied string.
 * @param clip Animation clip instance.
 * @param event_index Zero-based event index.
 * @param text NUL-terminated event string.
 * @return True on success; the old payload remains if allocation fails.
 */
LDK_API bool ldk_keyframe_animation_event_set_string(
    LDKKeyframeAnimation *clip, u32 event_index, const char *text);
/**
 * @brief Synchronously deliver all events crossed in (previous_time, current_time].
 * @param clip Animation clip instance.
 * @param root Root entity containing the animation source.
 * @param previous_time Exclusive lower time bound in seconds.
 * @param current_time Inclusive upper time bound in seconds.
 * @param fn Event callback.
 * @param user Opaque callback context.
 * @return Nothing. Event payloads are temporary snapshots valid during each callback.
 * @note Callbacks may mutate or destroy the source; do not keep event pointers.
 */
LDK_API void ldk_keyframe_animation_events_dispatch(
    const LDKKeyframeAnimation *clip, LDKEntity root, float previous_time,
    float current_time, LDKKeyframeEventFn fn, void *user);
/**
 * @brief Parse a version-2 TML animation into an initialized clip.
 * @param clip Animation clip instance.
 * @param source NUL-terminated TML input.
 * @return True on success; clip remains unchanged on failure.
 */
LDK_API bool ldk_keyframe_animation_from_tml(
    LDKKeyframeAnimation *clip, const char *source);
/**
 * @brief Serialize a structurally valid clip to TML version 2.
 * @param clip Animation clip instance.
 * @param out_source Receives a newly allocated NUL-terminated TML buffer.
 * @return True on success. Caller must free *out_source.
 */
LDK_API bool ldk_keyframe_animation_to_tml(
    const LDKKeyframeAnimation *clip, char **out_source);
/**
 * @brief Load a clip from the configured asset source.
 * @param clip Animation clip instance.
 * @param asset_path Logical .anim asset path.
 * @return True on success; existing clip remains unchanged on failure.
 */
LDK_API bool ldk_keyframe_animation_load(
    LDKKeyframeAnimation *clip, const char *asset_path);
/**
 * @brief Save a clip as TML through the configured asset source.
 * @param clip Animation clip instance.
 * @param asset_path Logical .anim asset path.
 * @return True if successfully serialized and written.
 */
LDK_API bool ldk_keyframe_animation_save(
    const LDKKeyframeAnimation *clip, const char *asset_path);

#ifdef __cplusplus
}
#endif
#endif
