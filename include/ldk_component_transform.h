/*
 * @file  ldk_component_transform.h
 * @brief LDK Transform Component
 * 
 * Coordinate System Conventions:
 * 
 * - Right-handed coordinate system
 * - +X = right
 * - +Y = up
 * - -Z = forward
 * 
 * Math Conventions (stdx_math):
 * 
 * - Column-major matrices
 * - Column vectors (v' = M * v)
 * - Matrix multiplication: mat4_mul(a, b) = a * b (apply b, then a)
 * 
 * Transform Rules:
 * 
 * - Local transform is defined by (position, rotation, scale) in TRS form
 * - World transform is derived, not stored as authoritative state
 * - Forward movement decreases Z in world space
 * - World matrix is computed as:
 * 
 *     world = parent_world * local
 * 
 * - Root transforms use:
 * 
 *     world = local
 * 
 * - No shear is supported (TRS only)
 * 
 * Hierarchy:
 * 
 * - Transform hierarchy is represented via parent/child/sibling links
 * - Only entities with a transform component may participate in hierarchy
 * - Cycles are forbidden
 * 
 * Direction Conventions:
 * 
 * - Right   = +X
 * - Up      = +Y
 * - Forward = -Z
 */

#ifndef LDK_COMPONENT_TRANSFORM_H
#define LDK_COMPONENT_TRANSFORM_H

#include <ldk_common.h>
#include <ldk_entity.h>
#include <ldk_component.h>
#include <stdx/stdx_math.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum LDKTransformFlags
{
  LDK_TRANSFORM_FLAG_NONE        = 0,
  LDK_TRANSFORM_FLAG_WORLD_DIRTY = 1 << 0,
} LDKTransformFlags;

typedef enum LDKBuiltinComponentType
{
  LDK_COMPONENT_TYPE_TRANSFORM = 1
} LDKBuiltinComponentType;

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
LDK_API bool ldk_transform_register(LDKComponentRegistry* registry, u32 initial_capacity);
LDK_API bool ldk_transform_attach(LDKEntity entity, const LDKTransform* initial_value);
LDK_API bool ldk_transform_detach(LDKEntity entity);
LDK_API bool ldk_transform_get(LDKEntity entity, LDKTransform* out_transform);
LDK_API bool ldk_transform_set(LDKEntity entity, const LDKTransform* transform);
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

#ifdef __cplusplus
}
#endif

#endif
