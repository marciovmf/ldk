#include "ldk_editor_atlas.h"
#include "ldk_editor_internal.h"
#include "ldk_editor_theme.h"
#include "ldk_os.h"
#include "module/ldk_ui.h"
#include <ldk_scene.h>
#include <ldk_mesh.h>
#include <component/ldk_camera.h>
#include <component/ldk_mesh_source.h>
#include <component/ldk_transform.h>
#include <module/ldk_ecs.h>
#include <module/ldk_scene_manager.h>
#include <stdx/stdx_ini.h>
#include <stdx/stdx_strbuilder.h>
#include <stdx/stdx_string.h>
#include <ctype.h>
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

const char *ldki_editor_console_last_message_get(LDKEditorContext *editor,
    LDKEditorConsoleEntryType *out_type);

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
  const LDKUIId MENU_ID_PROJECT = 11;
  const LDKUIId MENU_ID_THEME = 12;
  const LDKUIId MENU_ID_SCENE = 13;
  const LDKUIId MENU_ID_WINDOW = 14;

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
  if (ldk_ui_button_flat(ui, "Project"))
  {
    ldk_ui_open_popup(ui, MENU_ID_PROJECT);
  }
  LDKUIRect edit_button_rect = ldk_ui_last_rect(ui);

  ldk_ui_set_next_weight(ui, 0.0f);
  if (ldk_ui_button_flat(ui, "Scene"))
  {
    ldk_ui_open_popup(ui, MENU_ID_SCENE);
  }
  LDKUIRect scene_button_rect = ldk_ui_last_rect(ui);

  ldk_ui_set_next_weight(ui, 0.0f);
  if (ldk_ui_button_flat(ui, "Window"))
  {
    ldk_ui_open_popup(ui, MENU_ID_WINDOW);
  }
  LDKUIRect window_button_rect = ldk_ui_last_rect(ui);

  ldk_ui_set_next_weight(ui, 0.0f);
  if (ldk_ui_button_flat(ui, "Theme"))
  {
    ldki_editor_theme_refresh(editor);
    ldk_ui_open_popup(ui, MENU_ID_THEME);
  }
  LDKUIRect theme_button_rect = ldk_ui_last_rect(ui);
  i32 menu_width = ldk_ui_measure_from(ui, mark).w;

  ldk_ui_spacer(ui);
  ldk_ui_end_horizontal(ui);
  ldk_ui_horizontal_line(ui);

  LDKUIRect popup_pos = {
      file_button_rect.x, file_button_rect.y + file_button_rect.h, 120, 10};

  bool can_edit_scene = editor->project.loaded &&
    editor->editor_state == LDK_EDITOR_STATE_STOPED;
  

  ldk_ui_begin_popup(ui, MENU_ID_FILE);
  {
    LDKUIMark mark = ldk_ui_mark(ui);

    if (ldk_ui_button_flat(ui, "New Project"))
    {
      ldki_editor_project_create_window_open(editor);
      ldk_ui_close_current_popup(ui);
    }

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
      ldki_editor_show_open_project_dialog(editor, NULL);
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

  ldk_ui_begin_popup(ui, MENU_ID_PROJECT);
  {
    LDKUIMark mark = ldk_ui_mark(ui);

    const bool can_not_build = !(can_edit_scene && !editor->project_build.active);

    ldk_ui_set_next_disabled(ui, can_not_build);
    if (ldk_ui_button_flat(ui, "Scene Catalog..."))
    {
      ldki_editor_scene_catalog_open(editor);
      ldk_ui_close_current_popup(ui);
    }

    ldk_ui_set_next_disabled(ui, can_not_build);
    if (ldk_ui_button_flat(ui, "Build"))
    {
      ldki_editor_project_build_request(editor);
      ldk_ui_close_current_popup(ui);
    }

    ldk_ui_set_next_disabled(ui, !editor->project_build.active);
    if (ldk_ui_button_flat(ui, "Cancel Build"))
    {
      ldki_editor_project_build_cancel_request(editor);
      ldk_ui_close_current_popup(ui);
    }

    ldk_ui_set_next_disabled(ui, can_not_build);
    if (ldk_ui_button_flat(ui, "Build Launcher"))
    {
      ldki_editor_project_release_request(editor);
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
    if (ldk_ui_button_flat(ui, "Add Cone"))
    {
      ldki_editor_scene_add_primitive(editor, LDK_MESH_PRIMITIVE_CONE, "Cone");
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
      ldki_editor_scene_add_primitive(editor, LDK_MESH_PRIMITIVE_CAPSULE, "Capsule");
      ldk_ui_close_current_popup(ui);
    }

    ldk_ui_set_next_disabled(ui, !can_add);
    if (ldk_ui_button_flat(ui, "Add Plane"))
    {
      ldki_editor_scene_add_primitive(editor, LDK_MESH_PRIMITIVE_PLANE, "Plane");
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

  popup_pos.x = window_button_rect.x;
  popup_pos.y = window_button_rect.y + window_button_rect.h;

  ldk_ui_begin_popup(ui, MENU_ID_WINDOW);
  {
    LDKUIMark mark = ldk_ui_mark(ui);

    if (ldk_ui_button_flat(ui, "Entity Group"))
    {
      ldki_editor_grouping_catalog_open(editor);
      ldk_ui_close_current_popup(ui);
    }

    u32 window_count = ldki_editor_window_count();

    for (u32 i = 0; i < window_count; ++i)
    {
      const LDKEditorWindow *window = ldki_editor_window_at(i);

      if (window == NULL)
      {
        continue;
      }

      if (ldk_ui_button_flat(ui, window->title))
      {
        ldki_editor_window_show(window->id);
        ldk_ui_close_current_popup(ui);
      }
    }

    LDKUIRect content_rect = ldk_ui_measure_from(ui, mark);
  }
  ldk_ui_end_popup(ui);

  popup_pos.x = theme_button_rect.x;
  popup_pos.y = theme_button_rect.y + theme_button_rect.h;

  ldk_ui_begin_popup(ui, MENU_ID_THEME);
  {
    LDKUIMark mark = ldk_ui_mark(ui);
    ldki_editor_theme_menu_show(editor);
    LDKUIRect content_rect = ldk_ui_measure_from(ui, mark);
  }
  ldk_ui_end_popup(ui);

  ldk_ui_end_window(ui);
}

static LDKUIIcon s_editor_status_message_icon(
    LDKEditorContext *editor, LDKEditorConsoleEntryType type)
{
  LDKUIIcon icon = {0};

  if (editor == NULL || type == LDK_EDITOR_CONSOLE_ENTRY_RAW)
  {
    return icon;
  }

  icon.size = ldk_sizef(
      LDK_UI_DEFAULT_CONTROL_HEIGHT, LDK_UI_DEFAULT_CONTROL_HEIGHT);
  icon.texture =
      ldk_renderer_texture_ui_handle(editor->renderer, editor->ui_atlas);
  icon.color = editor->ui.theme.colors[LDK_UI_COLOR_TEXT];

  if (type == LDK_EDITOR_CONSOLE_ENTRY_INFO)
  {
    icon.uv = ldk_editor_icon_rects[LDK_EDITOR_ICON_INFO];
  }
  else if (type == LDK_EDITOR_CONSOLE_ENTRY_WARNING)
  {
    icon.uv = ldk_editor_icon_rects[LDK_EDITOR_ICON_WARNING];
    icon.color = LDK_EDITOR_COLOR_ICON_WARNING;
  }
  else if (type == LDK_EDITOR_CONSOLE_ENTRY_ERROR)
  {
    icon.uv = ldk_editor_icon_rects[LDK_EDITOR_ICON_ERROR];
    icon.color = LDK_EDITOR_COLOR_ICON_ERROR;
  }

  return icon;
}

static const char *s_editor_job_action_label(
    LDKEditorProjectActionType action_type)
{
  switch (action_type)
  {
  case LDK_EDITOR_PROJECT_ACTION_CREATE:
    return "Creating project";
  case LDK_EDITOR_PROJECT_ACTION_BUILD:
    return "Building game DLL";
  case LDK_EDITOR_PROJECT_ACTION_RELEASE:
    return "Building game launcher";
  case LDK_EDITOR_PROJECT_ACTION_PACKAGE:
    return "Packaging game";
  default:
    return "Project task";
  }
}

static const char *s_editor_job_status_label(LDKEditorJobStatus status)
{
  switch (status)
  {
  case LDK_EDITOR_JOB_STATUS_BUSY:
    return "BUSY";
  case LDK_EDITOR_JOB_STATUS_DONE:
    return "DONE";
  case LDK_EDITOR_JOB_STATUS_FAILED:
    return "FAILED";
  case LDK_EDITOR_JOB_STATUS_CANCELLED:
    return "CANCELLED";
  default:
    return "";
  }
}

static const char *s_editor_job_stage_label(LDKEditorProjectBuildStage stage)
{
  switch (stage)
  {
  case LDK_EDITOR_PROJECT_BUILD_STAGE_GAME_CONFIGURE:
    return "CMake configure";
  case LDK_EDITOR_PROJECT_BUILD_STAGE_GAME_BUILD:
    return "Game build";
  case LDK_EDITOR_PROJECT_BUILD_STAGE_RELEASE_CONFIGURE:
    return "Release configure";
  case LDK_EDITOR_PROJECT_BUILD_STAGE_RELEASE_BUILD:
    return "Release build";
  case LDK_EDITOR_PROJECT_BUILD_STAGE_PACKAGE:
    return "Package build";
  default:
    return "";
  }
}

static void s_editor_jobs_popup(LDKEditorContext *editor, LDKUIId popup_id)
{
  LDKUIContext *ui = &editor->ui;

  if (!ldk_ui_begin_popup(ui, popup_id))
  {
    return;
  }

  ldk_ui_set_next_width(ui, ldk_ui_px(420.0f));
  ldk_ui_label(ui, "Processes");
  ldk_ui_horizontal_line(ui);

  if (editor->project_build.active)
  {
    char label[256];
    const char *action_label =
        s_editor_job_action_label(editor->project_build.action_type);
    const char *stage_label =
        s_editor_job_stage_label(editor->project_build.stage);

    if (stage_label[0] != 0)
    {
      snprintf(label, sizeof(label), "%s - %s", action_label, stage_label);
    }
    else
    {
      snprintf(label, sizeof(label), "%s", action_label);
    }

    ldk_ui_begin_horizontal(ui);
    ldk_ui_set_next_width(ui, ldk_ui_px(260.0f));
    ldk_ui_label(ui, label);
    ldk_ui_set_next_width(ui, ldk_ui_px(70.0f));
    ldk_ui_label(ui, s_editor_job_status_label(LDK_EDITOR_JOB_STATUS_BUSY));
    ldk_ui_set_next_width(ui, ldk_ui_px(80.0f));
    ldk_ui_set_next_disabled(ui, editor->project_build.cancel_requested);
    if (ldk_ui_button(ui, "Cancel"))
    {
      ldki_editor_project_build_cancel_request(editor);
    }
    ldk_ui_end_horizontal(ui);
  }

  for (u32 i = 0; i < editor->job_history_count; ++i)
  {
    const LDKEditorJobHistoryEntry *entry = &editor->job_history[i];

    ldk_ui_begin_horizontal(ui);
    ldk_ui_set_next_width(ui, ldk_ui_px(330.0f));
    ldk_ui_label(ui, s_editor_job_action_label(entry->action_type));
    ldk_ui_set_next_width(ui, ldk_ui_px(80.0f));
    ldk_ui_label(ui, s_editor_job_status_label(entry->status));
    ldk_ui_end_horizontal(ui);
  }

  ldk_ui_end_popup(ui);
}

static void s_editor_status_bar(LDKEditorContext *editor)
{
  static u8 alpha = 0;
  static double acc = 0.0f;
  const LDKUIId jobs_popup_id = 0x4A4F4253u;

  LDKUIContext *ui = &editor->ui;
  LDKEditorConsoleEntryType message_type = LDK_EDITOR_CONSOLE_ENTRY_RAW;
  const char *message =
      ldki_editor_console_last_message_get(editor, &message_type);
  LDKUIIcon message_icon =
      s_editor_status_message_icon(editor, message_type);
  LDKUIIcon build_icon = {0};
  bool has_jobs =
      editor->project_build.active || editor->job_history_count > 0;

  build_icon.size = ldk_sizef(LDK_UI_DEFAULT_CONTROL_HEIGHT,
      LDK_UI_DEFAULT_CONTROL_HEIGHT);
  build_icon.texture =
      ldk_renderer_texture_ui_handle(editor->renderer, editor->ui_atlas);
  build_icon.color = ui->theme.colors[LDK_UI_COLOR_CONTROL_TEXT];
  build_icon.uv = ldk_editor_icon_rects[LDK_EDITOR_ICON_HEXAGON];

  if (!editor->project_build.active && editor->job_history_count > 0)
  {
    if (editor->job_history[0].status == LDK_EDITOR_JOB_STATUS_FAILED)
    {
      build_icon.color = LDK_EDITOR_COLOR_ICON_ERROR;
    }
    else if (editor->job_history[0].status == LDK_EDITOR_JOB_STATUS_CANCELLED)
    {
      build_icon.color = LDK_EDITOR_COLOR_ICON_WARNING;
    }
  }

  const u32 status_bar_height = LDK_EDITOR_STATUS_BAR_HEIGHT;
  LDKUIRect rect = {0, ui->viewport.h - status_bar_height, ui->viewport.w,
      status_bar_height};

  ldk_ui_begin_window(ui, "", rect, 0);
  ldk_ui_horizontal_line(ui);
  ldk_ui_begin_horizontal(ui);

  if (message != NULL && message[0] != 0)
  {
    ldk_ui_set_next_weight(ui, 1.0f);
    ldk_ui_icon_label(ui, message_icon, message);
  }
  else
  {
    ldk_ui_spacer(ui);
  }

  if (has_jobs)
  {
    bool open_popup;
    LDKUIRect icon_rect;

    if (editor->project_build.active)
    {
      acc += editor->ui.delta_time * 2;
      alpha = (u8)(127 + (127 * sinf(acc)));

      build_icon.color &= 0xFFFFFF00;
      build_icon.color |= alpha;
    }

    ldk_ui_set_next_weight(ui, 0.0f);
    open_popup = ldk_ui_icon_button(ui, build_icon, NULL);
    icon_rect = ldk_ui_last_rect(ui);

    if (open_popup && ldk_ui_popup_is_open(ui, jobs_popup_id))
    {
      ldk_ui_close_popup(ui, jobs_popup_id);
    }
    else if (open_popup || editor->jobs_popup_open_requested)
    {
      u32 popup_row_count = editor->job_history_count +
                            (editor->project_build.active ? 1u : 0u);
      float popup_height =
          LDK_UI_DEFAULT_CONTROL_HEIGHT * (2.0f + (float)popup_row_count) +
          LDK_UI_DEFAULT_PADDING * 4.0f;
      LDKUIPoint popup_position = {icon_rect.x + icon_rect.w - 420.0f,
          icon_rect.y - popup_height};
      ldk_ui_open_popup_at(ui, jobs_popup_id, popup_position);
      editor->jobs_popup_open_requested = false;
    }
  }

  ldk_ui_end_horizontal(ui);
  ldk_ui_end_window(ui);

  s_editor_jobs_popup(editor, jobs_popup_id);
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

static bool s_editor_push_button(
  LDKUIContext *ui, const char *text, LDKUIIcon icon, bool pushed)
{
  if (ui == NULL)
  {
    return false;
  }

  rgba32 control_bg = ui->theme.colors[LDK_UI_COLOR_CONTROL_BG];
  rgba32 control_bg_hovered =
      ui->theme.colors[LDK_UI_COLOR_CONTROL_BG_HOVERED];
  rgba32 control_text = ui->theme.colors[LDK_UI_COLOR_CONTROL_TEXT];
  rgba32 control_text_hovered =
      ui->theme.colors[LDK_UI_COLOR_CONTROL_TEXT_HOVERED];
  rgba32 control_border = ui->theme.colors[LDK_UI_COLOR_CONTROL_BORDER];
  rgba32 control_border_hovered =
      ui->theme.colors[LDK_UI_COLOR_CONTROL_BORDER_HOVERED];

  icon.color = control_text;

  if (pushed)
  {
    ui->theme.colors[LDK_UI_COLOR_CONTROL_BG] =
        ui->theme.colors[LDK_UI_COLOR_CONTROL_BG_ACTIVE];
    ui->theme.colors[LDK_UI_COLOR_CONTROL_BG_HOVERED] =
        ui->theme.colors[LDK_UI_COLOR_CONTROL_BG_ACTIVE];
    ui->theme.colors[LDK_UI_COLOR_CONTROL_TEXT] =
        ui->theme.colors[LDK_UI_COLOR_CONTROL_TEXT_ACTIVE];
    ui->theme.colors[LDK_UI_COLOR_CONTROL_TEXT_HOVERED] =
        ui->theme.colors[LDK_UI_COLOR_CONTROL_TEXT_ACTIVE];
    ui->theme.colors[LDK_UI_COLOR_CONTROL_BORDER] =
        ui->theme.colors[LDK_UI_COLOR_CONTROL_BORDER_ACTIVE];
    ui->theme.colors[LDK_UI_COLOR_CONTROL_BORDER_HOVERED] =
        ui->theme.colors[LDK_UI_COLOR_CONTROL_BORDER_ACTIVE];
  }

  bool clicked = ldk_ui_icon_button(ui, icon, text);

  ui->theme.colors[LDK_UI_COLOR_CONTROL_BG] = control_bg;
  ui->theme.colors[LDK_UI_COLOR_CONTROL_BG_HOVERED] = control_bg_hovered;
  ui->theme.colors[LDK_UI_COLOR_CONTROL_TEXT] = control_text;
  ui->theme.colors[LDK_UI_COLOR_CONTROL_TEXT_HOVERED] = control_text_hovered;
  ui->theme.colors[LDK_UI_COLOR_CONTROL_BORDER] = control_border;
  ui->theme.colors[LDK_UI_COLOR_CONTROL_BORDER_HOVERED] =
      control_border_hovered;

  return clicked;
}

static void s_editor_gizmo_space_buttons(LDKEditorContext *editor)
{
  static const LDKEditorIcon items[] = {
    LDK_EDITOR_ICON_TOOL_MODE_GLOBAL,
    LDK_EDITOR_ICON_TOOL_MODE_LOCAL,
  };

  if (editor == NULL)
  {
    return;
  }

  LDKUIContext *ui = &editor->ui;
  u32 item_count = (u32)(sizeof(items) / sizeof(items[0]));
  u32 selected_index = editor->gizmo.mode == LDK_EDITOR_GIZMO_MODE_SCALE
                           ? (u32)LDK_EDITOR_GIZMO_SPACE_LOCAL
                           : (u32)editor->gizmo.space;

  if (selected_index >= item_count)
  {
    selected_index = (u32)LDK_EDITOR_GIZMO_SPACE_GLOBAL;
  }

  ldk_ui_push_id_cstr(ui, "gizmo-space");
  ldk_ui_begin_disabled(ui,
      editor->gizmo.dragging ||
          editor->gizmo.mode == LDK_EDITOR_GIZMO_MODE_SCALE ||
          editor->gizmo.mode == LDK_EDITOR_GIZMO_MODE_PAN);

  LDKUIIcon icon;
  icon.texture = ldk_renderer_texture_ui_handle(editor->renderer, editor->ui_atlas);
  icon.size.w = icon.size.h = 24;

  for (u32 i = 0; i < item_count; ++i)
  {
    icon.uv = ldk_editor_icon_rects[items[i]];

    ldk_ui_set_next_width(ui, ldk_ui_px(24.0f + 2 * LDK_UI_DEFAULT_PADDING));
    if (s_editor_push_button(ui, NULL, icon, i == selected_index))
    {
      editor->gizmo.space = (LDKEditorGizmoSpace)i;
    }
  }

  ldk_ui_end_disabled(ui);
  ldk_ui_pop_id(ui);
}

static void s_editor_gizmo_mode_buttons(LDKEditorContext *editor)
{
  static const LDKEditorIcon items[] = {
      LDK_EDITOR_ICON_PANTOOL, LDK_EDITOR_ICON_MOVETOOL,
      LDK_EDITOR_ICON_ROTATETOOL, LDK_EDITOR_ICON_SCALETOOL};
  static const LDKEditorGizmoMode modes[] = {
      LDK_EDITOR_GIZMO_MODE_PAN, LDK_EDITOR_GIZMO_MODE_TRANSLATE,
      LDK_EDITOR_GIZMO_MODE_ROTATE, LDK_EDITOR_GIZMO_MODE_SCALE};

  if (editor == NULL)
  {
    return;
  }

  LDKUIContext *ui = &editor->ui;
  LDKUIIcon icon;
  icon.texture = ldk_renderer_texture_ui_handle(editor->renderer, editor->ui_atlas);
  icon.size.w = icon.size.h = 24;

  u32 item_count = (u32)(sizeof(items) / sizeof(items[0]));
  u32 selected_index = 1;
  for (u32 i = 0; i < item_count; ++i)
  {
    if (modes[i] == editor->gizmo.mode)
    {
      selected_index = i;
      break;
    }
  }

  ldk_ui_push_id_cstr(ui, "gizmo-mode");
  ldk_ui_begin_disabled(ui, editor->gizmo.dragging ||
      editor->camera_controller.panning || editor->camera_controller.orbiting);

  for (u32 i = 0; i < item_count; ++i)
  {
    icon.uv = ldk_editor_icon_rects[items[i]];
    ldk_ui_set_next_width(ui, ldk_ui_px(24.0f + 2 * LDK_UI_DEFAULT_PADDING));
    if (s_editor_push_button(ui, NULL, icon, i == selected_index))
    {
      editor->gizmo.mode = modes[i];
    }
  }

  ldk_ui_end_disabled(ui);
  ldk_ui_pop_id(ui);
}

static void s_editor_scene_view_camera_buttons(LDKEditorContext *editor)
{
  LDKUIContext *ui;
  LDKECS *ecs;
  LDKCamera *camera;
  LDKEntity selected = x_handle_null();
  bool has_selection;

  if (editor == NULL)
  {
    return;
  }

  ui = &editor->ui;
  ecs = ldk_module_get(LDK_MODULE_ECS);
  camera = ldk_ecs_component_get(
      editor->editor_camera, LDK_COMPONENT_TYPE_CAMERA);
  has_selection = ecs != NULL &&
                  ldki_editor_selected_entity_get(editor, ecs, &selected) &&
                  !ldk_entity_internal_flags_has(
                      &ecs->entity, selected, LDK_ENTITY_INTERNAL_EDITOR);

  ldk_ui_push_id_cstr(ui, "scene-camera");

  ldk_ui_begin_disabled(ui, !has_selection);
  ldk_ui_set_next_width(ui, ldk_ui_px(76.0f));
  if (ldk_ui_button(ui, "Focus (F)"))
  {
    ldki_editor_camera_focus_selected(editor);
  }
  ldk_ui_end_disabled(ui);

  ldk_ui_begin_disabled(ui, camera == NULL);
  ldk_ui_set_next_width(ui, ldk_ui_px(104.0f));
  if (ldk_ui_button(ui,
          camera != NULL &&
                  camera->projection == LDK_CAMERA_PROJECTION_ORTHOGRAPHIC
              ? "Orthographic"
              : "Perspective"))
  {
    ldki_editor_camera_projection_toggle(editor);
  }
  ldk_ui_end_disabled(ui);

  ldk_ui_begin_disabled(
      ui, !has_selection || editor->editor_state != LDK_EDITOR_STATE_STOPED);
  ldk_ui_set_next_width(ui, ldk_ui_px(112.0f));
  if (ldk_ui_button(ui, "Align with View"))
  {
    ldki_editor_selected_align_with_view(editor);
  }
  ldk_ui_end_disabled(ui);

  ldk_ui_pop_id(ui);
}

void ldki_editor_scene_view_toolbar_show(LDKEditorContext *editor)
{
  if (editor == NULL)
  {
    return;
  }

  LDKUIContext *ui = &editor->ui;

  ldk_ui_set_next_height(ui, ldk_ui_px(LDK_UI_DEFAULT_CONTROL_HEIGHT + 2 * LDK_UI_DEFAULT_PADDING) );
  ldk_ui_begin_horizontal(ui);
  s_editor_gizmo_mode_buttons(editor);
  ldk_ui_set_next_width(ui, ldk_ui_px(LDK_UI_DEFAULT_SPACING * 2.0f));
  ldk_ui_spacer(ui);
  s_editor_gizmo_space_buttons(editor);
  ldk_ui_set_next_width(ui, ldk_ui_px(LDK_UI_DEFAULT_SPACING * 2.0f));
  ldk_ui_spacer(ui);
  s_editor_scene_view_camera_buttons(editor);
  ldk_ui_spacer(ui);
  ldk_ui_end_horizontal(ui);
}

static bool s_editor_ini_bool_write(
    const XFSPath *path, const char *key, bool value)
{
  XIni ini = {0};
  XIniError error = {0};
  bool result;

  if (path == NULL || key == NULL || !x_ini_load_file(path->buf, &ini, &error))
  {
    return false;
  }

  result = x_ini_set_bool(&ini, ".editor", key, value) &&
           x_ini_write_file(path->buf, &ini, &error);
  x_ini_free(&ini);
  return result;
}

typedef struct LDKEditorSettingsDraft
{
  bool initialized;
  bool dirty;
  bool restore_last_project;
  bool open_folders_single_click;
  char ui_scale[32];
  char font_size[32];
  char font_path[X_FS_PATH_MAX_LENGTH];
  LDKEditorFileAssociation
      file_associations[LDK_EDITOR_FILE_ASSOCIATION_CAPACITY];
  u32 file_association_count;
} LDKEditorSettingsDraft;

static void s_editor_settings_association_name_suggest(
    LDKEditorFileAssociation *association)
{
  XSlice stem;
  size_t length;

  if (association == NULL || association->program[0] == 0 ||
      (association->name[0] != 0 &&
          strcmp(association->name, "New program") != 0))
  {
    return;
  }

  stem = x_fs_path_stem_cstr(association->program);
  length = stem.length;
  if (length == 0)
  {
    return;
  }

  if (length >= sizeof(association->name))
  {
    length = sizeof(association->name) - 1u;
  }

  memcpy(association->name, stem.ptr, length);
  association->name[length] = 0;
}

static void s_editor_settings_draft_reset(
    LDKEditorSettingsDraft *draft, const LDKEditorContext *editor)
{
  if (draft == NULL || editor == NULL)
  {
    return;
  }

  memset(draft, 0, sizeof(*draft));
  draft->initialized = true;
  draft->restore_last_project = editor->restore_last_project;
  draft->open_folders_single_click =
      editor->file_explorer_open_folders_single_click;
  snprintf(draft->ui_scale, sizeof(draft->ui_scale), "%.2f",
      editor->editor_ui_scale);
  snprintf(draft->font_size, sizeof(draft->font_size), "%d",
      editor->editor_font_size);
  snprintf(draft->font_path, sizeof(draft->font_path), "%s",
      editor->editor_font.buf);
  draft->file_association_count = editor->file_association_count;
  memcpy(draft->file_associations, editor->file_associations,
      sizeof(draft->file_associations));
}

static bool s_editor_settings_number_parse(
    const char *text, float *out_value)
{
  char *end = NULL;
  float value;

  if (text == NULL || out_value == NULL)
  {
    return false;
  }

  value = strtof(text, &end);
  if (end == text)
  {
    return false;
  }

  while (*end != 0 && isspace((u8)*end))
  {
    ++end;
  }

  if (*end != 0 || !isfinite(value))
  {
    return false;
  }

  *out_value = value;
  return true;
}

static bool s_editor_settings_integer_parse(
    const char *text, i32 *out_value)
{
  char *end = NULL;
  long value;

  if (text == NULL || out_value == NULL)
  {
    return false;
  }

  value = strtol(text, &end, 10);
  if (end == text)
  {
    return false;
  }

  while (*end != 0 && isspace((u8)*end))
  {
    ++end;
  }

  if (*end != 0 || value < INT32_MIN || value > INT32_MAX)
  {
    return false;
  }

  *out_value = (i32)value;
  return true;
}

static bool s_editor_settings_save(LDKEditorContext *editor,
    LDKEditorSettingsDraft *draft)
{
  XIni ini = {0};
  XIniError error = {0};
  const char *association_prefix = ".file_association.";
  size_t association_prefix_length = strlen(association_prefix);
  float ui_scale;
  i32 font_size;
  XFSPath old_font_path;
  LDKAssetFont old_font;
  LDKFontInstance *old_font_instance;
  i32 old_font_size;
  bool font_changed;
  bool ok;

  if (editor == NULL || draft == NULL ||
      editor->editor_config_path.length == 0 ||
      !s_editor_settings_number_parse(draft->ui_scale, &ui_scale) ||
      ui_scale < 0.5f || ui_scale > 3.0f ||
      !s_editor_settings_integer_parse(draft->font_size, &font_size) ||
      font_size < 6 || font_size > 96 || draft->font_path[0] == 0)
  {
    return false;
  }

  old_font_path = editor->editor_font;
  old_font = editor->font;
  old_font_instance = editor->font_instance;
  old_font_size = editor->editor_font_size;
  font_changed = strcmp(editor->editor_font.buf, draft->font_path) != 0 ||
                 editor->editor_font_size != font_size;

  if (font_changed &&
      !ldki_editor_font_apply(editor, draft->font_path, font_size))
  {
    return false;
  }

  if (!x_ini_load_file(editor->editor_config_path.buf, &ini, &error))
  {
    if (font_changed)
    {
      editor->font = old_font;
      editor->font_instance = old_font_instance;
      editor->ui.font = old_font_instance;
      editor->editor_font = old_font_path;
      editor->editor_font_size = old_font_size;
    }
    return false;
  }

  for (i32 section_i = x_ini_section_count(&ini) - 1; section_i >= 0;
       --section_i)
  {
    const char *section = x_ini_section_name(&ini, section_i);
    if (section != NULL &&
        strncmp(section, association_prefix, association_prefix_length) == 0)
    {
      char section_name[128];
      snprintf(section_name, sizeof(section_name), "%s", section);
      x_ini_remove_section(&ini, section_name);
    }
  }

  ok = x_ini_set_bool(&ini, ".editor", "restore_last_project",
           draft->restore_last_project) &&
       x_ini_set_bool(&ini, ".editor",
           "file_explorer_open_folders_single_click",
           draft->open_folders_single_click) &&
       x_ini_set_f32(&ini, ".editor", "ui_scale", ui_scale) &&
       x_ini_set_i32(&ini, ".editor", "font_size", font_size) &&
       x_ini_set(&ini, ".editor", "font", draft->font_path);

  for (u32 i = 0; ok && i < draft->file_association_count; ++i)
  {
    char section[64];
    LDKEditorFileAssociation *association = &draft->file_associations[i];

    snprintf(section, sizeof(section), ".file_association.%u", i);

    ok = x_ini_set(&ini, section, "name", association->name) &&
         x_ini_set(&ini, section, "program", association->program) &&
         x_ini_set(&ini, section, "arguments", association->arguments) &&
         x_ini_set(&ini, section, "extensions", association->extensions);
  }

  if (ok)
  {
    ok = x_ini_write_file(editor->editor_config_path.buf, &ini, &error);
  }
  x_ini_free(&ini);

  if (!ok)
  {
    if (font_changed)
    {
      editor->font = old_font;
      editor->font_instance = old_font_instance;
      editor->ui.font = old_font_instance;
      editor->editor_font = old_font_path;
      editor->editor_font_size = old_font_size;
    }
    return false;
  }

  editor->restore_last_project = draft->restore_last_project;
  editor->file_explorer_open_folders_single_click =
      draft->open_folders_single_click;
  editor->editor_ui_scale = ui_scale;
  editor->editor_font_size = font_size;
  x_fs_path_set(&editor->editor_font, draft->font_path);
  if (x_fs_path_is_absolute_cstr(editor->editor_font.buf))
  {
    x_fs_path_normalize(&editor->editor_font);
  }
  editor->file_association_count = draft->file_association_count;
  memcpy(editor->file_associations, draft->file_associations,
      sizeof(editor->file_associations));

  snprintf(draft->font_path, sizeof(draft->font_path), "%s",
      editor->editor_font.buf);
  snprintf(draft->ui_scale, sizeof(draft->ui_scale), "%.2f",
      editor->editor_ui_scale);
  snprintf(draft->font_size, sizeof(draft->font_size), "%d",
      editor->editor_font_size);
  draft->dirty = false;
  return true;
}

static u32 s_editor_settings_input_row(LDKUIContext *ui, const char *label,
    char *buffer, u32 buffer_size)
{
  u32 result;

  ldk_ui_begin_horizontal(ui);
  ldk_ui_set_next_weight(ui, 0.0f);
  ldk_ui_label(ui, label);
  result = ldk_ui_input_box(ui, buffer, buffer_size);
  ldk_ui_end_horizontal(ui);
  return result;
}

static u32 s_editor_settings_browse_row(LDKEditorContext *editor,
    LDKUIContext *ui, const char *label, char *buffer, u32 buffer_size,
    const char *dialog_title, const char *filter)
{
  u32 result = 0;

  ldk_ui_begin_horizontal(ui);
  ldk_ui_set_next_weight(ui, 0.0f);
  ldk_ui_label(ui, label);
  result = ldk_ui_input_box(ui, buffer, buffer_size);
  ldk_ui_set_next_weight(ui, 0.0f);
  if (ldk_ui_button(ui, "..."))
  {
    char selected[X_FS_PATH_MAX_LENGTH] = {0};
    if (ldk_os_dialog_show_open_file(editor->window, dialog_title, filter,
            selected, sizeof(selected)))
    {
      XFSPath path = {0};
      x_fs_path_set(&path, selected);
      x_fs_path_normalize(&path);
      snprintf(buffer, buffer_size, "%s", path.buf);
      result |= LDK_UI_INPUT_BOX_CHANGED | LDK_UI_INPUT_BOX_COMMITTED;
    }
  }
  ldk_ui_end_horizontal(ui);
  return result;
}

void ldki_editor_settings_show(LDKEditor *opaque_editor, void *data)
{
  static LDKUIPoint scroll = {0};
  static LDKEditorSettingsDraft draft = {0};
  static bool external_programs_expanded = true;
  static bool look_and_feel_expanded = true;
  static bool general_expanded = true;
  static bool program_expanded[LDK_EDITOR_FILE_ASSOCIATION_CAPACITY] = {0};
  static bool expansion_initialized = false;
  LDKEditorContext *editor = (LDKEditorContext *)opaque_editor;
  LDKUIContext *ui;
  bool delete_requested = false;
  u32 delete_index = 0;
  (void)data;

  if (editor == NULL)
  {
    return;
  }

  if (!draft.initialized)
  {
    s_editor_settings_draft_reset(&draft, editor);
  }

  if (!expansion_initialized)
  {
    for (u32 i = 0; i < LDK_EDITOR_FILE_ASSOCIATION_CAPACITY; ++i)
    {
      program_expanded[i] = true;
    }
    expansion_initialized = true;
  }

  ui = &editor->ui;
  scroll = ldk_ui_begin_scrollview(
      ui, scroll, LDK_UI_SCROLL_VERTICAL | LDK_UI_SCROLL_IF_NEEDED);

  general_expanded =
      ldk_ui_tree_node(ui, "General", general_expanded, 0, 0);
  if (general_expanded)
  {
    bool restore_last_project;
    ldk_ui_begin_horizontal(ui);
    ldk_ui_set_next_weight(ui, 0.0f);
    ldk_ui_label(ui, "Restore last project");
    ldk_ui_set_next_weight(ui, 0.0f);
    restore_last_project =
        ldk_ui_toggle(ui, draft.restore_last_project);
    if (restore_last_project != draft.restore_last_project)
    {
      draft.restore_last_project = restore_last_project;
      draft.dirty = true;
    }
    ldk_ui_spacer(ui);
    ldk_ui_end_horizontal(ui);
  }

  ldk_ui_spacer(ui);
  ldk_ui_begin_horizontal(ui);
  ldk_ui_set_next_weight(ui, 1.0f);
  external_programs_expanded = ldk_ui_tree_node(
      ui, "External programs", external_programs_expanded, 0, 0);
  ldk_ui_set_next_disabled(ui,
      draft.file_association_count >= LDK_EDITOR_FILE_ASSOCIATION_CAPACITY);
  ldk_ui_set_next_weight(ui, 0.0f);
  if (ldk_ui_button(ui, "Add"))
  {
    u32 index = draft.file_association_count++;
    LDKEditorFileAssociation *association = &draft.file_associations[index];
    memset(association, 0, sizeof(*association));
    snprintf(association->name, sizeof(association->name), "New program");
    snprintf(association->arguments, sizeof(association->arguments),
        "%%file%%");
    program_expanded[index] = true;
    external_programs_expanded = true;
    draft.dirty = true;
  }
  ldk_ui_end_horizontal(ui);

  if (external_programs_expanded)
  {
    ldk_ui_label(ui,
        "Use %file% in Arguments. Extensions may be separated by spaces, "
        "commas, or semicolons.");

    for (u32 i = 0; i < draft.file_association_count; ++i)
    {
      LDKEditorFileAssociation *association = &draft.file_associations[i];
      u32 result = 0;
      const char *title =
          association->name[0] != 0 ? association->name : "New program";

      ldk_ui_push_id_u32(ui, i + 1u);
      ldk_ui_begin_horizontal(ui);
      ldk_ui_set_next_weight(ui, 1.0f);
      program_expanded[i] =
          ldk_ui_tree_node(ui, title, program_expanded[i], 1, 0);
      ldk_ui_set_next_weight(ui, 0.0f);
      if (ldk_ui_button(ui, "Remove"))
      {
        delete_requested = true;
        delete_index = i;
      }
      ldk_ui_end_horizontal(ui);

      if (program_expanded[i])
      {
        u32 program_result;

        result |= s_editor_settings_input_row(ui, "Name", association->name,
            (u32)sizeof(association->name));
        program_result = s_editor_settings_browse_row(editor, ui, "Program",
            association->program, (u32)sizeof(association->program),
            "Choose Program", "Programs\0*.exe\0All Files\0*.*\0\0");
        if ((program_result & LDK_UI_INPUT_BOX_CHANGED) != 0)
        {
          s_editor_settings_association_name_suggest(association);
        }
        result |= program_result;
        result |= s_editor_settings_input_row(ui, "Arguments",
            association->arguments, (u32)sizeof(association->arguments));
        result |= s_editor_settings_input_row(ui, "Associations",
            association->extensions, (u32)sizeof(association->extensions));
      }
      ldk_ui_pop_id(ui);

      if ((result & LDK_UI_INPUT_BOX_CHANGED) != 0)
      {
        draft.dirty = true;
      }

      if (delete_requested)
      {
        break;
      }
    }
  }

  if (delete_requested && delete_index < draft.file_association_count)
  {
    for (u32 i = delete_index; i + 1 < draft.file_association_count; ++i)
    {
      draft.file_associations[i] = draft.file_associations[i + 1];
      program_expanded[i] = program_expanded[i + 1];
    }
    draft.file_association_count -= 1;
    memset(&draft.file_associations[draft.file_association_count], 0,
        sizeof(draft.file_associations[draft.file_association_count]));
    program_expanded[draft.file_association_count] = true;
    draft.dirty = true;
  }

  ldk_ui_spacer(ui);
  look_and_feel_expanded =
      ldk_ui_tree_node(ui, "Look and feel", look_and_feel_expanded, 0, 0);
  if (look_and_feel_expanded)
  {
    bool single_click;
    u32 result = 0;

    ldk_ui_begin_horizontal(ui);
    ldk_ui_set_next_weight(ui, 0.0f);
    ldk_ui_label(ui, "Open folders with single click");
    ldk_ui_set_next_weight(ui, 0.0f);
    single_click = ldk_ui_toggle(ui, draft.open_folders_single_click);
    if (single_click != draft.open_folders_single_click)
    {
      draft.open_folders_single_click = single_click;
      draft.dirty = true;
    }
    ldk_ui_spacer(ui);
    ldk_ui_end_horizontal(ui);

    result |= s_editor_settings_input_row(
        ui, "UI scale", draft.ui_scale, (u32)sizeof(draft.ui_scale));
    result |= s_editor_settings_input_row(
        ui, "Font size", draft.font_size, (u32)sizeof(draft.font_size));
    result |= s_editor_settings_browse_row(editor, ui, "Editor font",
        draft.font_path, (u32)sizeof(draft.font_path), "Choose Editor Font",
        "TrueType Font\0*.ttf\0All Files\0*.*\0\0");

    if ((result & LDK_UI_INPUT_BOX_CHANGED) != 0)
    {
      draft.dirty = true;
    }
  }

  ldk_ui_spacer(ui);
  ldk_ui_begin_horizontal(ui);
  ldk_ui_spacer(ui);
  ldk_ui_set_next_disabled(ui, !draft.dirty);
  ldk_ui_set_next_weight(ui, 0.0f);
  if (ldk_ui_button(ui, "Save"))
  {
    if (!s_editor_settings_save(editor, &draft))
    {
      ldki_editor_log_error(editor,
          "Failed to save editor settings. Check UI scale, font size, and font "
          "path.");
    }
    else
    {
      ldki_editor_log_info(editor, "Editor settings saved.");
    }
  }
  ldk_ui_end_horizontal(ui);

  ldk_ui_spacer(ui);
  ldk_ui_end_scrollview(ui);
}

static bool s_editor_project_play_current_scene_write(
    LDKEditorContext *editor, bool value)
{
  if (editor == NULL || !editor->project.loaded)
  {
    return false;
  }

  return s_editor_ini_bool_write(
      &editor->project.project_file_path, "play_current_scene", value);
}

static bool s_editor_project_build_on_play_write(
    LDKEditorContext *editor, bool value)
{
  if (editor == NULL || !editor->project.loaded)
  {
    return false;
  }

  return s_editor_ini_bool_write(
      &editor->project.project_file_path, "build_on_play", value);
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
    icon.color = editor->ui.theme.colors[LDK_UI_COLOR_CONTROL_TEXT];
    icon.size =
        ldk_sizef(LDK_UI_DEFAULT_CONTROL_HEIGHT, LDK_UI_DEFAULT_CONTROL_HEIGHT);
    icon.texture =
        ldk_renderer_texture_ui_handle(editor->renderer, editor->ui_atlas);

    ldk_ui_set_next_disabled(
        ui, !editor->project.loaded ||
                editor->editor_state != LDK_EDITOR_STATE_STOPED);
    static const char *const play_sources[] = {
        "Play Project", "Play Current Scene"};
    ldk_ui_set_next_width(ui, ldk_ui_px(168.0f));
    bool play_current_scene =
        ldk_ui_combo_box(ui, play_sources, 2,
            editor->project.play_current_scene ? 1 : 0) == 1;
    if (play_current_scene != editor->project.play_current_scene)
    {
      if (!s_editor_project_play_current_scene_write(editor, play_current_scene))
      {
        ldki_editor_log_warning(
            editor, "Could not save the project play mode.");
      }
      else
      {
        editor->project.play_current_scene = play_current_scene;
      }
    }

    ldk_ui_set_next_width(ui, ldk_ui_px(LDK_UI_DEFAULT_SPACING * 2.0f));
    ldk_ui_spacer(ui);
    ldk_ui_set_next_disabled(ui, !editor->project.loaded);
    ldk_ui_set_next_weight(ui, 0.0f);
    bool build_on_play = ldk_ui_toggle(ui, editor->project.build_on_play);
    if (build_on_play != editor->project.build_on_play)
    {
      if (!s_editor_project_build_on_play_write(editor, build_on_play))
      {
        ldki_editor_log_warning(
            editor, "Could not save the Build on Play preference.");
      }
      else
      {
        editor->project.build_on_play = build_on_play;
      }
    }
    ldk_ui_set_next_weight(ui, 0.0f);
    ldk_ui_label(ui, "Build on Play");

    // Play/Stop button
    if (editor->editor_state != LDK_EDITOR_STATE_PLAYING)
    {
      LDKSceneManager *manager = ldk_module_get(LDK_MODULE_SCENE_MANAGER);
      bool can_play = editor->project.loaded && !editor->project_build.active &&
                      editor->pending_project_action.type ==
                          LDK_EDITOR_PROJECT_ACTION_NONE &&
                      (editor->editor_state == LDK_EDITOR_STATE_PAUSED ||
                          (editor->project.play_current_scene
                                  ? editor->current_scene_path.length != 0
                                  : ldk_scene_manager_count(manager) != 0));
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

    if (editor->editor_state == LDK_EDITOR_STATE_PAUSED)
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

  ldk_ui_set_next_weight(ui, 0.0f);
  bool show_statistics = ldk_ui_toggle(ui, editor->show_statistics);
  if (show_statistics != editor->show_statistics)
  {
    if (!s_editor_ini_bool_write(
            &editor->editor_config_path, "show_statistics", show_statistics))
    {
      ldki_editor_log_warning(
          editor, "Could not save the Statistics preference.");
    }
    else
    {
      editor->show_statistics = show_statistics;
    }
  }
  ldk_ui_set_next_weight(ui, 0.0f);
  ldk_ui_label(ui, "Statistics");

  ldk_ui_set_next_weight(ui, 0.0f);
  bool profile = ldk_ui_toggle(ui, editor->profile);
  if (profile != editor->profile)
  {
    if (!s_editor_ini_bool_write(
            &editor->editor_config_path, "profile", profile))
    {
      ldki_editor_log_warning(
          editor, "Could not save the Profile preference.");
    }
    else
    {
      editor->profile = profile;
    }
  }
  ldk_ui_set_next_weight(ui, 0.0f);
  ldk_ui_label(ui, "Profile");

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
  char selected_path[X_FS_PATH_MAX_LENGTH] = {0};

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

//------------------------------------------------------------
// Internal
//------------------------------------------------------------

void ldki_editor_menubar_show(LDKEditorContext *editor)
{
  s_editor_menu_bar(editor);
}

void ldki_editor_status_show(LDKEditorContext *editor)
{
  s_editor_status_bar(editor);
}

void ldki_editor_project_create_show(LDKEditorContext *editor)
{
  typedef struct LDKEditorProjectGenerator
  {
    const char *cmake_generator;
    bool uses_platform;
  } LDKEditorProjectGenerator;

  static const LDKEditorProjectGenerator s_generators[] = {
      {"Visual Studio 18 2026", true},
      {"Visual Studio 17 2022", true},
      {"Ninja", false},
      {"Ninja Multi-Config", false},
      {"NMake Makefiles", false},
      {"MinGW Makefiles", false},
  };
  static const char *s_generator_labels[] = {
      "Visual Studio 2026",
      "Visual Studio 2022",
      "Ninja",
      "Ninja Multi-Config",
      "NMake",
      "MinGW Make",
  };
  static bool need_clean = false;
  static XSmallstr s_project_name = {0};
  static XFSPath s_project_path = {0};
  static u32 s_generator = 0;
  LDKUIContext *ui;
  const LDKEditorProjectGenerator *generator;
  const char *cmake_arch;

  if (editor == NULL)
  {
    return;
  }

  bool is_busy = editor->project_build.active;
  if (need_clean && !is_busy)
  {
    x_smallstr_clear(&s_project_name);
    x_smallstr_clear(&s_project_path);
    s_generator = 0;
    need_clean = false;
  }
 
  ui = &editor->ui;
  ldk_ui_begin_disabled(ui, is_busy);

  ldk_ui_set_next_height(ui, ldk_ui_px(LDK_UI_DEFAULT_CONTROL_HEIGHT));
  ldk_ui_begin_horizontal(ui);
  {
    ldk_ui_set_next_width(ui, ldk_ui_px(100.0f));
    ldk_ui_label(ui, "Project name");
    ldk_ui_input_box(ui, s_project_name.buf, (u32)sizeof(s_project_name.buf));
  }
  ldk_ui_end_horizontal(ui);

  ldk_ui_set_next_height(ui, ldk_ui_px(LDK_UI_DEFAULT_CONTROL_HEIGHT));
  ldk_ui_begin_horizontal(ui);
  {
    ldk_ui_set_next_width(ui, ldk_ui_px(100.0f));
    ldk_ui_label(ui, "Generator");
    s_generator = ldk_ui_combo_box(ui, s_generator_labels,
        (u32)(sizeof(s_generator_labels) / sizeof(s_generator_labels[0])),
        s_generator);
  }
  ldk_ui_end_horizontal(ui);

  if (s_generator >= sizeof(s_generators) / sizeof(s_generators[0]))
  {
    s_generator = 0;
  }
  generator = &s_generators[s_generator];

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
      need_clean = true;
      cmake_arch = generator->uses_platform
                       ? ldki_editor_cmake_native_arch_get()
                       : "";

      if (!ldki_editor_project_create_request(editor, s_project_name.buf,
              s_project_path.buf, generator->cmake_generator, cmake_arch))
      {
        ldki_editor_log_error(editor, "Failed to queue project creation.");
      }
    }

    ldk_ui_set_next_width(ui, ldk_ui_px(80.0f));
    
    if (ldk_ui_button(ui, "CANCEL"))
    {
      need_clean = true;
      editor->create_project_window_close_requested = true;
    }
  }
  ldk_ui_end_horizontal(ui);
  ldk_ui_end_disabled(ui);
}

void ldki_editor_project_create_window(LDKEditor *opaque_editor, void *data)
{
  (void)data;
  ldki_editor_project_create_show((LDKEditorContext *)opaque_editor);
}

void ldki_editor_toolbar_show(LDKEditorContext *editor)
{
  s_editor_tool_bar(editor);
}

void ldki_editor_log_error(LDKEditorContext *editor, const char *msg)
{
  ldki_editor_console_append(editor, LDK_EDITOR_CONSOLE_ENTRY_ERROR, msg);
  ldk_log_error(msg);
}

void ldki_editor_log_warning(LDKEditorContext *editor, const char *msg)
{
  ldki_editor_console_append(editor, LDK_EDITOR_CONSOLE_ENTRY_WARNING, msg);
  ldk_log_warning(msg);
}

void ldki_editor_log_info(LDKEditorContext *editor, const char *msg)
{
  ldki_editor_console_append(editor, LDK_EDITOR_CONSOLE_ENTRY_INFO, msg);
  ldk_log_info(msg);
}
