#include <component/ldk_terrain_follower.h>

#include <module/ldk_entity.h>

#include <string.h>

LDKTerrainFollowerComponent ldk_terrain_follower_component_make_default(void)
{
  LDKTerrainFollowerComponent component;

  memset(&component, 0, sizeof(component));
  return component;
}

#ifdef LDK_ENGINE
static bool s_terrain_follower_attach(LDKEntityRegistry *entities,
    LDKComponentRegistry *components, LDKEntity entity, void *component,
    u32 index, const void *initial_value, void *user)
{
  LDKTerrainFollowerComponent *follower =
      (LDKTerrainFollowerComponent *)component;

  (void)entities;
  (void)components;
  (void)entity;
  (void)index;
  (void)user;

  if (!follower)
  {
    return false;
  }

  *follower = initial_value
      ? *(const LDKTerrainFollowerComponent *)initial_value
      : ldk_terrain_follower_component_make_default();
  return true;
}

LDKComponentDesc ldk_terrain_follower_component_desc(u32 initial_capacity)
{
  static const u32 required_components[] = {
      LDK_COMPONENT_TYPE_TRANSFORM};
  LDKComponentDesc desc = {0};

  desc.name = "TerrainFollowerComponent";
  desc.type = LDK_COMPONENT_TYPE_TERRAIN_FOLLOWER;
  desc.entry_size = sizeof(LDKTerrainFollowerComponent);
  desc.initial_capacity = initial_capacity;
  desc.required_components = required_components;
  desc.required_component_count =
      (u32)(sizeof(required_components) / sizeof(required_components[0]));
  desc.attach = s_terrain_follower_attach;
  return desc;
}
#endif
