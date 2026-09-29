#include "ldk_editor_project_window.h"
#include "ldk_editor_internal.h"
#include "ldk_editor_project_options.h"
#include "ldk_os.h"
#include <module/ldk_ui.h>
#include <ctype.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static bool s_editor_project_integer_parse(const char *text, i32 *out_value)
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

typedef struct LDKEditorProjectSettingsDraft
{
  bool initialized;
  bool dirty;
  XFSPath project_file_path;
  char game_name[X_SMALLSTR_MAX_LENGTH];
  char icon_path[X_FS_PATH_MAX_LENGTH];
  char resolution_width[32];
  char resolution_height[32];
  char build_config[X_SMALLSTR_MAX_LENGTH];
  char cmake_generator[X_SMALLSTR_MAX_LENGTH];
  char cmake_arch[X_SMALLSTR_MAX_LENGTH];
  u32 build_type_index;
  u32 generator_index;
} LDKEditorProjectSettingsDraft;

static void s_editor_project_settings_draft_reset(
    LDKEditorProjectSettingsDraft *draft, const LDKEditorContext *editor)
{
  if (draft == NULL || editor == NULL || !editor->project.loaded)
  {
    return;
  }

  memset(draft, 0, sizeof(*draft));
  draft->initialized = true;
  draft->project_file_path = editor->project.project_file_path;
  snprintf(draft->game_name, sizeof(draft->game_name), "%s",
      editor->project.game_name.buf);
  snprintf(draft->icon_path, sizeof(draft->icon_path), "%s",
      editor->project.icon_path.buf);
  snprintf(draft->resolution_width, sizeof(draft->resolution_width), "%d",
      editor->project.project_resolution_width);
  snprintf(draft->resolution_height, sizeof(draft->resolution_height), "%d",
      editor->project.project_resolution_height);
  snprintf(draft->build_config, sizeof(draft->build_config), "%s",
      editor->project.build_config.buf);
  snprintf(draft->cmake_generator, sizeof(draft->cmake_generator), "%s",
      editor->project.cmake_generator.buf);
  snprintf(draft->cmake_arch, sizeof(draft->cmake_arch), "%s",
      editor->project.cmake_arch.buf);
  draft->build_type_index =
      ldki_editor_project_build_type_index_get(draft->build_config);
  draft->generator_index =
      ldki_editor_project_generator_index_get(draft->cmake_generator);
}

static const float s_editor_project_label_width_min = 80.0f;
static const float s_editor_project_control_width_min = 100.0f;

static float s_editor_project_label_width_clamp(
    LDKUIContext *ui, float width)
{
  float max_width = s_editor_project_label_width_min;
  float spacing = LDK_UI_DEFAULT_SPACING;

  if (ui != NULL && ui->current_layout != NULL)
  {
    spacing = ui->current_layout->spacing;
    max_width = ui->current_layout->content_rect.w -
                s_editor_project_control_width_min - spacing;
    if (max_width < s_editor_project_label_width_min)
    {
      max_width = s_editor_project_label_width_min;
    }
  }

  if (width < s_editor_project_label_width_min)
  {
    return s_editor_project_label_width_min;
  }
  if (width > max_width)
  {
    return max_width;
  }
  return width;
}

static void s_editor_project_row_begin(
    LDKEditorContext *editor, const char *label)
{
  LDKUIContext *ui = &editor->ui;
  float spacing;
  float label_width;
  float max_width;

  if (!isfinite(editor->project_label_width) ||
      editor->project_label_width <= 0.0f)
  {
    editor->project_label_width = LDK_EDITOR_PROJECT_LABEL_WIDTH_DEFAULT;
  }

  editor->project_label_width =
      s_editor_project_label_width_clamp(ui, editor->project_label_width);

  ldk_ui_set_next_height(ui, ldk_ui_px(LDK_UI_DEFAULT_CONTROL_HEIGHT));
  ldk_ui_begin_horizontal(ui);

  spacing = ui->current_layout != NULL ? ui->current_layout->spacing
                                       : LDK_UI_DEFAULT_SPACING;
  label_width = editor->project_label_width - spacing;
  if (label_width < 1.0f)
  {
    label_width = 1.0f;
  }

  ldk_ui_set_next_width(ui, ldk_ui_px(label_width));
  ldk_ui_label(ui, label != NULL ? label : "");

  max_width = ui->current_layout != NULL
                  ? ui->current_layout->content_rect.w -
                        s_editor_project_control_width_min - spacing
                  : editor->project_label_width;
  if (max_width < s_editor_project_label_width_min)
  {
    max_width = s_editor_project_label_width_min;
  }

  editor->project_label_width = ldk_ui_resize_handle_vertical(ui,
      editor->project_label_width, s_editor_project_label_width_min,
      max_width);
}

static u32 s_editor_project_input_row(LDKEditorContext *editor,
    const char *label, char *buffer, u32 buffer_size)
{
  LDKUIContext *ui = &editor->ui;
  u32 result;

  s_editor_project_row_begin(editor, label);
  result = ldk_ui_input_box(ui, buffer, buffer_size);
  ldk_ui_end_horizontal(ui);
  return result;
}

static void s_editor_project_read_only_row(
    LDKEditorContext *editor, const char *label, const char *value)
{
  LDKUIContext *ui = &editor->ui;
  char buffer[X_FS_PATH_MAX_LENGTH];

  snprintf(buffer, sizeof(buffer), "%s", value != NULL ? value : "");
  s_editor_project_row_begin(editor, label);
  ldk_ui_set_next_disabled(ui, true);
  ldk_ui_input_box(ui, buffer, (u32)sizeof(buffer));
  ldk_ui_end_horizontal(ui);
}

static bool s_editor_project_relative_path_is_inside(const XFSPath *path)
{
  if (path == NULL || path->length == 0)
  {
    return false;
  }

  return strcmp(path->buf, "..") != 0 && strncmp(path->buf, "../", 3) != 0 &&
         strncmp(path->buf, "..\\", 3) != 0;
}

static u32 s_editor_project_icon_row(LDKEditorContext *editor,
    LDKUIContext *ui, char *buffer, u32 buffer_size)
{
  u32 result;

  s_editor_project_row_begin(editor, "Icon");
  result = ldk_ui_input_box(ui, buffer, buffer_size);
  ldk_ui_set_next_weight(ui, 0.0f);
  if (ldk_ui_button(ui, "..."))
  {
    char selected[X_FS_PATH_MAX_LENGTH] = {0};
    if (ldk_os_dialog_show_open_file(editor->window, "Choose Game Icon",
            "Icon\0*.ico\0All Files\0*.*\0\0", selected,
            sizeof(selected)))
    {
      XFSPath selected_path = {0};
      XFSPath relative_path = {0};

      x_fs_path_set(&selected_path, selected);
      x_fs_path_normalize(&selected_path);
      if (x_fs_path_relative_to(&editor->project.run_root_path,
              &selected_path, &relative_path) != 0 &&
          s_editor_project_relative_path_is_inside(&relative_path))
      {
        snprintf(buffer, buffer_size, "%s", relative_path.buf);
      }
      else
      {
        snprintf(buffer, buffer_size, "%s", selected_path.buf);
      }
      result |= LDK_UI_INPUT_BOX_CHANGED | LDK_UI_INPUT_BOX_COMMITTED;
    }
  }
  ldk_ui_end_horizontal(ui);
  return result;
}

void ldki_editor_project_window_show(LDKEditor *opaque_editor, void *data)
{
  static LDKUIPoint scroll = {0};
  static LDKEditorProjectSettingsDraft draft = {0};
  static bool project_expanded = true;
  static bool identity_expanded = true;
  static bool runtime_expanded = true;
  static bool build_expanded = true;
  LDKEditorContext *editor = (LDKEditorContext *)opaque_editor;
  LDKUIContext *ui;
  XFSPath game_dll_path = {0};
  i32 resolution_width;
  i32 resolution_height;
  bool can_modify;
  (void)data;

  if (editor == NULL)
  {
    return;
  }

  ui = &editor->ui;
  if (!editor->project.loaded)
  {
    draft = (LDKEditorProjectSettingsDraft){0};
    ldk_ui_label(ui, "No project loaded.");
    return;
  }

  if (!draft.initialized ||
      strcmp(draft.project_file_path.buf,
          editor->project.project_file_path.buf) != 0)
  {
    s_editor_project_settings_draft_reset(&draft, editor);
  }

  can_modify = editor->editor_state == LDK_EDITOR_STATE_STOPED &&
               !editor->project_build.active &&
               editor->pending_project_action.type ==
                   LDK_EDITOR_PROJECT_ACTION_NONE;

  scroll = ldk_ui_begin_scrollview(
      ui, scroll, LDK_UI_SCROLL_VERTICAL | LDK_UI_SCROLL_IF_NEEDED);

  project_expanded = ldk_ui_tree_node(ui, "Project", project_expanded, 0, 0);
  if (project_expanded)
  {
    s_editor_project_read_only_row(
        editor, "Project name", editor->project.name.buf);
    s_editor_project_read_only_row(
        editor, "Project file", editor->project.project_file_path.buf);
    s_editor_project_read_only_row(
        editor, "Project root", editor->project.project_root_path.buf);
    s_editor_project_read_only_row(
        editor, "RunTree", editor->project.run_root_path.buf);
  }

  ldk_ui_spacer(ui);
  identity_expanded =
      ldk_ui_tree_node(ui, "Identity", identity_expanded, 0, 0);
  if (identity_expanded)
  {
    u32 result = 0;
    ldk_ui_begin_disabled(ui, !can_modify);
    result |= s_editor_project_input_row(editor, "Game name", draft.game_name,
        (u32)sizeof(draft.game_name));
    result |= s_editor_project_icon_row(
        editor, ui, draft.icon_path, (u32)sizeof(draft.icon_path));
    ldk_ui_end_disabled(ui);
    if ((result & LDK_UI_INPUT_BOX_CHANGED) != 0)
    {
      draft.dirty = true;
    }
  }

  ldk_ui_spacer(ui);
  runtime_expanded = ldk_ui_tree_node(
      ui, "Runtime defaults", runtime_expanded, 0, 0);
  if (runtime_expanded)
  {
    u32 result = 0;
    ldk_ui_begin_disabled(ui, !can_modify);
    result |= s_editor_project_input_row(editor, "Target width",
        draft.resolution_width, (u32)sizeof(draft.resolution_width));
    result |= s_editor_project_input_row(editor, "Target height",
        draft.resolution_height, (u32)sizeof(draft.resolution_height));
    ldk_ui_end_disabled(ui);
    if ((result & LDK_UI_INPUT_BOX_CHANGED) != 0)
    {
      draft.dirty = true;
    }
  }

  ldk_ui_spacer(ui);
  build_expanded = ldk_ui_tree_node(ui, "Build", build_expanded, 0, 0);
  if (build_expanded)
  {
    u32 new_build_type;
    u32 new_generator;

    ldk_ui_begin_disabled(ui, !can_modify);

    s_editor_project_row_begin(editor, "Build type");
    new_build_type = ldk_ui_combo_box(ui, ldki_editor_project_build_types_get(),
        ldki_editor_project_build_type_count(),
        draft.build_type_index);
    ldk_ui_end_horizontal(ui);
    if (new_build_type != draft.build_type_index &&
        new_build_type < ldki_editor_project_build_type_count())
    {
      draft.build_type_index = new_build_type;
      snprintf(draft.build_config, sizeof(draft.build_config), "%s",
          ldki_editor_project_build_types_get()[new_build_type]);
      draft.dirty = true;
    }

    s_editor_project_row_begin(editor, "CMake generator");
    new_generator = ldk_ui_combo_box(ui, ldki_editor_project_generator_labels_get(),
        ldki_editor_project_generator_count(),
        draft.generator_index);
    ldk_ui_end_horizontal(ui);
    if (new_generator != draft.generator_index &&
        new_generator < ldki_editor_project_generator_count())
    {
      const LDKEditorProjectGenerator *generator =
          ldki_editor_project_generator_get(new_generator);
      draft.generator_index = new_generator;
      snprintf(draft.cmake_generator, sizeof(draft.cmake_generator), "%s",
          generator->cmake_generator);
      snprintf(draft.cmake_arch, sizeof(draft.cmake_arch), "%s",
          generator->uses_platform ? ldki_editor_cmake_native_arch_get() : "");
      draft.dirty = true;
    }

    ldk_ui_end_disabled(ui);

    s_editor_project_read_only_row(
        editor, "Build directory", editor->project.cmake_root_path.buf);
    if (ldk_project_game_module_output_path_get(
            &editor->project, draft.build_config, &game_dll_path))
    {
      s_editor_project_read_only_row(editor, "Game DLL", game_dll_path.buf);
    }

    s_editor_project_row_begin(editor, "");
    ldk_ui_set_next_disabled(ui, !can_modify);
    if (ldk_ui_button(ui, "Clean Build") &&
        ldk_os_dialog_show_yes_no(editor->window, "Clean Build",
            "Run the CMake clean target for the current build configuration?"))
    {
      ldki_editor_project_clean_build_request(editor);
    }
    ldk_ui_spacer(ui);
    ldk_ui_end_horizontal(ui);
  }

  ldk_ui_end_scrollview(ui);

  ldk_ui_set_next_weight(ui, 0.0f);
  ldk_ui_horizontal_line(ui);
  ldk_ui_set_next_weight(ui, 0.0f);
  ldk_ui_begin_horizontal(ui);
  ldk_ui_spacer(ui);

  ldk_ui_set_next_disabled(ui, !can_modify || !draft.dirty);
  ldk_ui_set_next_weight(ui, 0.0f);
  if (ldk_ui_button(ui, "Save"))
  {
    if (!s_editor_project_integer_parse(
            draft.resolution_width, &resolution_width) ||
        !s_editor_project_integer_parse(
            draft.resolution_height, &resolution_height) ||
        resolution_width <= 0 || resolution_height <= 0 ||
        draft.game_name[0] == 0 || draft.build_config[0] == 0 ||
        draft.cmake_generator[0] == 0)
    {
      ldki_editor_log_error(editor, "Invalid project settings.");
    }
    else
    {
      LDKProject updated = editor->project;
      x_smallstr_from_cstr(&updated.game_name, draft.game_name);
      x_smallstr_from_cstr(&updated.build_config, draft.build_config);
      x_smallstr_from_cstr(&updated.cmake_generator, draft.cmake_generator);
      x_smallstr_from_cstr(&updated.cmake_arch, draft.cmake_arch);
      x_fs_path_set(&updated.icon_path, draft.icon_path);
      updated.project_resolution_width = resolution_width;
      updated.project_resolution_height = resolution_height;

      if (!ldki_editor_project_settings_apply(editor, &updated))
      {
        ldki_editor_log_error(editor, "Failed to save project settings.");
      }
      else
      {
        s_editor_project_settings_draft_reset(&draft, editor);
        ldki_editor_log_info(editor, "Project settings saved.");
      }
    }
  }

  ldk_ui_set_next_weight(ui, 0.0f);
  if (ldk_ui_button(ui, "Cancel"))
  {
    s_editor_project_settings_draft_reset(&draft, editor);
  }
  ldk_ui_end_horizontal(ui);
}

