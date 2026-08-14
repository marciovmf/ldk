#include "ldk_editor_internal.h"
#include "ldk_os.h"
#include "module/ldk_ui.h"
#include <ldk_scene.h>
#include <ldk_mesh.h>
#include <component/ldk_mesh_source.h>
#include <component/ldk_transform.h>
#include <stdx/stdx_strbuilder.h>
#include <stdx/stdx_string.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#define LDK_EDITOR_COLOR_FILE 0xFFFFFFFF
#define LDK_EDITOR_COLOR_FOLDER 0xFAD460FF
#define LDK_EDITOR_COLOR_ICON_ERROR 0xE71A2DFF
#define LDK_EDITOR_COLOR_ICON_WARNING 0xF7B217FF

//------------------------------------------------------------
// Menu bar
//------------------------------------------------------------

static void s_editor_menu_bar(LDKEditorContext *editor)
{
  LDKUIContext *ui = &editor->ui;

  ldki_editor_scene_state_sync(editor);

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
      LDK_UI_DEFAULT_CONTROL_HEIGHT + LDK_UI_DEFAULT_PADDING; // * 2.0f;

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
      ldki_editor_scene_new(editor);
      ldk_ui_close_current_popup(ui);
    }

    ldk_ui_set_next_disabled(
        ui, !can_edit_scene || editor->current_scene_path.length == 0);
    if (ldk_ui_button_flat(ui, "Save Scene"))
    {
      ldki_editor_scene_save(editor);
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
      ldki_editor_scene_add_primitive(editor, LDK_MESH_PRIMITIVE_CUBE, "Cube");
      ldk_ui_close_current_popup(ui);
    }

    ldk_ui_set_next_disabled(ui, !can_add);
    if (ldk_ui_button_flat(ui, "Add Sphere"))
    {
      ldki_editor_scene_add_primitive(
          editor, LDK_MESH_PRIMITIVE_SPHERE, "Sphere");
      ldk_ui_close_current_popup(ui);
    }

    ldk_ui_set_next_disabled(ui, !can_add);
    if (ldk_ui_button_flat(ui, "Add Capsule"))
    {
      ldki_editor_scene_add_primitive(
          editor, LDK_MESH_PRIMITIVE_CAPSULE, "Capsule");
      ldk_ui_close_current_popup(ui);
    }

    ldk_ui_set_next_disabled(ui, !can_add);
    if (ldk_ui_button_flat(ui, "Add Plane"))
    {
      ldki_editor_scene_add_primitive(
          editor, LDK_MESH_PRIMITIVE_PLANE, "Plane");
      ldk_ui_close_current_popup(ui);
    }

    ldk_ui_set_next_disabled(ui, !can_add);
    if (ldk_ui_button_flat(ui, "Add Quad"))
    {
      ldki_editor_scene_add_primitive(editor, LDK_MESH_PRIMITIVE_QUAD, "Quad");
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
      ldki_editor_theme_icons_set(editor, &theme);
      ldk_ui_theme_set(ui, &theme);
      ldk_ui_close_current_popup(ui);
    }
    if (ldk_ui_button_flat(ui, "Light"))
    {
      ldk_ui_theme_get(LDK_UI_THEME_DEFAULT_LIGHT, &theme);
      ldki_editor_theme_icons_set(editor, &theme);
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

  bool saved = ldki_editor_dock_layout_save(out);
  x_strbuilder_destroy(out);
  return saved;
}

bool ldki_editor_layout_save_as(LDKEditorContext *editor)
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
    ldki_editor_log_error(editor, "The layout name cannot be empty.");
    return false;
  }

  if (name_slice.length >= sizeof(layout_name))
  {
    ldki_editor_log_error(editor, "The layout name is too long.");
    return false;
  }

  memcpy(layout_name, name_slice.ptr, name_slice.length);
  layout_name[name_slice.length] = 0;

  u32 layout_count = ldki_editor_dock_layout_count();
  for (u32 i = 0; i < layout_count; ++i)
  {
    const char *existing_name = ldki_editor_dock_layout_name_get(i);

    if (existing_name != NULL && strcmp(existing_name, layout_name) == 0)
    {
      ldki_editor_log_error(editor, "A layout with that name already exists.");
      return false;
    }
  }

  if (!ldki_editor_dock_layout_create(layout_name))
  {
    ldki_editor_log_error(editor, "Failed to save the layout.");
    return false;
  }

  if (!s_editor_layouts_save())
  {
    ldki_editor_log_error(editor,
        "Layout created in memory, but failed to save the layout file.");
  }
  else
  {
    ldki_editor_log_info(editor, "Layout created.");
  }

  return true;
}

u32 ldki_editor_input_window(LDKEditorContext *editor, const char *title)
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

  if (!ldk_ui_begin_window_open(ui, title, rect, &editor->show_input_window,
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
    bool confirm_requested = (result & LDK_UI_INPUT_BOX_COMMITTED) != 0;
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
  const char *current_name = ldki_editor_dock_layout_current_name_get();
  u32 stored_layout_count = ldki_editor_dock_layout_count();
  u32 layout_count = stored_layout_count;
  u32 selected_index = 0;

  for (u32 i = 0; i < layout_count; ++i)
  {
    items[i] = ldki_editor_dock_layout_name_get(i);

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
    if (stored_layout_count > 0 && ldki_editor_dock_set_current(items[result]))
    {
      if (!s_editor_layouts_save())
      {
        ldki_editor_log_error(editor,
            "Layout changed in memory, but failed to save the layout file.");
      }
    }
    else if (stored_layout_count > 0)
    {
      ldki_editor_log_error(editor, "Failed to change layout.");
    }
  }
  else if (result == save_layout_index)
  {
    if (!s_editor_layouts_save())
    {
      ldki_editor_log_error(editor, "Failed to save the layout.");
    }
    else
    {
      ldki_editor_log_info(editor, "Layout saved.");
    }
  }
  else if (result == save_layout_as_index)
  {
    memset(editor->input_window_buffer, 0, sizeof(editor->input_window_buffer));
    editor->input_window_rect = (LDKUIRect){0};
    editor->show_input_window = true;
  }
  else if (result == delete_layout_index)
  {
    char message[X_SMALLSTR_MAX_LENGTH];
    snprintf(message, sizeof(message),
        "Delete the \"%s\" layout? This cannot be undone.", current_name);

    if (ldk_os_dialog_show_yes_no(editor->window, "Delete layout?", message))
    {
      if (!ldki_editor_dock_layout_delete(current_name))
      {
        ldki_editor_log_error(editor, "Failed to delete layout.");
      }
      else if (!s_editor_layouts_save())
      {
        ldki_editor_log_error(editor,
            "Layout deleted in memory, but failed to save the layout file.");
      }
      else
      {
        ldki_editor_log_info(editor, "Layout deleted.");
      }
    }
  }
}

static void s_editor_gizmo_space_combo_box(LDKEditorContext *editor)
{
  static const char *items[] = {"GLOBAL", "LOCAL"};
  LDKUIContext *ui;
  u32 item_count;
  u32 selected_index;

  if (editor == NULL)
  {
    return;
  }

  ui = &editor->ui;
  item_count = (u32)(sizeof(items) / sizeof(items[0]));
  selected_index = editor->gizmo.mode == LDK_EDITOR_GIZMO_MODE_SCALE
                       ? (u32)LDK_EDITOR_GIZMO_SPACE_LOCAL
                       : (u32)editor->gizmo.space;
  if (selected_index >= item_count)
  {
    selected_index = (u32)LDK_EDITOR_GIZMO_SPACE_GLOBAL;
  }

  ldk_ui_set_next_disabled(ui,
      editor->gizmo.dragging ||
      editor->gizmo.mode == LDK_EDITOR_GIZMO_MODE_SCALE);
  ldk_ui_set_next_width(ui, ldk_ui_px(100.0f));
  selected_index = ldk_ui_combo_box(ui, items, item_count, selected_index);
  if (editor->gizmo.mode != LDK_EDITOR_GIZMO_MODE_SCALE)
  {
    editor->gizmo.space = (LDKEditorGizmoSpace)selected_index;
  }
}

static void s_editor_gizmo_mode_combo_box(LDKEditorContext *editor)
{
  static const char *items[] = {"TRANSLATE", "SCALE"};
  LDKUIContext *ui;
  u32 item_count;
  u32 selected_index;

  if (editor == NULL)
  {
    return;
  }

  ui = &editor->ui;
  item_count = (u32)(sizeof(items) / sizeof(items[0]));
  selected_index = (u32)editor->gizmo.mode;
  if (selected_index >= item_count)
  {
    selected_index = (u32)LDK_EDITOR_GIZMO_MODE_TRANSLATE;
  }

  ldk_ui_set_next_disabled(ui, editor->gizmo.dragging);
  ldk_ui_set_next_width(ui, ldk_ui_px(110.0f));
  selected_index = ldk_ui_combo_box(ui, items, item_count, selected_index);
  editor->gizmo.mode = (LDKEditorGizmoMode)selected_index;
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
  s_editor_gizmo_mode_combo_box(editor);
  s_editor_gizmo_space_combo_box(editor);
  ldk_ui_spacer(ui);

  {
    LDKUIIcon icon;
    icon.color = editor->ui.theme.colors[LDK_UI_COLOR_CONTROL_TEXT];
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
// Scene utils
//------------------------------------------------------------

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

bool ldki_editor_scene_load(LDKEditorContext *editor, const XFSPath *path)
{
  LDKSceneResult result;
  XFSPath relative = {0};

  ldki_editor_scene_state_sync(editor);

  if (!editor || editor->editor_state != LDK_EDITOR_STATE_STOPED ||
      !ldki_editor_scene_path_is_scene(path) ||
      !s_editor_scene_path_relative(editor, path, &relative))
  {
    return false;
  }

  if (!s_editor_scene_ecs_clear())
  {
    ldki_editor_log_error(editor, "Failed to clear the current scene.");
    return false;
  }

  s_editor_scene_selection_clear(editor);
  memset(&editor->current_scene_path, 0, sizeof(editor->current_scene_path));

  if (!ldk_scene_load_tml_file(x_fs_path_cstr(path), &result))
  {
    s_editor_scene_ecs_clear();
    ldki_editor_log_error(editor, result.error);
    return false;
  }

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

  if (!ldk_scene_save_tml_file(x_fs_path_cstr(&path), &result))
  {
    ldki_editor_log_error(editor, result.error);
    return false;
  }

  ldki_editor_log_info(editor, "Scene saved.");
  return true;
}

bool ldki_editor_scene_new(LDKEditorContext *editor)
{
  LDKSceneResult result;
  XFSPath path = {0};
  XFSPath relative = {0};
  char selected_path[X_FS_PATH_MAX_LENGTH] = {0};

  ldki_editor_scene_state_sync(editor);

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
    ldki_editor_log_error(editor, "Failed to clear the current scene.");
    return false;
  }

  s_editor_scene_selection_clear(editor);
  memset(&editor->current_scene_path, 0, sizeof(editor->current_scene_path));

  if (!ldk_scene_save_tml_file(x_fs_path_cstr(&path), &result))
  {
    ldki_editor_log_error(editor, result.error);
    return false;
  }

  editor->current_scene_path = relative;
  ldki_editor_log_info(editor, "Scene created.");
  return true;
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
  return true;
}

//------------------------------------------------------------
// Internal
//------------------------------------------------------------

void ldki_editor_menubar_show(LDKEditorContext *editor)
{
  s_editor_menu_bar(editor);
}

void ldki_editor_project_create_show(LDKEditorContext *editor)
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
      LDK_UI_WINDOW_TITLE_BAR | LDK_UI_WINDOW_DRAGGABLE | LDK_UI_WINDOW_BORDER);

  //----------------------------------------------------------------------
  // Project name
  //----------------------------------------------------------------------

  ldk_ui_set_next_height(ui, ldk_ui_px(LDK_UI_DEFAULT_CONTROL_HEIGHT));
  ldk_ui_begin_horizontal(ui);
  {
    ldk_ui_set_next_width(ui, ldk_ui_px(100.0f));
    ldk_ui_label(ui, "Project name");

    ldk_ui_input_box(ui, s_project_name.buf, (u32)sizeof(s_project_name.buf));
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
    ldk_ui_input_box(ui, s_project_path.buf, (u32)sizeof(s_project_path.buf));

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
        ldki_editor_log_info(editor, "Project Created.");
      }
      else
      {
        ldki_editor_log_error(editor, "Failed to create project.");
        ldk_os_dialog_show_error(
            editor->window, "Failed to create project", s_project_name.buf);
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

void ldki_editor_toolbar_show(LDKEditorContext *editor)
{
  s_editor_tool_bar(editor);
}

void ldki_editor_log_error(LDKEditorContext *editor, const char *msg)
{
  x_strbuilder_append_format(editor->console_sb, "%s\n", msg);
  ldk_log_error(msg);
}

void ldki_editor_log_warning(LDKEditorContext *editor, const char *msg)
{
  x_strbuilder_append_format(editor->console_sb, "%s\n", msg);
  ldk_log_warning(msg);
}

void ldki_editor_log_info(LDKEditorContext *editor, const char *msg)
{
  x_strbuilder_append_format(editor->console_sb, "%s\n", msg);
  ldk_log_info(msg);
}
