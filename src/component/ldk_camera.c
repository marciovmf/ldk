/**
 * @file ldk_camera.c
 * @brief Camera component data.
 */

#include <component/ldk_transform.h>
#include <component/ldk_camera.h>
#include <module/ldk_ecs.h>
#include <stdx/stdx_math.h>

static LDKCamera s_camera_make_default(void)
{
  LDKCamera camera = {0};

  camera.projection = LDK_CAMERA_PROJECTION_PERSPECTIVE;
  camera.role = LDK_CAMERA_ROLE_NONE;
  camera.fov_y = deg_to_rad(60.0f);
  camera.orthographic_height = 10.0f;
  camera.near_plane = 0.1f;
  camera.far_plane = 1000.0f;
  camera.enabled = true;
  return camera;
}

#ifdef LDK_ENGINE
static bool s_camera_attach(LDKEntityRegistry* entity_registry, LDKComponentRegistry* component_registry,
    LDKEntity entity, void* component, u32 component_index, const void* initial_value, void* user)
{
  LDKCamera* camera = (LDKCamera*)component;

  (void)component_registry;
  (void)component_index;
  (void)user;

  if (!entity_registry || !camera)
  {
    return false;
  }

  if (!ldk_entity_internal_flags_has(entity_registry, entity, LDK_ENTITY_INTERNAL_HAS_TRANSFORM))
  {
    return false;
  }

  if (!initial_value)
  {
    *camera = s_camera_make_default();
  }

  ldk_entity_internal_flags_add(
      entity_registry,
      entity,
      LDK_ENTITY_INTERNAL_HAS_CAMERA);

  return true;
}

static void s_camera_destroy(LDKEntityRegistry* entity_registry, LDKComponentRegistry* component_registry,
    LDKEntity entity, void* component, u32 component_index, void* user)
{
  (void)component_registry;
  (void)component;
  (void)component_index;
  (void)user;

  if (!entity_registry)
  {
    return;
  }

  ldk_entity_internal_flags_remove(
      entity_registry,
      entity,
      LDK_ENTITY_INTERNAL_HAS_CAMERA);
}

LDKComponentDesc ldk_camera_component_desc(u32 initial_capacity)
{
  LDKComponentDesc desc = {0};

  desc.name = "Camera";
  desc.type = LDK_COMPONENT_TYPE_CAMERA;
  desc.entry_size = sizeof(LDKCamera);
  desc.initial_capacity = initial_capacity;
  desc.attach = s_camera_attach;
  desc.destroy = s_camera_destroy;
  desc.user = NULL;
  return desc;
}

#endif // LDK_ENGINE

bool ldk_camera_look_at(LDKEntity entity, Vec3 target)
{
  Vec3 position = vec3_make(0.0f, 0.0f, 0.0f);

  if (!ldk_transform_get_local_position(entity, &position))
  {
    return false;
  }

  Mat4 view = mat4_look_at_rh(
      position,
      target,
      vec3_make(0.0f, 1.0f, 0.0f));

  Mat4 world = mat4_inverse_affine(view);

  Vec3 decomposed_position = vec3_make(0.0f, 0.0f, 0.0f);
  Quat rotation = quat_id();
  Vec3 scale = vec3_make(1.0f, 1.0f, 1.0f);

  mat4_decompose(
      world,
      &decomposed_position,
      &rotation,
      &scale);

  return ldk_transform_set_local_rotation(entity, rotation);
}

bool ldk_camera_get_world_matrix(LDKEntity entity, Mat4* out_world)
{
  if (!out_world)
  {
    return false;
  }

  return ldk_transform_get_world_matrix(entity, out_world);
}

bool ldk_camera_get_view_matrix(LDKEntity entity, Mat4* out_view)
{
  if (!out_view)
  {
    return false;
  }

  Mat4 world = mat4_identity();

  if (!ldk_camera_get_world_matrix(entity, &world))
  {
    return false;
  }

  *out_view = mat4_inverse_affine(world);
  return true;
}

bool ldk_camera_get_projection_matrix(LDKEntity entity, float aspect, Mat4* out_projection)
{
  if (!out_projection)
  {
    return false;
  }

  if (aspect <= 0.0f)
  {
    return false;
  }

  LDKCamera* camera = (LDKCamera*)ldk_ecs_component_get(
      entity,
      LDK_COMPONENT_TYPE_CAMERA);

  if (!camera)
  {
    return false;
  }

  if (camera->projection == LDK_CAMERA_PROJECTION_ORTHOGRAPHIC)
  {
    float half_height = camera->orthographic_height * 0.5f;
    float half_width = half_height * aspect;

    *out_projection = mat4_orthographic_rh_no(
        -half_width,
         half_width,
        -half_height,
         half_height,
         camera->near_plane,
         camera->far_plane);

    return true;
  }

  *out_projection = mat4_perspective_rh_no(
      camera->fov_y,
      aspect,
      camera->near_plane,
      camera->far_plane);

  return true;
}
