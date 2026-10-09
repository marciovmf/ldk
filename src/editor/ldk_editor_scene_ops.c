#include "ldk_editor_scene_ops.h"
#include "ldk_editor_internal.h"
#include "ldk_os.h"
#include <ldk_scene.h>
#include <ldk_game.h>
#include <component/ldk_instanced_mesh_source.h>
#include <component/ldk_mesh_source.h>
#include <component/ldk_transform.h>
#include <module/ldk_asset_manager.h>
#include <module/ldk_ecs.h>
#include <module/ldk_scene_manager.h>
#include <module/ldk_scenegraph.h>
#include <stdx/stdx_strbuilder.h>
#include <stdlib.h>
#include <string.h>

typedef struct LDKEditorSceneEntityList
{
  XArray *entities;
  bool ok;
} LDKEditorSceneEntityList;

static XFSPath s_editor_scene_runtree = {0};

typedef enum LDKEditorSceneApplyKind
{
  LDK_EDITOR_SCENE_APPLY_COMPONENT = 0,
  LDK_EDITOR_SCENE_APPLY_SYSTEM,
} LDKEditorSceneApplyKind;

typedef struct LDKEditorSceneApplyMaterial
{
  LDKMaterialDesc material;
  LDKAssetMaterial material_asset;
  u64 material_revision;
} LDKEditorSceneApplyMaterial;

typedef struct LDKEditorSceneApplyOverride
{
  LDKEditorSceneApplyKind kind;
  i32 scene_entity_id;
  u32 component_type;
  bool component_enabled;
  u64 system_id;
  void *data;
  u32 data_size;
  LDKEditorSceneApplyMaterial *materials;
  u32 material_count;
  Mat4 *instances;
  u32 instance_count;
} LDKEditorSceneApplyOverride;

typedef struct LDKEditorPlaySceneApplyState
{
  char *authoring_snapshot;
  LDKEditorSceneApplyOverride *authoring_systems;
  u32 authoring_system_count;
  LDKEntity *source_entities;
  u32 source_entity_count;
  LDKEditorSceneApplyOverride *overrides;
  u32 override_count;
  u32 override_capacity;
  bool active;
} LDKEditorPlaySceneApplyState;

typedef struct LDKEditorPlaySceneEntityCollector
{
  LDKEntityRegistry *registry;
  LDKEntity *entities;
  u32 count;
  u32 capacity;
  bool ok;
} LDKEditorPlaySceneEntityCollector;

static LDKEditorPlaySceneApplyState s_editor_play_scene = {0};

static bool s_editor_scene_entity_equal(LDKEntity left, LDKEntity right)
{
  return left.index == right.index && left.version == right.version;
}

static void s_editor_play_scene_override_payload_clear(
    LDKEditorSceneApplyOverride *override)
{
  if (!override)
  {
    return;
  }

  free(override->data);
  free(override->materials);
  free(override->instances);
  override->data = NULL;
  override->materials = NULL;
  override->instances = NULL;
  override->data_size = 0;
  override->material_count = 0;
  override->instance_count = 0;
}

static void s_editor_play_scene_apply_state_clear(void)
{
  for (u32 i = 0; i < s_editor_play_scene.authoring_system_count; ++i)
  {
    s_editor_play_scene_override_payload_clear(
        &s_editor_play_scene.authoring_systems[i]);
  }

  for (u32 i = 0; i < s_editor_play_scene.override_count; ++i)
  {
    s_editor_play_scene_override_payload_clear(
        &s_editor_play_scene.overrides[i]);
  }

  free(s_editor_play_scene.authoring_systems);
  free(s_editor_play_scene.overrides);
  free(s_editor_play_scene.source_entities);
  free(s_editor_play_scene.authoring_snapshot);
  memset(&s_editor_play_scene, 0, sizeof(s_editor_play_scene));
}

static bool s_editor_play_scene_authoring_snapshot_capture(
    LDKEditorContext *editor)
{
  LDKSceneResult result;
  XStrBuilder *builder;
  const char *source;
  char *snapshot;
  size_t size;

  if (!editor)
  {
    return false;
  }

  builder = x_strbuilder_create();
  if (!builder)
  {
    return false;
  }

  if (!ldk_scene_to_tml(builder, &result))
  {
    ldki_editor_log_error(editor, result.error);
    x_strbuilder_destroy(builder);
    return false;
  }

  source = x_strbuilder_to_string(builder);
  size = x_strbuilder_length(builder);
  snapshot = (char *)malloc(size + 1u);
  if (!snapshot)
  {
    x_strbuilder_destroy(builder);
    return false;
  }

  memcpy(snapshot, source, size);
  snapshot[size] = 0;
  x_strbuilder_destroy(builder);

  free(s_editor_play_scene.authoring_snapshot);
  s_editor_play_scene.authoring_snapshot = snapshot;
  return true;
}

static bool s_editor_play_scene_entity_collect(LDKEntity entity, void *user)
{
  LDKEditorPlaySceneEntityCollector *collector =
      (LDKEditorPlaySceneEntityCollector *)user;
  LDKEntity *entities;
  u32 capacity;

  if (!collector || !collector->ok || !collector->registry)
  {
    return false;
  }

  if (ldk_entity_internal_flags_has(
          collector->registry, entity, LDK_ENTITY_INTERNAL_EDITOR))
  {
    return true;
  }

  if (collector->count == collector->capacity)
  {
    capacity = collector->capacity ? collector->capacity * 2u : 64u;
    entities = (LDKEntity *)realloc(
        collector->entities, sizeof(*entities) * (size_t)capacity);
    if (!entities)
    {
      collector->ok = false;
      return false;
    }
    collector->entities = entities;
    collector->capacity = capacity;
  }

  collector->entities[collector->count++] = entity;
  return true;
}

static bool s_editor_play_scene_entities_collect(
    LDKEntity **out_entities, u32 *out_count)
{
  LDKEditorPlaySceneEntityCollector collector = {0};
  LDKECS *ecs;

  if (!out_entities || !out_count)
  {
    return false;
  }

  ecs = (LDKECS *)ldk_module_get(LDK_MODULE_ECS);
  if (!ecs)
  {
    return false;
  }

  collector.registry = &ecs->entity;
  collector.ok = true;
  if (!ldk_ecs_entity_foreach(
          s_editor_play_scene_entity_collect, &collector) ||
      !collector.ok)
  {
    free(collector.entities);
    return false;
  }

  *out_entities = collector.entities;
  *out_count = collector.count;
  return true;
}

static bool s_editor_play_scene_source_entity_id(
    LDKEntity entity, i32 *out_scene_id)
{
  if (!out_scene_id)
  {
    return false;
  }

  for (u32 i = 0; i < s_editor_play_scene.source_entity_count; ++i)
  {
    if (s_editor_scene_entity_equal(
            s_editor_play_scene.source_entities[i], entity))
    {
      *out_scene_id = (i32)i;
      return true;
    }
  }

  return false;
}

static bool s_editor_play_scene_field_size(
    const LDKComponentFieldMeta *field, u32 *out_size)
{
  if (!field || !out_size)
  {
    return false;
  }

  switch (field->type)
  {
  case LDK_FIELD_BOOL:
    *out_size = sizeof(bool);
    return true;
  case LDK_FIELD_ENUM:
    *out_size = field->enum_meta ? field->enum_meta->size : sizeof(i32);
    return true;
  case LDK_FIELD_I32:
    *out_size = sizeof(i32);
    return true;
  case LDK_FIELD_U32:
    *out_size = sizeof(u32);
    return true;
  case LDK_FIELD_FLOAT:
    *out_size = sizeof(float);
    return true;
  case LDK_FIELD_VEC2:
    *out_size = sizeof(Vec2);
    return true;
  case LDK_FIELD_VEC3:
    *out_size = sizeof(Vec3);
    return true;
  case LDK_FIELD_VEC4:
    *out_size = sizeof(Vec4);
    return true;
  case LDK_FIELD_QUAT:
    *out_size = sizeof(Quat);
    return true;
  case LDK_FIELD_MAT4:
    *out_size = sizeof(Mat4);
    return true;
  case LDK_FIELD_ENTITY:
    *out_size = sizeof(LDKEntity);
    return true;
  case LDK_FIELD_ASSET_MESH:
    *out_size = sizeof(LDKAssetMesh);
    return true;
  case LDK_FIELD_ASSET_FONT:
    *out_size = sizeof(LDKAssetFont);
    return true;
  case LDK_FIELD_ASSET_IMAGE:
    *out_size = sizeof(LDKAssetImage);
    return true;
  case LDK_FIELD_ASSET_MATERIAL:
    *out_size = sizeof(LDKAssetMaterial);
    return true;
  case LDK_FIELD_ASSET_SKYBOX:
    *out_size = sizeof(LDKAssetSkybox);
    return true;
  case LDK_FIELD_ASSET_AUDIO:
    *out_size = sizeof(LDKAssetAudio);
    return true;
  case LDK_FIELD_STRING:
    *out_size = sizeof(XSmallstr);
    return true;
  case LDK_FIELD_RESOURCE_MESH:
  default:
    return false;
  }
}

static bool s_editor_play_scene_system_field_is_serializable(
    const LDKComponentFieldMeta *field)
{
  return field && !(field->flags & LDK_FIELD_FLAG_RUNTIME) &&
         field->type != LDK_FIELD_RESOURCE_MESH;
}

static bool s_editor_play_scene_asset_reference_is_persistent(
    const LDKComponentFieldMeta *field, const void *data)
{
  const u8 *base;
  LDKAssetHandle asset;
  LDKAssetManager *assets;
  const LDKAssetInfo *info;
  LDKAssetType expected_type;

  if (!field || !data)
  {
    return false;
  }

  base = (const u8 *)data;
  switch (field->type)
  {
  case LDK_FIELD_ASSET_MESH:
    asset.h = ((const LDKAssetMesh *)(base + field->offset))->h;
    expected_type = LDK_ASSET_TYPE_MESH;
    break;
  case LDK_FIELD_ASSET_FONT:
    asset.h = ((const LDKAssetFont *)(base + field->offset))->h;
    expected_type = LDK_ASSET_TYPE_FONT;
    break;
  case LDK_FIELD_ASSET_IMAGE:
    asset.h = ((const LDKAssetImage *)(base + field->offset))->h;
    expected_type = LDK_ASSET_TYPE_IMAGE;
    break;
  case LDK_FIELD_ASSET_MATERIAL:
    asset.h = ((const LDKAssetMaterial *)(base + field->offset))->h;
    expected_type = LDK_ASSET_TYPE_MATERIAL;
    break;
  case LDK_FIELD_ASSET_SKYBOX:
    asset.h = ((const LDKAssetSkybox *)(base + field->offset))->h;
    expected_type = LDK_ASSET_TYPE_SKYBOX;
    break;
  case LDK_FIELD_ASSET_AUDIO:
    asset.h = ((const LDKAssetAudio *)(base + field->offset))->h;
    expected_type = LDK_ASSET_TYPE_AUDIO;
    break;
  default:
    return true;
  }

  if (x_handle_is_null(asset.h))
  {
    return true;
  }

  assets = ldk_module_get(LDK_MODULE_ASSET_MANAGER);
  if (!assets)
  {
    return false;
  }

  info = ldk_asset_get_info_const(assets, asset);
  if (!info || info->type != expected_type)
  {
    return false;
  }

  if (field->type == LDK_FIELD_ASSET_MESH)
  {
    LDKAssetMesh mesh = {asset.h};
    const LDKAssetMeshData *mesh_data =
        ldk_asset_manager_mesh_get_const(assets, mesh);
    if (mesh_data && (u32)mesh_data->primitive < LDK_MESH_PRIMITIVE_COUNT)
    {
      return true;
    }
  }

  if (!assets->source || info->source_revision != assets->source->revision ||
      info->asset_path.length == 0u ||
      info->asset_path.length > LDK_ASSET_PATH_MAX_LENGTH ||
      info->asset_path.buf[info->asset_path.length] != 0 ||
      strlen(info->asset_path.buf) != info->asset_path.length)
  {
    return false;
  }

  return true;
}

static bool s_editor_play_scene_entity_fields_validate(
    const LDKComponentMeta *meta, const void *data, bool system_fields)
{
  const u8 *base = (const u8 *)data;

  if (!meta || !data)
  {
    return false;
  }

  for (u32 i = 0; i < meta->field_count; ++i)
  {
    const LDKComponentFieldMeta *field = &meta->fields[i];
    bool serializable = system_fields
        ? s_editor_play_scene_system_field_is_serializable(field)
        : ldk_scene_component_field_is_serializable(meta, field);

    if (!serializable || field->type != LDK_FIELD_ENTITY)
    {
      continue;
    }

    if (field->offset > meta->size ||
        sizeof(LDKEntity) > meta->size - field->offset)
    {
      return false;
    }

    LDKEntity reference = *(const LDKEntity *)(base + field->offset);
    i32 scene_id;
    if (!x_handle_is_null(reference) &&
        !s_editor_play_scene_source_entity_id(reference, &scene_id))
    {
      return false;
    }
  }

  return true;
}

static LDKEditorSceneApplyOverride *s_editor_play_scene_override_get(
    LDKEditorSceneApplyKind kind, i32 scene_entity_id, u32 component_type,
    u64 system_id)
{
  LDKEditorSceneApplyOverride *overrides;
  u32 capacity;

  for (u32 i = 0; i < s_editor_play_scene.override_count; ++i)
  {
    LDKEditorSceneApplyOverride *override =
        &s_editor_play_scene.overrides[i];
    if (override->kind == kind &&
        override->scene_entity_id == scene_entity_id &&
        override->component_type == component_type &&
        override->system_id == system_id)
    {
      s_editor_play_scene_override_payload_clear(override);
      return override;
    }
  }

  if (s_editor_play_scene.override_count ==
      s_editor_play_scene.override_capacity)
  {
    capacity = s_editor_play_scene.override_capacity
        ? s_editor_play_scene.override_capacity * 2u
        : 8u;
    overrides = (LDKEditorSceneApplyOverride *)realloc(
        s_editor_play_scene.overrides,
        sizeof(*overrides) * (size_t)capacity);
    if (!overrides)
    {
      return NULL;
    }
    s_editor_play_scene.overrides = overrides;
    s_editor_play_scene.override_capacity = capacity;
  }

  LDKEditorSceneApplyOverride *override =
      &s_editor_play_scene.overrides[s_editor_play_scene.override_count++];
  memset(override, 0, sizeof(*override));
  override->kind = kind;
  override->scene_entity_id = scene_entity_id;
  override->component_type = component_type;
  override->system_id = system_id;
  return override;
}

static void s_editor_play_scene_override_remove(
    LDKEditorSceneApplyOverride *override)
{
  u32 index;
  u32 last_index;

  if (!override || !s_editor_play_scene.overrides ||
      s_editor_play_scene.override_count == 0u)
  {
    return;
  }

  index = (u32)(override - s_editor_play_scene.overrides);
  if (index >= s_editor_play_scene.override_count)
  {
    return;
  }

  s_editor_play_scene_override_payload_clear(override);
  last_index = --s_editor_play_scene.override_count;
  if (index != last_index)
  {
    s_editor_play_scene.overrides[index] =
        s_editor_play_scene.overrides[last_index];
  }
  memset(&s_editor_play_scene.overrides[last_index], 0,
      sizeof(s_editor_play_scene.overrides[last_index]));
}

static bool s_editor_play_scene_mesh_capture(
    LDKEditorSceneApplyOverride *override, const LDKMeshSource *mesh)
{
  u32 material_count;

  if (!override || !mesh)
  {
    return false;
  }

  material_count = mesh->material_count ? mesh->material_count : 1u;
  override->materials = (LDKEditorSceneApplyMaterial *)calloc(
      material_count, sizeof(*override->materials));
  if (!override->materials)
  {
    return false;
  }
  override->material_count = material_count;

  override->materials[0].material = mesh->material;
  override->materials[0].material_asset = mesh->material_asset;
  override->materials[0].material_revision = mesh->material_revision;

  for (u32 i = 1; i < material_count; ++i)
  {
    const LDKMeshSourceMaterialBinding *binding =
        ldk_mesh_source_additional_material_binding_const(mesh, i);
    if (!binding)
    {
      return false;
    }
    override->materials[i].material = binding->material;
    override->materials[i].material_asset = binding->material_asset;
    override->materials[i].material_revision = binding->material_revision;
  }

  return true;
}

static bool s_editor_play_scene_mesh_apply(
    const LDKEditorSceneApplyOverride *override, LDKMeshSource *mesh)
{
  LDKAssetManager *assets;

  if (!override || !mesh || !override->material_count ||
      !override->materials)
  {
    return true;
  }

  assets = ldk_module_get(LDK_MODULE_ASSET_MANAGER);
  if (!assets || !ldk_mesh_source_materials_sync(mesh, assets) ||
      ldk_mesh_source_material_count(mesh) != override->material_count)
  {
    return false;
  }

  for (u32 i = 0; i < override->material_count; ++i)
  {
    const LDKEditorSceneApplyMaterial *material = &override->materials[i];
    bool ok;

    if (material->material_revision != 0u &&
        !x_handle_is_null(material->material_asset.h))
    {
      ok = ldk_mesh_source_set_material_asset_at(
          mesh, assets, i, material->material_asset);
    }
    else
    {
      ok = ldk_mesh_source_set_material_at(mesh, i, &material->material);
    }

    if (!ok)
    {
      return false;
    }
  }

  mesh->dirty = true;
  mesh->material_dirty = true;
  return true;
}

static bool s_editor_play_scene_fields_apply(const LDKComponentMeta *meta,
    const void *source, void *target, bool system_fields,
    const LDKEntity *restored_entities, u32 restored_entity_count)
{
  const u8 *source_bytes = (const u8 *)source;
  u8 *target_bytes = (u8 *)target;

  if (!meta || !source || !target)
  {
    return false;
  }

  for (u32 i = 0; i < meta->field_count; ++i)
  {
    const LDKComponentFieldMeta *field = &meta->fields[i];
    bool serializable = system_fields
        ? s_editor_play_scene_system_field_is_serializable(field)
        : ldk_scene_component_field_is_serializable(meta, field);
    u32 field_size;

    if (!serializable)
    {
      continue;
    }

    if (!s_editor_play_scene_field_size(field, &field_size) ||
        field->offset > meta->size || field_size > meta->size - field->offset)
    {
      return false;
    }

    if (!s_editor_play_scene_asset_reference_is_persistent(field, source))
    {
      continue;
    }

    if (field->type == LDK_FIELD_ENTITY)
    {
      LDKEntity source_entity =
          *(const LDKEntity *)(source_bytes + field->offset);
      LDKEntity target_entity = x_handle_null();

      if (!x_handle_is_null(source_entity))
      {
        i32 scene_id;
        if (!s_editor_play_scene_source_entity_id(source_entity, &scene_id) ||
            scene_id < 0 || (u32)scene_id >= restored_entity_count)
        {
          return false;
        }
        target_entity = restored_entities[scene_id];
      }

      *(LDKEntity *)(target_bytes + field->offset) = target_entity;
      continue;
    }

    memcpy(target_bytes + field->offset, source_bytes + field->offset,
        field_size);
  }

  return true;
}

static const LDKSystemMeta *s_editor_play_scene_system_meta(
    LDKGame *game, u64 system_id)
{
  if (!game || !game->system_metadata_count || !game->system_metadata_get)
  {
    return NULL;
  }

  for (u32 i = 0; i < game->system_metadata_count(); ++i)
  {
    const LDKSystemMeta *meta = game->system_metadata_get(i);
    if (meta && meta->id == system_id)
    {
      return meta;
    }
  }

  return NULL;
}

static bool s_editor_play_scene_system_index(
    const LDKSceneSystems *systems, u64 system_id, u32 *out_index)
{
  if (!systems || !system_id)
  {
    return false;
  }

  for (u32 i = 0; i < systems->count; ++i)
  {
    if (systems->ids[i] == system_id)
    {
      if (out_index)
      {
        *out_index = i;
      }
      return true;
    }
  }

  return false;
}

static bool s_editor_play_scene_authoring_systems_capture(
    LDKEditorContext *editor)
{
  LDKEditorSceneApplyOverride *snapshots;
  LDKGame *game;
  u32 count;

  if (!editor)
  {
    return false;
  }

  count = editor->current_scene_systems.count;
  if (count == 0u)
  {
    return true;
  }

  snapshots = (LDKEditorSceneApplyOverride *)calloc(
      count, sizeof(*snapshots));
  if (!snapshots)
  {
    return false;
  }

  game = ldk_game_get();
  for (u32 i = 0; i < count; ++i)
  {
    LDKEditorSceneApplyOverride *snapshot = &snapshots[i];
    const LDKSystemMeta *meta;
    const void *data;
    u64 system_id = editor->current_scene_systems.ids[i];
    u32 data_size = editor->current_scene_systems.data_sizes
        ? editor->current_scene_systems.data_sizes[i]
        : 0u;

    snapshot->kind = LDK_EDITOR_SCENE_APPLY_SYSTEM;
    snapshot->scene_entity_id = -1;
    snapshot->system_id = system_id;


    meta = s_editor_play_scene_system_meta(game, system_id);
    if (!meta)
    {
      if (data_size != 0u)
      {
        goto failed;
      }
      continue;
    }

    if (data_size != meta->size)
    {
      goto failed;
    }

    snapshot->data_size = meta->size;
    if (meta->size == 0u)
    {
      continue;
    }

    data = ldk_scene_systems_data_get(
        &editor->current_scene_systems, system_id);
    if (!data)
    {
      goto failed;
    }

    LDKComponentMeta field_meta = {0};
    field_meta.name = meta->name;
    field_meta.size = meta->size;
    field_meta.fields = meta->fields;
    field_meta.field_count = meta->field_count;
    field_meta.groups = meta->groups;
    field_meta.group_count = meta->group_count;
    if (!s_editor_play_scene_entity_fields_validate(
            &field_meta, data, true))
    {
      goto failed;
    }

    snapshot->data = malloc(meta->size);
    if (!snapshot->data)
    {
      goto failed;
    }
    memcpy(snapshot->data, data, meta->size);
  }

  s_editor_play_scene.authoring_systems = snapshots;
  s_editor_play_scene.authoring_system_count = count;
  return true;

failed:
  for (u32 i = 0; i < count; ++i)
  {
    s_editor_play_scene_override_payload_clear(&snapshots[i]);
  }
  free(snapshots);
  return false;
}

static bool s_editor_play_scene_component_override_apply(
    LDKEditorContext *editor, const LDKEditorSceneApplyOverride *override,
    const LDKEntity *restored_entities, u32 restored_entity_count)
{
  LDKGame *game;
  const LDKComponentMeta *meta;
  LDKEntity entity;
  void *component;
  void *fields_target;

  if (!editor || !override || override->scene_entity_id < 0 ||
      (u32)override->scene_entity_id >= restored_entity_count)
  {
    return false;
  }

  game = ldk_game_get();
  entity = restored_entities[override->scene_entity_id];
  component = ldk_ecs_component_get(entity, override->component_type);
  if (!component)
  {
    component = ldk_ecs_component_add(entity, override->component_type, NULL);
    if (!component)
    {
      return false;
    }
  }

  if (!ldk_ecs_component_enabled_set(
          entity, override->component_type, override->component_enabled))
  {
    return false;
  }
  if (override->component_type == LDK_COMPONENT_TYPE_INSTANCED_MESH_SOURCE)
  {
    LDKInstancedMeshSource *instances = (LDKInstancedMeshSource *)component;
    meta = ldk_scene_component_meta_find_by_type(
        game, LDK_COMPONENT_TYPE_MESH_SOURCE);
    fields_target = &instances->source;
  }
  else
  {
    meta = ldk_scene_component_meta_find_by_type(
        game, override->component_type);
    fields_target = component;
  }

  if (!meta || meta->size != override->data_size ||
      (meta->size && !override->data) ||
      !s_editor_play_scene_fields_apply(meta, override->data, fields_target,
          false, restored_entities, restored_entity_count))
  {
    return false;
  }

  if (override->component_type == LDK_COMPONENT_TYPE_MESH_SOURCE)
  {
    if (!s_editor_play_scene_mesh_apply(
            override, (LDKMeshSource *)component))
    {
      return false;
    }
  }
  else if (override->component_type ==
           LDK_COMPONENT_TYPE_INSTANCED_MESH_SOURCE)
  {
    LDKInstancedMeshSource *instances = (LDKInstancedMeshSource *)component;
    if (!s_editor_play_scene_mesh_apply(override, &instances->source) ||
        !ldk_instanced_mesh_source_set_instances(
            instances, override->instances, override->instance_count))
    {
      return false;
    }
  }

  if (override->component_type == LDK_COMPONENT_TYPE_TRANSFORM)
  {
    if (!ldk_transform_mark_dirty(entity) ||
        !ldk_scenegraph_update_entity(entity))
    {
      return false;
    }
  }

  return true;
}

static bool s_editor_play_scene_system_override_apply(
    LDKEditorContext *editor, const LDKEditorSceneApplyOverride *override,
    const LDKEntity *restored_entities, u32 restored_entity_count)
{
  LDKGame *game;
  const LDKSystemMeta *meta;
  u32 system_index;
  void *target;
  LDKComponentMeta field_meta = {0};

  if (!editor || !override)
  {
    return false;
  }

  if (!s_editor_play_scene_system_index(
          &editor->current_scene_systems, override->system_id, &system_index))
  {
    if (!ldk_scene_systems_add(&editor->current_scene_systems,
            override->system_id) ||
        !s_editor_play_scene_system_index(&editor->current_scene_systems,
            override->system_id, &system_index))
    {
      return false;
    }
  }


  game = ldk_game_get();
  meta = s_editor_play_scene_system_meta(game, override->system_id);
  if (!meta)
  {
    return false;
  }

  if (meta->size == 0u)
  {
    return override->data_size == 0u;
  }

  if (meta->size != override->data_size || !override->data ||
      !editor->current_scene_systems.data_sizes ||
      editor->current_scene_systems.data_sizes[system_index] != meta->size)
  {
    return false;
  }

  target = ldk_scene_systems_data_get(
      &editor->current_scene_systems, override->system_id);
  if (!target)
  {
    return false;
  }

  field_meta.name = meta->name;
  field_meta.size = meta->size;
  field_meta.fields = meta->fields;
  field_meta.field_count = meta->field_count;
  field_meta.groups = meta->groups;
  field_meta.group_count = meta->group_count;
  return s_editor_play_scene_fields_apply(&field_meta, override->data, target,
      true, restored_entities, restored_entity_count);
}

static void s_editor_scene_selection_clear(LDKEditorContext *editor)
{
  if (!editor)
  {
    return;
  }

  editor->selected_entity = x_handle_null();
  editor->selected_system_id = 0;
  if (editor->hierarchy_expanded_entities != NULL)
  {
    x_array_clear(editor->hierarchy_expanded_entities);
  }
}

static bool s_editor_scene_entity_collect(LDKEntity entity, void *user)
{
  LDKEditorSceneEntityList *list = (LDKEditorSceneEntityList *)user;

  if (!list || !list->ok || !list->entities)
  {
    return false;
  }

  if (x_array_add(list->entities, &entity) != XARRAY_OK)
  {
    list->ok = false;
    return false;
  }

  return true;
}

static bool s_editor_scene_ecs_clear(void)
{
  LDKEditorSceneEntityList list = {0};

  list.entities = x_array_create(sizeof(LDKEntity), 64);
  list.ok = list.entities != NULL;

  if (!list.ok)
  {
    return false;
  }

  if (!ldk_ecs_entity_foreach(s_editor_scene_entity_collect, &list) || !list.ok)
  {
    x_array_destroy(list.entities);
    return false;
  }

  for (u32 i = 0; i < x_array_count(list.entities); ++i)
  {
    LDKEntity *entity = x_array_get(list.entities, i);
    if (entity != NULL)
    {
      ldk_ecs_entity_destroy(*entity);
    }
  }

  x_array_destroy(list.entities);

  /* The editor reloads a scene into the same ECS registries. Group
   * definitions remain registered, but membership must start empty. */
  if (!ldk_ecs_grouping_runtime_reset())
  {
    return false;
  }

  return true;
}


bool ldki_editor_scene_play_apply_begin(LDKEditorContext *editor)
{
  LDKEntity *entities = NULL;
  u32 entity_count = 0;

  s_editor_play_scene_apply_state_clear();

  if (!editor || !editor->project.loaded ||
      editor->editor_state != LDK_EDITOR_STATE_STOPED)
  {
    return false;
  }

  /* Apply to Scene targets the scene that was open in the editor when Play
   * began. Catalog play loads a different scene and has no authoring target in
   * the editor to copy values back into. */
  if (!editor->project.play_current_scene)
  {
    return true;
  }

  if (editor->current_scene_path.length == 0 ||
      !s_editor_play_scene_entities_collect(&entities, &entity_count))
  {
    free(entities);
    s_editor_play_scene_apply_state_clear();
    return false;
  }

  s_editor_play_scene.source_entities = entities;
  s_editor_play_scene.source_entity_count = entity_count;
  if (!s_editor_play_scene_authoring_snapshot_capture(editor) ||
      !s_editor_play_scene_authoring_systems_capture(editor))
  {
    s_editor_play_scene_apply_state_clear();
    return false;
  }

  s_editor_play_scene.active = true;
  return true;
}

void ldki_editor_scene_play_apply_discard(LDKEditorContext *editor)
{
  (void)editor;
  s_editor_play_scene_apply_state_clear();
}

bool ldki_editor_scene_play_apply_available(const LDKEditorContext *editor)
{
  return editor && s_editor_play_scene.active;
}

bool ldki_editor_scene_play_component_can_apply(
    const LDKEditorContext *editor, LDKEntity entity)
{
  i32 scene_id;

  return ldki_editor_scene_play_apply_available(editor) &&
         s_editor_play_scene_source_entity_id(entity, &scene_id);
}

bool ldki_editor_scene_play_apply_component(
    LDKEditorContext *editor, LDKEntity entity, u32 component_type)
{
  LDKGame *game;
  const LDKComponentMeta *meta;
  LDKComponentDesc desc = {0};
  LDKEditorSceneApplyOverride *override;
  void *component;
  const void *fields_source;
  i32 scene_id;

  if (!ldki_editor_scene_play_component_can_apply(editor, entity) ||
      !s_editor_play_scene_source_entity_id(entity, &scene_id))
  {
    return false;
  }

  game = ldk_game_get();
  component = ldk_ecs_component_get(entity, component_type);
  if (!game || !component)
  {
    return false;
  }

  if (component_type == LDK_COMPONENT_TYPE_INSTANCED_MESH_SOURCE)
  {
    const LDKInstancedMeshSource *instances =
        (const LDKInstancedMeshSource *)component;
    meta = ldk_scene_component_meta_find_by_type(
        game, LDK_COMPONENT_TYPE_MESH_SOURCE);
    fields_source = &instances->source;
  }
  else
  {
    meta = ldk_scene_component_meta_find_by_type(game, component_type);
    fields_source = component;
    LDKECS *ecs = ldk_module_get(LDK_MODULE_ECS);
    if (!ecs || !ldk_component_desc_get(
                    &ecs->component, component_type, &desc) ||
        !meta || desc.entry_size < meta->size)
    {
      return false;
    }
  }

  if (!meta || !fields_source ||
      !s_editor_play_scene_entity_fields_validate(meta, fields_source, false))
  {
    return false;
  }

  override = s_editor_play_scene_override_get(
      LDK_EDITOR_SCENE_APPLY_COMPONENT, scene_id, component_type, 0);
  if (!override)
  {
    return false;
  }

  override->component_enabled =
      ldk_ecs_component_is_enabled(entity, component_type);
  override->data_size = meta->size;
  if (meta->size)
  {
    override->data = malloc(meta->size);
    if (!override->data)
    {
      s_editor_play_scene_override_remove(override);
      return false;
    }
    memcpy(override->data, fields_source, meta->size);
  }

  if (component_type == LDK_COMPONENT_TYPE_MESH_SOURCE)
  {
    if (!s_editor_play_scene_mesh_capture(
            override, (const LDKMeshSource *)component))
    {
      s_editor_play_scene_override_remove(override);
      return false;
    }
  }
  else if (component_type == LDK_COMPONENT_TYPE_INSTANCED_MESH_SOURCE)
  {
    const LDKInstancedMeshSource *instances =
        (const LDKInstancedMeshSource *)component;
    if (!s_editor_play_scene_mesh_capture(override, &instances->source))
    {
      s_editor_play_scene_override_remove(override);
      return false;
    }

    override->instance_count = instances->instance_count;
    if (override->instance_count)
    {
      if (!instances->instances)
      {
        s_editor_play_scene_override_remove(override);
        return false;
      }
      override->instances = (Mat4 *)malloc(
          sizeof(*override->instances) * (size_t)override->instance_count);
      if (!override->instances)
      {
        s_editor_play_scene_override_remove(override);
        return false;
      }
      memcpy(override->instances, instances->instances,
          sizeof(*override->instances) * (size_t)override->instance_count);
    }
  }

  ldki_editor_log_info(editor, "Component changes applied to scene.");
  return true;
}

bool ldki_editor_scene_play_apply_system(
    LDKEditorContext *editor, u64 system_id)
{
  LDKGame *game;
  const LDKSystemMeta *meta;
  LDKEditorSceneApplyOverride *override;
  u32 system_index;
  const void *data = NULL;
  LDKComponentMeta field_meta = {0};

  if (!ldki_editor_scene_play_apply_available(editor) || !system_id ||
      !s_editor_play_scene_system_index(
          &editor->current_scene_systems, system_id, &system_index))
  {
    return false;
  }

  game = ldk_game_get();
  meta = s_editor_play_scene_system_meta(game, system_id);
  if (!meta)
  {
    return false;
  }

  if (meta->size)
  {
    if (!editor->current_scene_systems.data_sizes ||
        editor->current_scene_systems.data_sizes[system_index] != meta->size)
    {
      return false;
    }
    data = ldk_scene_systems_data_get(
        &editor->current_scene_systems, system_id);
    if (!data)
    {
      return false;
    }

    field_meta.name = meta->name;
    field_meta.size = meta->size;
    field_meta.fields = meta->fields;
    field_meta.field_count = meta->field_count;
    field_meta.groups = meta->groups;
    field_meta.group_count = meta->group_count;
    if (!s_editor_play_scene_entity_fields_validate(
            &field_meta, data, true))
    {
      return false;
    }
  }

  override = s_editor_play_scene_override_get(
      LDK_EDITOR_SCENE_APPLY_SYSTEM, -1, 0, system_id);
  if (!override)
  {
    return false;
  }


  override->data_size = meta->size;
  if (meta->size)
  {
    override->data = malloc(meta->size);
    if (!override->data)
    {
      s_editor_play_scene_override_remove(override);
      return false;
    }
    memcpy(override->data, data, meta->size);
  }

  ldki_editor_log_info(editor, "System changes applied to scene.");
  return true;
}

static bool s_editor_play_scene_authoring_snapshot_restore(
    LDKEditorContext *editor)
{
  LDKSceneResult result;

  if (!editor || !s_editor_play_scene.authoring_snapshot ||
      editor->current_scene_path.length == 0)
  {
    return false;
  }

  if (!s_editor_scene_ecs_clear())
  {
    return false;
  }

  if (!ldk_scene_from_tml(
          s_editor_play_scene.authoring_snapshot, &result))
  {
    s_editor_scene_ecs_clear();
    ldki_editor_log_error(editor, result.error);
    return false;
  }

  return true;
}

static bool s_editor_play_scene_authoring_system_contains(u64 system_id)
{
  for (u32 i = 0; i < s_editor_play_scene.authoring_system_count; ++i)
  {
    if (s_editor_play_scene.authoring_systems[i].system_id == system_id)
    {
      return true;
    }
  }
  return false;
}

static bool s_editor_play_scene_authoring_systems_restore(
    LDKEditorContext *editor, const LDKEntity *restored_entities,
    u32 restored_entity_count)
{
  if (!editor)
  {
    return false;
  }

  for (u32 i = editor->current_scene_systems.count; i > 0; --i)
  {
    u64 system_id = editor->current_scene_systems.ids[i - 1u];
    if (!s_editor_play_scene_authoring_system_contains(system_id) &&
        !ldk_scene_systems_remove(&editor->current_scene_systems, system_id))
    {
      return false;
    }
  }

  for (u32 i = 0; i < s_editor_play_scene.authoring_system_count; ++i)
  {
    const LDKEditorSceneApplyOverride *snapshot =
        &s_editor_play_scene.authoring_systems[i];
    const LDKSystemMeta *meta = s_editor_play_scene_system_meta(
        ldk_game_get(), snapshot->system_id);

    if (!meta)
    {
      if (snapshot->data_size != 0u)
      {
        return false;
      }
      if (!s_editor_play_scene_system_index(
              &editor->current_scene_systems, snapshot->system_id, NULL))
      {
        if (!ldk_scene_systems_add(
                &editor->current_scene_systems, snapshot->system_id))
        {
          return false;
        }
      }

      continue;
    }

    if (!s_editor_play_scene_system_override_apply(editor, snapshot,
            restored_entities, restored_entity_count))
    {
      return false;
    }
  }

  return true;
}

bool ldki_editor_scene_play_apply_restore(LDKEditorContext *editor)
{
  LDKEntity *restored_entities = NULL;
  u32 restored_entity_count = 0;
  bool ok = true;

  if (!editor || !s_editor_play_scene.active ||
      editor->editor_state != LDK_EDITOR_STATE_STOPED ||
      ldk_game_instance_is_started() || ldk_game_instance_is_updating())
  {
    return false;
  }

  if (!s_editor_play_scene_authoring_snapshot_restore(editor))
  {
    ok = false;
    goto cleanup;
  }

  if (!s_editor_play_scene_entities_collect(
          &restored_entities, &restored_entity_count) ||
      restored_entity_count !=
          s_editor_play_scene.source_entity_count)
  {
    ok = false;
    goto cleanup;
  }

  if (!s_editor_play_scene_authoring_systems_restore(
          editor, restored_entities, restored_entity_count))
  {
    ldki_editor_log_error(
        editor, "Failed to restore authoring system state after Play.");
    ok = false;
    goto cleanup;
  }

  for (u32 i = 0; i < s_editor_play_scene.override_count; ++i)
  {
    const LDKEditorSceneApplyOverride *override =
        &s_editor_play_scene.overrides[i];
    bool applied = override->kind == LDK_EDITOR_SCENE_APPLY_COMPONENT
        ? s_editor_play_scene_component_override_apply(editor, override,
              restored_entities, restored_entity_count)
        : s_editor_play_scene_system_override_apply(editor, override,
              restored_entities, restored_entity_count);
    if (!applied)
    {
      ldki_editor_log_error(
          editor, "Failed to restore an Apply to Scene change.");
      ok = false;
    }
  }

cleanup:
  free(restored_entities);
  s_editor_play_scene_apply_state_clear();
  return ok;
}

void ldki_editor_scene_state_sync(LDKEditorContext *editor)
{
  XFSPath runtree = {0};

  if (!editor)
  {
    return;
  }

  if (!editor->project.loaded)
  {
    s_editor_play_scene_apply_state_clear();
    memset(&editor->current_scene_path, 0, sizeof(editor->current_scene_path));
    memset(&s_editor_scene_runtree, 0, sizeof(s_editor_scene_runtree));
    ldk_scene_systems_clear(&editor->current_scene_systems);
    return;
  }

  x_fs_path_set(&runtree, editor->project.run_root_path.buf);
  x_fs_path_normalize(&runtree);

  if (s_editor_scene_runtree.length == 0 ||
      x_fs_path_compare(&s_editor_scene_runtree, &runtree) != 0)
  {
    s_editor_play_scene_apply_state_clear();
    s_editor_scene_runtree = runtree;
    memset(&editor->current_scene_path, 0, sizeof(editor->current_scene_path));
    ldk_scene_systems_clear(&editor->current_scene_systems);
    s_editor_scene_selection_clear(editor);
  }
}

bool ldki_editor_scene_path_is_scene(const XFSPath *path)
{
  const char *text;
  size_t length;
  const char *extension = ".scene";
  size_t extension_length = strlen(extension);

  if (!path)
  {
    return false;
  }

  text = x_fs_path_cstr(path);
  if (!text)
  {
    return false;
  }

  length = strlen(text);
  return length >= extension_length &&
         strcmp(text + length - extension_length, extension) == 0;
}

static bool s_editor_scene_path_relative(
    LDKEditorContext *editor, const XFSPath *path, XFSPath *out_relative)
{
  XFSPath runtree = {0};
  XFSPath normalized = {0};
  const char *relative;

  if (!editor || !editor->project.loaded || !path || !out_relative)
  {
    return false;
  }

  x_fs_path_set(&runtree, editor->project.run_root_path.buf);
  x_fs_path_normalize(&runtree);
  normalized = *path;
  x_fs_path_normalize(&normalized);

  memset(out_relative, 0, sizeof(*out_relative));
  if (!x_fs_path_common_prefix(
          x_fs_path_cstr(&runtree), x_fs_path_cstr(&normalized), out_relative))
  {
    return false;
  }

  relative = x_fs_path_cstr(out_relative);
  if (!relative || relative[0] == 0 || strcmp(relative, ".") == 0 ||
      x_fs_path_is_absolute(out_relative))
  {
    memset(out_relative, 0, sizeof(*out_relative));
    return false;
  }

  return true;
}

static bool s_editor_scene_full_path(
    LDKEditorContext *editor, const XFSPath *relative, XFSPath *out_path)
{
  if (!editor || !editor->project.loaded || !relative || !out_path ||
      relative->length == 0 || x_fs_path_is_absolute(relative))
  {
    return false;
  }

  x_fs_path(
      out_path, editor->project.run_root_path.buf, x_fs_path_cstr(relative));
  x_fs_path_normalize(out_path);
  return true;
}

bool ldki_editor_scene_clear(LDKEditorContext *editor)
{
  if (!editor || editor->editor_state != LDK_EDITOR_STATE_STOPED ||
      ldk_game_instance_is_started() || ldk_game_instance_is_updating())
  {
    return false;
  }

  if (!s_editor_scene_ecs_clear())
  {
    ldki_editor_log_error(editor, "Failed to clear the current scene.");
    return false;
  }

  LDKSceneManager *manager = ldk_module_get(LDK_MODULE_SCENE_MANAGER);
  if (!manager || !ldk_scene_manager_current_reset(manager))
  {
    ldki_editor_log_error(editor, "Failed to reset Scene Manager state.");
    return false;
  }

  ldk_scene_systems_clear(&editor->current_scene_systems);
  s_editor_scene_selection_clear(editor);
  memset(&editor->current_scene_path, 0, sizeof(editor->current_scene_path));
  return true;
}

bool ldki_editor_scene_load(LDKEditorContext *editor, const XFSPath *path)
{
  LDKSceneResult result;
  LDKSceneSystems systems = {0};
  XFSPath relative = {0};

  ldki_editor_scene_state_sync(editor);

  if (!editor || editor->editor_state != LDK_EDITOR_STATE_STOPED ||
      !ldki_editor_scene_path_is_scene(path) ||
      !s_editor_scene_path_relative(editor, path, &relative))
  {
    return false;
  }

  /* Validate the association list before destroying the open scene. */
  if (!ldk_scene_systems_load_tml_file(x_fs_path_cstr(path), &systems, &result))
  {
    ldki_editor_log_error(editor, result.error);
    return false;
  }

  if (!ldki_editor_scene_clear(editor))
  {
    ldk_scene_systems_clear(&systems);
    return false;
  }

  if (!ldk_scene_load_tml_file_with_systems(
          x_fs_path_cstr(path), &systems, &result))
  {
    s_editor_scene_ecs_clear();
    ldk_scene_systems_clear(&systems);
    ldki_editor_log_error(editor, result.error);
    return false;
  }

  editor->current_scene_systems = systems;
  editor->current_scene_path = relative;
  ldki_editor_log_info(editor, "Scene loaded.");
  return true;
}

bool ldki_editor_scene_save(LDKEditorContext *editor)
{
  LDKSceneResult result;
  XFSPath path = {0};

  ldki_editor_scene_state_sync(editor);

  if (!editor || editor->editor_state != LDK_EDITOR_STATE_STOPED ||
      editor->current_scene_path.length == 0 ||
      !s_editor_scene_full_path(editor, &editor->current_scene_path, &path))
  {
    return false;
  }

  if (!ldk_scene_systems_save_tml_file(x_fs_path_cstr(&path),
          &editor->current_scene_systems, &result))
  {
    ldki_editor_log_error(editor, result.error);
    return false;
  }

  ldki_editor_log_info(editor, "Scene saved.");
  return true;
}

bool ldki_editor_scene_new_at_path(
    LDKEditorContext *editor, const XFSPath *path)
{
  LDKSceneResult result;
  XFSPath normalized = {0};
  XFSPath relative = {0};

  ldki_editor_scene_state_sync(editor);

  if (!editor || !path || !editor->project.loaded ||
      editor->editor_state != LDK_EDITOR_STATE_STOPED)
  {
    return false;
  }

  normalized = *path;
  x_fs_path_normalize(&normalized);
  x_fs_path_change_extension(&normalized, ".scene");

  if (!s_editor_scene_path_relative(editor, &normalized, &relative))
  {
    ldk_os_dialog_show_error(editor->window, "Invalid scene path",
        "Scene files must be saved inside the project runtree.");
    return false;
  }

  if (!ldki_editor_scene_clear(editor))
  {
    return false;
  }

  if (!ldk_scene_systems_save_tml_file(x_fs_path_cstr(&normalized),
          &editor->current_scene_systems, &result))
  {
    ldki_editor_log_error(editor, result.error);
    return false;
  }

  editor->current_scene_path = relative;
  ldki_editor_log_info(editor, "Scene created.");
  return true;
}

bool ldki_editor_scene_new(LDKEditorContext *editor)
{
  XFSPath path = {0};

  if (!editor || !editor->project.loaded ||
      editor->editor_state != LDK_EDITOR_STATE_STOPED)
  {
    return false;
  }

  if (!ldk_os_dialog_show_save_file(
          editor->window, "New Scene", "*.scene", &path))
  {
    return false;
  }

  return ldki_editor_scene_new_at_path(editor, &path);
}

bool ldki_editor_scene_add_primitive(
    LDKEditorContext *editor, LDKMeshPrimitive primitive, const char *name)
{
  LDKAssetManager *asset_manager;
  LDKAssetMesh asset;
  LDKEntity entity;
  LDKMeshSource *mesh_source;

  ldki_editor_scene_state_sync(editor);

  if (!editor || !editor->project.loaded ||
      editor->editor_state != LDK_EDITOR_STATE_STOPED ||
      editor->current_scene_path.length == 0)
  {
    return false;
  }

  asset_manager = ldk_module_get(LDK_MODULE_ASSET_MANAGER);
  if (!asset_manager)
  {
    return false;
  }

  asset = ldk_mesh_primitive_asset_get(asset_manager, primitive);
  if (x_handle_is_null(asset.h))
  {
    return false;
  }

  entity = ldk_ecs_entity_create();
  if (x_handle_is_null(entity))
  {
    return false;
  }

  if (name && name[0] != 0)
  {
    ldk_ecs_entity_name_set(entity, name);
  }

  mesh_source = (LDKMeshSource *)ldk_ecs_component_add(
      entity, LDK_COMPONENT_TYPE_MESH_SOURCE, NULL);
  if (!mesh_source || !ldk_mesh_source_set_data(mesh_source, asset))
  {
    ldk_ecs_entity_destroy(entity);
    return false;
  }

  editor->selected_entity = entity;
  editor->selected_system_id = 0;
  return true;
}

