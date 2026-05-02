/**
 * @file ldk_camera.h
 * @brief Camera component data.
 */

#ifndef LDK_CAMERA_H
#define LDK_CAMERA_H

#include <ldk_common.h>
#include <module/ldk_entity.h>
#include <module/ldk_component.h>
#include <stdx/stdx_math.h>

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

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

#ifdef LDK_ENGINE
  LDK_API LDKComponentDesc ldk_camera_component_desc(u32 initial_capacity);
#endif // LDK_ENGINE

#ifdef __cplusplus
}
#endif

#endif // LDK_CAMERA_H
