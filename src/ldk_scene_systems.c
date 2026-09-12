#include <ldk_scene_systems.h>
#include <ldk.h>
#include <module/ldk_ecs.h>

#include <stdx/stdx_tml.h>

#include <errno.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static bool s_scene_system_find_index(
    const LDKSceneSystems *systems, u64 id, u32 *out_index)
{
  if (!systems || id == 0)
  {
    return false;
  }

  for (u32 i = 0; i < systems->count; ++i)
  {
    if (systems->ids[i] == id)
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

static bool s_scene_systems_reserve(
    LDKSceneSystems *systems, u32 min_capacity)
{
  u64 *ids;
  u64 *grouping_ids;
  u32 capacity;

  if (!systems)
  {
    return false;
  }

  if (systems->capacity >= min_capacity && systems->ids &&
      systems->grouping_ids)
  {
    return true;
  }

  capacity = systems->capacity ? systems->capacity : 8u;
  if (systems->ids && systems->grouping_ids && capacity < min_capacity &&
      capacity <= UINT32_MAX / 2u)
  {
    capacity *= 2u;
  }
  while (capacity < min_capacity)
  {
    if (capacity > UINT32_MAX / 2u)
    {
      capacity = min_capacity;
      break;
    }
    capacity *= 2u;
  }

  if (capacity < min_capacity ||
      (size_t)capacity > SIZE_MAX / sizeof(*ids))
  {
    return false;
  }

  ids = (u64 *)malloc(sizeof(*ids) * (size_t)capacity);
  grouping_ids = (u64 *)malloc(sizeof(*grouping_ids) * (size_t)capacity);
  if (!ids || !grouping_ids)
  {
    free(grouping_ids);
    free(ids);
    return false;
  }

  if (systems->count)
  {
    memcpy(ids, systems->ids, sizeof(*ids) * (size_t)systems->count);
    if (systems->grouping_ids)
    {
      memcpy(grouping_ids, systems->grouping_ids,
          sizeof(*grouping_ids) * (size_t)systems->count);
    }
    else
    {
      memset(grouping_ids, 0,
          sizeof(*grouping_ids) * (size_t)systems->count);
    }
  }

  free(systems->ids);
  free(systems->grouping_ids);
  systems->ids = ids;
  systems->grouping_ids = grouping_ids;
  systems->capacity = capacity;
  return true;
}

void ldk_scene_systems_clear(LDKSceneSystems *systems)
{
  if (!systems)
  {
    return;
  }

  free(systems->ids);
  free(systems->grouping_ids);
  memset(systems, 0, sizeof(*systems));
}

bool ldk_scene_systems_contains(const LDKSceneSystems *systems, u64 id)
{
  return s_scene_system_find_index(systems, id, NULL);
}

bool ldk_scene_systems_add(LDKSceneSystems *systems, u64 id)
{
  return ldk_scene_systems_add_with_grouping(systems, id, 0);
}

bool ldk_scene_systems_add_with_grouping(
    LDKSceneSystems *systems, u64 id, u64 grouping_id)
{
  if (!systems || id == 0)
  {
    return false;
  }

  if (ldk_scene_systems_contains(systems, id))
  {
    return true;
  }

  if (!s_scene_systems_reserve(systems, systems->count + 1u))
  {
    return false;
  }

  systems->ids[systems->count] = id;
  systems->grouping_ids[systems->count] = grouping_id;
  systems->count += 1u;
  return true;
}

bool ldk_scene_systems_remove(LDKSceneSystems *systems, u64 id)
{
  u32 index;

  if (!s_scene_system_find_index(systems, id, &index))
  {
    return false;
  }

  memmove(&systems->ids[index], &systems->ids[index + 1u],
      sizeof(*systems->ids) * (size_t)(systems->count - index - 1u));
  if (systems->grouping_ids)
  {
    memmove(&systems->grouping_ids[index],
        &systems->grouping_ids[index + 1u],
        sizeof(*systems->grouping_ids) *
            (size_t)(systems->count - index - 1u));
  }
  systems->count -= 1u;
  return true;
}

bool ldk_scene_systems_grouping_set(
    LDKSceneSystems *systems, u64 system_id, u64 grouping_id)
{
  u32 index;

  if (!s_scene_system_find_index(systems, system_id, &index))
  {
    return false;
  }

  if (!systems->grouping_ids &&
      !s_scene_systems_reserve(systems, systems->count))
  {
    return false;
  }

  systems->grouping_ids[index] = grouping_id;
  return true;
}

u64 ldk_scene_systems_grouping_get(
    const LDKSceneSystems *systems, u64 system_id)
{
  u32 index;
  return s_scene_system_find_index(systems, system_id, &index) &&
             systems->grouping_ids
             ? systems->grouping_ids[index]
             : 0;
}

static bool s_scene_system_id_parse(
    const TMLEntry *entry, bool allow_zero, u64 *out_id)
{
  i64 integer;
  TMLString string;
  unsigned long long value;
  char *end;

  if (!entry || !out_id)
  {
    return false;
  }

  if (tml_entry_get_i64(entry, &integer))
  {
    if (integer < 0 || (!allow_zero && integer == 0))
    {
      return false;
    }

    *out_id = (u64)integer;
    return true;
  }

  if (!tml_entry_get_string(entry, &string) || !string.data ||
      string.size == 0 || string.data[0] == '-')
  {
    return false;
  }

  errno = 0;
  value = strtoull(string.data, &end, 0);
  if (errno == ERANGE || end == string.data || *end != 0 ||
      (!allow_zero && value == 0))
  {
    return false;
  }

  *out_id = (u64)value;
  return true;
}

bool ldk_scene_systems_from_tml(
    const char *source, LDKSceneSystems *out, LDKSceneResult *result)
{
  TMLParseResult parse;
  const TMLNode *scene;
  const TMLNode *node;
  LDKSceneSystems systems = {0};

  ldk_scene_result_clear(result);

  if (!source || !out)
  {
    ldk_scene_result_set_error(result, "invalid scene systems arguments");
    return false;
  }

  parse = tml_parse(source);
  if (!parse.ok)
  {
    char error[256];
    snprintf(error, sizeof(error), "TML parse error at %u:%u: %s", parse.line,
        parse.column, parse.error);
    ldk_scene_result_set_error(result, error);
    return false;
  }

  scene = tml_path_find_node(parse.document, "scene");
  if (!scene)
  {
    ldk_scene_result_set_error(result, "scene node is missing");
    tml_document_free(parse.document);
    return false;
  }

  if (!tml_node_find_child(parse.document, scene, "entities"))
  {
    ldk_scene_result_set_error(result, "scene.entities node is missing");
    tml_document_free(parse.document);
    return false;
  }

  node = tml_node_find_child(parse.document, scene, "systems");
  if (node)
  {
    if (node->entry_count != 0)
    {
      ldk_scene_result_set_error(
          result, "scene.systems must contain system nodes");
      goto failed;
    }

    for (u32 i = 0; i < node->child_count; ++i)
    {
      const TMLNode *child = tml_node_child_at(parse.document, node, i);
      const TMLEntry *entry;
      u64 id;
      u64 grouping_id = 0;

      if (!child)
      {
        ldk_scene_result_set_error(result, "invalid system node");
        goto failed;
      }

      entry = tml_node_find_entry(parse.document, child, "id");
      if (!s_scene_system_id_parse(entry, false, &id))
      {
        ldk_scene_result_set_error(result, "invalid scene system id");
        goto failed;
      }

      entry = tml_node_find_entry(parse.document, child, "grouping");
      if (entry && !s_scene_system_id_parse(entry, true, &grouping_id))
      {
        ldk_scene_result_set_error(result, "invalid scene system grouping id");
        goto failed;
      }

      if (ldk_scene_systems_contains(&systems, id))
      {
        ldk_scene_result_set_error(result, "duplicate scene system id");
        goto failed;
      }

      if (!ldk_scene_systems_add_with_grouping(&systems, id, grouping_id))
      {
        ldk_scene_result_set_error(result, "failed to allocate scene systems");
        goto failed;
      }
    }
  }

  tml_document_free(parse.document);
  ldk_scene_systems_clear(out);
  *out = systems;
  return true;

failed:
  tml_document_free(parse.document);
  ldk_scene_systems_clear(&systems);
  return false;
}

bool ldk_scene_systems_load_tml_file(
    const char *path, LDKSceneSystems *out, LDKSceneResult *result)
{
  FILE *file;
  long size;
  char *source;
  bool ok;

  ldk_scene_result_clear(result);

  if (!path || !out)
  {
    ldk_scene_result_set_error(result, "invalid scene systems arguments");
    return false;
  }

  file = fopen(path, "rb");
  if (!file)
  {
    ldk_scene_result_set_error(result, "failed to read scene TML file");
    return false;
  }

  if (fseek(file, 0, SEEK_END) != 0 || (size = ftell(file)) < 0 ||
      fseek(file, 0, SEEK_SET) != 0 || (size_t)size >= SIZE_MAX)
  {
    fclose(file);
    ldk_scene_result_set_error(result, "failed to read scene TML file");
    return false;
  }

  source = (char *)malloc((size_t)size + 1u);
  if (!source)
  {
    fclose(file);
    ldk_scene_result_set_error(result, "failed to allocate scene source");
    return false;
  }

  ok = fread(source, 1, (size_t)size, file) == (size_t)size;
  fclose(file);
  if (!ok)
  {
    free(source);
    ldk_scene_result_set_error(result, "failed to read scene TML file");
    return false;
  }

  source[size] = 0;
  ok = ldk_scene_systems_from_tml(source, out, result);
  free(source);
  return ok;
}

bool ldk_scene_systems_validate_bindings(
    LDKSystemRegistry *registry, const LDKSceneSystems *systems)
{
  if (!registry || !systems)
  {
    return false;
  }

  for (u32 i = 0; i < systems->count; ++i)
  {
    u64 id = systems->ids[i];
    u64 grouping_id = systems->grouping_ids ? systems->grouping_ids[i] : 0;

    if (!ldk_system_registry_has(registry, id))
    {
      ldk_log_error("Scene references unregistered system 0x%016" PRIx64
                    ". Skipping.\n",
          id);
      continue;
    }

    if (grouping_id != 0 && !ldk_ecs_grouping_get(grouping_id))
    {
      ldk_log_error("Scene system 0x%016" PRIx64
                    " references unknown grouping 0x%016" PRIx64 ".\n",
          id, grouping_id);
      return false;
    }
  }

  return true;
}

bool ldk_scene_systems_stop_missing(
    LDKSystemRegistry *registry, const LDKSceneSystems *systems)
{
  u32 count;

  if (!registry || !registry->is_started)
  {
    return false;
  }

  count = ldk_system_registry_count(registry);
  for (u32 i = count; i > 0; --i)
  {
    LDKSystemDesc desc = {0};
    if (!ldk_system_registry_at(registry, i - 1u, &desc))
    {
      return false;
    }

    if (!ldk_system_registry_system_is_started(registry, desc.id) ||
        (systems && ldk_scene_systems_contains(systems, desc.id)))
    {
      continue;
    }

    if (!ldk_system_registry_system_stop(registry, desc.id))
    {
      ldk_log_error("Failed to stop system 0x%016" PRIx64 ".\n", desc.id);
      return false;
    }

    if (!ldk_system_registry_system_group_set(registry, desc.id, NULL))
    {
      return false;
    }
  }

  return true;
}

bool ldk_scene_systems_start(
    LDKSystemRegistry *registry, const LDKSceneSystems *systems)
{
  u32 count;

  if (!registry || !registry->is_started || !systems)
  {
    return false;
  }

  if (!ldk_scene_systems_validate_bindings(registry, systems))
  {
    return false;
  }

  /* Update bindings even for systems already initialized by the previous
   * scene. Group selection is scene state, not system lifecycle state.
   */
  for (u32 i = 0; i < systems->count; ++i)
  {
    u64 id = systems->ids[i];
    u64 grouping_id = systems->grouping_ids ? systems->grouping_ids[i] : 0;
    const LDKEntityGroup *group = NULL;

    if (!ldk_system_registry_has(registry, id))
    {
      continue;
    }

    if (grouping_id != 0)
    {
      group = ldk_ecs_grouping_get(grouping_id);
      if (!group)
      {
        return false;
      }
    }

    if (!ldk_system_registry_system_group_set(registry, id, group))
    {
      return false;
    }
  }

  count = ldk_system_registry_count(registry);
  for (u32 i = 0; i < count; ++i)
  {
    LDKSystemDesc desc = {0};

    if (!ldk_system_registry_at(registry, i, &desc))
    {
      return false;
    }

    if (!ldk_scene_systems_contains(systems, desc.id))
    {
      continue;
    }

    if (!ldk_system_registry_system_start(registry, desc.id))
    {
      ldk_log_error("Failed to initialize scene system '%s' "
                    "(0x%016" PRIx64 ").\n",
          desc.name ? desc.name : "<unnamed system>", desc.id);
      return false;
    }
  }

  return true;
}

#ifdef LDK_EDITOR

#include <stdx/stdx_strbuilder.h>

bool ldk_scene_systems_to_tml(XStrBuilder *out,
    const LDKSceneSystems *systems, LDKSceneResult *result)
{
  if (!out || !systems)
  {
    ldk_scene_result_set_error(result, "invalid scene systems arguments");
    return false;
  }

  if (!ldk_scene_to_tml(out, result))
  {
    return false;
  }

  if (systems->count == 0)
  {
    return true;
  }

  /* ldk_scene_to_tml leaves the builder at the end of scene.entities.
   * A two-space indent returns to the scene root, so the optional systems
   * node can be appended without coupling this serializer to entity output.
   */
  x_strbuilder_append(out, "  systems:\n");
  for (u32 i = 0; i < systems->count; ++i)
  {
    x_strbuilder_append_format(out, "    - id: \"0x%016" PRIx64 "\"\n",
        systems->ids[i]);
    u64 grouping_id =
        systems->grouping_ids ? systems->grouping_ids[i] : 0;
    if (grouping_id != 0)
    {
      x_strbuilder_append_format(out,
          "      grouping: \"0x%016" PRIx64 "\"\n", grouping_id);
    }
  }

  return true;
}

bool ldk_scene_systems_save_tml_file(const char *path,
    const LDKSceneSystems *systems, LDKSceneResult *result)
{
  XStrBuilder *out;
  FILE *file;
  const char *source;
  size_t size;
  bool ok;

  ldk_scene_result_clear(result);

  if (!path || !systems)
  {
    ldk_scene_result_set_error(result, "invalid scene systems arguments");
    return false;
  }

  out = x_strbuilder_create();
  if (!out)
  {
    ldk_scene_result_set_error(result, "failed to allocate scene serializer");
    return false;
  }

  if (!ldk_scene_systems_to_tml(out, systems, result))
  {
    x_strbuilder_destroy(out);
    return false;
  }

  source = x_strbuilder_to_string(out);
  size = x_strbuilder_length(out);
  file = fopen(path, "wb");
  if (!file)
  {
    x_strbuilder_destroy(out);
    ldk_scene_result_set_error(result, "failed to open scene file for writing");
    return false;
  }

  ok = fwrite(source, 1, size, file) == size;
  if (fclose(file) != 0)
  {
    ok = false;
  }
  x_strbuilder_destroy(out);

  if (!ok)
  {
    ldk_scene_result_set_error(result, "failed to write scene TML file");
  }
  return ok;
}

#endif // LDK_EDITOR
