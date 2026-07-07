
#define X_IMPL_LOG
#include <stdx/stdx_log.h>

#define X_IMPL_STRING
#include <stdx/stdx_string.h>

#define X_IMPL_STRBUILDER
#include <stdx/stdx_strbuilder.h>

#define X_IMPL_FILESYSTEM
#include <stdx/stdx_filesystem.h>

#define X_IMPL_INI
#include <stdx/stdx_ini.h>

#define X_IMPL_ARRAY
#include <stdx/stdx_array.h>

#include <ldk_common.h>
#include <module/ldk_ecs.h>
#include "ldk_editor_atlas.h"
#include <ldk_game.h>
#include <ldk_event.h>
#include <ldk_os.h>
#include <ldk_image.h>
#include <ldk_project.h>
#include <module/ldk_ui.h>
#include <module/ldk_renderer.h>
#include <module/ldk_asset_manager.h>

#include <inttypes.h> // for PRIu64

#ifndef LDK_DEFAULT_UI_INITIAL_INDEX_CAPACITY
#define LDK_DEFAULT_UI_INITIAL_INDEX_CAPACITY 256
#endif

#ifndef LDK_DEFAULT_UI_INITIAL_VERTEX_CAPACITY
#define LDK_DEFAULT_UI_INITIAL_VERTEX_CAPACITY 1024
#endif

#ifndef LDK_DEFAULT_UI_INITIAL_COMMAND_CAPACITY
#define LDK_DEFAULT_UI_INITIAL_COMMAND_CAPACITY 32
#endif

#ifndef LDK_DEFAULT_UI_INITIAL_WINDOW_CAPACITY
#define LDK_DEFAULT_UI_INITIAL_WINDOW_CAPACITY 16
#endif

#ifndef LDK_DEFAULT_UI_INITIAL_STACK_CAPACITY
#define LDK_DEFAULT_UI_INITIAL_STACK_CAPACITY 16
#endif

typedef enum LDKEditorState
{
  LDK_EDITOR_STATE_STOPED = 0,
  LDK_EDITOR_STATE_PAUSED = 1,
  LDK_EDITOR_STATE_STEPPING = 2,
  LDK_EDITOR_STATE_PLAYING = 3
} LDKEditorState;

typedef struct LDKEditor
{
  LDKWindow window;
  LDKUIContext ui;
  LDKAssetFont font;
  LDKFontInstance *font_instance;
  LDKRenderer *renderer;

  LDKUITextInputState text_input_state;
  LDKProject project;
  bool initialized;
  LDKEditorState editor_state;
  XFSPath engine_runtree;
  LDKGameUpdateFunc original_game_update_fn;

  LDKResourceTexture ui_atlas;

  // config
  XFSPath editor_font;
  XSmallstr editor_theme;
  i32 editor_font_size;
} LDKEditor;

static void s_editor_update(
    LDKEditor *editor, i32 window_width, i32 window_height, float delta_time);
static bool s_editor_state_set_play(LDKEditor *editor);
static void s_editor_state_set_stop(LDKEditor *editor);
static void s_editor_state_set_pause(LDKEditor *editor);
static void s_editor_state_set_step(LDKEditor *editor);
static bool s_project_load(LDKEditor *editor, const char *project_file_path);

/**
 * Tiny function to return a static editor instance.
 */
static LDKEditor *s_editor_instance(void)
{
  static LDKEditor editor = {0};
  return &editor;
}

static inline void s_editor_command_quit(LDKEditor *editor)
{
  if (ldk_os_dialog_show_yes_no(editor->window, "Quit editor ?",
          "Are you sure you want to quit the editor ?"))
  {
    ldk_log_info("Closing game window\n");
    ldk_engine_stop(0);
  }
}

static void s_theme_icons_set(LDKEditor* editor, LDKUITheme* theme)
{
  LDKUIIcon icon = {0};
  icon.size = ldk_sizef(24, 24);
  icon.texture = ldk_renderer_texture_ui_handle(editor->renderer, editor->ui_atlas);

  icon.uv = ldk_editor_icon_rects[LDK_EDITOR_ICON_CHEV_RIGHT];
  editor->ui.theme.icons[LDK_UI_THEME_ICON_TREE_NODE_COLLAPSED] = icon;
  
  icon.uv = ldk_editor_icon_rects[LDK_EDITOR_ICON_CHEV_DOWN];
  editor->ui.theme.icons[LDK_UI_THEME_ICON_TREE_NODE_EXPANDED] = icon;

  icon.uv = ldk_editor_icon_rects[LDK_EDITOR_ICON_CHEV_DOWN];
  editor->ui.theme.icons[LDK_UI_THEME_ICON_TREE_NODE_EXPANDED] = icon;

  icon.uv = ldk_editor_icon_rects[LDK_EDITOR_ICON_CHECKBOX_UNCHECKED];
  editor->ui.theme.icons[LDK_UI_THEME_ICON_TOGGLE_UNCHECKED] = icon;

  icon.uv = ldk_editor_icon_rects[LDK_EDITOR_ICON_CHECKBOX_CHECKED];
  editor->ui.theme.icons[LDK_UI_THEME_ICON_TOGGLE_CHECKED] = icon;
}

//----------------------------------------------------------
// Event Handlers
//----------------------------------------------------------

static bool on_event_keyboard(const LDKEvent *event, void *state)
{
  LDKEditor *editor = (LDKEditor *)state;
  if (event->keyboard_event.type == LDK_KEYBOARD_EVENT_KEY_DOWN)
  {
    if (event->keyboard_event.ctrl_is_down &&
        event->keyboard_event.shift_is_down)
    {
      // CTRL+SHIFT+P
      if (event->keyboard_event.keyCode == LDK_KEYCODE_P)
      {
        s_editor_state_set_stop(editor);
        return true;
      }
    }
    else if (event->keyboard_event.ctrl_is_down)
    {
      // CTRL+P
      if (event->keyboard_event.keyCode == LDK_KEYCODE_P)
      {
        s_editor_state_set_play(editor);
        return true;
      }

      // CTRL+O
      if (event->keyboard_event.keyCode == LDK_KEYCODE_O)
      {
        XFSPath out = {0};
        if (ldk_os_dialog_show_open_file(editor->window, "Open Project",
                "*.ldk", out.buf, X_SMALLSTR_MAX_LENGTH))
        {
          s_editor_state_set_stop(editor);
          ldk_project_unload(&editor->project);
          ldk_game_instance_unload();

          if (!s_project_load(editor, out.buf))
          {
            ldk_os_dialog_show_ok(
                editor->window, "Failed to load project", out.buf);
          }
        }
        return true;
      }
    }
  }
  return false;
}

static bool on_event_text(const LDKEvent *event, void *state)
{
  LDKEditor *editor = (LDKEditor *)state;
  if (event->text_event.type == LDK_TEXT_EVENT_CHARACTER_INPUT)
  {
    if (editor->text_input_state.codepoint_count <
        LDK_UI_INPUT_CODEPOINTS_CAPACITY)
    {
      editor->text_input_state
          .codepoints[editor->text_input_state.codepoint_count++] =
          event->text_event.character;
      return true;
    }
  }
  return false;
}

static bool on_event_frame(const LDKEvent *event, void *state)
{
  LDKEditor *editor = (LDKEditor *)state;
  if (event->type != LDK_EVENT_TYPE_FRAME ||
      event->frame_event.type != LDK_FRAME_EVENT_SUBMIT_AFTER)
    return false;

  // TODO: Replace this by widow event listener
  LDKSize size = ldk_os_window_client_area_size_get(editor->window);

  s_editor_update(editor, size.w, size.h, event->frame_event.delta_time);
  return true;
}

static bool on_event_window(const LDKEvent *event, void *state)
{
  LDKEditor *editor = (LDKEditor *)state;

  if (event->window_event.type == LDK_WINDOW_EVENT_CLOSE)
  {
    s_editor_command_quit(editor);
    return true; // Do not propagate this message further
  }
  return false;
}

/*
 *  Test functions
 */

static void s_editor_test_treeview(LDKEditor *editor)
{
  LDKUIContext *ui = &editor->ui;
  static LDKUIRect s_entity_list_rect = {150, 90, 200, 180};
  static LDKUIPoint scroll = {0};
  static bool s_root_open[10] = {0};
  static bool s_child_open[10] = {0};
  static char const *s_root_labels[10] = {
      "Root 0",
      "Root 1",
      "Root 2",
      "Root 3",
      "Root 4",
      "Root 5",
      "Root 6",
      "Root 7",
      "Root 8",
      "Root 9",
  };

  s_entity_list_rect = ldk_ui_begin_window_fixed(
      ui, "test A", s_entity_list_rect, LDK_UI_WINDOW_TOOL);

  scroll = ldk_ui_begin_scrollview(
      ui, scroll, LDK_UI_SCROLL_VERTICAL | LDK_UI_SCROLL_IF_NEEDED);

  for (u32 i = 0; i < 10; i++)
  {
    ldk_ui_push_id_u32(ui, i);

    s_root_open[i] =
        ldk_ui_tree_node(ui, s_root_labels[i], s_root_open[i], 0, 0);
    if (s_root_open[i])
    {
      if (ldk_ui_tree_node(ui, "Position", false, 1, LDK_UI_TREE_NODE_LEAF))
      {
        ldk_log_info("Clicked!");
      }

      ldk_ui_tree_node(ui, "Rotation", false, 1, LDK_UI_TREE_NODE_LEAF);
      ldk_ui_tree_node(ui, "Scale", false, 1, LDK_UI_TREE_NODE_LEAF);
        

      s_child_open[i] = ldk_ui_tree_node(ui, "Nested", s_child_open[i], 1, 0);

      if (s_child_open[i])
      {
        ldk_ui_tree_node(
            ui, "Nested Position", false, 2, LDK_UI_TREE_NODE_LEAF);
        ldk_ui_tree_node(
            ui, "Nested Rotation", false, 2, LDK_UI_TREE_NODE_LEAF);
        ldk_ui_tree_node(ui, "Nested Scale", false, 2, LDK_UI_TREE_NODE_LEAF);
      }
    }

    ldk_ui_pop_id(ui);
  }

  ldk_ui_spacer(ui);
  ldk_ui_end_scrollview(ui);
  ldk_ui_end_window(ui);
}

static void s_editor_test_a(LDKEditor *editor)
{
  LDKUIContext *ui = &editor->ui;
  static LDKUIRect s_entity_list_rect = {150, 90, 200, 180};
  s_entity_list_rect = ldk_ui_begin_window_fixed(
      ui, "test A", s_entity_list_rect, LDK_UI_WINDOW_TOOL);
  static LDKUIPoint scroll = {0};
  scroll = ldk_ui_begin_scrollview(
      ui, scroll, LDK_UI_SCROLL_VERTICAL | LDK_UI_SCROLL_IF_NEEDED);
  for (u32 i = 0; i < 10; i++)
  {
    ldk_ui_button(ui, "btn");
  }

  ldk_ui_end_scrollview(ui);
  ldk_ui_end_window(ui);
}

static void s_editor_test_b(LDKEditor *editor)
{
  static bool check = false;
  LDKUIContext *ui = &editor->ui;
  static LDKUIRect s_entity_list_rect = {10, 90, 100, 300};
  s_entity_list_rect = ldk_ui_begin_window_fixed(
      ui, "test A", s_entity_list_rect, LDK_UI_WINDOW_TOOL);

  static LDKUIPoint scroll = {0};
  scroll = ldk_ui_begin_scrollview(
      ui, scroll, LDK_UI_SCROLL_VERTICAL | LDK_UI_SCROLL_IF_NEEDED);

  static bool open_a = true;
  open_a = ldk_ui_begin_area(ui, "Transform", open_a);
  if (open_a)
  {
    ldk_ui_label(ui, "Hello, Sailor!");
    ldk_ui_button(ui, "Click me");
    check = ldk_ui_toggle(ui, check);
  }
  ldk_ui_end_area(ui);

  static bool open_b = true;
  static bool open_b1 = true;
  static bool open_b2 = true;
  open_b = ldk_ui_begin_area(ui, "Transform", open_b);
  if (open_b)
  {
    open_b1 = ldk_ui_begin_area(ui, "Transform", open_b1);
    if (open_b1)
    {
      ldk_ui_label(ui, "Hello, Sailor!");
      ldk_ui_button(ui, "Click me");
    }
    ldk_ui_end_area(ui);

    open_b2 = ldk_ui_begin_area(ui, "Transform", open_b2);
    if (open_b2)
    {
      ldk_ui_label(ui, "Hello, Sailor!");
      ldk_ui_button(ui, "Click me");
    }
    ldk_ui_end_area(ui);
  }

  ldk_ui_end_area(ui);
  ldk_ui_end_scrollview(ui);
  ldk_ui_end_window(ui);
}

static void s_editor_console(LDKEditor *editor)
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
    x_smallstr_clear(&input);
    // scroll down
    scroll.y += 10000.0f;
  }

  ldk_ui_end_window(ui);
}

static void s_editor_project_explorer(LDKEditor *editor, const char *root_path)
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

  LDKUIContext *ui = &editor->ui;

  LDKUIIcon file_icon = {0};
  file_icon.size = ldk_sizef(s_icon_size, s_icon_size);
  file_icon.texture =
      ldk_renderer_texture_ui_handle(editor->renderer, editor->ui_atlas);
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
//----------------------------------------------------------
// Editor Udpate
//----------------------------------------------------------

static void s_editor_menu_bar(LDKEditor* editor)
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
        s_editor_state_set_stop(editor);
        ldk_project_unload(&editor->project);
        ldk_game_instance_unload();

        if (!s_project_load(editor, out.buf))
        {
          ldk_os_dialog_show_error(editor->window, "Failed to load project", out.buf);
        }
      }

      ldk_ui_close_current_popup(ui);
    }

    if (ldk_ui_button_flat(ui, "Exit"))
    {
      s_editor_command_quit(editor);
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
      s_theme_icons_set(editor, &theme);
      ldk_ui_theme_set(ui, &theme);
      ldk_ui_close_current_popup(ui);
    }
    if (ldk_ui_button_flat(ui, "Light"))
    {
      ldk_ui_theme_get(LDK_UI_THEME_DEFAULT_LIGHT, &theme);
      s_theme_icons_set(editor, &theme);
      ldk_ui_theme_set(ui, &theme);
      ldk_ui_close_current_popup(ui);
    }

    LDKUIRect content_rect = ldk_ui_measure_from(ui, mark);
  }
  ldk_ui_end_popup(ui);

  ldk_ui_end_window(ui);
}

static void s_test_popup(LDKEditor *editor)
{
  LDKUIContext *ctx = &editor->ui;
  LDKUIId popup_id = 0x1001u;

  ldk_ui_begin_vertical(ctx);
  ldk_ui_set_next_size(ctx, ldk_ui_px(120.0f), ldk_ui_px(22.0f));
  if (ldk_ui_button(ctx, "Open popup"))
  {
    ldk_ui_open_popup(ctx, popup_id);
  }
  LDKUIRect button_rect = ldk_ui_last_rect(ctx);

  LDKUIRect popup_rect = ldk_ui_rect(
    button_rect.x, button_rect.y + button_rect.h + 4.0f, 180.0f, 96.0f);

  if (ldk_ui_begin_popup(ctx, popup_id))
  {
    ldk_ui_set_next_height(ctx, ldk_ui_px(22.0f));
    ldk_ui_label(ctx, "Popup content");

    ldk_ui_set_next_height(ctx, ldk_ui_px(22.0f));

    if (ldk_ui_button_flat(ctx, "Close"))
    {
      ldk_ui_close_popup(ctx, popup_id);
    }

    ldk_ui_end_popup(ctx);
  }

  ldk_ui_end(ctx);
}

static void s_editor_tool_bar(LDKEditor *editor)
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
        s_editor_state_set_play(editor);
      }
    }
    else
    {
      icon.uv = ldk_editor_icon_rects[LDK_EDITOR_ICON_BUTTON_STOP];
      ldk_ui_set_next_weight(ui, 0.0f);
      if (ldk_ui_icon_button(ui, icon))
      {
        s_editor_state_set_stop(editor);
      }
    }

    { // Pause button
      bool can_pause = (editor->editor_state == LDK_EDITOR_STATE_PLAYING);
      ldk_ui_set_next_disabled(ui, !can_pause);
      icon.uv = ldk_editor_icon_rects[LDK_EDITOR_ICON_BUTTON_PAUSE];
      ldk_ui_set_next_weight(ui, 0.0f);
      if (ldk_ui_icon_button(ui, icon))
      {
        s_editor_state_set_pause(editor);
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
        s_editor_state_set_step(editor);
      }
    }
  }

  ldk_ui_spacer(ui);
  ldk_ui_end_horizontal(&editor->ui);
  ldk_ui_end_window(ui);
}

static void s_editor_entity_list_window(LDKEditor *editor, LDKECS *ecs)
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

static void s_draw_editor_ui(LDKEditor *editor, float delta_time)
{
  LDKECS *ecs = ldk_module_get(LDK_MODULE_ECS);
  //s_editor_tool_bar(editor);
  //s_editor_entity_list_window(editor, ecs);
  s_editor_test_a(editor);
  s_editor_test_b(editor);
  s_editor_test_treeview(editor);
  s_editor_console(editor);
  s_editor_menu_bar(editor);
  s_editor_project_explorer(editor, "c:\\work\\ldk");
}

static void s_editor_update(LDKEditor *editor, i32 window_width,
                            i32 window_height, float delta_time)
{
  LDKMouseState mouse_state;
  LDKKeyboardState kbd_state;
  LDKUIRect ui_viewport = (LDKUIRect){.x = 0.0f,
                                      .y = 0.0f,
                                      .w = (float)window_width,
                                      .h = (float)window_height};
  ldk_os_mouse_state_get(&mouse_state);
  ldk_os_keyboard_state_get(&kbd_state);

  ldk_ui_begin_frame(&editor->ui, delta_time, &mouse_state, &kbd_state,
                     &editor->text_input_state, ui_viewport);
  s_draw_editor_ui(editor, delta_time);
  ldk_ui_end_frame(&editor->ui);
  const LDKUIRenderData *ui_data = ldk_ui_get_render_data(&editor->ui);
  ldk_renderer_submit_ui(ldk_module_get(LDK_MODULE_RENDERER), ui_data);
  editor->text_input_state.codepoint_count = 0;
}

//----------------------------------------------------------
// Editor Initialization
//----------------------------------------------------------

static bool s_editor_config_load_from_ini(
  LDKEditor *editor, XIni *ini, LDKConfig *config)
{
  LDK_ASSERT(editor);
  LDK_ASSERT(editor->renderer);
  LDK_ASSERT(!editor->initialized);

  // load .editor section
  const char *EDITOR = ".editor";
  editor->editor_font_size = x_ini_get_i32(ini, EDITOR, "font_size", 18);
  x_smallstr_from_cstr(
    &editor->editor_theme, x_ini_get(ini, EDITOR, "theme", "dark"));
  x_fs_path(&editor->editor_font, config->runtree_path,
            x_ini_get(ini, EDITOR, "font", "assets/InterDisplay-Regular.ttf"));

  // load a te texture atlas
  XFSPath atlas_path;

  x_fs_path(&atlas_path, config->runtree_path, "assets", "ui_atlas.png");
  LDKImage *image_atlas = ldk_image_create_from_memory(
    ldk_editor_icon_atlas_png, ldk_editor_icon_atlas_png_size);
  if (image_atlas == NULL)
  {
    ldk_log_error("Failed to load editor atas '%s'\n", atlas_path.buf);
    return false;
  }
  LDKResourceTexture texture_atlas = ldk_renderer_texture_create_from_image(
    ldk_module_get(LDK_MODULE_RENDERER), image_atlas, 0);

  if (ldk_renderer_texture_null().id == texture_atlas.id)
  {
    ldk_log_error(
      "Failed to create texture from image atas '%s'\n", atlas_path.buf);
    return false;
  }

  editor->ui_atlas = texture_atlas;
  editor->initialized = true;
  return true;
}

static bool s_editor_load_resources(LDKEditor *editor, LDKConfig *config)
{
  LDK_ASSERT(editor);
  LDK_ASSERT(editor->initialized);

  LDKAssetManager *asset_manager = ldk_module_get(LDK_MODULE_ASSET_MANAGER);
  LDKAssetManager *renderer = ldk_module_get(LDK_MODULE_RENDERER);

  // Load UI editor font
  editor->font =
    ldk_asset_manager_font_load(asset_manager, editor->editor_font.buf);
  LDKAssetFontData *editor_font_data =
    ldk_asset_manager_font_get(asset_manager, editor->font);
  if (!editor_font_data || !editor_font_data->face)
  {
    ldk_log_error(
      "Failed to load editor font '%s'.\n", editor->editor_font.buf);
    return false;
  }

  LDKFontAtlasDesc font_atlas_desc = {0};
  font_atlas_desc.padding = 1;
  font_atlas_desc.page_width = 256;
  font_atlas_desc.page_height = 256;

  editor->font_instance = ldk_ttf_get_instance(editor_font_data->face,
                                               (float)editor->editor_font_size, &font_atlas_desc);

  if (!editor->font_instance)
  {
    ldk_log_error("Failed to create editor font instance.\n");
    return false;
  }

  return true;
}

static bool s_editor_gui_initialize(LDKEditor *editor, LDKRenderer *renderer)
{
  LDK_ASSERT(editor);
  LDK_ASSERT(editor->initialized);

  // Editor UI Initialization
  LDKUIConfig ui_cfg = {0};
  ui_cfg.frame_arena_size = 1024 * 4;
  ui_cfg.initial_vertex_capacity = LDK_DEFAULT_UI_INITIAL_VERTEX_CAPACITY;
  ui_cfg.initial_index_capacity = LDK_DEFAULT_UI_INITIAL_INDEX_CAPACITY;
  ui_cfg.initial_command_capacity = LDK_DEFAULT_UI_INITIAL_COMMAND_CAPACITY;
  ui_cfg.initial_window_capacity = LDK_DEFAULT_UI_INITIAL_WINDOW_CAPACITY;
  ui_cfg.initial_id_stack_capacity = LDK_DEFAULT_UI_INITIAL_STACK_CAPACITY;
  ui_cfg.font = editor->font_instance;

  if (strncmp(editor->editor_theme.buf, "light", 5) == 0)
    ui_cfg.theme = LDK_UI_THEME_DEFAULT_LIGHT;
  else if (strncmp(editor->editor_theme.buf, "dark", 4) == 0)
    ui_cfg.theme = LDK_UI_THEME_DEFAULT_DARK;
  else
  {
    ldk_log_warning("Unknown Editor theme name '%s'. Default to 'light'.",
                    editor->editor_theme.buf);
    ui_cfg.theme = LDK_UI_THEME_DEFAULT_LIGHT;
  }

  ui_cfg.font_texture_user = renderer;
  ui_cfg.get_font_page_texture = ldk_renderer_get_font_page_texture_callback;

  if (!ldk_ui_initialize(&editor->ui, &ui_cfg))
  {
    ldk_log_error("Failed to initialize module: UI System.");
    return false;
  }

  s_theme_icons_set(editor, &editor->ui.theme); // set theme icons
  return true;
}

//----------------------------------------------------------
// Play / Stop
//----------------------------------------------------------

/**
 * This function replaces the original game_update function.
 * It is prevents the engine from updating the game when
 * the editor is not on PLAY mode
 */
static inline void s_game_update(LDKGame *game, float delta_time)
{
  LDKEditor *editor = s_editor_instance();

  if (editor->editor_state == LDK_EDITOR_STATE_STEPPING)
  {
    editor->original_game_update_fn(game, delta_time);
    editor->editor_state = LDK_EDITOR_STATE_PAUSED;
  }

  if (editor->editor_state == LDK_EDITOR_STATE_PLAYING)
    editor->original_game_update_fn(game, delta_time);
}

/**
 * Change Editor mode to PLAY
 */
static bool s_editor_state_set_play(LDKEditor *editor)
{
  if (!editor->project.loaded)
    return false;

  if (editor->editor_state == LDK_EDITOR_STATE_PAUSED)
  {
    editor->editor_state = LDK_EDITOR_STATE_PLAYING;
    return true;
  }

  if (editor->editor_state == LDK_EDITOR_STATE_PLAYING)
    return true;

  LDKGame *game = ldk_game_get();
  if (!game->start(game))
  {
    return false;
  }

  editor->editor_state = LDK_EDITOR_STATE_PLAYING;
  return true;
}

/**
 * Change Editor mode to STOP
 */
static void s_editor_state_set_stop(LDKEditor *editor)
{
  if (editor->editor_state == LDK_EDITOR_STATE_STOPED)
    return;

  LDKGame *game = ldk_game_get();
  game->stop(game);
  editor->editor_state = LDK_EDITOR_STATE_STOPED;
}

static void s_editor_state_set_pause(LDKEditor *editor)
{
  if (editor->editor_state == LDK_EDITOR_STATE_STOPED)
    return;

  LDKGame *game = ldk_game_get();
  editor->editor_state = LDK_EDITOR_STATE_PAUSED;
}

static void s_editor_state_set_step(LDKEditor *editor)
{
  if (editor->editor_state != LDK_EDITOR_STATE_PAUSED)
    return;

  LDKGame *game = ldk_game_get();
  editor->editor_state = LDK_EDITOR_STATE_STEPPING;
}

//----------------------------------------------------------
// Project handling
//----------------------------------------------------------

static bool s_project_load(LDKEditor *editor, const char *project_file_path)
{
  LDK_ASSERT(editor);
  LDK_ASSERT(editor->initialized);

  if (!project_file_path)
    return false;

  if (editor->project.loaded)
    return false;

  if (!ldk_project_load(&editor->project, project_file_path))
    return false;

  if (!ldk_game_instance_load_from_shared_lib(
        editor->project.game_dll_path.buf))
    return false;

  if (!ldk_game_instance_initialize())
    return false;

  LDKECS *ecs = ldk_module_get(LDK_MODULE_ECS);
  ldk_ecs_terminate();
  ldk_ecs_initialize(ecs, 64, 1);

  // we change the game update function to call us so we can
  // update the game only in PLAY mode.
  LDKGame *game = ldk_game_get();
  editor->editor_state = LDK_EDITOR_STATE_STOPED;
  editor->original_game_update_fn = game->update;
  game->update = s_game_update;

  XSmallstr title = {0};
  x_smallstr_format(
    &title, "LDK Editor - %s - %s", editor->project.name, project_file_path);
  ldk_os_window_title_set(editor->window, title.buf);

  return true;
}

static void s_editor_terminate(LDKEditor *editor)
{
  LDKEventQueue *eq = ldk_module_get(LDK_MODULE_EVENT);
  ldk_event_handler_remove(eq, on_event_text);
  ldk_event_handler_remove(eq, on_event_frame);
  ldk_event_handler_remove(eq, on_event_keyboard);
  ldk_event_handler_remove(eq, on_event_window);
}

//----------------------------------------------------------
// Entrypoint
//----------------------------------------------------------

static i32 s_editor_main(const char *project_file_path)
{
  LDKEditor *editor = s_editor_instance();
  XIni ini;
  XIniError ini_error;
  LDKConfig config;
  XFSPath editor_ini_path;

  memset(&ini, 0, sizeof(ini));
  memset(&ini_error, 0, sizeof(ini_error));

  // Load editor.ini from engine runtree
  x_fs_path_from_executable(&editor->engine_runtree);
  x_fs_path_dirname(&editor->engine_runtree, &editor->engine_runtree);
  x_fs_path_join(&editor->engine_runtree, "..", "..", "runtree");
  x_fs_path(&editor_ini_path, &editor->engine_runtree, "editor.ini");

  if (!x_ini_load_file(editor_ini_path.buf, &ini, &ini_error))
  {
    ldk_log_error("Failed to load config file '%s'. Syntax error at %d:%d: %s",
                  editor_ini_path.buf, ini_error.line, ini_error.column,
                  ini_error.message ? ini_error.message : "Unknown error");
    return false;
  }

  if (!ldk_engine_config_from_ini(&config, &ini, editor_ini_path.buf))
    return 1;

  // Initialize engine. Must be initialized before editor and projects
  if (!ldk_engine_initialize_with_config(&config))
    return 1;

  // Listen to text events for editor UI
  LDKEventQueue *module_event = ldk_module_get(LDK_MODULE_EVENT);
  ldk_event_handler_add(
    module_event, on_event_text, LDK_EVENT_TYPE_TEXT, editor);
  ldk_event_handler_add(
    module_event, on_event_keyboard, LDK_EVENT_TYPE_KEYBOARD, editor);
  ldk_event_handler_add(
    module_event, on_event_frame, LDK_EVENT_TYPE_FRAME, editor);
  ldk_event_handler_add(
    module_event, on_event_window, LDK_EVENT_TYPE_WINDOW, editor);

  editor->window = ldk_engine_main_window_get();
  editor->renderer = ldk_module_get(LDK_MODULE_RENDERER);

  // Initialize editor
  if (!s_editor_config_load_from_ini(editor, &ini, &config))
  {
    ldk_engine_terminate();
    return 1;
  }

  // Load editor resources
  if (!s_editor_load_resources(editor, &config))
  {
    ldk_engine_terminate();
    return 1;
  }

  if (!s_editor_gui_initialize(editor, ldk_module_get(LDK_MODULE_RENDERER)))
  {
    ldk_engine_terminate();
    return 1;
  }

  // If a project file was passed, loat that project
  if (project_file_path)
  {
    s_project_load(editor, project_file_path);
  }

  i32 exit_code = ldk_engine_run();
  s_editor_terminate(editor);
  ldk_engine_terminate();
  return exit_code;
}

int main(i32 argc, char **argv)
{
  char *project_file_path;

  if (argc == 1)
    project_file_path = NULL;
  else if (argc == 2)
    project_file_path = argv[1];
  else
  {
    printf("Usage:\n%s [project_file]\n", argv[0]);
    return 1;
  }

  return s_editor_main(project_file_path);
}
