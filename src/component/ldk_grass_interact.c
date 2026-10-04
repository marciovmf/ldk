#include <component/ldk_grass_interact.h>

#include <module/ldk_entity.h>

#include <string.h>

LDKGrassInteractComponent ldk_grass_interact_component_make_default(void)
{
  LDKGrassInteractComponent component;

  memset(&component, 0, sizeof(component));
  component.enabled = true;
  component.radius = 0.5f;
  component.strength = 1.0f;
  return component;
}

#ifdef LDK_ENGINE
static bool s_grass_interact_attach(LDKEntityRegistry *entities,
    LDKComponentRegistry *components, LDKEntity entity, void *component,
    u32 index, const void *initial_value, void *user)
{
  LDKGrassInteractComponent *grass_interact =
      (LDKGrassInteractComponent *)component;

  (void)entities;
  (void)components;
  (void)entity;
  (void)index;
  (void)user;

  if (!grass_interact)
  {
    return false;
  }

  *grass_interact = initial_value
      ? *(const LDKGrassInteractComponent *)initial_value
      : ldk_grass_interact_component_make_default();
  return true;
}

LDKComponentDesc ldk_grass_interact_component_desc(u32 initial_capacity)
{
  static const u32 required_components[] = {
      LDK_COMPONENT_TYPE_TRANSFORM};
  LDKComponentDesc desc = {0};

  desc.name = "GrassInteractComponent";
  desc.type = LDK_COMPONENT_TYPE_GRASS_INTERACT;
  desc.entry_size = sizeof(LDKGrassInteractComponent);
  desc.initial_capacity = initial_capacity;
  desc.required_components = required_components;
  desc.required_component_count =
      (u32)(sizeof(required_components) / sizeof(required_components[0]));
  desc.attach = s_grass_interact_attach;
  return desc;
}
#endif
