#include <component/ldk_light.h>
#include <module/ldk_entity.h>
#include <stdx/stdx_math.h>

#ifdef LDK_ENGINE
static bool s_light_can_attach(LDKEntityRegistry *registry, LDKEntity entity)
{
  return registry &&
         ldk_entity_internal_flags_has(
             registry, entity, LDK_ENTITY_INTERNAL_HAS_TRANSFORM) &&
         !ldk_entity_internal_flags_has(
             registry, entity, LDK_ENTITY_INTERNAL_HAS_LIGHT);
}

static void s_light_destroy(LDKEntityRegistry *registry,
    LDKComponentRegistry *components, LDKEntity entity, void *component,
    u32 index, void *user)
{
  (void)components;
  (void)component;
  (void)index;
  (void)user;
  if (registry)
  {
    ldk_entity_internal_flags_remove(
        registry, entity, LDK_ENTITY_INTERNAL_HAS_LIGHT);
  }
}

static bool s_point_light_attach(LDKEntityRegistry *registry,
    LDKComponentRegistry *components, LDKEntity entity, void *component,
    u32 index, const void *initial_value, void *user)
{
  (void)components;
  (void)index;
  (void)user;
  if (!component || !s_light_can_attach(registry, entity))
  {
    return false;
  }
  if (!initial_value)
  {
    LDKPointLight *light = component;
    *light = (LDKPointLight){0};
    light->color = 0xffffffffu;
    light->intensity = 1.0f;
    light->range = 10.0f;
    light->enabled = true;
  }
  ldk_entity_internal_flags_add(
      registry, entity, LDK_ENTITY_INTERNAL_HAS_LIGHT);
  return true;
}

LDKComponentDesc ldk_point_light_component_desc(u32 initial_capacity)
{
  LDKComponentDesc desc = {0};
  desc.name = "PointLight";
  desc.type = LDK_COMPONENT_TYPE_POINT_LIGHT;
  desc.entry_size = sizeof(LDKPointLight);
  desc.initial_capacity = initial_capacity;
  desc.attach = s_point_light_attach;
  desc.destroy = s_light_destroy;
  return desc;
}

static bool s_spot_light_attach(LDKEntityRegistry *registry,
    LDKComponentRegistry *components, LDKEntity entity, void *component,
    u32 index, const void *initial_value, void *user)
{
  (void)components;
  (void)index;
  (void)user;
  if (!component || !s_light_can_attach(registry, entity))
  {
    return false;
  }
  if (!initial_value)
  {
    LDKSpotLight *light = component;
    *light = (LDKSpotLight){0};
    light->color = 0xffffffffu;
    light->intensity = 1.0f;
    light->range = 10.0f;
    light->inner_angle = deg_to_rad(20.0f);
    light->outer_angle = deg_to_rad(30.0f);
    light->enabled = true;
  }
  ldk_entity_internal_flags_add(
      registry, entity, LDK_ENTITY_INTERNAL_HAS_LIGHT);
  return true;
}

LDKComponentDesc ldk_spot_light_component_desc(u32 initial_capacity)
{
  LDKComponentDesc desc = {0};
  desc.name = "SpotLight";
  desc.type = LDK_COMPONENT_TYPE_SPOT_LIGHT;
  desc.entry_size = sizeof(LDKSpotLight);
  desc.initial_capacity = initial_capacity;
  desc.attach = s_spot_light_attach;
  desc.destroy = s_light_destroy;
  return desc;
}

static bool s_directional_light_attach(LDKEntityRegistry *registry,
    LDKComponentRegistry *components, LDKEntity entity, void *component,
    u32 index, const void *initial_value, void *user)
{
  (void)components;
  (void)index;
  (void)user;
  if (!component || !s_light_can_attach(registry, entity))
  {
    return false;
  }
  if (!initial_value)
  {
    LDKDirectionalLight *light = component;
    *light = (LDKDirectionalLight){0};
    light->color = 0xffffffffu;
    light->intensity = 1.0f;
    light->enabled = true;
  }
  ldk_entity_internal_flags_add(
      registry, entity, LDK_ENTITY_INTERNAL_HAS_LIGHT);
  return true;
}

LDKComponentDesc ldk_directional_light_component_desc(u32 initial_capacity)
{
  LDKComponentDesc desc = {0};
  desc.name = "DirectionalLight";
  desc.type = LDK_COMPONENT_TYPE_DIRECTIONAL_LIGHT;
  desc.entry_size = sizeof(LDKDirectionalLight);
  desc.initial_capacity = initial_capacity;
  desc.attach = s_directional_light_attach;
  desc.destroy = s_light_destroy;
  return desc;
}

#endif // LDK_ENGINE
