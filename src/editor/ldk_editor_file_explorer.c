#include "ldk_editor_internal.h"
#include <ldk_scene.h>

#include <stdio.h>
#include <string.h>

enum
{
  PROJECT_EXPLORER_INITIAL_CAPACITY = 32,
  PROJECT_EXPLORER_TREE_ICON_SIZE = 20,
  PROJECT_EXPLORER_MIN_ICON_SIZE = 20,
  PROJECT_EXPLORER_TILE_LABEL_LINE_COUNT = 2,
  PROJECT_EXPLORER_CONTEXT_POPUP_ID = 0x50454301u,
  PROJECT_EXPLORER_TREE_RENAME_INPUT_ID = 0x54524901u,
  PROJECT_EXPLORER_TILE_RENAME_INPUT_ID = 0x54494901u,
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

typedef struct ProjectExplorerTileResult
{
  bool clicked;
  bool pressed;
  bool right_clicked;
} ProjectExplorerTileResult;

typedef struct ProjectExplorerFileIcon
{
  const char *extension;
  LDKEditorIcon atlas_id;
} ProjectExplorerFileIcon;

typedef enum ProjectExplorerSurface
{
  PROJECT_EXPLORER_SURFACE_TREE,
  PROJECT_EXPLORER_SURFACE_FILES,
} ProjectExplorerSurface;

typedef struct ProjectExplorerContextTarget
{
  XFSPath path;
  bool is_directory;
  ProjectExplorerSurface surface;
} ProjectExplorerContextTarget;

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
  ProjectExplorerContextTarget context_target;
  LDKUIRect rename_input_rect;
  LDKUIId rename_input_id;
  char rename_buffer[X_SMALLSTR_MAX_LENGTH + 1];
  bool rename_active;
  bool rename_focus_requested;
  bool rename_had_focus;
  bool root_expanded;
  float icon_size;
} ProjectExplorerState;

static ProjectExplorerState s_project_explorer_state = {
    .window_rect = {10.0f, 60.0f, 640.0f, 420.0f},
    .root_expanded = true,
    .icon_size = 48.0f,
};

static const ProjectExplorerFileIcon s_project_explorer_file_icons[] = {
    {"c", LDK_EDITOR_ICON_CODE},
    {"cc", LDK_EDITOR_ICON_CODE},
    {"cpp", LDK_EDITOR_ICON_CODE},
    {"cxx", LDK_EDITOR_ICON_CODE},
    {"h", LDK_EDITOR_ICON_CODE},
    {"hh", LDK_EDITOR_ICON_CODE},
    {"hpp", LDK_EDITOR_ICON_CODE},
    {"hxx", LDK_EDITOR_ICON_CODE},
    {"inl", LDK_EDITOR_ICON_CODE},
    {"glsl", LDK_EDITOR_ICON_CODE},
    {"vert", LDK_EDITOR_ICON_CODE},
    {"frag", LDK_EDITOR_ICON_CODE},
    {"png", LDK_EDITOR_ICON_IMAGE},
    {"jpg", LDK_EDITOR_ICON_IMAGE},
    {"jpeg", LDK_EDITOR_ICON_IMAGE},
    {"bmp", LDK_EDITOR_ICON_IMAGE},
    {"tga", LDK_EDITOR_ICON_IMAGE},
    {"gif", LDK_EDITOR_ICON_IMAGE},
    {"hdr", LDK_EDITOR_ICON_IMAGE},
    {"wav", LDK_EDITOR_ICON_AUDIO_FILE},
    {"ogg", LDK_EDITOR_ICON_AUDIO_FILE},
    {"mp3", LDK_EDITOR_ICON_AUDIO_FILE},
    {"flac", LDK_EDITOR_ICON_AUDIO_FILE},
    {"scene", LDK_EDITOR_ICON_HIERARCHY},
    {"ldk", LDK_EDITOR_ICON_DATA_OBJECT},
    {"tml", LDK_EDITOR_ICON_DATA_OBJECT},
    {"json", LDK_EDITOR_ICON_DATA_OBJECT},
    {"obj", LDK_EDITOR_ICON_OBJECT},
    {"fbx", LDK_EDITOR_ICON_OBJECT},
    {"gltf", LDK_EDITOR_ICON_OBJECT},
    {"glb", LDK_EDITOR_ICON_OBJECT},
};

static LDKEditorIcon s_project_explorer_file_icon_get(const XFSPath *path)
{
  XSlice extension;

  if (!path)
  {
    return LDK_EDITOR_ICON_FILE;
  }

  extension = x_fs_path_extension_as_slice(path);
  for (u32 i = 0; i < sizeof(s_project_explorer_file_icons) /
                          sizeof(s_project_explorer_file_icons[0]);
       i++)
  {
    if (x_slice_eq_ci(
            extension, x_slice(s_project_explorer_file_icons[i].extension)))
    {
      return s_project_explorer_file_icons[i].atlas_id;
    }
  }

  return LDK_EDITOR_ICON_FILE;
}

static void s_project_explorer_on_right_click(LDKUIContext *ui,
    ProjectExplorerState *state, const ProjectExplorerEntry *entry,
    bool is_directory, ProjectExplorerSurface surface);
static u32 s_project_explorer_rename_input_draw(LDKEditorContext *editor,
    ProjectExplorerState *state, LDKUIContext *ui, LDKUIId id, LDKUIRect rect);

static bool s_project_explorer_rename_matches(ProjectExplorerState *state,
    const XFSPath *path, ProjectExplorerSurface surface)
{
  return state && path && state->rename_active &&
         state->context_target.surface == surface &&
         x_fs_path_compare(&state->context_target.path, path) == 0;
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
    memset(&state->context_target, 0, sizeof(state->context_target));
    memset(&state->rename_input_rect, 0, sizeof(state->rename_input_rect));
    state->rename_input_id = 0;
    state->rename_buffer[0] = 0;
    state->rename_active = false;
    state->rename_focus_requested = false;
    state->rename_had_focus = false;
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
    x_array_add(state->expanded_paths, (XFSPath *)path);
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

  x_array_insert(entries, (XArray *)entry, insert_index);
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
  XFSDireHandle *dir = x_fs_find_first_file(x_fs_path_cstr(path), &fs_entry);

  while (dir != NULL)
  {
    bool special_entry =
        strcmp(fs_entry.name, ".") == 0 || strcmp(fs_entry.name, "..") == 0;
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

static bool s_project_explorer_rect_button_down(
    LDKUIContext *ui, LDKUIRect rect, LDKMouseButton button)
{
  LDKMouseState *mouse;
  LDKPoint cursor;

  if (!ui || !ui->mouse)
  {
    return false;
  }

  mouse = (LDKMouseState *)ui->mouse;
  cursor = ldk_os_mouse_cursor(mouse);
  return ldk_os_mouse_button_down(mouse, button) &&
         ldk_rectf_contains(&rect, (float)cursor.x, (float)cursor.y);
}

static bool s_project_explorer_icon_valid(LDKUIIcon icon)
{
  return icon.texture != 0 && icon.uv.w > 0.0f && icon.uv.h > 0.0f &&
         icon.size.w > 0.0f && icon.size.h > 0.0f;
}

static LDKUIRect s_project_explorer_tree_rename_rect(LDKUIContext *ui,
    LDKUIRect row_rect, const ProjectExplorerNode *node, LDKUIIcon folder_icon,
    bool expanded)
{
  LDKUIIcon chevron =
      ui->theme.icons[expanded ? LDK_UI_THEME_ICON_TREE_NODE_EXPANDED
                               : LDK_UI_THEME_ICON_TREE_NODE_COLLAPSED];
  float chevron_width = s_project_explorer_icon_valid(chevron)
                            ? chevron.size.w
                            : LDK_UI_TREE_NODE_CHEVRON_WIDTH;
  float input_x = row_rect.x +
                  (float)node->depth * LDK_UI_TREE_NODE_INDENT_WIDTH +
                  chevron_width + LDK_UI_DEFAULT_SPACING;

  if (s_project_explorer_icon_valid(folder_icon))
  {
    input_x += folder_icon.size.w + LDK_UI_DEFAULT_SPACING;
  }

  LDKUIRect input_rect = row_rect;
  input_rect.x = input_x;
  input_rect.w = row_rect.x + row_rect.w - input_x;
  if (input_rect.w < 0.0f)
  {
    input_rect.w = 0.0f;
  }

  return input_rect;
}

static bool s_project_explorer_tree_node(LDKEditorContext *editor,
    ProjectExplorerState *state, LDKUIContext *ui,
    const ProjectExplorerNode *node, LDKUIIcon folder_icon)
{
  i32 expanded_index =
      node->root ? -1
                 : s_project_explorer_expanded_path_index(state, &node->path);
  bool was_expanded = node->root ? state->root_expanded : expanded_index >= 0;
  u32 flags = 0;

  if (x_fs_path_compare(&state->selected_directory, &node->path) == 0)
  {
    flags |= LDK_UI_TREE_NODE_SELECTED;
  }

  bool renaming = s_project_explorer_rename_matches(
      state, &node->path, PROJECT_EXPLORER_SURFACE_TREE);
  if (renaming)
  {
    ldk_ui_set_next_width(ui, ldk_ui_fill());
  }

  u32 result = ldk_ui_tree_node_ex(ui, renaming ? "" : node->name.buf,
      folder_icon, was_expanded, node->depth, flags);
  LDKUIRect row_rect = ldk_ui_last_bounding_rect(ui);
  LDKUIId node_id = ui->last_id;
  bool expanded = was_expanded;

  if (renaming)
  {
    LDKUIRect input_rect = s_project_explorer_tree_rename_rect(
        ui, row_rect, node, folder_icon, was_expanded);
    s_project_explorer_rename_input_draw(editor, state, ui,
        node_id ^ PROJECT_EXPLORER_TREE_RENAME_INPUT_ID, input_rect);
  }
  else if (s_project_explorer_rect_button_down(
               ui, row_rect, LDK_MOUSE_BUTTON_RIGHT))
  {
    ProjectExplorerEntry entry = {0};
    entry.path = node->path;
    entry.name = node->name;
    s_project_explorer_on_right_click(
        ui, state, &entry, true, PROJECT_EXPLORER_SURFACE_TREE);
  }

  if (renaming)
  {
    return was_expanded;
  }

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
    x_array_add(state->expanded_paths, (ProjectExplorerNode *)&node->path);
  }
  else
  {
    x_array_delete_at(state->expanded_paths, (u32)expanded_index);
  }

  return expanded;
}

static void s_project_explorer_tree_draw(LDKEditorContext *editor,
    ProjectExplorerState *state, LDKUIContext *ui, LDKUIIcon folder_icon)
{
  ldk_ui_set_next_width(ui, ldk_ui_px(220.0f));
  state->tree_scroll = ldk_ui_begin_scrollview(
      ui, state->tree_scroll, LDK_UI_SCROLL_VERTICAL | LDK_UI_SCROLL_IF_NEEDED);

  x_array_clear(state->stack);

  ProjectExplorerNode root_node = {0};
  root_node.path = state->root;
  x_fs_path_basename(&state->root, &root_node.name);
  if (root_node.name.length == 0)
  {
    x_smallstr_from_cstr(&root_node.name, x_fs_path_cstr(&state->root));
  }
  root_node.root = true;
  x_array_add(state->stack, &root_node);

  while (x_array_count(state->stack) > 0)
  {
    u32 stack_index = x_array_count(state->stack) - 1;
    ProjectExplorerNode *node_ptr = x_array_get(state->stack, stack_index);
    ProjectExplorerNode node = *node_ptr;
    x_array_delete_at(state->stack, stack_index);

    if (!s_project_explorer_tree_node(editor, state, ui, &node, folder_icon))
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

static void s_project_explorer_on_file_double_click(
    LDKEditorContext *editor, const ProjectExplorerEntry *entry)
{
  if (!editor || !entry)
  {
    return;
  }

  if (ldki_editor_scene_path_is_scene(&entry->path))
  {
    ldki_editor_scene_load(editor, &entry->path);
  }
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
  bool double_click =
      same_file && state->last_click_ticks != 0 &&
      ldk_os_time_ticks_interval_get_seconds(state->last_click_ticks, now) <=
          PROJECT_EXPLORER_DOUBLE_CLICK_SECONDS;

  state->selected_file = entry->path;
  state->last_click_ticks = double_click ? 0 : now;

  if (double_click)
  {
    s_project_explorer_on_file_double_click(editor, entry);
  }
}

static void s_project_explorer_on_right_click(LDKUIContext *ui,
    ProjectExplorerState *state, const ProjectExplorerEntry *entry,
    bool is_directory, ProjectExplorerSurface surface)
{
  LDKPoint cursor;
  LDKUIPoint position;

  if (!ui || !ui->mouse || !state || !entry || state->rename_active)
  {
    return;
  }

  state->context_target.path = entry->path;
  state->context_target.is_directory = is_directory;
  state->context_target.surface = surface;
  state->last_click_ticks = 0;

  cursor = ldk_os_mouse_cursor((LDKMouseState *)ui->mouse);
  position.x = (float)cursor.x;
  position.y = (float)cursor.y;

  ldk_ui_close_all_popups(ui);
  ldk_ui_open_popup_at(ui, PROJECT_EXPLORER_CONTEXT_POPUP_ID, position);
}

static bool s_project_explorer_path_name_valid(const char *name)
{
  if (!name || name[0] == 0 || strcmp(name, ".") == 0 ||
      strcmp(name, "..") == 0)
  {
    return false;
  }

  for (const char *cursor = name; *cursor != 0; cursor++)
  {
    if (*cursor == '/' || *cursor == '\\' || *cursor == ':' || *cursor == '*' ||
        *cursor == '?' || *cursor == '"' || *cursor == '<' || *cursor == '>' ||
        *cursor == '|')
    {
      return false;
    }
  }

  return true;
}

static bool s_project_explorer_duplicate_path_get(
    const XFSPath *path, bool is_directory, u32 copy_index, XFSPath *out_path)
{
  XFSPath directory = {0};
  XSlice basename;
  XSlice extension;
  size_t stem_length;
  char name[X_SMALLSTR_MAX_LENGTH + 1];
  int length;

  if (!path || !out_path || copy_index == 0 ||
      x_fs_path_dirname(path, &directory) == 0)
  {
    return false;
  }

  basename = x_fs_path_basename_as_slice(path);
  extension = is_directory ? x_slice("") : x_fs_path_extension_as_slice(path);
  stem_length = basename.length;

  if (extension.length > 0 && basename.length > extension.length + 1)
  {
    stem_length -= extension.length + 1;
  }
  else
  {
    extension.length = 0;
  }

  if (copy_index == 1)
  {
    length = snprintf(name, sizeof(name), "%.*s copy%s%.*s", (int)stem_length,
        basename.ptr, extension.length > 0 ? "." : "", (int)extension.length,
        extension.ptr);
  }
  else
  {
    length = snprintf(name, sizeof(name), "%.*s copy %u%s%.*s",
        (int)stem_length, basename.ptr, copy_index,
        extension.length > 0 ? "." : "", (int)extension.length, extension.ptr);
  }

  if (length <= 0 || (size_t)length >= sizeof(name))
  {
    return false;
  }

  return x_fs_path(out_path, x_fs_path_cstr(&directory), name);
}

static bool s_project_explorer_path_duplicate(
    const XFSPath *path, bool is_directory, XFSPath *out_path)
{
  XFSPath destination = {0};

  if (!path || !out_path || !x_fs_path_exists(path))
  {
    return false;
  }

  for (u32 copy_index = 1; copy_index < 1000; copy_index++)
  {
    if (!s_project_explorer_duplicate_path_get(
            path, is_directory, copy_index, &destination))
    {
      return false;
    }

    if (!x_fs_path_exists(&destination))
    {
      bool copied = is_directory
                        ? x_fs_directory_copy(path->buf, destination.buf)
                        : x_fs_file_copy(path->buf, destination.buf);

      if (copied)
      {
        *out_path = destination;
      }
      return copied;
    }
  }

  return false;
}

static bool s_project_explorer_path_rename(const XFSPath *path,
    bool is_directory, const char *new_name, XFSPath *out_path)
{
  XFSPath directory = {0};
  XFSPath destination = {0};

  if (!path || !out_path || !s_project_explorer_path_name_valid(new_name) ||
      x_fs_path_dirname(path, &directory) == 0 ||
      !x_fs_path(&destination, directory.buf, new_name))
  {
    return false;
  }

  if (x_fs_path_compare(path, &destination) == 0)
  {
    *out_path = *path;
    return true;
  }

  if (x_fs_path_exists(&destination))
  {
    return false;
  }

  bool renamed = is_directory
                     ? x_fs_directory_rename(path->buf, destination.buf)
                     : x_fs_file_rename(path->buf, destination.buf);
  if (!renamed)
  {
    return false;
  }

  *out_path = destination;
  return true;
}

static void s_project_explorer_rename_end(ProjectExplorerState *state)
{
  if (!state)
  {
    return;
  }

  state->rename_active = false;
  state->rename_focus_requested = false;
  state->rename_had_focus = false;
  state->rename_input_id = 0;
  memset(&state->rename_input_rect, 0, sizeof(state->rename_input_rect));
  state->rename_buffer[0] = 0;
}

static bool s_project_explorer_rename_commit(
    LDKEditorContext *editor, ProjectExplorerState *state)
{
  XFSPath old_path;
  XFSPath renamed = {0};

  if (!editor || !state || !state->rename_active)
  {
    return false;
  }

  old_path = state->context_target.path;
  if (!s_project_explorer_path_rename(&old_path,
          state->context_target.is_directory, state->rename_buffer, &renamed))
  {
    ldki_editor_log_error(editor, "Failed to rename path.");
    state->rename_focus_requested = true;
    state->rename_had_focus = false;
    return false;
  }

  state->context_target.path = renamed;

  if (state->context_target.is_directory)
  {
    s_project_explorer_directory_select(state, &renamed, false);
    x_array_clear(state->expanded_paths);
  }
  else if (x_fs_path_compare(&state->selected_file, &old_path) == 0)
  {
    state->selected_file = renamed;
  }

  s_project_explorer_rename_end(state);
  return true;
}

static void s_project_explorer_rename_begin(ProjectExplorerState *state)
{
  XFSPath basename = {0};

  if (!state || x_fs_path_basename(&state->context_target.path, &basename) == 0)
  {
    return;
  }

  snprintf(
      state->rename_buffer, sizeof(state->rename_buffer), "%s", basename.buf);
  state->rename_active = true;
  state->rename_focus_requested = true;
  state->rename_had_focus = false;
  state->rename_input_id = 0;
  memset(&state->rename_input_rect, 0, sizeof(state->rename_input_rect));
}

static void s_project_explorer_rename_input_prepare(
    ProjectExplorerState *state, LDKUIContext *ui)
{
  if (state->rename_focus_requested)
  {
    ldk_ui_set_next_focus(ui);
    state->rename_focus_requested = false;
  }
}

static u32 s_project_explorer_rename_input_finish(LDKEditorContext *editor,
    ProjectExplorerState *state, LDKUIContext *ui, LDKUIId id, LDKUIRect rect,
    u32 result)
{
  state->rename_input_id = id;
  state->rename_input_rect = rect;

  if (ui->focused_id == id)
  {
    state->rename_had_focus = true;
  }

  if ((result & LDK_UI_INPUT_BOX_CANCELED) != 0)
  {
    s_project_explorer_rename_end(state);
  }
  else if ((result & LDK_UI_INPUT_BOX_COMMITTED) != 0)
  {
    s_project_explorer_rename_commit(editor, state);
  }

  return result;
}

static u32 s_project_explorer_rename_input_draw(LDKEditorContext *editor,
    ProjectExplorerState *state, LDKUIContext *ui, LDKUIId id, LDKUIRect rect)
{
  if (!editor || !state || !ui || !state->rename_active)
  {
    return LDK_UI_INPUT_BOX_NONE;
  }

  s_project_explorer_rename_input_prepare(state, ui);
  u32 result = ldk_ui_widget_input_label(
      ui, id, state->rename_buffer, (u32)sizeof(state->rename_buffer), rect);
  return s_project_explorer_rename_input_finish(
      editor, state, ui, id, rect, result);
}

static u32 s_project_explorer_rename_input_layout_draw(
    LDKEditorContext *editor, ProjectExplorerState *state, LDKUIContext *ui)
{
  if (!editor || !state || !ui || !state->rename_active)
  {
    return LDK_UI_INPUT_BOX_NONE;
  }

  s_project_explorer_rename_input_prepare(state, ui);
  u32 result = ldk_ui_input_label(
      ui, state->rename_buffer, (u32)sizeof(state->rename_buffer));
  return s_project_explorer_rename_input_finish(
      editor, state, ui, ui->last_id, ldk_ui_last_bounding_rect(ui), result);
}

static void s_project_explorer_rename_before_draw(
    LDKEditorContext *editor, ProjectExplorerState *state, LDKUIContext *ui)
{
  if (!editor || !state || !ui || !state->rename_active ||
      !state->rename_had_focus)
  {
    return;
  }

  bool focus_lost = ui->focused_id != state->rename_input_id;
  bool clicked_outside = false;

  if (ui->mouse && ldk_os_mouse_button_down(
                       (LDKMouseState *)ui->mouse, LDK_MOUSE_BUTTON_LEFT))
  {
    LDKPoint cursor = ldk_os_mouse_cursor((LDKMouseState *)ui->mouse);
    clicked_outside = !ldk_rectf_contains(
        &state->rename_input_rect, (float)cursor.x, (float)cursor.y);
  }

  if (focus_lost || clicked_outside)
  {
    s_project_explorer_rename_commit(editor, state);
  }
}

static void s_project_explorer_context_menu_draw(
    LDKEditorContext *editor, ProjectExplorerState *state, LDKUIContext *ui)
{
  if (ldk_ui_begin_popup(ui, PROJECT_EXPLORER_CONTEXT_POPUP_ID))
  {
    bool is_directory = state->context_target.is_directory;
    bool is_root =
        x_fs_path_compare(&state->root, &state->context_target.path) == 0;

    if (ldk_ui_button_flat(ui, is_directory ? "Open Folder" : "Open"))
    {
      if (is_directory)
      {
        s_project_explorer_directory_select(
            state, &state->context_target.path, true);
      }
      else
      {
        ProjectExplorerEntry entry = {0};
        entry.path = state->context_target.path;
        s_project_explorer_on_file_double_click(editor, &entry);
      }
      ldk_ui_close_current_popup(ui);
    }

    if (ldk_ui_button_flat(ui, "Copy Path"))
    {
      if (!ldk_os_clipboard_text_set(
              editor->window, state->context_target.path.buf))
      {
        ldki_editor_log_error(editor, "Failed to copy path.");
      }
      ldk_ui_close_current_popup(ui);
    }

    ldk_ui_begin_disabled(ui, is_root);

    if (ldk_ui_button_flat(ui, "Duplicate"))
    {
      XFSPath duplicate = {0};

      if (!s_project_explorer_path_duplicate(
              &state->context_target.path, is_directory, &duplicate))
      {
        ldki_editor_log_error(editor, "Failed to duplicate path.");
      }
      else if (!is_directory)
      {
        state->selected_file = duplicate;
      }
      ldk_ui_close_current_popup(ui);
    }

    if (ldk_ui_button_flat(ui, "Rename"))
    {
      s_project_explorer_rename_begin(state);
      ldk_ui_close_current_popup(ui);
    }

    if (ldk_ui_button_flat(ui, "Delete"))
    {
      const char *message = is_directory
                                ? "Delete this folder and all its contents?"
                                : "Delete this file?";

      if (ldk_os_dialog_show_yes_no(editor->window, "Delete path?", message))
      {
        bool deleted = is_directory
                           ? x_fs_directory_delete_recursive(
                                 state->context_target.path.buf)
                           : x_fs_file_delete(state->context_target.path.buf);

        if (!deleted)
        {
          ldki_editor_log_error(editor, "Failed to delete path.");
        }
        else if (is_directory)
        {
          XFSPath parent = {0};
          if (x_fs_path_dirname(&state->context_target.path, &parent) > 0)
          {
            s_project_explorer_directory_select(state, &parent, false);
          }
          x_array_clear(state->expanded_paths);
        }
        else if (x_fs_path_compare(
                     &state->selected_file, &state->context_target.path) == 0)
        {
          memset(&state->selected_file, 0, sizeof(state->selected_file));
        }
      }
      ldk_ui_close_current_popup(ui);
    }

    ldk_ui_end_disabled(ui);
    ldk_ui_end_popup(ui);
  }
}

static u32 s_project_explorer_text_prefix_fit(
    LDKFontInstance *font, const char *text, float max_width)
{
  const char *cursor = text;
  const char *last_fit = text;

  if (font == NULL || text == NULL || max_width <= 0.0f)
  {
    return 0;
  }

  while (*cursor != '\0')
  {
    const char *next = cursor;
    u32 codepoint = 0;

    if (!ldk_ttf_utf8_consume_codepoint(&next, &codepoint) || next <= cursor)
    {
      break;
    }

    u32 byte_count = (u32)(next - text);
    LDKTextSize text_size = ldk_ttf_measure_text_cstrn(font, text, byte_count);

    if (text_size.w > max_width)
    {
      break;
    }

    last_fit = next;
    cursor = next;
  }

  return (u32)(last_fit - text);
}

static void s_project_explorer_text_copy(
    char *destination, u32 capacity, const char *source, u32 length)
{
  if (destination == NULL || capacity == 0)
  {
    return;
  }

  destination[0] = '\0';

  if (source == NULL)
  {
    return;
  }

  if (length >= capacity)
  {
    length = capacity - 1;
  }

  memcpy(destination, source, length);
  destination[length] = '\0';
}

static u32 s_project_explorer_tile_text_wrap(LDKUIContext *ui, const char *text,
    float max_width, char *first_line, u32 first_capacity, char *second_line,
    u32 second_capacity)
{
  static const char ellipsis[] = "...";
  const u32 ellipsis_length = (u32)(sizeof(ellipsis) - 1);

  if (first_line == NULL || first_capacity == 0 || second_line == NULL ||
      second_capacity == 0)
  {
    return 0;
  }

  first_line[0] = '\0';
  second_line[0] = '\0';

  if (ui == NULL || ui->font == NULL || text == NULL || text[0] == '\0')
  {
    return 0;
  }

  u32 text_length = (u32)strlen(text);
  LDKTextSize text_size = ldk_ttf_measure_text_cstr(ui->font, text);

  if (text_size.w <= max_width)
  {
    s_project_explorer_text_copy(first_line, first_capacity, text, text_length);
    return 1;
  }

  u32 first_length =
      s_project_explorer_text_prefix_fit(ui->font, text, max_width);
  s_project_explorer_text_copy(first_line, first_capacity, text, first_length);

  const char *remaining = text + first_length;
  while (*remaining == ' ' || *remaining == '\t')
  {
    remaining++;
  }

  if (*remaining == '\0')
  {
    return first_line[0] != '\0' ? 1 : 0;
  }

  u32 remaining_length = (u32)strlen(remaining);
  text_size = ldk_ttf_measure_text_cstr(ui->font, remaining);

  if (text_size.w <= max_width)
  {
    s_project_explorer_text_copy(
        second_line, second_capacity, remaining, remaining_length);
    return first_line[0] != '\0' ? 2 : 1;
  }

  LDKTextSize ellipsis_size = ldk_ttf_measure_text_cstr(ui->font, ellipsis);
  float second_width = max_width - ellipsis_size.w;
  u32 second_length =
      s_project_explorer_text_prefix_fit(ui->font, remaining, second_width);
  u32 second_prefix_capacity = second_capacity - 1;

  if (second_prefix_capacity >= ellipsis_length)
  {
    second_prefix_capacity -= ellipsis_length;
  }
  else
  {
    second_prefix_capacity = 0;
  }

  if (second_length > second_prefix_capacity)
  {
    second_length = second_prefix_capacity;

    while (
        second_length > 0 && (((u8)remaining[second_length] & 0xC0u) == 0x80u))
    {
      second_length--;
    }
  }

  s_project_explorer_text_copy(
      second_line, second_capacity, remaining, second_length);

  if (second_capacity - (u32)strlen(second_line) > ellipsis_length)
  {
    strcat(second_line, ellipsis);
  }

  return first_line[0] != '\0' ? 2 : 1;
}

static void s_project_explorer_tile_label_draw(LDKUIContext *ui,
    const char *text, LDKUIRect tile_rect, float label_y, float line_height)
{
  char lines[PROJECT_EXPLORER_TILE_LABEL_LINE_COUNT]
            [sizeof(((ProjectExplorerEntry *)0)->name.buf)] = {0};
  float max_width = tile_rect.w - LDK_UI_DEFAULT_SPACING * 2.0f;
  u32 line_count = s_project_explorer_tile_text_wrap(ui, text, max_width,
      lines[0], (u32)sizeof(lines[0]), lines[1], (u32)sizeof(lines[1]));

  for (u32 line_i = 0; line_i < line_count; line_i++)
  {
    LDKTextSize text_size = ldk_ttf_measure_text_cstr(ui->font, lines[line_i]);
    LDKUIRect line_rect = {
        tile_rect.x + (tile_rect.w - text_size.w) * 0.5f,
        label_y + line_height * (float)line_i,
        text_size.w,
        line_height,
    };

    ldk_ui_widget_label(ui, 0, lines[line_i], line_rect);
  }
}

static ProjectExplorerTileResult s_project_explorer_tile(
    LDKEditorContext *editor, ProjectExplorerState *state, LDKUIContext *ui,
    const ProjectExplorerEntry *entry, LDKUIIcon icon, float tile_width,
    float tile_height, float line_height)
{
  ProjectExplorerTileResult result = {0};

  if (ui == NULL || entry == NULL)
  {
    return result;
  }

  ldk_ui_push_id_cstr(ui, x_fs_path_cstr(&entry->path));

  ldk_ui_set_next_size(ui, ldk_ui_px(tile_width), ldk_ui_px(tile_height));
  result.clicked = ldk_ui_button_flat(ui, "");
  LDKUIRect tile_rect = ldk_ui_last_rect(ui);
  LDKUIRect tile_bounding_rect = ldk_ui_last_bounding_rect(ui);
  LDKUIId tile_id = ui->last_id;
  result.pressed = ui->mouse != NULL && ui->active_id == tile_id &&
                   ldk_os_mouse_button_down(
                       (LDKMouseState *)ui->mouse, LDK_MOUSE_BUTTON_LEFT);
  result.right_clicked = s_project_explorer_rect_button_down(
      ui, tile_bounding_rect, LDK_MOUSE_BUTTON_RIGHT);

  LDKUIRect icon_rect = {
      tile_rect.x + (tile_rect.w - icon.size.w) * 0.5f,
      tile_rect.y + LDK_UI_DEFAULT_SPACING,
      icon.size.w,
      icon.size.h,
  };

  ldk_ui_widget_icon_label(ui, 0, icon, "", icon_rect);

  float label_y = icon_rect.y + icon_rect.h + LDK_UI_DEFAULT_SPACING;
  if (s_project_explorer_rename_matches(
          state, &entry->path, PROJECT_EXPLORER_SURFACE_FILES))
  {
    LDKUIRect input_rect = {tile_rect.x + LDK_UI_DEFAULT_SPACING, label_y,
        tile_rect.w - LDK_UI_DEFAULT_SPACING * 2.0f, line_height};
    s_project_explorer_rename_input_draw(editor, state, ui,
        tile_id ^ PROJECT_EXPLORER_TILE_RENAME_INPUT_ID, input_rect);
  }
  else
  {
    s_project_explorer_tile_label_draw(
        ui, entry->name.buf, tile_rect, label_y, line_height);
  }

  ui->last_rect = tile_rect;
  ui->last_bounding_rect = tile_bounding_rect;
  ui->last_id = tile_id;

  ldk_ui_pop_id(ui);
  return result;
}

static bool s_project_explorer_entries_draw(LDKEditorContext *editor,
    ProjectExplorerState *state, LDKUIContext *ui, LDKUIIcon folder_icon,
    LDKUIIcon file_icon)
{
  u32 total_count = x_array_count(state->dirs) + x_array_count(state->files);
  bool compact_mode = state->icon_size <= PROJECT_EXPLORER_MIN_ICON_SIZE;
  bool right_click_handled = false;

  if (compact_mode)
  {
    for (u32 entry_i = 0; entry_i < total_count; entry_i++)
    {
      bool is_directory = false;
      ProjectExplorerEntry *entry =
          s_project_explorer_entry_get(state, entry_i, &is_directory);
      LDKUIIcon entry_icon = is_directory ? folder_icon : file_icon;
      LDKUIMark entry_mark = ldk_ui_mark(ui);

      if (!is_directory)
      {
        entry_icon.uv = ldk_editor_icon_rects[s_project_explorer_file_icon_get(
            &entry->path)];
      }

      ldk_ui_set_next_height(ui, ldk_ui_px(LDK_UI_DEFAULT_CONTROL_HEIGHT));
      ldk_ui_begin_horizontal(ui);

      ldk_ui_set_next_weight(ui, 0.0f);
      bool icon_clicked = ldk_ui_icon_button(ui, entry_icon, NULL);
      bool renaming = s_project_explorer_rename_matches(
          state, &entry->path, PROJECT_EXPLORER_SURFACE_FILES);
      bool label_clicked = false;
      if (renaming)
      {
        s_project_explorer_rename_input_layout_draw(editor, state, ui);
      }
      else
      {
        label_clicked = ldk_ui_button_flat(ui, entry->name.buf);
      }

      ldk_ui_end_horizontal(ui);

      bool right_clicked = s_project_explorer_rect_button_down(
          ui, ldk_ui_measure_from(ui, entry_mark), LDK_MOUSE_BUTTON_RIGHT);

      if (icon_clicked || label_clicked)
      {
        s_project_explorer_entry_activate(editor, state, entry, is_directory);
      }

      if (right_clicked)
      {
        s_project_explorer_on_right_click(
            ui, state, entry, is_directory, PROJECT_EXPLORER_SURFACE_FILES);
        right_click_handled = true;
      }
    }

    return right_click_handled;
  }

  float tile_w = state->icon_size + 32.0f;
  float line_height = ldk_ttf_get_line_height(ui->font);
  float tile_h = state->icon_size +
                 line_height * PROJECT_EXPLORER_TILE_LABEL_LINE_COUNT +
                 LDK_UI_DEFAULT_SPACING * 3.0f;
  float available_w =
      ui->current_layout != NULL ? ui->current_layout->content_rect.w : tile_w;
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

    for (u32 column = 0; column < column_count && tile_index < total_count;
         column++, tile_index++)
    {
      bool is_directory = false;
      ProjectExplorerEntry *entry =
          s_project_explorer_entry_get(state, tile_index, &is_directory);
      LDKUIIcon entry_icon = is_directory ? folder_icon : file_icon;

      if (!is_directory)
      {
        entry_icon.uv = ldk_editor_icon_rects[s_project_explorer_file_icon_get(
            &entry->path)];
      }

      ProjectExplorerTileResult result = s_project_explorer_tile(
          editor, state, ui, entry, entry_icon, tile_w, tile_h, line_height);

      if ((is_directory && result.clicked) || (!is_directory && result.pressed))
      {
        s_project_explorer_entry_activate(editor, state, entry, is_directory);
      }

      if (result.right_clicked)
      {
        s_project_explorer_on_right_click(
            ui, state, entry, is_directory, PROJECT_EXPLORER_SURFACE_FILES);
        right_click_handled = true;
      }
    }

    ldk_ui_spacer(ui);
    ldk_ui_end_horizontal(ui);
  }

  return right_click_handled;
}

static void s_project_explorer_files_draw(LDKEditorContext *editor,
    ProjectExplorerState *state, LDKUIContext *ui, LDKUIIcon folder_icon,
    LDKUIIcon file_icon)
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
      ui, state->file_scroll, LDK_UI_SCROLL_VERTICAL | LDK_UI_SCROLL_IF_NEEDED);
  LDKUIRect file_view_rect = ui->clip_rect;

  s_project_explorer_directory_read(
      &state->selected_directory, state->dirs, state->files);
  bool right_click_handled = s_project_explorer_entries_draw(
      editor, state, ui, folder_icon, file_icon);

  if (!right_click_handled && s_project_explorer_rect_button_down(
                                  ui, file_view_rect, LDK_MOUSE_BUTTON_RIGHT))
  {
    ProjectExplorerEntry directory_entry = {0};
    directory_entry.path = state->selected_directory;
    x_fs_path_basename(&directory_entry.path, &directory_entry.name);
    s_project_explorer_on_right_click(
        ui, state, &directory_entry, true, PROJECT_EXPLORER_SURFACE_TREE);
  }

  ldk_ui_spacer(ui);
  ldk_ui_end_scrollview(ui);
  s_project_explorer_context_menu_draw(editor, state, ui);
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

  s_project_explorer_rename_before_draw(editor, state, ui);

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
  s_project_explorer_tree_draw(editor, state, ui, tree_folder_icon);
  s_project_explorer_files_draw(editor, state, ui, folder_icon, file_icon);
  ldk_ui_end_horizontal(ui);

  if (owns_window)
  {
    ldk_ui_end_window(ui);
  }
}

void ldk_editor_file_explorer_show(LDKEditor *editor, const char *root_path)
{
  s_editor_project_explorer((LDKEditorContext *)editor, root_path);
}
