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

/**
 * @brief Construct the default configuration for an animation source component.
 * @return Value with autoplay/loop enabled, speed one and no selected clip.
 */
LDK_API LDKKeyFrameAnimationSource ldk_keyframe_animation_source_make_default(void);
/**
 * @brief Append a live animation asset to an entity animation source.
 * @param root Root entity containing the animation source.
 * @param animation Live asset handle; remains owned by the asset manager.
 * @return True if appended. Does not transfer ownership of the asset.
 */
LDK_API bool ldk_keyframe_animation_source_add(
    LDKEntity root, LDKAssetKeyframeAnimation animation);
/**
 * @brief Detach an asset at the given index from an animation source.
 * @param root Root entity containing the animation source.
 * @param index Zero-based animation index.
 * @return True if removed. Does not unload the asset.
 */
LDK_API bool ldk_keyframe_animation_source_remove(LDKEntity root, u32 index);
/**
 * @brief Replace a source animation entry with another live animation asset.
 * @param root Root entity containing the animation source.
 * @param index Zero-based animation index.
 * @param animation Live asset handle; remains owned by the asset manager.
 * @return True on success. Replacing the active entry resets playback.
 */
LDK_API bool ldk_keyframe_animation_source_replace(
    LDKEntity root, u32 index, LDKAssetKeyframeAnimation animation);
/**
 * @brief Select the animation index used for subsequent playback.
 * @param root Root entity containing the animation source.
 * @param index Zero-based animation index.
 * @return True for a valid index. Changing selection resets playback.
 */
LDK_API bool ldk_keyframe_animation_source_set_current(LDKEntity root, i32 index);
/**
 * @brief Start or resume the current animation and apply its current pose.
 * @param root Root entity containing the animation source.
 * @return True if a live clip is selected and the component is enabled.
 */
LDK_API bool ldk_keyframe_animation_source_play(LDKEntity root);
/**
 * @brief Pause playback while retaining the current position.
 * @param root Root entity containing the animation source.
 * @return Nothing.
 */
LDK_API void ldk_keyframe_animation_source_pause(LDKEntity root);
/**
 * @brief Stop playback, seek to zero and apply the initial pose.
 * @param root Root entity containing the animation source.
 * @return Nothing. Does not restore the pre-animation component values.
 */
LDK_API void ldk_keyframe_animation_source_stop(LDKEntity root);
/**
 * @brief Set playback time and immediately evaluate the new pose without events.
 * @param root Root entity containing the animation source.
 * @param time Time in seconds.
 * @return True if a live clip is selected. Time is clamped to its duration.
 */
LDK_API bool ldk_keyframe_animation_source_seek(LDKEntity root, float time);
/**
 * @brief Set the synchronous global callback for keyframe animation events.
 * @param handler Event handler invoked synchronously, or NULL to unregister.
 * @param user Opaque callback context.
 * @return Nothing. Passing NULL disables notifications.
 * @note Callbacks may mutate or destroy the source; do not keep event pointers.
 */
LDK_API void ldk_keyframe_animation_source_event_handler_set(
    LDKKeyframeEventFn handler, void *user);

#ifdef LDK_ENGINE
/**
 * @brief Describe the keyframe animation source component to the ECS.
 * @param initial_capacity Initial component storage capacity.
 * @return Component description with Transform as a required component.
 */
LDK_API LDKComponentDesc ldk_keyframe_animation_source_component_desc(
    u32 initial_capacity);
/**
 * @brief Advance enabled animation sources during the engine update.
 * @param delta_time Elapsed time in seconds.
 * @return Nothing. Events are dispatched before the render phase.
 */
LDK_API void ldk_keyframe_animation_source_update_all(float delta_time);
/**
 * @brief Reset runtime playback state for every registered animation source.
 * @return Nothing. Preserves assigned assets and playback configuration.
 */
LDK_API void ldk_keyframe_animation_source_reset_all(void);
#endif

#ifdef __cplusplus
}
#endif
#endif /* LDK_KEYFRAME_ANIMATION_SOURCE_H */
