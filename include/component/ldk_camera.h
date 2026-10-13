/**
 * # Camera component
 *
 * Camera component data.
 */

#ifndef LDK_CAMERA_H
#define LDK_CAMERA_H

#include <ldk_common.h>
#include <ldk_resource.h>
#include <ldk_skybox_asset.h>
#include <module/ldk_entity.h>
#include <module/ldk_component.h>
#include <stdx/stdx_math.h>

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

  //@enum
  typedef enum LDKCameraProjection
  {
    LDK_CAMERA_PROJECTION_PERSPECTIVE = 0,
    LDK_CAMERA_PROJECTION_ORTHOGRAPHIC
  } LDKCameraProjection;

  //@enum
  typedef enum LDKCameraRole
  {
    LDK_CAMERA_ROLE_NONE = 0,
    LDK_CAMERA_ROLE_MAIN,
    LDK_CAMERA_ROLE_EDITOR,
    LDK_CAMERA_ROLE_RENDER_TARGET
  } LDKCameraRole;

  //@enum
  typedef enum LDKCameraBackgroundMode
  {
    LDK_CAMERA_BACKGROUND_COLOR = 0,
    LDK_CAMERA_BACKGROUND_SKYBOX,
    LDK_CAMERA_BACKGROUND_NONE
  } LDKCameraBackgroundMode;

  //@component
  typedef struct LDKCamera
  {
    LDKCameraProjection projection;
    LDKCameraRole role;
    float fov_y;
    float orthographic_height;
    float near_plane;
    float far_plane;
    bool enabled;
    LDKCameraBackgroundMode background_mode;
    //@inspect widget=COLOR
    u32 background_color;
    LDKAssetSkybox skybox_asset;
    //@inspect hidden runtime
    u64 skybox_revision;
    //@inspect hidden runtime
    LDKResourceSkybox renderer_skybox;
    //@inspect hidden runtime
    struct LDKRenderer* renderer;
  } LDKCamera;

#ifdef LDK_ENGINE
  /**
   * @brief Build the ECS registration descriptor for the camera component.
   * @param initial_capacity Initial number of entries reserved in the registry.
   * @return A descriptor for the requested component or resource.
   */
  LDK_API LDKComponentDesc ldk_camera_component_desc(u32 initial_capacity);
#endif // LDK_ENGINE

/**
 * @brief Orient a camera entity so it looks toward a world-space target.
 * @param entity Entity to inspect or modify.
 * @param target Target used by this operation.
 * @return True if the operation succeeds; false otherwise.
 */
LDK_API bool ldk_camera_look_at(LDKEntity entity, Vec3 target);
/**
 * @brief Get camera world matrix.
 * @param entity Entity to inspect or modify.
 * @param out_world Receives the computed world transformation matrix.
 * @return True if the operation succeeds; false otherwise.
 */
LDK_API bool ldk_camera_get_world_matrix(LDKEntity entity, Mat4* out_world);
/**
 * @brief Get camera view matrix.
 * @param entity Entity to inspect or modify.
 * @param out_view Receives the camera view matrix.
 * @return True if the operation succeeds; false otherwise.
 */
LDK_API bool ldk_camera_get_view_matrix(LDKEntity entity, Mat4* out_view);
/**
 * @brief Get camera projection matrix.
 * @param entity Entity to inspect or modify.
 * @param aspect Viewport aspect ratio.
 * @param out_projection Receives the projection matrix.
 * @return True if the operation succeeds; false otherwise.
 */
LDK_API bool ldk_camera_get_projection_matrix(LDKEntity entity, float aspect, Mat4* out_projection);

#ifdef __cplusplus
}
#endif

#endif // LDK_CAMERA_H
