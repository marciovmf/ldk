/**
 * @file ldk_mesh_source.c
 * @brief Mesh source component data.
 */

#include <component/ldk_mesh_source.h>
#include <ldk_resource.h>
#include <ldk_material_asset.h>
#include <module/ldk_renderer.h>

#include <stdlib.h>
#include <string.h>

static void s_material_binding_defaults(LDKMeshSourceMaterialBinding* binding)
{
  if (!binding)
  {
    return;
  }

  memset(binding, 0, sizeof(*binding));
  ldk_material_desc_defaults(
      LDK_MATERIAL_TYPE_VERTEX_COLOR, &binding->material);
  binding->material_asset = ldk_asset_material_null();
  binding->renderer_material = LDK_RESOURCE_MATERIAL_INVALID;
  binding->renderer_texture = LDK_RESOURCE_TEXTURE_INVALID;
  binding->material_dirty = true;
}

static void s_material_binding_release(
    LDKMeshSource* mesh_source, LDKMeshSourceMaterialBinding* binding)
{
  if (!mesh_source || !binding || !mesh_source->renderer)
  {
    return;
  }

  ldk_renderer_material_destroy(
      mesh_source->renderer, binding->renderer_material);
  ldk_renderer_image_release(
      mesh_source->renderer, binding->renderer_texture);
  binding->renderer_material = LDK_RESOURCE_MATERIAL_INVALID;
  binding->renderer_texture = LDK_RESOURCE_TEXTURE_INVALID;
}

static bool s_mesh_source_material_count_set(
    LDKMeshSource* mesh_source, u32 material_count)
{
  LDKMeshSourceMaterialBinding* additional = NULL;
  u32 old_count;
  u32 preserve_count;

  if (!mesh_source || material_count == 0)
  {
    return false;
  }

  old_count = mesh_source->material_count ? mesh_source->material_count : 1;
  if (!ldk_material_desc_is_valid(&mesh_source->material))
  {
    ldk_material_desc_defaults(
        LDK_MATERIAL_TYPE_VERTEX_COLOR, &mesh_source->material);
    mesh_source->material_asset = ldk_asset_material_null();
    mesh_source->material_revision = 0;
    mesh_source->renderer_material = LDK_RESOURCE_MATERIAL_INVALID;
    mesh_source->renderer_texture = LDK_RESOURCE_TEXTURE_INVALID;
    mesh_source->material_dirty = true;
  }

  if (old_count == material_count)
  {
    mesh_source->material_count = material_count;
    return true;
  }

  if (material_count > 1)
  {
    additional = (LDKMeshSourceMaterialBinding*)calloc(
        (size_t)material_count - 1u,
        sizeof(LDKMeshSourceMaterialBinding));
    if (!additional)
    {
      return false;
    }

    preserve_count = old_count < material_count ? old_count : material_count;
    if (preserve_count > 1 && mesh_source->additional_materials)
    {
      memcpy(additional, mesh_source->additional_materials,
          ((size_t)preserve_count - 1u) *
              sizeof(LDKMeshSourceMaterialBinding));
    }

    for (u32 i = preserve_count; i < material_count; i++)
    {
      s_material_binding_defaults(&additional[i - 1u]);
    }
  }

  if (mesh_source->additional_materials && old_count > material_count)
  {
    for (u32 i = material_count; i < old_count; i++)
    {
      s_material_binding_release(
          mesh_source, &mesh_source->additional_materials[i - 1u]);
    }
  }

  free(mesh_source->additional_materials);
  mesh_source->additional_materials = additional;
  mesh_source->material_count = material_count;
  return true;
}

static LDKMeshSource s_mesh_source_make_default(void)
{
  LDKMeshSource mesh_source = {0};

  mesh_source.source_asset = ldk_asset_mesh_null();
  ldk_material_desc_defaults(
      LDK_MATERIAL_TYPE_VERTEX_COLOR, &mesh_source.material);
  mesh_source.material_asset = ldk_asset_material_null();
  mesh_source.material_revision = 0;
  mesh_source.renderer_mesh = LDK_RESOURCE_MESH_INVALID;
  mesh_source.renderer_material = LDK_RESOURCE_MATERIAL_INVALID;
  mesh_source.renderer_texture = LDK_RESOURCE_TEXTURE_INVALID;
  mesh_source.dirty = true;
  mesh_source.material_dirty = true;
  mesh_source.material_count = 1;
  return mesh_source;
}

static bool s_mesh_source_attach(LDKEntityRegistry* entity_registry,
    LDKComponentRegistry* component_registry, LDKEntity entity,
    void* component, u32 component_index, const void* initial_value, void* user)
{
  LDKMeshSource* mesh_source = (LDKMeshSource*)component;

  (void)component_registry;
  (void)component_index;
  (void)user;

  if (!entity_registry || !mesh_source)
  {
    return false;
  }

  if (!ldk_entity_internal_flags_has(
          entity_registry, entity, LDK_ENTITY_INTERNAL_HAS_TRANSFORM))
  {
    return false;
  }

  if (!initial_value)
  {
    *mesh_source = s_mesh_source_make_default();
  }
  else
  {
    const LDKMeshSource* source = (const LDKMeshSource*)initial_value;
    LDKMeshSourceMaterialBinding* additional = NULL;
    u32 material_count = source->material_count ? source->material_count : 1;

    if (material_count > 1)
    {
      if (!source->additional_materials)
      {
        return false;
      }

      additional = (LDKMeshSourceMaterialBinding*)malloc(
          ((size_t)material_count - 1u) *
          sizeof(LDKMeshSourceMaterialBinding));
      if (!additional)
      {
        return false;
      }

      memcpy(additional, source->additional_materials,
          ((size_t)material_count - 1u) *
          sizeof(LDKMeshSourceMaterialBinding));
    }

    mesh_source->additional_materials = additional;
    mesh_source->material_count = material_count;

    if (!ldk_material_desc_is_valid(&mesh_source->material))
    {
      ldk_material_desc_defaults(
          LDK_MATERIAL_TYPE_VERTEX_COLOR, &mesh_source->material);
      mesh_source->material_asset = ldk_asset_material_null();
      mesh_source->material_revision = 0;
    }

    mesh_source->renderer_mesh = LDK_RESOURCE_MESH_INVALID;
    mesh_source->renderer_material = LDK_RESOURCE_MATERIAL_INVALID;
    mesh_source->renderer_texture = LDK_RESOURCE_TEXTURE_INVALID;
    mesh_source->renderer = NULL;
    mesh_source->dirty = true;
    mesh_source->material_dirty = true;

    for (u32 i = 1; i < material_count; i++)
    {
      LDKMeshSourceMaterialBinding* binding =
          &mesh_source->additional_materials[i - 1u];
      if (!ldk_material_desc_is_valid(&binding->material))
      {
        ldk_material_desc_defaults(
            LDK_MATERIAL_TYPE_VERTEX_COLOR, &binding->material);
        binding->material_asset = ldk_asset_material_null();
        binding->material_revision = 0;
      }
      binding->renderer_material = LDK_RESOURCE_MATERIAL_INVALID;
      binding->renderer_texture = LDK_RESOURCE_TEXTURE_INVALID;
      binding->material_dirty = true;
    }
  }

  ldk_entity_internal_flags_add(
      entity_registry, entity, LDK_ENTITY_INTERNAL_HAS_RENDERABLE);

  return true;
}

static void s_mesh_source_destroy(LDKEntityRegistry* entity_registry,
    LDKComponentRegistry* component_registry, LDKEntity entity,
    void* component, u32 component_index, void* user)
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

    for (u32 i = 1; i < mesh_source->material_count; i++)
    {
      s_material_binding_release(
          mesh_source, &mesh_source->additional_materials[i - 1u]);
    }

    ldk_renderer_mesh_destroy(
        mesh_source->renderer, mesh_source->renderer_mesh);
    mesh_source->renderer_texture = LDK_RESOURCE_TEXTURE_INVALID;
    mesh_source->renderer_material = LDK_RESOURCE_MATERIAL_INVALID;
    mesh_source->renderer_mesh = LDK_RESOURCE_MESH_INVALID;
    mesh_source->renderer = NULL;
  }

  if (mesh_source)
  {
    free(mesh_source->additional_materials);
    mesh_source->additional_materials = NULL;
    mesh_source->material_count = 0;
  }

  ldk_entity_internal_flags_remove(
      entity_registry, entity, LDK_ENTITY_INTERNAL_HAS_RENDERABLE);
}

bool ldk_mesh_source_set_data(LDKMeshSource* mesh_source, LDKAssetMesh asset)
{
  if (!mesh_source || x_handle_is_null(asset.h))
  {
    return false;
  }

  mesh_source->source_asset = asset;
  mesh_source->dirty = true;
  return true;
}

bool ldk_mesh_source_materials_sync(
    LDKMeshSource* mesh_source, LDKAssetManager* assets)
{
  const LDKAssetMeshData* data;
  u32 material_count;

  if (!mesh_source)
  {
    return false;
  }

  if (!assets || x_handle_is_null(mesh_source->source_asset.h))
  {
    return s_mesh_source_material_count_set(mesh_source, 1);
  }

  data = ldk_asset_manager_mesh_get_const(assets, mesh_source->source_asset);
  if (!data)
  {
    return false;
  }

  material_count = data->material_slot_count ? data->material_slot_count : 1;
  return s_mesh_source_material_count_set(mesh_source, material_count);
}

u32 ldk_mesh_source_material_count(const LDKMeshSource* mesh_source)
{
  return mesh_source ? mesh_source->material_count : 0;
}

LDKMeshSourceMaterialBinding* ldk_mesh_source_additional_material_binding(
    LDKMeshSource* mesh_source, u32 material_slot)
{
  if (!mesh_source || material_slot == 0 ||
      material_slot >= mesh_source->material_count ||
      !mesh_source->additional_materials)
  {
    return NULL;
  }

  return &mesh_source->additional_materials[material_slot - 1u];
}

const LDKMeshSourceMaterialBinding*
ldk_mesh_source_additional_material_binding_const(
    const LDKMeshSource* mesh_source, u32 material_slot)
{
  if (!mesh_source || material_slot == 0 ||
      material_slot >= mesh_source->material_count ||
      !mesh_source->additional_materials)
  {
    return NULL;
  }

  return &mesh_source->additional_materials[material_slot - 1u];
}

bool ldk_mesh_source_set_material_at(LDKMeshSource* mesh_source,
    u32 material_slot, LDKMaterialDesc const* material)
{
  if (!mesh_source || !ldk_material_desc_is_valid(material))
  {
    return false;
  }

  if (mesh_source->material_count == 0 &&
      !s_mesh_source_material_count_set(mesh_source, 1))
  {
    return false;
  }

  if (material_slot == 0)
  {
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

  LDKMeshSourceMaterialBinding* binding =
      ldk_mesh_source_additional_material_binding(mesh_source, material_slot);
  if (!binding)
  {
    return false;
  }

  binding->material_asset = ldk_asset_material_null();
  binding->material_revision = 0;
  if (ldk_material_desc_equal(&binding->material, material))
  {
    return true;
  }

  binding->material = *material;
  binding->material_dirty = true;
  return true;
}

bool ldk_mesh_source_set_material(
    LDKMeshSource* mesh_source, LDKMaterialDesc const* material)
{
  return ldk_mesh_source_set_material_at(mesh_source, 0, material);
}

bool ldk_mesh_source_set_material_asset_at(LDKMeshSource* mesh_source,
    LDKAssetManager* assets, u32 material_slot, LDKAssetMaterial asset)
{
  const LDKAssetMaterialData* data;

  if (!mesh_source || !assets)
  {
    return false;
  }

  if (mesh_source->material_count == 0 &&
      !s_mesh_source_material_count_set(mesh_source, 1))
  {
    return false;
  }

  data = ldk_asset_manager_material_get_const(assets, asset);
  if (!data)
  {
    return false;
  }

  if (material_slot == 0)
  {
    if (!ldk_material_desc_equal(&mesh_source->material, &data->descriptor))
    {
      mesh_source->material_dirty = true;
    }
    mesh_source->material = data->descriptor;
    mesh_source->material_asset = asset;
    mesh_source->material_revision = data->revision;
    return true;
  }

  LDKMeshSourceMaterialBinding* binding =
      ldk_mesh_source_additional_material_binding(mesh_source, material_slot);
  if (!binding)
  {
    return false;
  }

  if (!ldk_material_desc_equal(&binding->material, &data->descriptor))
  {
    binding->material_dirty = true;
  }
  binding->material = data->descriptor;
  binding->material_asset = asset;
  binding->material_revision = data->revision;
  return true;
}

bool ldk_mesh_source_set_material_asset(LDKMeshSource* mesh_source,
    LDKAssetManager* assets, LDKAssetMaterial asset)
{
  return ldk_mesh_source_set_material_asset_at(
      mesh_source, assets, 0, asset);
}

bool ldk_mesh_source_material_sync(LDKMeshSource* mesh_source,
    LDKAssetManager* assets)
{
  if (!mesh_source || !assets)
  {
    return false;
  }

  if (mesh_source->material_revision)
  {
    const LDKAssetMaterialData* data = ldk_asset_manager_material_get_const(
        assets, mesh_source->material_asset);
    if (!data)
    {
      return false;
    }
    if (data->revision != mesh_source->material_revision &&
        !ldk_mesh_source_set_material_asset(
            mesh_source, assets, mesh_source->material_asset))
    {
      return false;
    }
  }

  for (u32 i = 1; i < mesh_source->material_count; i++)
  {
    LDKMeshSourceMaterialBinding* binding =
        &mesh_source->additional_materials[i - 1u];
    if (!binding->material_revision)
    {
      continue;
    }

    const LDKAssetMaterialData* data = ldk_asset_manager_material_get_const(
        assets, binding->material_asset);
    if (!data)
    {
      return false;
    }
    if (data->revision != binding->material_revision &&
        !ldk_mesh_source_set_material_asset_at(
            mesh_source, assets, i, binding->material_asset))
    {
      return false;
    }
  }

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
