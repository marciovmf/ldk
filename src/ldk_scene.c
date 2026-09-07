#include <ldk_scene.h>
#include <ldk_game.h>

#include <component/ldk_camera.h>
#include <component/ldk_mesh_source.h>
#include <component/ldk_transform.h>

#include <stdio.h>
#include <string.h>

void ldk_scene_result_clear(LDKSceneResult* result)
{
  if (!result)
  {
    return;
  }

  result->ok = true;
  result->error[0] = 0;
}

void ldk_scene_result_set_error(LDKSceneResult* result, const char* error)
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

u32 ldk_scene_component_meta_runtime_type(const LDKComponentMeta* meta)
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

const LDKComponentMeta* ldk_scene_component_meta_find_by_type(
    LDKGame* game,
    u32 component_type)
{
  u32 i = 0;
  u32 count = 0;

  if (!game || !game->metadata_count || !game->metadata_get)
  {
    return NULL;
  }

  count = game->metadata_count();
  for (i = 0; i < count; ++i)
  {
    const LDKComponentMeta* meta = game->metadata_get(i);

    if (ldk_scene_component_meta_runtime_type(meta) == component_type)
    {
      return meta;
    }
  }

  return NULL;
}

const LDKComponentFieldMeta* ldk_scene_component_field_find(
    const LDKComponentMeta* meta,
    const char* field_name)
{
  u32 i = 0;

  if (!meta || !field_name)
  {
    return NULL;
  }

  for (i = 0; i < meta->field_count; ++i)
  {
    const LDKComponentFieldMeta* field = &meta->fields[i];

    if (field->name && strcmp(field->name, field_name) == 0)
    {
      return field;
    }
  }

  return NULL;
}

bool ldk_scene_component_field_is_serializable(
    const LDKComponentMeta* meta,
    const LDKComponentFieldMeta* field)
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

  if (field->type == LDK_FIELD_ASSET_MESH ||
      field->type == LDK_FIELD_RESOURCE_MESH)
  {
    return false;
  }

  return true;
}
