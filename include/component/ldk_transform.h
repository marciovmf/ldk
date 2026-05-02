/*
 * @file  ldk_transform.h
 * @brief LDK Transform Component
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
extern "C" {
#endif

  typedef enum LDKTransformFlags
  {
    LDK_TRANSFORM_FLAG_NONE        = 0,
    LDK_TRANSFORM_FLAG_WORLD_DIRTY = 1 << 0,
  } LDKTransformFlags;

  typedef struct LDKTransform
  {
    Vec3 local_position;
    Quat local_rotation;
    Vec3 local_scale;
    Mat4 world_matrix;
    LDKEntity parent;
    LDKEntity first_child;
    LDKEntity next_sibling;
    LDKEntity prev_sibling;
    u32 flags;
  } LDKTransform;

  LDK_API LDKTransform ldk_transform_make_default(void);

  LDK_API bool ldk_transform_set_local_position(LDKEntity entity, Vec3 position);
  LDK_API bool ldk_transform_set_local_rotation(LDKEntity entity, Quat rotation);
  LDK_API bool ldk_transform_set_local_scale(LDKEntity entity, Vec3 scale);
  LDK_API bool ldk_transform_get_local_position(LDKEntity entity, Vec3* out_position);
  LDK_API bool ldk_transform_get_local_rotation(LDKEntity entity, Quat* out_rotation);
  LDK_API bool ldk_transform_get_local_scale(LDKEntity entity, Vec3* out_scale);
  LDK_API bool ldk_transform_get_world_matrix(LDKEntity entity, Mat4* out_world_matrix);
  LDK_API bool ldk_transform_set_parent(LDKEntity child_entity, LDKEntity parent_entity);
  LDK_API LDKEntity ldk_transform_get_parent(LDKEntity entity);
  LDK_API bool ldk_transform_mark_dirty(LDKEntity entity);

#ifdef LDK_ENGINE
  LDK_API LDKComponentDesc ldk_transform_component_desc(u32 initial_capacity);
#endif // LDK_ENGINE

#ifdef __cplusplus
}
#endif

#endif // LDK_TRANSFORM_H
