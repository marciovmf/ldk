#include "ldk_editor_internal.h"
#include "ldk_os.h"
#include <ldk_scene.h>
#include <ldk_mesh.h>
#include <component/ldk_mesh_source.h>
#include <component/ldk_transform.h>
#include <stdx/stdx_strbuilder.h>
#include <stdx/stdx_string.h>
#include <inttypes.h> // for PRIu64
#include <string.h>

#define LDK_EDITOR_COLOR_FILE 0xFFFFFFFF
#define LDK_EDITOR_COLOR_FOLDER 0xFAD460FF
#define LDK_EDITOR_COLOR_ICON_ERROR 0xE71A2DFF
#define LDK_EDITOR_COLOR_ICON_WARNING 0xF7B217FF

//------------------------------------------------------------
// Project Explorer
//------------------------------------------------------------

enum
{
  PROJECT_EXPLORER_INITIAL_CAPACITY = 32,
  PROJECT_EXPLORER_TREE_ICON_SIZE = 20,
  PROJECT_EXPLORER_MIN_ICON_SIZE = 20,
};

#define PROJECT_EXPLORER_DOUBLE_CLICK_SECONDS 0.35

typedef struct ProjectExplorerNode
{
  XFSPath path;
  XSmallstr name;
  u32 depth;
  bool root;
} ProjectExplorerNode;

typedef struct ProjectExplorerEntry
{
  XFSPath path;
  XSmallstr name;
  size_t size;
  time_t last_modified;
} ProjectExplorerEntry;

typedef struct ProjectExplorerState
{
  LDKUIRect window_rect;
  LDKUIPoint tree_scroll;
  LDKUIPoint file_scroll;
  XFSPath root;
  XFSPath selected_directory;
  XFSPath selected_file;
  u64 last_click_ticks;
  XArray *expanded_paths;
  XArray *stack;
  XArray *dirs;
  XArray *files;
  bool root_expanded;
  float icon_size;
} ProjectExplorerState;

static ProjectExplorerState s_project_explorer_state = {
  .window_rect = {10.0f, 60.0f, 640.0f, 420.0f},
  .root_expanded = true,
  .icon_size = 48.0f,
};

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

  if (!ldk_ecs_entity_foreach(s_editor_scene_entity_collect, &list) ||
      !list.ok)
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
  return true;
}

static void s_editor_scene_state_sync(LDKEditorContext *editor)
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
    return;
  }

  x_fs_path_set(&runtree, editor->project.run_root_path.buf);
  x_fs_path_normalize(&runtree);

  if (s_editor_scene_runtree.length == 0 ||
      x_fs_path_compare(&s_editor_scene_runtree, &runtree) != 0)
  {
    s_editor_scene_runtree = runtree;
    memset(&editor->current_scene_path, 0, sizeof(editor->current_scene_path));
    s_editor_scene_selection_clear(editor);
  }
}

static bool s_editor_scene_path_is_scene(const XFSPath *path)
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

static bool s_editor_scene_path_relative(LDKEditorContext *editor,
    const XFSPath *path, XFSPath *out_relative)
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
  if (!x_fs_path_common_prefix(x_fs_path_cstr(&runtree),
        x_fs_path_cstr(&normalized), out_relative))
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

static bool s_editor_scene_full_path(LDKEditorContext *editor,
    const XFSPath *relative, XFSPath *out_path)
{
  if (!editor || !editor->project.loaded || !relative || !out_path ||
      relative->length == 0 || x_fs_path_is_absolute(relative))
  {
    return false;
  }

  x_fs_path(out_path, editor->project.run_root_path.buf,
      x_fs_path_cstr(relative));
  x_fs_path_normalize(out_path);
  return true;
}

static bool s_editor_scene_load(
    LDKEditorContext *editor, const XFSPath *path)
{
  LDKSceneResult result;
  XFSPath relative = {0};

  s_editor_scene_state_sync(editor);

  if (!editor || editor->editor_state != LDK_EDITOR_STATE_STOPED ||
      !s_editor_scene_path_is_scene(path) ||
      !s_editor_scene_path_relative(editor, path, &relative))
  {
    return false;
  }

  if (!s_editor_scene_ecs_clear())
  {
    ldk_editor_internal_log_error(editor, "Failed to clear the current scene.");
    return false;
  }

  s_editor_scene_selection_clear(editor);
  memset(&editor->current_scene_path, 0, sizeof(editor->current_scene_path));

  if (!ldk_scene_load_tml_file(x_fs_path_cstr(path), &result))
  {
    s_editor_scene_ecs_clear();
    ldk_editor_internal_log_error(editor, result.error);
    return false;
  }

  editor->current_scene_path = relative;
  ldk_editor_internal_log_info(editor, "Scene loaded.");
  return true;
}

static bool s_editor_scene_save(LDKEditorContext *editor)
{
  LDKSceneResult result;
  XFSPath path = {0};

  s_editor_scene_state_sync(editor);

  if (!editor || editor->editor_state != LDK_EDITOR_STATE_STOPED ||
      editor->current_scene_path.length == 0 ||
      !s_editor_scene_full_path(editor, &editor->current_scene_path, &path))
  {
    return false;
  }

  if (!ldk_scene_save_tml_file(x_fs_path_cstr(&path), &result))
  {
    ldk_editor_internal_log_error(editor, result.error);
    return false;
  }

  ldk_editor_internal_log_info(editor, "Scene saved.");
  return true;
}

static bool s_editor_scene_new(LDKEditorContext *editor)
{
  LDKSceneResult result;
  XFSPath path = {0};
  XFSPath relative = {0};
  char selected_path[X_FS_PATH_MAX_LENGTH] = {0};

  s_editor_scene_state_sync(editor);

  if (!editor || !editor->project.loaded ||
      editor->editor_state != LDK_EDITOR_STATE_STOPED)
  {
    return false;
  }

  if (!ldk_os_dialog_show_save_file(editor->window, "New Scene", "*.scene",
        selected_path, sizeof(selected_path)))
  {
    return false;
  }

  x_fs_path_set(&path, selected_path);
  x_fs_path_normalize(&path);
  x_fs_path_change_extension(&path, ".scene");

  if (!s_editor_scene_path_relative(editor, &path, &relative))
  {
    ldk_os_dialog_show_error(editor->window, "Invalid scene path",
        "Scene files must be saved inside the project runtree.");
    return false;
  }

  if (!s_editor_scene_ecs_clear())
  {
    ldk_editor_internal_log_error(editor, "Failed to clear the current scene.");
    return false;
  }

  s_editor_scene_selection_clear(editor);
  memset(&editor->current_scene_path, 0, sizeof(editor->current_scene_path));

  if (!ldk_scene_save_tml_file(x_fs_path_cstr(&path), &result))
  {
    ldk_editor_internal_log_error(editor, result.error);
    return false;
  }

  editor->current_scene_path = relative;
  ldk_editor_internal_log_info(editor, "Scene created.");
  return true;
}

static bool s_editor_scene_add_primitive(LDKEditorContext *editor,
    LDKMeshPrimitive primitive, const char *name)
{
  LDKAssetManager *asset_manager;
  LDKAssetMesh asset;
  LDKEntity entity;
  LDKMeshSource *mesh_source;

  s_editor_scene_state_sync(editor);

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
  return true;
}

static bool s_project_explorer_initialize(ProjectExplorerState *state)
{
  if (state->expanded_paths == NULL)
  {
    state->expanded_paths =
      x_array_create(sizeof(XFSPath), PROJECT_EXPLORER_INITIAL_CAPACITY);
  }

  if (state->stack == NULL)
  {
    state->stack = x_array_create(
      sizeof(ProjectExplorerNode), PROJECT_EXPLORER_INITIAL_CAPACITY);
  }

  if (state->dirs == NULL)
  {
    state->dirs = x_array_create(
      sizeof(ProjectExplorerEntry), PROJECT_EXPLORER_INITIAL_CAPACITY);
  }

  if (state->files == NULL)
  {
    state->files = x_array_create(
      sizeof(ProjectExplorerEntry), PROJECT_EXPLORER_INITIAL_CAPACITY);
  }

  return state->expanded_paths != NULL && state->stack != NULL &&
         state->dirs != NULL && state->files != NULL;
}

static i32 s_project_explorer_expanded_path_index(
  ProjectExplorerState *state, const XFSPath *path)
{
  for (u32 i = 0; i < x_array_count(state->expanded_paths); i++)
  {
    XFSPath *expanded_path = x_array_get(state->expanded_paths, i);
    if (x_fs_path_compare(expanded_path, path) == 0)
    {
      return (i32)i;
    }
  }

  return -1;
}

static bool s_project_explorer_root_set(
  ProjectExplorerState *state, const char *root_path)
{
  if (root_path == NULL)
  {
    return false;
  }

  XFSPath root = {0};
  x_fs_path_set(&root, root_path);
  x_fs_path_normalize(&root);

  if (!x_fs_path_is_directory(&root))
  {
    return false;
  }

  if (state->root.length == 0 || x_fs_path_compare(&state->root, &root) != 0)
  {
    state->root = root;
    state->selected_directory = root;
    memset(&state->selected_file, 0, sizeof(state->selected_file));
    x_array_clear(state->expanded_paths);
    state->last_click_ticks = 0;
    state->root_expanded = true;
  }

  return true;
}

static void s_project_explorer_directory_select(
  ProjectExplorerState *state, const XFSPath *path, bool expand)
{
  state->selected_directory = *path;
  memset(&state->selected_file, 0, sizeof(state->selected_file));

  if (!expand)
  {
    return;
  }

  if (x_fs_path_compare(&state->root, path) == 0)
  {
    state->root_expanded = true;
    return;
  }

  if (s_project_explorer_expanded_path_index(state, path) < 0)
  {
    x_array_add(state->expanded_paths, (XFSPath*) path);
  }
}

static void s_project_explorer_entry_insert_sorted(
  XArray *entries, const ProjectExplorerEntry *entry)
{
  u32 insert_index = x_array_count(entries);

  for (u32 i = 0; i < x_array_count(entries); i++)
  {
    ProjectExplorerEntry *it = x_array_get(entries, i);
    if (strcmp(it->name.buf, entry->name.buf) > 0)
    {
      insert_index = i;
      break;
    }
  }

  x_array_insert(entries, (XArray*) entry, insert_index);
}

static void s_project_explorer_directory_read(
  const XFSPath *path, XArray *dirs, XArray *files)
{
  if (dirs != NULL)
  {
    x_array_clear(dirs);
  }

  if (files != NULL)
  {
    x_array_clear(files);
  }

  XFSDireEntry fs_entry = {0};
  XFSDireHandle *dir =
    x_fs_find_first_file(x_fs_path_cstr(path), &fs_entry);

  while (dir != NULL)
  {
    bool special_entry = strcmp(fs_entry.name, ".") == 0 ||
                         strcmp(fs_entry.name, "..") == 0;
    XArray *target = fs_entry.is_directory ? dirs : files;

    if (!special_entry && target != NULL)
    {
      ProjectExplorerEntry entry = {0};
      entry.path = *path;
      x_fs_path_join(&entry.path, fs_entry.name);
      x_smallstr_from_cstr(&entry.name, fs_entry.name);
      entry.size = fs_entry.size;
      entry.last_modified = fs_entry.last_modified;
      s_project_explorer_entry_insert_sorted(target, &entry);
    }

    if (!x_fs_find_next_file(dir, &fs_entry))
    {
      break;
    }
  }

  if (dir != NULL)
  {
    x_fs_find_close(dir);
  }
}

static bool s_project_explorer_tree_node(
  ProjectExplorerState *state, LDKUIContext *ui,
  const ProjectExplorerNode *node, LDKUIIcon folder_icon)
{
  i32 expanded_index = node->root
                         ? -1
                         : s_project_explorer_expanded_path_index(
                             state, &node->path);
  bool was_expanded = node->root
                        ? state->root_expanded
                        : expanded_index >= 0;
  u32 flags = 0;

  if (x_fs_path_compare(&state->selected_directory, &node->path) == 0)
  {
    flags |= LDK_UI_TREE_NODE_SELECTED;
  }

  u32 result = ldk_ui_tree_node_ex(
    ui, node->name.buf, folder_icon, was_expanded, node->depth, flags);
  bool expanded = was_expanded;

  if (result & LDK_UI_TREE_NODE_RESULT_CLICKED)
  {
    s_project_explorer_directory_select(state, &node->path, false);
  }

  if (result & LDK_UI_TREE_NODE_RESULT_TOGGLED)
  {
    expanded = !was_expanded;
  }

  if (expanded == was_expanded)
  {
    return expanded;
  }

  s_project_explorer_directory_select(state, &node->path, false);

  if (node->root)
  {
    state->root_expanded = expanded;
  }
  else if (expanded)
  {
    x_array_add(state->expanded_paths, (ProjectExplorerNode*)&node->path);
  }
  else
  {
    x_array_delete_at(state->expanded_paths, (u32)expanded_index);
  }

  return expanded;
}

static void s_project_explorer_tree_draw(
  ProjectExplorerState *state, LDKUIContext *ui, LDKUIIcon folder_icon)
{
  ldk_ui_set_next_width(ui, ldk_ui_px(220.0f));
  state->tree_scroll = ldk_ui_begin_scrollview(
    ui, state->tree_scroll,
    LDK_UI_SCROLL_VERTICAL | LDK_UI_SCROLL_IF_NEEDED);

  x_array_clear(state->stack);

  ProjectExplorerNode root_node = {0};
  root_node.path = state->root;
  x_fs_path_basename(&state->root, &root_node.name);
  if (root_node.name.length == 0)
  {
    x_smallstr_from_cstr(
      &root_node.name, x_fs_path_cstr(&state->root));
  }
  root_node.root = true;
  x_array_add(state->stack, &root_node);

  while (x_array_count(state->stack) > 0)
  {
    u32 stack_index = x_array_count(state->stack) - 1;
    ProjectExplorerNode *node_ptr = x_array_get(state->stack, stack_index);
    ProjectExplorerNode node = *node_ptr;
    x_array_delete_at(state->stack, stack_index);

    if (!s_project_explorer_tree_node(state, ui, &node, folder_icon))
    {
      continue;
    }

    s_project_explorer_directory_read(&node.path, state->dirs, NULL);

    for (u32 i = x_array_count(state->dirs); i > 0; i--)
    {
      ProjectExplorerEntry *entry = x_array_get(state->dirs, i - 1);
      ProjectExplorerNode child = {0};
      child.path = entry->path;
      child.name = entry->name;
      child.depth = node.depth + 1;
      x_array_add(state->stack, &child);
    }
  }

  ldk_ui_spacer(ui);
  ldk_ui_end_scrollview(ui);
}

static ProjectExplorerEntry *s_project_explorer_entry_get(
  ProjectExplorerState *state, u32 index, bool *is_directory)
{
  u32 directory_count = x_array_count(state->dirs);
  *is_directory = index < directory_count;

  if (*is_directory)
  {
    return x_array_get(state->dirs, index);
  }

  return x_array_get(state->files, index - directory_count);
}

static void s_project_explorer_entry_activate(LDKEditorContext *editor,
  ProjectExplorerState *state, const ProjectExplorerEntry *entry,
  bool is_directory)
{
  if (is_directory)
  {
    s_project_explorer_directory_select(state, &entry->path, true);
    return;
  }

  u64 now = ldk_os_time_ticks_get();
  bool same_file = state->selected_file.length != 0 &&
                   x_fs_path_compare(&state->selected_file, &entry->path) == 0;
  bool double_click = same_file && state->last_click_ticks != 0 &&
                      ldk_os_time_ticks_interval_get_seconds(
                        state->last_click_ticks, now) <=
                        PROJECT_EXPLORER_DOUBLE_CLICK_SECONDS;

  state->selected_file = entry->path;
  state->last_click_ticks = double_click ? 0 : now;

  if (double_click && s_editor_scene_path_is_scene(&entry->path))
  {
    s_editor_scene_load(editor, &entry->path);
  }
}

static void s_project_explorer_entries_draw(LDKEditorContext *editor,
  ProjectExplorerState *state, LDKUIContext *ui,
  LDKUIIcon folder_icon, LDKUIIcon file_icon)
{
  u32 total_count = x_array_count(state->dirs) + x_array_count(state->files);
  bool compact_mode =
    state->icon_size <= PROJECT_EXPLORER_MIN_ICON_SIZE;

  if (compact_mode)
  {
    for (u32 entry_i = 0; entry_i < total_count; entry_i++)
    {
      bool is_directory = false;
      ProjectExplorerEntry *entry =
        s_project_explorer_entry_get(state, entry_i, &is_directory);

      ldk_ui_set_next_height(ui, ldk_ui_px(LDK_UI_DEFAULT_CONTROL_HEIGHT));
      ldk_ui_begin_horizontal(ui);

      ldk_ui_set_next_weight(ui, 0.0f);
      bool icon_clicked =
        ldk_ui_icon_button(ui, is_directory ? folder_icon : file_icon, NULL);
      bool label_clicked = ldk_ui_button_flat(ui, entry->name.buf);

      ldk_ui_end_horizontal(ui);

      if (icon_clicked || label_clicked)
      {
        s_project_explorer_entry_activate(editor, state, entry, is_directory);
      }
    }

    return;
  }

  float tile_w = state->icon_size + 32.0f;
  float tile_h =
    state->icon_size + LDK_UI_DEFAULT_CONTROL_HEIGHT + 12.0f;
  float available_w = ui->current_layout != NULL
                        ? ui->current_layout->content_rect.w
                        : tile_w;
  u32 column_count = (u32)(available_w / tile_w);
  if (column_count == 0)
  {
    column_count = 1;
  }
  u32 tile_index = 0;
  while (tile_index < total_count)
  {
    ldk_ui_set_next_height(ui, ldk_ui_px(tile_h));
    ldk_ui_begin_horizontal(ui);

    for (u32 column = 0;
         column < column_count && tile_index < total_count;
         column++, tile_index++)
    {
      bool is_directory = false;
      ProjectExplorerEntry *entry =
        s_project_explorer_entry_get(state, tile_index, &is_directory);

      ldk_ui_set_next_size(ui, ldk_ui_px(tile_w), ldk_ui_px(tile_h));
      ldk_ui_begin_vertical(ui);

      ldk_ui_set_next_size(
        ui, ldk_ui_px(state->icon_size), ldk_ui_px(state->icon_size));
      bool icon_clicked =
        ldk_ui_icon_button(ui, is_directory ? folder_icon : file_icon, NULL);

      ldk_ui_set_next_height(ui, ldk_ui_px(LDK_UI_DEFAULT_CONTROL_HEIGHT));
      bool label_clicked = ldk_ui_button_flat(ui, entry->name.buf);

      ldk_ui_end_vertical(ui);

      if (icon_clicked || label_clicked)
      {
        s_project_explorer_entry_activate(editor, state, entry, is_directory);
      }
    }

    ldk_ui_spacer(ui);
    ldk_ui_end_horizontal(ui);
  }
}

static void s_project_explorer_files_draw(LDKEditorContext *editor,
  ProjectExplorerState *state, LDKUIContext *ui,
  LDKUIIcon folder_icon, LDKUIIcon file_icon)
{
  ldk_ui_begin_vertical(ui);
  ldk_ui_set_next_weight(ui, 0.0f);

  XFSPath relative_directory = {0};
  if (x_fs_path_relative_to(
        &state->root, &state->selected_directory, &relative_directory) > 0)
  {
    ldk_ui_label(ui, relative_directory.buf);
  }
  else
  {
    ldk_ui_label(ui, state->selected_directory.buf);
  }

  ldk_ui_set_next_weight(ui, 0.0f);
  state->icon_size = ldk_ui_slider(
    ui, state->icon_size, PROJECT_EXPLORER_MIN_ICON_SIZE, 72.0f);

  folder_icon.size = ldk_sizef(state->icon_size, state->icon_size);
  file_icon.size = folder_icon.size;

  state->file_scroll = ldk_ui_begin_scrollview(
    ui, state->file_scroll,
    LDK_UI_SCROLL_VERTICAL | LDK_UI_SCROLL_IF_NEEDED);

  s_project_explorer_directory_read(
    &state->selected_directory, state->dirs, state->files);
  s_project_explorer_entries_draw(editor, state, ui, folder_icon, file_icon);

  ldk_ui_spacer(ui);
  ldk_ui_end_scrollview(ui);
  ldk_ui_end_vertical(ui);
}

static void s_editor_project_explorer(
  LDKEditorContext *editor, const char *root_path)
{
  ProjectExplorerState *state = &s_project_explorer_state;
  LDKUIContext *ui = &editor->ui;
  bool owns_window = ui->current_window == NULL;

  if (owns_window)
  {
    state->window_rect = ldk_ui_begin_window(
      ui, "Project Explorer", state->window_rect, LDK_UI_WINDOW_TOOL);
  }

  if (!s_project_explorer_initialize(state))
  {
    ldk_ui_label(ui, "Project explorer allocation failed.");

    if (owns_window)
    {
      ldk_ui_end_window(ui);
    }
    return;
  }

  if (!s_project_explorer_root_set(state, root_path))
  {
    ldk_ui_label(ui, "No project root.");

    if (owns_window)
    {
      ldk_ui_end_window(ui);
    }
    return;
  }

  LDKUIIcon file_icon = {0};
  file_icon.size = ldk_sizef(state->icon_size, state->icon_size);
  file_icon.texture =
    ldk_renderer_texture_ui_handle(editor->renderer, editor->ui_atlas);
  file_icon.uv = ldk_editor_icon_rects[LDK_EDITOR_ICON_FILE];
  file_icon.color = LDK_EDITOR_COLOR_FILE;

  LDKUIIcon folder_icon = file_icon;
  folder_icon.uv = ldk_editor_icon_rects[LDK_EDITOR_ICON_FOLDER];
  folder_icon.color = LDK_EDITOR_COLOR_FOLDER;

  LDKUIIcon tree_folder_icon = folder_icon;
  tree_folder_icon.size = ldk_sizef(
    PROJECT_EXPLORER_TREE_ICON_SIZE, PROJECT_EXPLORER_TREE_ICON_SIZE);

  ldk_ui_begin_horizontal(ui);
  s_project_explorer_tree_draw(state, ui, tree_folder_icon);
  s_project_explorer_files_draw(editor, state, ui, folder_icon, file_icon);
  ldk_ui_end_horizontal(ui);

  if (owns_window)
  {
    ldk_ui_end_window(ui);
  }
}

//------------------------------------------------------------
// Console
//------------------------------------------------------------

static void s_editor_console(LDKEditorContext *editor)
{
  LDK_ASSERT(editor);
  LDK_ASSERT(editor->console_sb);

  static XSmallstr input = {0};
  static LDKUIPoint scroll = {0};
  static LDKUIRect window_rect = {150, 90, 200, 180};
  LDKUIContext *ui = &editor->ui;
  bool owns_window = ui->current_window == NULL;

  if (owns_window)
  {
    window_rect = ldk_ui_begin_window_fixed(
      ui, "CONSOLE", window_rect, LDK_UI_WINDOW_TOOL);
  }

  LDKUIIcon icon = {0};
  icon.size =
    ldk_sizef(LDK_UI_DEFAULT_CONTROL_HEIGHT, LDK_UI_DEFAULT_CONTROL_HEIGHT);
  icon.texture =
    ldk_renderer_texture_ui_handle(editor->renderer, editor->ui_atlas);

  scroll = ldk_ui_begin_scrollview(
    ui, scroll, LDK_UI_SCROLL_VERTICAL | LDK_UI_SCROLL_IF_NEEDED);
  icon.uv = ldk_editor_icon_rects[LDK_EDITOR_ICON_WARNING];
  icon.color = LDK_EDITOR_COLOR_ICON_WARNING;
  ldk_ui_icon_label(ui, icon, "This is a warning.");
  icon.uv = ldk_editor_icon_rects[LDK_EDITOR_ICON_ERROR];
  icon.color = LDK_EDITOR_COLOR_ICON_ERROR;
  ldk_ui_icon_label(ui, icon, "This is an error.");
  ldk_ui_label(ui, x_strbuilder_to_string(editor->console_sb));
  ldk_ui_end_scrollview(ui);

  ldk_ui_set_next_weight(ui, 0.0f);
  if (ldk_ui_input_box(ui, input.buf, X_SMALLSTR_MAX_LENGTH) &
      LDK_UI_INPUT_BOX_COMMITTED)
  {
    x_strbuilder_append_format(editor->console_sb, "%s\n", input.buf);
    ldk_editor_command_run(editor, input.buf);
    x_smallstr_clear(&input);
    scroll.y += 10000.0f;
  }

  if (owns_window)
  {
    ldk_ui_end_window(ui);
  }
}

//------------------------------------------------------------
// Menu bar
//------------------------------------------------------------

static void s_editor_menu_bar(LDKEditorContext *editor)
{
  LDKUIContext *ui = &editor->ui;

  s_editor_scene_state_sync(editor);

  static LDKUIRect s_toolbar_rect = {0, 0, 0, 0};
  static LDKUIRect s_file_popup_rect = {0, 0, 1024, 1024};
  static LDKUIRect s_edit_popup_rect = {0, 0, 1024, 1024};
  static LDKUIRect s_theme_popup_rect = {0, 0, 1024, 1024};

  const LDKUIId MENU_ID_FILE = 10;
  const LDKUIId MENU_ID_EDIT = 11;
  const LDKUIId MENU_ID_THEME = 12;
  const LDKUIId MENU_ID_SCENE = 13;

  s_toolbar_rect.w = ui->viewport.w;
  s_toolbar_rect.h =
    LDK_UI_DEFAULT_CONTROL_HEIGHT + LDK_UI_DEFAULT_PADDING;// * 2.0f;

  s_toolbar_rect = ldk_ui_begin_window(ui, "TOOLBAR", s_toolbar_rect, 0);

  ldk_ui_begin_horizontal(ui);
  LDKUIMark mark = ldk_ui_mark(ui);

  ldk_ui_set_next_weight(ui, 0.0f);
  if (ldk_ui_button_flat(ui, "File"))
  {
    ldk_ui_open_popup(ui, MENU_ID_FILE);
  }
  LDKUIRect file_button_rect = ldk_ui_last_rect(ui);

  ldk_ui_set_next_weight(ui, 0.0f);
  if (ldk_ui_button_flat(ui, "Edit"))
  {
    ldk_ui_open_popup(ui, MENU_ID_EDIT);
  }
  LDKUIRect edit_button_rect = ldk_ui_last_rect(ui);

  ldk_ui_set_next_weight(ui, 0.0f);
  if (ldk_ui_button_flat(ui, "Scene"))
  {
    ldk_ui_open_popup(ui, MENU_ID_SCENE);
  }
  LDKUIRect scene_button_rect = ldk_ui_last_rect(ui);

  ldk_ui_set_next_weight(ui, 0.0f);
  if (ldk_ui_button_flat(ui, "Theme"))
  {
    ldk_ui_open_popup(ui, MENU_ID_THEME);
  }
  LDKUIRect theme_button_rect = ldk_ui_last_rect(ui);
  i32 menu_width = ldk_ui_measure_from(ui, mark).w;

  ldk_ui_spacer(ui);
  ldk_ui_end_horizontal(ui);
  ldk_ui_horizontal_line(ui);

  LDKUIRect popup_pos = {
      file_button_rect.x, file_button_rect.y + file_button_rect.h, 120, 10};

  ldk_ui_begin_popup(ui, MENU_ID_FILE);
  {
    LDKUIMark mark = ldk_ui_mark(ui);

    bool can_edit_scene = editor->project.loaded &&
                          editor->editor_state == LDK_EDITOR_STATE_STOPED;

    ldk_ui_set_next_disabled(ui, !can_edit_scene);
    if (ldk_ui_button_flat(ui, "New Scene"))
    {
      s_editor_scene_new(editor);
      ldk_ui_close_current_popup(ui);
    }

    ldk_ui_set_next_disabled(
      ui, !can_edit_scene || editor->current_scene_path.length == 0);
    if (ldk_ui_button_flat(ui, "Save Scene"))
    {
      s_editor_scene_save(editor);
      ldk_ui_close_current_popup(ui);
    }

    if (ldk_ui_button_flat(ui, "Open"))
    {
      XFSPath out = {0};
      if (ldk_os_dialog_show_open_file(editor->window, "Open Project", "*.ldk",
              out.buf, X_SMALLSTR_MAX_LENGTH))
      {
        ldk_editor_state_set_stop(editor);
        ldk_project_unload(&editor->project);
        ldk_game_instance_unload();

        if (!ldk_editor_project_load(editor, out.buf))
        {
          ldk_os_dialog_show_error(
              editor->window, "Failed to load project", out.buf);
        }
      }

      ldk_ui_close_current_popup(ui);
    }

    if (ldk_ui_button_flat(ui, "Exit"))
    {
      ldk_editor_quit(editor);
    }
    LDKUIRect content_rect = ldk_ui_measure_from(ui, mark);
  }
  ldk_ui_end_popup(ui);

  popup_pos.x = edit_button_rect.x;
  popup_pos.y = edit_button_rect.y + edit_button_rect.h;

  ldk_ui_begin_popup(ui, MENU_ID_EDIT);
  {
    LDKUIMark mark = ldk_ui_mark(ui);

    if (ldk_ui_button_flat(ui, "Undo"))
    {
      ldk_ui_close_current_popup(ui);
    }

    if (ldk_ui_button_flat(ui, "Redo"))
    {
      ldk_ui_close_current_popup(ui);
    }

    LDKUIRect content_rect = ldk_ui_measure_from(ui, mark);
  }
  ldk_ui_end_popup(ui);

  popup_pos.x = scene_button_rect.x;
  popup_pos.y = scene_button_rect.y + scene_button_rect.h;

  ldk_ui_begin_popup(ui, MENU_ID_SCENE);
  {
    LDKUIMark mark = ldk_ui_mark(ui);
    bool can_add = editor->project.loaded &&
                   editor->editor_state == LDK_EDITOR_STATE_STOPED &&
                   editor->current_scene_path.length != 0;

    ldk_ui_set_next_disabled(ui, !can_add);
    if (ldk_ui_button_flat(ui, "Add Cube"))
    {
      s_editor_scene_add_primitive(
        editor, LDK_MESH_PRIMITIVE_CUBE, "Cube");
      ldk_ui_close_current_popup(ui);
    }

    ldk_ui_set_next_disabled(ui, !can_add);
    if (ldk_ui_button_flat(ui, "Add Sphere"))
    {
      s_editor_scene_add_primitive(
        editor, LDK_MESH_PRIMITIVE_SPHERE, "Sphere");
      ldk_ui_close_current_popup(ui);
    }

    ldk_ui_set_next_disabled(ui, !can_add);
    if (ldk_ui_button_flat(ui, "Add Capsule"))
    {
      s_editor_scene_add_primitive(
        editor, LDK_MESH_PRIMITIVE_CAPSULE, "Capsule");
      ldk_ui_close_current_popup(ui);
    }

    ldk_ui_set_next_disabled(ui, !can_add);
    if (ldk_ui_button_flat(ui, "Add Plane"))
    {
      s_editor_scene_add_primitive(
        editor, LDK_MESH_PRIMITIVE_PLANE, "Plane");
      ldk_ui_close_current_popup(ui);
    }

    ldk_ui_set_next_disabled(ui, !can_add);
    if (ldk_ui_button_flat(ui, "Add Quad"))
    {
      s_editor_scene_add_primitive(
        editor, LDK_MESH_PRIMITIVE_QUAD, "Quad");
      ldk_ui_close_current_popup(ui);
    }

    LDKUIRect content_rect = ldk_ui_measure_from(ui, mark);
  }
  ldk_ui_end_popup(ui);

  popup_pos.x = theme_button_rect.x;
  popup_pos.y = theme_button_rect.y + theme_button_rect.h;

  ldk_ui_begin_popup(ui, MENU_ID_THEME);
  {
    LDKUITheme theme;
    LDKUIMark mark = ldk_ui_mark(ui);
    if (ldk_ui_button_flat(ui, "Dark"))
    {
      ldk_ui_theme_get(LDK_UI_THEME_DEFAULT_DARK, &theme);
      ldk_editor_internal_theme_icons_set(editor, &theme);
      ldk_ui_theme_set(ui, &theme);
      ldk_ui_close_current_popup(ui);
    }
    if (ldk_ui_button_flat(ui, "Light"))
    {
      ldk_ui_theme_get(LDK_UI_THEME_DEFAULT_LIGHT, &theme);
      ldk_editor_internal_theme_icons_set(editor, &theme);
      ldk_ui_theme_set(ui, &theme);
      ldk_ui_close_current_popup(ui);
    }

    LDKUIRect content_rect = ldk_ui_measure_from(ui, mark);
  }
  ldk_ui_end_popup(ui);

  ldk_ui_end_window(ui);
}

//------------------------------------------------------------
// Toolbar
//------------------------------------------------------------

static bool s_editor_layouts_save(void)
{
  XStrBuilder *out = x_strbuilder_create();
  if (out == NULL)
  {
    return false;
  }

  bool saved = ldk_editor_internal_dock_layout_save(out);
  x_strbuilder_destroy(out);
  return saved;
}

bool ldk_editor_internal_layout_save_as(LDKEditorContext *editor)
{
  XSlice name_slice;
  char layout_name[LDK_EDITOR_DOCK_LAYOUT_NAME_CAPACITY];

  if (editor == NULL)
  {
    return false;
  }

  name_slice = x_slice_trim(x_slice(editor->input_window_buffer));

  if (name_slice.length == 0)
  {
    ldk_editor_internal_log_error(
        editor, "The layout name cannot be empty.");
    return false;
  }

  if (name_slice.length >= sizeof(layout_name))
  {
    ldk_editor_internal_log_error(editor, "The layout name is too long.");
    return false;
  }

  memcpy(layout_name, name_slice.ptr, name_slice.length);
  layout_name[name_slice.length] = 0;

  u32 layout_count = ldk_editor_internal_dock_layout_count();
  for (u32 i = 0; i < layout_count; ++i)
  {
    const char *existing_name =
        ldk_editor_internal_dock_layout_name_get(i);

    if (existing_name != NULL && strcmp(existing_name, layout_name) == 0)
    {
      ldk_editor_internal_log_error(
          editor, "A layout with that name already exists.");
      return false;
    }
  }

  if (!ldk_editor_internal_dock_layout_create(layout_name))
  {
    ldk_editor_internal_log_error(editor, "Failed to save the layout.");
    return false;
  }

  if (!s_editor_layouts_save())
  {
    ldk_editor_internal_log_error(editor,
        "Layout created in memory, but failed to save the layout file.");
  }
  else
  {
    ldk_editor_internal_log_info(editor, "Layout created.");
  }

  return true;
}

u32 ldk_editor_internal_input_window(
    LDKEditorContext *editor, const char *title)
{
  LDKUIContext *ui;
  LDKUIRect *rect;
  u32 result;

  if (editor == NULL || title == NULL || !editor->show_input_window)
  {
    return LDK_UI_INPUT_BOX_NONE;
  }

  ui = &editor->ui;
  rect = &editor->input_window_rect;

  if (rect->w <= 0.0f || rect->h <= 0.0f)
  {
    rect->w = 400.0f;
    rect->h = 128.0f;
    rect->x = (ui->viewport.w - rect->w) * 0.5f;
    rect->y = (ui->viewport.h - rect->h) * 0.5f;
  }

  if (!ldk_ui_begin_window_open(ui, title, rect,
        &editor->show_input_window,
        LDK_UI_WINDOW_TITLE_BAR | LDK_UI_WINDOW_DRAGGABLE |
            LDK_UI_WINDOW_BORDER | LDK_UI_WINDOW_CLOSE_BUTTON))
  {
    return LDK_UI_INPUT_BOX_CANCELED;
  }

  result = ldk_ui_input_box(ui, editor->input_window_buffer,
      (u32)sizeof(editor->input_window_buffer));

  ldk_ui_spacer(ui);
  ldk_ui_begin_horizontal(ui);
  {
    bool confirm_requested =
        (result & LDK_UI_INPUT_BOX_COMMITTED) != 0;
    bool cancel_requested = (result & LDK_UI_INPUT_BOX_CANCELED) != 0;

    ldk_ui_spacer(ui);

    ldk_ui_set_next_width(ui, ldk_ui_px(80.0f));
    cancel_requested |= ldk_ui_button(ui, "CANCEL");

    ldk_ui_set_next_width(ui, ldk_ui_px(80.0f));
    confirm_requested |= ldk_ui_button(ui, "OK");

    if (cancel_requested)
    {
      editor->show_input_window = false;
      result |= LDK_UI_INPUT_BOX_CANCELED;
    }
    else if (confirm_requested)
    {
      result |= LDK_UI_INPUT_BOX_COMMITTED;
    }
  }
  ldk_ui_end_horizontal(ui);

  ldk_ui_end_window(ui);
  return result;
}

static void s_editor_layout_combo_box(LDKEditorContext *editor)
{
  LDKUIContext *ui = &editor->ui;
  const char *items[LDK_EDITOR_DOCK_LAYOUT_CAPACITY + 3];
  const char *current_name =
      ldk_editor_internal_dock_layout_current_name_get();
  u32 stored_layout_count = ldk_editor_internal_dock_layout_count();
  u32 layout_count = stored_layout_count;
  u32 selected_index = 0;

  for (u32 i = 0; i < layout_count; ++i)
  {
    items[i] = ldk_editor_internal_dock_layout_name_get(i);

    if (current_name != NULL && items[i] != NULL &&
        strcmp(items[i], current_name) == 0)
    {
      selected_index = i;
    }
  }

  // Before the first layout file is saved, the live dock is the compiled
  // default layout but the named-layout collection is still empty.
  if (layout_count == 0)
  {
    current_name = "default";
    items[0] = current_name;
    layout_count = 1;
  }
  else if (current_name == NULL)
  {
    current_name = items[0] != NULL ? items[0] : "default";
  }

  u32 save_layout_index = layout_count;
  items[save_layout_index] = "Save...";

  u32 save_layout_as_index = layout_count + 1;
  items[save_layout_as_index] = "Save as...";

  u32 item_count = layout_count + 2;
  u32 delete_layout_index = UINT32_MAX;

  if (strcmp(current_name, "default") != 0)
  {
    delete_layout_index = item_count;
    items[item_count++] = "Delete current layout...";
  }

  ldk_ui_set_next_width(ui, ldk_ui_px(150.0f));
  u32 result = ldk_ui_combo_box(ui, items, item_count, selected_index);

  if (result == selected_index)
  {
    return;
  }

  if (result < layout_count)
  {
    if (stored_layout_count > 0 &&
        ldk_editor_internal_dock_set_current(items[result]))
    {
      if (!s_editor_layouts_save())
      {
        ldk_editor_internal_log_error(editor,
            "Layout changed in memory, but failed to save the layout file.");
      }
    }
    else if (stored_layout_count > 0)
    {
      ldk_editor_internal_log_error(editor, "Failed to change layout.");
    }
  }
  else if (result == save_layout_index)
  {
    if (!s_editor_layouts_save())
    {
      ldk_editor_internal_log_error(editor, "Failed to save the layout.");
    }
    else
    {
      ldk_editor_internal_log_info(editor, "Layout saved.");
    }
  }
  else if (result == save_layout_as_index)
  {
    memset(editor->input_window_buffer, 0,
        sizeof(editor->input_window_buffer));
    editor->input_window_rect = (LDKUIRect){0};
    editor->show_input_window = true;
  }
  else if (result == delete_layout_index)
  {
    char message[X_SMALLSTR_MAX_LENGTH];
    snprintf(message, sizeof(message),
        "Delete the \"%s\" layout? This cannot be undone.", current_name);

    if (ldk_os_dialog_show_yes_no(
          editor->window, "Delete layout?", message))
    {
      if (!ldk_editor_internal_dock_layout_delete(current_name))
      {
        ldk_editor_internal_log_error(editor, "Failed to delete layout.");
      }
      else if (!s_editor_layouts_save())
      {
        ldk_editor_internal_log_error(editor,
            "Layout deleted in memory, but failed to save the layout file.");
      }
      else
      {
        ldk_editor_internal_log_info(editor, "Layout deleted.");
      }
    }
  }
}

static void s_editor_tool_bar(LDKEditorContext *editor)
{
  LDKUIContext *ui = &editor->ui;
  static LDKUIRect toolbar_rect = {0, LDK_UI_DEFAULT_CONTROL_HEIGHT, 0, 0};
  toolbar_rect.w = ui->viewport.w;
  toolbar_rect.h =
      LDK_UI_DEFAULT_CONTROL_HEIGHT + LDK_UI_DEFAULT_PADDING * 4.0f;

  toolbar_rect =
      ldk_ui_begin_window_fixed(ui, "EDITOR COMMANDS", toolbar_rect, 0);
  ldk_ui_begin_horizontal(&editor->ui);
  ldk_ui_spacer(ui);

  {
    LDKUIIcon icon;
    icon.size =
        ldk_sizef(LDK_UI_DEFAULT_CONTROL_HEIGHT, LDK_UI_DEFAULT_CONTROL_HEIGHT);
    icon.texture =
        ldk_renderer_texture_ui_handle(editor->renderer, editor->ui_atlas);

    // Play/Stop button
    if (editor->editor_state != LDK_EDITOR_STATE_PLAYING)
    {
      bool can_play = editor->project.loaded;
      ldk_ui_set_next_disabled(ui, !can_play);
      icon.uv = ldk_editor_icon_rects[LDK_EDITOR_ICON_BUTTON_PLAY];

      ldk_ui_set_next_weight(ui, 0.0f);
      if (ldk_ui_icon_button(ui, icon, NULL))
      {
        ldk_editor_state_set_play(editor);
      }
    }
    else
    {
      icon.uv = ldk_editor_icon_rects[LDK_EDITOR_ICON_BUTTON_STOP];
      ldk_ui_set_next_weight(ui, 0.0f);
      if (ldk_ui_icon_button(ui, icon, NULL))
      {
        ldk_editor_state_set_stop(editor);
      }
    }

    {
      bool can_pause = (editor->editor_state == LDK_EDITOR_STATE_PLAYING);
      ldk_ui_set_next_disabled(ui, !can_pause);
      icon.uv = ldk_editor_icon_rects[LDK_EDITOR_ICON_BUTTON_PAUSE];
      ldk_ui_set_next_weight(ui, 0.0f);
      if (ldk_ui_icon_button(ui, icon, NULL))
      {
        ldk_editor_state_set_pause(editor);
      }
    }

    {
      ldk_ui_set_next_disabled(
          ui, (editor->editor_state != LDK_EDITOR_STATE_PAUSED));
      icon.uv = ldk_editor_icon_rects[LDK_EDITOR_ICON_BUTTON_SKIP];
      ldk_ui_set_next_weight(ui, 0.0f);
      if (ldk_ui_icon_button(ui, icon, NULL))
      {
        ldk_editor_state_play_one_frame(editor);
      }
    }
  }

  ldk_ui_spacer(ui);
  s_editor_layout_combo_box(editor);
  ldk_ui_end_horizontal(&editor->ui);
  ldk_ui_end_window(ui);
}

//------------------------------------------------------------
// Hierarchy / Inspector
//------------------------------------------------------------

static u64 s_editor_entity_id(LDKEntity entity)
{
  u64 id = 0;
  size_t copy_size = sizeof(entity) < sizeof(id) ? sizeof(entity) : sizeof(id);
  memcpy(&id, &entity, copy_size);
  return id;
}

static bool s_editor_entity_equal(LDKEntity a, LDKEntity b)
{
  return memcmp(&a, &b, sizeof(a)) == 0;
}

static bool s_editor_selected_entity_get(
    LDKEditorContext *editor, LDKECS *ecs, LDKEntity *out_entity)
{
  if (!editor || !ecs ||
      !ldk_entity_is_alive(&ecs->entity, editor->selected_entity))
  {
    if (editor)
    {
      editor->selected_entity = x_handle_null();
    }
    return false;
  }

  if (out_entity)
  {
    *out_entity = editor->selected_entity;
  }
  return true;
}

static void s_editor_entity_display_name(
    const LDKEntityInfo *info, LDKEntity entity, char *out, size_t out_size)
{
  const char *name = NULL;

#if defined(_DEBUG) || defined(LDK_EDITOR)
  if (info && info->name[0] != 0)
  {
    name = (const char *)info->name;
  }
#endif

  if (name)
  {
    snprintf(out, out_size, "%s", name);
  }
  else
  {
    snprintf(out, out_size, "Entity 0x%016" PRIx64,
        s_editor_entity_id(entity));
  }
}

static bool s_editor_hierarchy_initialize(LDKEditorContext *editor)
{
  if (editor->hierarchy_expanded_entities == NULL)
  {
    editor->hierarchy_expanded_entities = x_array_create(sizeof(LDKEntity), 32);
  }

  return editor->hierarchy_expanded_entities != NULL;
}

static i32 s_editor_hierarchy_expanded_index(
    LDKEditorContext *editor, LDKEntity entity)
{
  if (editor->hierarchy_expanded_entities == NULL)
  {
    return -1;
  }

  for (u32 i = 0; i < x_array_count(editor->hierarchy_expanded_entities); ++i)
  {
    LDKEntity *expanded = x_array_get(editor->hierarchy_expanded_entities, i);
    if (expanded != NULL && s_editor_entity_equal(*expanded, entity))
    {
      return (i32)i;
    }
  }

  return -1;
}

static bool s_editor_hierarchy_expanded_get(
    LDKEditorContext *editor, LDKEntity entity)
{
  return s_editor_hierarchy_expanded_index(editor, entity) >= 0;
}

static void s_editor_hierarchy_expanded_set(
    LDKEditorContext *editor, LDKEntity entity, bool expanded)
{
  i32 index = s_editor_hierarchy_expanded_index(editor, entity);

  if (expanded)
  {
    if (index < 0)
    {
      x_array_add(editor->hierarchy_expanded_entities, &entity);
    }
    return;
  }

  if (index >= 0)
  {
    x_array_delete_at(editor->hierarchy_expanded_entities, (u32)index);
  }
}

static void s_editor_hierarchy_entity_draw(LDKEditorContext *editor,
    LDKECS *ecs, LDKEntity entity, u32 depth, LDKEntity *selected_entity,
    bool *has_selection)
{
  LDKUIContext *ui = &editor->ui;
  const LDKEntityInfo *info = ldk_entity_info_get(&ecs->entity, entity);
  const LDKTransform *transform = ldk_entity_transform_get_const(
      &ecs->entity, &ecs->component, entity);
  LDKEntity first_child = transform != NULL
                            ? transform->first_child
                            : x_handle_null();
  bool has_children = !x_handle_is_null(first_child) &&
                      ldk_entity_is_alive(&ecs->entity, first_child);
  bool expanded = has_children &&
                  s_editor_hierarchy_expanded_get(editor, entity);
  u32 flags = has_children ? LDK_UI_TREE_NODE_NONE : LDK_UI_TREE_NODE_LEAF;
  char label[LDK_ENTITY_NAME_MAX_LEN + 32];
  u64 id = s_editor_entity_id(entity);
  LDKUIIcon icon = {0};

  s_editor_entity_display_name(info, entity, label, sizeof(label));

  if (*has_selection && s_editor_entity_equal(*selected_entity, entity))
  {
    flags |= LDK_UI_TREE_NODE_SELECTED;
  }

  ldk_ui_push_id_u32(ui, (u32)id);
  ldk_ui_push_id_u32(ui, (u32)(id >> 32));

  u32 result = ldk_ui_tree_node_ex(
      ui, label, icon, expanded, depth, flags);

  if ((result & LDK_UI_TREE_NODE_RESULT_CLICKED) != 0)
  {
    editor->selected_entity = entity;
    *selected_entity = entity;
    *has_selection = true;
  }

  if ((result & LDK_UI_TREE_NODE_RESULT_TOGGLED) != 0)
  {
    expanded = !expanded;
    s_editor_hierarchy_expanded_set(editor, entity, expanded);
  }

  ldk_ui_pop_id(ui);
  ldk_ui_pop_id(ui);

  if (!has_children || !expanded)
  {
    return;
  }

  LDKEntity child = first_child;
  while (!x_handle_is_null(child))
  {
    if (!ldk_entity_is_alive(&ecs->entity, child))
    {
      break;
    }

    const LDKTransform *child_transform = ldk_entity_transform_get_const(
        &ecs->entity, &ecs->component, child);
    LDKEntity next_sibling = child_transform != NULL
                               ? child_transform->next_sibling
                               : x_handle_null();

    s_editor_hierarchy_entity_draw(editor, ecs, child, depth + 1,
        selected_entity, has_selection);

    child = next_sibling;
  }
}

static void s_editor_entity_list_window(LDKEditorContext *editor, LDKECS *ecs)
{
  LDKUIContext *ui = &editor->ui;
  static LDKUIRect window_rect = {10, 60, 100, 100};
  static LDKUIPoint scroll = {0};
  bool owns_window = ui->current_window == NULL;
  LDKEntity selected_entity = x_handle_null();
  bool has_selection = false;

  if (editor == NULL || ecs == NULL)
  {
    return;
  }

  has_selection = s_editor_selected_entity_get(
      editor, ecs, &selected_entity);

  if (owns_window)
  {
    window_rect = ldk_ui_begin_window(
      ui, "HIERARCHY", window_rect, LDK_UI_WINDOW_TOOL);
  }

  if (!s_editor_hierarchy_initialize(editor))
  {
    ldk_ui_label(ui, "Hierarchy allocation failed.");
    if (owns_window)
    {
      ldk_ui_end_window(ui);
    }
    return;
  }

  scroll = ldk_ui_begin_scrollview(
      ui, scroll, LDK_UI_SCROLL_VERTICAL | LDK_UI_SCROLL_IF_NEEDED);

  static bool systems_expanded = true;
  static bool entities_expanded = true;
  LDKUIIcon icon = {0};

  ldk_ui_push_id_cstr(ui, "systems");
  u32 systems_result = ldk_ui_tree_node_ex(
      ui, "Systems", icon, systems_expanded, 0, LDK_UI_TREE_NODE_NONE);
  if ((systems_result & LDK_UI_TREE_NODE_RESULT_TOGGLED) != 0)
  {
    systems_expanded = !systems_expanded;
  }
  ldk_ui_pop_id(ui);

  if (systems_expanded)
  {
    u32 system_count = ldk_system_registry_count(&ecs->system);
    for (u32 i = 0; i < system_count; ++i)
    {
      LDKSystemDesc desc = {0};
      if (!ldk_system_registry_at(&ecs->system, i, &desc))
      {
        continue;
      }

      ldk_ui_push_id_u32(ui, i);
      ldk_ui_tree_node_ex(ui, desc.name != NULL ? desc.name : "<unnamed system>",
          icon, false, 1, LDK_UI_TREE_NODE_LEAF);
      ldk_ui_pop_id(ui);
    }
  }

  ldk_ui_push_id_cstr(ui, "entity");
  u32 entities_result = ldk_ui_tree_node_ex(
      ui, "Entity", icon, entities_expanded, 0, LDK_UI_TREE_NODE_NONE);
  if ((entities_result & LDK_UI_TREE_NODE_RESULT_TOGGLED) != 0)
  {
    entities_expanded = !entities_expanded;
  }
  ldk_ui_pop_id(ui);

  if (entities_expanded)
  {
    LDKEntityIterator it = ldk_entity_iterator_begin(&ecs->entity);
    LDKEntity entity;

    while (ldk_entity_iterator_next(&it, &entity))
    {
      const LDKTransform *transform = ldk_entity_transform_get_const(
          &ecs->entity, &ecs->component, entity);

      if (transform != NULL && !x_handle_is_null(transform->parent) &&
          ldk_entity_is_alive(&ecs->entity, transform->parent))
      {
        continue;
      }

      s_editor_hierarchy_entity_draw(
          editor, ecs, entity, 1, &selected_entity, &has_selection);
    }

    ldk_entity_iterator_end(&it);
  }
  ldk_ui_spacer(ui);
  ldk_ui_end_scrollview(ui);

  if (owns_window)
  {
    ldk_ui_end_window(ui);
  }
}

static void s_editor_inspector_field_value_format(
    char *out, size_t out_size, const LDKComponentFieldMeta *field,
    const void *value)
{
  switch (field->type)
  {
    case LDK_FIELD_BOOL:
      snprintf(out, out_size, "%s", *(const bool *)value ? "true" : "false");
      break;
    case LDK_FIELD_I32:
      snprintf(out, out_size, "%d", *(const i32 *)value);
      break;
    case LDK_FIELD_U32:
      snprintf(out, out_size, "%u", *(const u32 *)value);
      break;
    case LDK_FIELD_FLOAT:
      snprintf(out, out_size, "%.4f", *(const float *)value);
      break;
    case LDK_FIELD_VEC2:
    {
      const Vec2 *v = (const Vec2 *)value;
      snprintf(out, out_size, "%.4f, %.4f", v->x, v->y);
      break;
    }
    case LDK_FIELD_VEC3:
    {
      const Vec3 *v = (const Vec3 *)value;
      snprintf(out, out_size, "%.4f, %.4f, %.4f", v->x, v->y, v->z);
      break;
    }
    case LDK_FIELD_VEC4:
    {
      const Vec4 *v = (const Vec4 *)value;
      snprintf(out, out_size, "%.4f, %.4f, %.4f, %.4f",
          v->x, v->y, v->z, v->w);
      break;
    }
    case LDK_FIELD_QUAT:
    {
      const Quat *q = (const Quat *)value;
      snprintf(out, out_size, "%.4f, %.4f, %.4f, %.4f",
          q->x, q->y, q->z, q->w);
      break;
    }
    case LDK_FIELD_MAT4:
      snprintf(out, out_size, "<mat4>");
      break;
    case LDK_FIELD_ENUM:
      snprintf(out, out_size, "<enum>");
      break;
    case LDK_FIELD_ENTITY:
    {
      LDKEntity entity;
      memcpy(&entity, value, sizeof(entity));
      snprintf(out, out_size, "0x%016" PRIx64, s_editor_entity_id(entity));
      break;
    }
    case LDK_FIELD_ASSET_MESH:
      snprintf(out, out_size, "<asset mesh>");
      break;
    case LDK_FIELD_RESOURCE_MESH:
      snprintf(out, out_size, "<resource mesh>");
      break;
    default:
      snprintf(out, out_size, "<unsupported>");
      break;
  }
}

static void s_editor_inspector_field_draw(LDKUIContext *ui,
    const LDKComponentMeta *meta, const LDKComponentFieldMeta *field,
    void *component)
{
  char value_text[128];
  u8 *field_value;
  bool readonly;

  if (!ui || !meta || !field || !component || field->offset >= meta->size)
  {
    return;
  }

  field_value = (u8 *)component + field->offset;
  readonly = (field->flags & LDK_FIELD_FLAG_READONLY) != 0;

  ldk_ui_push_id_cstr(ui, field->name);
  ldk_ui_set_next_height(ui, ldk_ui_px(LDK_UI_DEFAULT_CONTROL_HEIGHT));
  ldk_ui_begin_horizontal(ui);
  ldk_ui_set_next_width(ui, ldk_ui_px(110.0f));
  ldk_ui_label(ui, field->name);

  if (field->type == LDK_FIELD_BOOL)
  {
    bool value = *(bool *)field_value;
    ldk_ui_begin_disabled(ui, readonly);
    value = ldk_ui_toggle(ui, value);
    ldk_ui_end_disabled(ui);
    if (!readonly)
    {
      *(bool *)field_value = value;
    }
  }
  else if (field->type == LDK_FIELD_FLOAT &&
           field->widget == LDK_FIELD_WIDGET_SLIDER &&
           field->min_value < field->max_value)
  {
    float value = *(float *)field_value;
    ldk_ui_begin_disabled(ui, readonly);
    value = ldk_ui_slider(ui, value, field->min_value, field->max_value);
    ldk_ui_end_disabled(ui);
    if (!readonly)
    {
      *(float *)field_value = value;
    }
  }
  else
  {
    s_editor_inspector_field_value_format(
        value_text, sizeof(value_text), field, field_value);
    ldk_ui_begin_disabled(ui, readonly);
    ldk_ui_label(ui, value_text);
    ldk_ui_end_disabled(ui);
  }

  ldk_ui_end_horizontal(ui);
  ldk_ui_pop_id(ui);
}

void ldk_editor_internal_inspector_show(LDKEditorContext *editor)
{
  static LDKUIPoint scroll = {0};
  LDKECS *ecs;
  LDKGame *game;
  LDKEntity entity;
  LDKEntityInfo *info;
  char entity_name[LDK_ENTITY_NAME_MAX_LEN + 32];

  if (!editor)
  {
    return;
  }

  LDKUIContext *ui = &editor->ui;
  ecs = ldk_module_get(LDK_MODULE_ECS);
  game = ldk_game_get();

  if (!ecs || !game || !s_editor_selected_entity_get(editor, ecs, &entity))
  {
    ldk_ui_label(ui, "No entity selected.");
    return;
  }

  info = ldk_entity_info_get(&ecs->entity, entity);
  if (!info)
  {
    editor->selected_entity = x_handle_null();
    ldk_ui_label(ui, "No entity selected.");
    return;
  }

  s_editor_entity_display_name(info, entity, entity_name, sizeof(entity_name));
  ldk_ui_label(ui, entity_name);
  ldk_ui_horizontal_line(ui);

  scroll = ldk_ui_begin_scrollview(
      ui, scroll, LDK_UI_SCROLL_VERTICAL | LDK_UI_SCROLL_IF_NEEDED);

  for (u32 component_i = 0;
       component_i < info->components.component_count; component_i++)
  {
    u32 component_type = info->components.component_type[component_i];
    const LDKComponentMeta *meta =
        ldk_scene_component_meta_find_by_type(game, component_type);
    const char *component_name = meta
        ? meta->name
        : ldk_component_name_get(&ecs->component, component_type);
    void *component = ldk_ecs_component_get(entity, component_type);

    ldk_ui_push_id_u32(ui, component_type);
    ldk_ui_label(ui, component_name ? component_name : "<unknown component>");
    ldk_ui_horizontal_line(ui);

    if (!meta)
    {
      ldk_ui_label(ui, "Component metadata unavailable.");
    }
    else if (!component)
    {
      ldk_ui_label(ui, "Component data unavailable.");
    }
    else
    {
      /*
       * Inspector visibility follows Comet metadata. It intentionally does
       * not use scene serialization rules: runtime/non-serialized fields may
       * still be useful to inspect.
       */
      for (u32 field_i = 0; field_i < meta->field_count; field_i++)
      {
        s_editor_inspector_field_draw(
            ui, meta, &meta->fields[field_i], component);
      }
    }

    ldk_ui_spacer(ui);
    ldk_ui_pop_id(ui);
  }

  ldk_ui_end_scrollview(ui);
}

//------------------------------------------------------------
// Internal
//------------------------------------------------------------

void ldk_editor_internal_menubar_show(LDKEditorContext *editor)
{
  s_editor_menu_bar(editor);
}

void ldk_editor_internal_project_create_show(LDKEditorContext *editor)
{
  enum
  {
    PROJECT_TYPE_COUNT = 3,
  };

  static const char *s_project_types[PROJECT_TYPE_COUNT] = {
      "Placeholder 1",
      "Placeholder 2",
      "Placeholder 3",
  };

  static XSmallstr s_project_name = {0};
  static XFSPath s_project_path = {0};
  static u32 s_project_type = 0;

  static LDKUIRect s_window_rect = {0};
  static bool s_window_initialized = false;

  LDKUIContext *ui = &editor->ui;

  if (!s_window_initialized)
  {
    s_window_rect.w = 480.0f;
    s_window_rect.h = 210.0f;
    s_window_rect.x = (ui->viewport.w - s_window_rect.w) * 0.5f;
    s_window_rect.y = (ui->viewport.h - s_window_rect.h) * 0.5f;

    s_window_initialized = true;
  }

  s_window_rect = ldk_ui_begin_window(ui, "CREATE PROJECT", s_window_rect,
      LDK_UI_WINDOW_TITLE_BAR | LDK_UI_WINDOW_DRAGGABLE |
          LDK_UI_WINDOW_BORDER);

  //----------------------------------------------------------------------
  // Project name
  //----------------------------------------------------------------------

  ldk_ui_set_next_height(ui, ldk_ui_px(LDK_UI_DEFAULT_CONTROL_HEIGHT));
  ldk_ui_begin_horizontal(ui);
  {
    ldk_ui_set_next_width(ui, ldk_ui_px(100.0f));
    ldk_ui_label(ui, "Project name");

    ldk_ui_input_box(
        ui, s_project_name.buf, (u32)sizeof(s_project_name.buf));
  }
  ldk_ui_end_horizontal(ui);

  //----------------------------------------------------------------------
  // Project type
  //----------------------------------------------------------------------

  ldk_ui_set_next_height(ui, ldk_ui_px(LDK_UI_DEFAULT_CONTROL_HEIGHT));
  ldk_ui_begin_horizontal(ui);
  {
    ldk_ui_set_next_width(ui, ldk_ui_px(100.0f));
    ldk_ui_label(ui, "Project type");

    s_project_type = ldk_ui_combo_box(
        ui, s_project_types, PROJECT_TYPE_COUNT, s_project_type);
  }
  ldk_ui_end_horizontal(ui);

  //----------------------------------------------------------------------
  // Project path
  //----------------------------------------------------------------------

  ldk_ui_set_next_height(ui, ldk_ui_px(LDK_UI_DEFAULT_CONTROL_HEIGHT));
  ldk_ui_begin_horizontal(ui);
  {
    ldk_ui_set_next_width(ui, ldk_ui_px(100.0f));
    ldk_ui_label(ui, "Project path");

    ldk_ui_set_next_disabled(ui, true);
    ldk_ui_input_box(
        ui, s_project_path.buf, (u32)sizeof(s_project_path.buf));

    ldk_ui_set_next_width(ui, ldk_ui_px(32.0f));
    if (ldk_ui_button(ui, "..."))
    {
      ldk_os_dialog_show_open_folder(editor->window, "Project Location", "",
          s_project_path.buf, (u32)sizeof(s_project_path.buf));
    }
  }
  ldk_ui_end_horizontal(ui);

  //----------------------------------------------------------------------
  // Actions
  //----------------------------------------------------------------------

  ldk_ui_spacer(ui);
  ldk_ui_horizontal_line(ui);

  ldk_ui_set_next_height(ui, ldk_ui_px(LDK_UI_DEFAULT_CONTROL_HEIGHT));
  ldk_ui_begin_horizontal(ui);
  {
    ldk_ui_spacer(ui);

    ldk_ui_set_next_width(ui, ldk_ui_px(80.0f));
    if (ldk_ui_button(ui, "OK"))
    {
      editor->create_project_window_show = false;

      LDKProjectCreateDesc desc = {0};
      desc.project_name = s_project_name.buf;
      desc.project_root_path = s_project_path.buf;
      desc.cmake_generator = "Visual Studio 18 2026";

      bool success = ldk_project_create(&desc);

      if (success)
      {
        ldk_editor_internal_log_info(editor, "Project Created.");
      }
      else
      {
        ldk_editor_internal_log_error(editor, "Failed to create project.");
        ldk_os_dialog_show_error(editor->window, "Failed to create project",
            s_project_name.buf);
      }
    }

    ldk_ui_set_next_width(ui, ldk_ui_px(80.0f));
    if (ldk_ui_button(ui, "CANCEL"))
    {
      editor->create_project_window_show = false;
      x_smallstr_clear(&s_project_path);
    }
  }
  ldk_ui_end_horizontal(ui);

  ldk_ui_end_window(ui);
}

void ldk_editor_internal_toolbar_show(LDKEditorContext *editor)
{
  s_editor_tool_bar(editor);
}

void ldk_editor_internal_log_error(LDKEditorContext *editor, const char *msg)
{
  x_strbuilder_append_format(editor->console_sb, "%s\n", msg);
  ldk_log_error(msg);
}

void ldk_editor_internal_log_warning(LDKEditorContext *editor, const char *msg)
{
  x_strbuilder_append_format(editor->console_sb, "%s\n", msg);
  ldk_log_warning(msg);
}

void ldk_editor_internal_log_info(LDKEditorContext *editor, const char *msg)
{
  x_strbuilder_append_format(editor->console_sb, "%s\n", msg);
  ldk_log_info(msg);
}

//------------------------------------------------------------
// Public
//------------------------------------------------------------

void ldk_editor_console_show(LDKEditor *editor)
{
  s_editor_console(editor);
}

void ldk_editor_file_explorer_show(LDKEditor *editor, const char *root_path)
{
  s_editor_project_explorer((LDKEditorContext *)editor, root_path);
}

void ldk_editor_hierarchy_show(LDKEditor *editor, LDKECS *ecs)
{
  s_editor_entity_list_window((LDKEditorContext *)editor, ecs);
}
