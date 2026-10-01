#include "ldk_editor_scene_ops.h"
#include "ldk_editor_internal.h"
#include "ldk_os.h"
#include <ldk_scene.h>
#include <component/ldk_mesh_source.h>
#include <module/ldk_asset_manager.h>
#include <module/ldk_ecs.h>
#include <module/ldk_scene_manager.h>
#include <string.h>

typedef struct LDKEditorSceneEntityList
{
  XArray *entities;
  bool ok;
} LDKEditorSceneEntityList;

static XFSPath s_editor_scene_runtree = {0};

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

void ldki_editor_scene_state_sync(LDKEditorContext *editor)
{
  XFSPath runtree = {0};

  if (!editor)
  {
    return;
  }

  if (!editor->project.loaded)
  {
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

