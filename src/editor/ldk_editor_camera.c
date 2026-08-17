#include "ldk_editor_internal.h"

#include <ldk.h>
#include <ldk_raycast.h>
#include <component/ldk_camera.h>
#include <component/ldk_mesh_source.h>
#include <component/ldk_transform.h>
#include <module/ldk_asset_manager.h>
#include <module/ldk_ecs.h>
#include <module/ldk_scenegraph.h>

#include <float.h>
#include <math.h>
#include <string.h>

#define LDK_EDITOR_CAMERA_ORBIT_SENSITIVITY 0.005f
#define LDK_EDITOR_CAMERA_ZOOM_STEP 0.85f
#define LDK_EDITOR_CAMERA_MIN_DISTANCE 0.05f
#define LDK_EDITOR_CAMERA_MAX_DISTANCE 10000.0f
#define LDK_EDITOR_CAMERA_MAX_PITCH 1.55334306f

static bool s_editor_camera_rect_contains(
    LDKUIRect const *rect, LDKPoint point)
{
  return rect != NULL && (float)point.x >= rect->x &&
         (float)point.y >= rect->y &&
         (float)point.x < rect->x + rect->w &&
         (float)point.y < rect->y + rect->h;
}

static bool s_editor_scene_view_ray_get(
    LDKEditorContext *editor, LDKPoint cursor, LDKRay *out_ray)
{
  LDKUIRect const *rect;
  Mat4 view;
  Mat4 projection;
  Mat4 inverse_view_projection;
  Vec3 near_position;
  Vec3 far_position;
  float aspect;
  float ndc_x;
  float ndc_y;
  bool inverse_ok;

  if (editor == NULL || editor->renderer == NULL || out_ray == NULL ||
      editor->renderer->game_width == 0 ||
      editor->renderer->game_height == 0)
  {
    return false;
  }

  rect = &editor->gizmo.scene_view_rect;
  if (rect->w <= 0.0f || rect->h <= 0.0f ||
      !s_editor_camera_rect_contains(rect, cursor))
  {
    return false;
  }

  aspect = (float)editor->renderer->game_width /
           (float)editor->renderer->game_height;
  if (!ldk_camera_get_view_matrix(editor->editor_camera, &view) ||
      !ldk_camera_get_projection_matrix(
          editor->editor_camera, aspect, &projection))
  {
    return false;
  }

  inverse_view_projection =
      mat4_inverse_full(mat4_mul(projection, view), &inverse_ok);
  if (!inverse_ok)
  {
    return false;
  }

  // UI coordinates grow downward while NDC grows upward. Keep this mapping
  // symmetrical with the gizmo's world-to-scene projection.
  ndc_x = (((float)cursor.x - rect->x) / rect->w) * 2.0f - 1.0f;
  ndc_y = 1.0f - (((float)cursor.y - rect->y) / rect->h) * 2.0f;
  near_position = mat4_mul_point(
      inverse_view_projection, vec3_make(ndc_x, ndc_y, -1.0f));
  far_position = mat4_mul_point(
      inverse_view_projection, vec3_make(ndc_x, ndc_y, 1.0f));

  return ldk_ray_make(
      near_position, vec3_sub(far_position, near_position), out_ray);
}

static void s_editor_scene_view_pick(
    LDKEditorContext *editor, LDKPoint cursor)
{
  LDKECS *ecs;
  LDKAssetManager *asset_manager;
  XArray *mesh_sources;
  XArray *mesh_owners;
  LDKRay ray;
  LDKEntity picked_entity = x_handle_null();
  float nearest_distance = FLT_MAX;
  u32 mesh_count;

  if (editor == NULL ||
      !s_editor_scene_view_ray_get(editor, cursor, &ray))
  {
    return;
  }

  ecs = ldk_module_get(LDK_MODULE_ECS);
  asset_manager = ldk_module_get(LDK_MODULE_ASSET_MANAGER);
  if (ecs == NULL || asset_manager == NULL)
  {
    return;
  }

  mesh_sources = ldk_component_store_get(
      &ecs->component, LDK_COMPONENT_TYPE_MESH_SOURCE);
  mesh_owners = ldk_component_owners_get(
      &ecs->component, LDK_COMPONENT_TYPE_MESH_SOURCE);

  if (mesh_sources == NULL || mesh_owners == NULL)
  {
    editor->selected_entity = x_handle_null();
    return;
  }

  mesh_count = x_array_count(mesh_sources);
  if (x_array_count(mesh_owners) < mesh_count)
  {
    mesh_count = x_array_count(mesh_owners);
  }

  for (u32 i = 0; i < mesh_count; ++i)
  {
    const LDKMeshSource *mesh_source = x_array_get(mesh_sources, i);
    const LDKEntity *entity = x_array_get(mesh_owners, i);
    const LDKAssetMeshData *mesh_data;
    LDKRaycastHit hit;
    Mat4 world;

    if (mesh_source == NULL || entity == NULL ||
        !ldk_entity_is_alive(&ecs->entity, *entity) ||
        ldk_entity_internal_flags_has(
            &ecs->entity, *entity, LDK_ENTITY_INTERNAL_EDITOR) ||
        !ldk_transform_get_world_matrix(*entity, &world))
    {
      continue;
    }

    mesh_data = ldk_asset_manager_mesh_get_const(
        asset_manager, mesh_source->source_asset);
    if (mesh_data == NULL ||
        !ldk_raycast_mesh_transformed(ray, &mesh_data->mesh, world, &hit))
    {
      continue;
    }

    if (hit.distance < nearest_distance)
    {
      nearest_distance = hit.distance;
      picked_entity = *entity;
    }
  }

  editor->selected_entity = picked_entity;
}

static Vec3 s_editor_camera_offset(
    LDKEditorCameraControllerState const *controller)
{
  float horizontal = controller->distance * cosf(controller->pitch);

  return vec3_make(
      horizontal * sinf(controller->yaw),
      controller->distance * sinf(controller->pitch),
      horizontal * cosf(controller->yaw));
}

static bool s_editor_camera_controller_initialize(
    LDKEditorContext *editor)
{
  LDKEditorCameraControllerState *controller;
  Mat4 camera_world;
  Vec3 camera_position;
  Vec3 offset;

  if (editor == NULL ||
      !ldk_camera_get_world_matrix(editor->editor_camera, &camera_world))
  {
    return false;
  }

  controller = &editor->camera_controller;
  memset(controller, 0, sizeof(*controller));
  controller->entity = editor->editor_camera;
  controller->pivot = vec3_make(0.0f, 0.0f, 0.0f);

  camera_position = vec3_make(
      camera_world.m[12], camera_world.m[13], camera_world.m[14]);
  offset = vec3_sub(camera_position, controller->pivot);
  controller->distance = vec3_len(offset);
  if (controller->distance <= LDK_EDITOR_CAMERA_MIN_DISTANCE)
  {
    controller->distance = 10.0f;
    offset = vec3_make(0.0f, 0.0f, controller->distance);
  }

  controller->yaw = atan2f(offset.x, offset.z);
  controller->pitch = asinf(float_clamp(
      offset.y / controller->distance, -1.0f, 1.0f));
  controller->initialized = true;
  return true;
}

static bool s_editor_camera_apply(LDKEditorContext *editor)
{
  LDKEditorCameraControllerState *controller;
  Vec3 position;

  if (editor == NULL)
  {
    return false;
  }

  controller = &editor->camera_controller;
  position = vec3_add(
      controller->pivot, s_editor_camera_offset(controller));

  return ldk_transform_set_local_position(
             editor->editor_camera, position) &&
         ldk_camera_look_at(editor->editor_camera, controller->pivot) &&
         ldk_scenegraph_update_entity(editor->editor_camera);
}

static bool s_editor_camera_pan(LDKEditorContext *editor,
    LDKCamera const *camera, float cursor_x, float cursor_y)
{
  LDKEditorCameraControllerState *controller;
  Mat4 camera_world;
  Vec3 right;
  Vec3 up;
  Vec3 movement;
  float units_per_pixel;

  if (editor == NULL || camera == NULL ||
      editor->gizmo.scene_view_rect.h <= 0.0f ||
      !ldk_camera_get_world_matrix(editor->editor_camera, &camera_world))
  {
    return false;
  }

  controller = &editor->camera_controller;
  if (camera->projection == LDK_CAMERA_PROJECTION_ORTHOGRAPHIC)
  {
    units_per_pixel = camera->orthographic_height /
                      editor->gizmo.scene_view_rect.h;
  }
  else
  {
    units_per_pixel =
        2.0f * controller->distance * tanf(camera->fov_y * 0.5f) /
        editor->gizmo.scene_view_rect.h;
  }

  right = vec3_make(
      camera_world.m[0], camera_world.m[1], camera_world.m[2]);
  up = vec3_make(
      camera_world.m[4], camera_world.m[5], camera_world.m[6]);
  movement = vec3_add(
      vec3_mul(right, -cursor_x * units_per_pixel),
      vec3_mul(up, cursor_y * units_per_pixel));
  controller->pivot = vec3_add(controller->pivot, movement);
  return true;
}

void ldki_editor_camera_update(LDKEditorContext *editor, float delta_time)
{
  LDKEditorCameraControllerState *controller;
  LDKMouseState mouse;
  LDKCamera *camera;
  LDKPoint cursor;
  bool inside;
  bool orbit_pressed;
  bool pan_pressed;
  bool changed = false;
  float cursor_x;
  float cursor_y;

  (void)delta_time;

  if (editor == NULL || !editor->gizmo.scene_view_visible ||
      x_handle_is_null(editor->editor_camera))
  {
    return;
  }

  controller = &editor->camera_controller;
  if (!controller->initialized ||
      !ldki_editor_entity_equal(controller->entity, editor->editor_camera))
  {
    if (!s_editor_camera_controller_initialize(editor))
    {
      return;
    }
  }

  camera = (LDKCamera *)ldk_ecs_component_get(
      editor->editor_camera, LDK_COMPONENT_TYPE_CAMERA);
  if (camera == NULL)
  {
    return;
  }

  ldk_os_mouse_state_get(&mouse);
  cursor = ldk_os_mouse_cursor(&mouse);
  inside = s_editor_camera_rect_contains(
      &editor->gizmo.scene_view_rect, cursor);
  orbit_pressed = ldk_os_mouse_button_is_pressed(
      &mouse, LDK_MOUSE_BUTTON_RIGHT);
  pan_pressed = ldk_os_mouse_button_is_pressed(
      &mouse, LDK_MOUSE_BUTTON_MIDDLE);

  if (!orbit_pressed)
  {
    controller->orbiting = false;
  }
  if (!pan_pressed)
  {
    controller->panning = false;
  }

  if (!editor->gizmo.dragging && inside &&
      ldk_os_mouse_button_down(&mouse, LDK_MOUSE_BUTTON_RIGHT))
  {
    controller->orbiting = true;
    controller->panning = false;
    controller->last_cursor = cursor;
  }
  else if (!editor->gizmo.dragging && inside &&
           ldk_os_mouse_button_down(&mouse, LDK_MOUSE_BUTTON_MIDDLE))
  {
    controller->panning = true;
    controller->orbiting = false;
    controller->last_cursor = cursor;
  }

  cursor_x = (float)(cursor.x - controller->last_cursor.x);
  cursor_y = (float)(cursor.y - controller->last_cursor.y);
  if (controller->orbiting && orbit_pressed)
  {
    controller->yaw -= cursor_x * LDK_EDITOR_CAMERA_ORBIT_SENSITIVITY;
    controller->pitch += cursor_y * LDK_EDITOR_CAMERA_ORBIT_SENSITIVITY;
    controller->pitch = float_clamp(controller->pitch,
        -LDK_EDITOR_CAMERA_MAX_PITCH, LDK_EDITOR_CAMERA_MAX_PITCH);
    controller->last_cursor = cursor;
    changed = cursor_x != 0.0f || cursor_y != 0.0f;
  }
  else if (controller->panning && pan_pressed)
  {
    changed = s_editor_camera_pan(editor, camera, cursor_x, cursor_y) &&
              (cursor_x != 0.0f || cursor_y != 0.0f);
    controller->last_cursor = cursor;
  }

  if (!editor->gizmo.dragging && inside && mouse.wheel_delta != 0)
  {
    float wheel_steps = (float)mouse.wheel_delta / 120.0f;

    if (camera->projection == LDK_CAMERA_PROJECTION_ORTHOGRAPHIC)
    {
      camera->orthographic_height = float_clamp(
          camera->orthographic_height *
              powf(LDK_EDITOR_CAMERA_ZOOM_STEP, wheel_steps),
          LDK_EDITOR_CAMERA_MIN_DISTANCE,
          LDK_EDITOR_CAMERA_MAX_DISTANCE);
    }
    else
    {
      controller->distance = float_clamp(
          controller->distance *
              powf(LDK_EDITOR_CAMERA_ZOOM_STEP, wheel_steps),
          LDK_EDITOR_CAMERA_MIN_DISTANCE,
          LDK_EDITOR_CAMERA_MAX_DISTANCE);
    }
    changed = true;
  }

  if (changed)
  {
    s_editor_camera_apply(editor);
  }

  if (inside && !editor->gizmo.dragging &&
      editor->gizmo.hovered_axis == LDK_EDITOR_GIZMO_AXIS_NONE &&
      ldk_os_mouse_button_up(&mouse, LDK_MOUSE_BUTTON_LEFT))
  {
    s_editor_scene_view_pick(editor, cursor);
  }
}
