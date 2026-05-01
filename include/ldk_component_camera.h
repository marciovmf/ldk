/**
 * @file ldk_component_camera.h
 * @brief Camera component API.
 */

#ifndef LDK_COMPONENT_CAMERA_H
#define LDK_COMPONENT_CAMERA_H

#ifdef __cplusplus
extern "C" {
#endif

#include <ldk_common.h>
#include <ldk_entity.h>
#include <ldk_component.h>

#include <stdx/stdx_math.h>

#include <stdbool.h>

/*
 * Required integration points:
 * - LDK_COMPONENT_TYPE_CAMERA in LDKComponentType.
 * - LDK_ENTITY_INTERNAL_HAS_CAMERA in LDKEntityInternalFlags.
 */

typedef enum LDKCameraProjection
{
  LDK_CAMERA_PROJECTION_PERSPECTIVE = 0,
  LDK_CAMERA_PROJECTION_ORTHOGRAPHIC
} LDKCameraProjection;

typedef enum LDKCameraRole
{
  LDK_CAMERA_ROLE_NONE = 0,
  LDK_CAMERA_ROLE_MAIN,
  LDK_CAMERA_ROLE_EDITOR,
  LDK_CAMERA_ROLE_RENDER_TARGET
} LDKCameraRole;

typedef struct LDKCamera
{
  LDKCameraProjection projection;
  LDKCameraRole role;
  float fov_y;
  float orthographic_height;
  float near_plane;
  float far_plane;
  bool enabled;
} LDKCamera;

LDKCamera ldk_camera_make_default(void);

bool ldk_camera_register(LDKComponentRegistry* registry, u32 initial_capacity);
bool ldk_camera_attach(LDKEntity entity, const LDKCamera* initial_value);
bool ldk_camera_detach(LDKEntity entity);

bool ldk_camera_get(LDKEntity entity, LDKCamera* out_camera);
bool ldk_camera_set(LDKEntity entity, const LDKCamera* camera);

bool ldk_camera_set_enabled(LDKEntity entity, bool enabled);
bool ldk_camera_set_role(LDKEntity entity, LDKCameraRole role);
bool ldk_camera_set_perspective(LDKEntity entity, float fov_y, float near_plane, float far_plane);
bool ldk_camera_set_orthographic(LDKEntity entity, float orthographic_height, float near_plane, float far_plane);

bool ldk_camera_get_enabled(LDKEntity entity, bool* out_enabled);
bool ldk_camera_get_role(LDKEntity entity, LDKCameraRole* out_role);

#ifdef __cplusplus
}
#endif

#endif
