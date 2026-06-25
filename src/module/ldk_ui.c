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

static LDKUIId s_ui_id_hash_u32(LDKUIId hash, u32 value)
{
  hash ^= value + 0x9e3779b9u + (hash << 6) + (hash >> 2);
  return hash;
}

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

static bool s_ui_text_codepoint_is_word_space(u32 codepoint)
{
  return codepoint == ' ' || codepoint == '\t' || codepoint == '\r';
}

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

static u32 s_ui_input_box_cursor_from_x(
    LDKUIContext *ctx, char const *text, LDKUIRect rect, float x)
{
  float padding_x = 6.0f;
  float local_x = x - rect.x - padding_x;
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
    // float cursor_x = ldk_ttf_measure_text_cstrn(ctx->font, text, cursor);
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

static bool s_ui_input_keyboard_shift_pressed(LDKUIContext *ctx)
{
  if (ctx->keyboard == NULL)
  {
    return false;
  }

  return ldk_os_keyboard_key_is_pressed(
      (LDKKeyboardState *)ctx->keyboard, LDK_KEYCODE_SHIFT);
}

static bool s_ui_input_keyboard_backspace_pressed(LDKUIContext *ctx)
{
  if (ctx->keyboard == NULL)
  {
    return false;
  }

  return ldk_os_keyboard_key_down(
      (LDKKeyboardState *)ctx->keyboard, LDK_KEYCODE_BACKSPACE);
}

static bool s_ui_input_keyboard_delete_pressed(LDKUIContext *ctx)
{
  if (ctx->keyboard == NULL)
  {
    return false;
  }

  return ldk_os_keyboard_key_down(
      (LDKKeyboardState *)ctx->keyboard, LDK_KEYCODE_DELETE);
}

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

static bool s_ui_input_keyboard_home_pressed(LDKUIContext *ctx)
{
  if (ctx->keyboard == NULL)
  {
    return false;
  }

  return ldk_os_keyboard_key_down(
      (LDKKeyboardState *)ctx->keyboard, LDK_KEYCODE_HOME);
}

static bool s_ui_input_keyboard_end_pressed(LDKUIContext *ctx)
{
  if (ctx->keyboard == NULL)
  {
    return false;
  }

  return ldk_os_keyboard_key_down(
      (LDKKeyboardState *)ctx->keyboard, LDK_KEYCODE_END);
}

static bool s_ui_input_keyboard_accept_pressed(LDKUIContext *ctx)
{
  if (ctx->keyboard == NULL)
  {
    return false;
  }

  if (ldk_os_keyboard_key_down(
          (LDKKeyboardState *)ctx->keyboard, LDK_KEYCODE_ENTER))
  {
    return true;
  }

  //if (ldk_os_keyboard_key_down(
  //        (LDKKeyboardState *)ctx->keyboard, LDK_KEYCODE_SPACE))
  //{
  //  return true;
  //}

  return false;
}

static bool s_ui_input_keyboard_left_pressed(LDKUIContext *ctx)
{
  if (ctx->keyboard == NULL)
  {
    return false;
  }

  return ldk_os_keyboard_key_down(
      (LDKKeyboardState *)ctx->keyboard, LDK_KEYCODE_LEFT);
}

static bool s_ui_input_keyboard_right_pressed(LDKUIContext *ctx)
{
  if (ctx->keyboard == NULL)
  {
    return false;
  }

  return ldk_os_keyboard_key_down(
      (LDKKeyboardState *)ctx->keyboard, LDK_KEYCODE_RIGHT);
}

static void s_ui_input_cursor_blink_reset(LDKUIContext *ctx)
{
  if (ctx == NULL)
  {
    return;
  }

  ctx->text_cursor_blink_timer = 0.0f;
  ctx->text_cursor_blink_visible = true;
}

static bool s_ui_rendering_popup(LDKUIContext *ctx)
{
  if (ctx == NULL || ctx->popup_stack == NULL)
  {
    return false;
  }

  return !x_array_ldk_ui_popup_stack_entry_is_empty(ctx->popup_stack);
}

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

static void s_ui_render_quad(LDKUIContext *ctx, LDKUIRect rect, u32 color,
    LDKUIRect clip_rect, LDKUITextureHandle texture)
{
  LDKUIRect uv = {0.0f, 0.0f, 1.0f, 1.0f};
  s_ui_render_quad_uv(ctx, rect, uv, color, clip_rect, texture);
}

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
  // text_size = ldk_ttf_measure_text_cstrn(ctx->font, text, start);
  float x1 = text_x + text_size.w;

  LDKUIRect highlight_rect;
  highlight_rect.x = x0;
  highlight_rect.y = rect.y + 3.0f;
  highlight_rect.w = x1 - x0;
  highlight_rect.h = rect.h - 6.0f;

  s_ui_render_quad(ctx, highlight_rect, color, clip, 0);
}

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

static float s_ui_slider_value_from_cursor(LDKUIRect rect, float thumb_width,
    float cursor_x, float min_value, float max_value)
{
  float usable_width = s_ui_maxf(rect.w - thumb_width, 1.0f);
  float local_x = cursor_x - rect.x - thumb_width * 0.5f;
  float t = s_ui_clampf(local_x / usable_width, 0.0f, 1.0f);

  return min_value + (max_value - min_value) * t;
}

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

static LDKUIRect s_ui_current_clip_rect(LDKUIContext *ctx)
{
  if (ctx == NULL)
  {
    return (LDKUIRect){0};
  }

  return ctx->clip_rect;
}

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

static bool s_ui_has_popup_frame_entries(LDKUIContext *ctx)
{
  if (ctx == NULL || ctx->popup_frame_entries == NULL)
  {
    return false;
  }

  return !x_array_ldk_ui_popup_frame_entry_is_empty(ctx->popup_frame_entries);
}

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

static LDKUIFrameState s_ui_frame_state(LDKUIContext *ctx, LDKUIId id,
    LDKUIRect rect, LDKUIRect clip, bool focusable, bool disabled)
{
  LDKUIFrameState state = {0};

  state.id = id;
  state.rect = rect;
  state.clip = clip;
  state.disabled = disabled;

  if (ctx == NULL)
  {
    return state;
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

  ctx->font_texture_user = config->font_texture_user;
  ctx->get_font_page_texture = config->get_font_page_texture;
  ctx->font = config->font;

  ldk_ui_set_theme(ctx, config->theme, NULL);

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
         ctx->windows != NULL && ctx->window_stack != NULL &&
         ctx->measure_entries != NULL && ctx->layout_items != NULL &&
         ctx->layout_item_cache != NULL && ctx->scrollview_stack != NULL &&
         ctx->scrollview_cache != NULL && ctx->area_stack != NULL;
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

void ldk_ui_set_theme(
    LDKUIContext *ctx, LDKUIThemeType type, LDKUITheme *custom)
{
  LDKUITheme *theme;

  if (ctx == NULL)
  {
    return;
  }

  theme = &ctx->theme;

  if (type == LDK_UI_THEME_CUSTOM && custom != NULL)
  {
    memcpy(&ctx->theme, custom, sizeof(*custom));
    return;
  }

  if (type == LDK_UI_THEME_DEFAULT_DARK)
  {
    rgba32 text = 0xd8d8d8FFu;
    rgba32 text_disabled = 0x707070FFu;
    rgba32 bg = 0x333333EEu;
    rgba32 panel = 0x252525EEu;
    rgba32 control = 0x444444FFu;
    rgba32 hover = 0x505050FFu;
    rgba32 active = 0x38b8a4FFu;
    rgba32 active_hover = 0x43c8b3FFu;
    rgba32 border = 0x1c1c1cFFu;
    rgba32 accent = 0x38b8a4FFu;
    rgba32 title = 0x3f3f3fFFu;
    rgba32 title_focus = 0x2a2a2aFFu;

    theme->colors[LDK_UI_COLOR_TEXT] = text;
    theme->colors[LDK_UI_COLOR_TEXT_DISABLED] = text_disabled;
    theme->colors[LDK_UI_COLOR_WINDOW_BG] = bg;
    theme->colors[LDK_UI_COLOR_PANEL_BG] = panel;
    theme->colors[LDK_UI_COLOR_CONTROL_BG] = control;
    theme->colors[LDK_UI_COLOR_CONTROL_BG_HOVERED] = hover;
    theme->colors[LDK_UI_COLOR_CONTROL_BG_ACTIVE] = active;
    theme->colors[LDK_UI_COLOR_CONTROL_BG_ACTIVE_HOVERED] = active_hover;
    theme->colors[LDK_UI_COLOR_CONTROL_TEXT] = text;
    theme->colors[LDK_UI_COLOR_CONTROL_TEXT_HOVERED] = text;
    theme->colors[LDK_UI_COLOR_CONTROL_TEXT_ACTIVE] = text;
    theme->colors[LDK_UI_COLOR_CONTROL_TEXT_ACTIVE_HOVERED] = text;
    theme->colors[LDK_UI_COLOR_CONTROL_TEXT_DISABLED] = text_disabled;
    theme->colors[LDK_UI_COLOR_CONTROL_BORDER] = border;
    theme->colors[LDK_UI_COLOR_CONTROL_BORDER_HOVERED] = border;
    theme->colors[LDK_UI_COLOR_CONTROL_BORDER_ACTIVE] = accent;
    theme->colors[LDK_UI_COLOR_CONTROL_BORDER_ACTIVE_HOVERED] = accent;
    theme->colors[LDK_UI_COLOR_CONTROL_BORDER_DISABLED] = text_disabled;
    theme->colors[LDK_UI_COLOR_BORDER] = border;
    theme->colors[LDK_UI_COLOR_FOCUS] = accent;
    theme->colors[LDK_UI_COLOR_TITLE_BAR] = title;
    theme->colors[LDK_UI_COLOR_TITLE_BAR_FOCUSED] = title_focus;
    theme->colors[LDK_UI_COLOR_SEPARATOR] = hover;
    theme->colors[LDK_UI_COLOR_SLIDER_TRACK] = control;
    theme->colors[LDK_UI_COLOR_SLIDER_TRACK_HOVERED] = hover;
    theme->colors[LDK_UI_COLOR_SLIDER_TRACK_ACTIVE] = hover;
    theme->colors[LDK_UI_COLOR_SLIDER_FILL] = accent;
    theme->colors[LDK_UI_COLOR_SLIDER_THUMB] = text;
    theme->colors[LDK_UI_COLOR_SLIDER_THUMB_HOVERED] = text;
    theme->colors[LDK_UI_COLOR_SLIDER_THUMB_ACTIVE] = active_hover;
    theme->colors[LDK_UI_COLOR_SCROLLBAR_TRACK] = control;
    theme->colors[LDK_UI_COLOR_SCROLLBAR_THUMB] = hover;
    theme->colors[LDK_UI_COLOR_SCROLLBAR_THUMB_HOVERED] = text;
    theme->colors[LDK_UI_COLOR_SCROLLBAR_THUMB_ACTIVE] = active_hover;
  }
  else
  {
    rgba32 text = 0x202020ffu;
    rgba32 text_disabled = 0xa0a0a0ffu;
    rgba32 bg = 0xf0f0f0ffu;
    rgba32 panel = 0xe0e0e0ffu;
    rgba32 control = 0xd0d0d0ffu;
    rgba32 hover = 0xc0c0c0ffu;
    rgba32 active = 0xb0b0b0ffu;
    rgba32 active_hover = 0xb4b4b4ffu;
    rgba32 border = 0xa0a0a0ffu;
    rgba32 accent = 0x4f8cc9ffu;
    rgba32 title = 0xdcdcdcffu;
    rgba32 title_focus = 0xbfcfffffu;

    theme->colors[LDK_UI_COLOR_TEXT] = text;
    theme->colors[LDK_UI_COLOR_TEXT_DISABLED] = text_disabled;
    theme->colors[LDK_UI_COLOR_WINDOW_BG] = bg;
    theme->colors[LDK_UI_COLOR_PANEL_BG] = panel;
    theme->colors[LDK_UI_COLOR_CONTROL_BG] = control;
    theme->colors[LDK_UI_COLOR_CONTROL_BG_HOVERED] = hover;
    theme->colors[LDK_UI_COLOR_CONTROL_BG_ACTIVE] = active;
    theme->colors[LDK_UI_COLOR_CONTROL_BG_ACTIVE_HOVERED] = active_hover;
    theme->colors[LDK_UI_COLOR_CONTROL_TEXT] = text;
    theme->colors[LDK_UI_COLOR_CONTROL_TEXT_HOVERED] = text;
    theme->colors[LDK_UI_COLOR_CONTROL_TEXT_ACTIVE] = text;
    theme->colors[LDK_UI_COLOR_CONTROL_TEXT_ACTIVE_HOVERED] = text;
    theme->colors[LDK_UI_COLOR_CONTROL_TEXT_DISABLED] = text_disabled;
    theme->colors[LDK_UI_COLOR_CONTROL_BORDER] = border;
    theme->colors[LDK_UI_COLOR_CONTROL_BORDER_HOVERED] = border;
    theme->colors[LDK_UI_COLOR_CONTROL_BORDER_ACTIVE] = accent;
    theme->colors[LDK_UI_COLOR_CONTROL_BORDER_ACTIVE_HOVERED] = accent;
    theme->colors[LDK_UI_COLOR_CONTROL_BORDER_DISABLED] = text_disabled;
    theme->colors[LDK_UI_COLOR_BORDER] = border;
    theme->colors[LDK_UI_COLOR_FOCUS] = accent;
    theme->colors[LDK_UI_COLOR_TITLE_BAR] = title;
    theme->colors[LDK_UI_COLOR_TITLE_BAR_FOCUSED] = title_focus;
    theme->colors[LDK_UI_COLOR_SEPARATOR] = hover;
    theme->colors[LDK_UI_COLOR_SLIDER_TRACK] = control;
    theme->colors[LDK_UI_COLOR_SLIDER_TRACK_HOVERED] = hover;
    theme->colors[LDK_UI_COLOR_SLIDER_TRACK_ACTIVE] = hover;
    theme->colors[LDK_UI_COLOR_SLIDER_FILL] = accent;
    theme->colors[LDK_UI_COLOR_SLIDER_THUMB] = text;
    theme->colors[LDK_UI_COLOR_SLIDER_THUMB_HOVERED] = text;
    theme->colors[LDK_UI_COLOR_SLIDER_THUMB_ACTIVE] = active_hover;
    theme->colors[LDK_UI_COLOR_SCROLLBAR_TRACK] = control;
    theme->colors[LDK_UI_COLOR_SCROLLBAR_THUMB] = hover;
    theme->colors[LDK_UI_COLOR_SCROLLBAR_THUMB_HOVERED] = text;
    theme->colors[LDK_UI_COLOR_SCROLLBAR_THUMB_ACTIVE] = active_hover;
  }

  theme->control_border_size = 1.0f;
  theme->window_border_size = 3.0f;
  theme->window_interaction_border_size = 4.0f;
  theme->slider_track_height = 0.27272728f;
  theme->slider_thumb_width = 0.63636363f;
  theme->text_cursor_blink = true;
  theme->text_cursor_blink_interval = 1.0f;
  theme->text_cursor_width = 1.0f;
  theme->text_cursor_padding_y = 4.0f;
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

static LDKUISize s_ui_widget_text_size(LDKUIContext *ctx, char const *text)
{
  LDKUISize size = {0};

  if (ctx == NULL || ctx->font == NULL || text == NULL)
  {
    return size;
  }

  size = ldk_ttf_measure_text_cstr(ctx->font, text);

  return size;
}

void ldk_ui_widget_panel(LDKUIContext *ctx, LDKUIId id, LDKUIRect rect)
{
  LDKUIWidgetBox box = {0};
  u32 bg;
  u32 border;

  if (!s_ui_widget_box_from_explicit_rect(ctx, &box, id, rect, false))
  {
    return;
  }

  bg = ctx->theme.colors[LDK_UI_COLOR_PANEL_BG];
  border = ctx->theme.colors[LDK_UI_COLOR_BORDER];

  s_ui_render_quad(ctx, box.rect, bg, box.clip, 0);
  s_ui_render_border(
      ctx, box.rect, ctx->theme.control_border_size, border, box.clip);
}

void ldk_ui_widget_label(
    LDKUIContext *ctx, LDKUIId id, char const *text, LDKUIRect rect)
{
  LDKUIWidgetBox box = {0};
  LDKUISize text_size;
  float text_y;

  if (text == NULL)
  {
    text = "";
  }

  if (!s_ui_widget_box_from_explicit_rect(ctx, &box, id, rect, false))
  {
    return;
  }

  text_size = s_ui_widget_text_size(ctx, text);
  text_y = box.rect.y + (box.rect.h - text_size.h) * 0.5f;

  s_ui_render_text_wrapped(ctx, text, box.rect.x, text_y, box.rect.w,
      ctx->theme.colors[LDK_UI_COLOR_TEXT], box.clip);
}

void ldk_ui_widget_image(LDKUIContext *ctx, LDKUIId id,
    LDKUITextureHandle texture, LDKUIRect uv, LDKUIRect rect)
{
  LDKUIWidgetBox box = {0};

  if (!s_ui_widget_box_from_explicit_rect(ctx, &box, id, rect, false))
  {
    return;
  }

  s_ui_render_quad_uv(ctx, box.rect, uv, 0xffffffffu, box.clip, texture);
}

bool ldk_ui_widget_button(
    LDKUIContext *ctx, LDKUIId id, char const *text, LDKUIRect rect)
{
  LDKUIWidgetBox box = {0};
  LDKUIFrameState frame;
  LDKUISize text_size;
  u32 bg;
  u32 border;
  u32 text_color;
  float text_x;
  float text_y;

  if (text == NULL)
  {
    text = "";
  }

  if (!s_ui_widget_box_from_explicit_rect(ctx, &box, id, rect, true))
  {
    return false;
  }

  frame = s_ui_frame_state(ctx, box.id, box.rect, box.clip, true, box.disabled);
  text_size = s_ui_widget_text_size(ctx, text);

  bg = s_ui_render_control_bg_color(ctx, frame.visual_state);
  border = s_ui_render_control_border_color(ctx, frame.visual_state);
  text_color = s_ui_render_control_text_color(ctx, frame.visual_state);

  s_ui_render_quad(ctx, box.rect, bg, box.clip, 0);
  s_ui_render_border(
      ctx, box.rect, ctx->theme.control_border_size, border, box.clip);

  text_x = box.rect.x + (box.rect.w - text_size.w) * 0.5f;
  text_y = box.rect.y + (box.rect.h - text_size.h) * 0.5f;

  s_ui_render_text(ctx, text, text_x, text_y, text_color, box.clip);

  return frame.clicked;
}

bool ldk_ui_widget_button_flat(
    LDKUIContext *ctx, LDKUIId id, char const *text, LDKUIRect rect)
{
  LDKUIWidgetBox box = {0};
  LDKUIFrameState frame;
  LDKUISize text_size;
  u32 text_color;
  float text_x;
  float text_y;

  if (text == NULL)
  {
    text = "";
  }

  if (!s_ui_widget_box_from_explicit_rect(ctx, &box, id, rect, true))
  {
    return false;
  }

  frame = s_ui_frame_state(ctx, box.id, box.rect, box.clip, true, box.disabled);
  text_size = s_ui_widget_text_size(ctx, text);
  text_color = s_ui_render_control_text_color(ctx, frame.visual_state);

  if (frame.visual_state == LDK_UI_CONTROL_VISUAL_STATE_HOVERED ||
      frame.visual_state == LDK_UI_CONTROL_VISUAL_STATE_ACTIVE ||
      frame.visual_state == LDK_UI_CONTROL_VISUAL_STATE_ACTIVE_HOVERED)
  {
    u32 bg = s_ui_render_control_bg_color(ctx, frame.visual_state);
    s_ui_render_quad(ctx, box.rect, bg, box.clip, 0);
  }

  text_x = box.rect.x + LDK_UI_DEFAULT_SPACING;
  text_y = box.rect.y + (box.rect.h - text_size.h) * 0.5f;

  s_ui_render_text(ctx, text, text_x, text_y, text_color, box.clip);

  return frame.clicked;
}

bool ldk_ui_widget_toggle(
    LDKUIContext *ctx, LDKUIId id, bool value, LDKUIRect rect)
{
  LDKUIWidgetBox box = {0};
  LDKUIFrameState frame;
  u32 bg;
  u32 border;
  float check_size;
  LDKUIRect check_rect;

  if (!s_ui_widget_box_from_explicit_rect(ctx, &box, id, rect, true))
  {
    return value;
  }

  frame = s_ui_frame_state(ctx, box.id, box.rect, box.clip, true, box.disabled);

  if (frame.clicked)
  {
    value = !value;
  }

  bg = s_ui_render_control_bg_color(ctx, frame.visual_state);
  border = s_ui_render_control_border_color(ctx, frame.visual_state);

  s_ui_render_quad(ctx, box.rect, bg, box.clip, 0);
  s_ui_render_border(
      ctx, box.rect, ctx->theme.control_border_size, border, box.clip);

  if (value)
  {
    check_size = s_ui_maxf(0.0f, s_ui_minf(box.rect.w, box.rect.h) - 8.0f);
    check_rect.x = box.rect.x + (box.rect.w - check_size) * 0.5f;
    check_rect.y = box.rect.y + (box.rect.h - check_size) * 0.5f;
    check_rect.w = check_size;
    check_rect.h = check_size;

    s_ui_render_quad(ctx, check_rect,
        ctx->theme.colors[LDK_UI_COLOR_CONTROL_TEXT_ACTIVE], box.clip, 0);
  }

  return value;
}

float ldk_ui_widget_slider(LDKUIContext *ctx, LDKUIId id, float value,
    float min_value, float max_value, LDKUIRect rect)
{
  LDKUIWidgetBox box = {0};
  LDKUIFrameState frame;
  float base_height;
  float track_height_factor;
  float thumb_width_factor;
  float track_height;
  float thumb_width;
  float t;
  LDKUIRect track_rect;
  LDKUIRect fill_rect;
  LDKUIRect thumb_rect;
  u32 track_color;
  u32 fill_color;
  u32 thumb_color;
  u32 border_color;

  if (!s_ui_widget_box_from_explicit_rect(ctx, &box, id, rect, true))
  {
    return value;
  }

  frame = s_ui_frame_state(ctx, box.id, box.rect, box.clip, true, box.disabled);

  base_height = s_ui_maxf(box.rect.h, 1.0f);
  track_height_factor = s_ui_clampf(ctx->theme.slider_track_height, 0.0f, 1.0f);
  thumb_width_factor = s_ui_clampf(ctx->theme.slider_thumb_width, 0.0f, 1.0f);

  track_height = s_ui_maxf(1.0f, base_height * track_height_factor);
  thumb_width = s_ui_minf(base_height * thumb_width_factor, box.rect.w);

  if (frame.active)
  {
    value = s_ui_slider_value_from_cursor(
        box.rect, thumb_width, frame.cursor.x, min_value, max_value);
  }

  if (max_value >= min_value)
  {
    value = s_ui_clampf(value, min_value, max_value);
  }
  else
  {
    value = s_ui_clampf(value, max_value, min_value);
  }

  t = s_ui_slider_normalize(value, min_value, max_value);

  track_rect = box.rect;
  track_rect.h = track_height;
  track_rect.y = box.rect.y + (box.rect.h - track_height) * 0.5f;

  fill_rect = track_rect;
  fill_rect.w =
      thumb_width * 0.5f + s_ui_maxf(0.0f, track_rect.w - thumb_width) * t;

  thumb_rect.x = box.rect.x + s_ui_maxf(0.0f, box.rect.w - thumb_width) * t;
  thumb_rect.y = box.rect.y;
  thumb_rect.w = thumb_width;
  thumb_rect.h = box.rect.h;

  track_color = s_ui_render_slider_track_color(ctx, frame.visual_state);
  fill_color = ctx->theme.colors[LDK_UI_COLOR_SLIDER_FILL];
  thumb_color = s_ui_render_slider_thumb_color(ctx, frame.visual_state);
  border_color = s_ui_render_control_border_color(ctx, frame.visual_state);

  s_ui_render_quad(ctx, track_rect, track_color, box.clip, 0);
  s_ui_render_quad(ctx, fill_rect, fill_color, box.clip, 0);
  s_ui_render_quad(ctx, thumb_rect, thumb_color, box.clip, 0);
  s_ui_render_border(
      ctx, box.rect, ctx->theme.control_border_size, border_color, box.clip);

  return value;
}

static float s_ui_widget_scrollbar(LDKUIContext *ctx, LDKUIId id, float scroll,
    float visible_size, float content_size, LDKUIRect rect, bool horizontal)
{
  LDKUIWidgetBox box = {0};
  LDKUIFrameState frame;
  LDKUIRect track_rect;
  LDKUIRect thumb_rect;
  float max_scroll;
  float thumb_size;
  float thumb_range;

  if (!s_ui_widget_box_from_explicit_rect(ctx, &box, id, rect, true))
  {
    return scroll;
  }

  max_scroll = s_ui_maxf(0.0f, content_size - visible_size);

  if (max_scroll <= 0.0f)
  {
    scroll = 0.0f;
  }
  else
  {
    scroll = s_ui_clampf(scroll, 0.0f, max_scroll);
  }

  s_ui_scrollbar_rects(box.rect, visible_size, content_size, scroll, horizontal,
      &track_rect, &thumb_rect);

  frame = s_ui_frame_state(ctx, box.id, box.rect, box.clip, true, box.disabled);

  if (frame.hot && frame.pressed)
  {
    if (horizontal)
    {
      if (s_ui_rect_contains(&thumb_rect, frame.cursor.x, frame.cursor.y))
      {
        ctx->scrollbar_drag_offset_x = frame.cursor.x - thumb_rect.x;
      }
      else
      {
        ctx->scrollbar_drag_offset_x = thumb_rect.w * 0.5f;
      }
    }
    else
    {
      if (s_ui_rect_contains(&thumb_rect, frame.cursor.x, frame.cursor.y))
      {
        ctx->scrollbar_drag_offset_y = frame.cursor.y - thumb_rect.y;
      }
      else
      {
        ctx->scrollbar_drag_offset_y = thumb_rect.h * 0.5f;
      }
    }
  }

  if (frame.active && max_scroll > 0.0f)
  {
    if (horizontal)
    {
      thumb_size = thumb_rect.w;
      thumb_range = s_ui_maxf(1.0f, track_rect.w - thumb_size);
      scroll = ((frame.cursor.x - track_rect.x - ctx->scrollbar_drag_offset_x) /
                   thumb_range) *
               max_scroll;
    }
    else
    {
      thumb_size = thumb_rect.h;
      thumb_range = s_ui_maxf(1.0f, track_rect.h - thumb_size);
      scroll = ((frame.cursor.y - track_rect.y - ctx->scrollbar_drag_offset_y) /
                   thumb_range) *
               max_scroll;
    }

    scroll = s_ui_clampf(scroll, 0.0f, max_scroll);
    s_ui_scrollbar_rects(box.rect, visible_size, content_size, scroll,
        horizontal, &track_rect, &thumb_rect);
  }

  s_ui_render_quad(ctx, track_rect,
      ctx->theme.colors[LDK_UI_COLOR_SCROLLBAR_TRACK], box.clip, 0);
  s_ui_render_quad(ctx, thumb_rect,
      ctx->theme.colors[LDK_UI_COLOR_SCROLLBAR_THUMB], box.clip, 0);

  return scroll;
}

float ldk_ui_widget_scrollbar_vertical(LDKUIContext *ctx, LDKUIId id,
    float scroll, float visible_size, float content_size, LDKUIRect rect)
{
  return s_ui_widget_scrollbar(
      ctx, id, scroll, visible_size, content_size, rect, false);
}

float ldk_ui_widget_scrollbar_horizontal(LDKUIContext *ctx, LDKUIId id,
    float scroll, float visible_size, float content_size, LDKUIRect rect)
{
  return s_ui_widget_scrollbar(
      ctx, id, scroll, visible_size, content_size, rect, true);
}

typedef enum LDKUIInputVisualMode
{
  LDK_UI_INPUT_VISUAL_BOX = 0,
  LDK_UI_INPUT_VISUAL_LABEL = 1,
} LDKUIInputVisualMode;

static u32 s_ui_widget_input(LDKUIContext *ctx, LDKUIId id, char *buffer,
    u32 buffer_size, LDKUIRect rect, LDKUIInputVisualMode visual_mode)
{
  u32 result = LDK_UI_INPUT_BOX_NONE;
  LDKUIWidgetBox box = {0};
  LDKUIFrameState frame;
  u32 buffer_len;
  LDKUISize text_size;
  u32 bg;
  u32 border;
  u32 text_color;
  float padding_x;
  float text_x;
  float text_y;
  u32 previous_text_cursor;
  u32 previous_text_select_start;
  u32 previous_text_select_end;

  if (buffer == NULL || buffer_size == 0)
  {
    return result;
  }

  buffer[buffer_size - 1] = 0;

  if (!s_ui_widget_box_from_explicit_rect(ctx, &box, id, rect, true))
  {
    return result;
  }

  buffer_len = s_ui_text_cstr_len_u32(buffer);
  text_size = s_ui_widget_text_size(ctx, buffer);
  frame = s_ui_frame_state(ctx, box.id, box.rect, box.clip, true, box.disabled);

  previous_text_cursor = ctx->text_cursor;
  previous_text_select_start = ctx->text_select_start;
  previous_text_select_end = ctx->text_select_end;

  if (frame.pressed && frame.hot)
  {
    ctx->input_box_id = box.id;
    ctx->text_cursor =
        s_ui_input_box_cursor_from_x(ctx, buffer, box.rect, frame.cursor.x);
    ctx->text_select_start = ctx->text_cursor;
    ctx->text_select_end = ctx->text_cursor;
    s_ui_input_cursor_blink_reset(ctx);
  }

  if (frame.focused && ctx->input_box_id != box.id)
  {
    ctx->input_box_id = box.id;
    ctx->text_cursor = buffer_len;
    ctx->text_select_start = buffer_len;
    ctx->text_select_end = buffer_len;
    s_ui_input_cursor_blink_reset(ctx);
  }

  if (frame.focused)
  {
    bool shift = s_ui_input_keyboard_shift_pressed(ctx);

    if (ctx->text_cursor > buffer_len)
    {
      ctx->text_cursor = buffer_len;
    }

    if (ctx->text_select_start > buffer_len)
    {
      ctx->text_select_start = buffer_len;
    }

    if (ctx->text_select_end > buffer_len)
    {
      ctx->text_select_end = buffer_len;
    }

    if (s_ui_input_keyboard_left_pressed(ctx))
    {
      ctx->text_cursor = x_utf8_prev(buffer, ctx->text_cursor);

      if (shift)
      {
        ctx->text_select_end = ctx->text_cursor;
      }
      else
      {
        ctx->text_select_start = ctx->text_cursor;
        ctx->text_select_end = ctx->text_cursor;
      }
    }

    if (s_ui_input_keyboard_right_pressed(ctx))
    {
      ctx->text_cursor = x_utf8_next(buffer, ctx->text_cursor);

      if (shift)
      {
        ctx->text_select_end = ctx->text_cursor;
      }
      else
      {
        ctx->text_select_start = ctx->text_cursor;
        ctx->text_select_end = ctx->text_cursor;
      }
    }

    if (s_ui_input_keyboard_home_pressed(ctx))
    {
      ctx->text_cursor = 0;

      if (shift)
      {
        ctx->text_select_end = ctx->text_cursor;
      }
      else
      {
        ctx->text_select_start = ctx->text_cursor;
        ctx->text_select_end = ctx->text_cursor;
      }
    }

    if (s_ui_input_keyboard_ctrla_pressed(ctx))
    {
      ctx->text_cursor = buffer_len;
      ctx->text_select_start = 0;
      ctx->text_select_end = buffer_len;
    }

    if (s_ui_input_keyboard_delete_pressed(ctx))
    {
      if (ctx->text_select_start != ctx->text_select_end)
      {
        u32 start = ctx->text_select_start;
        u32 end = ctx->text_select_end;

        if (start > end)
        {
          u32 temp = start;
          start = end;
          end = temp;
        }

        if (s_ui_text_delete_range(buffer, buffer_len, start, end))
        {
          ctx->text_cursor = start;
          ctx->text_select_start = start;
          ctx->text_select_end = start;
          result |= LDK_UI_INPUT_BOX_CHANGED;
        }
      }
      else if (ctx->text_cursor < buffer_len)
      {
        u32 end = x_utf8_next(buffer, ctx->text_cursor);

        if (s_ui_text_delete_range(buffer, buffer_len, ctx->text_cursor, end))
        {
          ctx->text_select_start = ctx->text_cursor;
          ctx->text_select_end = ctx->text_cursor;
          result |= LDK_UI_INPUT_BOX_CHANGED;
        }
      }

      buffer_len = s_ui_text_cstr_len_u32(buffer);
    }

    if (s_ui_input_keyboard_end_pressed(ctx))
    {
      ctx->text_cursor = buffer_len;

      if (shift)
      {
        ctx->text_select_end = ctx->text_cursor;
      }
      else
      {
        ctx->text_select_start = ctx->text_cursor;
        ctx->text_select_end = ctx->text_cursor;
      }
    }

    if (s_ui_input_keyboard_backspace_pressed(ctx))
    {
      if (ctx->text_select_start != ctx->text_select_end)
      {
        u32 start = ctx->text_select_start;
        u32 end = ctx->text_select_end;

        if (start > end)
        {
          u32 temp = start;
          start = end;
          end = temp;
        }

        if (s_ui_text_delete_range(buffer, buffer_len, start, end))
        {
          ctx->text_cursor = start;
          ctx->text_select_start = start;
          ctx->text_select_end = start;
          result |= LDK_UI_INPUT_BOX_CHANGED;
        }
      }
      else if (ctx->text_cursor > 0)
      {
        u32 start = x_utf8_prev(buffer, ctx->text_cursor);

        if (s_ui_text_delete_range(buffer, buffer_len, start, ctx->text_cursor))
        {
          ctx->text_cursor = start;
          ctx->text_select_start = start;
          ctx->text_select_end = start;
          result |= LDK_UI_INPUT_BOX_CHANGED;
        }
      }

      buffer_len = s_ui_text_cstr_len_u32(buffer);
    }

    if (ctx->input_text != NULL)
    {
      for (u32 i = 0; i < ctx->input_text->codepoint_count; ++i)
      {
        char encoded[4] = {0};
        u32 encoded_len = 0;

        if (!x_utf8_encode(
                ctx->input_text->codepoints[i], encoded, &encoded_len))
        {
          continue;
        }

        if (ctx->text_select_start != ctx->text_select_end)
        {
          u32 start = ctx->text_select_start;
          u32 end = ctx->text_select_end;

          if (start > end)
          {
            u32 temp = start;
            start = end;
            end = temp;
          }

          if (s_ui_text_delete_range(buffer, buffer_len, start, end))
          {
            ctx->text_cursor = start;
            ctx->text_select_start = start;
            ctx->text_select_end = start;
            buffer_len = s_ui_text_cstr_len_u32(buffer);
          }
        }

        if (s_ui_text_insert_bytes(
                buffer, buffer_size, &ctx->text_cursor, encoded, encoded_len))
        {
          ctx->text_select_start = ctx->text_cursor;
          ctx->text_select_end = ctx->text_cursor;
          buffer_len = s_ui_text_cstr_len_u32(buffer);
          result |= LDK_UI_INPUT_BOX_CHANGED;
        }
      }
    }

    if (s_ui_input_keyboard_accept_pressed(ctx))
    {
      result |= LDK_UI_INPUT_BOX_COMMITTED;
    }

    if (ctx->keyboard != NULL &&
        ldk_os_keyboard_key_down(
            (LDKKeyboardState *)ctx->keyboard, LDK_KEYCODE_ESCAPE))
    {
      result |= LDK_UI_INPUT_BOX_CANCELED;
    }
  }

  if (frame.active)
  {
    ctx->cursor_type = LDK_CURSOR_TEXT_SELECT;
  }

  if (ctx->text_cursor != previous_text_cursor ||
      ctx->text_select_start != previous_text_select_start ||
      ctx->text_select_end != previous_text_select_end ||
      (result & LDK_UI_INPUT_BOX_CHANGED) != 0)
  {
    s_ui_input_cursor_blink_reset(ctx);
  }

  text_size = s_ui_widget_text_size(ctx, buffer);

  bg = s_ui_render_control_bg_color(ctx, frame.visual_state);
  border = s_ui_render_control_border_color(ctx, frame.visual_state);
  text_color = s_ui_render_control_text_color(ctx, frame.visual_state);

  if (visual_mode == LDK_UI_INPUT_VISUAL_BOX)
  {
    s_ui_render_quad(ctx, box.rect, bg, box.clip, 0);
    s_ui_render_border(
        ctx, box.rect, ctx->theme.control_border_size, border, box.clip);
  }

  padding_x = 6.0f;
  text_x = box.rect.x + padding_x;
  text_y = box.rect.y + (box.rect.h - text_size.h) * 0.5f;

  if (frame.focused && ctx->text_select_start != ctx->text_select_end)
  {
    s_ui_render_text_highlight(ctx, buffer, ctx->text_select_start,
        ctx->text_select_end, text_x, box.rect, box.clip,
        ctx->theme.colors[LDK_UI_COLOR_FOCUS]);
  }

  s_ui_render_text(ctx, buffer, text_x, text_y, text_color, box.clip);

  if (frame.focused &&
      (!ctx->theme.text_cursor_blink || ctx->text_cursor_blink_visible))
  {
    LDKTextSize cursor_text_size =
        ldk_ttf_measure_text_cstrn(ctx->font, buffer, ctx->text_cursor);
    float cursor_x = text_x + cursor_text_size.w;
    float cursor_width = ctx->theme.text_cursor_width;
    float cursor_padding_y = ctx->theme.text_cursor_padding_y;

    if (cursor_width <= 0.0f)
    {
      cursor_width = 1.0f;
    }

    if (cursor_padding_y < 0.0f)
    {
      cursor_padding_y = 0.0f;
    }

    if (cursor_padding_y * 2.0f > box.rect.h)
    {
      cursor_padding_y = box.rect.h * 0.5f;
    }

    LDKUIRect cursor_rect = {cursor_x, box.rect.y + cursor_padding_y,
        cursor_width, box.rect.h - cursor_padding_y * 2.0f};
    s_ui_render_quad(
        ctx, cursor_rect, ctx->theme.colors[LDK_UI_COLOR_FOCUS], box.clip, 0);
  }

  return result;
}

u32 ldk_ui_widget_input_box(LDKUIContext *ctx, LDKUIId id, char *buffer,
    u32 buffer_size, LDKUIRect rect)
{
  return s_ui_widget_input(
      ctx, id, buffer, buffer_size, rect, LDK_UI_INPUT_VISUAL_BOX);
}

u32 ldk_ui_widget_input_label(LDKUIContext *ctx, LDKUIId id, char *buffer,
    u32 buffer_size, LDKUIRect rect)
{
  return s_ui_widget_input(
      ctx, id, buffer, buffer_size, rect, LDK_UI_INPUT_VISUAL_LABEL);
}

#include "ui/ldk_ui_layout.inl"
#include "ui/ldk_ui_area.inl"
#include "ui/ldk_ui_window.inl"
#include "ui/ldk_ui_scrollview.inl"
#include "ui/ldk_ui_treenode.inl"
#include "ui/ldk_ui_popup.inl"
