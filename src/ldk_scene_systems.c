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
  void **data;
  u32 *data_sizes;
  u32 capacity;

  if (!systems)
  {
    return false;
  }

  if (systems->capacity >= min_capacity && systems->ids &&
      systems->grouping_ids && systems->data && systems->data_sizes)
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
      (capacity && sizeof(*ids) > SIZE_MAX / (size_t)capacity))
  {
    return false;
  }

  ids = (u64 *)malloc(sizeof(*ids) * (size_t)capacity);
  grouping_ids = (u64 *)malloc(sizeof(*grouping_ids) * (size_t)capacity);
  data = (void **)calloc(capacity, sizeof(*data));
  data_sizes = (u32 *)calloc(capacity, sizeof(*data_sizes));
  if (!ids || !grouping_ids || !data || !data_sizes)
  {
    free(data_sizes);
    free(data);
    free(grouping_ids);
    free(ids);
    return false;
  }

  if (systems->count)
  {
    if (systems->data)
    {
      memcpy(data, systems->data, sizeof(*data) * (size_t)systems->count);
      memcpy(data_sizes, systems->data_sizes,
          sizeof(*data_sizes) * (size_t)systems->count);
    }
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
  free(systems->data);
  free(systems->data_sizes);
  systems->data = data;
  systems->data_sizes = data_sizes;
  systems->ids = ids;
  systems->grouping_ids = grouping_ids;
  systems->capacity = capacity;
  return true;
}

static bool s_scene_system_data_release(LDKSceneSystems *systems, u32 index)
{
  void *data = systems->data ? systems->data[index] : NULL;
  LDKSystemRegistry *registry =
      ldk_engine_is_initialized() ? ldk_ecs_system_registry_get() : NULL;
  if (data && registry &&
      ldk_system_registry_system_data_get(
          registry, systems->ids[index]) == data)
  {
    if ((ldk_system_registry_system_is_started(registry, systems->ids[index]) &&
         !ldk_system_registry_system_stop(registry, systems->ids[index])) ||
        !ldk_system_registry_system_data_set(
            registry, systems->ids[index], NULL) ||
        !ldk_system_registry_system_group_set(
            registry, systems->ids[index], NULL))
    {
      return false;
    }
  }
  free(data);
  if (systems->data)
  {
    systems->data[index] = NULL;
    systems->data_sizes[index] = 0;
  }
  return true;
}

void *ldk_scene_systems_data_get(const LDKSceneSystems *systems, u64 system_id)
{
  u32 index;
  return s_scene_system_find_index(systems, system_id, &index) && systems->data
      ? systems->data[index] : NULL;
}

bool ldk_scene_systems_prepare(
    LDKSystemRegistry *registry, LDKSceneSystems *systems)
{
  if (!registry || !systems || ldk_system_registry_is_busy(registry))
  {
    return false;
  }
  for (u32 i = 0; i < systems->count; ++i)
  {
    LDKSystemDesc desc = {0};
    if (!ldk_system_registry_find_by_id(registry, systems->ids[i], &desc))
    {
      continue;
    }
    if (systems->data[i])
    {
      if (systems->data_sizes[i] != desc.data_size)
      {
        return false;
      }
      continue;
    }
    if (desc.data_size)
    {
      systems->data[i] = calloc(1, desc.data_size);
      if (!systems->data[i])
      {
        return false;
      }
      systems->data_sizes[i] = desc.data_size;
    }
  }
  return true;
}

void ldk_scene_systems_clear(LDKSceneSystems *systems)
{
  if (!systems)
  {
    return;
  }

  LDKSystemRegistry *registry =
      ldk_engine_is_initialized() ? ldk_ecs_system_registry_get() : NULL;
  if (registry && ldk_system_registry_is_busy(registry))
  {
    return;
  }
  for (u32 i = systems->count; i > 0; --i)
  {
    if (!s_scene_system_data_release(systems, i - 1u))
    {
      return;
    }
  }
  free(systems->data);
  free(systems->data_sizes);
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

  if (systems->count == UINT32_MAX ||
      !s_scene_systems_reserve(systems, systems->count + 1u))
  {
    return false;
  }

  systems->ids[systems->count] = id;
  systems->grouping_ids[systems->count] = grouping_id;
  systems->data[systems->count] = NULL;
  systems->data_sizes[systems->count] = 0;
  systems->count += 1u;
  LDKSystemRegistry *registry =
      ldk_engine_is_initialized() ? ldk_ecs_system_registry_get() : NULL;
  LDKSystemDesc desc = {0};
  if (registry && ldk_system_registry_find_by_id(registry, id, &desc) &&
      desc.data_size)
  {
    void *data = calloc(1, desc.data_size);
    if (!data)
    {
      systems->count -= 1u;
      return false;
    }
    systems->data[systems->count - 1u] = data;
    systems->data_sizes[systems->count - 1u] = desc.data_size;
  }
  return true;
}

bool ldk_scene_systems_remove(LDKSceneSystems *systems, u64 id)
{
  u32 index;

  LDKSystemRegistry *registry =
      ldk_engine_is_initialized() ? ldk_ecs_system_registry_get() : NULL;
  if ((registry && ldk_system_registry_is_busy(registry)) ||
      !s_scene_system_find_index(systems, id, &index))
  {
    return false;
  }

  if (!s_scene_system_data_release(systems, index))
  {
    return false;
  }
  memmove(&systems->data[index], &systems->data[index + 1u],
      sizeof(*systems->data) * (size_t)(systems->count - index - 1u));
  memmove(&systems->data_sizes[index], &systems->data_sizes[index + 1u],
      sizeof(*systems->data_sizes) * (size_t)(systems->count - index - 1u));
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

  LDKSystemRegistry *registry =
      ldk_engine_is_initialized() ? ldk_ecs_system_registry_get() : NULL;
  if (!source || !out || (registry && ldk_system_registry_is_busy(registry)))
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

    if (systems && ldk_scene_systems_contains(systems, desc.id))
    {
      continue;
    }

    if (!ldk_system_registry_system_stop(registry, desc.id))
    {
      ldk_log_error("Failed to stop system 0x%016" PRIx64 ".\n", desc.id);
      return false;
    }

    if (!ldk_system_registry_system_data_set(registry, desc.id, NULL) ||
        !ldk_system_registry_system_group_set(registry, desc.id, NULL))
    {
      return false;
    }
  }

  return true;
}

bool ldk_scene_systems_start(
    LDKSystemRegistry *registry, LDKSceneSystems *systems)
{
  u32 count;
  u32 started_count = 0;
  u64 *started;

  if (!registry || !registry->is_started || !systems ||
      !ldk_scene_systems_validate_bindings(registry, systems) ||
      !ldk_scene_systems_prepare(registry, systems))
  {
    return false;
  }
  if (!systems->count)
  {
    return true;
  }
  started = (u64 *)calloc(systems->count, sizeof(*started));
  if (!started)
  {
    return false;
  }
  count = ldk_system_registry_count(registry);
  for (u32 i = 0; i < count; ++i)
  {
    LDKSystemDesc desc = {0};
    u32 index;
    const LDKEntityGroup *group;
    u64 grouping_id;
    if (!ldk_system_registry_at(registry, i, &desc))
    {
      goto failed;
    }
    if (!s_scene_system_find_index(systems, desc.id, &index))
    {
      continue;
    }
    grouping_id = systems->grouping_ids[index];
    group = grouping_id ? ldk_ecs_grouping_get(grouping_id) : NULL;
    if (ldk_system_registry_system_data_get(registry, desc.id) !=
            systems->data[index] &&
        !ldk_system_registry_system_stop(registry, desc.id))
    {
      goto failed;
    }
    if (!ldk_system_registry_system_group_set(registry, desc.id, group))
    {
      goto failed;
    }
    if (ldk_system_registry_system_is_started(registry, desc.id))
    {
      continue;
    }
    if (!ldk_system_registry_system_data_set(
            registry, desc.id, systems->data[index]))
    {
      goto failed;
    }
    /* Include a failed initializer in unbinding. The registry itself invokes
     * its terminate callback exactly once on initialization failure. */
    started[started_count++] = desc.id;
    if (!ldk_system_registry_system_start(registry, desc.id))
    {
      ldk_log_error("Failed to initialize scene system '%s' "
                    "(0x%016" PRIx64 ").\n",
          desc.name ? desc.name : "<unnamed system>", desc.id);
      goto failed;
    }
  }
  free(started);
  return true;

failed:
  while (started_count)
  {
    u64 id = started[--started_count];
    ldk_system_registry_system_stop(registry, id);
    ldk_system_registry_system_data_set(registry, id, NULL);
    ldk_system_registry_system_group_set(registry, id, NULL);
  }
  free(started);
  return false;
}

#ifdef LDK_EDITOR

#include <stdx/stdx_strbuilder.h>

bool ldk_scene_systems_to_tml(XStrBuilder *out,
    const LDKSceneSystems *systems, LDKSceneResult *result)
{
  return ldk_scene_to_tml_with_systems(out, systems, result);
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

