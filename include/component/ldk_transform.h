/**
 * # Transform component
 *
 * LDK Transform Component
 *
 * Coordinate System Conventions:
 * - Right-handed coordinate system
 * - +X = right
 * - +Y = up
 * - -Z = forward
 */

#ifndef LDK_TRANSFORM_H
#define LDK_TRANSFORM_H

#include <ldk_common.h>
#include <module/ldk_entity.h>
#include <module/ldk_component.h>
#include <stdx/stdx_math.h>

#ifdef __cplusplus
extern "C"
{
#endif

  /** @group ldk.component.transform */
  typedef enum LDKTransformFlags
  {
    LDK_TRANSFORM_FLAG_NONE = 0,
    LDK_TRANSFORM_FLAG_WORLD_DIRTY = 1 << 0,
  } LDKTransformFlags;

  /** @group ldk.component.transform */
  //@component
  typedef struct LDKTransform
  {
    u32 flags;
    Vec3 local_position;
    Vec3 local_scale;
    //@inspect widget=EULER
    Quat local_rotation;
    //@inspect hidden
    Mat4 world_matrix;
    //@inspect hidden
    LDKEntity parent;
    //@inspect hidden
    LDKEntity first_child;
    //@inspect hidden
    LDKEntity next_sibling;
    //@inspect hidden
    LDKEntity prev_sibling;
  } LDKTransform;

  /**
   * @brief Create default settings for transform.
   * @return A value initialized with the engine defaults.
   * @group ldk.component.transform
   */
  LDK_API LDKTransform ldk_transform_make_default(void);

  /**
   * @brief Set transform local position.
   * @param entity Entity to inspect or modify.
   * @param position Position used by the operation.
   * @return True if the operation succeeds; false otherwise.
   * @group ldk.component.transform
   */
  LDK_API bool ldk_transform_set_local_position(
      LDKEntity entity, Vec3 position);
  /**
   * @brief Set transform local rotation.
   * @param entity Entity to inspect or modify.
   * @param rotation Rotation used by the operation.
   * @return True if the operation succeeds; false otherwise.
   * @group ldk.component.transform
   */
  LDK_API bool ldk_transform_set_local_rotation(
      LDKEntity entity, Quat rotation);
  /**
   * @brief Set transform local scale.
   * @param entity Entity to inspect or modify.
   * @param scale Scale used by the operation.
   * @return True if the operation succeeds; false otherwise.
   * @group ldk.component.transform
   */
  LDK_API bool ldk_transform_set_local_scale(LDKEntity entity, Vec3 scale);
  /**
   * @brief Get transform local position.
   * @param entity Entity to inspect or modify.
   * @param out_position Receives the resulting position.
   * @return True if the operation succeeds; false otherwise.
   * @group ldk.component.transform
   */
  LDK_API bool ldk_transform_get_local_position(
      LDKEntity entity, Vec3 *out_position);
  /**
   * @brief Get transform local rotation.
   * @param entity Entity to inspect or modify.
   * @param out_rotation Receives the resulting rotation.
   * @return True if the operation succeeds; false otherwise.
   * @group ldk.component.transform
   */
  LDK_API bool ldk_transform_get_local_rotation(
      LDKEntity entity, Quat *out_rotation);
  /**
   * @brief Get transform local scale.
   * @param entity Entity to inspect or modify.
   * @param out_scale Receives the resulting scale.
   * @return True if the operation succeeds; false otherwise.
   * @group ldk.component.transform
   */
  LDK_API bool ldk_transform_get_local_scale(LDKEntity entity, Vec3 *out_scale);
  /**
   * @brief Get transform world matrix.
   * @param entity Entity to inspect or modify.
   * @param out_world_matrix Receives the computed world transformation matrix.
   * @return True if the operation succeeds; false otherwise.
   * @group ldk.component.transform
   */
  LDK_API bool ldk_transform_get_world_matrix(
      LDKEntity entity, Mat4 *out_world_matrix);
  /**
   * @brief Set transform parent.
   * @param child_entity Entity whose parent is being set.
   * @param parent_entity Parent entity to assign.
   * @return True if the operation succeeds; false otherwise.
   * @group ldk.component.transform
   */
  LDK_API bool ldk_transform_set_parent(
      LDKEntity child_entity, LDKEntity parent_entity);
  /**
   * @brief Get transform parent.
   * @param entity Entity to inspect or modify.
   * @return The requested value.
   * @group ldk.component.transform
   */
  LDK_API LDKEntity ldk_transform_get_parent(LDKEntity entity);
  /**
   * @brief Mark an entity transform dirty so its world state can be recalculated.
   * @param entity Entity to inspect or modify.
   * @return True if the operation succeeds; false otherwise.
   * @group ldk.component.transform
   */
  LDK_API bool ldk_transform_mark_dirty(LDKEntity entity);

#ifdef LDK_ENGINE
  /**
   * @brief Build the ECS registration descriptor for the transform component.
   * @param initial_capacity Initial number of entries reserved in the registry.
   * @return A descriptor for the requested component or resource.
   * @group ldk.component.transform
   */
  LDK_API LDKComponentDesc ldk_transform_component_desc(u32 initial_capacity);
#endif // LDK_ENGINE

#ifdef __cplusplus
}
#endif

#endif // LDK_TRANSFORM_H
