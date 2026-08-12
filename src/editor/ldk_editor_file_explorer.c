#include "ldk_editor_internal.h"
#include <ldk_scene.h>

enum{
    PROJECT_EXPLORER_INITIAL_CAPACITY = 32,
    PROJECT_EXPLORER_TREE_ICON_SIZE = 20,
    PROJECT_EXPLORER_MIN_ICON_SIZE = 20,
    PROJECT_EXPLORER_TILE_LABEL_LINE_COUNT = 2,
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
} ProjectExplorerTileResult;

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

static bool s_project_explorer_tree_node(ProjectExplorerState *state,
    LDKUIContext *ui, const ProjectExplorerNode *node, LDKUIIcon folder_icon)
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
    x_array_add(state->expanded_paths, (ProjectExplorerNode *)&node->path);
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
  bool double_click =
      same_file && state->last_click_ticks != 0 &&
      ldk_os_time_ticks_interval_get_seconds(state->last_click_ticks, now) <=
          PROJECT_EXPLORER_DOUBLE_CLICK_SECONDS;

  state->selected_file = entry->path;
  state->last_click_ticks = double_click ? 0 : now;

  if (double_click && ldki_editor_scene_path_is_scene(&entry->path))
  {
    ldki_editor_scene_load(editor, &entry->path);
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

static ProjectExplorerTileResult s_project_explorer_tile(LDKUIContext *ui,
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

  LDKUIRect icon_rect = {
      tile_rect.x + (tile_rect.w - icon.size.w) * 0.5f,
      tile_rect.y + LDK_UI_DEFAULT_SPACING,
      icon.size.w,
      icon.size.h,
  };

  ldk_ui_widget_icon_label(ui, 0, icon, "", icon_rect);

  float label_y = icon_rect.y + icon_rect.h + LDK_UI_DEFAULT_SPACING;
  s_project_explorer_tile_label_draw(
      ui, entry->name.buf, tile_rect, label_y, line_height);

  ui->last_rect = tile_rect;
  ui->last_bounding_rect = tile_bounding_rect;
  ui->last_id = tile_id;

  ldk_ui_pop_id(ui);
  return result;
}

static void s_project_explorer_entries_draw(LDKEditorContext *editor,
    ProjectExplorerState *state, LDKUIContext *ui, LDKUIIcon folder_icon,
    LDKUIIcon file_icon)
{
  u32 total_count = x_array_count(state->dirs) + x_array_count(state->files);
  bool compact_mode = state->icon_size <= PROJECT_EXPLORER_MIN_ICON_SIZE;

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

      ProjectExplorerTileResult result = s_project_explorer_tile(ui, entry,
          is_directory ? folder_icon : file_icon, tile_w, tile_h, line_height);

      if ((is_directory && result.clicked) || (!is_directory && result.pressed))
      {
        s_project_explorer_entry_activate(editor, state, entry, is_directory);
      }
    }

    ldk_ui_spacer(ui);
    ldk_ui_end_horizontal(ui);
  }
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

void ldk_editor_file_explorer_show(LDKEditor *editor, const char *root_path)
{
  s_editor_project_explorer((LDKEditorContext *)editor, root_path);
}
