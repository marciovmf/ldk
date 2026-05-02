/**
 * @file ldk_camera.c
 * @brief Camera component data.
 */

#include <component/ldk_camera.h>

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

  if (!ldk_entity_has_internal_flags(entity_registry, entity, LDK_ENTITY_INTERNAL_HAS_TRANSFORM))
  {
    return false;
  }

  if (!initial_value)
  {
    *camera = s_camera_make_default();
  }

  ldk_entity_add_internal_flags(
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

  ldk_entity_remove_internal_flags(
      entity_registry,
      entity,
      LDK_ENTITY_INTERNAL_HAS_CAMERA);
}


#ifdef LDK_ENGINE
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
