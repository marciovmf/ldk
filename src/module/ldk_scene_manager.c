#include <module/ldk_scene_manager.h>
#include <module/ldk_ecs.h>

#include <stdlib.h>
#include <string.h>

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

  if (!config || !config->scenes || config->scene_count == 0)
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

    x_fs_path_normalize(&path);

    if (s_string_is_empty(x_fs_path_cstr(&path)) ||
        x_fs_path_is_absolute_cstr(x_fs_path_cstr(&path)))
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

static LDKScene *s_catalog_copy(
    const LDKSceneManagerConfig *config)
{
  LDKScene *scenes;
  u32 i;

  X_ASSERT(config != NULL);

  scenes = (LDKScene *)calloc(
      (size_t)config->scene_count, sizeof(LDKScene));

  if (!scenes)
  {
    return NULL;
  }

  for (i = 0; i < config->scene_count; i++)
  {
    s_scene_set(&scenes[i], &config->scenes[i], i);
  }

  return scenes;
}

static bool s_entity_list_reserve(
    LDKSceneEntityList *list, u32 min_capacity)
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
  return manager && manager->is_initialized &&
      manager->scenes && manager->scene_count > 0;
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

  if (result)
  {
    ldk_scene_result_clear(result);
  }

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

  if (!s_ecs_clear())
  {
    ldk_scene_result_set_error(result, "failed to clear ECS");
    return NULL;
  }

  manager->current_scene = NULL;

  if (!ldk_scene_load_tml_file(x_fs_path_cstr(&path), result))
  {
    /* Remove entities created before a deserialization error. */
    s_ecs_clear();
    return NULL;
  }

  manager->current_scene = scene;
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

  if (!manager || !manager->is_initialized)
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

    new_scenes = s_catalog_copy(config);
    if (!new_scenes)
    {
      return false;
    }

    new_scene_count = config->scene_count;
    new_runtree_path = config->runtree_path;
    x_fs_path_normalize(&new_runtree_path);
  }

  if (!s_ecs_clear())
  {
    free(new_scenes);
    return false;
  }

  manager->current_scene = NULL;

  free(manager->scenes);
  manager->scenes = new_scenes;
  manager->scene_count = new_scene_count;
  manager->runtree_path = new_runtree_path;
  return true;
}

void ldk_scene_manager_terminate(LDKSceneManager *manager)
{
  if (!manager)
  {
    return;
  }

  if (manager->is_initialized)
  {
    ldk_scene_manager_unload(manager);
  }

  free(manager->scenes);
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

  return s_scene_load(manager, scene, result);
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

  if (!manager->current_scene)
  {
    ldk_scene_result_set_error(result, "there is no current scene");
    return NULL;
  }

  next_index = manager->current_scene->index + 1u;

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
  if (!manager || !manager->is_initialized)
  {
    return false;
  }

  if (!s_ecs_clear())
  {
    return false;
  }

  manager->current_scene = NULL;
  return true;
}

const LDKScene *ldk_scene_manager_current(
    const LDKSceneManager *manager)
{
  if (!manager || !manager->is_initialized)
  {
    return NULL;
  }

  return manager->current_scene;
}
