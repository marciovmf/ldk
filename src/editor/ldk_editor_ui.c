#include "ldk_editor_internal.h"
#include <stdx/stdx_strbuilder.h>
#include <inttypes.h> // for PRIu64

//------------------------------------------------------------
// Project Explorer
//------------------------------------------------------------

static void s_editor_project_explorer(LDKEditorContext *editor, const char *root_path)
{
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

  static LDKUIRect s_window_rect = {10.0f, 60.0f, 640.0f, 420.0f};
  static LDKUIPoint s_tree_scroll = {0};
  static LDKUIPoint s_file_scroll = {0};
  static XFSPath s_root = {0};
  static XFSPath s_selected_directory = {0};
  static XFSPath s_selected_file = {0};
  static XArray *s_expanded_paths = NULL;
  static XArray *s_stack = NULL;
  static XArray *s_dirs = NULL;
  static XArray *s_files = NULL;
  static bool s_root_expanded = true;
  static float s_icon_size = 48.0f;

  LDKEditorContext* peditor = (LDKEditorContext*) editor;
  LDKUIContext *ui = &peditor->ui;

  LDKUIIcon file_icon = {0};
  file_icon.size = ldk_sizef(s_icon_size, s_icon_size);
  file_icon.texture =
      ldk_renderer_texture_ui_handle(peditor->renderer, peditor->ui_atlas);
  file_icon.uv = ldk_editor_icon_rects[LDK_EDITOR_ICON_FILE];

  LDKUIIcon folder_icon = file_icon;
  folder_icon.uv = ldk_editor_icon_rects[LDK_EDITOR_ICON_FOLDER];

  LDKUIIcon tree_folder_icon = folder_icon;
  tree_folder_icon.size =
      ldk_sizef(PROJECT_EXPLORER_TREE_ICON_SIZE, PROJECT_EXPLORER_TREE_ICON_SIZE);

  s_window_rect = ldk_ui_begin_window(
      ui, "Project Explorer", s_window_rect, LDK_UI_WINDOW_TOOL);

  if (s_expanded_paths == NULL)
  {
    s_expanded_paths =
        x_array_create(sizeof(XFSPath), PROJECT_EXPLORER_INITIAL_CAPACITY);
    s_stack = x_array_create(
        sizeof(ProjectExplorerNode), PROJECT_EXPLORER_INITIAL_CAPACITY);
    s_dirs = x_array_create(
        sizeof(ProjectExplorerEntry), PROJECT_EXPLORER_INITIAL_CAPACITY);
    s_files = x_array_create(
        sizeof(ProjectExplorerEntry), PROJECT_EXPLORER_INITIAL_CAPACITY);
  }

  if (s_expanded_paths == NULL || s_stack == NULL || s_dirs == NULL ||
      s_files == NULL)
  {
    ldk_ui_label(ui, "Project explorer allocation failed.");
    ldk_ui_end_window(ui);
    return;
  }

  if (root_path == NULL)
  {
    ldk_ui_label(ui, "No project root.");
    ldk_ui_end_window(ui);
    return;
  }

  XFSPath root = {0};
  x_fs_path_set(&root, root_path);
  x_fs_path_normalize(&root);

  if (!x_fs_path_is_directory(&root))
  {
    ldk_ui_label(ui, "No project root.");
    ldk_ui_end_window(ui);
    return;
  }

  if (s_root.length == 0 || x_fs_path_compare(&s_root, &root) != 0)
  {
    s_root = root;
    s_selected_directory = root;
    memset(&s_selected_file, 0, sizeof(s_selected_file));
    x_array_clear(s_expanded_paths);
    s_root_expanded = true;
  }

  ldk_ui_begin_horizontal(ui);

  ldk_ui_set_next_width(ui, ldk_ui_px(220.0f));
  s_tree_scroll = ldk_ui_begin_scrollview(
      ui, s_tree_scroll, LDK_UI_SCROLL_VERTICAL | LDK_UI_SCROLL_IF_NEEDED);

  x_array_clear(s_stack);

  ProjectExplorerNode root_node = {0};
  root_node.path = root;
  x_fs_path_basename(&root, &root_node.name);
  if (root_node.name.length == 0)
  {
    x_smallstr_from_cstr(&root_node.name, x_fs_path_cstr(&root));
  }
  root_node.depth = 0;
  root_node.root = true;
  x_array_add(s_stack, &root_node);

  while (x_array_count(s_stack) > 0)
  {
    u32 stack_index = x_array_count(s_stack) - 1;
    ProjectExplorerNode *node_ptr = x_array_get(s_stack, stack_index);
    ProjectExplorerNode node = *node_ptr;
    bool expanded = false;
    bool was_expanded = false;
    i32 expanded_index = -1;
    x_array_delete_at(s_stack, stack_index);

    if (node.root)
    {
      expanded = s_root_expanded;
      was_expanded = s_root_expanded;
    }
    else
    {
      for (u32 i = 0; i < x_array_count(s_expanded_paths); i++)
      {
        XFSPath *expanded_path = x_array_get(s_expanded_paths, i);
        if (x_fs_path_compare(expanded_path, &node.path) == 0)
        {
          expanded = true;
          was_expanded = true;
          expanded_index = (i32)i;
          break;
        }
      }
    }

    u32 flags = 0;
    if (x_fs_path_compare(&s_selected_directory, &node.path) == 0)
    {
      flags |= LDK_UI_TREE_NODE_SELECTED;
    }

    ldk_ui_begin_horizontal(ui);

    ldk_ui_set_next_weight(ui, 0.0f);
    bool icon_clicked = ldk_ui_icon_button(ui, tree_folder_icon);

    bool node_expanded =
        ldk_ui_tree_node(ui, node.name.buf, expanded, node.depth, flags);

    ldk_ui_end_horizontal(ui);

    expanded = icon_clicked ? !was_expanded : node_expanded;

    if (expanded != was_expanded)
    {
      s_selected_directory = node.path;
      memset(&s_selected_file, 0, sizeof(s_selected_file));

      if (node.root)
      {
        s_root_expanded = expanded;
      }
      else if (expanded && expanded_index < 0)
      {
        x_array_add(s_expanded_paths, &node.path);
      }
      else if (!expanded && expanded_index >= 0)
      {
        x_array_delete_at(s_expanded_paths, (u32)expanded_index);
      }
    }

    if (!expanded)
    {
      continue;
    }

    x_array_clear(s_dirs);

    XFSDireEntry fs_entry = {0};
    XFSDireHandle *dir =
        x_fs_find_first_file(x_fs_path_cstr(&node.path), &fs_entry);

    while (dir != NULL)
    {
      if (strcmp(fs_entry.name, ".") != 0 && strcmp(fs_entry.name, "..") != 0 &&
          fs_entry.is_directory)
      {
        ProjectExplorerEntry entry = {0};
        entry.path = node.path;
        x_fs_path_join(&entry.path, fs_entry.name);
        x_smallstr_from_cstr(&entry.name, fs_entry.name);
        entry.size = fs_entry.size;
;
        entry.last_modified = fs_entry.last_modified;

        u32 insert_index = x_array_count(s_dirs);
        for (u32 i = 0; i < x_array_count(s_dirs); i++)
        {
          ProjectExplorerEntry *it = x_array_get(s_dirs, i);
          if (strcmp(it->name.buf, entry.name.buf) > 0)
          {
            insert_index = i;
            break;
          }
        }

        x_array_insert(s_dirs, &entry, insert_index);
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

    for (u32 i = x_array_count(s_dirs); i > 0; i--)
    {
      ProjectExplorerEntry *entry = x_array_get(s_dirs, i - 1);
      ProjectExplorerNode child = {0};
      child.path = entry->path;
      child.name = entry->name;
      child.depth = node.depth + 1;
      child.root = false;
      x_array_add(s_stack, &child);
    }
  }

  ldk_ui_spacer(ui);
  ldk_ui_end_scrollview(ui);

  ldk_ui_begin_vertical(ui);
  ldk_ui_set_next_weight(ui, 0.0f);

  XFSPath relative_selected_directory = {0};
  if (x_fs_path_relative_to(&s_root, &s_selected_directory,
          &relative_selected_directory) > 0)
  {
    ldk_ui_label(ui, relative_selected_directory.buf);
  }
  else
  {
    ldk_ui_label(ui, s_selected_directory.buf);
  }

  ldk_ui_set_next_weight(ui, 0.0f);
  s_icon_size = ldk_ui_slider(ui, s_icon_size, PROJECT_EXPLORER_MIN_ICON_SIZE, 72.0f);

  s_file_scroll = ldk_ui_begin_scrollview(
      ui, s_file_scroll, LDK_UI_SCROLL_VERTICAL | LDK_UI_SCROLL_IF_NEEDED);

  x_array_clear(s_dirs);
  x_array_clear(s_files);

  XFSDireEntry fs_entry = {0};
  XFSDireHandle *dir =
      x_fs_find_first_file(x_fs_path_cstr(&s_selected_directory), &fs_entry);

  while (dir != NULL)
  {
    if (strcmp(fs_entry.name, ".") != 0 && strcmp(fs_entry.name, "..") != 0)
    {
      ProjectExplorerEntry entry = {0};
      entry.path = s_selected_directory;
      x_fs_path_join(&entry.path, fs_entry.name);
      x_smallstr_from_cstr(&entry.name, fs_entry.name);
      entry.size = fs_entry.size;
      entry.last_modified = fs_entry.last_modified;

      XArray *target = fs_entry.is_directory ? s_dirs : s_files;
      u32 insert_index = x_array_count(target);

      for (u32 i = 0; i < x_array_count(target); i++)
      {
        ProjectExplorerEntry *it = x_array_get(target, i);
        if (strcmp(it->name.buf, entry.name.buf) > 0)
        {
          insert_index = i;
          break;
        }
      }

      x_array_insert(target, &entry, insert_index);
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

  u32 total_count = x_array_count(s_dirs) + x_array_count(s_files);
  bool compact_mode = s_icon_size <= PROJECT_EXPLORER_MIN_ICON_SIZE;

  if (compact_mode)
  {
    for (u32 entry_i = 0; entry_i < total_count; entry_i++)
    {
      bool is_directory = entry_i < x_array_count(s_dirs);
      u32 entry_index = is_directory
                            ? entry_i
                            : entry_i - x_array_count(s_dirs);
      ProjectExplorerEntry *entry =
          x_array_get(is_directory ? s_dirs : s_files, entry_index);

      ldk_ui_set_next_height(ui, ldk_ui_px(LDK_UI_DEFAULT_CONTROL_HEIGHT));
      ldk_ui_begin_horizontal(ui);

      ldk_ui_set_next_weight(ui, 0.0f);
      bool icon_clicked =
          ldk_ui_icon_button(ui, is_directory ? folder_icon : file_icon);

      bool label_clicked = ldk_ui_button_flat(ui, entry->name.buf);

      ldk_ui_end_horizontal(ui);

      if (icon_clicked || label_clicked)
      {
        if (is_directory)
        {
          s_selected_directory = entry->path;
          memset(&s_selected_file, 0, sizeof(s_selected_file));

          bool already_expanded = false;
          for (u32 i = 0; i < x_array_count(s_expanded_paths); i++)
          {
            XFSPath *expanded_path = x_array_get(s_expanded_paths, i);
            if (x_fs_path_compare(expanded_path, &entry->path) == 0)
            {
              already_expanded = true;
              break;
            }
          }

          if (!already_expanded)
          {
            x_array_add(s_expanded_paths, &entry->path);
          }
        }
        else
        {
          s_selected_file = entry->path;
        }
      }
    }
  }
  else
  {
    float tile_w = s_icon_size + 32.0f;
    float tile_h = s_icon_size + LDK_UI_DEFAULT_CONTROL_HEIGHT + 12.0f;
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

      for (u32 column = 0; column < column_count && tile_index < total_count;
           column++, tile_index++)
      {
        bool is_directory = tile_index < x_array_count(s_dirs);
        u32 entry_index = is_directory
                              ? tile_index
                              : tile_index - x_array_count(s_dirs);
        ProjectExplorerEntry *entry =
            x_array_get(is_directory ? s_dirs : s_files, entry_index);

        ldk_ui_set_next_size(ui, ldk_ui_px(tile_w), ldk_ui_px(tile_h));
        ldk_ui_begin_vertical(ui);

        ldk_ui_set_next_size(ui, ldk_ui_px(s_icon_size), ldk_ui_px(s_icon_size));
        bool icon_clicked =
            ldk_ui_icon_button(ui, is_directory ? folder_icon : file_icon);

        ldk_ui_set_next_height(ui, ldk_ui_px(LDK_UI_DEFAULT_CONTROL_HEIGHT));
        bool label_clicked = ldk_ui_button_flat(ui, entry->name.buf);

        ldk_ui_end_vertical(ui);

        if (icon_clicked || label_clicked)
        {
          if (is_directory)
          {
            s_selected_directory = entry->path;
            memset(&s_selected_file, 0, sizeof(s_selected_file));

            bool already_expanded = false;
            for (u32 i = 0; i < x_array_count(s_expanded_paths); i++)
            {
              XFSPath *expanded_path = x_array_get(s_expanded_paths, i);
              if (x_fs_path_compare(expanded_path, &entry->path) == 0)
              {
                already_expanded = true;
                break;
              }
            }

            if (!already_expanded)
            {
              x_array_add(s_expanded_paths, &entry->path);
            }
          }
          else
          {
            s_selected_file = entry->path;
          }
        }
      }

      ldk_ui_spacer(ui);
      ldk_ui_end_horizontal(ui);
    }
  }

  ldk_ui_spacer(ui);
  ldk_ui_end_scrollview(ui);
  ldk_ui_end_vertical(ui);

  ldk_ui_end_horizontal(ui);
  ldk_ui_end_window(ui);
}

//------------------------------------------------------------
// Console
//------------------------------------------------------------

static void s_editor_console(LDKEditorContext *editor)
{
  static XStrBuilder* sb = NULL;
  static XSmallstr input = {0};

  if (sb == NULL)
  {
    sb = x_strbuilder_create();
  }

  LDKUIContext *ui = &editor->ui;
  static LDKUIRect s_entity_list_rect = {150, 90, 200, 180};
  s_entity_list_rect = ldk_ui_begin_window_fixed(
    ui, "Console", s_entity_list_rect, LDK_UI_WINDOW_TOOL);
  static LDKUIPoint scroll = {0};
  scroll = ldk_ui_begin_scrollview(
    ui, scroll, LDK_UI_SCROLL_VERTICAL | LDK_UI_SCROLL_IF_NEEDED);
  ldk_ui_label(ui, x_strbuilder_to_string(sb));
  ldk_ui_end_scrollview(ui);

  ldk_ui_set_next_weight(ui, 0.0f);
  if (ldk_ui_input_box(ui, input.buf, X_SMALLSTR_MAX_LENGTH) & LDK_UI_INPUT_BOX_COMMITTED)
  {
    x_strbuilder_append_format(sb, "%s\n", input.buf);
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
  LDKUIContext* ui = &editor->ui;

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

  s_toolbar_rect = ldk_ui_begin_window(ui, "toolbar", s_toolbar_rect, 0);

  ldk_ui_begin_horizontal(ui);
  LDKUIMark mark = ldk_ui_mark(ui);

  ldk_ui_set_next_weight(ui, 0.0f);
  if (ldk_ui_button_flat(ui, "File")) { ldk_ui_open_popup(ui, MENU_ID_FILE); }
  LDKUIRect file_button_rect = ldk_ui_last_rect(ui);

  ldk_ui_set_next_weight(ui, 0.0f);
  if (ldk_ui_button_flat(ui, "Edit")) { ldk_ui_open_popup(ui, MENU_ID_EDIT); }
  LDKUIRect edit_button_rect = ldk_ui_last_rect(ui);

  ldk_ui_set_next_weight(ui, 0.0f);
  if (ldk_ui_button_flat(ui, "Theme")) { ldk_ui_open_popup(ui, MENU_ID_THEME); }
  LDKUIRect theme_button_rect = ldk_ui_last_rect(ui);
  i32 menu_width = ldk_ui_measure_from(ui, mark).w;

  ldk_ui_spacer(ui);
  ldk_ui_end_horizontal(ui);
  ldk_ui_horizontal_line(ui);
    
  LDKUIRect popup_pos =
    {file_button_rect.x, file_button_rect.y + file_button_rect.h, 120, 10};

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
      if (ldk_os_dialog_show_open_file(editor->window, "Open Project", "*.ldk", out.buf, X_SMALLSTR_MAX_LENGTH))
      {
        ldk_editor_state_set_stop(editor);
        ldk_project_unload(&editor->project);
        ldk_game_instance_unload();

        if (!ldk_editor_project_load(editor, out.buf))
        {
          ldk_os_dialog_show_error(editor->window, "Failed to load project", out.buf);
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
    ldk_ui_begin_window_fixed(ui, "Editor Commands", toolbar_rect, 0);
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
    ui, "Entities", s_entity_list_rect, LDK_UI_WINDOW_TOOL);
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

void ldk_editor_internal_menubar_show(LDKEditorContext* editor)
{
  s_editor_menu_bar(editor);
}

void ldk_editor_internal_toolbar_show(LDKEditorContext* editor)
{
  s_editor_tool_bar(editor);
}

//------------------------------------------------------------
// Public
//------------------------------------------------------------

void ldk_editor_console_show(LDKEditor* editor)
{
  s_editor_console(editor);
}

void ldk_editor_file_explorer_show(LDKEditor* editor, const char* root_path)
{
  s_editor_project_explorer((LDKEditorContext*)editor, root_path);
}

void ldk_editor_hierarchy_show(LDKEditor* editor, LDKECS *ecs)
{
  s_editor_entity_list_window((LDKEditorContext*) editor, ecs);
}
