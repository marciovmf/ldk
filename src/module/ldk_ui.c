#include <ldk_common.h>
#include <ldk_geom.h>
#include <ldk_os.h>
#include <ldk_ttf.h>
#include <module/ldk_ui.h>
#include <stdx/stdx_math.h>
#include <stdx/stdx_array.h>
#include <stdx/stdx_string.h>
#include <string.h>
#include <math.h>

#define LDK_UI_DEFAULT_CONTROL_HEIGHT 22.0f
#define LDK_UI_DEFAULT_SPACING 4.0f
#define LDK_UI_DEFAULT_PADDING 4.0f
#define LDK_UI_TITLE_BAR_HEIGHT 24.0f

#define s_ui_maxf float_max
#define s_ui_minf float_min
#define s_ui_clampf float_clamp
#define s_ui_rect_contains ldk_rectf_contains
#define s_ui_rect_intersect ldk_rectf_intersect

static LDKUISize s_ui_widget_text_size(LDKUIContext *ctx, char const *text);
static void s_ui_windows_destroy_all(LDKUIContext *ctx);
static void s_ui_windows_clear_frame_buffers(LDKUIContext *ctx);
static void s_ui_window_destroy_buffers(LDKUIWindow *window);
static void s_ui_windows_refresh_z_order(LDKUIContext *ctx);
static void s_ui_append_window_draw_data(
    LDKUIContext *ctx, LDKUIWindow *window);
static void s_ui_submit_windows_in_z_order(LDKUIContext *ctx);
static void s_ui_append_draw_data(LDKUIContext *ctx,
    XArray_ldk_ui_vertex *vertices, XArray_ldk_ui_u32 *indices,
    XArray_ldk_ui_draw_cmd *commands);
static void s_ui_submit_popup_draw_data(LDKUIContext *ctx);
static void s_ui_close_popups_on_outside_click(LDKUIContext *ctx);
static void s_ui_window_cache_gc(LDKUIContext *ctx);

/**
 * Mixes a 32-bit value into an existing UI identifier hash.
 * @arg hash Current hash value.
 * @arg value Value to mix into the hash.
 * @return Updated hash value.
 */
static LDKUIId s_ui_id_hash_u32(LDKUIId hash, u32 value)
{
  hash ^= value + 0x9e3779b9u + (hash << 6) + (hash >> 2);
  return hash;
}

/**
 * Mixes every byte of a null-terminated string into a UI identifier hash.
 * @arg hash Current hash value.
 * @arg text String to mix into the hash. May be NULL.
 * @return Updated hash value, or the original hash when text is NULL.
 */
static LDKUIId s_ui_id_hash_cstr(LDKUIId hash, char const *text)
{
  char const *cursor = text;

  if (cursor == NULL)
  {
    return hash;
  }

  while (*cursor != 0)
  {
    hash = s_ui_id_hash_u32(hash, (u32)(uint8_t)*cursor);
    cursor += 1;
  }

  return hash;
}

/**
 * Checks whether a codepoint is treated as intra-line word spacing.
 * @arg codepoint Unicode codepoint to classify.
 * @return true for space, tab, or carriage return. False otherwise.
 */
static bool s_ui_text_codepoint_is_word_space(u32 codepoint)
{
  return codepoint == ' ' || codepoint == '\t' || codepoint == '\r';
}

/**
 * Calculates the horizontal advance for a codepoint, including kerning.
 * @arg font Font instance used to retrieve glyph and kerning information.
 * @arg previous_codepoint Codepoint that precedes the current one, or zero.
 * @arg codepoint Codepoint whose advance will be calculated.
 * @return Horizontal advance in pixels, or zero when no valid glyph exists.
 */
static float s_ui_text_codepoint_advance_get(
    LDKFontInstance *font, u32 previous_codepoint, u32 codepoint)
{
  LDKGlyph const *glyph = NULL;
  float advance = 0.0f;

  if (font == NULL)
  {
    return 0.0f;
  }

  glyph = ldk_ttf_get_glyph(font, codepoint);

  if (glyph == NULL || !glyph->valid)
  {
    return 0.0f;
  }

  if (previous_codepoint != 0)
  {
    advance += ldk_ttf_get_kerning(font, previous_codepoint, codepoint);
  }

  advance += (float)glyph->advance_x;

  return advance;
}

/**
 * Advances a UTF-8 byte cursor past spaces, tabs, and carriage returns.
 * @arg cursor Current byte position in a null-terminated string. May be NULL.
 * @return First byte that is not word spacing, or NULL when cursor is NULL.
 */
static char const *s_ui_text_skip_word_spaces(char const *cursor)
{
  char const *it = cursor;

  if (it == NULL)
  {
    return NULL;
  }

  while (*it == ' ' || *it == '\t' || *it == '\r')
  {
    it += 1;
  }

  return it;
}

/**
 * Finds the next line produced by wrapping text to a maximum width.
 * @arg font Font instance used to measure codepoint advances.
 * @arg start Byte position from which line parsing starts.
 * @arg max_width Maximum line width. Non-positive values disable wrapping.
 * @arg out_line_start Receives the first byte included in the line.
 * @arg out_line_end Receives the byte immediately after the visible line text.
 * @arg out_next Receives the byte from which the following line must start.
 * @arg out_width Receives the measured width of the produced line.
 * @return true when a line was produced. False for invalid or empty input.
 */
static bool s_ui_text_wrapped_next_line(LDKFontInstance *font,
    char const *start, float max_width, char const **out_line_start,
    char const **out_line_end, char const **out_next, float *out_width)
{
  char const *line_start = start;
  char const *cursor = NULL;
  char const *line_end = NULL;
  char const *last_break_next = NULL;
  char const *last_break_line_end = NULL;
  float width = 0.0f;
  float line_end_width = 0.0f;
  float last_break_width = 0.0f;
  u32 previous_codepoint = 0;
  bool has_visible_codepoint = false;

  if (out_line_start != NULL)
  {
    *out_line_start = start;
  }

  if (out_line_end != NULL)
  {
    *out_line_end = start;
  }

  if (out_next != NULL)
  {
    *out_next = start;
  }

  if (out_width != NULL)
  {
    *out_width = 0.0f;
  }

  if (font == NULL || start == NULL || *start == '\0')
  {
    return false;
  }

  if (max_width <= 0.0f)
  {
    max_width = 3.402823466e+38F;
  }

  line_start = s_ui_text_skip_word_spaces(start);
  cursor = line_start;
  line_end = line_start;

  if (*cursor == '\n')
  {
    if (out_line_start != NULL)
    {
      *out_line_start = cursor;
    }

    if (out_line_end != NULL)
    {
      *out_line_end = cursor;
    }

    if (out_next != NULL)
    {
      *out_next = cursor + 1;
    }

    return true;
  }

  while (*cursor != '\0')
  {
    char const *before = cursor;
    u32 codepoint = 0;

    if (!ldk_ttf_utf8_consume_codepoint(&cursor, &codepoint))
    {
      break;
    }

    if (codepoint == '\n')
    {
      if (out_line_start != NULL)
      {
        *out_line_start = line_start;
      }

      if (out_line_end != NULL)
      {
        *out_line_end = line_end;
      }

      if (out_next != NULL)
      {
        *out_next = cursor;
      }

      if (out_width != NULL)
      {
        *out_width = line_end_width;
      }

      return true;
    }

    if (s_ui_text_codepoint_is_word_space(codepoint))
    {
      if (has_visible_codepoint)
      {
        last_break_next = s_ui_text_skip_word_spaces(cursor);
        last_break_line_end = line_end;
        last_break_width = line_end_width;
      }

      width +=
          s_ui_text_codepoint_advance_get(font, previous_codepoint, codepoint);
      previous_codepoint = codepoint;
      continue;
    }

    float advance =
        s_ui_text_codepoint_advance_get(font, previous_codepoint, codepoint);

    if (has_visible_codepoint && width + advance > max_width)
    {
      if (last_break_next != NULL && last_break_next > line_start)
      {
        if (out_line_start != NULL)
        {
          *out_line_start = line_start;
        }

        if (out_line_end != NULL)
        {
          *out_line_end = last_break_line_end;
        }

        if (out_next != NULL)
        {
          *out_next = last_break_next;
        }

        if (out_width != NULL)
        {
          *out_width = last_break_width;
        }

        return true;
      }

      if (out_line_start != NULL)
      {
        *out_line_start = line_start;
      }

      if (out_line_end != NULL)
      {
        *out_line_end = line_end;
      }

      if (out_next != NULL)
      {
        *out_next = before;
      }

      if (out_width != NULL)
      {
        *out_width = line_end_width;
      }

      return true;
    }

    width += advance;
    previous_codepoint = codepoint;
    line_end = cursor;
    line_end_width = width;
    has_visible_codepoint = true;
  }

  if (out_line_start != NULL)
  {
    *out_line_start = line_start;
  }

  if (out_line_end != NULL)
  {
    *out_line_end = line_end;
  }

  if (out_next != NULL)
  {
    *out_next = cursor;
  }

  if (out_width != NULL)
  {
    *out_width = line_end_width;
  }

  return true;
}

/**
 * Measures the byte length of a null-terminated string as a 32-bit value.
 * @arg text String to measure. May be NULL.
 * @return String length in bytes, clamped to UINT32_MAX.
 */
static u32 s_ui_text_cstr_len_u32(char const *text)
{
  size_t len = 0;

  if (text == NULL)
  {
    return 0;
  }

  len = strlen(text);

  if (len > UINT32_MAX)
  {
    return UINT32_MAX;
  }

  return (u32)len;
}

/**
 * Calculates the text origin required to keep an input cursor visible.
 * @arg ctx UI context that owns the font used for measurement.
 * @arg text Null-terminated text displayed by the input widget.
 * @arg cursor UTF-8 byte offset of the editing cursor.
 * @arg rect Rectangle allocated to the input widget.
 * @arg focused Whether the input widget currently owns focus.
 * @return Horizontal screen position at which the text must be rendered.
 */
static float s_ui_input_box_text_x(LDKUIContext *ctx, char const *text,
    u32 cursor, LDKUIRect rect, bool focused)
{
  const float padding_x = 6.0f;
  float text_x = rect.x + padding_x;

  if (!focused)
  {
    return text_x;
  }

  u32 text_length = s_ui_text_cstr_len_u32(text);
  if (cursor > text_length)
  {
    cursor = text_length;
  }

  LDKTextSize cursor_text_size =
      ldk_ttf_measure_text_cstrn(ctx->font, text, cursor);
  float available_width = rect.w - padding_x * 2.0f;

  if (available_width < 0.0f)
  {
    available_width = 0.0f;
  }

  if (cursor_text_size.w > available_width)
  {
    text_x -= cursor_text_size.w - available_width;
  }

  return text_x;
}

/**
 * Finds the nearest UTF-8 cursor position for a horizontal screen coordinate.
 * @arg ctx UI context that owns the font used for measurement.
 * @arg text Null-terminated input text.
 * @arg text_x Horizontal screen position at which the text begins.
 * @arg x Horizontal screen coordinate to map into the text.
 * @return UTF-8 byte offset nearest to the supplied coordinate.
 */
static u32 s_ui_input_box_cursor_from_x(
    LDKUIContext *ctx, char const *text, float text_x, float x)
{
  float local_x = x - text_x;
  u32 len = s_ui_text_cstr_len_u32(text);
  u32 best = 0;
  float best_distance = 1000000000.0f;
  u32 cursor = 0;

  if (local_x <= 0.0f)
  {
    return 0;
  }

  while (cursor <= len)
  {
    LDKTextSize text_size = ldk_ttf_measure_text_cstrn(ctx->font, text, cursor);
    float cursor_x = text_size.w;
    float distance = fabsf(cursor_x - local_x);

    if (distance < best_distance)
    {
      best_distance = distance;
      best = cursor;
    }

    if (cursor == len)
    {
      break;
    }

    cursor = x_utf8_next(text, cursor);
  }

  return best;
}

/**
 * Moves an input cursor to the previous UTF-8 codepoint boundary.
 * @arg text Null-terminated UTF-8 input text.
 * @arg cursor Current cursor byte offset.
 * @return Byte offset of the previous codepoint boundary.
 */
static u32 s_ui_input_text_cursor_prev(char const *text, u32 cursor)
{
  u32 len = s_ui_text_cstr_len_u32(text);
  u32 previous;

  if (text == NULL || cursor == 0)
  {
    return 0;
  }

  if (cursor > len)
  {
    cursor = len;
  }

  previous = x_utf8_prev(text, cursor);

  if (previous >= cursor || previous > len)
  {
    return 0;
  }

  return previous;
}

/**
 * Moves an input cursor to the next UTF-8 codepoint boundary.
 * @arg text Null-terminated UTF-8 input text.
 * @arg cursor Current cursor byte offset.
 * @return Byte offset of the next codepoint boundary.
 */
static u32 s_ui_input_text_cursor_next(char const *text, u32 cursor)
{
  u32 len = s_ui_text_cstr_len_u32(text);
  u32 next;

  if (text == NULL)
  {
    return 0;
  }

  if (cursor >= len)
  {
    return len;
  }

  next = x_utf8_next(text, cursor);

  if (next <= cursor || next > len)
  {
    return len;
  }

  return next;
}

/**
 * Removes a byte range from a null-terminated text buffer.
 * @arg buffer Mutable text buffer from which bytes will be removed.
 * @arg buffer_len Current text length in bytes.
 * @arg start Inclusive byte offset of the range to remove.
 * @arg end Exclusive byte offset of the range to remove.
 * @return true when a valid non-empty range was removed. False otherwise.
 */
static bool s_ui_text_delete_range(
    char *buffer, u32 buffer_len, u32 start, u32 end)
{
  if (buffer == NULL || start >= end || end > buffer_len)
  {
    return false;
  }

  memmove(buffer + start, buffer + end, (size_t)(buffer_len - end + 1));
  return true;
}

/**
 * Inserts bytes into a null-terminated text buffer at the editing cursor.
 * @arg buffer Mutable destination text buffer.
 * @arg buffer_size Total capacity of the destination buffer in bytes.
 * @arg cursor Cursor byte offset, updated to follow the inserted bytes.
 * @arg text Bytes to insert into the destination buffer.
 * @arg text_len Number of source bytes requested for insertion.
 * @return true when at least one byte was inserted. False otherwise.
 */
static bool s_ui_text_insert_bytes(
    char *buffer, u32 buffer_size, u32 *cursor, char const *text, u32 text_len)
{
  u32 buffer_len = s_ui_text_cstr_len_u32(buffer);

  if (buffer == NULL || cursor == NULL || text == NULL || buffer_size == 0 ||
      text_len == 0)
  {
    return false;
  }

  if (*cursor > buffer_len)
  {
    *cursor = buffer_len;
  }

  if (buffer_len + text_len >= buffer_size)
  {
    text_len = buffer_size - buffer_len - 1;
  }

  if (text_len == 0)
  {
    return false;
  }

  memmove(buffer + *cursor + text_len, buffer + *cursor,
      (size_t)(buffer_len - *cursor + 1));
  memcpy(buffer + *cursor, text, (size_t)text_len);
  *cursor += text_len;
  return true;
}

/**
 * Checks whether the Shift modifier is currently held.
 * @arg ctx UI context that provides the current keyboard state.
 * @return true when Shift is held. False otherwise.
 */
static bool s_ui_input_keyboard_shift_pressed(LDKUIContext *ctx)
{
  if (ctx->keyboard == NULL)
  {
    return false;
  }

  return ldk_os_keyboard_key_is_pressed(
      (LDKKeyboardState *)ctx->keyboard, LDK_KEYCODE_SHIFT);
}

/**
 * Checks whether Backspace was pressed during the current frame.
 * @arg ctx UI context that provides the current keyboard state.
 * @return true when Backspace was pressed. False otherwise.
 */
static bool s_ui_input_keyboard_backspace_pressed(LDKUIContext *ctx)
{
  if (ctx->keyboard == NULL)
  {
    return false;
  }

  return ldk_os_keyboard_key_down(
      (LDKKeyboardState *)ctx->keyboard, LDK_KEYCODE_BACKSPACE);
}

/**
 * Checks whether Delete was pressed during the current frame.
 * @arg ctx UI context that provides the current keyboard state.
 * @return true when Delete was pressed. False otherwise.
 */
static bool s_ui_input_keyboard_delete_pressed(LDKUIContext *ctx)
{
  if (ctx->keyboard == NULL)
  {
    return false;
  }

  return ldk_os_keyboard_key_down(
      (LDKKeyboardState *)ctx->keyboard, LDK_KEYCODE_DELETE);
}

/**
 * Checks whether the Select All keyboard chord was pressed.
 * @arg ctx UI context that provides the current keyboard state.
 * @return true when Control is held and A was pressed. False otherwise.
 */
static bool s_ui_input_keyboard_ctrla_pressed(LDKUIContext *ctx)
{
  if (ctx->keyboard == NULL)
  {
    return false;
  }

  return ldk_os_keyboard_key_down(
             (LDKKeyboardState *)ctx->keyboard, LDK_KEYCODE_A) &&
         ldk_os_keyboard_key_is_pressed(
             (LDKKeyboardState *)ctx->keyboard, LDK_KEYCODE_CONTROL);
}

/**
 * Checks whether Home was pressed during the current frame.
 * @arg ctx UI context that provides the current keyboard state.
 * @return true when Home was pressed. False otherwise.
 */
static bool s_ui_input_keyboard_home_pressed(LDKUIContext *ctx)
{
  if (ctx->keyboard == NULL)
  {
    return false;
  }

  return ldk_os_keyboard_key_down(
      (LDKKeyboardState *)ctx->keyboard, LDK_KEYCODE_HOME);
}

/**
 * Checks whether End was pressed during the current frame.
 * @arg ctx UI context that provides the current keyboard state.
 * @return true when End was pressed. False otherwise.
 */
static bool s_ui_input_keyboard_end_pressed(LDKUIContext *ctx)
{
  if (ctx->keyboard == NULL)
  {
    return false;
  }

  return ldk_os_keyboard_key_down(
      (LDKKeyboardState *)ctx->keyboard, LDK_KEYCODE_END);
}

/**
 * Checks whether Enter was pressed during the current frame.
 * @arg ctx UI context that provides the current keyboard state.
 * @return true when Enter was pressed. False otherwise.
 */
static bool s_ui_input_keyboard_enter_pressed(LDKUIContext *ctx)
{
  if (ctx->keyboard == NULL)
  {
    return false;
  }

  return ldk_os_keyboard_key_down(
      (LDKKeyboardState *)ctx->keyboard, LDK_KEYCODE_ENTER);
}

/**
 * Checks whether the Left Arrow key was pressed during the current frame.
 * @arg ctx UI context that provides the current keyboard state.
 * @return true when Left Arrow was pressed. False otherwise.
 */
static bool s_ui_input_keyboard_left_pressed(LDKUIContext *ctx)
{
  if (ctx->keyboard == NULL)
  {
    return false;
  }

  return ldk_os_keyboard_key_down(
      (LDKKeyboardState *)ctx->keyboard, LDK_KEYCODE_LEFT);
}

/**
 * Checks whether the Right Arrow key was pressed during the current frame.
 * @arg ctx UI context that provides the current keyboard state.
 * @return true when Right Arrow was pressed. False otherwise.
 */
static bool s_ui_input_keyboard_right_pressed(LDKUIContext *ctx)
{
  if (ctx->keyboard == NULL)
  {
    return false;
  }

  return ldk_os_keyboard_key_down(
      (LDKKeyboardState *)ctx->keyboard, LDK_KEYCODE_RIGHT);
}

/**
 * Restarts the text cursor blink cycle in its visible state.
 * @arg ctx UI context whose cursor blink state will be reset.
 */
static void s_ui_input_cursor_blink_reset(LDKUIContext *ctx)
{
  if (ctx == NULL)
  {
    return;
  }

  ctx->text_cursor_blink_timer = 0.0f;
  ctx->text_cursor_blink_visible = true;
}

/**
 * Checks whether draw data is currently being submitted to a popup scope.
 * @arg ctx UI context whose popup stack will be inspected.
 * @return true when at least one popup scope is active. False otherwise.
 */
static bool s_ui_rendering_popup(LDKUIContext *ctx)
{
  if (ctx == NULL || ctx->popup_stack == NULL)
  {
    return false;
  }

  return !x_array_ldk_ui_popup_stack_entry_is_empty(ctx->popup_stack);
}

/**
 * Selects the vertex buffer for the currently active rendering scope.
 * @arg ctx UI context that owns the global, window, and popup buffers.
 * @return Active popup, window, or global vertex buffer.
 */
static XArray_ldk_ui_vertex *s_ui_target_vertices(LDKUIContext *ctx)
{
  if (s_ui_rendering_popup(ctx) && ctx->popup_vertices != NULL)
  {
    return ctx->popup_vertices;
  }

  if (ctx != NULL && ctx->current_window != NULL &&
      ctx->current_window->vertices != NULL)
  {
    return ctx->current_window->vertices;
  }

  return ctx->vertices;
}

/**
 * Selects the index buffer for the currently active rendering scope.
 * @arg ctx UI context that owns the global, window, and popup buffers.
 * @return Active popup, window, or global index buffer.
 */
static XArray_ldk_ui_u32 *s_ui_target_indices(LDKUIContext *ctx)
{
  if (s_ui_rendering_popup(ctx) && ctx->popup_indices != NULL)
  {
    return ctx->popup_indices;
  }

  if (ctx != NULL && ctx->current_window != NULL &&
      ctx->current_window->indices != NULL)
  {
    return ctx->current_window->indices;
  }

  return ctx->indices;
}

/**
 * Selects the draw command buffer for the active rendering scope.
 * @arg ctx UI context that owns the global, window, and popup buffers.
 * @return Active popup, window, or global draw command buffer.
 */
static XArray_ldk_ui_draw_cmd *s_ui_target_commands(LDKUIContext *ctx)
{
  if (s_ui_rendering_popup(ctx) && ctx->popup_commands != NULL)
  {
    return ctx->popup_commands;
  }

  if (ctx != NULL && ctx->current_window != NULL &&
      ctx->current_window->commands != NULL)
  {
    return ctx->current_window->commands;
  }

  return ctx->commands;
}

/**
 * Appends a draw command or merges it with the previous compatible command.
 * @arg ctx UI context that selects the active command buffer.
 * @arg texture Texture handle used by the submitted geometry.
 * @arg clip_rect Clip rectangle applied while executing the command.
 * @arg index_offset Offset of the first index consumed by the command.
 * @arg index_count Number of indices consumed by the command.
 */
static void s_ui_render_add_draw_cmd(LDKUIContext *ctx,
    LDKUITextureHandle texture, LDKUIRect clip_rect, u32 index_offset,
    u32 index_count)
{
  XArray_ldk_ui_draw_cmd *commands = s_ui_target_commands(ctx);
  LDKUIDrawCmd *back = x_array_ldk_ui_draw_cmd_back(commands);

  if (back != NULL)
  {
    bool same_texture = back->texture == texture;
    bool same_clip =
        back->clip_rect.x == clip_rect.x && back->clip_rect.y == clip_rect.y &&
        back->clip_rect.w == clip_rect.w && back->clip_rect.h == clip_rect.h;
    bool contiguous = back->index_offset + back->index_count == index_offset;

    if (same_texture && same_clip && contiguous)
    {
      back->index_count += index_count;
      return;
    }
  }

  LDKUIDrawCmd cmd;
  cmd.texture = texture;
  cmd.clip_rect = clip_rect;
  cmd.index_offset = index_offset;
  cmd.index_count = index_count;

  x_array_ldk_ui_draw_cmd_push(commands, cmd);
}

/**
 * Emits a textured quad into the active rendering buffers.
 * @arg ctx UI context that owns the active rendering buffers.
 * @arg rect Screen-space rectangle occupied by the quad.
 * @arg uv Texture-space rectangle mapped onto the quad.
 * @arg color Color multiplied with the quad texture.
 * @arg clip_rect Clip rectangle applied to the generated draw command.
 * @arg texture Texture sampled by the quad, or zero for an untextured quad.
 */
static void s_ui_render_quad_uv(LDKUIContext *ctx, LDKUIRect rect, LDKUIRect uv,
    u32 color, LDKUIRect clip_rect, LDKUITextureHandle texture)
{
  XArray_ldk_ui_vertex *vertices = s_ui_target_vertices(ctx);
  XArray_ldk_ui_u32 *indices = s_ui_target_indices(ctx);
  u32 index_offset = x_array_ldk_ui_u32_count(indices);
  u32 base_index = x_array_ldk_ui_vertex_count(vertices);

  color = LDK_RGBA32(color);

  x_array_ldk_ui_vertex_push(
      vertices, (LDKUIVertex){rect.x, rect.y, uv.x, uv.y, color});
  x_array_ldk_ui_vertex_push(vertices,
      (LDKUIVertex){rect.x + rect.w, rect.y, uv.x + uv.w, uv.y, color});
  x_array_ldk_ui_vertex_push(
      vertices, (LDKUIVertex){rect.x + rect.w, rect.y + rect.h, uv.x + uv.w,
                    uv.y + uv.h, color});
  x_array_ldk_ui_vertex_push(vertices,
      (LDKUIVertex){rect.x, rect.y + rect.h, uv.x, uv.y + uv.h, color});

  x_array_ldk_ui_u32_push(indices, base_index + 0);
  x_array_ldk_ui_u32_push(indices, base_index + 1);
  x_array_ldk_ui_u32_push(indices, base_index + 2);
  x_array_ldk_ui_u32_push(indices, base_index + 2);
  x_array_ldk_ui_u32_push(indices, base_index + 3);
  x_array_ldk_ui_u32_push(indices, base_index + 0);

  s_ui_render_add_draw_cmd(ctx, texture, clip_rect, index_offset, 6);
}

/**
 * Emits a quad using the full UV range of its texture.
 * @arg ctx UI context that owns the active rendering buffers.
 * @arg rect Screen-space rectangle occupied by the quad.
 * @arg color Color multiplied with the quad texture.
 * @arg clip_rect Clip rectangle applied to the generated draw command.
 * @arg texture Texture sampled by the quad, or zero for an untextured quad.
 */
static void s_ui_render_quad(LDKUIContext *ctx, LDKUIRect rect, u32 color,
    LDKUIRect clip_rect, LDKUITextureHandle texture)
{
  LDKUIRect uv = {0.0f, 0.0f, 1.0f, 1.0f};
  s_ui_render_quad_uv(ctx, rect, uv, color, clip_rect, texture);
}

/**
 * Checks whether an icon contains all data required for rendering.
 * @arg icon Icon descriptor to validate.
 * @return true when texture, UV area, and display size are valid.
 */
static bool s_ui_icon_valid(LDKUIIcon icon)
{
  return icon.texture != 0 && icon.uv.w > 0.0f && icon.uv.h > 0.0f &&
         icon.size.w > 0.0f && icon.size.h > 0.0f;
}

/**
 * Retrieves an icon from the active theme and applies its control text color.
 * @arg ctx UI context that owns the active theme.
 * @arg slot Theme icon slot to retrieve.
 * @return Configured theme icon, or an empty icon for an invalid request.
 */
static LDKUIIcon s_ui_theme_icon(LDKUIContext *ctx, LDKUIThemeIconSlot slot)
{
  LDKUIIcon icon = {0};

  if (ctx == NULL || slot < 0 || slot >= LDK_UI_THEME_ICON_COUNT)
  {
    return icon;
  }

  icon = ctx->theme.icons[slot];
  icon.color = ctx->theme.colors[LDK_UI_COLOR_CONTROL_TEXT];
  return icon;
}

/**
 * Renders an icon into a screen-space rectangle.
 * @arg ctx UI context that owns the active rendering buffers.
 * @arg icon Icon descriptor containing texture, UV, and tint information.
 * @arg rect Screen-space rectangle occupied by the icon.
 * @arg color Requested tint color. The current implementation uses icon.color.
 * @arg clip_rect Clip rectangle applied while rendering the icon.
 */
static void s_ui_render_icon(LDKUIContext *ctx, LDKUIIcon icon, LDKUIRect rect,
    u32 color, LDKUIRect clip_rect)
{
  if (!s_ui_icon_valid(icon))
  {
    return;
  }

  s_ui_render_quad_uv(ctx, rect, icon.uv, icon.color, clip_rect,
      (LDKUITextureHandle)icon.texture);
}

/**
 * Renders a solid border around a rectangle.
 * @arg ctx UI context that owns the active rendering buffers.
 * @arg rect Rectangle enclosed by the border.
 * @arg size Border thickness in pixels.
 * @arg color Border color.
 * @arg clip_rect Clip rectangle applied while rendering the border.
 */
static void s_ui_render_border(LDKUIContext *ctx, LDKUIRect rect, float size,
    u32 color, LDKUIRect clip_rect)
{
  float border_size = s_ui_maxf(size, 0.0f);
  LDKUIRect top;
  LDKUIRect bottom;
  LDKUIRect left;
  LDKUIRect right;

  if (border_size <= 0.0f)
  {
    return;
  }

  top = (LDKUIRect){rect.x, rect.y, rect.w, border_size};
  bottom =
      (LDKUIRect){rect.x, rect.y + rect.h - border_size, rect.w, border_size};
  left = (LDKUIRect){rect.x, rect.y, border_size, rect.h};
  right =
      (LDKUIRect){rect.x + rect.w - border_size, rect.y, border_size, rect.h};

  s_ui_render_quad(ctx, top, color, clip_rect, 0);
  s_ui_render_quad(ctx, bottom, color, clip_rect, 0);
  s_ui_render_quad(ctx, left, color, clip_rect, 0);
  s_ui_render_quad(ctx, right, color, clip_rect, 0);
}

/**
 * Renders the selection highlight behind a byte range of input text.
 * @arg ctx UI context that owns the font and rendering buffers.
 * @arg text Null-terminated text containing the selected range.
 * @arg start Byte offset of one end of the selection.
 * @arg end Byte offset of the other end of the selection.
 * @arg text_x Horizontal screen position at which the text begins.
 * @arg rect Rectangle occupied by the input widget.
 * @arg clip Clip rectangle applied to the highlight.
 * @arg color Selection highlight color.
 */
static void s_ui_render_text_highlight(LDKUIContext *ctx, char const *text,
    u32 start, u32 end, float text_x, LDKUIRect rect, LDKUIRect clip, u32 color)
{
  if (ctx == NULL || text == NULL || start == end)
  {
    return;
  }

  if (start > end)
  {
    u32 temp = start;
    start = end;
    end = temp;
  }

  LDKTextSize text_size = ldk_ttf_measure_text_cstrn(ctx->font, text, start);
  float x0 = text_x + text_size.w;

  text_size = ldk_ttf_measure_text_cstrn(ctx->font, text, end);
  float x1 = text_x + text_size.w;

  LDKUIRect highlight_rect;
  highlight_rect.x = x0;
  highlight_rect.y = rect.y + 3.0f;
  highlight_rect.w = x1 - x0;
  highlight_rect.h = rect.h - 6.0f;

  s_ui_render_quad(ctx, highlight_rect, color, clip, 0);
}

/**
 * Emits glyph geometry for a null-terminated UTF-8 string.
 * @arg ctx UI context that owns the font and rendering buffers.
 * @arg text Null-terminated UTF-8 text to render.
 * @arg x Horizontal screen position of the text origin.
 * @arg y Vertical screen position of the text origin.
 * @arg color Text color.
 * @arg clip_rect Clip rectangle applied while rendering the text.
 */
static void s_ui_render_text(LDKUIContext *ctx, char const *text, float x,
    float y, u32 color, LDKUIRect clip_rect)
{
  LDKFontInstance *font;
  LDKFontMetrics metrics;
  float pen_x;
  float pen_y;
  u32 prev_codepoint;
  char const *cursor;

  if (ctx == NULL || text == NULL)
  {
    return;
  }

  font = ctx->font;

  if (font == NULL)
  {
    return;
  }

  color = LDK_RGBA32(color);
  metrics = ldk_ttf_get_metrics(font);
  pen_x = x;
  pen_y = y + metrics.ascent;
  prev_codepoint = 0;
  cursor = text;

  while (*cursor != '\0')
  {
    u32 codepoint = 0;
    LDKGlyph const *glyph;
    LDKFontPageInfo page;
    float gx0;
    float gy0;
    float gx1;
    float gy1;
    float u0;
    float v0;
    float u1;
    float v1;
    u32 base_index;
    u32 index_offset;
    LDKUITextureHandle texture = 0;
    XArray_ldk_ui_vertex *vertices = s_ui_target_vertices(ctx);
    XArray_ldk_ui_u32 *indices = s_ui_target_indices(ctx);

    if (!ldk_ttf_utf8_consume_codepoint(&cursor, &codepoint))
    {
      break;
    }

    if (codepoint == '\n')
    {
      pen_x = x;
      pen_y += metrics.line_height;
      prev_codepoint = 0;
      continue;
    }

    glyph = ldk_ttf_get_glyph(font, codepoint);

    if (glyph == NULL || !glyph->valid)
    {
      prev_codepoint = 0;
      continue;
    }

    if (prev_codepoint != 0)
    {
      pen_x += ldk_ttf_get_kerning(font, prev_codepoint, codepoint);
    }

    if (!ldk_ttf_get_page_info(font, glyph->page_index, &page))
    {
      pen_x += (float)glyph->advance_x;
      prev_codepoint = codepoint;
      continue;
    }

    gx0 = pen_x + (float)glyph->offset_x;
    gy0 = pen_y + (float)glyph->offset_y;
    gx1 = gx0 + (float)(glyph->atlas_x1 - glyph->atlas_x0);
    gy1 = gy0 + (float)(glyph->atlas_y1 - glyph->atlas_y0);

    u0 = (float)glyph->atlas_x0 / (float)page.width;
    v0 = (float)glyph->atlas_y0 / (float)page.height;
    u1 = (float)glyph->atlas_x1 / (float)page.width;
    v1 = (float)glyph->atlas_y1 / (float)page.height;

    index_offset = x_array_ldk_ui_u32_count(indices);
    base_index = x_array_ldk_ui_vertex_count(vertices);

    x_array_ldk_ui_vertex_push(
        vertices, (LDKUIVertex){gx0, gy0, u0, v0, color});
    x_array_ldk_ui_vertex_push(
        vertices, (LDKUIVertex){gx1, gy0, u1, v0, color});
    x_array_ldk_ui_vertex_push(
        vertices, (LDKUIVertex){gx1, gy1, u1, v1, color});
    x_array_ldk_ui_vertex_push(
        vertices, (LDKUIVertex){gx0, gy1, u0, v1, color});

    x_array_ldk_ui_u32_push(indices, base_index + 0);
    x_array_ldk_ui_u32_push(indices, base_index + 1);
    x_array_ldk_ui_u32_push(indices, base_index + 2);
    x_array_ldk_ui_u32_push(indices, base_index + 2);
    x_array_ldk_ui_u32_push(indices, base_index + 3);
    x_array_ldk_ui_u32_push(indices, base_index + 0);

    if (ctx->get_font_page_texture != NULL)
    {
      texture = ctx->get_font_page_texture(
          ctx->font_texture_user, ctx->font, glyph->page_index);
    }

    s_ui_render_add_draw_cmd(ctx, texture, clip_rect, index_offset, 6);

    pen_x += (float)glyph->advance_x;
    prev_codepoint = codepoint;
  }
}

/**
 * Emits glyph geometry for a half-open UTF-8 byte range.
 * @arg ctx UI context that owns the font and rendering buffers.
 * @arg text_start First byte included in the rendered range.
 * @arg text_end First byte excluded from the rendered range.
 * @arg x Horizontal screen position of the text origin.
 * @arg y Vertical screen position of the text origin.
 * @arg color Text color.
 * @arg clip_rect Clip rectangle applied while rendering the text.
 */
static void s_ui_render_text_range(LDKUIContext *ctx, char const *text_start,
    char const *text_end, float x, float y, u32 color, LDKUIRect clip_rect)
{
  if (ctx == NULL || text_start == NULL || text_end == NULL ||
      text_start >= text_end)
  {
    return;
  }

  LDKFontInstance *font = ctx->font;

  if (font == NULL)
  {
    return;
  }

  color = LDK_RGBA32(color);
  LDKFontMetrics metrics = ldk_ttf_get_metrics(font);
  float pen_x = x;
  float pen_y = y + metrics.ascent;
  u32 prev_codepoint = 0;
  char const *cursor = text_start;

  while (cursor < text_end && *cursor != '\0')
  {
    u32 codepoint = 0;
    char const *before = cursor;

    if (!ldk_ttf_utf8_consume_codepoint(&cursor, &codepoint))
    {
      break;
    }

    if (cursor > text_end)
    {
      cursor = before;
      break;
    }

    if (codepoint == '\n')
    {
      break;
    }

    LDKGlyph const *glyph = ldk_ttf_get_glyph(font, codepoint);

    if (glyph == NULL || !glyph->valid)
    {
      prev_codepoint = 0;
      continue;
    }

    if (prev_codepoint != 0)
    {
      pen_x += ldk_ttf_get_kerning(font, prev_codepoint, codepoint);
    }

    LDKFontPageInfo page;

    if (!ldk_ttf_get_page_info(font, glyph->page_index, &page))
    {
      pen_x += (float)glyph->advance_x;
      prev_codepoint = codepoint;
      continue;
    }

    float gx0 = pen_x + (float)glyph->offset_x;
    float gy0 = pen_y + (float)glyph->offset_y;
    float gx1 = gx0 + (float)(glyph->atlas_x1 - glyph->atlas_x0);
    float gy1 = gy0 + (float)(glyph->atlas_y1 - glyph->atlas_y0);

    float u0 = (float)glyph->atlas_x0 / (float)page.width;
    float v0 = (float)glyph->atlas_y0 / (float)page.height;
    float u1 = (float)glyph->atlas_x1 / (float)page.width;
    float v1 = (float)glyph->atlas_y1 / (float)page.height;

    XArray_ldk_ui_vertex *vertices = s_ui_target_vertices(ctx);
    XArray_ldk_ui_u32 *indices = s_ui_target_indices(ctx);
    u32 index_offset = x_array_ldk_ui_u32_count(indices);
    u32 base_index = x_array_ldk_ui_vertex_count(vertices);

    x_array_ldk_ui_vertex_push(
        vertices, (LDKUIVertex){gx0, gy0, u0, v0, color});
    x_array_ldk_ui_vertex_push(
        vertices, (LDKUIVertex){gx1, gy0, u1, v0, color});
    x_array_ldk_ui_vertex_push(
        vertices, (LDKUIVertex){gx1, gy1, u1, v1, color});
    x_array_ldk_ui_vertex_push(
        vertices, (LDKUIVertex){gx0, gy1, u0, v1, color});

    x_array_ldk_ui_u32_push(indices, base_index + 0);
    x_array_ldk_ui_u32_push(indices, base_index + 1);
    x_array_ldk_ui_u32_push(indices, base_index + 2);
    x_array_ldk_ui_u32_push(indices, base_index + 2);
    x_array_ldk_ui_u32_push(indices, base_index + 3);
    x_array_ldk_ui_u32_push(indices, base_index + 0);

    LDKUITextureHandle texture = 0;

    if (ctx->get_font_page_texture != NULL)
    {
      texture = ctx->get_font_page_texture(
          ctx->font_texture_user, ctx->font, glyph->page_index);
    }

    s_ui_render_add_draw_cmd(ctx, texture, clip_rect, index_offset, 6);

    pen_x += (float)glyph->advance_x;
    prev_codepoint = codepoint;
  }
}

/**
 * Renders a UTF-8 string as multiple lines constrained to a maximum width.
 * @arg ctx UI context that owns the font and rendering buffers.
 * @arg text Null-terminated UTF-8 text to wrap and render.
 * @arg x Horizontal screen position of the first line.
 * @arg y Vertical screen position of the first line.
 * @arg max_width Maximum width available to each rendered line.
 * @arg color Text color.
 * @arg clip_rect Clip rectangle applied while rendering the text.
 */
static void s_ui_render_text_wrapped(LDKUIContext *ctx, char const *text,
    float x, float y, float max_width, u32 color, LDKUIRect clip_rect)
{
  if (ctx == NULL || ctx->font == NULL || text == NULL)
  {
    return;
  }

  float line_height = ldk_ttf_get_line_height(ctx->font);
  char const *cursor = text;
  float line_y = y;

  while (cursor != NULL && *cursor != '\0')
  {
    char const *line_start = NULL;
    char const *line_end = NULL;
    char const *next = NULL;
    if (!s_ui_text_wrapped_next_line(
            ctx->font, cursor, max_width, &line_start, &line_end, &next, NULL))
    {
      break;
    }

    s_ui_render_text_range(
        ctx, line_start, line_end, x, line_y, color, clip_rect);
    line_y += line_height;

    if (next == NULL || next <= cursor)
    {
      break;
    }

    cursor = next;
  }
}

/**
 * Renders an icon and a wrapped label inside a shared rectangle.
 * @arg ctx UI context that owns the font and rendering buffers.
 * @arg icon Optional icon displayed before the label.
 * @arg text Null-terminated label text.
 * @arg rect Rectangle available to the combined icon and label.
 * @arg color Label color.
 * @arg clip Clip rectangle applied while rendering the content.
 */
static void s_ui_render_icon_label(LDKUIContext *ctx, LDKUIIcon icon,
    char const *text, LDKUIRect rect, u32 color, LDKUIRect clip)
{
  LDKUIRect icon_rect;
  float text_x;
  float text_w;

  if (ctx == NULL)
  {
    return;
  }

  if (text == NULL)
  {
    text = "";
  }

  text_x = rect.x;

  if (s_ui_icon_valid(icon))
  {
    icon_rect.x = rect.x;
    icon_rect.y = rect.y + (rect.h - icon.size.h) * 0.5f;
    icon_rect.w = icon.size.w;
    icon_rect.h = icon.size.h;

    s_ui_render_icon(ctx, icon, icon_rect, color, clip);

    text_x = icon_rect.x + icon_rect.w + LDK_UI_DEFAULT_SPACING;
  }

  text_w = s_ui_maxf(0.0f, rect.x + rect.w - text_x);

  s_ui_render_text_wrapped(ctx, text, text_x, rect.y, text_w, color, clip);
}

/**
 * Renders a centered icon and single-line label inside a shared rectangle.
 * @arg ctx UI context that owns the font and rendering buffers.
 * @arg icon Optional icon displayed before the label.
 * @arg text Null-terminated label text.
 * @arg rect Rectangle available to the combined icon and label.
 * @arg color Label color.
 * @arg clip Clip rectangle applied while rendering the content.
 */
static void s_ui_render_icon_label_nowrap(LDKUIContext *ctx, LDKUIIcon icon,
    char const *text, LDKUIRect rect, u32 color, LDKUIRect clip)
{
  if (ctx == NULL)
  {
    return;
  }

  if (text == NULL)
  {
    text = "";
  }

  float cursor_x = rect.x;

  if (s_ui_icon_valid(icon))
  {
    LDKUIRect icon_rect;
    icon_rect.x = cursor_x;
    icon_rect.y = rect.y + (rect.h - icon.size.h) * 0.5f;
    icon_rect.w = icon.size.w;
    icon_rect.h = icon.size.h;

    s_ui_render_icon(ctx, icon, icon_rect, color, clip);

    cursor_x += icon.size.w;

    if (text[0] != '\0')
    {
      cursor_x += LDK_UI_DEFAULT_SPACING;
    }
  }

  if (text[0] != '\0')
  {
    LDKUISize text_size = s_ui_widget_text_size(ctx, text);
    float text_y = rect.y + (rect.h - text_size.h) * 0.5f;

    s_ui_render_text(ctx, text, cursor_x, text_y, color, clip);
  }
}

/**
 * Selects the themed control background color for a visual state.
 * @arg ctx UI context that owns the active theme.
 * @arg state Current visual state of the control.
 * @return Background color associated with the supplied visual state.
 */
static u32 s_ui_render_control_bg_color(
    LDKUIContext *ctx, LDKUIControlVisualState state)
{
  if (state == LDK_UI_CONTROL_VISUAL_STATE_HOVERED)
  {
    return ctx->theme.colors[LDK_UI_COLOR_CONTROL_BG_HOVERED];
  }

  if (state == LDK_UI_CONTROL_VISUAL_STATE_ACTIVE ||
      state == LDK_UI_CONTROL_VISUAL_STATE_ACTIVE_HOVERED)
  {
    return ctx->theme.colors[LDK_UI_COLOR_CONTROL_BG_ACTIVE];
  }

  return ctx->theme.colors[LDK_UI_COLOR_CONTROL_BG];
}

/**
 * Selects the themed control text color for a visual state.
 * @arg ctx UI context that owns the active theme.
 * @arg state Current visual state of the control.
 * @return Text color associated with the supplied visual state.
 */
static u32 s_ui_render_control_text_color(
    LDKUIContext *ctx, LDKUIControlVisualState state)
{
  if (state == LDK_UI_CONTROL_VISUAL_STATE_DISABLED)
  {
    return ctx->theme.colors[LDK_UI_COLOR_CONTROL_TEXT_DISABLED];
  }

  if (state == LDK_UI_CONTROL_VISUAL_STATE_ACTIVE ||
      state == LDK_UI_CONTROL_VISUAL_STATE_ACTIVE_HOVERED)
  {
    return ctx->theme.colors[LDK_UI_COLOR_CONTROL_TEXT_ACTIVE];
  }

  if (state == LDK_UI_CONTROL_VISUAL_STATE_HOVERED)
  {
    return ctx->theme.colors[LDK_UI_COLOR_CONTROL_TEXT_HOVERED];
  }

  return ctx->theme.colors[LDK_UI_COLOR_CONTROL_TEXT];
}

/**
 * Selects the themed control border color for a visual state.
 * @arg ctx UI context that owns the active theme.
 * @arg state Current visual state of the control.
 * @return Border color associated with the supplied visual state.
 */
static u32 s_ui_render_control_border_color(
    LDKUIContext *ctx, LDKUIControlVisualState state)
{
  if (state == LDK_UI_CONTROL_VISUAL_STATE_DISABLED)
  {
    return ctx->theme.colors[LDK_UI_COLOR_CONTROL_BORDER_DISABLED];
  }

  if (state == LDK_UI_CONTROL_VISUAL_STATE_ACTIVE ||
      state == LDK_UI_CONTROL_VISUAL_STATE_ACTIVE_HOVERED)
  {
    return ctx->theme.colors[LDK_UI_COLOR_CONTROL_BORDER_ACTIVE];
  }

  if (state == LDK_UI_CONTROL_VISUAL_STATE_HOVERED)
  {
    return ctx->theme.colors[LDK_UI_COLOR_CONTROL_BORDER_HOVERED];
  }

  return ctx->theme.colors[LDK_UI_COLOR_CONTROL_BORDER];
}

/**
 * Selects the themed slider track color for a visual state.
 * @arg ctx UI context that owns the active theme.
 * @arg state Current visual state of the slider.
 * @return Track color associated with the supplied visual state.
 */
static u32 s_ui_render_slider_track_color(
    LDKUIContext *ctx, LDKUIControlVisualState state)
{
  if (state == LDK_UI_CONTROL_VISUAL_STATE_ACTIVE ||
      state == LDK_UI_CONTROL_VISUAL_STATE_ACTIVE_HOVERED)
  {
    return ctx->theme.colors[LDK_UI_COLOR_SLIDER_TRACK_ACTIVE];
  }

  if (state == LDK_UI_CONTROL_VISUAL_STATE_HOVERED)
  {
    return ctx->theme.colors[LDK_UI_COLOR_SLIDER_TRACK_HOVERED];
  }

  return ctx->theme.colors[LDK_UI_COLOR_SLIDER_TRACK];
}

/**
 * Selects the themed slider thumb color for a visual state.
 * @arg ctx UI context that owns the active theme.
 * @arg state Current visual state of the slider.
 * @return Thumb color associated with the supplied visual state.
 */
static u32 s_ui_render_slider_thumb_color(
    LDKUIContext *ctx, LDKUIControlVisualState state)
{
  if (state == LDK_UI_CONTROL_VISUAL_STATE_ACTIVE ||
      state == LDK_UI_CONTROL_VISUAL_STATE_ACTIVE_HOVERED)
  {
    return ctx->theme.colors[LDK_UI_COLOR_SLIDER_THUMB_ACTIVE];
  }

  if (state == LDK_UI_CONTROL_VISUAL_STATE_HOVERED)
  {
    return ctx->theme.colors[LDK_UI_COLOR_SLIDER_THUMB_HOVERED];
  }

  return ctx->theme.colors[LDK_UI_COLOR_SLIDER_THUMB];
}

/**
 * Converts a slider value to a normalized position.
 * @arg value Slider value to normalize.
 * @arg min_value Minimum value represented by the slider.
 * @arg max_value Maximum value represented by the slider.
 * @return Value in the inclusive range from zero to one.
 */
static float s_ui_slider_normalize(
    float value, float min_value, float max_value)
{
  float range = max_value - min_value;

  if (range <= 0.0f)
  {
    return 0.0f;
  }

  return s_ui_clampf((value - min_value) / range, 0.0f, 1.0f);
}

/**
 * Converts a cursor position to a value within a horizontal slider range.
 * @arg rect Screen-space rectangle occupied by the slider.
 * @arg thumb_width Width of the draggable slider thumb.
 * @arg cursor_x Horizontal screen position of the cursor.
 * @arg min_value Minimum value represented by the slider.
 * @arg max_value Maximum value represented by the slider.
 * @return Slider value corresponding to the cursor position.
 */
static float s_ui_slider_value_from_cursor(LDKUIRect rect, float thumb_width,
    float cursor_x, float min_value, float max_value)
{
  float usable_width = s_ui_maxf(rect.w - thumb_width, 1.0f);
  float local_x = cursor_x - rect.x - thumb_width * 0.5f;
  float t = s_ui_clampf(local_x / usable_width, 0.0f, 1.0f);

  return min_value + (max_value - min_value) * t;
}

/**
 * Calculates the track and thumb rectangles for a scrollbar.
 * @arg rect Rectangle allocated to the scrollbar.
 * @arg visible_size Size of the visible content along the scrolling axis.
 * @arg content_size Total content size along the scrolling axis.
 * @arg scroll Current scroll offset along the scrolling axis.
 * @arg horizontal Whether to calculate a horizontal scrollbar.
 * @arg track_rect Receives the calculated scrollbar track rectangle.
 * @arg thumb_rect Receives the calculated scrollbar thumb rectangle.
 * @return true when both output rectangles are valid. False otherwise.
 */
static bool s_ui_scrollbar_rects(LDKUIRect rect, float visible_size,
    float content_size, float scroll, bool horizontal, LDKUIRect *track_rect,
    LDKUIRect *thumb_rect)
{
  float thickness = 12.0f;
  float max_scroll = s_ui_maxf(0.0f, content_size - visible_size);

  if (track_rect == NULL || thumb_rect == NULL)
  {
    return false;
  }

  *track_rect = (LDKUIRect){0};
  *thumb_rect = (LDKUIRect){0};

  if (horizontal)
  {
    float thumb_w;
    float thumb_range;
    float t = 0.0f;

    track_rect->x = rect.x;
    track_rect->y = rect.y + rect.h - thickness;
    track_rect->w = rect.w;
    track_rect->h = thickness;

    thumb_w = max_scroll > 0.0f ? (visible_size / content_size) * track_rect->w
                                : track_rect->w;
    thumb_w = s_ui_clampf(thumb_w, 16.0f, track_rect->w);

    thumb_range = s_ui_maxf(0.0f, track_rect->w - thumb_w);

    if (max_scroll > 0.0f)
    {
      t = s_ui_clampf(scroll / max_scroll, 0.0f, 1.0f);
    }

    thumb_rect->x = track_rect->x + thumb_range * t;
    thumb_rect->y = track_rect->y;
    thumb_rect->w = thumb_w;
    thumb_rect->h = track_rect->h;
  }
  else
  {
    float thumb_h;
    float thumb_range;
    float t = 0.0f;

    track_rect->x = rect.x + rect.w - thickness;
    track_rect->y = rect.y;
    track_rect->w = thickness;
    track_rect->h = rect.h;

    thumb_h = max_scroll > 0.0f ? (visible_size / content_size) * track_rect->h
                                : track_rect->h;
    thumb_h = s_ui_clampf(thumb_h, 16.0f, track_rect->h);

    thumb_range = s_ui_maxf(0.0f, track_rect->h - thumb_h);

    if (max_scroll > 0.0f)
    {
      t = s_ui_clampf(scroll / max_scroll, 0.0f, 1.0f);
    }

    thumb_rect->x = track_rect->x;
    thumb_rect->y = track_rect->y + thumb_range * t;
    thumb_rect->w = track_rect->w;
    thumb_rect->h = thumb_h;
  }

  return true;
}

typedef struct LDKUIFrameState
{
  LDKUIId id;
  LDKUIRect rect;
  LDKUIRect clip;
  LDKUIPoint cursor;
  LDKUIPoint local_cursor;
  bool hot;
  bool active;
  bool focused;
  bool pressed;
  bool released;
  bool clicked;
  bool dragging;
  bool disabled;
  LDKUIControlVisualState visual_state;
} LDKUIFrameState;

typedef struct LDKUIWidgetBox
{
  LDKUIId id;
  LDKUIRect rect;
  LDKUIRect clip;
  bool disabled;
} LDKUIWidgetBox;

/**
 * Retrieves the clip rectangle of the currently active UI scope.
 * @arg ctx UI context whose clip rectangle will be queried.
 * @return Current clip rectangle, or an empty rectangle when ctx is NULL.
 */
static LDKUIRect s_ui_current_clip_rect(LDKUIContext *ctx)
{
  if (ctx == NULL)
  {
    return (LDKUIRect){0};
  }

  return ctx->clip_rect;
}

/**
 * Finds the topmost window from the current frame that contains a point.
 * @arg ctx UI context that owns the ordered window list.
 * @arg x Horizontal screen coordinate to test.
 * @arg y Vertical screen coordinate to test.
 * @return Identifier of the topmost matching window, or zero when none match.
 */
static LDKUIId s_ui_topmost_window_id_at_point(
    LDKUIContext *ctx, float x, float y)
{
  if (ctx == NULL)
  {
    return 0;
  }

  for (u32 i = x_array_ldk_ui_window_count(ctx->windows); i > 0; --i)
  {
    LDKUIWindow *window = x_array_ldk_ui_window_get(ctx->windows, i - 1);

    if (window == NULL)
    {
      continue;
    }

    if (window->last_frame_seen != ctx->frame_index)
    {
      continue;
    }

    if (s_ui_rect_contains(&window->rect, x, y))
    {
      return window->id;
    }
  }

  return 0;
}

/**
 * Registers an item as a hit-testing candidate for the current frame.
 * @arg ctx UI context that owns the hit candidate list.
 * @arg item_id Identifier of the submitted interactive item.
 * @arg rect Screen-space rectangle occupied by the item.
 * @arg clip_rect Clip rectangle that limits the item's interactive area.
 */
static void s_ui_add_hit_candidate(
    LDKUIContext *ctx, LDKUIId item_id, LDKUIRect rect, LDKUIRect clip_rect)
{
  LDKUIHitCandidate candidate;

  if (ctx == NULL || ctx->hit_candidates == NULL)
  {
    return;
  }

  candidate.layer = s_ui_rendering_popup(ctx) ? LDK_UI_HIT_LAYER_POPUP
                                              : LDK_UI_HIT_LAYER_NORMAL;
  candidate.window_id = candidate.layer == LDK_UI_HIT_LAYER_POPUP ? 0
                        : ctx->current_window != NULL ? ctx->current_window->id
                                                      : 0;
  candidate.item_id = item_id;
  candidate.rect = rect;
  candidate.clip_rect = clip_rect;
  candidate.order = ctx->hit_order;
  ctx->hit_order += 1;

  x_array_ldk_ui_hit_candidate_push(ctx->hit_candidates, candidate);
}

/**
 * Checks whether any popup submitted geometry during the current frame.
 * @arg ctx UI context that owns the current popup frame entries.
 * @return true when at least one popup frame entry exists. False otherwise.
 */
static bool s_ui_has_popup_frame_entries(LDKUIContext *ctx)
{
  if (ctx == NULL || ctx->popup_frame_entries == NULL)
  {
    return false;
  }

  return !x_array_ldk_ui_popup_frame_entry_is_empty(ctx->popup_frame_entries);
}

/**
 * Checks whether the mouse cursor lies inside a popup from the current frame.
 * @arg ctx UI context that owns the mouse and popup frame entries.
 * @return true when the cursor is inside any submitted popup. False otherwise.
 */
static bool s_ui_cursor_inside_any_popup(LDKUIContext *ctx)
{
  if (ctx == NULL || ctx->mouse == NULL || ctx->popup_frame_entries == NULL)
  {
    return false;
  }

  LDKPoint cursor = ldk_os_mouse_cursor((LDKMouseState *)ctx->mouse);

  for (u32 i = x_array_ldk_ui_popup_frame_entry_count(ctx->popup_frame_entries);
      i > 0; --i)
  {
    LDKUIPopupFrameEntry *entry =
        x_array_ldk_ui_popup_frame_entry_get(ctx->popup_frame_entries, i - 1);

    if (entry == NULL)
    {
      continue;
    }

    if (s_ui_rect_contains(&entry->rect, (float)cursor.x, (float)cursor.y))
    {
      return true;
    }
  }

  return false;
}

/**
 * Discards popup geometry, commands, and hit regions produced this frame.
 * @arg ctx UI context whose popup frame data will be cleared.
 */
static void s_ui_clear_popup_frame_draw_data(LDKUIContext *ctx)
{
  if (ctx == NULL)
  {
    return;
  }

  if (ctx->popup_vertices != NULL)
  {
    x_array_ldk_ui_vertex_clear(ctx->popup_vertices);
  }

  if (ctx->popup_indices != NULL)
  {
    x_array_ldk_ui_u32_clear(ctx->popup_indices);
  }

  if (ctx->popup_commands != NULL)
  {
    x_array_ldk_ui_draw_cmd_clear(ctx->popup_commands);
  }

  if (ctx->popup_frame_entries != NULL)
  {
    x_array_ldk_ui_popup_frame_entry_clear(ctx->popup_frame_entries);
  }
}

/**
 * Closes all open popups when a press occurs outside their visible regions.
 * @arg ctx UI context that owns popup and mouse interaction state.
 */
static void s_ui_close_popups_on_outside_click(LDKUIContext *ctx)
{
  if (ctx == NULL || ctx->mouse == NULL || ctx->open_popups == NULL)
  {
    return;
  }

  if (x_array_ldk_ui_id_is_empty(ctx->open_popups))
  {
    return;
  }

  if (!ldk_os_mouse_button_down(
          (LDKMouseState *)ctx->mouse, LDK_MOUSE_BUTTON_LEFT))
  {
    return;
  }

  if (s_ui_cursor_inside_any_popup(ctx))
  {
    return;
  }

  ldk_ui_close_all_popups(ctx);
  s_ui_clear_popup_frame_draw_data(ctx);

  ctx->next_hot_id = 0;
  ctx->hot_id = 0;
  ctx->active_id = 0;
}

/**
 * Resolves the topmost hit candidate under the cursor for the next frame.
 * @arg ctx UI context containing the current frame's hit candidates.
 */
static void s_ui_resolve_hot_item(LDKUIContext *ctx)
{
  LDKPoint cursor;
  LDKUIHitCandidate *best_candidate = NULL;
  LDKUIId top_window_id;
  u32 count;
  bool has_popup_frame_entries;

  if (ctx == NULL || ctx->mouse == NULL)
  {
    ctx->next_hot_id = 0;
    return;
  }

  cursor = ldk_os_mouse_cursor((LDKMouseState *)ctx->mouse);
  has_popup_frame_entries = s_ui_has_popup_frame_entries(ctx);

  if (has_popup_frame_entries)
  {
    if (!s_ui_cursor_inside_any_popup(ctx))
    {
      ctx->next_hot_id = 0;
      return;
    }
  }

  top_window_id =
      s_ui_topmost_window_id_at_point(ctx, (float)cursor.x, (float)cursor.y);
  count = x_array_ldk_ui_hit_candidate_count(ctx->hit_candidates);

  for (u32 i = 0; i < count; ++i)
  {
    LDKUIHitCandidate *candidate =
        x_array_ldk_ui_hit_candidate_get(ctx->hit_candidates, i);

    if (candidate == NULL)
    {
      continue;
    }

    if (has_popup_frame_entries && candidate->layer != LDK_UI_HIT_LAYER_POPUP)
    {
      continue;
    }

    if (candidate->layer != LDK_UI_HIT_LAYER_POPUP)
    {
      if (top_window_id != 0)
      {
        if (candidate->window_id != top_window_id)
        {
          continue;
        }
      }
      else if (candidate->window_id != 0)
      {
        continue;
      }
    }

    if (!s_ui_rect_contains(&candidate->rect, (float)cursor.x, (float)cursor.y))
    {
      continue;
    }

    if (!s_ui_rect_contains(
            &candidate->clip_rect, (float)cursor.x, (float)cursor.y))
    {
      continue;
    }

    if (best_candidate == NULL || candidate->layer > best_candidate->layer ||
        (candidate->layer == best_candidate->layer &&
            candidate->order > best_candidate->order))
    {
      best_candidate = candidate;
    }
  }

  ctx->next_hot_id = best_candidate != NULL ? best_candidate->item_id : 0;
}

/**
 * Consumes the next-item disabled flag and combines it with parent state.
 * @arg ctx UI context that owns next-item and disabled stack state.
 * @return Effective disabled state for the next submitted item.
 */
static bool s_ui_take_next_disabled(LDKUIContext *ctx)
{
  bool disabled = false;

  if (ctx == NULL)
  {
    return false;
  }

  disabled = ctx->next_disabled;
  ctx->next_disabled = false;

  bool const *parent_disabled = x_array_ldk_ui_bool_back(ctx->disabled_stack);

  if (parent_disabled != NULL && *parent_disabled)
  {
    disabled = true;
  }

  return disabled;
}

/**
 * Calculates item interaction and visual state for the current frame.
 * @arg ctx UI context containing mouse, focus, and active item state.
 * @arg id Identifier of the item being evaluated.
 * @arg rect Screen-space rectangle occupied by the item.
 * @arg clip Clip rectangle that limits the item's interactive area.
 * @arg focusable Whether pressing the item can assign keyboard focus.
 * @arg disabled Whether the item must ignore interaction.
 * @return Complete per-frame interaction and visual state for the item.
 */
static LDKUIFrameState s_ui_frame_state(LDKUIContext *ctx, LDKUIId id,
    LDKUIRect rect, LDKUIRect clip, bool focusable, bool disabled)
{
  LDKUIFrameState state = {0};
  bool focus_requested = false;

  state.id = id;
  state.rect = rect;
  state.clip = clip;
  state.disabled = disabled;

  if (ctx == NULL)
  {
    return state;
  }

  focus_requested = ctx->next_focus && focusable;
  if (focus_requested)
  {
    ctx->next_focus = false;
  }

  if (disabled)
  {
    if (ctx->active_id == id)
    {
      ctx->active_id = 0;
    }

    if (ctx->focused_id == id)
    {
      ctx->focused_id = 0;
    }

    if (ctx->input_box_id == id)
    {
      ctx->input_box_id = 0;
    }

    state.visual_state = LDK_UI_CONTROL_VISUAL_STATE_DISABLED;
    return state;
  }

  state.hot = ctx->hot_id == id;
  state.active = ctx->active_id == id;
  state.focused = ctx->focused_id == id;

  if (focus_requested && focusable)
  {
    ctx->focused_id = id;
    if (ctx->current_window != NULL)
    {
      ctx->current_window->focused_id = id;
    }
    state.focused = true;
  }

  if (ctx->mouse != NULL)
  {
    LDKPoint cursor = ldk_os_mouse_cursor((LDKMouseState *)ctx->mouse);

    state.cursor.x = (float)cursor.x;
    state.cursor.y = (float)cursor.y;
    state.local_cursor.x = state.cursor.x - rect.x;
    state.local_cursor.y = state.cursor.y - rect.y;

    state.pressed = ldk_os_mouse_button_down(
        (LDKMouseState *)ctx->mouse, LDK_MOUSE_BUTTON_LEFT);
    state.released = ldk_os_mouse_button_up(
        (LDKMouseState *)ctx->mouse, LDK_MOUSE_BUTTON_LEFT);
    state.dragging = false;

    if (state.active)
    {
      state.dragging = ldk_os_mouse_button_is_pressed(
          (LDKMouseState *)ctx->mouse, LDK_MOUSE_BUTTON_LEFT);
    }
  }

  if (state.hot && state.pressed)
  {
    ctx->active_id = id;

    if (focusable)
    {
      ctx->focused_id = id;
    }

    state.active = true;
    state.focused = focusable;
  }

  state.clicked = state.active && state.hot && state.released;

  if (state.active && state.hot)
  {
    state.visual_state = LDK_UI_CONTROL_VISUAL_STATE_ACTIVE_HOVERED;
  }
  else if (state.active)
  {
    state.visual_state = LDK_UI_CONTROL_VISUAL_STATE_ACTIVE;
  }
  else if (state.hot)
  {
    state.visual_state = LDK_UI_CONTROL_VISUAL_STATE_HOVERED;
  }
  else
  {
    state.visual_state = LDK_UI_CONTROL_VISUAL_STATE_IDLE;
  }

  return state;
}

bool ldk_ui_initialize(LDKUIContext *ctx, LDKUIConfig const *config)
{
  if (ctx == NULL || config == NULL)
  {
    return false;
  }

  memset(ctx, 0, sizeof(*ctx));

  u32 initial_window_capacity = config->initial_window_capacity;

  if (initial_window_capacity < LDK_UI_WINDOW_CAPACITY)
  {
    initial_window_capacity = LDK_UI_WINDOW_CAPACITY;
  }

  ctx->frame_arena = x_arena_create(config->frame_arena_size);
  ctx->id_stack = x_array_ldk_ui_id_create(config->initial_id_stack_capacity);
  ctx->vertices = x_array_ldk_ui_vertex_create(config->initial_vertex_capacity);
  ctx->indices = x_array_ldk_ui_u32_create(config->initial_index_capacity);
  ctx->commands =
      x_array_ldk_ui_draw_cmd_create(config->initial_command_capacity);
  ctx->popup_vertices =
      x_array_ldk_ui_vertex_create(config->initial_vertex_capacity);
  ctx->popup_indices =
      x_array_ldk_ui_u32_create(config->initial_index_capacity);
  ctx->popup_commands =
      x_array_ldk_ui_draw_cmd_create(config->initial_command_capacity);
  ctx->disabled_stack = x_array_ldk_ui_bool_create(8);
  ctx->hit_candidates = x_array_ldk_ui_hit_candidate_create(128);
  ctx->layout_stack =
      x_array_ldk_ui_layout_create(LDK_UI_LAYOUT_STACK_CAPACITY);
  ctx->windows = x_array_ldk_ui_window_create(initial_window_capacity);
  ctx->window_stack =
      x_array_ldk_ui_window_stack_entry_create(LDK_UI_WINDOW_STACK_CAPACITY);
  ctx->measure_entries =
      x_array_ldk_ui_measure_entry_create(LDK_UI_MEASURE_ENTRY_CAPACITY);
  ctx->layout_items =
      x_array_ldk_ui_layout_item_create(LDK_UI_LAYOUT_ITEM_CAPACITY);
  ctx->layout_item_cache = x_array_ldk_ui_layout_item_cache_create(
      LDK_UI_LAYOUT_ITEM_CACHE_CAPACITY);
  ctx->scrollview_stack = x_array_ldk_ui_scrollview_stack_entry_create(
      LDK_UI_SCROLLVIEW_STACK_CAPACITY);
  ctx->scrollview_cache =
      x_array_ldk_ui_scrollview_cache_create(LDK_UI_SCROLLVIEW_CACHE_CAPACITY);
  ctx->area_stack =
      x_array_ldk_ui_area_stack_entry_create(LDK_UI_AREA_STACK_CAPACITY);
  ctx->open_popups = x_array_ldk_ui_id_create(LDK_UI_POPUP_STACK_CAPACITY);
  ctx->popup_stack =
      x_array_ldk_ui_popup_stack_entry_create(LDK_UI_POPUP_STACK_CAPACITY);
  ctx->popup_frame_entries =
      x_array_ldk_ui_popup_frame_entry_create(LDK_UI_POPUP_STACK_CAPACITY);
  ctx->popup_cache =
      x_array_ldk_ui_popup_cache_create(LDK_UI_POPUP_STACK_CAPACITY);

  ctx->font_texture_user = config->font_texture_user;
  ctx->get_font_page_texture = config->get_font_page_texture;
  ctx->font = config->font;

  // We just get theme straight into the actual ctx->theme
  ldk_ui_theme_get(LDK_UI_THEME_DEFAULT_DARK, &ctx->theme);

  if (ctx->font != NULL)
  {
    ldk_ttf_preload_basic_ascii(ctx->font);
  }

  return ctx->frame_arena != NULL && ctx->id_stack != NULL &&
         ctx->vertices != NULL && ctx->indices != NULL &&
         ctx->commands != NULL && ctx->popup_vertices != NULL &&
         ctx->popup_indices != NULL && ctx->popup_commands != NULL &&
         ctx->disabled_stack != NULL && ctx->hit_candidates != NULL &&
         ctx->layout_stack != NULL && ctx->open_popups != NULL &&
         ctx->popup_stack != NULL && ctx->popup_frame_entries != NULL &&
         ctx->popup_cache != NULL && ctx->windows != NULL &&
         ctx->window_stack != NULL && ctx->measure_entries != NULL &&
         ctx->layout_items != NULL && ctx->layout_item_cache != NULL &&
         ctx->scrollview_stack != NULL && ctx->scrollview_cache != NULL &&
         ctx->area_stack != NULL;
}

void ldk_ui_terminate(LDKUIContext *ctx)
{
  if (ctx == NULL)
  {
    return;
  }

  ldk_os_cursor_type_set(LDK_CURSOR_ARROW);

  s_ui_windows_destroy_all(ctx);

  x_array_destroy(ctx->id_stack);
  x_array_destroy(ctx->vertices);
  x_array_destroy(ctx->indices);
  x_array_destroy(ctx->commands);
  x_array_destroy(ctx->popup_vertices);
  x_array_destroy(ctx->popup_indices);
  x_array_destroy(ctx->popup_commands);
  x_array_destroy(ctx->disabled_stack);
  x_array_destroy(ctx->hit_candidates);
  x_array_destroy(ctx->layout_stack);
  x_array_destroy(ctx->open_popups);
  x_array_destroy(ctx->popup_stack);
  x_array_destroy(ctx->popup_frame_entries);
  x_array_destroy(ctx->popup_cache);
  x_array_destroy(ctx->windows);
  x_array_destroy(ctx->window_stack);
  x_array_destroy(ctx->measure_entries);
  x_array_destroy(ctx->layout_items);
  x_array_destroy(ctx->layout_item_cache);
  x_array_destroy(ctx->scrollview_stack);
  x_array_destroy(ctx->scrollview_cache);
  x_array_destroy(ctx->area_stack);
  x_arena_destroy(ctx->frame_arena);

  memset(ctx, 0, sizeof(*ctx));
}

void ldk_ui_begin_frame(LDKUIContext *ctx, float delta,
    LDKMouseState const *mouse, LDKKeyboardState const *keyboard,
    LDKUITextInputState const *text_input, LDKUIRect viewport)
{
  if (ctx == NULL)
  {
    return;
  }

  ctx->delta_time = delta;

  if (ctx->theme.text_cursor_blink &&
      ctx->theme.text_cursor_blink_interval > 0.0f)
  {
    float blink_interval = ctx->theme.text_cursor_blink_interval;
    ctx->text_cursor_blink_timer += delta;

    while (ctx->text_cursor_blink_timer >= blink_interval)
    {
      ctx->text_cursor_blink_timer -= blink_interval;
    }

    ctx->text_cursor_blink_visible =
        ctx->text_cursor_blink_timer < blink_interval * 0.5f;
  }
  else
  {
    ctx->text_cursor_blink_timer = 0.0f;
    ctx->text_cursor_blink_visible = true;
  }

  ctx->frame_index += 1;
  ctx->mouse = mouse;
  ctx->keyboard = keyboard;
  ctx->input_text = text_input;
  ctx->viewport = viewport;
  ctx->clip_rect = viewport;
  ctx->current_window = NULL;
  x_array_ldk_ui_window_stack_entry_clear(ctx->window_stack);
  ctx->next_disabled = false;
  ctx->next_focus = false;
  ctx->hot_id = ctx->next_hot_id;
  ctx->next_hot_id = 0;
  ctx->hit_order = 0;
  ctx->last_rect = (LDKUIRect){0};
  ctx->last_bounding_rect = (LDKUIRect){0};

  x_arena_reset_keep_head(ctx->frame_arena);
  x_array_ldk_ui_id_clear(ctx->id_stack);
  x_array_ldk_ui_bool_clear(ctx->disabled_stack);
  x_array_ldk_ui_vertex_clear(ctx->vertices);
  x_array_ldk_ui_u32_clear(ctx->indices);
  x_array_ldk_ui_draw_cmd_clear(ctx->commands);
  x_array_ldk_ui_vertex_clear(ctx->popup_vertices);
  x_array_ldk_ui_u32_clear(ctx->popup_indices);
  x_array_ldk_ui_draw_cmd_clear(ctx->popup_commands);
  x_array_ldk_ui_hit_candidate_clear(ctx->hit_candidates);
  x_array_ldk_ui_popup_stack_entry_clear(ctx->popup_stack);
  x_array_ldk_ui_popup_frame_entry_clear(ctx->popup_frame_entries);

  s_ui_windows_clear_frame_buffers(ctx);

  ldk_os_cursor_type_set(ctx->cursor_type);
  ctx->cursor_type = LDK_CURSOR_ARROW;
}

/**
 * Appends a complete draw-data set to the context's final render buffers.
 * @arg ctx UI context that owns the final frame buffers.
 * @arg vertices Source vertex buffer to append.
 * @arg indices Source index buffer to append and rebase.
 * @arg commands Source draw commands to append and offset.
 */
static void s_ui_append_draw_data(LDKUIContext *ctx,
    XArray_ldk_ui_vertex *vertices, XArray_ldk_ui_u32 *indices,
    XArray_ldk_ui_draw_cmd *commands)
{
  if (ctx == NULL || vertices == NULL || indices == NULL || commands == NULL)
  {
    return;
  }

  u32 vertex_base = x_array_ldk_ui_vertex_count(ctx->vertices);
  u32 index_base = x_array_ldk_ui_u32_count(ctx->indices);
  u32 vertex_count = x_array_ldk_ui_vertex_count(vertices);
  u32 index_count = x_array_ldk_ui_u32_count(indices);
  u32 command_count = x_array_ldk_ui_draw_cmd_count(commands);

  for (u32 i = 0; i < vertex_count; ++i)
  {
    LDKUIVertex *vertex = x_array_ldk_ui_vertex_get(vertices, i);

    if (vertex != NULL)
    {
      x_array_ldk_ui_vertex_push(ctx->vertices, *vertex);
    }
  }

  for (u32 i = 0; i < index_count; ++i)
  {
    u32 *index = x_array_ldk_ui_u32_get(indices, i);

    if (index != NULL)
    {
      x_array_ldk_ui_u32_push(ctx->indices, *index + vertex_base);
    }
  }

  for (u32 i = 0; i < command_count; ++i)
  {
    LDKUIDrawCmd *cmd = x_array_ldk_ui_draw_cmd_get(commands, i);

    if (cmd != NULL)
    {
      LDKUIDrawCmd adjusted = *cmd;
      adjusted.index_offset += index_base;
      x_array_ldk_ui_draw_cmd_push(ctx->commands, adjusted);
    }
  }
}

/**
 * Appends popup draw data after the regular and window draw data.
 * @arg ctx UI context that owns the popup and final render buffers.
 */
static void s_ui_submit_popup_draw_data(LDKUIContext *ctx)
{
  if (ctx == NULL)
  {
    return;
  }

  s_ui_append_draw_data(
      ctx, ctx->popup_vertices, ctx->popup_indices, ctx->popup_commands);
}

void ldk_ui_end_frame(LDKUIContext *ctx)
{
  if (ctx == NULL)
  {
    return;
  }

  s_ui_resolve_hot_item(ctx);
  s_ui_close_popups_on_outside_click(ctx);

  if (ctx->mouse != NULL)
  {
    if (ldk_os_mouse_button_up(
            (LDKMouseState *)ctx->mouse, LDK_MOUSE_BUTTON_LEFT))
    {
      if (ctx->active_id != 0)
      {
        ctx->active_id = 0;
      }
    }
  }

  s_ui_submit_windows_in_z_order(ctx);
  s_ui_submit_popup_draw_data(ctx);
  s_ui_window_cache_gc(ctx);

  ctx->render_data.vertices = x_array_ldk_ui_vertex_data_const(ctx->vertices);
  ctx->render_data.vertex_count = x_array_ldk_ui_vertex_count(ctx->vertices);
  ctx->render_data.indices = x_array_ldk_ui_u32_data_const(ctx->indices);
  ctx->render_data.index_count = x_array_ldk_ui_u32_count(ctx->indices);
  ctx->render_data.commands = x_array_ldk_ui_draw_cmd_data_const(ctx->commands);
  ctx->render_data.command_count = x_array_ldk_ui_draw_cmd_count(ctx->commands);
}

LDKUIRenderData const *ldk_ui_get_render_data(LDKUIContext const *ctx)
{
  if (ctx == NULL)
  {
    return NULL;
  }

  return &ctx->render_data;
}

bool ldk_ui_theme_set(LDKUIContext *ctx, LDKUITheme *theme)
{
  if (ctx == NULL || theme == NULL)
  {
    return false;
  }

  ctx->theme = *theme;
  return true;
}

bool ldk_ui_theme_get(LDKUIThemeType type, LDKUITheme *theme)
{
  rgba32 text;
  rgba32 text_disabled;
  rgba32 window_bg;
  rgba32 panel_bg;

  rgba32 control_bg;
  rgba32 control_bg_hovered;
  rgba32 control_bg_active;
  rgba32 control_bg_active_hovered;
  rgba32 control_text;
  rgba32 control_border;
  rgba32 control_border_hovered;
  rgba32 control_border_active;
  rgba32 control_border_disabled;

  rgba32 border;
  rgba32 focus;
  rgba32 separator;

  rgba32 slider_track;
  rgba32 slider_thumb;
  rgba32 slider_thumb_hovered;

  rgba32 title;
  rgba32 title_bar;
  rgba32 title_bar_focused;

  rgba32 scrollbar_track;
  rgba32 scrollbar_thumb;
  rgba32 scrollbar_thumb_hovered;

  rgba32 tab_bar_bg;
  rgba32 tab_bg;
  rgba32 tab_bg_hovered;
  rgba32 tab_text;
  rgba32 tab_active_bg;

  if (theme == NULL)
  {
    return false;
  }

  memset(theme, 0, sizeof(LDKUITheme));

  if (type == LDK_UI_THEME_DEFAULT_DARK)
  {
    text = 0xFFFFFFFFu;
    text_disabled = 0x707070FFu;

    window_bg = 0x383838FFu;
    panel_bg = 0x383838FFu;

    control_bg = 0x515151FFu;
    control_bg_hovered = 0x585858FFu;
    control_bg_active = 0x46607CFFu;
    control_bg_active_hovered = 0x4F657FFFu;

    control_text = 0xFFFFFFFFu;
    control_border = 0x303030FFu;
    control_border_hovered = 0x656565FFu;
    control_border_active = 0x0D0D0DFFu;
    control_border_disabled = 0x333333FFu;

    border = 0x242424FFu;
    focus = 0x2C5D87FFu;
    separator = 0x232323FFu;

    slider_track = 0x5E5E5EFFu;
    slider_thumb = 0x999999FFu;
    slider_thumb_hovered = 0xEAEAEAFFu;

    title = 0xBDBDBDFFu;
    title_bar = 0x353535FFu;
    title_bar_focused = 0x3C3C3CFFu;

    scrollbar_track = 0x0000000Du;
    scrollbar_thumb = 0x5F5F5FFFu;
    scrollbar_thumb_hovered = 0x686868FFu;

    tab_bar_bg = 0x3C3C3CFFu;
    tab_bg = 0x353535FFu;
    tab_bg_hovered = 0x303030FFu;
    tab_text = 0xBDBDBDFFu;
    tab_active_bg = 0x3C3C3CFFu;
  }
  else if (type == LDK_UI_THEME_DEFAULT_LIGHT)
  {
    text = 0x090909FFu;
    text_disabled = 0x707070FFu;

    window_bg = 0xC8C8C8FFu;
    panel_bg = 0xC8C8C8FFu;

    control_bg = 0xDFDFDFFFu;
    control_bg_hovered = 0xE4E4E4FFu;
    control_bg_active = 0x96C3FBFFu;
    control_bg_active_hovered = 0xB0D2FCFFu;

    control_text = 0x090909FFu;
    control_border = 0xB2B2B2FFu;
    control_border_hovered = 0x6C6C6CFFu;
    control_border_active = 0x707070FFu;
    control_border_disabled = 0xBDBDBDFFu;

    border = 0x939393FFu;
    focus = 0x3A72B0FFu;
    separator = 0x999999FFu;

    slider_track = 0x8F8F8FFFu;
    slider_thumb = 0x616161FFu;
    slider_thumb_hovered = 0x4F4F4FFFu;

    title = 0x090909FFu;
    title_bar = 0xB6B6B6FFu;
    title_bar_focused = 0xCBCBCBFFu;

    scrollbar_track = 0x0000000Du;
    scrollbar_thumb = 0x9A9A9AFFu;
    scrollbar_thumb_hovered = 0x8E8E8EFFu;

    tab_bar_bg = 0xCBCBCBFFu;
    tab_bg = 0xB6B6B6FFu;
    tab_bg_hovered = 0xB0B0B0FFu;
    tab_text = 0x090909FFu;
    tab_active_bg = 0xCBCBCBFFu;
  }
  else
  {
    return false;
  }

  theme->colors[LDK_UI_COLOR_TEXT] = text;
  theme->colors[LDK_UI_COLOR_TEXT_DISABLED] = text_disabled;

  theme->colors[LDK_UI_COLOR_WINDOW_BG] = window_bg;
  theme->colors[LDK_UI_COLOR_PANEL_BG] = panel_bg;

  theme->colors[LDK_UI_COLOR_CONTROL_BG] = control_bg;
  theme->colors[LDK_UI_COLOR_CONTROL_BG_HOVERED] = control_bg_hovered;
  theme->colors[LDK_UI_COLOR_CONTROL_BG_ACTIVE] = control_bg_active;
  theme->colors[LDK_UI_COLOR_CONTROL_BG_ACTIVE_HOVERED] =
      control_bg_active_hovered;

  theme->colors[LDK_UI_COLOR_CONTROL_TEXT] = control_text;
  theme->colors[LDK_UI_COLOR_CONTROL_TEXT_HOVERED] = control_text;
  theme->colors[LDK_UI_COLOR_CONTROL_TEXT_ACTIVE] = control_text;
  theme->colors[LDK_UI_COLOR_CONTROL_TEXT_ACTIVE_HOVERED] = control_text;
  theme->colors[LDK_UI_COLOR_CONTROL_TEXT_DISABLED] = text_disabled;

  theme->colors[LDK_UI_COLOR_CONTROL_BORDER] = control_border;
  theme->colors[LDK_UI_COLOR_CONTROL_BORDER_HOVERED] = control_border_hovered;
  theme->colors[LDK_UI_COLOR_CONTROL_BORDER_ACTIVE] = control_border_active;
  theme->colors[LDK_UI_COLOR_CONTROL_BORDER_ACTIVE_HOVERED] =
      control_border_active;
  theme->colors[LDK_UI_COLOR_CONTROL_BORDER_DISABLED] = control_border_disabled;

  theme->colors[LDK_UI_COLOR_BORDER] = border;
  theme->colors[LDK_UI_COLOR_FOCUS] = focus;
  theme->colors[LDK_UI_COLOR_SEPARATOR] = separator;

  theme->colors[LDK_UI_COLOR_SLIDER_TRACK] = slider_track;
  theme->colors[LDK_UI_COLOR_SLIDER_TRACK_HOVERED] = slider_track;
  theme->colors[LDK_UI_COLOR_SLIDER_TRACK_ACTIVE] = slider_track;
  theme->colors[LDK_UI_COLOR_SLIDER_FILL] = slider_track;
  theme->colors[LDK_UI_COLOR_SLIDER_THUMB] = slider_thumb;
  theme->colors[LDK_UI_COLOR_SLIDER_THUMB_HOVERED] = slider_thumb_hovered;
  theme->colors[LDK_UI_COLOR_SLIDER_THUMB_ACTIVE] = slider_thumb_hovered;

  theme->colors[LDK_UI_COLOR_TITLE] = title;
  theme->colors[LDK_UI_COLOR_TITLE_BAR] = title_bar;
  theme->colors[LDK_UI_COLOR_TITLE_BAR_FOCUSED] = title_bar_focused;

  theme->colors[LDK_UI_COLOR_SCROLLBAR_TRACK] = scrollbar_track;
  theme->colors[LDK_UI_COLOR_SCROLLBAR_THUMB] = scrollbar_thumb;
  theme->colors[LDK_UI_COLOR_SCROLLBAR_THUMB_HOVERED] = scrollbar_thumb_hovered;
  theme->colors[LDK_UI_COLOR_SCROLLBAR_THUMB_ACTIVE] = scrollbar_thumb_hovered;

  theme->colors[LDK_UI_COLOR_TAB_BAR_BG] = tab_bar_bg;
  theme->colors[LDK_UI_COLOR_TAB_BAR_SEPARATOR] = separator;
  theme->colors[LDK_UI_COLOR_TAB_BG] = tab_bg;
  theme->colors[LDK_UI_COLOR_TAB_BG_HOVERED] = tab_bg_hovered;
  theme->colors[LDK_UI_COLOR_TAB_TEXT] = tab_text;
  theme->colors[LDK_UI_COLOR_TAB_TEXT_HOVERED] = tab_text;
  theme->colors[LDK_UI_COLOR_TAB_BORDER] = border;
  theme->colors[LDK_UI_COLOR_TAB_BORDER_HOVERED] = border;
  theme->colors[LDK_UI_COLOR_TAB_ACTIVE_BG] = tab_active_bg;
  theme->colors[LDK_UI_COLOR_TAB_ACTIVE_TEXT] = tab_text;
  theme->colors[LDK_UI_COLOR_TAB_ACTIVE_BORDER] = border;

  theme->control_border_size = 1.0f;
  theme->window_border_size = 1.0f;
  theme->window_interaction_border_size = 4.0f;

  theme->slider_track_height = 0.27272728f;
  theme->slider_thumb_width = 0.63636363f;

  theme->text_cursor_blink = true;
  theme->text_cursor_blink_interval = 1.0f;
  theme->text_cursor_width = 2.0f;
  theme->text_cursor_padding_y = 4.0f;

  return true;
}

void ldk_ui_push_id_u32(LDKUIContext *ctx, u32 value)
{
  if (ctx != NULL)
  {
    x_array_ldk_ui_id_push(ctx->id_stack, value);
  }
}

void ldk_ui_push_id_ptr(LDKUIContext *ctx, void const *value)
{
  uintptr_t raw = (uintptr_t)value;
  LDKUIId hashed = (LDKUIId)(raw ^ (raw >> 32));

  if (ctx != NULL)
  {
    x_array_ldk_ui_id_push(ctx->id_stack, hashed);
  }
}

void ldk_ui_push_id_cstr(LDKUIContext *ctx, char const *value)
{
  if (ctx != NULL)
  {
    x_array_ldk_ui_id_push(
        ctx->id_stack, s_ui_id_hash_cstr(2166136261u, value));
  }
}

void ldk_ui_pop_id(LDKUIContext *ctx)
{
  if (ctx != NULL && !x_array_ldk_ui_id_is_empty(ctx->id_stack))
  {
    x_array_ldk_ui_id_pop(ctx->id_stack);
  }
}

void ldk_ui_set_next_disabled(LDKUIContext *ctx, bool disabled)
{
  if (ctx == NULL)
  {
    return;
  }

  ctx->next_disabled = disabled;
}

void ldk_ui_set_next_focus(LDKUIContext *ctx)
{
  if (ctx == NULL)
  {
    return;
  }

  ctx->next_focus = true;
}

void ldk_ui_begin_disabled(LDKUIContext *ctx, bool disabled)
{
  if (ctx == NULL)
  {
    return;
  }

  bool effective_disabled = disabled;
  bool const *parent_disabled = x_array_ldk_ui_bool_back(ctx->disabled_stack);

  if (parent_disabled != NULL && *parent_disabled)
  {
    effective_disabled = true;
  }

  x_array_ldk_ui_bool_push(ctx->disabled_stack, effective_disabled);
}

void ldk_ui_end_disabled(LDKUIContext *ctx)
{
  if (ctx == NULL)
  {
    return;
  }

  u32 count = x_array_ldk_ui_bool_count(ctx->disabled_stack);

  if (count == 0)
  {
    return;
  }

  x_array_ldk_ui_bool_delete_at(ctx->disabled_stack, count - 1);
}

LDKUIRect ldk_ui_rect(float x, float y, float w, float h)
{
  LDKUIRect rect;

  rect.x = x;
  rect.y = y;
  rect.w = w;
  rect.h = h;

  return rect;
}

/**
 * Prepares common box state for a widget with an explicit rectangle.
 * @arg ctx UI context that owns clipping, disabled, and hit-testing state.
 * @arg box Receives the prepared widget identifier, rectangle, and clip state.
 * @arg id Identifier assigned to the widget.
 * @arg rect Screen-space rectangle occupied by the widget.
 * @arg hit_test Whether the widget must participate in hit testing.
 * @return true when the widget box was prepared successfully. False otherwise.
 */
static bool s_ui_widget_box_from_explicit_rect(LDKUIContext *ctx,
    LDKUIWidgetBox *box, LDKUIId id, LDKUIRect rect, bool hit_test)
{
  LDKUIRect parent_clip;

  if (ctx == NULL || box == NULL)
  {
    return false;
  }

  if (hit_test && id == 0)
  {
    return false;
  }

  memset(box, 0, sizeof(*box));

  box->id = id;
  box->rect = rect;

  parent_clip = s_ui_current_clip_rect(ctx);
  box->clip = s_ui_rect_intersect(&parent_clip, &box->rect);
  box->disabled = s_ui_take_next_disabled(ctx);

  ctx->last_id = id;
  ctx->last_rect = rect;
  ctx->last_bounding_rect = rect;

  if (hit_test && !box->disabled)
  {
    s_ui_add_hit_candidate(ctx, box->id, box->rect, box->clip);
  }

  return true;
}

#include "ui/ldk_ui_layout.inl"
#include "ui/ldk_ui_area.inl"
#include "ui/ldk_ui_widgets.inl"
#include "ui/ldk_ui_window.inl"
#include "ui/ldk_ui_scrollview.inl"
#include "ui/ldk_ui_treenode.inl"
#include "ui/ldk_ui_popup.inl"
