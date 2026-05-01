/**
 * @file ldk_component_camera.c
 * @brief Camera component API.
 */

#include <component/ldk_camera.h>
#include <ldk.h>

#include <string.h>

static bool s_ldk_camera_get_modules(
    LDKEntityRegistry** out_entity_registry,
    LDKComponentRegistry** out_component_registry)
{
  LDKEntityRegistry* entity_registry = NULL;
  LDKComponentRegistry* component_registry = NULL;

  if (!out_entity_registry || !out_component_registry)
  {
    return false;
  }

  if (!ldk_engine_is_initialized())
  {
    return false;
  }

  entity_registry = (LDKEntityRegistry*)ldk_module_get(LDK_MODULE_ENTITY);
  component_registry = (LDKComponentRegistry*)ldk_module_get(LDK_MODULE_COMPONENT);

  if (!entity_registry || !component_registry)
  {
    return false;
  }

  *out_entity_registry = entity_registry;
  *out_component_registry = component_registry;
  return true;
}

static bool s_ldk_camera_get_ref(
    LDKEntityRegistry* entity_registry,
    LDKEntity entity,
    LDKComponentRef* out_ref)
{
  if (!entity_registry || !out_ref)
  {
    return false;
  }

  return ldk_entity_get_component_ref(
      entity_registry,
      entity,
      LDK_COMPONENT_TYPE_CAMERA,
      out_ref);
}

static LDKCamera* s_ldk_camera_from_ref(
    LDKEntityRegistry* entity_registry,
    LDKComponentRegistry* component_registry,
    LDKComponentRef ref)
{
  return (LDKCamera*)ldk_component_ref_get(entity_registry, component_registry, ref);
}

static const LDKCamera* s_ldk_camera_from_ref_const(
    LDKEntityRegistry* entity_registry,
    LDKComponentRegistry* component_registry,
    LDKComponentRef ref)
{
  return (const LDKCamera*)ldk_component_ref_get_const(entity_registry, component_registry, ref);
}

static LDKCamera* s_ldk_camera_get_ptr(
    LDKEntityRegistry* entity_registry,
    LDKComponentRegistry* component_registry,
    LDKEntity entity)
{
  LDKComponentRef ref = {0};

  if (!s_ldk_camera_get_ref(entity_registry, entity, &ref))
  {
    return NULL;
  }

  return s_ldk_camera_from_ref(entity_registry, component_registry, ref);
}

static const LDKCamera* s_ldk_camera_get_ptr_const(
    LDKEntityRegistry* entity_registry,
    LDKComponentRegistry* component_registry,
    LDKEntity entity)
{
  LDKComponentRef ref = {0};

  if (!s_ldk_camera_get_ref(entity_registry, entity, &ref))
  {
    return NULL;
  }

  return s_ldk_camera_from_ref_const(entity_registry, component_registry, ref);
}

static bool s_ldk_camera_has(
    LDKEntityRegistry* entity_registry,
    LDKEntity entity)
{
  if (!entity_registry)
  {
    return false;
  }

  return ldk_entity_has_component(
      entity_registry,
      entity,
      LDK_COMPONENT_TYPE_CAMERA);
}

LDKCamera ldk_camera_make_default(void)
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

bool ldk_camera_register(LDKComponentRegistry* registry, u32 initial_capacity)
{
  if (!registry)
  {
    return false;
  }

  return ldk_component_register(
      registry,
      "Camera",
      LDK_COMPONENT_TYPE_CAMERA,
      sizeof(LDKCamera),
      initial_capacity);
}

bool ldk_camera_attach(LDKEntity entity, const LDKCamera* initial_value)
{
  LDKEntityRegistry* entity_registry = NULL;
  LDKComponentRegistry* component_registry = NULL;
  LDKCamera camera = ldk_camera_make_default();
  LDKCamera* new_camera = NULL;

  if (!s_ldk_camera_get_modules(&entity_registry, &component_registry))
  {
    return false;
  }

  if (!ldk_entity_is_alive(entity_registry, entity))
  {
    return false;
  }

  if (!ldk_component_is_registered(component_registry, LDK_COMPONENT_TYPE_CAMERA))
  {
    return false;
  }

  if (s_ldk_camera_has(entity_registry, entity))
  {
    return false;
  }

  if (initial_value)
  {
    camera = *initial_value;
  }

  new_camera = (LDKCamera*)ldk_entity_add_component(
      entity_registry,
      component_registry,
      entity,
      LDK_COMPONENT_TYPE_CAMERA);

  if (!new_camera)
  {
    return false;
  }

  *new_camera = camera;

  ldk_entity_add_internal_flags(
      entity_registry,
      entity,
      LDK_ENTITY_INTERNAL_HAS_CAMERA);

  return true;
}

bool ldk_camera_detach(LDKEntity entity)
{
  LDKEntityRegistry* entity_registry = NULL;
  LDKComponentRegistry* component_registry = NULL;
  LDKCamera* camera = NULL;

  if (!s_ldk_camera_get_modules(&entity_registry, &component_registry))
  {
    return false;
  }

  camera = s_ldk_camera_get_ptr(entity_registry, component_registry, entity);
  if (!camera)
  {
    return false;
  }

  ldk_entity_remove_internal_flags(entity_registry, entity, LDK_ENTITY_INTERNAL_HAS_CAMERA);

  return ldk_component_remove_entity(
      component_registry,
      entity_registry,
      entity,
      LDK_COMPONENT_TYPE_CAMERA);
}

bool ldk_camera_get(LDKEntity entity, LDKCamera* out_camera)
{
  LDKEntityRegistry* entity_registry = NULL;
  LDKComponentRegistry* component_registry = NULL;
  const LDKCamera* camera = NULL;

  if (!out_camera)
  {
    return false;
  }

  if (!s_ldk_camera_get_modules(&entity_registry, &component_registry))
  {
    return false;
  }

  camera = s_ldk_camera_get_ptr_const(entity_registry, component_registry, entity);
  if (!camera)
  {
    return false;
  }

  *out_camera = *camera;
  return true;
}

bool ldk_camera_set(LDKEntity entity, const LDKCamera* camera)
{
  LDKEntityRegistry* entity_registry = NULL;
  LDKComponentRegistry* component_registry = NULL;
  LDKCamera* current = NULL;

  if (!camera)
  {
    return false;
  }

  if (!s_ldk_camera_get_modules(&entity_registry, &component_registry))
  {
    return false;
  }

  current = s_ldk_camera_get_ptr(entity_registry, component_registry, entity);
  if (!current)
  {
    return false;
  }

  *current = *camera;
  return true;
}

bool ldk_camera_set_enabled(LDKEntity entity, bool enabled)
{
  LDKEntityRegistry* entity_registry = NULL;
  LDKComponentRegistry* component_registry = NULL;
  LDKCamera* camera = NULL;

  if (!s_ldk_camera_get_modules(&entity_registry, &component_registry))
  {
    return false;
  }

  camera = s_ldk_camera_get_ptr(entity_registry, component_registry, entity);
  if (!camera)
  {
    return false;
  }

  camera->enabled = enabled;
  return true;
}

bool ldk_camera_set_role(LDKEntity entity, LDKCameraRole role)
{
  LDKEntityRegistry* entity_registry = NULL;
  LDKComponentRegistry* component_registry = NULL;
  LDKCamera* camera = NULL;

  if (!s_ldk_camera_get_modules(&entity_registry, &component_registry))
  {
    return false;
  }

  camera = s_ldk_camera_get_ptr(entity_registry, component_registry, entity);
  if (!camera)
  {
    return false;
  }

  camera->role = role;
  return true;
}

bool ldk_camera_set_perspective(LDKEntity entity, float fov_y, float near_plane, float far_plane)
{
  LDKEntityRegistry* entity_registry = NULL;
  LDKComponentRegistry* component_registry = NULL;
  LDKCamera* camera = NULL;

  if (!s_ldk_camera_get_modules(&entity_registry, &component_registry))
  {
    return false;
  }

  camera = s_ldk_camera_get_ptr(entity_registry, component_registry, entity);
  if (!camera)
  {
    return false;
  }

  camera->projection = LDK_CAMERA_PROJECTION_PERSPECTIVE;
  camera->fov_y = fov_y;
  camera->near_plane = near_plane;
  camera->far_plane = far_plane;
  return true;
}

bool ldk_camera_set_orthographic(LDKEntity entity, float orthographic_height, float near_plane, float far_plane)
{
  LDKEntityRegistry* entity_registry = NULL;
  LDKComponentRegistry* component_registry = NULL;
  LDKCamera* camera = NULL;

  if (!s_ldk_camera_get_modules(&entity_registry, &component_registry))
  {
    return false;
  }

  camera = s_ldk_camera_get_ptr(entity_registry, component_registry, entity);
  if (!camera)
  {
    return false;
  }

  camera->projection = LDK_CAMERA_PROJECTION_ORTHOGRAPHIC;
  camera->orthographic_height = orthographic_height;
  camera->near_plane = near_plane;
  camera->far_plane = far_plane;
  return true;
}

bool ldk_camera_get_enabled(LDKEntity entity, bool* out_enabled)
{
  LDKCamera camera = {0};

  if (!out_enabled)
  {
    return false;
  }

  if (!ldk_camera_get(entity, &camera))
  {
    return false;
  }

  *out_enabled = camera.enabled;
  return true;
}

bool ldk_camera_get_role(LDKEntity entity, LDKCameraRole* out_role)
{
  LDKCamera camera = {0};

  if (!out_role)
  {
    return false;
  }

  if (!ldk_camera_get(entity, &camera))
  {
    return false;
  }

  *out_role = camera.role;
  return true;
}
