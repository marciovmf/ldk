#include "ldk_editor_atlas.h"
#include "ldk_editor_internal.h"
#include "ldk_editor_scene_ops.h"
#include "ldk_editor_theme.h"
#include "ldk_os.h"
#include "module/ldk_ui.h"
#include <module/ldk_audio.h>
#include <module/ldk_scene_manager.h>
#include <stdx/stdx_ini.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

const char *ldki_editor_console_last_message_get(LDKEditorContext *editor,
    LDKEditorConsoleEntryType *out_type);

//------------------------------------------------------------
// Menu bar
//------------------------------------------------------------

static void s_editor_menu_bar(LDKEditorContext *editor)
{
  LDKUIContext *ui = &editor->ui;

  ldki_editor_scene_state_sync(editor);

  LDKUIRect s_toolbar_rect = {ui->viewport.x, ui->viewport.y, ui->viewport.w,
      LDK_EDITOR_MENU_BAR_HEIGHT(ui)};
  static LDKUIRect s_file_popup_rect = {0, 0, 1024, 1024};
  static LDKUIRect s_edit_popup_rect = {0, 0, 1024, 1024};
  static LDKUIRect s_theme_popup_rect = {0, 0, 1024, 1024};

  const LDKUIId MENU_ID_FILE = 10;
  const LDKUIId MENU_ID_PROJECT = 11;
  const LDKUIId MENU_ID_THEME = 12;
  const LDKUIId MENU_ID_SCENE = 13;
  const LDKUIId MENU_ID_WINDOW = 14;

  ldk_ui_begin_window_fixed(ui, "TOOLBAR", s_toolbar_rect, 0);

  ldk_ui_begin_horizontal(ui);
  LDKUIMark mark = ldk_ui_mark(ui);

  ldk_ui_set_next_weight(ui, 0.0f);
  if (ldk_ui_button_flat(ui, "FILE"))
  {
    ldk_ui_open_popup(ui, MENU_ID_FILE);
  }
  LDKUIRect file_button_rect = ldk_ui_last_rect(ui);

  ldk_ui_set_next_weight(ui, 0.0f);
  if (ldk_ui_button_flat(ui, "PROJECT"))
  {
    ldk_ui_open_popup(ui, MENU_ID_PROJECT);
  }
  LDKUIRect edit_button_rect = ldk_ui_last_rect(ui);

  ldk_ui_set_next_weight(ui, 0.0f);
  if (ldk_ui_button_flat(ui, "SCENE"))
  {
    ldk_ui_open_popup(ui, MENU_ID_SCENE);
  }
  LDKUIRect scene_button_rect = ldk_ui_last_rect(ui);

  ldk_ui_set_next_weight(ui, 0.0f);
  if (ldk_ui_button_flat(ui, "WINDOW"))
  {
    ldk_ui_open_popup(ui, MENU_ID_WINDOW);
  }
  LDKUIRect window_button_rect = ldk_ui_last_rect(ui);

  ldk_ui_set_next_weight(ui, 0.0f);
  if (ldk_ui_button_flat(ui, "THEME"))
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
  

  if (ldk_ui_begin_popup(ui, MENU_ID_FILE))
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

    ldk_ui_set_next_disabled(ui, !editor->project.loaded);
    if (ldk_ui_button_flat(ui, "Preferences"))
    {
      ldki_editor_window_show(LDK_EDITOR_WINDOW_SETTINGS);
      ldk_ui_close_current_popup(ui);
    }

    ldk_ui_horizontal_line(ui);

    if (ldk_ui_button_flat(ui, "Exit"))
    {
      ldk_editor_quit(editor);
    }
    LDKUIRect content_rect = ldk_ui_measure_from(ui, mark);
    ldk_ui_end_popup(ui);
  }

  popup_pos.x = edit_button_rect.x;
  popup_pos.y = edit_button_rect.y + edit_button_rect.h;

  if (ldk_ui_begin_popup(ui, MENU_ID_PROJECT))
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

    ldk_ui_set_next_disabled(ui, !editor->project.loaded);
    if (ldk_ui_button_flat(ui, "Project Settings"))
    {
      ldki_editor_window_show(LDK_EDITOR_WINDOW_PROJECT);
      ldk_ui_close_current_popup(ui);
    }

    LDKUIRect content_rect = ldk_ui_measure_from(ui, mark);
    ldk_ui_end_popup(ui);
  }

  popup_pos.x = scene_button_rect.x;
  popup_pos.y = scene_button_rect.y + scene_button_rect.h;

  if (ldk_ui_begin_popup(ui, MENU_ID_SCENE))
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
    ldk_ui_end_popup(ui);
  }

  popup_pos.x = window_button_rect.x;
  popup_pos.y = window_button_rect.y + window_button_rect.h;

  if (ldk_ui_begin_popup(ui, MENU_ID_WINDOW))
  {
    LDKUIMark mark = ldk_ui_mark(ui);

    u32 window_count = ldki_editor_window_count();

    for (u32 i = 0; i < window_count; ++i)
    {
      const LDKEditorWindow *window = ldki_editor_window_at(i);

      if (window == NULL)
      {
        continue;
      }

      if (window->id == LDK_EDITOR_WINDOW_SETTINGS ||
          window->id == LDK_EDITOR_WINDOW_PROJECT)
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
    ldk_ui_end_popup(ui);
  }

  popup_pos.x = theme_button_rect.x;
  popup_pos.y = theme_button_rect.y + theme_button_rect.h;

  if (ldk_ui_begin_popup(ui, MENU_ID_THEME))
  {
    LDKUIMark mark = ldk_ui_mark(ui);
    ldki_editor_theme_menu_show(editor);
    LDKUIRect content_rect = ldk_ui_measure_from(ui, mark);
    ldk_ui_end_popup(ui);
  }

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
  case LDK_EDITOR_PROJECT_ACTION_CLEAN:
    return "Cleaning game build";
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
  case LDK_EDITOR_PROJECT_BUILD_STAGE_GAME_CLEAN:
    return "CMake clean";
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

  const float status_bar_height = LDK_EDITOR_STATUS_BAR_HEIGHT(ui);
  LDKUIRect rect = {ui->viewport.x,
      ui->viewport.y + ui->viewport.h - status_bar_height, ui->viewport.w,
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

void ldki_editor_profile_set(LDKEditorContext *editor, bool profile)
{
  if (profile == editor->profile)
  {
    return;
  }
  if (!s_editor_ini_bool_write(&editor->editor_config_path, "profile", profile))
  {
    ldki_editor_log_warning(editor, "Could not save the Profile preference.");
    return;
  }
  editor->profile = profile;
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

static void s_editor_audio_volume_popup(LDKEditorContext *editor,
    LDKUIId popup_id)
{
  LDKUIContext *ui = &editor->ui;
  LDKAudio *audio = (LDKAudio *)ldk_module_get(LDK_MODULE_AUDIO);
  float volume;
  float next_volume;
  char volume_text[8];

  if (!ldk_ui_begin_popup(ui, popup_id))
  {
    return;
  }

  if (!audio || !audio->is_initialized)
  {
    ldk_ui_close_current_popup(ui);
    ldk_ui_end_popup(ui);
    return;
  }

  volume = ldk_audio_master_volume_get(audio) * 100.0f;
  ldk_ui_begin_horizontal(ui);
  ldk_ui_set_next_width(ui, ldk_ui_px(160.0f));
  next_volume = ldk_ui_slider(ui, volume, 0.0f, 100.0f);
  snprintf(volume_text, sizeof(volume_text), "%.0f%%", next_volume);
  ldk_ui_set_next_weight(ui, 0.0f);
  ldk_ui_label(ui, volume_text);
  ldk_ui_end_horizontal(ui);

  if (next_volume != volume &&
      !ldk_audio_master_volume_set(audio, next_volume / 100.0f))
  {
    ldki_editor_log_warning(editor, "Failed to set master audio volume.");
  }

  ldk_ui_end_popup(ui);
}

static void s_editor_tool_bar(LDKEditorContext *editor)
{
  LDKUIContext *ui = &editor->ui;
  const LDKUIId audio_popup_id = 0x41554456u;
  LDKUIRect toolbar_rect = {ui->viewport.x,
      ui->viewport.y + LDK_EDITOR_MENU_BAR_HEIGHT(ui), ui->viewport.w,
      LDK_EDITOR_TOOL_BAR_HEIGHT(ui)};
  ldk_ui_begin_window_fixed(ui, "EDITOR COMMANDS", toolbar_rect, 0);
  ldk_ui_begin_horizontal(&editor->ui);

  {
    LDKAudio *audio = (LDKAudio *)ldk_module_get(LDK_MODULE_AUDIO);
    LDKUIIcon icon = {0};
    LDKUIRect button_rect;
    bool clicked;

    icon.color = ui->theme.colors[LDK_UI_COLOR_CONTROL_TEXT];
    icon.size = ldk_sizef(
        LDK_UI_DEFAULT_CONTROL_HEIGHT, LDK_UI_DEFAULT_CONTROL_HEIGHT);
    icon.texture =
        ldk_renderer_texture_ui_handle(editor->renderer, editor->ui_atlas);
    icon.uv = ldk_editor_icon_rects[
        ldk_audio_master_volume_get(audio) <= 0.0f
            ? LDK_EDITOR_ICON_AUDIO_MUTE
            : LDK_EDITOR_ICON_AUDIO];

    ldk_ui_set_next_weight(ui, 0.0f);
    ldk_ui_set_next_disabled(ui, !audio || !audio->is_initialized);
    clicked = ldk_ui_icon_button(ui, icon, NULL);
    button_rect = ldk_ui_last_rect(ui);

    if (clicked)
    {
      if (ldk_ui_popup_is_open(ui, audio_popup_id))
      {
        ldk_ui_close_popup(ui, audio_popup_id);
      }
      else
      {
        LDKUIPoint position = {
            button_rect.x, button_rect.y + button_rect.h};
        ldk_ui_open_popup_at(ui, audio_popup_id, position);
      }
    }
  }

  {
    LDKUIIcon icon = {0};
    icon.texture =
        ldk_renderer_texture_ui_handle(editor->renderer, editor->ui_atlas);
    icon.size = ldk_sizef(
        LDK_UI_DEFAULT_CONTROL_HEIGHT, LDK_UI_DEFAULT_CONTROL_HEIGHT);
    icon.uv = ldk_editor_icon_rects[LDK_EDITOR_ICON_FULLSCREEN];

    ldk_ui_set_next_disabled(
        ui, editor->editor_state != LDK_EDITOR_STATE_STOPED);
    ldk_ui_set_next_weight(ui, 0.0f);
    if (s_editor_push_button(ui, NULL, icon, editor->exclusive_mode))
    {
      editor->exclusive_mode = !editor->exclusive_mode;
    }
  }

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

  s_editor_layout_combo_box(editor);

  ldk_ui_end_horizontal(&editor->ui);
  ldk_ui_end_window(ui);
  s_editor_audio_volume_popup(editor, audio_popup_id);
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

void ldki_editor_toolbar_show(LDKEditorContext *editor)
{
  s_editor_tool_bar(editor);
}

void ldki_editor_log_error(LDKEditorContext *editor, const char *msg)
{
  ldki_editor_console_append(editor, LDK_EDITOR_CONSOLE_ENTRY_ERROR, msg);
  ldk_log_error("%s\n", msg ? msg : "<null error>");
}

void ldki_editor_log_warning(LDKEditorContext *editor, const char *msg)
{
  ldki_editor_console_append(editor, LDK_EDITOR_CONSOLE_ENTRY_WARNING, msg);
  ldk_log_warning("%s\n", msg ? msg : "<null warning>");
}

void ldki_editor_log_info(LDKEditorContext *editor, const char *msg)
{
  ldki_editor_console_append(editor, LDK_EDITOR_CONSOLE_ENTRY_INFO, msg);
  ldk_log_info("%s\n", msg ? msg : "<null info>");
}
