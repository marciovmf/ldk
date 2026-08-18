#include <ldk_scene.h>
#include <ldk.h>
#include <ldk_game.h>
#include <ldk_mesh.h>

#include <component/ldk_camera.h>
#include <component/ldk_mesh_source.h>
#include <component/ldk_transform.h>

#include <module/ldk_asset_manager.h>
#include <module/ldk_component.h>
#include <module/ldk_ecs.h>
#include <module/ldk_entity.h>

#include <stdx/stdx_hpool.h>
#include <stdx/stdx_tml.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct LDKSceneEntityMapEntry
{
  LDKEntity entity;
  i32 scene_id;
} LDKSceneEntityMapEntry;

typedef struct LDKSceneEntityMap
{
  LDKSceneEntityMapEntry *entries;
  u32 count;
  u32 capacity;
} LDKSceneEntityMap;

typedef struct LDKScenePendingParent
{
  LDKEntity entity;
  i32 parent_id;
} LDKScenePendingParent;

typedef struct LDKScenePendingParentList
{
  LDKScenePendingParent *entries;
  u32 count;
  u32 capacity;
} LDKScenePendingParentList;

#ifdef LDK_EDITOR
typedef struct LDKSceneSaveContext
{
  LDKGame *game;
  LDKSceneResult *result;
  LDKSceneEntityMap map;
  XStrBuilder *out;
  bool ok;
} LDKSceneSaveContext;

static bool s_scene_entity_is_editor_only(LDKEntity entity)
{
  LDKEntityRegistry *registry = ldk_ecs_entity_registry_get();

  return registry != NULL && ldk_entity_internal_flags_has(
      registry, entity, LDK_ENTITY_INTERNAL_EDITOR);
}
#endif

void ldk_scene_result_clear(LDKSceneResult *result)
{
  if (!result)
  {
    return;
  }

  result->ok = true;
  result->error[0] = 0;
}

void ldk_scene_result_set_error(
    LDKSceneResult *result, const char *error)
{
  if (!result)
  {
    return;
  }

  result->ok = false;

  if (!error)
  {
    result->error[0] = 0;
    return;
  }

  snprintf(result->error, sizeof(result->error), "%s", error);
}

u32 ldk_scene_component_meta_runtime_type(const LDKComponentMeta *meta)
{
  if (!meta)
  {
    return 0u;
  }

  /*
   * Comet-generated metadata can use hashed component ids, while the ECS uses
   * the built-in component ids below. Normalize known built-ins by name.
   */
  if (meta->name)
  {
    if (strcmp(meta->name, "LDKTransform") == 0)
    {
      return LDK_COMPONENT_TYPE_TRANSFORM;
    }

    if (strcmp(meta->name, "LDKCamera") == 0)
    {
      return LDK_COMPONENT_TYPE_CAMERA;
    }

    if (strcmp(meta->name, "LDKMeshSource") == 0)
    {
      return LDK_COMPONENT_TYPE_MESH_SOURCE;
    }
  }

  return meta->type;
}

const LDKComponentMeta *ldk_scene_component_meta_find_by_type(
    LDKGame *game, u32 component_type)
{
  u32 i;
  u32 count;

  if (!game || !game->metadata_count || !game->metadata_get)
  {
    return NULL;
  }

  count = game->metadata_count();

  for (i = 0; i < count; i++)
  {
    const LDKComponentMeta *meta = game->metadata_get(i);

    if (ldk_scene_component_meta_runtime_type(meta) == component_type)
    {
      return meta;
    }
  }

  return NULL;
}

const LDKComponentFieldMeta *ldk_scene_component_field_find(
    const LDKComponentMeta *meta, const char *field_name)
{
  u32 i;

  if (!meta || !field_name)
  {
    return NULL;
  }

  for (i = 0; i < meta->field_count; i++)
  {
    const LDKComponentFieldMeta *field = &meta->fields[i];

    if (field->name && strcmp(field->name, field_name) == 0)
    {
      return field;
    }
  }

  return NULL;
}

bool ldk_scene_component_field_is_serializable(
    const LDKComponentMeta *meta, const LDKComponentFieldMeta *field)
{
  if (!meta || !field)
  {
    return false;
  }

  if (field->flags & LDK_FIELD_FLAG_RUNTIME)
  {
    return false;
  }

  /*
   * Transform hierarchy/cache fields are runtime graph state. Parenting is
   * serialized as the scene-level "parent:" field instead.
   */
  if (meta->name && strcmp(meta->name, "LDKTransform") == 0)
  {
    if (strcmp(field->name, "parent") == 0 ||
        strcmp(field->name, "first_child") == 0 ||
        strcmp(field->name, "next_sibling") == 0 ||
        strcmp(field->name, "prev_sibling") == 0 ||
        strcmp(field->name, "world_matrix") == 0 ||
        strcmp(field->name, "flags") == 0)
    {
      return false;
    }
  }

  if (meta->name && strcmp(meta->name, "LDKMeshSource") == 0)
  {
    if (strcmp(field->name, "renderer_mesh") == 0 ||
        strcmp(field->name, "dirty") == 0)
    {
      return false;
    }
  }

  if (field->type == LDK_FIELD_RESOURCE_MESH)
  {
    return false;
  }

  return true;
}

static bool s_map_reserve(LDKSceneEntityMap *map, u32 min_capacity)
{
  LDKSceneEntityMapEntry *entries;
  u32 capacity;

  if (!map)
  {
    return false;
  }

  if (map->capacity >= min_capacity)
  {
    return true;
  }

  capacity = map->capacity ? map->capacity * 2u : 64u;
  while (capacity < min_capacity)
  {
    capacity *= 2u;
  }

  entries = (LDKSceneEntityMapEntry *)realloc(
      map->entries, sizeof(LDKSceneEntityMapEntry) * (size_t)capacity);

  if (!entries)
  {
    return false;
  }

  map->entries = entries;
  map->capacity = capacity;
  return true;
}

static void s_map_free(LDKSceneEntityMap *map)
{
  if (!map)
  {
    return;
  }

  free(map->entries);
  memset(map, 0, sizeof(*map));
}

static bool s_map_push(
    LDKSceneEntityMap *map, LDKEntity entity, i32 scene_id)
{
  if (!s_map_reserve(map, map->count + 1u))
  {
    return false;
  }

  map->entries[map->count].entity = entity;
  map->entries[map->count].scene_id = scene_id;
  map->count += 1u;
  return true;
}

static bool s_map_find_entity_by_id(
    const LDKSceneEntityMap *map, i32 scene_id, LDKEntity *out_entity)
{
  u32 i;

  if (!map || !out_entity)
  {
    return false;
  }

  for (i = 0; i < map->count; i++)
  {
    if (map->entries[i].scene_id == scene_id)
    {
      *out_entity = map->entries[i].entity;
      return true;
    }
  }

  return false;
}

static bool s_pending_parent_reserve(
    LDKScenePendingParentList *list, u32 min_capacity)
{
  LDKScenePendingParent *entries;
  u32 capacity;

  if (!list)
  {
    return false;
  }

  if (list->capacity >= min_capacity)
  {
    return true;
  }

  capacity = list->capacity ? list->capacity * 2u : 64u;
  while (capacity < min_capacity)
  {
    capacity *= 2u;
  }

  entries = (LDKScenePendingParent *)realloc(
      list->entries, sizeof(LDKScenePendingParent) * (size_t)capacity);

  if (!entries)
  {
    return false;
  }

  list->entries = entries;
  list->capacity = capacity;
  return true;
}

static bool s_pending_parent_push(
    LDKScenePendingParentList *list, LDKEntity entity, i32 parent_id)
{
  if (!s_pending_parent_reserve(list, list->count + 1u))
  {
    return false;
  }

  list->entries[list->count].entity = entity;
  list->entries[list->count].parent_id = parent_id;
  list->count += 1u;
  return true;
}

static void s_pending_parent_free(LDKScenePendingParentList *list)
{
  if (!list)
  {
    return;
  }

  free(list->entries);
  memset(list, 0, sizeof(*list));
}

static void s_result_error(LDKSceneResult *result, const char *error)
{
  ldk_scene_result_set_error(result, error);
}

static bool s_mesh_primitive_from_asset_reference(
    const char *reference, LDKMeshPrimitive *out_primitive)
{
  static const char *references[LDK_MESH_PRIMITIVE_COUNT] =
  {
    "builtin:mesh/cube",
    "builtin:mesh/cone",
    "builtin:mesh/sphere",
    "builtin:mesh/capsule",
    "builtin:mesh/plane",
    "builtin:mesh/quad",
  };
  u32 i;

  if (!reference)
  {
    return false;
  }

  for (i = 0; i < LDK_MESH_PRIMITIVE_COUNT; i++)
  {
    if (strcmp(reference, references[i]) != 0)
    {
      continue;
    }

    if (out_primitive)
    {
      *out_primitive = (LDKMeshPrimitive)i;
    }

    return true;
  }

  return false;
}

static const TMLNode *s_node_find_child(
    const TMLDocument *doc, const TMLNode *node, const char *name)
{
  return tml_node_find_child(doc, node, name);
}

static const TMLEntry *s_node_find_entry(
    const TMLDocument *doc, const TMLNode *node, const char *name)
{
  return tml_node_find_entry(doc, node, name);
}

static bool s_entry_get_i32(const TMLEntry *entry, i32 *out_value)
{
  i64 value;

  if (!entry || !out_value)
  {
    return false;
  }

  if (!tml_entry_get_i64(entry, &value))
  {
    return false;
  }

  *out_value = (i32)value;
  return true;
}

static bool s_node_get_i32(const TMLDocument *doc, const TMLNode *node,
    const char *name, i32 *out_value)
{
  const TMLEntry *entry = s_node_find_entry(doc, node, name);
  return s_entry_get_i32(entry, out_value);
}

static bool s_node_get_u32(const TMLDocument *doc, const TMLNode *node,
    const char *name, u32 *out_value)
{
  const TMLEntry *entry;
  i64 value;

  if (!out_value)
  {
    return false;
  }

  entry = s_node_find_entry(doc, node, name);
  if (!entry || !tml_entry_get_i64(entry, &value) || value < 0)
  {
    return false;
  }

  *out_value = (u32)value;
  return true;
}

static bool s_read_f32_array(const TMLDocument *doc,
    const TMLEntry *entry, float *out_values, u32 expected_count)
{
  u32 i;

  if (!entry || !out_values)
  {
    return false;
  }

  if (entry->type == TML_VALUE_ARRAY_F64)
  {
    TMLF64Slice slice;

    if (!tml_entry_get_f64_array(doc, entry, &slice) ||
        slice.count != expected_count)
    {
      return false;
    }

    for (i = 0; i < expected_count; i++)
    {
      out_values[i] = (float)slice.data[i];
    }

    return true;
  }

  if (entry->type == TML_VALUE_ARRAY_I64)
  {
    TMLI64Slice slice;

    if (!tml_entry_get_i64_array(doc, entry, &slice) ||
        slice.count != expected_count)
    {
      return false;
    }

    for (i = 0; i < expected_count; i++)
    {
      out_values[i] = (float)slice.data[i];
    }

    return true;
  }

  return false;
}

static bool s_read_scene_entity_reference(
    const LDKSceneEntityMap *map, const TMLEntry *entry,
    LDKEntity *out_entity)
{
  i32 scene_id;

  if (!entry || !out_entity || !s_entry_get_i32(entry, &scene_id))
  {
    return false;
  }

  if (scene_id == LDK_SCENE_NULL_ENTITY_ID)
  {
    *out_entity = x_handle_null();
    return true;
  }

  if (scene_id < 0)
  {
    return false;
  }

  return s_map_find_entity_by_id(map, scene_id, out_entity);
}

static bool s_apply_field_value(const TMLDocument *doc,
    const TMLEntry *entry, const LDKSceneEntityMap *entity_map,
    const LDKComponentFieldMeta *field, void *component)
{
  u8 *base;
  void *ptr;

  if (!entry || !field || !component)
  {
    return false;
  }

  base = (u8 *)component;
  ptr = base + field->offset;

  switch (field->type)
  {
  case LDK_FIELD_BOOL:
  {
    u8 value;

    if (!tml_entry_get_bool(entry, &value))
    {
      return false;
    }

    *(bool *)ptr = value != 0u;
  }
  break;

  case LDK_FIELD_I32:
  case LDK_FIELD_ENUM:
  {
    i64 value;

    if (!tml_entry_get_i64(entry, &value))
    {
      return false;
    }

    *(i32 *)ptr = (i32)value;
  }
  break;

  case LDK_FIELD_U32:
  {
    i64 value;

    if (!tml_entry_get_i64(entry, &value) || value < 0)
    {
      return false;
    }

    *(u32 *)ptr = (u32)value;
  }
  break;

  case LDK_FIELD_FLOAT:
  {
    f64 value;

    if (entry->type == TML_VALUE_F64)
    {
      if (!tml_entry_get_f64(entry, &value))
      {
        return false;
      }
    }
    else if (entry->type == TML_VALUE_I64)
    {
      i64 integer;

      if (!tml_entry_get_i64(entry, &integer))
      {
        return false;
      }

      value = (f64)integer;
    }
    else
    {
      return false;
    }

    *(float *)ptr = (float)value;
  }
  break;

  case LDK_FIELD_VEC2:
  {
    Vec2 *value = (Vec2 *)ptr;
    float data[2];

    if (!s_read_f32_array(doc, entry, data, 2u))
    {
      return false;
    }

    value->x = data[0];
    value->y = data[1];
  }
  break;

  case LDK_FIELD_VEC3:
  {
    Vec3 *value = (Vec3 *)ptr;
    float data[3];

    if (!s_read_f32_array(doc, entry, data, 3u))
    {
      return false;
    }

    value->x = data[0];
    value->y = data[1];
    value->z = data[2];
  }
  break;

  case LDK_FIELD_VEC4:
  {
    Vec4 *value = (Vec4 *)ptr;
    float data[4];

    if (!s_read_f32_array(doc, entry, data, 4u))
    {
      return false;
    }

    value->x = data[0];
    value->y = data[1];
    value->z = data[2];
    value->w = data[3];
  }
  break;

  case LDK_FIELD_QUAT:
  {
    Quat *value = (Quat *)ptr;
    float data[4];

    if (!s_read_f32_array(doc, entry, data, 4u))
    {
      return false;
    }

    value->x = data[0];
    value->y = data[1];
    value->z = data[2];
    value->w = data[3];
  }
  break;

  case LDK_FIELD_MAT4:
  {
    Mat4 *value = (Mat4 *)ptr;

    if (!s_read_f32_array(doc, entry, value->m, 16u))
    {
      return false;
    }
  }
  break;

  case LDK_FIELD_ENTITY:
  {
    LDKEntity value;

    if (!s_read_scene_entity_reference(entity_map, entry, &value))
    {
      return false;
    }

    *(LDKEntity *)ptr = value;
  }
  break;

  case LDK_FIELD_ASSET_MESH:
  {
    if (entry->type == TML_VALUE_I64)
    {
      i32 asset_id;

      if (!s_entry_get_i32(entry, &asset_id) || asset_id != -1)
      {
        return false;
      }

      *(LDKAssetMesh *)ptr = ldk_asset_mesh_null();
      break;
    }

    if (entry->type == TML_VALUE_STRING)
    {
      TMLString reference;
      LDKMeshPrimitive primitive;
      LDKAssetManager *asset_manager;
      LDKAssetMesh asset;

      if (!tml_entry_get_string(entry, &reference) ||
          !s_mesh_primitive_from_asset_reference(
              reference.data, &primitive))
      {
        return false;
      }

      asset_manager = (LDKAssetManager *)ldk_module_get(
          LDK_MODULE_ASSET_MANAGER);
      if (!asset_manager)
      {
        return false;
      }

      asset = ldk_mesh_primitive_asset_get(asset_manager, primitive);
      if (x_handle_is_null(asset.h))
      {
        return false;
      }

      *(LDKAssetMesh *)ptr = asset;
      break;
    }

    return false;
  }

  case LDK_FIELD_RESOURCE_MESH:
    return false;

  default:
    return false;
  }

  return true;
}

static bool s_read_entity_headers(const TMLDocument *doc,
    const TMLNode *entities_node, LDKSceneEntityMap *map,
    LDKScenePendingParentList *parents, LDKSceneResult *result)
{
  u32 i;

  for (i = 0; i < entities_node->child_count; i++)
  {
    const TMLNode *entity_node =
        tml_node_child_at(doc, entities_node, i);
    i32 scene_id = LDK_SCENE_NULL_ENTITY_ID;
    i32 parent_id = LDK_SCENE_NULL_ENTITY_ID;
    LDKEntity entity = x_handle_null();

    if (!entity_node)
    {
      continue;
    }

    if (!s_node_get_i32(doc, entity_node, "entity", &scene_id) ||
        scene_id < 0)
    {
      s_result_error(
          result, "entity is missing a valid non-negative scene id");
      return false;
    }

    if (s_map_find_entity_by_id(map, scene_id, &entity))
    {
      s_result_error(result, "duplicate entity scene id");
      return false;
    }

    entity = ldk_ecs_entity_create();
    if (x_handle_is_null(entity))
    {
      s_result_error(result, "failed to create runtime entity");
      return false;
    }

#if defined(_DEBUG) || defined(LDK_EDITOR)
    {
      TMLString name;

      if (tml_node_get_string(doc, entity_node, "name", &name))
      {
        char buffer[LDK_ENTITY_NAME_MAX_LEN];
        u32 copy_size = name.size;

        if (copy_size >= LDK_ENTITY_NAME_MAX_LEN)
        {
          copy_size = LDK_ENTITY_NAME_MAX_LEN - 1u;
        }

        memcpy(buffer, name.data, copy_size);
        buffer[copy_size] = 0;
        ldk_ecs_entity_name_set(entity, buffer);
      }
    }
#endif

    if (!s_map_push(map, entity, scene_id))
    {
      s_result_error(result, "failed to allocate scene entity map");
      return false;
    }

    if (s_node_get_i32(doc, entity_node, "parent", &parent_id))
    {
      if (!s_pending_parent_push(parents, entity, parent_id))
      {
        s_result_error(result, "failed to allocate pending parent list");
        return false;
      }
    }
  }

  return true;
}

static bool s_apply_component_fields(const TMLDocument *doc,
    const TMLNode *fields_node, const LDKSceneEntityMap *map,
    const LDKComponentMeta *meta, void *component, LDKSceneResult *result)
{
  u32 i;

  if (!fields_node)
  {
    return true;
  }

  for (i = 0; i < fields_node->entry_count; i++)
  {
    const TMLEntry *entry = tml_node_entry_at(doc, fields_node, i);
    const LDKComponentFieldMeta *field;
    char field_name[128];

    if (!entry)
    {
      continue;
    }

    if (entry->name.size >= sizeof(field_name))
    {
      s_result_error(result, "component field name is too long");
      return false;
    }

    memcpy(field_name, entry->name.data, entry->name.size);
    field_name[entry->name.size] = 0;

    field = ldk_scene_component_field_find(meta, field_name);
    if (!field)
    {
      /* Tolerate metadata differences between scene and runtime. */
      continue;
    }

    if (!ldk_scene_component_field_is_serializable(meta, field))
    {
      continue;
    }

    if (!s_apply_field_value(doc, entry, map, field, component))
    {
      s_result_error(result, "failed to parse component field");
      return false;
    }
  }

  return true;
}

static bool s_apply_entity_components(const TMLDocument *doc,
    const TMLNode *entities_node, LDKGame *game,
    const LDKSceneEntityMap *map, LDKSceneResult *result)
{
  u32 entity_index;

  for (entity_index = 0; entity_index < entities_node->child_count;
      entity_index++)
  {
    const TMLNode *entity_node =
        tml_node_child_at(doc, entities_node, entity_index);
    const TMLNode *components_node;
    LDKEntity entity = x_handle_null();
    i32 scene_id = LDK_SCENE_NULL_ENTITY_ID;
    u32 component_index;

    if (!entity_node)
    {
      continue;
    }

    if (!s_node_get_i32(doc, entity_node, "entity", &scene_id))
    {
      s_result_error(result, "entity is missing scene id");
      return false;
    }

    if (!s_map_find_entity_by_id(map, scene_id, &entity))
    {
      s_result_error(result, "entity id was not created");
      return false;
    }

    components_node = s_node_find_child(doc, entity_node, "components");
    if (!components_node)
    {
      continue;
    }

    for (component_index = 0;
        component_index < components_node->child_count; component_index++)
    {
      const TMLNode *component_node =
          tml_node_child_at(doc, components_node, component_index);
      const TMLNode *fields_node;
      const LDKComponentMeta *meta;
      u32 component_type;
      void *component;

      if (!component_node)
      {
        continue;
      }

      if (!s_node_get_u32(doc, component_node, "type", &component_type))
      {
        s_result_error(result, "component is missing type");
        return false;
      }

      meta = ldk_scene_component_meta_find_by_type(game, component_type);
      if (!meta)
      {
        /* Tolerate component types unknown to this runtime. */
        continue;
      }

      if (component_type == LDK_COMPONENT_TYPE_TRANSFORM)
      {
        component = ldk_ecs_component_get(entity, component_type);
      }
      else
      {
        component = ldk_ecs_component_add(entity, component_type, NULL);
      }

      if (!component)
      {
        s_result_error(result, "failed to create component");
        return false;
      }

      fields_node = s_node_find_child(doc, component_node, "fields");
      if (!s_apply_component_fields(
              doc, fields_node, map, meta, component, result))
      {
        return false;
      }
    }
  }

  return true;
}

static bool s_resolve_parent_links(const LDKSceneEntityMap *map,
    const LDKScenePendingParentList *parents, LDKSceneResult *result)
{
  u32 i;

  for (i = 0; i < parents->count; i++)
  {
    LDKEntity entity = parents->entries[i].entity;
    i32 parent_id = parents->entries[i].parent_id;
    LDKEntity parent = x_handle_null();

    if (parent_id == LDK_SCENE_NULL_ENTITY_ID)
    {
      continue;
    }

    if (parent_id < 0)
    {
      s_result_error(result, "invalid negative parent id");
      return false;
    }

    if (!s_map_find_entity_by_id(map, parent_id, &parent))
    {
      s_result_error(result, "parent id does not resolve to an entity");
      return false;
    }

    if (!ldk_transform_set_parent(entity, parent))
    {
      s_result_error(result, "failed to set entity parent");
      return false;
    }
  }

  return true;
}

bool ldk_scene_from_tml(
    const char *source, LDKSceneResult *result)
{
  LDKGame *game;
  TMLParseResult parse;
  TMLDocument *doc;
  const TMLNode *scene_node;
  const TMLNode *entities_node;
  LDKSceneEntityMap map;
  LDKScenePendingParentList parents;
  bool ok;

  if (result)
  {
    ldk_scene_result_clear(result);
  }

  if (!source)
  {
    s_result_error(result, "invalid scene load arguments");
    return false;
  }

  game = ldk_game_get();
  if (!game || !game->metadata_count || !game->metadata_get)
  {
    s_result_error(result, "game component metadata is not available");
    return false;
  }

  memset(&map, 0, sizeof(map));
  memset(&parents, 0, sizeof(parents));
  doc = NULL;
  ok = false;

  parse = tml_parse(source);
  if (!parse.ok)
  {
    if (result)
    {
      result->ok = false;
      snprintf(result->error, sizeof(result->error),
          "TML parse error at %u:%u: %s", parse.line, parse.column,
          parse.error);
    }

    return false;
  }

  doc = parse.document;
  scene_node = tml_path_find_node(doc, "scene");
  if (!scene_node)
  {
    s_result_error(result, "missing scene root node");
    goto cleanup;
  }

  entities_node = s_node_find_child(doc, scene_node, "entities");
  if (!entities_node)
  {
    s_result_error(result, "missing scene.entities node");
    goto cleanup;
  }

  if (!s_read_entity_headers(doc, entities_node, &map, &parents, result))
  {
    goto cleanup;
  }

  if (!s_apply_entity_components(doc, entities_node, game, &map, result))
  {
    goto cleanup;
  }

  if (!s_resolve_parent_links(&map, &parents, result))
  {
    goto cleanup;
  }

  ok = true;

cleanup:
  s_map_free(&map);
  s_pending_parent_free(&parents);
  tml_document_free(doc);
  return ok;
}

static bool s_file_read_text(const char *path, char **out_text)
{
  FILE *file;
  long size;
  char *text;
  size_t read_size;

  if (!path || !out_text)
  {
    return false;
  }

  *out_text = NULL;

  file = fopen(path, "rb");
  if (!file)
  {
    return false;
  }

  if (fseek(file, 0, SEEK_END) != 0)
  {
    fclose(file);
    return false;
  }

  size = ftell(file);
  if (size < 0 || fseek(file, 0, SEEK_SET) != 0)
  {
    fclose(file);
    return false;
  }

  text = (char *)malloc((size_t)size + 1u);
  if (!text)
  {
    fclose(file);
    return false;
  }

  read_size = fread(text, 1u, (size_t)size, file);
  fclose(file);

  text[read_size] = 0;
  *out_text = text;
  return true;
}

bool ldk_scene_load_tml_file(
    const char *path, LDKSceneResult *result)
{
  char *text;
  bool ok;

  if (result)
  {
    ldk_scene_result_clear(result);
  }

  if (!path)
  {
    s_result_error(result, "invalid scene load path");
    return false;
  }

  if (!s_file_read_text(path, &text))
  {
    s_result_error(result, "failed to read scene TML file");
    return false;
  }

  ok = ldk_scene_from_tml(text, result);
  free(text);
  return ok;
}

#ifdef LDK_EDITOR

static bool s_entity_equal(LDKEntity left, LDKEntity right)
{
  return left.index == right.index && left.version == right.version;
}

static bool s_map_find_id_by_entity(
    const LDKSceneEntityMap *map, LDKEntity entity, i32 *out_scene_id)
{
  u32 i;

  if (!map || !out_scene_id)
  {
    return false;
  }

  for (i = 0; i < map->count; i++)
  {
    if (s_entity_equal(map->entries[i].entity, entity))
    {
      *out_scene_id = map->entries[i].scene_id;
      return true;
    }
  }

  return false;
}

static void s_result_error_format(LDKSceneResult *result,
    const char *format, const char *first, const char *second)
{
  if (!result)
  {
    return;
  }

  result->ok = false;
  snprintf(result->error, sizeof(result->error), format,
      first ? first : "", second ? second : "");
}

static void s_append_indent(XStrBuilder *out, u32 indent)
{
  u32 i;

  for (i = 0; i < indent; i++)
  {
    x_strbuilder_append(out, "  ");
  }
}

static void s_append_escaped_string(XStrBuilder *out, const char *text)
{
  const char *cursor = text;

  x_strbuilder_append_char(out, '"');

  if (cursor)
  {
    while (*cursor)
    {
      switch (*cursor)
      {
      case '\n':
        x_strbuilder_append(out, "\\n");
        break;

      case '\r':
        x_strbuilder_append(out, "\\r");
        break;

      case '\t':
        x_strbuilder_append(out, "\\t");
        break;

      case '"':
        x_strbuilder_append(out, "\\\"");
        break;

      case '\\':
        x_strbuilder_append(out, "\\\\");
        break;

      default:
        x_strbuilder_append_char(out, *cursor);
        break;
      }

      cursor++;
    }
  }

  x_strbuilder_append_char(out, '"');
}

static bool s_write_scene_entity_reference(XStrBuilder *out,
    const LDKSceneEntityMap *map, LDKEntity entity)
{
  i32 scene_id;

  if (x_handle_is_null(entity))
  {
    x_strbuilder_append_format(out, "%d", LDK_SCENE_NULL_ENTITY_ID);
    return true;
  }

  if (!s_map_find_id_by_entity(map, entity, &scene_id))
  {
    return false;
  }

  x_strbuilder_append_format(out, "%d", scene_id);
  return true;
}

static bool s_write_field_value(XStrBuilder *out,
    const LDKSceneEntityMap *entity_map,
    const LDKComponentFieldMeta *field, const void *component)
{
  const u8 *base;
  const void *ptr;

  if (!out || !field || !component)
  {
    return false;
  }

  base = (const u8 *)component;
  ptr = base + field->offset;

  switch (field->type)
  {
  case LDK_FIELD_BOOL:
  {
    const bool *value = (const bool *)ptr;
    x_strbuilder_append(out, *value ? "true" : "false");
  }
  break;

  case LDK_FIELD_I32:
  case LDK_FIELD_ENUM:
  {
    const i32 *value = (const i32 *)ptr;
    x_strbuilder_append_format(out, "%d", *value);
  }
  break;

  case LDK_FIELD_U32:
  {
    const u32 *value = (const u32 *)ptr;
    x_strbuilder_append_format(out, "%u", *value);
  }
  break;

  case LDK_FIELD_FLOAT:
  {
    const float *value = (const float *)ptr;
    x_strbuilder_append_format(out, "%#.9g", (double)*value);
  }
  break;

  case LDK_FIELD_VEC2:
  {
    const Vec2 *value = (const Vec2 *)ptr;
    x_strbuilder_append_format(
        out, "%#.9g, %#.9g", (double)value->x, (double)value->y);
  }
  break;

  case LDK_FIELD_VEC3:
  {
    const Vec3 *value = (const Vec3 *)ptr;
    x_strbuilder_append_format(out, "%#.9g, %#.9g, %#.9g", (double)value->x,
        (double)value->y, (double)value->z);
  }
  break;

  case LDK_FIELD_VEC4:
  {
    const Vec4 *value = (const Vec4 *)ptr;
    x_strbuilder_append_format(out, "%#.9g, %#.9g, %#.9g, %#.9g",
        (double)value->x, (double)value->y, (double)value->z,
        (double)value->w);
  }
  break;

  case LDK_FIELD_QUAT:
  {
    const Quat *value = (const Quat *)ptr;
    x_strbuilder_append_format(out, "%#.9g, %#.9g, %#.9g, %#.9g",
        (double)value->x, (double)value->y, (double)value->z,
        (double)value->w);
  }
  break;

  case LDK_FIELD_MAT4:
  {
    const Mat4 *value = (const Mat4 *)ptr;
    u32 i;

    for (i = 0; i < 16u; i++)
    {
      if (i > 0u)
      {
        x_strbuilder_append(out, ", ");
      }

      x_strbuilder_append_format(out, "%#.9g", (double)value->m[i]);
    }
  }
  break;

  case LDK_FIELD_ENTITY:
  {
    const LDKEntity *value = (const LDKEntity *)ptr;
    return s_write_scene_entity_reference(out, entity_map, *value);
  }

  case LDK_FIELD_ASSET_MESH:
  {
    const LDKAssetMesh *value = (const LDKAssetMesh *)ptr;
    LDKAssetManager *asset_manager;
    const LDKAssetInfo *info;
    LDKAssetHandle generic;
    const char *reference;

    if (x_handle_is_null(value->h))
    {
      x_strbuilder_append_format(out, "%d", -1);
      break;
    }

    asset_manager = (LDKAssetManager *)ldk_module_get(
        LDK_MODULE_ASSET_MANAGER);
    if (!asset_manager)
    {
      return false;
    }

    generic.h = value->h;
    info = ldk_asset_get_info_const(asset_manager, generic);
    if (!info || info->type != LDK_ASSET_TYPE_MESH)
    {
      return false;
    }

    reference = x_fs_path_cstr(&info->asset_path);
    if (!s_mesh_primitive_from_asset_reference(reference, NULL))
    {
      return false;
    }

    s_append_escaped_string(out, reference);
  }
  break;

  case LDK_FIELD_RESOURCE_MESH:
    return false;

  default:
    return false;
  }

  return true;
}

static bool s_collect_entity_id_callback(LDKEntity entity, void *user)
{
  LDKSceneSaveContext *context = (LDKSceneSaveContext *)user;
  i32 scene_id;

  if (!context || !context->ok)
  {
    return false;
  }

  if (s_scene_entity_is_editor_only(entity))
  {
    return true;
  }

  scene_id = (i32)context->map.count;

  if (!s_map_push(&context->map, entity, scene_id))
  {
    s_result_error(context->result, "failed to allocate scene entity map");
    context->ok = false;
    return false;
  }

  return true;
}

static bool s_write_component(LDKSceneSaveContext *context,
    LDKEntity entity, u32 component_type, bool *out_wrote_any)
{
  const LDKComponentMeta *meta;
  const void *component;
  u32 i;
  bool wrote_any_field;

  meta = ldk_scene_component_meta_find_by_type(
      context->game, component_type);

  if (!meta)
  {
    /* No metadata means the component's binary layout is unknown. */
    return true;
  }

  component = ldk_ecs_component_get_const(entity, component_type);
  if (!component)
  {
    return false;
  }

  s_append_indent(context->out, 4u);
  x_strbuilder_append_format(context->out, "- type: %u # %s\n",
      component_type, meta->name ? meta->name : "component");

  s_append_indent(context->out, 5u);
  x_strbuilder_append(context->out, "fields:\n");

  if (out_wrote_any)
  {
    *out_wrote_any = true;
  }

  wrote_any_field = false;

  for (i = 0; i < meta->field_count; i++)
  {
    const LDKComponentFieldMeta *field = &meta->fields[i];

    if (!ldk_scene_component_field_is_serializable(meta, field))
    {
      continue;
    }

    s_append_indent(context->out, 6u);
    x_strbuilder_append_format(context->out, "%s: ", field->name);

    if (!s_write_field_value(
            context->out, &context->map, field, component))
    {
      s_result_error_format(context->result,
          "failed to serialize component field: %s.%s", meta->name,
          field->name);
      return false;
    }

    x_strbuilder_append_char(context->out, '\n');
    wrote_any_field = true;
  }

  if (!wrote_any_field)
  {
    s_append_indent(context->out, 6u);
    x_strbuilder_append(context->out, "# no serializable fields\n");
  }

  return true;
}

static bool s_write_entity_callback(LDKEntity entity, void *user)
{
  LDKSceneSaveContext *context = (LDKSceneSaveContext *)user;
  i32 scene_id;
  LDKEntity parent;
  i32 parent_id;
  u32 component_count;
  u32 i;
  bool wrote_any_component;

  if (!context || !context->ok)
  {
    return false;
  }

  if (s_scene_entity_is_editor_only(entity))
  {
    return true;
  }

  if (!s_map_find_id_by_entity(&context->map, entity, &scene_id))
  {
    s_result_error(context->result, "failed to find serialized entity id");
    context->ok = false;
    return false;
  }

  s_append_indent(context->out, 2u);
  x_strbuilder_append_format(context->out, "- entity: %d\n", scene_id);

  parent = ldk_transform_get_parent(entity);
  parent_id = LDK_SCENE_NULL_ENTITY_ID;

  if (!x_handle_is_null(parent) &&
      !s_map_find_id_by_entity(&context->map, parent, &parent_id))
  {
    s_result_error(
        context->result, "entity parent was not found in scene map");
    context->ok = false;
    return false;
  }

  s_append_indent(context->out, 3u);
  x_strbuilder_append_format(context->out, "parent: %d\n", parent_id);

  {
    const char *name = ldk_ecs_entity_name_get(entity);

    if (name && name[0])
    {
      s_append_indent(context->out, 3u);
      x_strbuilder_append(context->out, "name: ");
      s_append_escaped_string(context->out, name);
      x_strbuilder_append_char(context->out, '\n');
    }
  }

  s_append_indent(context->out, 3u);
  x_strbuilder_append(context->out, "components:\n");

  component_count = ldk_ecs_entity_component_count(entity);
  wrote_any_component = false;

  for (i = 0; i < component_count; i++)
  {
    u32 component_type;

    if (!ldk_ecs_entity_component_type_at(entity, i, &component_type))
    {
      s_result_error(context->result,
          "failed to enumerate entity component");
      context->ok = false;
      return false;
    }

    if (!s_write_component(
            context, entity, component_type, &wrote_any_component))
    {
      context->ok = false;
      return false;
    }
  }

  if (!wrote_any_component)
  {
    s_append_indent(context->out, 4u);
    x_strbuilder_append(context->out, "# no serializable components\n");
  }

  return true;
}

bool ldk_scene_to_tml(
    XStrBuilder *out, LDKSceneResult *result)
{
  LDKGame *game;
  LDKSceneSaveContext context;

  if (result)
  {
    ldk_scene_result_clear(result);
  }

  if (!out)
  {
    s_result_error(result, "invalid scene serialization arguments");
    return false;
  }

  game = ldk_game_get();
  if (!game || !game->metadata_count || !game->metadata_get)
  {
    s_result_error(result, "game component metadata is not available");
    return false;
  }

  memset(&context, 0, sizeof(context));
  context.game = game;
  context.result = result;
  context.out = out;
  context.ok = true;

  x_strbuilder_clear(out);

  if (!ldk_ecs_entity_foreach(s_collect_entity_id_callback, &context))
  {
    s_result_error(result, "ECS registries are not available");
    return false;
  }

  if (!context.ok)
  {
    s_map_free(&context.map);
    return false;
  }

  x_strbuilder_append(out, "scene:\n");
  x_strbuilder_append_format(
      out, "  version: %u\n", LDK_SCENE_TML_VERSION);
  x_strbuilder_append(out, "  entities:\n");

  if (!ldk_ecs_entity_foreach(s_write_entity_callback, &context))
  {
    s_map_free(&context.map);
    s_result_error(result, "ECS registries are not available");
    return false;
  }

  s_map_free(&context.map);
  return context.ok;
}

static bool s_file_write_text(const char *path, const char *text)
{
  FILE *file;
  size_t length;

  if (!path || !text)
  {
    return false;
  }

  file = fopen(path, "wb");
  if (!file)
  {
    return false;
  }

  length = strlen(text);
  if (fwrite(text, 1u, length, file) != length)
  {
    fclose(file);
    return false;
  }

  fclose(file);
  return true;
}

bool ldk_scene_save_tml_file(
    const char *path, LDKSceneResult *result)
{
  XStrBuilder *builder;
  bool ok;

  if (result)
  {
    ldk_scene_result_clear(result);
  }

  if (!path)
  {
    s_result_error(result, "invalid scene save path");
    return false;
  }

  builder = x_strbuilder_create();
  if (!builder)
  {
    s_result_error(result, "failed to create TML string builder");
    return false;
  }

  if (!ldk_scene_to_tml(builder, result))
  {
    x_strbuilder_destroy(builder);
    return false;
  }

  ok = s_file_write_text(path, x_strbuilder_to_string(builder));
  x_strbuilder_destroy(builder);

  if (!ok)
  {
    s_result_error(result, "failed to write scene TML file");
    return false;
  }

  return true;
}

#endif // LDK_EDITOR
