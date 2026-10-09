/**
 * @file ldk_keyframe_animation_source.h
 * @brief Playback of an .anim asset, rooted at this entity.
 */
#ifndef LDK_KEYFRAME_ANIMATION_SOURCE_H
#define LDK_KEYFRAME_ANIMATION_SOURCE_H

#include <ldk_asset.h>
#include <ldk_keyframe_animation.h>
#include <module/ldk_component.h>

#ifdef __cplusplus
extern "C" {
#endif

//@component
typedef struct LDKKeyFrameAnimationSource
{
  /* Assets shared by all instances; bindings are relative to this entity. */
  //@inspect hidden
  LDKAssetKeyframeAnimation *animations;
  //@inspect hidden
  u32 animation_count;
  //@inspect hidden
  u32 animation_capacity;
  i32 current_animation;
  bool play_on_start;
  bool loop;
  float speed;

  //@inspect hidden runtime
  float time;
  //@inspect hidden runtime
  bool playing;
  //@inspect hidden runtime
  bool runtime_started;
  //@inspect hidden runtime
  bool restart_pending;
} LDKKeyFrameAnimationSource;

LDK_API LDKKeyFrameAnimationSource ldk_keyframe_animation_source_make_default(void);
LDK_API bool ldk_keyframe_animation_source_add(
    LDKEntity root, LDKAssetKeyframeAnimation animation);
LDK_API bool ldk_keyframe_animation_source_remove(LDKEntity root, u32 index);
LDK_API bool ldk_keyframe_animation_source_replace(
    LDKEntity root, u32 index, LDKAssetKeyframeAnimation animation);
LDK_API bool ldk_keyframe_animation_source_set_current(LDKEntity root, i32 index);
LDK_API bool ldk_keyframe_animation_source_play(LDKEntity root);
LDK_API void ldk_keyframe_animation_source_pause(LDKEntity root);
LDK_API void ldk_keyframe_animation_source_stop(LDKEntity root);
/* Seek evaluates the pose without triggering events. */
LDK_API bool ldk_keyframe_animation_source_seek(LDKEntity root, float time);
/* Event callbacks run synchronously in update, with the source entity as root. */
LDK_API void ldk_keyframe_animation_source_event_handler_set(
    LDKKeyframeEventFn handler, void *user);

#ifdef LDK_ENGINE
LDK_API LDKComponentDesc ldk_keyframe_animation_source_component_desc(
    u32 initial_capacity);
LDK_API void ldk_keyframe_animation_source_update_all(float delta_time);
LDK_API void ldk_keyframe_animation_source_reset_all(void);
#endif

#ifdef __cplusplus
}
#endif
#endif /* LDK_KEYFRAME_ANIMATION_SOURCE_H */
