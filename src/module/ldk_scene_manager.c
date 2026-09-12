#include <module/ldk_scene_manager.h>
#include <module/ldk_ecs.h>
#include <ldk.h>
#include <ldk_scene_systems.h>

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <ctype.h>
#include <stdx/stdx_ini.h>

typedef struct LDKSceneEntityList
{
  LDKEntity *entities;
  u32 count;
  u32 capacity;
  bool ok;
} LDKSceneEntityList;

static bool s_string_is_empty(const char *str)
{
  return str == NULL || str[0] == 0;
}

static bool s_path_equal(const XFSPath *left, const XFSPath *right)
{
  if (!left || !right)
  {
    return false;
  }

  return strcmp(x_fs_path_cstr(left), x_fs_path_cstr(right)) == 0;
}

static void s_scene_name_set(LDKScene *scene)
{
  const char *path;
  const char *filename;
  const char *end;
  const char *cursor;
  char name[X_SMALLSTR_MAX_LENGTH];
  size_t length;

  X_ASSERT(scene != NULL);

  path = x_fs_path_cstr(&scene->path);
  filename = path;
  cursor = path;

  while (*cursor)
  {
    if (*cursor == '/' || *cursor == '\\')
    {
      filename = cursor + 1;
    }

    cursor++;
  }

  end = cursor;
  cursor = end;

  while (cursor > filename)
  {
    cursor--;

    if (*cursor == '.')
    {
      end = cursor;
      break;
    }
  }

  length = (size_t)(end - filename);
  if (length >= sizeof(name))
  {
    length = sizeof(name) - 1u;
  }

  memcpy(name, filename, length);
  name[length] = 0;

  x_smallstr_from_cstr(&scene->name, name);
}

static void s_scene_set(LDKScene *scene, const XFSPath *path, u32 index)
{
  X_ASSERT(scene != NULL);
  X_ASSERT(path != NULL);

  memset(scene, 0, sizeof(*scene));

  scene->path = *path;
  x_fs_path_normalize(&scene->path);
  scene->index = index;

  s_scene_name_set(scene);
}

static bool s_catalog_is_valid(const LDKSceneManagerConfig *config)
{
  u32 i;
  u32 j;

  if (!config || (config->scene_count && !config->scenes))
  {
    return false;
  }

  if (s_string_is_empty(x_fs_path_cstr(&config->runtree_path)))
  {
    return false;
  }

  for (i = 0; i < config->scene_count; i++)
  {
    XFSPath path = config->scenes[i];

    /* Check parent segments before normalization, which discards leading .. */
    const char *segment = path.buf;
    while (*segment)
    {
      const char *end = segment;
      while (*end && *end != '/' && *end != '\\')
      {
        ++end;
      }
      if (end - segment == 2 && segment[0] == '.' && segment[1] == '.')
      {
        return false;
      }
      segment = *end ? end + 1 : end;
    }
    x_fs_path_normalize(&path);

    if (s_string_is_empty(x_fs_path_cstr(&path)) ||
        x_fs_path_is_absolute_cstr(x_fs_path_cstr(&path)) ||
        strcmp(path.buf, "..") == 0 || strncmp(path.buf, "../", 3) == 0 ||
        strncmp(path.buf, "..\\", 3) == 0 || strchr(path.buf, ':'))
    {
      return false;
    }

    for (j = i + 1u; j < config->scene_count; j++)
    {
      XFSPath other = config->scenes[j];

      x_fs_path_normalize(&other);

      if (s_path_equal(&path, &other))
      {
        return false;
      }
    }
  }

  return true;
}

static LDKScene *s_catalog_copy(const LDKSceneManagerConfig *config)
{
  LDKScene *scenes;
  u32 i;

  X_ASSERT(config != NULL);

  scenes = (LDKScene *)calloc((size_t)config->scene_count, sizeof(LDKScene));

  if (!scenes)
  {
    return NULL;
  }

  for (i = 0; i < config->scene_count; i++)
  {
    s_scene_set(&scenes[i], &config->scenes[i], i);
    if (config->names && config->names[i].buf[0])
    {
      scenes[i].name = config->names[i];
    }
  }

  return scenes;
}

static bool s_entity_list_reserve(LDKSceneEntityList *list, u32 min_capacity)
{
  LDKEntity *entities;
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

  entities = (LDKEntity *)realloc(
      list->entities, sizeof(LDKEntity) * (size_t)capacity);

  if (!entities)
  {
    return false;
  }

  list->entities = entities;
  list->capacity = capacity;
  return true;
}

static bool s_entity_collect(LDKEntity entity, void *user)
{
  LDKSceneEntityList *list = (LDKSceneEntityList *)user;

  if (!list || !list->ok)
  {
    return false;
  }

  if (!s_entity_list_reserve(list, list->count + 1u))
  {
    list->ok = false;
    return false;
  }

  list->entities[list->count] = entity;
  list->count += 1u;
  return true;
}

static bool s_ecs_clear(void)
{
  LDKSceneEntityList list;
  u32 i;

  memset(&list, 0, sizeof(list));
  list.ok = true;

  if (!ldk_ecs_entity_foreach(s_entity_collect, &list))
  {
    free(list.entities);
    return false;
  }

  if (!list.ok)
  {
    free(list.entities);
    return false;
  }

  for (i = 0; i < list.count; i++)
  {
    ldk_ecs_entity_destroy(list.entities[i]);
  }

  free(list.entities);
  return true;
}

static bool s_manager_is_configured(const LDKSceneManager *manager)
{
  return manager && manager->is_initialized && manager->scenes &&
         manager->scene_count > 0;
}

static void s_scene_resolve_path(const LDKSceneManager *manager,
    const LDKScene *scene, XFSPath *out_path)
{
  X_ASSERT(manager != NULL);
  X_ASSERT(scene != NULL);
  X_ASSERT(out_path != NULL);

  x_fs_path(out_path, x_fs_path_cstr(&manager->runtree_path),
      x_fs_path_cstr(&scene->path));
  x_fs_path_normalize(out_path);
}

static const LDKScene *s_scene_load(LDKSceneManager *manager,
    const LDKScene *scene, LDKSceneResult *result)
{
  XFSPath path;
  LDKSceneSystems systems = {0};
  LDKECS *ecs;
  bool session_started;

  ldk_scene_result_clear(result);

  if (!manager || !manager->is_initialized)
  {
    ldk_scene_result_set_error(result, "Scene Manager is not initialized");
    return NULL;
  }

  if (!s_manager_is_configured(manager))
  {
    ldk_scene_result_set_error(result, "Scene Manager is not configured");
    return NULL;
  }

  if (!scene)
  {
    ldk_scene_result_set_error(result, "invalid scene");
    return NULL;
  }

  s_scene_resolve_path(manager, scene, &path);

  /* Read and validate the target before destroying the current scene. */
  if (!ldk_scene_systems_load_tml_file(
          x_fs_path_cstr(&path), &systems, result))
  {
    return NULL;
  }

  ecs = ldk_module_get(LDK_MODULE_ECS);
  session_started = ldk_game_instance_is_started();

  if (ecs && !ldk_scene_systems_validate_bindings(&ecs->system, &systems))
  {
    ldk_scene_result_set_error(result, "invalid scene system grouping binding");
    ldk_scene_systems_clear(&systems);
    return NULL;
  }

  if (ecs && ecs->system.is_started &&
      !ldk_scene_systems_stop_missing(&ecs->system, NULL))
  {
    ldk_scene_result_set_error(result, "failed to stop previous scene systems");
    ldk_scene_systems_clear(&systems);
    return NULL;
  }

  if (!s_ecs_clear())
  {
    ldk_scene_result_set_error(result, "failed to clear ECS");
    ldk_scene_systems_clear(&systems);
    if (session_started)
    {
      ldk_game_instance_stop();
    }
    return NULL;
  }

  manager->current_scene = NULL;
  ldk_scene_systems_clear(&manager->current_systems);

  if (!ldk_scene_load_tml_file_with_systems(
          x_fs_path_cstr(&path), &systems, result))
  {
    /* The low-level loader can leave partially deserialized entities. */
    if (session_started)
    {
      ldk_game_instance_stop();
    }
    s_ecs_clear();
    ldk_scene_systems_clear(&systems);
    return NULL;
  }

  manager->current_systems = systems;
  manager->current_scene = scene;

  if (session_started && ecs &&
      !ldk_scene_systems_start(&ecs->system, &manager->current_systems))
  {
    ldk_scene_result_set_error(result, "failed to initialize scene systems");
    ldk_game_instance_stop();
    s_ecs_clear();
    ldk_scene_systems_clear(&manager->current_systems);
    manager->current_scene = NULL;
    return NULL;
  }

  return scene;
}

bool ldk_scene_manager_initialize(LDKSceneManager *manager)
{
  if (!manager)
  {
    return false;
  }

  memset(manager, 0, sizeof(*manager));
  manager->is_initialized = true;
  return true;
}

bool ldk_scene_manager_override(
    LDKSceneManager *manager, const LDKSceneManagerConfig *config)
{
  LDKScene *new_scenes = NULL;
  XFSPath new_runtree_path;
  u32 new_scene_count = 0;

  if (!manager || !manager->is_initialized || ldk_game_instance_is_started() ||
      ldk_game_instance_is_updating())
  {
    return false;
  }

  memset(&new_runtree_path, 0, sizeof(new_runtree_path));

  if (config)
  {
    if (!s_catalog_is_valid(config))
    {
      return false;
    }

    new_scenes = config->scene_count ? s_catalog_copy(config) : NULL;
    if (config->scene_count && !new_scenes)
    {
      return false;
    }

    new_scene_count = config->scene_count;
    new_runtree_path = config->runtree_path;
    x_fs_path_normalize(&new_runtree_path);
  }

  if (!ldk_scene_manager_unload(manager))
  {
    free(new_scenes);
    return false;
  }

  free(manager->scenes);
  manager->scenes = new_scenes;
  manager->scene_count = new_scene_count;
  manager->runtree_path = new_runtree_path;
  return true;
}

bool ldk_scene_manager_catalog_validate(const LDKScene *scenes, u32 count)
{
  if (count && !scenes)
  {
    return false;
  }
  for (u32 i = 0; i < count; ++i)
  {
    LDKSceneManagerConfig config = {0};
    config.scenes = &scenes[i].path;
    config.scene_count = 1;
    x_fs_path_set(&config.runtree_path, ".");
    if (!s_catalog_is_valid(&config))
    {
      return false;
    }
    XFSPath path = scenes[i].path;
    x_fs_path_normalize(&path);
    for (u32 j = 0; j < i; ++j)
    {
      XFSPath other = scenes[j].path;
      x_fs_path_normalize(&other);
      if (x_fs_path_compare(&path, &other) == 0)
      {
        return false;
      }
    }
  }
  return true;
}

bool ldk_scene_manager_catalog_exchange(
    LDKSceneManager *manager, LDKScene **scenes, u32 *count)
{
  if (!manager || !manager->is_initialized || !scenes || !count ||
      ldk_game_instance_is_started() || ldk_game_instance_is_updating() ||
      !ldk_scene_manager_catalog_validate(*scenes, *count))
  {
    return false;
  }
  XFSPath current_path = {0};
  if (manager->current_scene)
  {
    current_path = manager->current_scene->path;
  }
  for (u32 i = 0; i < *count; ++i)
  {
    (*scenes)[i].index = i;
    x_fs_path_normalize(&(*scenes)[i].path);
    if (!(*scenes)[i].name.buf[0])
    {
      s_scene_name_set(&(*scenes)[i]);
    }
  }
  LDKScene *old_scenes = manager->scenes;
  u32 old_count = manager->scene_count;
  manager->scenes = *scenes;
  manager->scene_count = *count;
  manager->current_scene = current_path.length
                               ? ldk_scene_manager_find(manager, current_path.buf)
                               : NULL;
  *scenes = old_scenes;
  *count = old_count;
  ldk_scene_manager_pending_clear(manager);
  return true;
}

bool ldk_scene_manager_configure_file(LDKSceneManager *manager,
    const char *ini_path, const XFSPath *runtree_path, LDKSceneResult *result)
{
  XIni ini = {0};
  XIniError error = {0};
  LDKSceneManagerConfig config = {0};
  XFSPath *paths = NULL;
  XSmallstr *names = NULL;
  bool ok = false;
  int section = -1;
  unsigned long count = 0;
  char *end;

  ldk_scene_result_clear(result);
  if (!manager || !ini_path || !runtree_path)
  {
    ldk_scene_result_set_error(result, "invalid scene catalog arguments");
    return false;
  }
  if (!x_ini_load_file(ini_path, &ini, &error))
  {
    ldk_scene_result_set_error(result, "failed to read scene catalog INI");
    return false;
  }


  for (int i = 0; i < x_ini_section_count(&ini); ++i)
  {
    if (strcmp(x_ini_section_name(&ini, i), "scenes") == 0)
    {
      section = i;
      break;
    }
  }
  if (section >= 0)
  {
    const char *value = x_ini_get(&ini, "scenes", "count", NULL);
    if (!value || !isdigit((unsigned char)*value))
    {
      goto invalid;
    }
    errno = 0;
    count = strtoul(value, &end, 10);
    /* At least one path key per entry; also bound allocations by input size. */
    if (errno || *end || count > (unsigned long)x_ini_key_count(&ini, section))
    {
      goto invalid;
    }
    for (int i = 0; i < x_ini_key_count(&ini, section); ++i)
    {
      const char *key = x_ini_key_name(&ini, section, i);
      char canonical[48];
      unsigned long index;
      if (strcmp(key, "count") == 0)
      {
        continue;
      }
      if (!isdigit((unsigned char)*key))
      {
        goto invalid;
      }
      errno = 0;
      index = strtoul(key, &end, 10);
      if (errno || index >= count ||
          (strcmp(end, ".path") != 0 && strcmp(end, ".name") != 0))
      {
        goto invalid;
      }
      snprintf(canonical, sizeof(canonical), "%lu%s", index, end);
      if (strcmp(canonical, key) != 0)
      {
        goto invalid;
      }
    }
  }
  if (count)
  {
    paths = calloc(count, sizeof(*paths));
    names = calloc(count, sizeof(*names));
    if (!paths || !names)
    {
      ldk_scene_result_set_error(result, "failed to allocate scene catalog");
      goto done;
    }
  }
  for (u32 i = 0; i < (u32)count; ++i)
  {
    char key[48];
    const char *value;
    snprintf(key, sizeof(key), "%u.path", i);
    value = x_ini_get(&ini, "scenes", key, NULL);
    if (!value || !*value || strlen(value) >= sizeof(paths[i].buf))
    {
      goto invalid;
    }
    x_fs_path_set(&paths[i], value);
    snprintf(key, sizeof(key), "%u.name", i);
    value = x_ini_get(&ini, "scenes", key, "");
    if (strlen(value) >= sizeof(names[i].buf))
    {
      goto invalid;
    }
    x_smallstr_from_cstr(&names[i], value);
  }
  config.scenes = paths;
  config.names = names;
  config.scene_count = (u32)count;
  config.runtree_path = *runtree_path;
  if (!s_catalog_is_valid(&config))
  {
    goto invalid;
  }

  /* Groupings depend on game component registration, so load them only after
   * the whole scene catalog has been validated. This prevents a malformed
   * [scenes] section from replacing an otherwise valid grouping catalog.
   */
  if (ldk_game_get() && !ldk_ecs_grouping_configure_file(ini_path))
  {
    ldk_scene_result_set_error(result, "failed to load [groupings]");
    goto done;
  }

  ok = ldk_scene_manager_override(manager, &config);
  if (!ok)
  {
    ldk_scene_result_set_error(result, "failed to apply scene catalog");
  }
  goto done;

invalid:
  ldk_scene_result_set_error(result,
      "invalid [scenes]: expected count and contiguous N.path/N.name entries; "
      "paths must be unique and relative to the runtree");
done:
  free(paths);
  free(names);
  x_ini_free(&ini);
  return ok;
}

void ldk_scene_manager_terminate(LDKSceneManager *manager)
{
  if (!manager)
  {
    return;
  }

  if (manager->is_initialized && !ldk_scene_manager_unload(manager))
  {
    return;
  }

  free(manager->scenes);
  ldk_scene_systems_clear(&manager->current_systems);
  memset(manager, 0, sizeof(*manager));
}

u32 ldk_scene_manager_count(const LDKSceneManager *manager)
{
  if (!manager || !manager->is_initialized)
  {
    return 0;
  }

  return manager->scene_count;
}

const LDKScene *ldk_scene_manager_at(
    const LDKSceneManager *manager, u32 index)
{
  if (!s_manager_is_configured(manager) || index >= manager->scene_count)
  {
    return NULL;
  }

  return &manager->scenes[index];
}

const LDKScene *ldk_scene_manager_find(
    const LDKSceneManager *manager, const char *path)
{
  XFSPath normalized_path;
  u32 i;

  if (!s_manager_is_configured(manager) || s_string_is_empty(path))
  {
    return NULL;
  }

  x_fs_path_set(&normalized_path, path);
  x_fs_path_normalize(&normalized_path);

  for (i = 0; i < manager->scene_count; i++)
  {
    if (s_path_equal(&manager->scenes[i].path, &normalized_path))
    {
      return &manager->scenes[i];
    }
  }

  return NULL;
}

const LDKScene *ldk_scene_manager_load(
    LDKSceneManager *manager, u32 index, LDKSceneResult *result)
{
  const LDKScene *scene;

  if (!manager || !manager->is_initialized)
  {
    ldk_scene_result_set_error(result, "Scene Manager is not initialized");
    return NULL;
  }

  if (!s_manager_is_configured(manager))
  {
    ldk_scene_result_set_error(result, "Scene Manager is not configured");
    return NULL;
  }

  scene = ldk_scene_manager_at(manager, index);
  if (!scene)
  {
    ldk_scene_result_set_error(result, "scene index is out of range");
    return NULL;
  }

  if (ldk_game_instance_is_updating())
  {
    manager->pending_scene_index = index;
    manager->has_pending_scene = true;
    ldk_scene_result_clear(result);
    return scene;
  }

  return s_scene_load(manager, scene, result);
}

const LDKScene *ldk_scene_manager_load_path(
    LDKSceneManager *manager, const char *path, LDKSceneResult *result)
{
  const LDKScene *scene;

  if (!manager || !manager->is_initialized)
  {
    ldk_scene_result_set_error(result, "Scene Manager is not initialized");
    return NULL;
  }

  if (!s_manager_is_configured(manager))
  {
    ldk_scene_result_set_error(result, "Scene Manager is not configured");
    return NULL;
  }

  scene = ldk_scene_manager_find(manager, path);
  if (!scene)
  {
    ldk_scene_result_set_error(result, "scene path is not registered");
    return NULL;
  }

  return ldk_scene_manager_load(manager, scene->index, result);
}

const LDKScene *ldk_scene_manager_load_next(
    LDKSceneManager *manager, LDKSceneResult *result)
{
  u32 next_index;

  if (!manager || !manager->is_initialized)
  {
    ldk_scene_result_set_error(result, "Scene Manager is not initialized");
    return NULL;
  }

  if (!s_manager_is_configured(manager))
  {
    ldk_scene_result_set_error(result, "Scene Manager is not configured");
    return NULL;
  }

  if (!manager->current_scene && !manager->has_pending_scene)
  {
    ldk_scene_result_set_error(result, "there is no current scene");
    return NULL;
  }

  next_index = manager->has_pending_scene
                   ? manager->pending_scene_index + 1u
                   : manager->current_scene->index + 1u;

  if (next_index >= manager->scene_count)
  {
    ldk_scene_result_set_error(
        result, "the current scene is the last registered scene");
    return NULL;
  }

  return ldk_scene_manager_load(manager, next_index, result);
}

bool ldk_scene_manager_unload(LDKSceneManager *manager)
{
  LDKECS *ecs;

  if (!manager || !manager->is_initialized || ldk_game_instance_is_updating())
  {
    return false;
  }

  ecs = ldk_engine_is_initialized() ? ldk_module_get(LDK_MODULE_ECS) : NULL;
  if (ecs && ecs->system.is_started &&
      !ldk_scene_systems_stop_missing(&ecs->system, NULL))
  {
    return false;
  }

  if (!s_ecs_clear())
  {
    return false;
  }

  manager->current_scene = NULL;
  ldk_scene_systems_clear(&manager->current_systems);
  ldk_scene_manager_pending_clear(manager);
  return true;
}

const LDKScene *ldk_scene_manager_current(const LDKSceneManager *manager)
{
  if (!manager || !manager->is_initialized)
  {
    return NULL;
  }

  return manager->current_scene;
}

const LDKSceneSystems *ldk_scene_manager_systems_get(
    const LDKSceneManager *manager)
{
  return manager && manager->is_initialized ? &manager->current_systems : NULL;
}

bool ldk_scene_manager_current_reset(LDKSceneManager *manager)
{
  if (!manager || !manager->is_initialized || ldk_game_instance_is_started() ||
      ldk_game_instance_is_updating())
  {
    return false;
  }

  manager->current_scene = NULL;
  ldk_scene_systems_clear(&manager->current_systems);
  ldk_scene_manager_pending_clear(manager);
  return true;
}

bool ldk_scene_manager_process_pending(LDKSceneManager *manager)
{
  u32 index;
  LDKSceneResult result;

  if (!manager || !manager->is_initialized || ldk_game_instance_is_updating())
  {
    return false;
  }

  if (!manager->has_pending_scene)
  {
    return true;
  }

  index = manager->pending_scene_index;
  ldk_scene_manager_pending_clear(manager);

  if (!ldk_scene_manager_load(manager, index, &result))
  {
    ldk_log_error("Deferred scene load failed: %s\n", result.error);
    return false;
  }

  return true;
}

void ldk_scene_manager_pending_clear(LDKSceneManager *manager)
{
  if (!manager)
  {
    return;
  }

  manager->has_pending_scene = false;
  manager->pending_scene_index = 0;
}

