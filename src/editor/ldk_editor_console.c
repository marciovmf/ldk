#include "ldk_editor_internal.h"

#include <float.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

//------------------------------------------------------------
// Console
//------------------------------------------------------------

static bool s_editor_console_entry_type_valid(char type)
{
  return type == (char)LDK_EDITOR_CONSOLE_ENTRY_INFO ||
         type == (char)LDK_EDITOR_CONSOLE_ENTRY_WARNING ||
         type == (char)LDK_EDITOR_CONSOLE_ENTRY_ERROR ||
         type == (char)LDK_EDITOR_CONSOLE_ENTRY_RAW;
}

static bool s_editor_console_entry_parse(char *cursor, char *end,
    LDKEditorConsoleEntryType *out_type, char **out_message,
    size_t *out_message_length, char **out_next)
{
  char *length_cursor;
  size_t message_length = 0;
  bool has_length = false;

  if (cursor == NULL || end == NULL || out_type == NULL ||
      out_message == NULL || out_message_length == NULL || out_next == NULL ||
      cursor >= end || (size_t)(end - cursor) < 6)
  {
    return false;
  }

  if (cursor[0] != '~' || cursor[1] != '~' ||
      !s_editor_console_entry_type_valid(cursor[2]) || cursor[3] != ':')
  {
    return false;
  }

  length_cursor = cursor + 4;

  while (length_cursor < end && *length_cursor >= '0' &&
         *length_cursor <= '9')
  {
    size_t digit = (size_t)(*length_cursor - '0');

    if (message_length > (SIZE_MAX - digit) / 10)
    {
      return false;
    }

    message_length = message_length * 10 + digit;
    has_length = true;
    length_cursor += 1;
  }

  if (!has_length || length_cursor >= end || *length_cursor != ':')
  {
    return false;
  }

  char *message = length_cursor + 1;
  if (message_length > (size_t)(end - message))
  {
    return false;
  }

  *out_type = (LDKEditorConsoleEntryType)cursor[2];
  *out_message = message;
  *out_message_length = message_length;
  *out_next = message + message_length;
  return true;
}

static bool s_editor_console_entry_starts_at(char *cursor, char *end)
{
  LDKEditorConsoleEntryType type;
  char *message;
  char *next;
  size_t message_length;

  if (cursor == NULL || end == NULL || cursor >= end || cursor[0] != '~' ||
      (size_t)(end - cursor) < 2 || cursor[1] != '~')
  {
    return false;
  }

  return s_editor_console_entry_parse(
      cursor, end, &type, &message, &message_length, &next);
}

static LDKUIIcon s_editor_console_entry_icon(
    LDKEditorContext *editor, LDKEditorConsoleEntryType type)
{
  LDKUIIcon icon = {0};

  if (editor == NULL || type == LDK_EDITOR_CONSOLE_ENTRY_RAW)
  {
    return icon;
  }

  icon.size =
      ldk_sizef(LDK_UI_DEFAULT_CONTROL_HEIGHT, LDK_UI_DEFAULT_CONTROL_HEIGHT);
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

static float s_editor_console_entry_height(
    LDKUIContext *ui, LDKUIIcon icon, const char *message)
{
  float text_width;
  float height = LDK_UI_DEFAULT_CONTROL_HEIGHT;

  if (ui == NULL || ui->font == NULL || ui->current_layout == NULL ||
      message == NULL)
  {
    return height;
  }

  text_width = ui->current_layout->content_rect.w;

  if (icon.texture != 0 && icon.uv.w > 0.0f && icon.uv.h > 0.0f &&
      icon.size.w > 0.0f && icon.size.h > 0.0f)
  {
    text_width -= icon.size.w + LDK_UI_DEFAULT_SPACING;
    if (icon.size.h > height)
    {
      height = icon.size.h;
    }
  }

  if (text_width < 1.0f)
  {
    text_width = 1.0f;
  }

  LDKSizef text_size =
      ldk_ttf_measure_text_cstr_wrapped(ui->font, message, text_width);
  if (text_size.h > height)
  {
    height = text_size.h;
  }

  return height;
}

static void s_editor_console_entry_draw(LDKEditorContext *editor,
    LDKEditorConsoleEntryType type, char *message, size_t message_length)
{
  LDKUIContext *ui;
  LDKUIIcon icon;
  float height;
  char saved;

  if (editor == NULL || message == NULL)
  {
    return;
  }

  if (type == LDK_EDITOR_CONSOLE_ENTRY_RAW && message_length > 0 &&
      message[message_length - 1] == '\r')
  {
    message_length -= 1;
  }

  saved = message[message_length];
  message[message_length] = 0;

  ui = &editor->ui;
  icon = s_editor_console_entry_icon(editor, type);
  height = s_editor_console_entry_height(ui, icon, message);

  ldk_ui_set_next_height(ui, ldk_ui_px(height));
  ldk_ui_icon_label(ui, icon, message);

  message[message_length] = saved;
}

static void s_editor_console_raw_draw(
    LDKEditorContext *editor, char **cursor_ptr, char *end)
{
  char *cursor;
  char *line_start;
  bool entry_follows = false;

  if (editor == NULL || cursor_ptr == NULL || *cursor_ptr == NULL ||
      end == NULL || *cursor_ptr >= end)
  {
    return;
  }

  cursor = *cursor_ptr;
  line_start = cursor;

  while (cursor < end && *cursor != '\n')
  {
    if (s_editor_console_entry_starts_at(cursor, end))
    {
      entry_follows = true;
      break;
    }

    cursor += 1;
  }

  if (cursor > line_start || (cursor < end && *cursor == '\n'))
  {
    s_editor_console_entry_draw(editor, LDK_EDITOR_CONSOLE_ENTRY_RAW,
        line_start, (size_t)(cursor - line_start));
  }

  if (cursor < end && *cursor == '\n')
  {
    cursor += 1;
  }
  else if (!entry_follows && cursor == line_start)
  {
    cursor += 1;
  }

  *cursor_ptr = cursor;
}

static void s_editor_console_entries_draw(LDKEditorContext *editor)
{
  char *cursor;
  char *end;

  if (editor == NULL || editor->console_sb == NULL)
  {
    return;
  }

  cursor = x_strbuilder_to_string(editor->console_sb);
  end = cursor + x_strbuilder_length(editor->console_sb);

  while (cursor < end)
  {
    LDKEditorConsoleEntryType type;
    char *message;
    char *next;
    size_t message_length;

    if (s_editor_console_entry_parse(
            cursor, end, &type, &message, &message_length, &next))
    {
      s_editor_console_entry_draw(editor, type, message, message_length);
      cursor = next;
      continue;
    }

    s_editor_console_raw_draw(editor, &cursor, end);
  }

}

static void s_editor_console_output_observe(LDKEditorContext *editor)
{
  size_t length;

  if (editor == NULL || editor->console_sb == NULL)
  {
    return;
  }

  length = x_strbuilder_length(editor->console_sb);

  if (length > editor->console_observed_length &&
      !editor->console_auto_scroll_disabled)
  {
    editor->console_scroll_pending = true;
  }

  editor->console_observed_length = length;
}

static void s_editor_console_toolbar(
    LDKEditorContext *editor, LDKUIPoint *scroll)
{
  LDKUIContext *ui;
  LDKUIIcon icon = {0};
  bool auto_scroll;

  if (editor == NULL || scroll == NULL)
  {
    return;
  }

  ui = &editor->ui;
  icon.size = ldk_sizef(LDK_UI_DEFAULT_CONTROL_HEIGHT,
      LDK_UI_DEFAULT_CONTROL_HEIGHT);
  icon.texture =
      ldk_renderer_texture_ui_handle(editor->renderer, editor->ui_atlas);
  icon.color = ui->theme.colors[LDK_UI_COLOR_CONTROL_TEXT];

  ldk_ui_set_next_height(ui, ldk_ui_px(LDK_UI_DEFAULT_CONTROL_HEIGHT));
  ldk_ui_begin_horizontal(ui);

  icon.uv = ldk_editor_icon_rects[LDK_EDITOR_ICON_DELETE];
  ldk_ui_set_next_weight(ui, 0.0f);
  if (ldk_ui_icon_button(ui, icon, "Clear"))
  {
    x_strbuilder_clear(editor->console_sb);
    *scroll = (LDKUIPoint){0};
    editor->console_scroll_pending = false;
    editor->console_observed_length = 0;
  }

  auto_scroll = !editor->console_auto_scroll_disabled;
  icon.uv = ldk_editor_icon_rects[auto_scroll
                                      ? LDK_EDITOR_ICON_CHECKBOX_CHECKED
                                      : LDK_EDITOR_ICON_CHECKBOX_UNCHECKED];
  ldk_ui_set_next_weight(ui, 0.0f);
  if (ldk_ui_icon_button(ui, icon, "Auto-scroll"))
  {
    editor->console_auto_scroll_disabled = auto_scroll;
    auto_scroll = !auto_scroll;

    if (!auto_scroll)
    {
      editor->console_scroll_pending = false;
    }
  }

  ldk_ui_spacer(ui);
  ldk_ui_end_horizontal(ui);
  ldk_ui_horizontal_line(ui);
}

void ldki_editor_console_append(LDKEditorContext *editor,
    LDKEditorConsoleEntryType type, const char *message)
{
  size_t message_length;
  size_t previous_length;

  if (editor == NULL || editor->console_sb == NULL || message == NULL ||
      !s_editor_console_entry_type_valid((char)type))
  {
    return;
  }

  message_length = strlen(message);
  previous_length = x_strbuilder_length(editor->console_sb);

  x_strbuilder_append_format(editor->console_sb, "~~%c:%zu:%s", (char)type,
      message_length, message);

  if (x_strbuilder_length(editor->console_sb) > previous_length &&
      !editor->console_auto_scroll_disabled)
  {
    editor->console_scroll_pending = true;
  }
}

static void s_editor_console(LDKEditorContext *editor)
{
  LDK_ASSERT(editor);
  LDK_ASSERT(editor->console_sb);

  static XSmallstr input = {0};
  static LDKUIPoint scroll = {0};
  static LDKUIRect window_rect = {150, 90, 200, 180};
  LDKUIContext *ui = &editor->ui;
  bool owns_window = ui->current_window == NULL;
  bool scroll_pending;

  if (owns_window)
  {
    window_rect = ldk_ui_begin_window_fixed(
        ui, "CONSOLE", window_rect, LDK_UI_WINDOW_TOOL);
  }

  s_editor_console_output_observe(editor);
  s_editor_console_toolbar(editor, &scroll);
  scroll_pending = editor->console_scroll_pending;

  if (scroll_pending)
  {
    scroll.y = FLT_MAX;
  }

  scroll = ldk_ui_begin_scrollview(
      ui, scroll, LDK_UI_SCROLL_VERTICAL | LDK_UI_SCROLL_IF_NEEDED);
  s_editor_console_entries_draw(editor);
  ldk_ui_end_scrollview(ui);

  if (scroll_pending)
  {
    scroll.y = FLT_MAX;
    editor->console_scroll_pending = false;
  }

  ldk_ui_set_next_weight(ui, 0.0f);
  if (ldk_ui_input_box(ui, input.buf, X_SMALLSTR_MAX_LENGTH) &
      LDK_UI_INPUT_BOX_COMMITTED)
  {
    ldki_editor_console_append(editor, LDK_EDITOR_CONSOLE_ENTRY_RAW, input.buf);
    ldk_editor_command_run(editor, input.buf);
    x_smallstr_clear(&input);
  }

  if (owns_window)
  {
    ldk_ui_end_window(ui);
  }
}

void ldk_editor_console_show(LDKEditor *editor)
{
  s_editor_console(editor);
}
