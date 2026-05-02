#include <component/ldk_mesh_source.h>
#include <module/ldk_asset_manager.h>
#include <module/ldk_entity.h>
#include <stdx/stdx_hpool.h>
#include <ldk.h>

static bool s_ldk_mesh_source_get_modules(
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

static bool s_ldk_mesh_source_get_ref(
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
      LDK_COMPONENT_TYPE_MESH_SOURCE,
      out_ref);
}

static LDKMeshSource* s_ldk_mesh_source_from_ref(
    LDKEntityRegistry* entity_registry,
    LDKComponentRegistry* component_registry,
    LDKComponentRef ref)
{
  return (LDKMeshSource*)ldk_component_ref_get(entity_registry, component_registry, ref);
}

static const LDKMeshSource* s_ldk_mesh_source_from_ref_const(
    LDKEntityRegistry* entity_registry,
    LDKComponentRegistry* component_registry,
    LDKComponentRef ref)
{
  return (const LDKMeshSource*)ldk_component_ref_get_const(entity_registry, component_registry, ref);
}

static LDKMeshSource* s_ldk_mesh_source_get_ptr(
    LDKEntityRegistry* entity_registry,
    LDKComponentRegistry* component_registry,
    LDKEntity entity)
{
  LDKComponentRef ref = {0};

  if (!s_ldk_mesh_source_get_ref(entity_registry, entity, &ref))
  {
    return NULL;
  }

  return s_ldk_mesh_source_from_ref(entity_registry, component_registry, ref);
}

static const LDKMeshSource* s_ldk_mesh_source_get_ptr_const(
    LDKEntityRegistry* entity_registry,
    LDKComponentRegistry* component_registry,
    LDKEntity entity)
{
  LDKComponentRef ref = {0};

  if (!s_ldk_mesh_source_get_ref(entity_registry, entity, &ref))
  {
    return NULL;
  }

  return s_ldk_mesh_source_from_ref_const(entity_registry, component_registry, ref);
}

static bool s_ldk_mesh_source_attach(
    LDKEntityRegistry* entity_registry,
    LDKComponentRegistry* component_registry,
    LDKEntity entity,
    void* component,
    u32 component_index,
    const void* initial_value,
    void* user)
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
    *mesh_source = ldk_mesh_source_make_default();
  }

  ldk_entity_add_internal_flags(
      entity_registry,
      entity,
      LDK_ENTITY_INTERNAL_HAS_RENDERABLE);

  return true;
}

static void s_ldk_mesh_source_destroy(
    LDKEntityRegistry* entity_registry,
    LDKComponentRegistry* component_registry,
    LDKEntity entity,
    void* component,
    u32 component_index,
    void* user)
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

LDKMeshSource ldk_mesh_source_make_default(void)
{
  LDKMeshSource mesh_source = {0};

  mesh_source.source_asset = ldk_asset_mesh_null();
  mesh_source.version = 1;
  mesh_source.renderer_mesh = LDK_RENDERER_MESH_INVALID;
  mesh_source.renderer_version = 0;
  return mesh_source;
}

bool ldk_mesh_source_register(LDKComponentRegistry* registry, u32 initial_capacity)
{
  if (!registry)
  {
    return false;
  }

  return ldk_component_register(
      registry,
      "MeshSource",
      LDK_COMPONENT_TYPE_MESH_SOURCE,
      sizeof(LDKMeshSource),
      initial_capacity,
      s_ldk_mesh_source_attach,
      s_ldk_mesh_source_destroy,
      NULL);
}

bool ldk_mesh_source_attach(LDKEntity entity, const LDKMeshSource* initial_value)
{
  LDKEntityRegistry* entity_registry = NULL;
  LDKComponentRegistry* component_registry = NULL;

  if (!s_ldk_mesh_source_get_modules(&entity_registry, &component_registry))
  {
    return false;
  }

  return ldk_entity_add_component(
      entity_registry,
      component_registry,
      entity,
      LDK_COMPONENT_TYPE_MESH_SOURCE,
      initial_value) != NULL;
}

bool ldk_mesh_source_detach(LDKEntity entity)
{
  LDKEntityRegistry* entity_registry = NULL;
  LDKComponentRegistry* component_registry = NULL;

  if (!s_ldk_mesh_source_get_modules(&entity_registry, &component_registry))
  {
    return false;
  }

  return ldk_component_remove_entity(
      component_registry,
      entity_registry,
      entity,
      LDK_COMPONENT_TYPE_MESH_SOURCE);
}

bool ldk_mesh_source_get(LDKEntity entity, LDKMeshSource* out_mesh_source)
{
  LDKEntityRegistry* entity_registry = NULL;
  LDKComponentRegistry* component_registry = NULL;
  const LDKMeshSource* mesh_source = NULL;

  if (!out_mesh_source)
  {
    return false;
  }

  if (!s_ldk_mesh_source_get_modules(&entity_registry, &component_registry))
  {
    return false;
  }

  mesh_source = s_ldk_mesh_source_get_ptr_const(entity_registry, component_registry, entity);
  if (!mesh_source)
  {
    return false;
  }

  *out_mesh_source = *mesh_source;
  return true;
}

bool ldk_mesh_source_set(LDKEntity entity, const LDKMeshSource* mesh_source)
{
  LDKEntityRegistry* entity_registry = NULL;
  LDKComponentRegistry* component_registry = NULL;
  LDKMeshSource* current = NULL;

  if (!mesh_source)
  {
    return false;
  }

  if (!s_ldk_mesh_source_get_modules(&entity_registry, &component_registry))
  {
    return false;
  }

  current = s_ldk_mesh_source_get_ptr(entity_registry, component_registry, entity);
  if (!current)
  {
    return false;
  }

  *current = *mesh_source;
  current->version++;
  current->renderer_mesh = LDK_RENDERER_MESH_INVALID;
  current->renderer_version = 0;
  return true;
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

bool ldk_mesh_source_set_asset(LDKEntity entity, LDKAssetMesh asset)
{
  LDKEntityRegistry* entity_registry = NULL;
  LDKComponentRegistry* component_registry = NULL;
  LDKMeshSource* mesh_source = NULL;

  if (x_handle_is_null(asset.h))
  {
    return false;
  }

  if (!s_ldk_mesh_source_get_modules(&entity_registry, &component_registry))
  {
    return false;
  }

  mesh_source = s_ldk_mesh_source_get_ptr(entity_registry, component_registry, entity);
  if (!mesh_source)
  {
    return false;
  }

  return ldk_mesh_source_set_data(mesh_source, asset);
}
