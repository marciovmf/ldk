#include "ldk_editor_internal.h"

#include <ldk.h>
#include <ldk_mesh.h>
#include <component/ldk_camera.h>
#include <component/ldk_transform.h>
#include <module/ldk_ecs.h>
#include <module/ldk_scenegraph.h>

#include <math.h>
#include <stdlib.h>
#include <string.h>

#define LDK_EDITOR_GIZMO_AXIS_COUNT 3u
#define LDK_EDITOR_GIZMO_PIXEL_LENGTH 96.0f
#define LDK_EDITOR_GIZMO_MIN_WORLD_LENGTH 0.05f
#define LDK_EDITOR_GIZMO_PICK_RADIUS 12.0f
#define LDK_EDITOR_GIZMO_CENTER_PICK_RADIUS 12.0f
#define LDK_EDITOR_GIZMO_MAX_HIERARCHY_DEPTH 256u
#define LDK_EDITOR_GIZMO_INTERSECTION_EPSILON 0.00001f
#define LDK_EDITOR_GIZMO_ROTATION_RING_SEGMENTS 32u
#define LDK_EDITOR_GIZMO_ROTATION_RING_INNER_RADIUS 0.947384f
#define LDK_EDITOR_GIZMO_ROTATION_RING_HALF_DEPTH 0.023540f
#define LDK_EDITOR_GIZMO_ROTATION_RADIUS_SCALE 0.88f
#define LDK_EDITOR_GIZMO_ROTATION_ORIGIN_LINE_SCALE 0.018f
#define LDK_EDITOR_GIZMO_PI 3.14159265358979323846f

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
static const u32 s_gizmo_center_color = 0xE0E0E0FFu;

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

  // Component-wise scale is stored in local TRS space. A world-aligned scale
  // of a rotated object would require shear, which LDKTransform cannot store.
  if (editor->gizmo.mode == LDK_EDITOR_GIZMO_MODE_SCALE ||
      editor->gizmo.space == LDK_EDITOR_GIZMO_SPACE_LOCAL)
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
            editor->renderer, editor->gizmo.axis_cube_meshes[i]) ||
        !ldk_renderer_mesh_is_valid(
            editor->renderer, editor->gizmo.axis_cone_meshes[i]) ||
        !ldk_renderer_mesh_is_valid(
            editor->renderer, editor->gizmo.rotation_arc_meshes[i]) ||
        !ldk_renderer_mesh_is_valid(editor->renderer,
            editor->gizmo.rotation_arc_highlight_meshes[i]))
    {
      return false;
    }
  }

  return ldk_renderer_mesh_is_valid(
             editor->renderer, editor->gizmo.cube_highlight_mesh) &&
         ldk_renderer_mesh_is_valid(
             editor->renderer, editor->gizmo.cone_highlight_mesh) &&
         ldk_renderer_mesh_is_valid(
             editor->renderer, editor->gizmo.center_cube_mesh);
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
        editor->renderer, editor->gizmo.axis_cube_meshes[i]);
    ldk_renderer_mesh_destroy(
        editor->renderer, editor->gizmo.axis_cone_meshes[i]);
    ldk_renderer_mesh_destroy(
        editor->renderer, editor->gizmo.rotation_arc_meshes[i]);
    ldk_renderer_mesh_destroy(editor->renderer,
        editor->gizmo.rotation_arc_highlight_meshes[i]);
  }

  ldk_renderer_mesh_destroy(
      editor->renderer, editor->gizmo.cube_highlight_mesh);
  ldk_renderer_mesh_destroy(
      editor->renderer, editor->gizmo.cone_highlight_mesh);
  ldk_renderer_mesh_destroy(
      editor->renderer, editor->gizmo.center_cube_mesh);

  memset(editor->gizmo.axis_cube_meshes, 0,
      sizeof(editor->gizmo.axis_cube_meshes));
  memset(editor->gizmo.axis_cone_meshes, 0,
      sizeof(editor->gizmo.axis_cone_meshes));
  memset(editor->gizmo.rotation_arc_meshes, 0,
      sizeof(editor->gizmo.rotation_arc_meshes));
  memset(editor->gizmo.rotation_arc_highlight_meshes, 0,
      sizeof(editor->gizmo.rotation_arc_highlight_meshes));
  editor->gizmo.cube_highlight_mesh = ldk_renderer_mesh_null();
  editor->gizmo.cone_highlight_mesh = ldk_renderer_mesh_null();
  editor->gizmo.center_cube_mesh = ldk_renderer_mesh_null();
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

static bool s_editor_gizmo_mesh_create(LDKEditorContext *editor,
    LDKMeshPrimitive primitive, u32 color, LDKResourceMesh *out_mesh)
{
  LDKMeshData mesh = {0};
  LDKRendererMeshDesc desc = {0};

  if (editor == NULL || editor->renderer == NULL || out_mesh == NULL ||
      !ldk_mesh_primitive_create(primitive, &mesh))
  {
    return false;
  }

  for (u32 vertex = 0; vertex < mesh.vertex_count; ++vertex)
  {
    mesh.vertices[vertex].color = color;
  }

  desc.vertices = mesh.vertices;
  desc.vertex_count = mesh.vertex_count;
  desc.indices = mesh.indices;
  desc.index_count = mesh.index_count;
  *out_mesh = ldk_renderer_mesh_create(editor->renderer, &desc);
  ldk_mesh_data_destroy(&mesh);

  return ldk_renderer_mesh_is_valid(editor->renderer, *out_mesh);
}

static Vec3 s_editor_gizmo_rotation_ring_direction(float angle)
{
  return vec3_make(cosf(angle), sinf(angle), 0.0f);
}

static Mat4 s_editor_gizmo_rotation_ring_orientation(
    Mat4 orientation, u32 axis)
{
  Mat4 axis_orientation = mat4_identity();

  if (axis == 0u)
  {
    axis_orientation = mat4_rot_y(LDK_EDITOR_GIZMO_PI * 0.5f);
  }
  else if (axis == 1u)
  {
    axis_orientation = mat4_rot_x(-LDK_EDITOR_GIZMO_PI * 0.5f);
  }

  return mat4_mul(orientation, axis_orientation);
}

static bool s_editor_gizmo_rotation_ring_mesh_create(
    LDKEditorContext *editor, u32 color,
    LDKResourceMesh *out_mesh)
{
  LDKMeshData mesh = {0};
  LDKRendererMeshDesc desc = {0};
  Vec3 ring_normal = vec3_make(0.0f, 0.0f, 1.0f);
  float radial_depth =
      1.0f - LDK_EDITOR_GIZMO_ROTATION_RING_INNER_RADIUS;
  u32 vertex_count;
  u32 index_count;
  u32 index = 0u;

  if (editor == NULL || editor->renderer == NULL || out_mesh == NULL)
  {
    return false;
  }

  vertex_count =
      (LDK_EDITOR_GIZMO_ROTATION_RING_SEGMENTS + 1u) * 4u;
  index_count = LDK_EDITOR_GIZMO_ROTATION_RING_SEGMENTS * 12u;
  mesh.vertices = (LDKMeshVertex *)calloc(
      vertex_count, sizeof(LDKMeshVertex));
  mesh.indices = (u32 *)calloc(index_count, sizeof(u32));
  if (mesh.vertices == NULL || mesh.indices == NULL)
  {
    ldk_mesh_data_destroy(&mesh);
    return false;
  }

  mesh.vertex_count = vertex_count;
  mesh.index_count = index_count;

  for (u32 segment = 0u;
       segment <= LDK_EDITOR_GIZMO_ROTATION_RING_SEGMENTS; ++segment)
  {
    float t = (float)segment /
              (float)LDK_EDITOR_GIZMO_ROTATION_RING_SEGMENTS;
    float angle = t * LDK_EDITOR_GIZMO_PI * 2.0f;
    Vec3 direction =
        s_editor_gizmo_rotation_ring_direction(angle);
    Vec3 front_normal = vec3_norm(vec3_add(
        vec3_mul(direction,
            LDK_EDITOR_GIZMO_ROTATION_RING_HALF_DEPTH),
        vec3_mul(ring_normal, radial_depth)));
    Vec3 back_normal = vec3_norm(vec3_sub(
        vec3_mul(direction,
            LDK_EDITOR_GIZMO_ROTATION_RING_HALF_DEPTH),
        vec3_mul(ring_normal, radial_depth)));
    Vec3 outer_position = direction;
    Vec3 inner_position = vec3_mul(direction,
        LDK_EDITOR_GIZMO_ROTATION_RING_INNER_RADIUS);
    u32 vertex = segment * 4u;
    LDKMeshVertex *front_outer = &mesh.vertices[vertex];
    LDKMeshVertex *front_inner = &mesh.vertices[vertex + 1u];
    LDKMeshVertex *back_outer = &mesh.vertices[vertex + 2u];
    LDKMeshVertex *back_inner = &mesh.vertices[vertex + 3u];

    front_outer->position = outer_position;
    front_outer->normal = front_normal;
    front_outer->uv = vec2_make(t, 1.0f);
    front_outer->color = color;

    front_inner->position = vec3_add(inner_position,
        vec3_mul(ring_normal,
            LDK_EDITOR_GIZMO_ROTATION_RING_HALF_DEPTH));
    front_inner->normal = front_normal;
    front_inner->uv = vec2_make(t, 0.0f);
    front_inner->color = color;

    back_outer->position = outer_position;
    back_outer->normal = back_normal;
    back_outer->uv = vec2_make(t, 1.0f);
    back_outer->color = color;

    back_inner->position = vec3_sub(inner_position,
        vec3_mul(ring_normal,
            LDK_EDITOR_GIZMO_ROTATION_RING_HALF_DEPTH));
    back_inner->normal = back_normal;
    back_inner->uv = vec2_make(t, 0.0f);
    back_inner->color = color;
  }

  for (u32 segment = 0u;
       segment < LDK_EDITOR_GIZMO_ROTATION_RING_SEGMENTS; ++segment)
  {
    u32 vertex = segment * 4u;
    u32 next_vertex = vertex + 4u;

    mesh.indices[index++] = vertex;
    mesh.indices[index++] = next_vertex;
    mesh.indices[index++] = vertex + 1u;
    mesh.indices[index++] = vertex + 1u;
    mesh.indices[index++] = next_vertex;
    mesh.indices[index++] = next_vertex + 1u;

    mesh.indices[index++] = vertex + 2u;
    mesh.indices[index++] = vertex + 3u;
    mesh.indices[index++] = next_vertex + 2u;
    mesh.indices[index++] = vertex + 3u;
    mesh.indices[index++] = next_vertex + 3u;
    mesh.indices[index++] = next_vertex + 2u;
  }

  desc.vertices = mesh.vertices;
  desc.vertex_count = mesh.vertex_count;
  desc.indices = mesh.indices;
  desc.index_count = mesh.index_count;
  *out_mesh = ldk_renderer_mesh_create(editor->renderer, &desc);
  ldk_mesh_data_destroy(&mesh);

  return ldk_renderer_mesh_is_valid(editor->renderer, *out_mesh);
}

static bool s_editor_gizmo_initialize(LDKEditorContext *editor)
{
  if (s_editor_gizmo_meshes_are_valid(editor))
  {
    return true;
  }

  s_editor_gizmo_resources_destroy(editor);
  if (editor == NULL || editor->renderer == NULL)
  {
    return false;
  }

  for (u32 axis = 0; axis < LDK_EDITOR_GIZMO_AXIS_COUNT; ++axis)
  {
    if (!s_editor_gizmo_mesh_create(editor, LDK_MESH_PRIMITIVE_CUBE,
            s_gizmo_axis_colors[axis],
            &editor->gizmo.axis_cube_meshes[axis]) ||
        !s_editor_gizmo_mesh_create(editor, LDK_MESH_PRIMITIVE_CONE,
            s_gizmo_axis_colors[axis],
            &editor->gizmo.axis_cone_meshes[axis]) ||
        !s_editor_gizmo_rotation_ring_mesh_create(editor,
            s_gizmo_axis_colors[axis],
            &editor->gizmo.rotation_arc_meshes[axis]) ||
        !s_editor_gizmo_rotation_ring_mesh_create(editor,
            s_gizmo_highlight_color,
            &editor->gizmo.rotation_arc_highlight_meshes[axis]))
    {
      s_editor_gizmo_resources_destroy(editor);
      return false;
    }
  }

  if (!s_editor_gizmo_mesh_create(editor, LDK_MESH_PRIMITIVE_CUBE,
          s_gizmo_highlight_color,
          &editor->gizmo.cube_highlight_mesh) ||
      !s_editor_gizmo_mesh_create(editor, LDK_MESH_PRIMITIVE_CONE,
          s_gizmo_highlight_color,
          &editor->gizmo.cone_highlight_mesh) ||
      !s_editor_gizmo_mesh_create(editor, LDK_MESH_PRIMITIVE_CUBE,
          s_gizmo_center_color, &editor->gizmo.center_cube_mesh))
  {
    s_editor_gizmo_resources_destroy(editor);
    return false;
  }

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

static Mat4 s_editor_gizmo_handle_orientation(
    Mat4 orientation, u32 axis)
{
  Mat4 axis_orientation = mat4_identity();

  if (axis == 0u)
  {
    axis_orientation = mat4_rot_z(deg_to_rad(-90.0f));
  }
  else if (axis == 2u)
  {
    axis_orientation = mat4_rot_x(deg_to_rad(90.0f));
  }

  return mat4_mul(orientation, axis_orientation);
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

  // UI coordinates grow downward while NDC grows upward. Keep this mapping
  // symmetrical with s_editor_gizmo_world_to_scene().
  ndc_x = ((cursor.x - rect->x) / rect->w) * 2.0f - 1.0f;
  ndc_y = 1.0f - ((cursor.y - rect->y) / rect->h) * 2.0f;
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
  // selected interaction axis.
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

static bool s_editor_gizmo_camera_plane_normal_get(
    LDKEditorContext *editor, Vec3 origin, Vec3 *out_normal)
{
  LDKCamera *camera;
  Mat4 camera_world;
  Vec3 camera_position;
  Vec3 normal;
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
    normal = vec3_make(
        -camera_world.m[8], -camera_world.m[9], -camera_world.m[10]);
  }
  else
  {
    camera_position = vec3_make(
        camera_world.m[12], camera_world.m[13], camera_world.m[14]);
    normal = vec3_sub(origin, camera_position);
  }

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

static bool s_editor_gizmo_rotation_drag_state_get(
    LDKEntity selected, Quat *out_world_rotation,
    Quat *out_parent_world_rotation)
{
  LDKEntity parent;

  if (out_world_rotation == NULL || out_parent_world_rotation == NULL ||
      !s_editor_gizmo_world_rotation_get(
          selected, 0u, out_world_rotation))
  {
    return false;
  }

  parent = ldk_transform_get_parent(selected);
  if (x_handle_is_null(parent))
  {
    *out_parent_world_rotation = quat_id();
    return true;
  }

  return s_editor_gizmo_world_rotation_get(
      parent, 0u, out_parent_world_rotation);
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
  cursor = ldk_pointf((float)mouse->cursor.x, (float)mouse->cursor.y);

  if (active_axis == LDK_EDITOR_GIZMO_AXIS_ALL)
  {
    if (editor->gizmo.mode == LDK_EDITOR_GIZMO_MODE_SCALE)
    {
      if (!ldk_transform_get_local_scale(
              selected, &editor->gizmo.drag_initial_scale))
      {
        return false;
      }
    }
    else if (editor->gizmo.mode == LDK_EDITOR_GIZMO_MODE_TRANSLATE)
    {
      if (!s_editor_gizmo_scene_ray_get(editor, cursor, &ray) ||
          !s_editor_gizmo_camera_plane_normal_get(
              editor, origin, &plane_normal) ||
          !s_editor_gizmo_ray_plane_intersect(
              ray, origin, plane_normal, &hit_position))
      {
        return false;
      }

      editor->gizmo.drag_initial_hit = hit_position;
      editor->gizmo.drag_plane_normal = plane_normal;
    }
    else
    {
      return false;
    }

    editor->gizmo.drag_orientation = orientation;
    editor->gizmo.drag_entity = selected;
    editor->gizmo.drag_origin = origin;
    editor->gizmo.drag_initial_cursor = cursor;
    editor->gizmo.drag_world_length =
        s_editor_gizmo_world_length(editor, origin);
    editor->gizmo.active_axis = active_axis;
    editor->gizmo.drag_mode = editor->gizmo.mode;
    editor->gizmo.dragging = true;
    return true;
  }

  axis_index = (u32)(active_axis - LDK_EDITOR_GIZMO_AXIS_X);
  if (axis_index >= LDK_EDITOR_GIZMO_AXIS_COUNT)
  {
    return false;
  }

  axis = vec3_norm(axes[axis_index]);
  if (editor->gizmo.mode == LDK_EDITOR_GIZMO_MODE_ROTATE)
  {
    Vec3 initial_direction;
    float initial_direction_length;

    if (!s_editor_gizmo_scene_ray_get(editor, cursor, &ray) ||
        !s_editor_gizmo_ray_plane_intersect(
            ray, origin, axis, &hit_position) ||
        !s_editor_gizmo_rotation_drag_state_get(selected,
            &editor->gizmo.drag_initial_world_rotation,
            &editor->gizmo.drag_parent_world_rotation))
    {
      return false;
    }

    initial_direction = vec3_sub(hit_position, origin);
    initial_direction_length = vec3_len(initial_direction);
    if (initial_direction_length <=
        LDK_EDITOR_GIZMO_INTERSECTION_EPSILON)
    {
      return false;
    }

    editor->gizmo.drag_orientation = orientation;
    editor->gizmo.drag_entity = selected;
    editor->gizmo.drag_axis = axis;
    editor->gizmo.drag_origin = origin;
    editor->gizmo.drag_initial_direction =
        vec3_div(initial_direction, initial_direction_length);
    editor->gizmo.drag_plane_normal = axis;
    editor->gizmo.drag_initial_cursor = cursor;
    editor->gizmo.drag_previous_angle = 0.0f;
    editor->gizmo.drag_accumulated_angle = 0.0f;
    editor->gizmo.drag_world_length =
        s_editor_gizmo_world_length(editor, origin);
    editor->gizmo.active_axis = active_axis;
    editor->gizmo.drag_mode = editor->gizmo.mode;
    editor->gizmo.dragging = true;
    return true;
  }

  if (!s_editor_gizmo_scene_ray_get(editor, cursor, &ray) ||
      !s_editor_gizmo_drag_plane_normal_get(
          editor, origin, axis, &plane_normal) ||
      !s_editor_gizmo_ray_plane_intersect(
          ray, origin, plane_normal, &hit_position))
  {
    return false;
  }

  if (editor->gizmo.mode == LDK_EDITOR_GIZMO_MODE_SCALE &&
      !ldk_transform_get_local_scale(
          selected, &editor->gizmo.drag_initial_scale))
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
  editor->gizmo.drag_initial_cursor = cursor;
  editor->gizmo.drag_world_length =
      s_editor_gizmo_world_length(editor, origin);
  editor->gizmo.active_axis = active_axis;
  editor->gizmo.drag_mode = editor->gizmo.mode;
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

static bool s_editor_gizmo_local_scale_set(
    LDKEntity entity, Vec3 local_scale)
{
  return ldk_transform_set_local_scale(entity, local_scale) &&
         ldk_scenegraph_update_entity(entity);
}

static bool s_editor_gizmo_local_rotation_set(
    LDKEntity entity, Quat local_rotation)
{
  return ldk_transform_set_local_rotation(
             entity, quat_norm(local_rotation)) &&
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
  out_position->y = rect->y + (1.0f - clip_y) * 0.5f * rect->h;
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

static float s_editor_gizmo_point_distance(
    LDKUIPoint a, LDKUIPoint b)
{
  float x = a.x - b.x;
  float y = a.y - b.y;
  return sqrtf(x * x + y * y);
}

static bool s_editor_gizmo_rotation_ring_segment_visible(
    Mat4 ring_orientation, Mat4 camera_world, Vec3 origin,
    float radius, float angle, bool orthographic)
{
  Vec3 ring_normal = vec3_make(0.0f, 0.0f, 1.0f);
  Vec3 direction = s_editor_gizmo_rotation_ring_direction(angle);
  Vec3 local_front_normal;
  Vec3 local_back_normal;
  Vec3 world_front_normal;
  Vec3 world_back_normal;
  Vec3 world_position;
  Vec3 to_camera;
  float radial_depth =
      1.0f - LDK_EDITOR_GIZMO_ROTATION_RING_INNER_RADIUS;
  float center_radius =
      (1.0f + LDK_EDITOR_GIZMO_ROTATION_RING_INNER_RADIUS) * 0.5f;

  local_front_normal = vec3_norm(vec3_add(
      vec3_mul(direction, LDK_EDITOR_GIZMO_ROTATION_RING_HALF_DEPTH),
      vec3_mul(ring_normal, radial_depth)));
  local_back_normal = vec3_norm(vec3_sub(
      vec3_mul(direction, LDK_EDITOR_GIZMO_ROTATION_RING_HALF_DEPTH),
      vec3_mul(ring_normal, radial_depth)));
  world_front_normal =
      vec3_norm(mat4_mul_dir(ring_orientation, local_front_normal));
  world_back_normal =
      vec3_norm(mat4_mul_dir(ring_orientation, local_back_normal));
  world_position = vec3_add(origin,
      vec3_mul(mat4_mul_dir(ring_orientation, direction),
          radius * center_radius));

  if (orthographic)
  {
    to_camera = vec3_make(
        camera_world.m[8], camera_world.m[9], camera_world.m[10]);
  }
  else
  {
    Vec3 camera_position = vec3_make(camera_world.m[12],
        camera_world.m[13], camera_world.m[14]);
    to_camera = vec3_norm(vec3_sub(camera_position, world_position));
  }

  return vec3_dot(world_front_normal, to_camera) > 0.0f ||
         vec3_dot(world_back_normal, to_camera) > 0.0f;
}

static bool s_editor_gizmo_rotation_ring_distance_get(
    LDKEditorContext *editor, Mat4 view_projection, Mat4 orientation,
    Mat4 camera_world, bool orthographic, Vec3 origin, u32 axis,
    float radius, LDKUIPoint cursor, float *out_distance)
{
  bool distance_valid = false;
  float best_distance = LDK_EDITOR_GIZMO_PICK_RADIUS;
  float center_radius =
      radius *
      (1.0f + LDK_EDITOR_GIZMO_ROTATION_RING_INNER_RADIUS) * 0.5f;
  Mat4 ring_orientation;

  if (editor == NULL || out_distance == NULL ||
      axis >= LDK_EDITOR_GIZMO_AXIS_COUNT)
  {
    return false;
  }

  ring_orientation =
      s_editor_gizmo_rotation_ring_orientation(orientation, axis);

  for (u32 segment = 0u;
       segment < LDK_EDITOR_GIZMO_ROTATION_RING_SEGMENTS; ++segment)
  {
    float start_t = (float)segment /
                    (float)LDK_EDITOR_GIZMO_ROTATION_RING_SEGMENTS;
    float end_t = (float)(segment + 1u) /
                  (float)LDK_EDITOR_GIZMO_ROTATION_RING_SEGMENTS;
    float start_angle = start_t * LDK_EDITOR_GIZMO_PI * 2.0f;
    float end_angle = end_t * LDK_EDITOR_GIZMO_PI * 2.0f;
    float middle_angle = (start_angle + end_angle) * 0.5f;
    Vec3 start_direction =
        s_editor_gizmo_rotation_ring_direction(start_angle);
    Vec3 end_direction =
        s_editor_gizmo_rotation_ring_direction(end_angle);
    Vec3 start_position = vec3_add(origin,
        vec3_mul(mat4_mul_dir(ring_orientation, start_direction),
            center_radius));
    Vec3 end_position = vec3_add(origin,
        vec3_mul(mat4_mul_dir(ring_orientation, end_direction),
            center_radius));
    LDKUIPoint start_screen;
    LDKUIPoint end_screen;

    if (s_editor_gizmo_rotation_ring_segment_visible(ring_orientation,
            camera_world, origin, radius, middle_angle, orthographic) &&
        s_editor_gizmo_world_to_scene(
            editor, view_projection, start_position, &start_screen) &&
        s_editor_gizmo_world_to_scene(
            editor, view_projection, end_position, &end_screen))
    {
      float distance = s_editor_gizmo_point_segment_distance(
          cursor, start_screen, end_screen);
      if (distance < best_distance)
      {
        best_distance = distance;
      }
      distance_valid = true;
    }
  }

  if (!distance_valid)
  {
    return false;
  }

  *out_distance = best_distance;
  return true;
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
  LDKCamera *camera;
  LDKMouseState mouse;
  LDKEntity selected;
  Mat4 selected_world;
  Mat4 camera_world;
  Mat4 view;
  Mat4 projection;
  Mat4 view_projection;
  Mat4 orientation;
  Vec3 axes[LDK_EDITOR_GIZMO_AXIS_COUNT];
  Vec3 origin;
  LDKUIPoint cursor;
  LDKUIPoint origin_screen;
  LDKEditorGizmoMode mode;
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

  camera = (LDKCamera *)ldk_ecs_component_get(
      editor->editor_camera, LDK_COMPONENT_TYPE_CAMERA);
  if (camera == NULL ||
      !ldk_camera_get_world_matrix(
          editor->editor_camera, &camera_world) ||
      !ldk_camera_get_view_matrix(editor->editor_camera, &view) ||
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
  mode = editor->gizmo.mode;
  best_distance = LDK_EDITOR_GIZMO_PICK_RADIUS;

  if (mode == LDK_EDITOR_GIZMO_MODE_ROTATE)
  {
    float radius = length * LDK_EDITOR_GIZMO_ROTATION_RADIUS_SCALE;

    for (u32 axis = 0u; axis < LDK_EDITOR_GIZMO_AXIS_COUNT; ++axis)
    {
      float distance;

      if (s_editor_gizmo_rotation_ring_distance_get(editor,
              view_projection, orientation, camera_world,
              camera->projection == LDK_CAMERA_PROJECTION_ORTHOGRAPHIC,
              origin, axis, radius, cursor, &distance) &&
          distance < best_distance)
      {
        best_distance = distance;
        editor->gizmo.hovered_axis =
            (LDKEditorGizmoAxis)(axis + LDK_EDITOR_GIZMO_AXIS_X);
      }
    }
  }
  else
  {
    if (s_editor_gizmo_point_distance(cursor, origin_screen) <=
        LDK_EDITOR_GIZMO_CENTER_PICK_RADIUS)
    {
      editor->gizmo.hovered_axis = LDK_EDITOR_GIZMO_AXIS_ALL;
    }

    if (editor->gizmo.hovered_axis == LDK_EDITOR_GIZMO_AXIS_NONE)
    {
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
    }
  }

  if (editor->gizmo.hovered_axis != LDK_EDITOR_GIZMO_AXIS_NONE &&
      ldk_os_mouse_button_down(&mouse, LDK_MOUSE_BUTTON_LEFT))
  {
    s_editor_gizmo_drag_begin(
        editor, selected, origin, orientation, axes, &mouse);
  }
}

static bool s_editor_gizmo_drag_parameter_get(
    LDKEditorContext *editor, LDKMouseState const *mouse,
    float *out_parameter)
{
  LDKEditorGizmoRay ray;
  Vec3 hit_position;

  if (editor == NULL || mouse == NULL || out_parameter == NULL ||
      !s_editor_gizmo_scene_ray_get(editor,
          ldk_pointf((float)mouse->cursor.x, (float)mouse->cursor.y), &ray) ||
      !s_editor_gizmo_ray_plane_intersect(ray, editor->gizmo.drag_origin,
          editor->gizmo.drag_plane_normal, &hit_position))
  {
    return false;
  }

  *out_parameter = vec3_dot(
      vec3_sub(hit_position, editor->gizmo.drag_origin),
      editor->gizmo.drag_axis);
  return true;
}

static bool s_editor_gizmo_translate_drag_update(
    LDKEditorContext *editor, LDKMouseState const *mouse)
{
  LDKEditorGizmoRay ray;
  Vec3 hit_position;
  float current_parameter;
  float translation;
  Vec3 world_position;

  if (editor->gizmo.active_axis == LDK_EDITOR_GIZMO_AXIS_ALL)
  {
    if (!s_editor_gizmo_scene_ray_get(editor,
            ldk_pointf(
                (float)mouse->cursor.x, (float)mouse->cursor.y), &ray) ||
        !s_editor_gizmo_ray_plane_intersect(ray,
            editor->gizmo.drag_origin, editor->gizmo.drag_plane_normal,
            &hit_position))
    {
      return true;
    }

    world_position = vec3_add(editor->gizmo.drag_origin,
        vec3_sub(hit_position, editor->gizmo.drag_initial_hit));
    return s_editor_gizmo_world_position_set(
        editor->gizmo.drag_entity, world_position);
  }

  if (!s_editor_gizmo_drag_parameter_get(
          editor, mouse, &current_parameter))
  {
    return true;
  }

  translation = current_parameter - editor->gizmo.drag_initial_parameter;
  world_position = vec3_add(editor->gizmo.drag_origin,
      vec3_mul(editor->gizmo.drag_axis, translation));
  return s_editor_gizmo_world_position_set(
      editor->gizmo.drag_entity, world_position);
}

static bool s_editor_gizmo_scale_drag_update(
    LDKEditorContext *editor, LDKMouseState const *mouse)
{
  LDKEditorGizmoAxis axis;
  Vec3 local_scale;
  float factor;

  axis = editor->gizmo.active_axis;
  if (axis != LDK_EDITOR_GIZMO_AXIS_X &&
      axis != LDK_EDITOR_GIZMO_AXIS_Y &&
      axis != LDK_EDITOR_GIZMO_AXIS_Z &&
      axis != LDK_EDITOR_GIZMO_AXIS_ALL)
  {
    return false;
  }

  if (axis == LDK_EDITOR_GIZMO_AXIS_ALL)
  {
    float delta_x =
        (float)mouse->cursor.x - editor->gizmo.drag_initial_cursor.x;
    float delta_y =
        editor->gizmo.drag_initial_cursor.y - (float)mouse->cursor.y;
    factor = 1.0f +
             (delta_x + delta_y) /
                 (LDK_EDITOR_GIZMO_PIXEL_LENGTH * 2.0f);
  }
  else
  {
    float current_parameter;

    if (fabsf(editor->gizmo.drag_initial_parameter) <=
            LDK_EDITOR_GIZMO_INTERSECTION_EPSILON ||
        !s_editor_gizmo_drag_parameter_get(
            editor, mouse, &current_parameter))
    {
      return true;
    }

    // Both parameters are signed distances from the gizmo origin along the
    // active axis. Their ratio is 1 at drag start, 0 at the origin and
    // negative immediately after the cursor crosses the origin. Using the
    // full gizmo length here would move the zero point whenever the user
    // grabbed the bar anywhere other than its endpoint.
    factor = current_parameter / editor->gizmo.drag_initial_parameter;
  }

  local_scale = editor->gizmo.drag_initial_scale;

  if (axis == LDK_EDITOR_GIZMO_AXIS_X ||
      axis == LDK_EDITOR_GIZMO_AXIS_ALL)
  {
    local_scale.x *= factor;
  }
  if (axis == LDK_EDITOR_GIZMO_AXIS_Y ||
      axis == LDK_EDITOR_GIZMO_AXIS_ALL)
  {
    local_scale.y *= factor;
  }
  if (axis == LDK_EDITOR_GIZMO_AXIS_Z ||
      axis == LDK_EDITOR_GIZMO_AXIS_ALL)
  {
    local_scale.z *= factor;
  }

  return s_editor_gizmo_local_scale_set(
      editor->gizmo.drag_entity, local_scale);
}

static bool s_editor_gizmo_rotation_drag_update(
    LDKEditorContext *editor, LDKMouseState const *mouse)
{
  LDKEditorGizmoRay ray;
  Vec3 hit_position;
  Vec3 current_direction;
  Vec3 cross;
  Quat delta_rotation;
  Quat world_rotation;
  Quat local_rotation;
  float current_direction_length;
  float current_angle;
  float angle_delta;

  if (editor == NULL || mouse == NULL ||
      !s_editor_gizmo_scene_ray_get(editor,
          ldk_pointf((float)mouse->cursor.x,
              (float)mouse->cursor.y),
          &ray) ||
      !s_editor_gizmo_ray_plane_intersect(ray,
          editor->gizmo.drag_origin, editor->gizmo.drag_plane_normal,
          &hit_position))
  {
    return true;
  }

  current_direction =
      vec3_sub(hit_position, editor->gizmo.drag_origin);
  current_direction_length = vec3_len(current_direction);
  if (current_direction_length <= LDK_EDITOR_GIZMO_INTERSECTION_EPSILON)
  {
    return true;
  }

  current_direction =
      vec3_div(current_direction, current_direction_length);
  cross = vec3_cross(
      editor->gizmo.drag_initial_direction, current_direction);
  current_angle = atan2f(
      vec3_dot(editor->gizmo.drag_axis, cross),
      vec3_dot(editor->gizmo.drag_initial_direction, current_direction));
  angle_delta = current_angle - editor->gizmo.drag_previous_angle;

  if (angle_delta > LDK_EDITOR_GIZMO_PI)
  {
    angle_delta -= LDK_EDITOR_GIZMO_PI * 2.0f;
  }
  else if (angle_delta < -LDK_EDITOR_GIZMO_PI)
  {
    angle_delta += LDK_EDITOR_GIZMO_PI * 2.0f;
  }

  editor->gizmo.drag_accumulated_angle += angle_delta;
  editor->gizmo.drag_previous_angle = current_angle;

  delta_rotation = quat_axis_angle(editor->gizmo.drag_axis,
      editor->gizmo.drag_accumulated_angle);
  world_rotation = quat_norm(quat_mul(delta_rotation,
      editor->gizmo.drag_initial_world_rotation));
  local_rotation = quat_norm(quat_mul(
      quat_inverse(editor->gizmo.drag_parent_world_rotation),
      world_rotation));

  return s_editor_gizmo_local_rotation_set(
      editor->gizmo.drag_entity, local_rotation);
}

void ldki_editor_gizmo_update(LDKEditorContext *editor)
{
  LDKECS *ecs;
  LDKMouseState mouse;
  bool update_ok;
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

  if (editor->gizmo.drag_mode == LDK_EDITOR_GIZMO_MODE_TRANSLATE)
  {
    update_ok = s_editor_gizmo_translate_drag_update(editor, &mouse);
  }
  else if (editor->gizmo.drag_mode == LDK_EDITOR_GIZMO_MODE_ROTATE)
  {
    update_ok = s_editor_gizmo_rotation_drag_update(editor, &mouse);
  }
  else if (editor->gizmo.drag_mode == LDK_EDITOR_GIZMO_MODE_SCALE)
  {
    update_ok = s_editor_gizmo_scale_drag_update(editor, &mouse);
  }
  else
  {
    update_ok = false;
  }

  if (!update_ok)
  {
    s_editor_gizmo_drag_end(editor);
    return;
  }

  if (released)
  {
    s_editor_gizmo_drag_end(editor);
  }
}

static void s_editor_gizmo_rotation_submit(LDKEditorContext *editor,
    Vec3 origin, Mat4 orientation,
    Vec3 const axes[LDK_EDITOR_GIZMO_AXIS_COUNT], float length)
{
  float radius;
  float line_length;
  float line_thickness;

  if (editor == NULL || axes == NULL)
  {
    return;
  }

  radius = length * LDK_EDITOR_GIZMO_ROTATION_RADIUS_SCALE;
  line_length = radius *
                LDK_EDITOR_GIZMO_ROTATION_RING_INNER_RADIUS;
  line_thickness =
      length * LDK_EDITOR_GIZMO_ROTATION_ORIGIN_LINE_SCALE;

  for (u32 axis = 0u; axis < LDK_EDITOR_GIZMO_AXIS_COUNT; ++axis)
  {
    Vec3 line_position = vec3_add(
        origin, vec3_mul(axes[axis], line_length * 0.5f));
    Vec3 line_scale =
        vec3_make(line_thickness, line_thickness, line_thickness);

    if (axis == 0u)
    {
      line_scale.x = line_length;
    }
    else if (axis == 1u)
    {
      line_scale.y = line_length;
    }
    else
    {
      line_scale.z = line_length;
    }
  }

  for (u32 axis = 0u; axis < LDK_EDITOR_GIZMO_AXIS_COUNT; ++axis)
  {
    bool highlighted =
        editor->gizmo.hovered_axis ==
        (LDKEditorGizmoAxis)(axis + LDK_EDITOR_GIZMO_AXIS_X);
    LDKResourceMesh arc_mesh = highlighted
        ? editor->gizmo.rotation_arc_highlight_meshes[axis]
        : editor->gizmo.rotation_arc_meshes[axis];
    Mat4 ring_orientation =
        s_editor_gizmo_rotation_ring_orientation(orientation, axis);

    ldk_renderer_submit_overlay_mesh_to_view(
        editor->renderer, editor->scene_view, arc_mesh,
        s_editor_gizmo_part_world(origin, ring_orientation,
            vec3_make(radius, radius, radius)));
  }
}

static void s_editor_gizmo_rotation_drag_submit(
    LDKEditorContext *editor)
{
  u32 axis;
  float radius;
  Mat4 ring_orientation;
  Mat4 reflected_orientation;
  Vec3 ring_scale;

  if (editor == NULL ||
      editor->gizmo.active_axis < LDK_EDITOR_GIZMO_AXIS_X ||
      editor->gizmo.active_axis > LDK_EDITOR_GIZMO_AXIS_Z)
  {
    return;
  }

  axis = (u32)(editor->gizmo.active_axis - LDK_EDITOR_GIZMO_AXIS_X);
  radius = editor->gizmo.drag_world_length *
           LDK_EDITOR_GIZMO_ROTATION_RADIUS_SCALE;
  ring_orientation = s_editor_gizmo_rotation_ring_orientation(
      editor->gizmo.drag_orientation, axis);
  reflected_orientation = mat4_mul(ring_orientation,
      mat4_scale(vec3_make(1.0f, 1.0f, -1.0f)));
  ring_scale = vec3_make(radius, radius, radius);

  ldk_renderer_submit_overlay_mesh_to_view(editor->renderer,
      editor->scene_view,
      editor->gizmo.rotation_arc_highlight_meshes[axis],
      s_editor_gizmo_part_world(editor->gizmo.drag_origin,
          ring_orientation, ring_scale));
  ldk_renderer_submit_overlay_mesh_to_view(editor->renderer,
      editor->scene_view,
      editor->gizmo.rotation_arc_highlight_meshes[axis],
      s_editor_gizmo_part_world(editor->gizmo.drag_origin,
          reflected_orientation, ring_scale));
}

void ldki_editor_gizmo_submit(LDKEditorContext *editor)
{
  LDKECS *ecs;
  LDKEntity selected;
  Mat4 selected_world;
  Mat4 orientation;
  Vec3 axes[LDK_EDITOR_GIZMO_AXIS_COUNT];
  Vec3 origin;
  LDKEditorGizmoMode mode;
  float length;
  float bar_length;
  float bar_thickness;
  float handle_size;
  float center_size;

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

  // Keep the interaction orientation fixed during a drag. Local mode uses
  // the entity orientation captured at drag start and adopts the resulting
  // local orientation after the drag ends.
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
  mode = editor->gizmo.dragging
             ? editor->gizmo.drag_mode
             : editor->gizmo.mode;
  length = s_editor_gizmo_world_length(editor, origin);

  if (mode == LDK_EDITOR_GIZMO_MODE_ROTATE)
  {
    if (editor->gizmo.dragging)
    {
      s_editor_gizmo_rotation_drag_submit(editor);
    }
    else
    {
      s_editor_gizmo_rotation_submit(
          editor, origin, orientation, axes, length);
    }
    return;
  }

  bar_length = length * 0.78f;
  bar_thickness = length * 0.055f;
  handle_size = length * 0.22f;
  center_size = length * 0.18f;

  for (u32 axis = 0; axis < LDK_EDITOR_GIZMO_AXIS_COUNT; ++axis)
  {
    bool highlighted =
        editor->gizmo.hovered_axis ==
        (LDKEditorGizmoAxis)(axis + LDK_EDITOR_GIZMO_AXIS_X);
    LDKResourceMesh bar_mesh = highlighted
        ? editor->gizmo.cube_highlight_mesh
        : editor->gizmo.axis_cube_meshes[axis];
    LDKResourceMesh handle_mesh = mode == LDK_EDITOR_GIZMO_MODE_SCALE
        ? (highlighted ? editor->gizmo.cube_highlight_mesh
                       : editor->gizmo.axis_cube_meshes[axis])
        : (highlighted ? editor->gizmo.cone_highlight_mesh
                       : editor->gizmo.axis_cone_meshes[axis]);
    Vec3 direction = axes[axis];
    Vec3 bar_position =
        vec3_add(origin, vec3_mul(direction, bar_length * 0.5f));
    Vec3 bar_scale = vec3_make(
        bar_thickness, bar_thickness, bar_thickness);
    Vec3 handle_position = vec3_add(
        origin, vec3_mul(direction, bar_length + handle_size * 0.5f));
    Vec3 handle_scale =
        vec3_make(handle_size, handle_size, handle_size);
    Mat4 handle_orientation = mode == LDK_EDITOR_GIZMO_MODE_SCALE
        ? orientation
        : s_editor_gizmo_handle_orientation(orientation, axis);

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

    ldk_renderer_submit_overlay_mesh_to_view(
        editor->renderer, editor->scene_view,
        bar_mesh,
        s_editor_gizmo_part_world(bar_position, orientation, bar_scale));
    ldk_renderer_submit_overlay_mesh_to_view(
        editor->renderer, editor->scene_view,
        handle_mesh,
        s_editor_gizmo_part_world(
            handle_position, handle_orientation, handle_scale));
  }

  {
    bool highlighted =
        editor->gizmo.hovered_axis == LDK_EDITOR_GIZMO_AXIS_ALL;
    LDKResourceMesh center_mesh = highlighted
        ? editor->gizmo.cube_highlight_mesh
        : editor->gizmo.center_cube_mesh;
    Vec3 center_scale =
        vec3_make(center_size, center_size, center_size);

    ldk_renderer_submit_overlay_mesh_to_view(
        editor->renderer, editor->scene_view, center_mesh,
        s_editor_gizmo_part_world(origin, orientation, center_scale));
  }
}
