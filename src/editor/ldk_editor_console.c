#include "ldk_editor_internal.h"

#include <float.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>


//------------------------------------------------------------
// Console
//------------------------------------------------------------

typedef struct LDKEditorConsoleLastMessageCache
{
  LDKEditorContext *editor;
  size_t observed_length;

  LDKEditorConsoleEntryType type;
  size_t message_length;
  char message[X_SMALLSTR_MAX_LENGTH];

  size_t raw_line_length;
  char raw_line[X_SMALLSTR_MAX_LENGTH];
  bool raw_active;
} LDKEditorConsoleLastMessageCache;

static LDKEditorConsoleLastMessageCache s_last_message_cache = {0};
static XStrBuilder *s_console_view_sb = NULL;

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

static void s_editor_console_last_message_cache_reset(
    LDKEditorConsoleLastMessageCache *cache, LDKEditorContext *editor)
{
  if (cache == NULL)
  {
    return;
  }

  memset(cache, 0, sizeof(*cache));
  cache->editor = editor;
  cache->type = LDK_EDITOR_CONSOLE_ENTRY_RAW;
}

static void s_editor_console_last_message_set(
    LDKEditorConsoleLastMessageCache *cache, LDKEditorConsoleEntryType type,
    const char *message, size_t message_length)
{
  size_t copy_length;

  if (cache == NULL || message == NULL)
  {
    return;
  }

  while (message_length > 0 &&
         (message[message_length - 1] == '\n' ||
          message[message_length - 1] == '\r'))
  {
    message_length -= 1;
  }

  if (message_length == 0)
  {
    return;
  }

  /* The status bar is a single line. If a structured message contains
   * newlines, show its last non-empty line. */
  size_t line_start = message_length;
  while (line_start > 0 && message[line_start - 1] != '\n' &&
         message[line_start - 1] != '\r')
  {
    line_start -= 1;
  }

  message += line_start;
  message_length -= line_start;
  copy_length = message_length;

  if (copy_length >= sizeof(cache->message))
  {
    copy_length = sizeof(cache->message) - 1;
  }

  memcpy(cache->message, message, copy_length);
  cache->message[copy_length] = 0;
  cache->message_length = copy_length;
  cache->type = type;
}

static void s_editor_console_last_raw_line_publish(
    LDKEditorConsoleLastMessageCache *cache)
{
  if (cache == NULL || cache->raw_line_length == 0)
  {
    return;
  }

  s_editor_console_last_message_set(cache, LDK_EDITOR_CONSOLE_ENTRY_RAW,
      cache->raw_line, cache->raw_line_length);
}

static void s_editor_console_last_raw_char_append(
    LDKEditorConsoleLastMessageCache *cache, char c)
{
  if (cache == NULL)
  {
    return;
  }

  if (c == '\n' || c == '\r')
  {
    /* Keep the completed line in the status bar. Do not blank it simply
     * because process output ended with a newline. */
    s_editor_console_last_raw_line_publish(cache);
    cache->raw_line_length = 0;
    cache->raw_line[0] = 0;
    return;
  }

  if (cache->raw_line_length + 1 < sizeof(cache->raw_line))
  {
    cache->raw_line[cache->raw_line_length++] = c;
    cache->raw_line[cache->raw_line_length] = 0;
  }

  /* Publish partial process output too, so the status bar updates while a
   * tool is still writing the current line. */
  s_editor_console_last_raw_line_publish(cache);
}

const char *ldki_editor_console_last_message_get(LDKEditorContext *editor,
    LDKEditorConsoleEntryType *out_type)
{
  LDKEditorConsoleLastMessageCache *cache = &s_last_message_cache;
  size_t length;
  char *buffer;
  char *cursor;
  char *end;

  if (out_type != NULL)
  {
    *out_type = LDK_EDITOR_CONSOLE_ENTRY_RAW;
  }

  if (editor == NULL || editor->console_sb == NULL)
  {
    return NULL;
  }

  if (cache->editor != editor)
  {
    s_editor_console_last_message_cache_reset(cache, editor);
  }

  length = x_strbuilder_length(editor->console_sb);

  /* The console may have been cleared. */
  if (length < cache->observed_length)
  {
    s_editor_console_last_message_cache_reset(cache, editor);
  }

  if (length != cache->observed_length)
  {
    buffer = x_strbuilder_to_string(editor->console_sb);
    cursor = buffer + cache->observed_length;
    end = buffer + length;

    while (cursor < end)
    {
      LDKEditorConsoleEntryType type;
      char *message;
      char *next;
      size_t message_length;

      if (s_editor_console_entry_parse(
              cursor, end, &type, &message, &message_length, &next))
      {
        cache->raw_active = false;
        cache->raw_line_length = 0;
        cache->raw_line[0] = 0;

        s_editor_console_last_message_set(
            cache, type, message, message_length);

        cursor = next;
        continue;
      }

      /* Build/process output is appended directly to console_sb. */
      if (!cache->raw_active)
      {
        cache->raw_active = true;
        cache->raw_line_length = 0;
        cache->raw_line[0] = 0;
      }

      s_editor_console_last_raw_char_append(cache, *cursor);
      cursor += 1;
    }

    cache->observed_length = length;
  }

  if (cache->message[0] == 0)
  {
    return NULL;
  }

  if (out_type != NULL)
  {
    *out_type = cache->type;
  }

  return cache->message;
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

/*
 * console_sb remains the append-only source stream. Structured editor
 * messages are encoded as ~~<type>:<length>:<message>; build output is raw.
 *
 * The view is deliberately different: it is one ordinary text document.
 * Structured metadata never reaches the selectable text widget. Icons are
 * rendered afterwards in a fixed gutter and are located from their offsets in
 * this flattened document.
 */
static bool s_editor_console_view_build(LDKEditorContext *editor)
{
  char *cursor;
  char *end;

  if (editor == NULL || editor->console_sb == NULL)
  {
    return false;
  }

  if (s_console_view_sb == NULL)
  {
    s_console_view_sb = x_strbuilder_create();
    if (s_console_view_sb == NULL)
    {
      return false;
    }
  }

  x_strbuilder_clear(s_console_view_sb);

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
      size_t view_length = x_strbuilder_length(s_console_view_sb);
      (void)type;
      size_t entry_offset;
      char *view_text = x_strbuilder_to_string(s_console_view_sb);

      if (view_length > 0 && view_text[view_length - 1] != '\n')
      {
        x_strbuilder_append_cstr(s_console_view_sb, "\n");
      }

      entry_offset = x_strbuilder_length(s_console_view_sb);

      if (message_length > 0)
      {
        x_strbuilder_append_substring(
            s_console_view_sb, message, message_length);
      }

      view_length = x_strbuilder_length(s_console_view_sb);
      view_text = x_strbuilder_to_string(s_console_view_sb);

      if (view_length == entry_offset || view_text[view_length - 1] != '\n')
      {
        x_strbuilder_append_cstr(s_console_view_sb, "\n");
      }

      cursor = next;
      continue;
    }

    x_strbuilder_append_substring(s_console_view_sb, cursor, 1);
    cursor += 1;
  }

  return true;
}

static void s_editor_console_view_icons_draw(LDKEditorContext *editor,
    char *view_text, LDKUIRect gutter_rect, LDKUIRect text_rect)
{
  LDKUIContext *ui;
  char *cursor;
  char *end;
  size_t display_offset = 0;
  size_t previous_icon_offset = 0;
  float icon_y = 0.0f;
  float line_height;
  u32 icon_index = 0;

  if (editor == NULL || editor->console_sb == NULL || view_text == NULL ||
      text_rect.w <= 0.0f)
  {
    return;
  }

  ui = &editor->ui;
  line_height = ldk_ttf_get_line_height(ui->font);
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
      size_t marker_offset;
      (void)message;

      if (display_offset > 0 && view_text[display_offset - 1] != '\n')
      {
        display_offset += 1;
      }

      marker_offset = display_offset;

      if (type != LDK_EDITOR_CONSOLE_ENTRY_RAW)
      {
        if (marker_offset > previous_icon_offset)
        {
          char saved = view_text[marker_offset];
          LDKSizef segment_size;

          view_text[marker_offset] = 0;
          segment_size = ldk_ttf_measure_text_cstr_wrapped(ui->font,
              view_text + previous_icon_offset, text_rect.w);
          view_text[marker_offset] = saved;
          icon_y += segment_size.h;
        }

        LDKUIIcon icon = s_editor_console_entry_icon(editor, type);
        LDKUIRect icon_rect = {
            gutter_rect.x + (gutter_rect.w - icon.size.w) * 0.5f,
            text_rect.y + icon_y + (line_height - icon.size.h) * 0.5f,
            icon.size.w,
            icon.size.h};

        ldk_ui_widget_icon_label(ui, 0xC0500000u + icon_index, icon, "",
            icon_rect);
        icon_index += 1;
        previous_icon_offset = marker_offset;
      }

      display_offset += message_length;

      if (display_offset == marker_offset ||
          view_text[display_offset - 1] != '\n')
      {
        display_offset += 1;
      }

      cursor = next;
      continue;
    }

    display_offset += 1;
    cursor += 1;
  }
}

static void s_editor_console_document_draw(LDKEditorContext *editor)
{
  LDKUIContext *ui;
  char *text;
  float gutter_width;
  float text_width;
  float height;
  LDKSizef text_size;
  LDKUIRect gutter_rect;
  LDKUIRect text_rect;

  if (editor == NULL || !s_editor_console_view_build(editor))
  {
    return;
  }

  ui = &editor->ui;
  if (ui->current_layout == NULL || ui->font == NULL)
  {
    return;
  }

  text = x_strbuilder_to_string(s_console_view_sb);
  gutter_width = LDK_UI_DEFAULT_CONTROL_HEIGHT;
  text_width = ui->current_layout->content_rect.w - gutter_width -
               LDK_UI_DEFAULT_SPACING;

  if (text_width < 1.0f)
  {
    text_width = 1.0f;
  }

  text_size = ldk_ttf_measure_text_cstr_wrapped(ui->font, text, text_width);
  height = text_size.h;
  if (height < LDK_UI_DEFAULT_CONTROL_HEIGHT)
  {
    height = LDK_UI_DEFAULT_CONTROL_HEIGHT;
  }

  ldk_ui_set_next_height(ui, ldk_ui_px(height));
  ldk_ui_begin_horizontal(ui);

  ldk_ui_set_next_width(ui, ldk_ui_px(gutter_width));
  ldk_ui_spacer(ui);
  gutter_rect = ldk_ui_last_rect(ui);

  ldk_ui_set_next_weight(ui, 1.0f);
  ldk_ui_clipboard_window_set(ui, editor->window);
  ldk_ui_selectable_text(ui, text);
  text_rect = ldk_ui_last_rect(ui);

  ldk_ui_end_horizontal(ui);

  s_editor_console_view_icons_draw(editor, text, gutter_rect, text_rect);
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
    s_editor_console_last_message_cache_reset(&s_last_message_cache, editor);
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
  s_editor_console_document_draw(editor);
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
