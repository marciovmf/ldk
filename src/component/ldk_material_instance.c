#include <component/ldk_material_instance.h>
#include <module/ldk_entity.h>
#include <string.h>

#ifdef LDK_ENGINE
static bool s_material_instance_attach(LDKEntityRegistry *entities,
    LDKComponentRegistry *components, LDKEntity entity, void *component,
    u32 index, const void *initial_value, void *user)
{
  (void)entities;
  (void)components;
  (void)entity;
  (void)index;
  (void)user;
  if (!component)
    return false;
  LDKMaterialInstance *instance = component;
  *instance = initial_value ? *(const LDKMaterialInstance *)initial_value
                            : (LDKMaterialInstance){0};
  instance->atlas.h = x_handle_null();
  instance->cached_atlas.h = x_handle_null();
  instance->cache_valid = false;
  return true;
}

LDKComponentDesc ldk_material_instance_component_desc(u32 initial_capacity)
{
  static const u32 required[] = {LDK_COMPONENT_TYPE_MESH_SOURCE};
  LDKComponentDesc desc = {0};
  desc.name = "MaterialInstance";
  desc.type = LDK_COMPONENT_TYPE_MATERIAL_INSTANCE;
  desc.entry_size = sizeof(LDKMaterialInstance);
  desc.initial_capacity = initial_capacity;
  desc.required_components = required;
  desc.required_component_count = 1;
  desc.attach = s_material_instance_attach;
  return desc;
}
#endif
