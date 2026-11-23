#include <component/ldk_terrain_component.h>
#include <module/ldk_asset_manager.h>

LDKTerrainComponent ldk_terrain_component_make_default(void)
{
  LDKTerrainComponent component = {0};
  component.terrain = ldk_asset_terrain_null();
  return component;
}

#ifdef LDK_ENGINE
static bool s_terrain_attach(LDKEntityRegistry *entities,
    LDKComponentRegistry *components, LDKEntity entity, void *component,
    u32 index, const void *initial_value, void *user)
{
  (void)entities;
  (void)components;
  (void)entity;
  (void)index;
  (void)user;
  if (!component) return false;
  *(LDKTerrainComponent *)component = initial_value
      ? *(const LDKTerrainComponent *)initial_value
      : ldk_terrain_component_make_default();
  return true;
}

LDKComponentDesc ldk_terrain_component_desc(u32 initial_capacity)
{
  static const u32 required[] = {LDK_COMPONENT_TYPE_TRANSFORM};
  LDKComponentDesc desc = {0};
  desc.name = "TerrainComponent";
  desc.type = LDK_COMPONENT_TYPE_TERRAIN;
  desc.entry_size = sizeof(LDKTerrainComponent);
  desc.initial_capacity = initial_capacity;
  desc.required_components = required;
  desc.required_component_count = 1u;
  desc.attach = s_terrain_attach;
  return desc;
}
#endif
