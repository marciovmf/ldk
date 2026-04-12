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
#include <stdx/stdx_math.h>

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

  uint32_t flags;
} LDKTransform;

#endif  // LDK_COMPONENT_TRANSFORM_H
