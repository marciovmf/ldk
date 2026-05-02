/**
 * @file ldk_mesh_source.c
 * @brief Mesh source component data.
 */

#include <component/ldk_mesh_source.h>

static LDKMeshSource s_mesh_source_make_default(void)
{
  LDKMeshSource mesh_source = {0};

  mesh_source.source_asset = ldk_asset_mesh_null();
  mesh_source.version = 1;
  mesh_source.renderer_mesh = LDK_RENDERER_MESH_INVALID;
  mesh_source.renderer_version = 0;
  return mesh_source;
}

static bool s_mesh_source_attach(LDKEntityRegistry* entity_registry, LDKComponentRegistry* component_registry,
    LDKEntity entity, void* component, u32 component_index, const void* initial_value, void* user)
{
  LDKMeshSource* mesh_source = (LDKMeshSource*)component;

  (void)component_registry;
  (void)component_index;
  (void)user;

  if (!entity_registry || !mesh_source)
  {
    return false;
  }

  if (!ldk_entity_has_internal_flags(entity_registry, entity, LDK_ENTITY_INTERNAL_HAS_TRANSFORM))
  {
    return false;
  }

  if (!initial_value)
  {
    *mesh_source = s_mesh_source_make_default();
  }

  ldk_entity_add_internal_flags(
      entity_registry,
      entity,
      LDK_ENTITY_INTERNAL_HAS_RENDERABLE);

  return true;
}

static void s_mesh_source_destroy(LDKEntityRegistry* entity_registry, LDKComponentRegistry* component_registry,
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
      LDK_ENTITY_INTERNAL_HAS_RENDERABLE);
}

bool ldk_mesh_source_set_data(LDKMeshSource* mesh_source, LDKAssetMesh asset)
{
  if (!mesh_source)
  {
    return false;
  }

  if (x_handle_is_null(asset.h))
  {
    return false;
  }

  mesh_source->source_asset = asset;
  mesh_source->version++;
  mesh_source->renderer_mesh = LDK_RENDERER_MESH_INVALID;
  mesh_source->renderer_version = 0;
  return true;
}

#ifdef LDK_ENGINE
LDKComponentDesc ldk_mesh_source_component_desc(u32 initial_capacity)
{
  LDKComponentDesc desc = {0};

  desc.name = "MeshSource";
  desc.type = LDK_COMPONENT_TYPE_MESH_SOURCE;
  desc.entry_size = sizeof(LDKMeshSource);
  desc.initial_capacity = initial_capacity;
  desc.attach = s_mesh_source_attach;
  desc.destroy = s_mesh_source_destroy;
  desc.user = NULL;
  return desc;
}
#endif // LDK_ENGINE
