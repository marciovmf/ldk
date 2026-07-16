#include "ldk_editor_internal.h"
#include <stdx/stdx_strbuilder.h>
#include <inttypes.h> // for PRIu64

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

static void s_project_explorer_entry_activate(
  ProjectExplorerState *state, const ProjectExplorerEntry *entry,
  bool is_directory)
{
  if (is_directory)
  {
    s_project_explorer_directory_select(state, &entry->path, true);
  }
  else
  {
    state->selected_file = entry->path;
  }
}

static void s_project_explorer_entries_draw(
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
        ldk_ui_icon_button(ui, is_directory ? folder_icon : file_icon);
      bool label_clicked = ldk_ui_button_flat(ui, entry->name.buf);

      ldk_ui_end_horizontal(ui);

      if (icon_clicked || label_clicked)
      {
        s_project_explorer_entry_activate(state, entry, is_directory);
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
        ldk_ui_icon_button(ui, is_directory ? folder_icon : file_icon);

      ldk_ui_set_next_height(ui, ldk_ui_px(LDK_UI_DEFAULT_CONTROL_HEIGHT));
      bool label_clicked = ldk_ui_button_flat(ui, entry->name.buf);

      ldk_ui_end_vertical(ui);

      if (icon_clicked || label_clicked)
      {
        s_project_explorer_entry_activate(state, entry, is_directory);
      }
    }

    ldk_ui_spacer(ui);
    ldk_ui_end_horizontal(ui);
  }
}

static void s_project_explorer_files_draw(
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
  s_project_explorer_entries_draw(state, ui, folder_icon, file_icon);

  ldk_ui_spacer(ui);
  ldk_ui_end_scrollview(ui);
  ldk_ui_end_vertical(ui);
}

static void s_editor_project_explorer(
  LDKEditorContext *editor, const char *root_path)
{
  ProjectExplorerState *state = &s_project_explorer_state;
  LDKUIContext *ui = &editor->ui;

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

  state->window_rect = ldk_ui_begin_window(
    ui, "Project Explorer", state->window_rect, LDK_UI_WINDOW_TOOL);

  if (!s_project_explorer_initialize(state))
  {
    ldk_ui_label(ui, "Project explorer allocation failed.");
    ldk_ui_end_window(ui);
    return;
  }

  if (!s_project_explorer_root_set(state, root_path))
  {
    ldk_ui_label(ui, "No project root.");
    ldk_ui_end_window(ui);
    return;
  }

  ldk_ui_begin_horizontal(ui);
  s_project_explorer_tree_draw(state, ui, tree_folder_icon);
  s_project_explorer_files_draw(state, ui, folder_icon, file_icon);
  ldk_ui_end_horizontal(ui);

  ldk_ui_end_window(ui);
}

//------------------------------------------------------------
// Console
//------------------------------------------------------------

static void s_editor_console(LDKEditorContext *editor)
{
  LDK_ASSERT(editor);
  LDK_ASSERT(editor->console_sb);

  static XSmallstr input = {0};
  LDKUIIcon icon;
  icon.size =
      ldk_sizef(LDK_UI_DEFAULT_CONTROL_HEIGHT, LDK_UI_DEFAULT_CONTROL_HEIGHT);
  icon.texture =
      ldk_renderer_texture_ui_handle(editor->renderer, editor->ui_atlas);

  LDKUIContext *ui = &editor->ui;
  static LDKUIRect s_entity_list_rect = {150, 90, 200, 180};
  s_entity_list_rect = ldk_ui_begin_window_fixed(
      ui, "CONSOLE", s_entity_list_rect, LDK_UI_WINDOW_TOOL);

  static LDKUIPoint scroll = {0};
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
    // scroll down
    scroll.y += 10000.0f;
  }

  ldk_ui_end_window(ui);
}

//------------------------------------------------------------
// Menu bar
//------------------------------------------------------------

static void s_editor_menu_bar(LDKEditorContext *editor)
{
  LDKUIContext *ui = &editor->ui;

  static LDKUIRect s_toolbar_rect = {0, 0, 0, 0};
  static LDKUIRect s_file_popup_rect = {0, 0, 1024, 1024};
  static LDKUIRect s_edit_popup_rect = {0, 0, 1024, 1024};
  static LDKUIRect s_theme_popup_rect = {0, 0, 1024, 1024};

  const LDKUIId MENU_ID_FILE = 10;
  const LDKUIId MENU_ID_EDIT = 11;
  const LDKUIId MENU_ID_THEME = 12;

  s_toolbar_rect.w = ui->viewport.w;
  s_toolbar_rect.h =
      LDK_UI_DEFAULT_CONTROL_HEIGHT + LDK_UI_DEFAULT_PADDING * 2.0f;

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

    if (ldk_ui_button_flat(ui, "New"))
    {
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

static void s_editor_tool_bar(LDKEditorContext *editor)
{
  LDKUIContext *ui = &editor->ui;
  static LDKUIRect toolbar_rect = {0, LDK_UI_DEFAULT_CONTROL_HEIGHT, 0, 0};
  toolbar_rect.w = ui->viewport.w;
  toolbar_rect.h =
      LDK_UI_DEFAULT_CONTROL_HEIGHT + LDK_UI_DEFAULT_PADDING * 2.0f;

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
      // Play
      bool can_play = editor->project.loaded;
      ldk_ui_set_next_disabled(ui, !can_play);
      icon.uv = ldk_editor_icon_rects[LDK_EDITOR_ICON_BUTTON_PLAY];

      ldk_ui_set_next_weight(ui, 0.0f);
      if (ldk_ui_icon_button(ui, icon))
      {
        ldk_editor_state_set_play(editor);
      }
    }
    else
    {
      icon.uv = ldk_editor_icon_rects[LDK_EDITOR_ICON_BUTTON_STOP];
      ldk_ui_set_next_weight(ui, 0.0f);
      if (ldk_ui_icon_button(ui, icon))
      {
        ldk_editor_state_set_stop(editor);
      }
    }

    { // Pause button
      bool can_pause = (editor->editor_state == LDK_EDITOR_STATE_PLAYING);
      ldk_ui_set_next_disabled(ui, !can_pause);
      icon.uv = ldk_editor_icon_rects[LDK_EDITOR_ICON_BUTTON_PAUSE];
      ldk_ui_set_next_weight(ui, 0.0f);
      if (ldk_ui_icon_button(ui, icon))
      {
        ldk_editor_state_set_pause(editor);
      }
    }

    { // Skip button
      bool can_skip = (editor->editor_state != LDK_EDITOR_STATE_PLAYING);
      ldk_ui_set_next_disabled(
          ui, (editor->editor_state != LDK_EDITOR_STATE_PAUSED));
      icon.uv = ldk_editor_icon_rects[LDK_EDITOR_ICON_BUTTON_SKIP];
      ldk_ui_set_next_weight(ui, 0.0f);
      if (ldk_ui_icon_button(ui, icon))
      {
        ldk_editor_state_play_one_frame(editor);
      }
    }
  }

  ldk_ui_spacer(ui);
  ldk_ui_end_horizontal(&editor->ui);
  ldk_ui_end_window(ui);
}

//------------------------------------------------------------
// Hierarchy
//------------------------------------------------------------

static void s_editor_entity_list_window(LDKEditorContext *editor, LDKECS *ecs)
{
  LDKUIContext *ui = &editor->ui;
  static LDKUIRect s_entity_list_rect = {10, 60, 100, 100};

  s_entity_list_rect = ldk_ui_begin_window(
      ui, "ENTITIES", s_entity_list_rect, LDK_UI_WINDOW_TOOL);
  LDKEntityIterator it = ldk_entity_iterator_begin(&ecs->entity);
  LDKEntity e;

  while (ldk_entity_iterator_next(&it, &e))
  {
    LDKEntityInfo *info = ldk_entity_info_get(&ecs->entity, e);
    u64 id = *((u64 *)&e);
    snprintf((char *const)&info->name, LDK_ENTITY_NAME_MAX_LEN,
        "0x%08" PRIu64 "(%d)", id, info->components.component_count);

    ldk_ui_label(ui, (const char *)info->name);
    for (u32 i = 0; i < info->components.component_count; i++)
    {
      const char *name = ldk_component_name_get(
          &ecs->component, info->components.component_type[i]);
      ldk_ui_label(ui, name);
    }
  }
  ldk_entity_iterator_end(&it);
  ldk_ui_end_window(ui);
}

//------------------------------------------------------------
// Internal
//------------------------------------------------------------

void ldk_editor_internal_menubar_show(LDKEditorContext *editor)
{
  s_editor_menu_bar(editor);
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

