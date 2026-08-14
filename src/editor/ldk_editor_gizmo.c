#include "ldk_editor_internal.h"

#include <ldk.h>
#include <ldk_mesh.h>
#include <component/ldk_camera.h>
#include <component/ldk_transform.h>
#include <module/ldk_ecs.h>
#include <module/ldk_scenegraph.h>

#include <math.h>
#include <string.h>

#define LDK_EDITOR_GIZMO_AXIS_COUNT 3u
#define LDK_EDITOR_GIZMO_PIXEL_LENGTH 96.0f
#define LDK_EDITOR_GIZMO_MIN_WORLD_LENGTH 0.05f
#define LDK_EDITOR_GIZMO_PICK_RADIUS 12.0f
#define LDK_EDITOR_GIZMO_MAX_HIERARCHY_DEPTH 256u
#define LDK_EDITOR_GIZMO_INTERSECTION_EPSILON 0.00001f

typedef struct LDKEditorGizmoRay
{
  Vec3 origin;
  Vec3 direction;
} LDKEditorGizmoRay;

static const u32 s_gizmo_axis_colors[LDK_EDITOR_GIZMO_AXIS_COUNT] = {
    0xFF4040FFu,
    0xFF40D040u,
    0xFFFF6040u,
};

static const u32 s_gizmo_highlight_color = 0xFF80FFFFu;

static bool s_editor_gizmo_world_rotation_get(
    LDKEntity entity, u32 depth, Quat *out_rotation)
{
  LDKEntity parent;
  Quat local_rotation;
  Quat parent_rotation;

  if (out_rotation == NULL ||
      depth >= LDK_EDITOR_GIZMO_MAX_HIERARCHY_DEPTH ||
      !ldk_transform_get_local_rotation(entity, &local_rotation))
  {
    return false;
  }

  parent = ldk_transform_get_parent(entity);
  if (x_handle_is_null(parent))
  {
    *out_rotation = quat_norm(local_rotation);
    return true;
  }

  if (!s_editor_gizmo_world_rotation_get(
          parent, depth + 1u, &parent_rotation))
  {
    return false;
  }

  *out_rotation = quat_norm(quat_mul(parent_rotation, local_rotation));
  return true;
}

static void s_editor_gizmo_axes_from_orientation(
    Mat4 orientation, Vec3 out_axes[3])
{
  out_axes[0] = vec3_make(
      orientation.m[0], orientation.m[1], orientation.m[2]);
  out_axes[1] = vec3_make(
      orientation.m[4], orientation.m[5], orientation.m[6]);
  out_axes[2] = vec3_make(
      orientation.m[8], orientation.m[9], orientation.m[10]);
}

static bool s_editor_gizmo_orientation_get(LDKEditorContext *editor,
    LDKEntity selected, Mat4 *out_orientation, Vec3 out_axes[3])
{
  Mat4 orientation = mat4_identity();

  if (editor == NULL || out_orientation == NULL || out_axes == NULL)
  {
    return false;
  }

  if (editor->gizmo.space == LDK_EDITOR_GIZMO_SPACE_LOCAL)
  {
    Quat world_rotation;

    if (!s_editor_gizmo_world_rotation_get(
            selected, 0u, &world_rotation))
    {
      return false;
    }

    orientation = mat4_from_quat(world_rotation);
  }

  s_editor_gizmo_axes_from_orientation(orientation, out_axes);
  *out_orientation = orientation;
  return true;
}

static bool s_editor_gizmo_meshes_are_valid(LDKEditorContext *editor)
{
  if (editor == NULL || editor->renderer == NULL ||
      !editor->gizmo.initialized)
  {
    return false;
  }

  for (u32 i = 0; i < LDK_EDITOR_GIZMO_AXIS_COUNT; ++i)
  {
    if (!ldk_renderer_mesh_is_valid(
            editor->renderer, editor->gizmo.translation_axis_meshes[i]))
    {
      return false;
    }
  }

  return ldk_renderer_mesh_is_valid(
      editor->renderer, editor->gizmo.translation_highlight_mesh);
}

static void s_editor_gizmo_resources_destroy(LDKEditorContext *editor)
{
  if (editor == NULL || editor->renderer == NULL)
  {
    return;
  }

  for (u32 i = 0; i < LDK_EDITOR_GIZMO_AXIS_COUNT; ++i)
  {
    ldk_renderer_mesh_destroy(
        editor->renderer, editor->gizmo.translation_axis_meshes[i]);
  }

  ldk_renderer_mesh_destroy(
      editor->renderer, editor->gizmo.translation_highlight_mesh);

  memset(editor->gizmo.translation_axis_meshes, 0,
      sizeof(editor->gizmo.translation_axis_meshes));
  editor->gizmo.translation_highlight_mesh = ldk_renderer_mesh_null();
  editor->gizmo.initialized = false;
}

void ldki_editor_gizmo_terminate(LDKEditorContext *editor)
{
  if (editor == NULL)
  {
    return;
  }

  s_editor_gizmo_resources_destroy(editor);
  memset(&editor->gizmo, 0, sizeof(editor->gizmo));
}

static bool s_editor_gizmo_initialize(LDKEditorContext *editor)
{
  LDKMeshData cube = {0};
  LDKRendererMeshDesc desc = {0};

  if (s_editor_gizmo_meshes_are_valid(editor))
  {
    return true;
  }

  s_editor_gizmo_resources_destroy(editor);

  if (editor == NULL || editor->renderer == NULL ||
      !ldk_mesh_primitive_create(LDK_MESH_PRIMITIVE_CUBE, &cube))
  {
    return false;
  }

  desc.vertices = cube.vertices;
  desc.vertex_count = cube.vertex_count;
  desc.indices = cube.indices;
  desc.index_count = cube.index_count;

  for (u32 axis = 0; axis < LDK_EDITOR_GIZMO_AXIS_COUNT; ++axis)
  {
    for (u32 vertex = 0; vertex < cube.vertex_count; ++vertex)
    {
      cube.vertices[vertex].color = s_gizmo_axis_colors[axis];
    }

    editor->gizmo.translation_axis_meshes[axis] =
        ldk_renderer_mesh_create(editor->renderer, &desc);

    if (!ldk_renderer_mesh_is_valid(
            editor->renderer, editor->gizmo.translation_axis_meshes[axis]))
    {
      ldk_mesh_data_destroy(&cube);
      s_editor_gizmo_resources_destroy(editor);
      return false;
    }
  }

  for (u32 vertex = 0; vertex < cube.vertex_count; ++vertex)
  {
    cube.vertices[vertex].color = s_gizmo_highlight_color;
  }

  editor->gizmo.translation_highlight_mesh =
      ldk_renderer_mesh_create(editor->renderer, &desc);

  if (!ldk_renderer_mesh_is_valid(
          editor->renderer, editor->gizmo.translation_highlight_mesh))
  {
    ldk_mesh_data_destroy(&cube);
    s_editor_gizmo_resources_destroy(editor);
    return false;
  }

  ldk_mesh_data_destroy(&cube);
  editor->gizmo.initialized = true;
  return true;
}

static float s_editor_gizmo_world_length(
    LDKEditorContext *editor, Vec3 position)
{
  LDKCamera *camera;
  Mat4 camera_world;
  Vec3 camera_position;
  float world_length;

  camera = (LDKCamera *)ldk_ecs_component_get(
      editor->editor_camera, LDK_COMPONENT_TYPE_CAMERA);
  if (camera == NULL || editor->renderer->game_height == 0 ||
      !ldk_camera_get_world_matrix(editor->editor_camera, &camera_world))
  {
    return 1.0f;
  }

  if (camera->projection == LDK_CAMERA_PROJECTION_ORTHOGRAPHIC)
  {
    world_length = camera->orthographic_height *
                   LDK_EDITOR_GIZMO_PIXEL_LENGTH /
                   (float)editor->renderer->game_height;
  }
  else
  {
    camera_position = vec3_make(
        camera_world.m[12], camera_world.m[13], camera_world.m[14]);
    float distance = vec3_len(vec3_sub(position, camera_position));
    float visible_height = 2.0f * distance * tanf(camera->fov_y * 0.5f);
    world_length = visible_height * LDK_EDITOR_GIZMO_PIXEL_LENGTH /
                   (float)editor->renderer->game_height;
  }

  return float_max(world_length, LDK_EDITOR_GIZMO_MIN_WORLD_LENGTH);
}

static Mat4 s_editor_gizmo_part_world(
    Vec3 position, Mat4 orientation, Vec3 scale)
{
  return mat4_mul(
      mat4_mul(mat4_translate(position), orientation), mat4_scale(scale));
}

static bool s_editor_gizmo_rect_contains(
    LDKUIRect const *rect, float x, float y)
{
  return rect != NULL && x >= rect->x && y >= rect->y &&
         x < rect->x + rect->w && y < rect->y + rect->h;
}

static bool s_editor_gizmo_scene_ray_get(LDKEditorContext *editor,
    LDKUIPoint cursor, LDKEditorGizmoRay *out_ray)
{
  LDKUIRect const *rect;
  Mat4 view;
  Mat4 projection;
  Mat4 inverse_view_projection;
  Vec3 near_position;
  Vec3 far_position;
  Vec3 direction;
  float direction_length;
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
  if (rect->w <= 0.0f || rect->h <= 0.0f)
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

  // The Scene View presents the render target without a vertical UV flip.
  // Keep this mapping symmetrical with s_editor_gizmo_world_to_scene().
  ndc_x = ((cursor.x - rect->x) / rect->w) * 2.0f - 1.0f;
  ndc_y = ((cursor.y - rect->y) / rect->h) * 2.0f - 1.0f;
  near_position = mat4_mul_point(
      inverse_view_projection, vec3_make(ndc_x, ndc_y, -1.0f));
  far_position = mat4_mul_point(
      inverse_view_projection, vec3_make(ndc_x, ndc_y, 1.0f));
  direction = vec3_sub(far_position, near_position);
  direction_length = vec3_len(direction);

  if (direction_length <= LDK_EDITOR_GIZMO_INTERSECTION_EPSILON)
  {
    return false;
  }

  out_ray->origin = near_position;
  out_ray->direction = vec3_div(direction, direction_length);
  return true;
}

static Vec3 s_editor_gizmo_perpendicular_component(
    Vec3 direction, Vec3 axis)
{
  return vec3_sub(direction, vec3_mul(axis, vec3_dot(direction, axis)));
}

static bool s_editor_gizmo_drag_plane_normal_get(LDKEditorContext *editor,
    Vec3 origin, Vec3 axis, Vec3 *out_normal)
{
  LDKCamera *camera;
  Mat4 camera_world;
  Vec3 camera_position;
  Vec3 view_direction;
  Vec3 normal;
  Vec3 camera_right;
  Vec3 camera_up;
  Vec3 right_candidate;
  Vec3 up_candidate;
  float normal_length;

  if (editor == NULL || out_normal == NULL ||
      !ldk_camera_get_world_matrix(editor->editor_camera, &camera_world))
  {
    return false;
  }

  camera = (LDKCamera *)ldk_ecs_component_get(
      editor->editor_camera, LDK_COMPONENT_TYPE_CAMERA);
  if (camera == NULL)
  {
    return false;
  }

  if (camera->projection == LDK_CAMERA_PROJECTION_ORTHOGRAPHIC)
  {
    view_direction = vec3_make(
        -camera_world.m[8], -camera_world.m[9], -camera_world.m[10]);
  }
  else
  {
    camera_position = vec3_make(
        camera_world.m[12], camera_world.m[13], camera_world.m[14]);
    view_direction = vec3_sub(origin, camera_position);
  }

  // This projection creates a camera-facing plane that still contains the
  // selected translation axis.
  normal = s_editor_gizmo_perpendicular_component(view_direction, axis);
  normal_length = vec3_len(normal);
  if (normal_length > LDK_EDITOR_GIZMO_INTERSECTION_EPSILON)
  {
    *out_normal = vec3_div(normal, normal_length);
    return true;
  }

  camera_right = vec3_make(
      camera_world.m[0], camera_world.m[1], camera_world.m[2]);
  camera_up = vec3_make(
      camera_world.m[4], camera_world.m[5], camera_world.m[6]);
  right_candidate =
      s_editor_gizmo_perpendicular_component(camera_right, axis);
  up_candidate = s_editor_gizmo_perpendicular_component(camera_up, axis);
  normal = vec3_len2(right_candidate) > vec3_len2(up_candidate)
               ? right_candidate
               : up_candidate;
  normal_length = vec3_len(normal);

  if (normal_length <= LDK_EDITOR_GIZMO_INTERSECTION_EPSILON)
  {
    return false;
  }

  *out_normal = vec3_div(normal, normal_length);
  return true;
}

static bool s_editor_gizmo_ray_plane_intersect(
    LDKEditorGizmoRay ray, Vec3 plane_point, Vec3 plane_normal,
    Vec3 *out_position)
{
  float denominator;
  float distance;

  if (out_position == NULL)
  {
    return false;
  }

  denominator = vec3_dot(ray.direction, plane_normal);
  if (fabsf(denominator) <= LDK_EDITOR_GIZMO_INTERSECTION_EPSILON)
  {
    return false;
  }

  distance =
      vec3_dot(vec3_sub(plane_point, ray.origin), plane_normal) /
      denominator;
  if (distance < 0.0f)
  {
    return false;
  }

  *out_position = vec3_add(ray.origin, vec3_mul(ray.direction, distance));
  return true;
}

static void s_editor_gizmo_drag_end(LDKEditorContext *editor)
{
  if (editor == NULL)
  {
    return;
  }

  editor->gizmo.drag_entity = x_handle_null();
  editor->gizmo.active_axis = LDK_EDITOR_GIZMO_AXIS_NONE;
  editor->gizmo.dragging = false;
}

static bool s_editor_gizmo_drag_begin(LDKEditorContext *editor,
    LDKEntity selected, Vec3 origin, Mat4 orientation, Vec3 const axes[3],
    LDKMouseState const *mouse)
{
  LDKEditorGizmoAxis active_axis;
  LDKEditorGizmoRay ray;
  Vec3 axis;
  Vec3 plane_normal;
  Vec3 hit_position;
  LDKUIPoint cursor;
  u32 axis_index;

  if (editor == NULL || axes == NULL || mouse == NULL ||
      editor->gizmo.hovered_axis == LDK_EDITOR_GIZMO_AXIS_NONE)
  {
    return false;
  }

  active_axis = editor->gizmo.hovered_axis;
  axis_index = (u32)(active_axis - LDK_EDITOR_GIZMO_AXIS_X);
  if (axis_index >= LDK_EDITOR_GIZMO_AXIS_COUNT)
  {
    return false;
  }

  axis = vec3_norm(axes[axis_index]);
  cursor = ldk_pointf((float)mouse->cursor.x, (float)mouse->cursor.y);
  if (!s_editor_gizmo_scene_ray_get(editor, cursor, &ray) ||
      !s_editor_gizmo_drag_plane_normal_get(
          editor, origin, axis, &plane_normal) ||
      !s_editor_gizmo_ray_plane_intersect(
          ray, origin, plane_normal, &hit_position))
  {
    return false;
  }

  editor->gizmo.drag_orientation = orientation;
  editor->gizmo.drag_entity = selected;
  editor->gizmo.drag_axis = axis;
  editor->gizmo.drag_origin = origin;
  editor->gizmo.drag_plane_normal = plane_normal;
  editor->gizmo.drag_initial_parameter =
      vec3_dot(vec3_sub(hit_position, origin), axis);
  editor->gizmo.active_axis = active_axis;
  editor->gizmo.dragging = true;
  return true;
}

static bool s_editor_gizmo_world_position_set(
    LDKEntity entity, Vec3 world_position)
{
  LDKEntity parent;
  Vec3 local_position;

  parent = ldk_transform_get_parent(entity);
  if (x_handle_is_null(parent))
  {
    local_position = world_position;
  }
  else
  {
    Mat4 parent_world;
    Mat4 inverse_parent_world;
    bool inverse_ok;

    if (!ldk_transform_get_world_matrix(parent, &parent_world))
    {
      return false;
    }

    // A TRS hierarchy can produce shear when rotation and non-uniform scale
    // are combined, so the general inverse is required here.
    inverse_parent_world = mat4_inverse_full(parent_world, &inverse_ok);
    if (!inverse_ok)
    {
      return false;
    }

    local_position = mat4_mul_point(inverse_parent_world, world_position);
  }

  return ldk_transform_set_local_position(entity, local_position) &&
         ldk_scenegraph_update_entity(entity);
}

static bool s_editor_gizmo_world_to_scene(
    LDKEditorContext *editor, Mat4 view_projection,
    Vec3 world_position, LDKUIPoint *out_position)
{
  LDKUIRect const *rect;
  float clip_x;
  float clip_y;
  float clip_w;

  if (editor == NULL || out_position == NULL)
  {
    return false;
  }

  rect = &editor->gizmo.scene_view_rect;
  if (rect->w <= 0.0f || rect->h <= 0.0f)
  {
    return false;
  }

  clip_x = view_projection.m[0] * world_position.x +
           view_projection.m[4] * world_position.y +
           view_projection.m[8] * world_position.z +
           view_projection.m[12];
  clip_y = view_projection.m[1] * world_position.x +
           view_projection.m[5] * world_position.y +
           view_projection.m[9] * world_position.z +
           view_projection.m[13];
  clip_w = view_projection.m[3] * world_position.x +
           view_projection.m[7] * world_position.y +
           view_projection.m[11] * world_position.z +
           view_projection.m[15];

  if (clip_w <= 0.0f)
  {
    return false;
  }

  clip_x /= clip_w;
  clip_y /= clip_w;
  out_position->x = rect->x + (clip_x + 1.0f) * 0.5f * rect->w;
  out_position->y = rect->y + (clip_y + 1.0f) * 0.5f * rect->h;
  return true;
}

static float s_editor_gizmo_point_segment_distance(LDKUIPoint point,
    LDKUIPoint segment_start, LDKUIPoint segment_end)
{
  float segment_x = segment_end.x - segment_start.x;
  float segment_y = segment_end.y - segment_start.y;
  float point_x = point.x - segment_start.x;
  float point_y = point.y - segment_start.y;
  float segment_length_squared =
      segment_x * segment_x + segment_y * segment_y;
  float segment_t = 0.0f;

  if (segment_length_squared > 0.0f)
  {
    segment_t = (point_x * segment_x + point_y * segment_y) /
                segment_length_squared;
    segment_t = float_clamp(segment_t, 0.0f, 1.0f);
  }

  float closest_x = segment_start.x + segment_x * segment_t;
  float closest_y = segment_start.y + segment_y * segment_t;
  float distance_x = point.x - closest_x;
  float distance_y = point.y - closest_y;
  return sqrtf(distance_x * distance_x + distance_y * distance_y);
}

void ldki_editor_gizmo_begin_ui_frame(LDKEditorContext *editor)
{
  if (editor != NULL)
  {
    editor->gizmo.scene_view_visible = false;
    editor->gizmo.hovered_axis = editor->gizmo.dragging
                                     ? editor->gizmo.active_axis
                                     : LDK_EDITOR_GIZMO_AXIS_NONE;
  }
}

void ldki_editor_gizmo_scene_view_set(
    LDKEditorContext *editor, LDKUIRect scene_view_rect)
{
  if (editor == NULL)
  {
    return;
  }

  editor->gizmo.scene_view_rect = scene_view_rect;
  editor->gizmo.scene_view_visible =
      scene_view_rect.w > 0.0f && scene_view_rect.h > 0.0f;
}

void ldki_editor_gizmo_hover_update(LDKEditorContext *editor)
{
  LDKECS *ecs;
  LDKMouseState mouse;
  LDKEntity selected;
  Mat4 selected_world;
  Mat4 view;
  Mat4 projection;
  Mat4 view_projection;
  Mat4 orientation;
  Vec3 axes[LDK_EDITOR_GIZMO_AXIS_COUNT];
  Vec3 origin;
  LDKUIPoint cursor;
  LDKUIPoint origin_screen;
  float length;
  float best_distance;
  float aspect;

  if (editor == NULL)
  {
    return;
  }

  if (editor->gizmo.dragging)
  {
    editor->gizmo.hovered_axis = editor->gizmo.active_axis;
    return;
  }

  editor->gizmo.hovered_axis = LDK_EDITOR_GIZMO_AXIS_NONE;
  if (!editor->gizmo.scene_view_visible || editor->renderer == NULL ||
      editor->renderer->game_width == 0 ||
      editor->renderer->game_height == 0)
  {
    return;
  }

  ldk_os_mouse_state_get(&mouse);
  if (!s_editor_gizmo_rect_contains(&editor->gizmo.scene_view_rect,
          (float)mouse.cursor.x, (float)mouse.cursor.y))
  {
    return;
  }

  ecs = ldk_module_get(LDK_MODULE_ECS);
  if (ecs == NULL ||
      !ldki_editor_selected_entity_get(editor, ecs, &selected) ||
      !ldk_transform_get_world_matrix(selected, &selected_world))
  {
    return;
  }

  origin = vec3_make(
      selected_world.m[12], selected_world.m[13], selected_world.m[14]);
  if (!s_editor_gizmo_orientation_get(
          editor, selected, &orientation, axes))
  {
    return;
  }

  length = s_editor_gizmo_world_length(editor, origin);
  aspect = (float)editor->renderer->game_width /
           (float)editor->renderer->game_height;

  if (!ldk_camera_get_view_matrix(editor->editor_camera, &view) ||
      !ldk_camera_get_projection_matrix(
          editor->editor_camera, aspect, &projection))
  {
    return;
  }

  view_projection = mat4_mul(projection, view);
  if (!s_editor_gizmo_world_to_scene(
          editor, view_projection, origin, &origin_screen))
  {
    return;
  }

  cursor = ldk_pointf((float)mouse.cursor.x, (float)mouse.cursor.y);
  best_distance = LDK_EDITOR_GIZMO_PICK_RADIUS;

  for (u32 axis = 0; axis < LDK_EDITOR_GIZMO_AXIS_COUNT; ++axis)
  {
    Vec3 end = vec3_add(origin, vec3_mul(axes[axis], length));
    LDKUIPoint end_screen;

    if (!s_editor_gizmo_world_to_scene(
            editor, view_projection, end, &end_screen))
    {
      continue;
    }

    float distance = s_editor_gizmo_point_segment_distance(
        cursor, origin_screen, end_screen);

    if (distance < best_distance)
    {
      best_distance = distance;
      editor->gizmo.hovered_axis =
          (LDKEditorGizmoAxis)(axis + LDK_EDITOR_GIZMO_AXIS_X);
    }
  }

  if (editor->gizmo.hovered_axis != LDK_EDITOR_GIZMO_AXIS_NONE &&
      ldk_os_mouse_button_down(&mouse, LDK_MOUSE_BUTTON_LEFT))
  {
    s_editor_gizmo_drag_begin(
        editor, selected, origin, orientation, axes, &mouse);
  }
}

void ldki_editor_gizmo_update(LDKEditorContext *editor)
{
  LDKECS *ecs;
  LDKMouseState mouse;
  LDKEditorGizmoRay ray;
  Vec3 hit_position;
  Vec3 world_position;
  float current_parameter;
  float translation;
  bool released;

  if (editor == NULL || !editor->gizmo.dragging)
  {
    return;
  }

  ldk_os_mouse_state_get(&mouse);
  released = ldk_os_mouse_button_up(&mouse, LDK_MOUSE_BUTTON_LEFT);
  if (!released &&
      !ldk_os_mouse_button_is_pressed(&mouse, LDK_MOUSE_BUTTON_LEFT))
  {
    s_editor_gizmo_drag_end(editor);
    return;
  }

  ecs = ldk_module_get(LDK_MODULE_ECS);
  if (ecs == NULL || !editor->gizmo.scene_view_visible ||
      !ldk_entity_is_alive(&ecs->entity, editor->gizmo.drag_entity) ||
      !ldki_editor_entity_equal(
          editor->selected_entity, editor->gizmo.drag_entity))
  {
    s_editor_gizmo_drag_end(editor);
    return;
  }

  if (s_editor_gizmo_scene_ray_get(editor,
          ldk_pointf((float)mouse.cursor.x, (float)mouse.cursor.y), &ray) &&
      s_editor_gizmo_ray_plane_intersect(ray, editor->gizmo.drag_origin,
          editor->gizmo.drag_plane_normal, &hit_position))
  {
    current_parameter = vec3_dot(
        vec3_sub(hit_position, editor->gizmo.drag_origin),
        editor->gizmo.drag_axis);
    translation =
        current_parameter - editor->gizmo.drag_initial_parameter;
    world_position = vec3_add(editor->gizmo.drag_origin,
        vec3_mul(editor->gizmo.drag_axis, translation));

    if (!s_editor_gizmo_world_position_set(
            editor->gizmo.drag_entity, world_position))
    {
      s_editor_gizmo_drag_end(editor);
      return;
    }
  }

  if (released)
  {
    s_editor_gizmo_drag_end(editor);
  }
}

void ldki_editor_gizmo_submit(LDKEditorContext *editor)
{
  LDKECS *ecs;
  LDKEntity selected;
  Mat4 selected_world;
  Mat4 orientation;
  Vec3 axes[LDK_EDITOR_GIZMO_AXIS_COUNT];
  Vec3 origin;
  float length;
  float bar_length;
  float bar_thickness;
  float handle_size;

  if (editor == NULL || editor->renderer == NULL ||
      editor->scene_view == LDK_RENDERER_VIEW_INVALID ||
      editor->scene_view == LDK_RENDERER_VIEW_ALL)
  {
    return;
  }

  ecs = ldk_module_get(LDK_MODULE_ECS);
  if (ecs == NULL ||
      !ldki_editor_selected_entity_get(editor, ecs, &selected) ||
      !ldk_transform_get_world_matrix(selected, &selected_world) ||
      !s_editor_gizmo_initialize(editor))
  {
    return;
  }

  if (editor->gizmo.dragging &&
      ldki_editor_entity_equal(selected, editor->gizmo.drag_entity))
  {
    orientation = editor->gizmo.drag_orientation;
    s_editor_gizmo_axes_from_orientation(orientation, axes);
  }
  else if (!s_editor_gizmo_orientation_get(
               editor, selected, &orientation, axes))
  {
    return;
  }

  origin = vec3_make(
      selected_world.m[12], selected_world.m[13], selected_world.m[14]);
  length = s_editor_gizmo_world_length(editor, origin);
  bar_length = length * 0.78f;
  bar_thickness = length * 0.055f;
  handle_size = length * 0.22f;

  for (u32 axis = 0; axis < LDK_EDITOR_GIZMO_AXIS_COUNT; ++axis)
  {
    LDKResourceMesh axis_mesh =
        editor->gizmo.hovered_axis ==
                (LDKEditorGizmoAxis)(axis + LDK_EDITOR_GIZMO_AXIS_X)
            ? editor->gizmo.translation_highlight_mesh
            : editor->gizmo.translation_axis_meshes[axis];
    Vec3 direction = axes[axis];
    Vec3 bar_position =
        vec3_add(origin, vec3_mul(direction, bar_length * 0.5f));
    Vec3 bar_scale = vec3_make(
        bar_thickness, bar_thickness, bar_thickness);
    Vec3 handle_position = vec3_add(
        origin, vec3_mul(direction, bar_length + handle_size * 0.5f));
    Vec3 handle_scale =
        vec3_make(handle_size, handle_size, handle_size);

    if (axis == 0)
    {
      bar_scale.x = bar_length;
    }
    else if (axis == 1)
    {
      bar_scale.y = bar_length;
    }
    else
    {
      bar_scale.z = bar_length;
    }

    ldk_renderer_submit_mesh_to_view(editor->renderer, editor->scene_view,
        axis_mesh,
        s_editor_gizmo_part_world(bar_position, orientation, bar_scale));
    ldk_renderer_submit_mesh_to_view(editor->renderer, editor->scene_view,
        axis_mesh,
        s_editor_gizmo_part_world(
            handle_position, orientation, handle_scale));
  }
}
