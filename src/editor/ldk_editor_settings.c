#include "ldk_editor_settings.h"
#include "ldk_editor_internal.h"
#include "ldk_os.h"
#include <module/ldk_ui.h>
#include <stdx/stdx_ini.h>
#include <ctype.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct LDKEditorSettingsDraft
{
  bool initialized;
  bool dirty;
  bool restore_last_project;
  bool open_folders_single_click;
  char ui_scale[32];
  char font_size[32];
  char font_path[X_FS_PATH_MAX_LENGTH];
  rgba32 debug_color;
  rgba32 selection_color_1;
  rgba32 selection_color_2;
  float debug_line_width;
  float selection_line_width;
  float selection_pulse_seconds;
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
  draft->debug_color = editor->debug_color;
  draft->selection_color_1 = editor->selection_color_1;
  draft->selection_color_2 = editor->selection_color_2;
  draft->debug_line_width = editor->debug_line_width;
  draft->selection_line_width = editor->selection_line_width;
  draft->selection_pulse_seconds = editor->selection_pulse_seconds;
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
      font_size < 6 || font_size > 96 || draft->font_path[0] == 0 ||
      !isfinite(draft->debug_line_width) ||
      draft->debug_line_width < LDK_EDITOR_LINE_WIDTH_MIN ||
      draft->debug_line_width > LDK_EDITOR_LINE_WIDTH_MAX ||
      !isfinite(draft->selection_line_width) ||
      draft->selection_line_width < LDK_EDITOR_LINE_WIDTH_MIN ||
      draft->selection_line_width > LDK_EDITOR_LINE_WIDTH_MAX ||
      !isfinite(draft->selection_pulse_seconds) ||
      draft->selection_pulse_seconds < LDK_EDITOR_SELECTION_PULSE_SECONDS_MIN ||
      draft->selection_pulse_seconds > LDK_EDITOR_SELECTION_PULSE_SECONDS_MAX)
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
       x_ini_set(&ini, ".editor", "font", draft->font_path) &&
       x_ini_set_u32_hex(
           &ini, ".editor", "debug_color", draft->debug_color) &&
       x_ini_set_u32_hex(
           &ini, ".editor", "selection_color_1", draft->selection_color_1) &&
       x_ini_set_u32_hex(
           &ini, ".editor", "selection_color_2", draft->selection_color_2) &&
       x_ini_set_f32(&ini, ".editor", "debug_line_width",
           draft->debug_line_width) &&
       x_ini_set_f32(&ini, ".editor", "selection_line_width",
           draft->selection_line_width) &&
       x_ini_set_f32(&ini, ".editor", "selection_pulse_seconds",
           draft->selection_pulse_seconds);

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
  editor->debug_color = draft->debug_color;
  editor->selection_color_1 = draft->selection_color_1;
  editor->selection_color_2 = draft->selection_color_2;
  editor->debug_line_width = draft->debug_line_width;
  editor->selection_line_width = draft->selection_line_width;
  editor->selection_pulse_seconds = draft->selection_pulse_seconds;
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

static const float s_editor_settings_label_width_min = 80.0f;
static const float s_editor_settings_control_width_min = 100.0f;

static float s_editor_settings_label_width_clamp(
    LDKUIContext *ui, float width)
{
  float max_width = s_editor_settings_label_width_min;
  float spacing = LDK_UI_DEFAULT_SPACING;

  if (ui != NULL && ui->current_layout != NULL)
  {
    spacing = ui->current_layout->spacing;
    max_width = ui->current_layout->content_rect.w -
                s_editor_settings_control_width_min - spacing;
    if (max_width < s_editor_settings_label_width_min)
    {
      max_width = s_editor_settings_label_width_min;
    }
  }

  if (width < s_editor_settings_label_width_min)
  {
    return s_editor_settings_label_width_min;
  }
  if (width > max_width)
  {
    return max_width;
  }
  return width;
}

static void s_editor_settings_row_begin(
    LDKEditorContext *editor, const char *label)
{
  LDKUIContext *ui = &editor->ui;
  float spacing;
  float label_width;
  float max_width;

  if (!isfinite(editor->settings_label_width) ||
      editor->settings_label_width <= 0.0f)
  {
    editor->settings_label_width = LDK_EDITOR_SETTINGS_LABEL_WIDTH_DEFAULT;
  }

  editor->settings_label_width =
      s_editor_settings_label_width_clamp(ui, editor->settings_label_width);

  ldk_ui_set_next_height(ui, ldk_ui_px(LDK_UI_DEFAULT_CONTROL_HEIGHT));
  ldk_ui_begin_horizontal(ui);

  spacing = ui->current_layout != NULL ? ui->current_layout->spacing
                                       : LDK_UI_DEFAULT_SPACING;
  label_width = editor->settings_label_width - spacing;
  if (label_width < 1.0f)
  {
    label_width = 1.0f;
  }

  ldk_ui_set_next_width(ui, ldk_ui_px(label_width));
  ldk_ui_label(ui, label != NULL ? label : "");

  max_width = ui->current_layout != NULL
                  ? ui->current_layout->content_rect.w -
                        s_editor_settings_control_width_min - spacing
                  : editor->settings_label_width;
  if (max_width < s_editor_settings_label_width_min)
  {
    max_width = s_editor_settings_label_width_min;
  }

  editor->settings_label_width = ldk_ui_resize_handle_vertical(ui,
      editor->settings_label_width, s_editor_settings_label_width_min,
      max_width);
}

static u32 s_editor_settings_input_row(LDKEditorContext *editor,
    const char *label, char *buffer, u32 buffer_size)
{
  LDKUIContext *ui = &editor->ui;
  u32 result;

  s_editor_settings_row_begin(editor, label);
  result = ldk_ui_input_box(ui, buffer, buffer_size);
  ldk_ui_end_horizontal(ui);
  return result;
}

static u32 s_editor_settings_browse_row(LDKEditorContext *editor,
    const char *label, char *buffer, u32 buffer_size, const char *dialog_title,
    const char *filter)
{
  LDKUIContext *ui = &editor->ui;
  u32 result = 0;

  s_editor_settings_row_begin(editor, label);
  result = ldk_ui_input_box(ui, buffer, buffer_size);
  ldk_ui_set_next_weight(ui, 0.0f);
  if (ldk_ui_button(ui, "..."))
  {
    XFSPath path = {0};
    if (ldk_os_dialog_show_open_file(
            editor->window, dialog_title, filter, &path))
    {
      x_fs_path_normalize(&path);
      snprintf(buffer, buffer_size, "%s", path.buf);
      result |= LDK_UI_INPUT_BOX_CHANGED | LDK_UI_INPUT_BOX_COMMITTED;
    }
  }
  ldk_ui_end_horizontal(ui);
  return result;
}

static bool s_editor_settings_color_row(
    LDKEditorContext *editor, const char *label, rgba32 *color)
{
  if (editor == NULL || color == NULL)
  {
    return false;
  }

  LDKUIContext *ui = &editor->ui;
  rgba32 previous = *color;
  s_editor_settings_row_begin(editor, label);
  ldk_ui_set_next_weight(ui, 0.0f);
  if (ldk_ui_color_view(ui, *color))
  {
    ldk_os_dialog_color_picker_show(editor->window, color);
  }
  ldk_ui_spacer(ui);
  ldk_ui_end_horizontal(ui);
  return *color != previous;
}

static bool s_editor_settings_slider_row(LDKEditorContext *editor,
    const char *label, float *value, float minimum, float maximum)
{
  if (editor == NULL || value == NULL)
  {
    return false;
  }

  LDKUIContext *ui = &editor->ui;
  float previous = *value;
  s_editor_settings_row_begin(editor, label);
  *value = ldk_ui_slider(ui, *value, minimum, maximum);
  ldk_ui_end_horizontal(ui);
  return *value != previous;
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
    s_editor_settings_row_begin(editor, "Restore last project");
    ldk_ui_set_next_weight(ui, 0.0f);
    restore_last_project = ldk_ui_toggle(ui, draft.restore_last_project);
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

        result |= s_editor_settings_input_row(editor, "Name", association->name,
            (u32)sizeof(association->name));
        program_result = s_editor_settings_browse_row(editor, "Program",
            association->program, (u32)sizeof(association->program),
            "Choose Program", "Programs\0*.exe\0All Files\0*.*\0\0");
        if ((program_result & LDK_UI_INPUT_BOX_CHANGED) != 0)
        {
          s_editor_settings_association_name_suggest(association);
        }
        result |= program_result;
        result |= s_editor_settings_input_row(editor, "Arguments",
            association->arguments, (u32)sizeof(association->arguments));
        result |= s_editor_settings_input_row(editor, "Associations",
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
    bool color_changed;
    u32 result = 0;

    s_editor_settings_row_begin(editor, "Open folders with single click");
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
        editor, "UI scale", draft.ui_scale, (u32)sizeof(draft.ui_scale));
    result |= s_editor_settings_input_row(
        editor, "Font size", draft.font_size, (u32)sizeof(draft.font_size));
    result |= s_editor_settings_browse_row(editor, "Editor font",
        draft.font_path, (u32)sizeof(draft.font_path), "Choose Editor Font",
        "TrueType Font\0*.ttf\0All Files\0*.*\0\0");

    color_changed = s_editor_settings_color_row(
        editor, "Debug color", &draft.debug_color);
    color_changed = s_editor_settings_color_row(
                        editor, "Selection color 1",
                        &draft.selection_color_1) ||
                    color_changed;
    color_changed = s_editor_settings_color_row(
                        editor, "Selection color 2",
                        &draft.selection_color_2) ||
                    color_changed;
    if (color_changed)
    {
      draft.dirty = true;
    }

    if (s_editor_settings_slider_row(editor, "Debug line width",
            &draft.debug_line_width, LDK_EDITOR_LINE_WIDTH_MIN,
            LDK_EDITOR_LINE_WIDTH_MAX) ||
        s_editor_settings_slider_row(editor, "Selection line width",
            &draft.selection_line_width, LDK_EDITOR_LINE_WIDTH_MIN,
            LDK_EDITOR_LINE_WIDTH_MAX) ||
        s_editor_settings_slider_row(editor, "Selection pulse seconds",
            &draft.selection_pulse_seconds,
            LDK_EDITOR_SELECTION_PULSE_SECONDS_MIN,
            LDK_EDITOR_SELECTION_PULSE_SECONDS_MAX))
    {
      draft.dirty = true;
    }

    if ((result & LDK_UI_INPUT_BOX_CHANGED) != 0)
    {
      draft.dirty = true;
    }
  }

  ldk_ui_end_scrollview(ui);

  ldk_ui_set_next_weight(ui, 0.0f);
  ldk_ui_horizontal_line(ui);
  ldk_ui_set_next_weight(ui, 0.0f);
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

  ldk_ui_set_next_weight(ui, 0.0f);
  if (ldk_ui_button(ui, "Cancel"))
  {
    s_editor_settings_draft_reset(&draft, editor);
  }
  ldk_ui_end_horizontal(ui);
}

