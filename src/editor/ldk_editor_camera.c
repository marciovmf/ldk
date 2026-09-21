#include "ldk_editor_internal.h"
#include "ldk_ui_drag_n_drop.h"

#include <ldk.h>
#include <ldk_mesh_asset.h>
#include <ldk_raycast.h>
#include <component/ldk_camera.h>
#include <component/ldk_mesh_source.h>
#include <component/ldk_instanced_mesh_source.h>
#include <component/ldk_transform.h>
#include <module/ldk_asset_manager.h>
#include <module/ldk_ecs.h>
#include <module/ldk_scenegraph.h>

#include <float.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

#define LDK_EDITOR_CAMERA_ORBIT_SENSITIVITY 0.005f
#define LDK_EDITOR_CAMERA_ZOOM_STEP 0.85f
#define LDK_EDITOR_CAMERA_MIN_DISTANCE 0.05f
#define LDK_EDITOR_CAMERA_MAX_DISTANCE 10000.0f
#define LDK_EDITOR_CAMERA_MAX_PITCH 1.55334306f

static bool s_editor_scene_view_drop_block_pick;

static bool s_editor_scene_view_ray_get(
    LDKEditorContext *editor, LDKPoint cursor, LDKRay *out_ray);

static bool s_editor_scene_view_path_is_mesh(const XFSPath *path)
{
  XSlice extension;

  if (!path)
  {
    return false;
  }

  extension = x_fs_path_extension_as_slice(path);
  return x_slice_eq_ci(extension, x_slice("mesh"));
}

static bool s_editor_scene_view_path_in_runtree(
    LDKEditorContext *editor, const XFSPath *path)
{
  XFSPath runtree = {0};
  XFSPath relative = {0};
  const char *relative_text;

  if (!editor || !editor->project.loaded || !path)
  {
    return false;
  }

  x_fs_path_set(&runtree, editor->project.run_root_path.buf);
  x_fs_path_normalize(&runtree);

  if (!x_fs_path_common_prefix(
          x_fs_path_cstr(&runtree), x_fs_path_cstr(path), &relative))
  {
    return false;
  }

  relative_text = x_fs_path_cstr(&relative);
  return relative_text && relative_text[0] != 0 &&
         strcmp(relative_text, ".") != 0 &&
         !x_fs_path_is_absolute(&relative);
}

static void s_editor_scene_view_entities_destroy(
    LDKEntity *entities, u32 count)
{
  if (!entities)
  {
    return;
  }

  for (u32 i = count; i-- > 0;)
  {
    if (!x_handle_is_null(entities[i]))
    {
      ldk_ecs_entity_destroy(entities[i]);
    }
  }
}

static bool s_editor_scene_view_mesh_instantiate(
    LDKEditorContext *editor, const XFSPath *path, Vec3 drop_position)
{
  LDKAssetManager *assets;
  LDKMeshAssetResult result = {0};
  LDKAssetMesh asset;
  LDKEntity *entities = NULL;
  LDKEntity selected = x_handle_null();
  u32 mesh_count;
  u32 node_count;
  bool ok = false;

  if (!editor || !path || !editor->project.loaded ||
      editor->editor_state != LDK_EDITOR_STATE_STOPED ||
      editor->current_scene_path.length == 0)
  {
    ldki_editor_log_error(
        editor, "Open a scene before adding a mesh asset.");
    return false;
  }

  assets = ldk_module_get(LDK_MODULE_ASSET_MANAGER);
  if (!assets)
  {
    ldki_editor_log_error(editor, "Asset manager is not available.");
    return false;
  }

  asset = ldk_asset_manager_mesh_load_shared(
      assets, x_fs_path_cstr(path), &result);
  if (x_handle_is_null(asset.h))
  {
    ldki_editor_log_error(editor,
        result.error[0] ? result.error : "Failed to load mesh asset.");
    return false;
  }

  mesh_count = ldk_asset_manager_mesh_count(assets, asset);
  node_count = ldk_asset_manager_mesh_node_count(assets, asset);
  if (!mesh_count || !node_count)
  {
    ldki_editor_log_error(
        editor, "Mesh asset contains no authored hierarchy.");
    return false;
  }

  entities = calloc(node_count, sizeof(*entities));
  if (!entities)
  {
    ldki_editor_log_error(editor, "Failed to allocate mesh hierarchy.");
    return false;
  }

  for (u32 i = 0; i < node_count; ++i)
  {
    entities[i] = x_handle_null();
  }

  for (u32 i = 0; i < node_count; ++i)
  {
    const LDKMeshNode *node =
        ldk_asset_manager_mesh_node_at(assets, asset, i);
    LDKMeshSource *mesh_source;
    Vec3 local_position;

    if (!node || node->parent_index < LDK_MESH_NODE_NONE ||
        node->parent_index >= (i32)i ||
        node->mesh_index < LDK_MESH_INDEX_NONE ||
        node->mesh_index >= (i32)mesh_count)
    {
      ldki_editor_log_error(editor, "Mesh asset hierarchy is invalid.");
      goto cleanup;
    }

    entities[i] = ldk_ecs_entity_create();
    if (x_handle_is_null(entities[i]))
    {
      ldki_editor_log_error(editor, "Failed to create mesh node entity.");
      goto cleanup;
    }

    if (node->name[0] && !ldk_ecs_entity_name_set(entities[i], node->name))
    {
      ldki_editor_log_error(editor, "Failed to name mesh node entity.");
      goto cleanup;
    }

    local_position = node->local_position;
    if (node->parent_index == LDK_MESH_NODE_NONE)
    {
      local_position = vec3_add(local_position, drop_position);
      if (x_handle_is_null(selected))
      {
        selected = entities[i];
      }
    }

    if (!ldk_transform_set_local_position(entities[i], local_position) ||
        !ldk_transform_set_local_rotation(
            entities[i], node->local_rotation) ||
        !ldk_transform_set_local_scale(entities[i], node->local_scale))
    {
      ldki_editor_log_error(editor, "Failed to set mesh node transform.");
      goto cleanup;
    }

    if (node->mesh_index == LDK_MESH_INDEX_NONE)
    {
      continue;
    }

    mesh_source = ldk_ecs_component_add(
        entities[i], LDK_COMPONENT_TYPE_MESH_SOURCE, NULL);
    if (!mesh_source || !ldk_mesh_source_set_data(mesh_source, asset) ||
        !ldk_mesh_source_set_mesh_index(
            mesh_source, assets, (u32)node->mesh_index))
    {
      ldki_editor_log_error(editor, "Failed to create mesh source.");
      goto cleanup;
    }
  }

  /* ldk_transform_set_parent inserts at the head of the child list. Walking
   * nodes backwards preserves the authored sibling order. */
  for (u32 i = node_count; i-- > 0;)
  {
    const LDKMeshNode *node =
        ldk_asset_manager_mesh_node_at(assets, asset, i);

    if (!node)
    {
      ldki_editor_log_error(editor, "Mesh asset hierarchy is invalid.");
      goto cleanup;
    }

    if (node->parent_index != LDK_MESH_NODE_NONE &&
        !ldk_transform_set_parent(
            entities[i], entities[(u32)node->parent_index]))
    {
      ldki_editor_log_error(editor, "Failed to parent mesh node entity.");
      goto cleanup;
    }
  }

  for (u32 i = 0; i < node_count; ++i)
  {
    const LDKMeshNode *node =
        ldk_asset_manager_mesh_node_at(assets, asset, i);

    if (node && node->parent_index == LDK_MESH_NODE_NONE &&
        !ldk_scenegraph_update_entity(entities[i]))
    {
      ldki_editor_log_error(editor, "Failed to update mesh hierarchy.");
      goto cleanup;
    }
  }

  editor->selected_entity = selected;
  editor->selected_system_id = 0;
  ok = true;

cleanup:
  if (!ok)
  {
    s_editor_scene_view_entities_destroy(entities, node_count);
  }
  free(entities);
  return ok;
}

static bool s_editor_scene_view_mesh_drop(
    LDKEditorContext *editor, LDKPoint cursor)
{
  XSmallstr payload = {0};
  XFSPath path = {0};
  LDKRay ray;
  LDKRaycastHit hit;
  u32 type = 0;

  if (!editor ||
      !ldk_ui_drag_n_drop_payload_get_and_remove(&type, &payload) ||
      type != LDK_EDITOR_DRAG_N_DROP_PAYLOAD_FILE_PATH)
  {
    return false;
  }

  x_fs_path_set(&path, payload.buf);
  x_fs_path_normalize(&path);
  if (!s_editor_scene_view_path_is_mesh(&path))
  {
    return false;
  }

  if (!x_fs_path_is_file(&path))
  {
    ldki_editor_log_error(editor, "Dropped mesh file does not exist.");
    return true;
  }

  if (!s_editor_scene_view_path_in_runtree(editor, &path))
  {
    ldki_editor_log_error(editor,
        "Dropped mesh file must be inside the project runtree.");
    return true;
  }

  if (!s_editor_scene_view_ray_get(editor, cursor, &ray) ||
      !ldk_raycast_plane(ray, vec3_make(0.0f, 0.0f, 0.0f),
          vec3_make(0.0f, 1.0f, 0.0f), &hit))
  {
    ldki_editor_log_error(editor, "Could not resolve mesh drop position.");
    return true;
  }

  (void)s_editor_scene_view_mesh_instantiate(editor, &path, hit.position);
  return true;
}

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

void ldki_editor_scene_view_pick(
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

  if (s_editor_scene_view_drop_block_pick)
  {
    return;
  }

  if (editor == NULL || editor->gizmo.mode == LDK_EDITOR_GIZMO_MODE_PAN ||
      editor->camera_controller.pan_block_pick ||
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

  const u32 mesh_types[] = {LDK_COMPONENT_TYPE_MESH_SOURCE,
      LDK_COMPONENT_TYPE_INSTANCED_MESH_SOURCE};
  for (u32 type_index = 0; type_index < 2; ++type_index)
  {
    mesh_sources = ldk_component_store_get(
        &ecs->component, mesh_types[type_index]);
    mesh_owners = ldk_component_owners_get(
        &ecs->component, mesh_types[type_index]);

    if (mesh_sources == NULL || mesh_owners == NULL)
    {
      continue;
    }

    mesh_count = x_array_count(mesh_sources);
    if (x_array_count(mesh_owners) < mesh_count)
    {
      mesh_count = x_array_count(mesh_owners);
    }

    for (u32 i = 0; i < mesh_count; ++i)
    {
      const void *component = x_array_get(mesh_sources, i);
      const LDKInstancedMeshSource *instances =
          type_index == 1 ? component : NULL;
      const LDKMeshSource *mesh_source =
          instances ? &instances->source : component;
      const LDKEntity *entity = x_array_get(mesh_owners, i);
      const LDKMeshData *mesh_data;
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

      mesh_data = ldk_asset_manager_mesh_data_at(
          asset_manager, mesh_source->source_asset, mesh_source->mesh_index);
      if (mesh_data == NULL)
      {
        continue;
      }

      u32 count = instances ? instances->instance_count : 1;
      for (u32 instance = 0; instance < count; ++instance)
      {
        Mat4 instance_world = instances
            ? mat4_mul(world, instances->instances[instance]) : world;
        if (ldk_raycast_mesh_transformed(
                ray, mesh_data, instance_world, &hit) &&
            hit.distance < nearest_distance)
        {
          nearest_distance = hit.distance;
          picked_entity = *entity;
        }
      }
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
  LDKKeyboardState keyboard;
  LDKCamera *camera;
  LDKPoint cursor;
  bool inside;
  bool orbit_pressed;
  bool pan_pressed;
  bool pan_modifier;
  bool changed = false;
  float cursor_x;
  float cursor_y;

  (void)delta_time;

  if (editor == NULL)
    return;

  s_editor_scene_view_drop_block_pick = false;
  // Keep the release frame blocked so a pan cannot select an object/icon.
  editor->camera_controller.pan_block_pick =
      editor->camera_controller.panning &&
      editor->camera_controller.pan_with_left;
  ldk_os_mouse_state_get(&mouse);
  ldk_os_keyboard_state_get(&keyboard);
  pan_modifier = ldk_os_keyboard_key_is_pressed(
      &keyboard, LDK_KEYCODE_LEFT_CONTROL);

  if (!editor->gizmo.scene_view_visible ||
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

  cursor = ldk_os_mouse_cursor(&mouse);
  inside = s_editor_camera_rect_contains(
      &editor->gizmo.scene_view_rect, cursor);

  if (!editor->gizmo.dragging && inside &&
      ldk_os_mouse_button_up(&mouse, LDK_MOUSE_BUTTON_LEFT) &&
      s_editor_scene_view_mesh_drop(editor, cursor))
  {
    s_editor_scene_view_drop_block_pick = true;
  }

  orbit_pressed = ldk_os_mouse_button_is_pressed(
      &mouse, LDK_MOUSE_BUTTON_RIGHT);
  pan_pressed = ldk_os_mouse_button_is_pressed(&mouse,
      controller->pan_with_left ? LDK_MOUSE_BUTTON_LEFT : LDK_MOUSE_BUTTON_MIDDLE);

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
      (ldk_os_mouse_button_down(&mouse, LDK_MOUSE_BUTTON_MIDDLE) ||
          ((editor->gizmo.mode == LDK_EDITOR_GIZMO_MODE_PAN || pan_modifier) &&
              ldk_os_mouse_button_down(&mouse, LDK_MOUSE_BUTTON_LEFT))))
  {
    controller->pan_with_left =
        !ldk_os_mouse_button_down(&mouse, LDK_MOUSE_BUTTON_MIDDLE);
    controller->panning = true;
    controller->pan_block_pick = controller->pan_with_left;
    controller->orbiting = false;
    controller->last_cursor = cursor;
    pan_pressed = true;
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

}
