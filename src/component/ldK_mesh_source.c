/**
 * @file ldk_mesh_source.c
 * @brief Mesh source component data.
 */

#include <component/ldk_mesh_source.h>
#include <ldk_resource.h>
#include <ldk_material_asset.h>
#include <module/ldk_renderer.h>

static LDKMeshSource s_mesh_source_make_default(void)
{
  LDKMeshSource mesh_source = {0};

  mesh_source.source_asset = ldk_asset_mesh_null();
  ldk_material_desc_defaults(
      LDK_MATERIAL_TYPE_VERTEX_COLOR, &mesh_source.material);
  mesh_source.renderer_mesh = LDK_RESOURCE_MESH_INVALID;
  mesh_source.renderer_material = LDK_RESOURCE_MATERIAL_INVALID;
  mesh_source.dirty = true;
  mesh_source.material_dirty = true;
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

  if (!ldk_entity_internal_flags_has(entity_registry, entity, LDK_ENTITY_INTERNAL_HAS_TRANSFORM))
  {
    return false;
  }

  if (!initial_value)
  {
    *mesh_source = s_mesh_source_make_default();
  }
  else
  {
    if (!ldk_material_desc_is_valid(&mesh_source->material))
    {
      ldk_material_desc_defaults(
          LDK_MATERIAL_TYPE_VERTEX_COLOR, &mesh_source->material);
    }

    mesh_source->renderer_mesh = LDK_RESOURCE_MESH_INVALID;
    mesh_source->renderer_material = LDK_RESOURCE_MATERIAL_INVALID;
    mesh_source->renderer_texture = LDK_RESOURCE_TEXTURE_INVALID;
    mesh_source->renderer = NULL;
    mesh_source->dirty = true;
    mesh_source->material_dirty = true;
  }

  ldk_entity_internal_flags_add(
      entity_registry,
      entity,
      LDK_ENTITY_INTERNAL_HAS_RENDERABLE);

  return true;
}

static void s_mesh_source_destroy(LDKEntityRegistry* entity_registry, LDKComponentRegistry* component_registry,
    LDKEntity entity, void* component, u32 component_index, void* user)
{
  LDKMeshSource* mesh_source = (LDKMeshSource*)component;

  (void)component_registry;
  (void)component_index;
  (void)user;

  if (!entity_registry)
  {
    return;
  }

  if (mesh_source && mesh_source->renderer)
  {
    ldk_renderer_material_destroy(
        mesh_source->renderer, mesh_source->renderer_material);
    ldk_renderer_image_release(
        mesh_source->renderer, mesh_source->renderer_texture);
    mesh_source->renderer_texture = LDK_RESOURCE_TEXTURE_INVALID;
    ldk_renderer_mesh_destroy(
        mesh_source->renderer, mesh_source->renderer_mesh);
    mesh_source->renderer_material = LDK_RESOURCE_MATERIAL_INVALID;
    mesh_source->renderer_mesh = LDK_RESOURCE_MESH_INVALID;
    mesh_source->renderer = NULL;
  }

  ldk_entity_internal_flags_remove(
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
  mesh_source->dirty = true;
  return true;
}

bool ldk_mesh_source_set_material(
    LDKMeshSource* mesh_source, LDKMaterialDesc const* material)
{
  if (!mesh_source || !ldk_material_desc_is_valid(material))
  {
    return false;
  }

  mesh_source->material_asset = ldk_asset_material_null();
  mesh_source->material_revision = 0;
  if (ldk_material_desc_equal(&mesh_source->material, material))
  {
    return true;
  }

  mesh_source->material = *material;
  mesh_source->material_dirty = true;
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

bool ldk_mesh_source_set_material_asset(LDKMeshSource* mesh_source,
    LDKAssetManager* assets, LDKAssetMaterial asset)
{
  const LDKAssetMaterialData* data =
      ldk_asset_manager_material_get_const(assets, asset);
  if (!mesh_source || !data)
    return false;
  if (!ldk_material_desc_equal(&mesh_source->material, &data->descriptor))
    mesh_source->material_dirty = true;
  mesh_source->material = data->descriptor;
  mesh_source->material_asset = asset;
  mesh_source->material_revision = data->revision;
  return true;
}

bool ldk_mesh_source_material_sync(LDKMeshSource* mesh_source,
    LDKAssetManager* assets)
{
  if (!mesh_source)
    return false;
  if (!mesh_source->material_revision)
    return true;
  const LDKAssetMaterialData* data =
      ldk_asset_manager_material_get_const(assets, mesh_source->material_asset);
  if (!data)
    return false;
  if (data->revision != mesh_source->material_revision)
    return ldk_mesh_source_set_material_asset(
        mesh_source, assets, mesh_source->material_asset);
  return true;
}
