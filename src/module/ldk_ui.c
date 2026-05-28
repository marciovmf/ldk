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

#define LDK_UI_MIN_WINDOW_WIDTH 64.0f
#define LDK_UI_MIN_WINDOW_HEIGHT 48.0f

typedef enum LDKUIWindowResizeEdges
{
  LDK_UI_WINDOW_RESIZE_NONE = 0,
  LDK_UI_WINDOW_RESIZE_LEFT = 1 << 0,
  LDK_UI_WINDOW_RESIZE_RIGHT = 1 << 1,
  LDK_UI_WINDOW_RESIZE_TOP = 1 << 2,
  LDK_UI_WINDOW_RESIZE_BOTTOM = 1 << 3,
} LDKUIWindowResizeEdges;


  typedef struct LDKUIContextInternal
  {
    XArena* frame_arena;
    LDKFontInstance* font;
    void* font_file;
    void* font_texture_user;
    LDKUIGetFontPageTextureFn get_font_page_texture;

    LDKMouseState const* mouse;
    LDKKeyboardState const* keyboard;
    LDKUITextInputState const* input_text;

    LDKUIWindow* current_window;
    LDKUILayout* current_layout;

    XArray_ldk_ui_window* windows;
    XArray_ldk_ui_id* id_stack;
    XArray_ldk_ui_vertex* vertices;
    XArray_ldk_ui_u32* indices;
    XArray_ldk_ui_draw_cmd* commands;
    XArray_ldk_ui_bool* disabled_stack;
    XArray_ldk_ui_hit_candidate* hit_candidates;
    XArray_ldk_ui_debug_rect* debug_rects;
    XArray_ldk_ui_scroll_content_cache* scroll_content_cache;
    XArray_ldk_ui_measure_entry* measure_entries;

    LDKUITheme theme;
    LDKUIRenderData render_data;
    LDKUIRect viewport;
    LDKUIRect last_rect;
    LDKUIRect last_bounding_rect;

    LDKUIId hovered_window_id;
    LDKUIId focused_window_id;
    LDKUIId hot_window_id;
    LDKUIId hot_id;
    LDKUIId next_hot_window_id;
    LDKUIId next_hot_id;
    LDKUIId active_window_id;
    LDKUIId active_id;
    LDKUIId focused_id;
    LDKUIId last_id;
    LDKUIId dragging_window_id;
    LDKUIId resizing_window_id;
    LDKUIId text_box_id;
    // Textbox
    u32 text_cursor;
    u32 text_select_start;
    u32 text_select_end;

    u32 resizing_window_edges;
    u32 hit_order;
    u32 last_measure_entry_index;

    bool next_disabled;
    bool next_focus;
    bool debug_draw;
    bool has_next_width;
    bool has_next_height;
    bool mouse_wheel_consumed;
    LDKUILayoutSize next_width;
    LDKUILayoutSize next_height;
    u32 root_item_count;
    u32 frame_index;
    i32 next_z_order;
    float drag_x;
    float drag_y;
    float resize_start_cursor_x;
    float resize_start_cursor_y;
    LDKUIRect resize_start_rect;

    float scrollbar_drag_offset_x; 
    float scrollbar_drag_offset_y; 
    LDKCursorType cursor_type;
  }LDKUIContextInternal;


#define LDK_UI_SCROLLBAR_THUMB_Y_ID 0x53545931u
#define LDK_UI_SCROLLBAR_THUMB_X_ID 0x53545831u
#define LDK_UI_SCROLLBAR_TRACK_Y_ID 0x53545932u
#define LDK_UI_SCROLLBAR_TRACK_X_ID 0x53545832u

//----------------------------------------------------------
// ID helpers
//----------------------------------------------------------

static LDKUIId s_ui_id_hash_u32(LDKUIId hash, u32 value)
{
  hash ^= value + 0x9e3779b9u + (hash << 6) + (hash >> 2);
  return hash;
}

static LDKUIId s_ui_id_hash_cstr(LDKUIId hash, char const* text)
{
  char const* cursor = text;

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

static LDKUIId s_ui_id_make_in_scope(LDKUIContext* ctx, u32 item_type, u32 item_index)
{
  LDKUIId hash = 2166136261u;
  u32 count = 0;

  if (ctx == NULL)
  {
    return 0;
  }

  count = x_array_ldk_ui_id_count(ctx->id_stack);

  for (u32 i = 0; i < count; ++i)
  {
    LDKUIId* value = x_array_ldk_ui_id_get(ctx->id_stack, i);

    if (value != NULL)
    {
      hash = s_ui_id_hash_u32(hash, *value);
    }
  }

  if (ctx->current_window != NULL)
  {
    hash = s_ui_id_hash_u32(hash, ctx->current_window->id);
  }

  if (ctx->current_layout != NULL)
  {
    hash = s_ui_id_hash_u32(hash, ctx->current_layout->id);
  }

  hash = s_ui_id_hash_u32(hash, item_type);
  hash = s_ui_id_hash_u32(hash, item_index);
  ctx->last_id = hash;

  return hash;
}

static LDKUIId s_ui_id_make_window(LDKUIContext* ctx, u32 item_index)
{
  return s_ui_id_make_in_scope(ctx, (u32)LDK_UI_ITEM_WINDOW, item_index);
}

//----------------------------------------------------------
// Text helpers
//----------------------------------------------------------

static bool s_ui_text_codepoint_is_word_space(u32 codepoint)
{
  return codepoint == ' ' || codepoint == '\t' || codepoint == '\r';
}

static float s_ui_text_codepoint_advance_get(LDKFontInstance* font, u32 previous_codepoint, u32 codepoint)
{
  LDKGlyph const* glyph = NULL;
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

static char const* s_ui_text_skip_word_spaces(char const* cursor)
{
  char const* it = cursor;

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

static bool s_ui_text_wrapped_next_line(LDKFontInstance* font, char const* start, float max_width,
    char const** out_line_start, char const** out_line_end, char const** out_next, float* out_width)
{
  char const* line_start = start;
  char const* cursor = NULL;
  char const* line_end = NULL;
  char const* last_break_next = NULL;
  char const* last_break_line_end = NULL;
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
    char const* before = cursor;
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

      width += s_ui_text_codepoint_advance_get(font, previous_codepoint, codepoint);
      previous_codepoint = codepoint;
      continue;
    }

    float advance = s_ui_text_codepoint_advance_get(font, previous_codepoint, codepoint);

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

static LDKUISize s_ui_text_measure_wrapped(LDKFontInstance* font, char const* text, float max_width)
{
  LDKUISize size = {0};

  if (font == NULL || text == NULL)
  {
    return size;
  }

  size = ldk_ttf_measure_text_cstr_wrapped(font, text, max_width);

  return size;
}

static u32 s_ui_text_cstr_len_u32(char const* text)
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

static u32 s_ui_text_box_cursor_from_x(LDKUIContext* ctx, char const* text, LDKUIRect rect, float x)
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
    //float cursor_x = ldk_ttf_measure_text_cstrn(ctx->font, text, cursor);
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

static bool s_ui_text_delete_range(char* buffer, u32 buffer_len, u32 start, u32 end)
{
  if (buffer == NULL || start >= end || end > buffer_len)
  {
    return false;
  }

  memmove(buffer + start, buffer + end, (size_t)(buffer_len - end + 1));
  return true;
}

static bool s_ui_text_insert_bytes(char* buffer, u32 buffer_size, u32* cursor, char const* text, u32 text_len)
{
  u32 buffer_len = s_ui_text_cstr_len_u32(buffer);

  if (buffer == NULL || cursor == NULL || text == NULL || buffer_size == 0 || text_len == 0)
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

  memmove(buffer + *cursor + text_len, buffer + *cursor, (size_t)(buffer_len - *cursor + 1));
  memcpy(buffer + *cursor, text, (size_t)text_len);
  *cursor += text_len;
  return true;
}

/**
 * Computes the optimal text color for a given background color.
 * Returns either black or white, whichever provides the greatest
 * contrast and readability against the specified background.
 *
 * @see
 *  https://developer.mozilla.org/en-US/docs/Web/Accessibility/Guides/Colors_and_Luminance
 *  https://www.w3.org/TR/WCAG22/?utm_source=chatgpt.com#dfn-contrast-ratio
 *  https://entropymine.com/imageworsener/srgbformula/
 */
static u32 s_ui_text_contrast_for_bg(u32 bg)
{
  // Extract sRGB components
  float r = (float)((bg >> 24) & 0xffu) / 255.0f;
  float g = (float)((bg >> 16) & 0xffu) / 255.0f;
  float b = (float)((bg >> 8) & 0xffu) / 255.0f;

  // Convert sRGB to linear space
  r = (r <= 0.04045f) ? r / 12.92f : powf((r + 0.055f) / 1.055f, 2.4f);
  g = (g <= 0.04045f) ? g / 12.92f : powf((g + 0.055f) / 1.055f, 2.4f);
  b = (b <= 0.04045f) ? b / 12.92f : powf((b + 0.055f) / 1.055f, 2.4f);

  // Compute relative luminance
  float bg_luminance = 0.2126f * r + 0.7152f * g + 0.0722f * b;

  // Compute contrast ratio against black and white
  // Return the highest contrast text color
  float white_contrast = (1.0f + 0.05f) / (bg_luminance + 0.05f);
  float black_contrast = (bg_luminance + 0.05f) / (0.0f + 0.05f);

  if (black_contrast >= white_contrast)
  {
    return 0x000000ffu;
  }

  return 0xffffffffu;
}


//----------------------------------------------------------
// Keyboard input helpers
//----------------------------------------------------------

static bool s_ui_input_keyboard_shift_pressed(LDKUIContext* ctx)
{
  if (ctx->keyboard == NULL)
  {
    return false;
  }

  return ldk_os_keyboard_key_is_pressed((LDKKeyboardState*)ctx->keyboard, LDK_KEYCODE_SHIFT);
}

static bool s_ui_input_keyboard_backspace_pressed(LDKUIContext* ctx)
{
  if (ctx->keyboard == NULL)
  {
    return false;
  }

  return ldk_os_keyboard_key_down((LDKKeyboardState*)ctx->keyboard, LDK_KEYCODE_BACKSPACE);
}

static bool s_ui_input_keyboard_delete_pressed(LDKUIContext* ctx)
{
  if (ctx->keyboard == NULL)
  {
    return false;
  }

  return ldk_os_keyboard_key_down((LDKKeyboardState*)ctx->keyboard, LDK_KEYCODE_DELETE);
}

static bool s_ui_input_keyboard_ctrla_pressed(LDKUIContext* ctx)
{
  if (ctx->keyboard == NULL)
  {
    return false;
  }

  return ldk_os_keyboard_key_down((LDKKeyboardState*)ctx->keyboard, LDK_KEYCODE_A) &&
    ldk_os_keyboard_key_is_pressed((LDKKeyboardState*)ctx->keyboard, LDK_KEYCODE_CONTROL);

}

static bool s_ui_input_keyboard_home_pressed(LDKUIContext* ctx)
{
  if (ctx->keyboard == NULL)
  {
    return false;
  }

  return ldk_os_keyboard_key_down((LDKKeyboardState*)ctx->keyboard, LDK_KEYCODE_HOME);
}

static bool s_ui_input_keyboard_end_pressed(LDKUIContext* ctx)
{
  if (ctx->keyboard == NULL)
  {
    return false;
  }

  return ldk_os_keyboard_key_down((LDKKeyboardState*)ctx->keyboard, LDK_KEYCODE_END);
}

static bool s_ui_input_keyboard_accept_pressed(LDKUIContext* ctx)
{
  if (ctx->keyboard == NULL)
  {
    return false;
  }

  if (ldk_os_keyboard_key_down((LDKKeyboardState*)ctx->keyboard, LDK_KEYCODE_ENTER))
  {
    return true;
  }

  if (ldk_os_keyboard_key_down((LDKKeyboardState*)ctx->keyboard, LDK_KEYCODE_SPACE))
  {
    return true;
  }

  return false;
}

static bool s_ui_input_keyboard_left_pressed(LDKUIContext* ctx)
{
  if (ctx->keyboard == NULL)
  {
    return false;
  }

  return ldk_os_keyboard_key_down((LDKKeyboardState*)ctx->keyboard, LDK_KEYCODE_LEFT);
}

static bool s_ui_input_keyboard_right_pressed(LDKUIContext* ctx)
{
  if (ctx->keyboard == NULL)
  {
    return false;
  }

  return ldk_os_keyboard_key_down((LDKKeyboardState*)ctx->keyboard, LDK_KEYCODE_RIGHT);
}

//----------------------------------------------------------
// Window state
//----------------------------------------------------------

static void s_ui_window_destroy_buffers(LDKUIWindow* window)
{
  if (window == NULL)
  {
    return;
  }

  if (window->vertices != NULL)
  {
    x_array_destroy(window->vertices);
    window->vertices = NULL;
  }

  if (window->indices != NULL)
  {
    x_array_destroy(window->indices);
    window->indices = NULL;
  }

  if (window->commands != NULL)
  {
    x_array_destroy(window->commands);
    window->commands = NULL;
  }
}

static LDKUIWindow* s_ui_window_find(LDKUIContext* ctx, LDKUIId id)
{
  u32 count = 0;

  if (ctx == NULL)
  {
    return NULL;
  }

  count = x_array_ldk_ui_window_count(ctx->windows);

  for (u32 i = 0; i < count; ++i)
  {
    LDKUIWindow* window = x_array_ldk_ui_window_get(ctx->windows, i);

    if (window != NULL && window->id == id)
    {
      return window;
    }
  }

  return NULL;
}

static LDKUIWindow* s_ui_window_get_or_create(LDKUIContext* ctx, LDKUIId id, char const* title)
{
  LDKUIWindow* window = s_ui_window_find(ctx, id);
  LDKUIWindow new_window;

  if (window != NULL)
  {
    return window;
  }

  memset(&new_window, 0, sizeof(new_window));
  new_window.id = id;
  new_window.z_order = ctx->next_z_order++;
  new_window.vertices = x_array_ldk_ui_vertex_create(256);
  new_window.indices = x_array_ldk_ui_u32_create(512);
  new_window.commands = x_array_ldk_ui_draw_cmd_create(64);

  if (title != NULL)
  {
    strncpy(new_window.title, title, sizeof(new_window.title) - 1);
    new_window.title[sizeof(new_window.title) - 1] = 0;
  }

  x_array_ldk_ui_window_push(ctx->windows, new_window);

  return x_array_ldk_ui_window_back(ctx->windows);
}

static LDKUIWindow* s_ui_window_topmost_at_cursor(LDKUIContext* ctx)
{
  LDKPoint cursor;
  LDKUIWindow* result = NULL;
  i32 best_z = INT32_MIN;
  u32 count = 0;

  if (ctx == NULL || ctx->mouse == NULL)
  {
    return NULL;
  }

  cursor = ldk_os_mouse_cursor((LDKMouseState*)ctx->mouse);
  count = x_array_ldk_ui_window_count(ctx->windows);

  for (u32 i = 0; i < count; ++i)
  {
    LDKUIWindow* window = x_array_ldk_ui_window_get(ctx->windows, i);

    if (window == NULL)
    {
      continue;
    }

    if (!s_ui_rect_contains(&window->rect, (float)cursor.x, (float)cursor.y))
    {
      continue;
    }

    if (result == NULL || window->z_order > best_z)
    {
      result = window;
      best_z = window->z_order;
    }
  }

  return result;
}

static void s_ui_window_bring_to_front(LDKUIContext* ctx, LDKUIWindow* window)
{
  if (ctx == NULL || window == NULL)
  {
    return;
  }

  window->z_order = ctx->next_z_order++;
  ctx->focused_window_id = window->id;
  ctx->focused_id = window->focused_id;
}

static void s_ui_windows_clear_frame_buffers(LDKUIContext* ctx)
{
  u32 count = x_array_ldk_ui_window_count(ctx->windows);

  for (u32 i = 0; i < count; ++i)
  {
    LDKUIWindow* window = x_array_ldk_ui_window_get(ctx->windows, i);

    if (window == NULL)
    {
      continue;
    }

    x_array_ldk_ui_vertex_clear(window->vertices);
    x_array_ldk_ui_u32_clear(window->indices);
    x_array_ldk_ui_draw_cmd_clear(window->commands);
  }
}

static void s_ui_windows_gc(LDKUIContext* ctx)
{
  u32 i = 0;

  while (i < x_array_ldk_ui_window_count(ctx->windows))
  {
    LDKUIWindow* window = x_array_ldk_ui_window_get(ctx->windows, i);

    if (window == NULL)
    {
      i += 1;
      continue;
    }

    if (window->last_frame_seen != ctx->frame_index)
    {
      if (ctx->hovered_window_id == window->id)
      {
        ctx->hovered_window_id = 0;
      }

      if (ctx->focused_window_id == window->id)
      {
        ctx->focused_window_id = 0;
        ctx->focused_id = 0;
      }

      if (ctx->active_window_id == window->id)
      {
        ctx->active_window_id = 0;
        ctx->active_id = 0;
      }

      s_ui_window_destroy_buffers(window);
      x_array_ldk_ui_window_delete_at(ctx->windows, i);
      continue;
    }

    i += 1;
  }
}

//----------------------------------------------------------
// Draw buffer helpers
//----------------------------------------------------------

static XArray_ldk_ui_vertex* s_ui_target_vertices(LDKUIContext* ctx)
{
  if (ctx->current_window != NULL && ctx->current_window->vertices != NULL)
  {
    return ctx->current_window->vertices;
  }

  return ctx->vertices;
}

static XArray_ldk_ui_u32* s_ui_target_indices(LDKUIContext* ctx)
{
  if (ctx->current_window != NULL && ctx->current_window->indices != NULL)
  {
    return ctx->current_window->indices;
  }

  return ctx->indices;
}

static XArray_ldk_ui_draw_cmd* s_ui_target_commands(LDKUIContext* ctx)
{
  if (ctx->current_window != NULL && ctx->current_window->commands != NULL)
  {
    return ctx->current_window->commands;
  }

  return ctx->commands;
}

static void s_ui_render_add_draw_cmd(LDKUIContext* ctx, LDKUITextureHandle texture, LDKUIRect clip_rect, u32 index_offset, u32 index_count)
{
  XArray_ldk_ui_draw_cmd* commands = s_ui_target_commands(ctx);
  LDKUIDrawCmd* back = x_array_ldk_ui_draw_cmd_back(commands);

  if (back != NULL)
  {
    bool same_texture = back->texture == texture;
    bool same_clip = back->clip_rect.x == clip_rect.x &&
      back->clip_rect.y == clip_rect.y &&
      back->clip_rect.w == clip_rect.w &&
      back->clip_rect.h == clip_rect.h;
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

static void s_ui_render_quad(LDKUIContext* ctx, LDKUIRect rect, u32 color, LDKUIRect clip_rect, LDKUITextureHandle texture)
{
  XArray_ldk_ui_vertex* vertices = s_ui_target_vertices(ctx);
  XArray_ldk_ui_u32* indices = s_ui_target_indices(ctx);
  u32 index_offset = x_array_ldk_ui_u32_count(indices);
  u32 base_index = x_array_ldk_ui_vertex_count(vertices);

  color = LDK_RGBA32(color);

  x_array_ldk_ui_vertex_push(vertices, (LDKUIVertex){ rect.x, rect.y, 0.0f, 0.0f, color });
  x_array_ldk_ui_vertex_push(vertices, (LDKUIVertex){ rect.x + rect.w, rect.y, 1.0f, 0.0f, color });
  x_array_ldk_ui_vertex_push(vertices, (LDKUIVertex){ rect.x + rect.w, rect.y + rect.h, 1.0f, 1.0f, color });
  x_array_ldk_ui_vertex_push(vertices, (LDKUIVertex){ rect.x, rect.y + rect.h, 0.0f, 1.0f, color });

  x_array_ldk_ui_u32_push(indices, base_index + 0);
  x_array_ldk_ui_u32_push(indices, base_index + 1);
  x_array_ldk_ui_u32_push(indices, base_index + 2);
  x_array_ldk_ui_u32_push(indices, base_index + 2);
  x_array_ldk_ui_u32_push(indices, base_index + 3);
  x_array_ldk_ui_u32_push(indices, base_index + 0);

  s_ui_render_add_draw_cmd(ctx, texture, clip_rect, index_offset, 6);
}

static void s_ui_render_border(LDKUIContext* ctx, LDKUIRect rect, float size, u32 color, LDKUIRect clip_rect)
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

  top = (LDKUIRect){ rect.x, rect.y, rect.w, border_size };
  bottom = (LDKUIRect){ rect.x, rect.y + rect.h - border_size, rect.w, border_size };
  left = (LDKUIRect){ rect.x, rect.y, border_size, rect.h };
  right = (LDKUIRect){ rect.x + rect.w - border_size, rect.y, border_size, rect.h };

  s_ui_render_quad(ctx, top, color, clip_rect, 0);
  s_ui_render_quad(ctx, bottom, color, clip_rect, 0);
  s_ui_render_quad(ctx, left, color, clip_rect, 0);
  s_ui_render_quad(ctx, right, color, clip_rect, 0);
}


static void s_ui_debug_add_rect(LDKUIContext* ctx, LDKUIId item_id, LDKUIDebugRectType type, LDKUIRect rect, LDKUIRect clip_rect)
{
  LDKUIDebugRect debug_rect;

  if (ctx == NULL || ctx->debug_rects == NULL || ctx->current_window == NULL)
  {
    return;
  }

  if (!ctx->debug_draw)
  {
    return;
  }

  debug_rect.window_id = ctx->current_window->id;
  debug_rect.item_id = item_id;
  debug_rect.type = type;
  debug_rect.rect = rect;
  debug_rect.clip_rect = clip_rect;

  x_array_ldk_ui_debug_rect_push(ctx->debug_rects, debug_rect);
}

static void s_ui_debug_draw_rects(LDKUIContext* ctx)
{
  LDKUIWindow* previous_window;
  u32 count;

  if (ctx == NULL || !ctx->debug_draw || ctx->debug_rects == NULL)
  {
    return;
  }

  previous_window = ctx->current_window;
  count = x_array_ldk_ui_debug_rect_count(ctx->debug_rects);

  for (u32 i = 0; i < count; ++i)
  {
    LDKUIDebugRect* debug_rect = x_array_ldk_ui_debug_rect_get(ctx->debug_rects, i);
    LDKUIWindow* window;

    if (debug_rect == NULL)
    {
      continue;
    }

    window = s_ui_window_find(ctx, debug_rect->window_id);

    if (window == NULL || window->last_frame_seen != ctx->frame_index)
    {
      continue;
    }

    ctx->current_window = window;

    if (debug_rect->type == LDK_UI_DEBUG_RECT_LAYOUT)
    {
      s_ui_render_border(ctx, debug_rect->rect, 1.0f, 0x0000ffffu, window->rect);
    }

    s_ui_render_border(ctx, debug_rect->clip_rect, 1.0f, 0xff00ffffu, window->rect);

    if (debug_rect->item_id != 0 &&
        debug_rect->window_id == ctx->next_hot_window_id &&
        debug_rect->item_id == ctx->next_hot_id)
    {
      s_ui_render_border(ctx, debug_rect->rect, 1.0f, 0xff0000ffu, window->rect);
    }
  }

  ctx->current_window = previous_window;
}

static void s_ui_render_text_highlight(LDKUIContext* ctx, char const* text, u32 start, u32 end,
    float text_x, LDKUIRect rect, LDKUIRect clip, u32 color)
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
  //text_size = ldk_ttf_measure_text_cstrn(ctx->font, text, start);
  float x1 = text_x + text_size.w;

  LDKUIRect highlight_rect;
  highlight_rect.x = x0;
  highlight_rect.y = rect.y + 3.0f;
  highlight_rect.w = x1 - x0;
  highlight_rect.h = rect.h - 6.0f;

  s_ui_render_quad(ctx, highlight_rect, color, clip, 0);
}

static void s_ui_render_text(LDKUIContext* ctx, char const* text, float x, float y, u32 color, LDKUIRect clip_rect)
{
  LDKFontInstance* font;
  LDKFontMetrics metrics;
  float pen_x;
  float pen_y;
  u32 prev_codepoint;
  char const* cursor;

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
    LDKGlyph const* glyph;
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
    XArray_ldk_ui_vertex* vertices = s_ui_target_vertices(ctx);
    XArray_ldk_ui_u32* indices = s_ui_target_indices(ctx);

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

    x_array_ldk_ui_vertex_push(vertices, (LDKUIVertex){ gx0, gy0, u0, v0, color });
    x_array_ldk_ui_vertex_push(vertices, (LDKUIVertex){ gx1, gy0, u1, v0, color });
    x_array_ldk_ui_vertex_push(vertices, (LDKUIVertex){ gx1, gy1, u1, v1, color });
    x_array_ldk_ui_vertex_push(vertices, (LDKUIVertex){ gx0, gy1, u0, v1, color });

    x_array_ldk_ui_u32_push(indices, base_index + 0);
    x_array_ldk_ui_u32_push(indices, base_index + 1);
    x_array_ldk_ui_u32_push(indices, base_index + 2);
    x_array_ldk_ui_u32_push(indices, base_index + 2);
    x_array_ldk_ui_u32_push(indices, base_index + 3);
    x_array_ldk_ui_u32_push(indices, base_index + 0);

    if (ctx->get_font_page_texture != NULL)
    {
      texture = ctx->get_font_page_texture(ctx->font_texture_user, ctx->font, glyph->page_index);
    }

    s_ui_render_add_draw_cmd(ctx, texture, clip_rect, index_offset, 6);

    pen_x += (float)glyph->advance_x;
    prev_codepoint = codepoint;
  }
}

static void s_ui_render_text_range(LDKUIContext* ctx, char const* text_start, char const* text_end,
    float x, float y, u32 color, LDKUIRect clip_rect)
{
  if (ctx == NULL || text_start == NULL || text_end == NULL || text_start >= text_end)
  {
    return;
  }

  LDKFontInstance* font = ctx->font;

  if (font == NULL)
  {
    return;
  }

  color = LDK_RGBA32(color);
  LDKFontMetrics metrics = ldk_ttf_get_metrics(font);
  float pen_x = x;
  float pen_y = y + metrics.ascent;
  u32 prev_codepoint = 0;
  char const* cursor = text_start;

  while (cursor < text_end && *cursor != '\0')
  {
    u32 codepoint = 0;
    char const* before = cursor;

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

    LDKGlyph const* glyph = ldk_ttf_get_glyph(font, codepoint);

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

    XArray_ldk_ui_vertex* vertices = s_ui_target_vertices(ctx);
    XArray_ldk_ui_u32* indices = s_ui_target_indices(ctx);
    u32 index_offset = x_array_ldk_ui_u32_count(indices);
    u32 base_index = x_array_ldk_ui_vertex_count(vertices);

    x_array_ldk_ui_vertex_push(vertices, (LDKUIVertex){ gx0, gy0, u0, v0, color });
    x_array_ldk_ui_vertex_push(vertices, (LDKUIVertex){ gx1, gy0, u1, v0, color });
    x_array_ldk_ui_vertex_push(vertices, (LDKUIVertex){ gx1, gy1, u1, v1, color });
    x_array_ldk_ui_vertex_push(vertices, (LDKUIVertex){ gx0, gy1, u0, v1, color });

    x_array_ldk_ui_u32_push(indices, base_index + 0);
    x_array_ldk_ui_u32_push(indices, base_index + 1);
    x_array_ldk_ui_u32_push(indices, base_index + 2);
    x_array_ldk_ui_u32_push(indices, base_index + 2);
    x_array_ldk_ui_u32_push(indices, base_index + 3);
    x_array_ldk_ui_u32_push(indices, base_index + 0);

    LDKUITextureHandle texture = 0;

    if (ctx->get_font_page_texture != NULL)
    {
      texture = ctx->get_font_page_texture(ctx->font_texture_user, ctx->font, glyph->page_index);
    }

    s_ui_render_add_draw_cmd(ctx, texture, clip_rect, index_offset, 6);

    pen_x += (float)glyph->advance_x;
    prev_codepoint = codepoint;
  }
}

static void s_ui_render_text_wrapped(LDKUIContext* ctx, char const* text, float x, float y,
    float max_width, u32 color, LDKUIRect clip_rect)
{
  if (ctx == NULL || ctx->font == NULL || text == NULL)
  {
    return;
  }

  float line_height = ldk_ttf_get_line_height(ctx->font);
  char const* cursor = text;
  float line_y = y;

  while (cursor != NULL && *cursor != '\0')
  {
    char const* line_start = NULL;
    char const* line_end = NULL;
    char const* next = NULL;
    if (!s_ui_text_wrapped_next_line(ctx->font, cursor, max_width, &line_start, &line_end, &next, NULL))
    {
      break;
    }

    s_ui_render_text_range(ctx, line_start, line_end, x, line_y, color, clip_rect);
    line_y += line_height;

    if (next == NULL || next <= cursor)
    {
      break;
    }

    cursor = next;
  }
}

static u32 s_ui_render_control_bg_color(LDKUIContext* ctx, LDKUIControlVisualState state)
{
  if (state == LDK_UI_CONTROL_VISUAL_STATE_HOVERED)
  {
    return ctx->theme.colors[LDK_UI_COLOR_CONTROL_BG_HOVERED];
  }

  if (state == LDK_UI_CONTROL_VISUAL_STATE_ACTIVE || state == LDK_UI_CONTROL_VISUAL_STATE_ACTIVE_HOVERED)
  {
    return ctx->theme.colors[LDK_UI_COLOR_CONTROL_BG_ACTIVE];
  }

  return ctx->theme.colors[LDK_UI_COLOR_CONTROL_BG];
}

static u32 s_ui_render_control_text_color(LDKUIContext* ctx, LDKUIControlVisualState state)
{
  if (state == LDK_UI_CONTROL_VISUAL_STATE_DISABLED)
  {
    return ctx->theme.colors[LDK_UI_COLOR_CONTROL_TEXT_DISABLED];
  }

  if (state == LDK_UI_CONTROL_VISUAL_STATE_ACTIVE || state == LDK_UI_CONTROL_VISUAL_STATE_ACTIVE_HOVERED)
  {
    return ctx->theme.colors[LDK_UI_COLOR_CONTROL_TEXT_ACTIVE];
  }

  if (state == LDK_UI_CONTROL_VISUAL_STATE_HOVERED)
  {
    return ctx->theme.colors[LDK_UI_COLOR_CONTROL_TEXT_HOVERED];
  }

  return ctx->theme.colors[LDK_UI_COLOR_CONTROL_TEXT];
}

static u32 s_ui_render_control_border_color(LDKUIContext* ctx, LDKUIControlVisualState state)
{
  if (state == LDK_UI_CONTROL_VISUAL_STATE_DISABLED)
  {
    return ctx->theme.colors[LDK_UI_COLOR_CONTROL_BORDER_DISABLED];
  }

  if (state == LDK_UI_CONTROL_VISUAL_STATE_ACTIVE || state == LDK_UI_CONTROL_VISUAL_STATE_ACTIVE_HOVERED)
  {
    return ctx->theme.colors[LDK_UI_COLOR_CONTROL_BORDER_ACTIVE];
  }

  if (state == LDK_UI_CONTROL_VISUAL_STATE_HOVERED)
  {
    return ctx->theme.colors[LDK_UI_COLOR_CONTROL_BORDER_HOVERED];
  }

  return ctx->theme.colors[LDK_UI_COLOR_CONTROL_BORDER];
}

static u32 s_ui_render_slider_track_color(LDKUIContext* ctx, LDKUIControlVisualState state)
{
  if (state == LDK_UI_CONTROL_VISUAL_STATE_ACTIVE || state == LDK_UI_CONTROL_VISUAL_STATE_ACTIVE_HOVERED)
  {
    return ctx->theme.colors[LDK_UI_COLOR_SLIDER_TRACK_ACTIVE];
  }

  if (state == LDK_UI_CONTROL_VISUAL_STATE_HOVERED)
  {
    return ctx->theme.colors[LDK_UI_COLOR_SLIDER_TRACK_HOVERED];
  }

  return ctx->theme.colors[LDK_UI_COLOR_SLIDER_TRACK];
}

static u32 s_ui_render_slider_thumb_color(LDKUIContext* ctx, LDKUIControlVisualState state)
{
  if (state == LDK_UI_CONTROL_VISUAL_STATE_ACTIVE || state == LDK_UI_CONTROL_VISUAL_STATE_ACTIVE_HOVERED)
  {
    return ctx->theme.colors[LDK_UI_COLOR_SLIDER_THUMB_ACTIVE];
  }

  if (state == LDK_UI_CONTROL_VISUAL_STATE_HOVERED)
  {
    return ctx->theme.colors[LDK_UI_COLOR_SLIDER_THUMB_HOVERED];
  }

  return ctx->theme.colors[LDK_UI_COLOR_SLIDER_THUMB];
}

static float s_ui_slider_normalize(float value, float min_value, float max_value)
{
  float range = max_value - min_value;

  if (range <= 0.0f)
  {
    return 0.0f;
  }

  return s_ui_clampf((value - min_value) / range, 0.0f, 1.0f);
}

static float s_ui_slider_value_from_cursor(LDKUIRect rect, float thumb_width, float cursor_x, float min_value, float max_value)
{
  float usable_width = s_ui_maxf(rect.w - thumb_width, 1.0f);
  float local_x = cursor_x - rect.x - thumb_width * 0.5f;
  float t = s_ui_clampf(local_x / usable_width, 0.0f, 1.0f);

  return min_value + (max_value - min_value) * t;
}

static bool s_ui_scrollbar_rects(LDKUIRect rect, float visible_size, float content_size, float scroll, bool horizontal, LDKUIRect* track_rect, LDKUIRect* thumb_rect)
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

    thumb_w = max_scroll > 0.0f ? (visible_size / content_size) * track_rect->w : track_rect->w;
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

    thumb_h = max_scroll > 0.0f ? (visible_size / content_size) * track_rect->h : track_rect->h;
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


static LDKUIScrollContentCache* s_ui_scroll_content_cache_find(LDKUIContext* ctx, LDKUIId id)
{
  u32 count;

  if (ctx == NULL || ctx->scroll_content_cache == NULL)
  {
    return NULL;
  }

  count = x_array_ldk_ui_scroll_content_cache_count(ctx->scroll_content_cache);

  for (u32 i = 0; i < count; ++i)
  {
    LDKUIScrollContentCache* cache = x_array_ldk_ui_scroll_content_cache_get(ctx->scroll_content_cache, i);

    if (cache != NULL && cache->id == id)
    {
      return cache;
    }
  }

  return NULL;
}

static LDKUIScrollContentCache* s_ui_scroll_content_cache_get_or_create(LDKUIContext* ctx, LDKUIId id)
{
  LDKUIScrollContentCache* cache;
  LDKUIScrollContentCache new_cache;

  if (ctx == NULL || ctx->scroll_content_cache == NULL)
  {
    return NULL;
  }

  cache = s_ui_scroll_content_cache_find(ctx, id);

  if (cache != NULL)
  {
    cache->last_frame_touched = ctx->frame_index;
    return cache;
  }

  memset(&new_cache, 0, sizeof(new_cache));
  new_cache.id = id;
  new_cache.last_frame_touched = ctx->frame_index;

  x_array_ldk_ui_scroll_content_cache_push(ctx->scroll_content_cache, new_cache);

  return x_array_ldk_ui_scroll_content_cache_back(ctx->scroll_content_cache);
}

static void s_ui_scroll_content_cache_gc(LDKUIContext* ctx)
{
  u32 i = 0;

  if (ctx == NULL || ctx->scroll_content_cache == NULL)
  {
    return;
  }

  while (i < x_array_ldk_ui_scroll_content_cache_count(ctx->scroll_content_cache))
  {
    LDKUIScrollContentCache* cache = x_array_ldk_ui_scroll_content_cache_get(ctx->scroll_content_cache, i);

    if (cache == NULL)
    {
      i += 1;
      continue;
    }

    if (ctx->frame_index - cache->last_frame_touched > 2)
    {
      x_array_ldk_ui_scroll_content_cache_delete_at(ctx->scroll_content_cache, i);
      continue;
    }

    i += 1;
  }
}


//----------------------------------------------------------
// Layout helpers
//----------------------------------------------------------


static float s_ui_rect_right(LDKUIRect rect)
{
  return rect.x + rect.w;
}

static float s_ui_rect_bottom(LDKUIRect rect)
{
  return rect.y + rect.h;
}

static bool s_ui_rect_is_empty(LDKUIRect rect)
{
  return rect.w <= 0.0f && rect.h <= 0.0f;
}

static LDKUIRect s_ui_rect_union_bounds(LDKUIRect a, LDKUIRect b)
{
  if (s_ui_rect_is_empty(a))
  {
    return b;
  }

  if (s_ui_rect_is_empty(b))
  {
    return a;
  }

  float x0 = s_ui_minf(a.x, b.x);
  float y0 = s_ui_minf(a.y, b.y);
  float x1 = s_ui_maxf(s_ui_rect_right(a), s_ui_rect_right(b));
  float y1 = s_ui_maxf(s_ui_rect_bottom(a), s_ui_rect_bottom(b));

  return (LDKUIRect){ x0, y0, x1 - x0, y1 - y0 };
}

static void s_ui_layout_expand_bounding_rect(LDKUILayout* layout, LDKUIRect rect)
{
  if (layout == NULL)
  {
    return;
  }

  layout->bounding_rect = s_ui_rect_union_bounds(layout->bounding_rect, rect);
  layout->content_used_right = s_ui_maxf(layout->content_used_right, s_ui_rect_right(rect));
  layout->content_used_bottom = s_ui_maxf(layout->content_used_bottom, s_ui_rect_bottom(rect));
}

static u32 s_ui_measure_entry_add(LDKUIContext* ctx, LDKUILayout* layout, LDKUIRect rect)
{
  if (ctx == NULL || ctx->measure_entries == NULL)
  {
    return UINT32_MAX;
  }

  u32 index = x_array_ldk_ui_measure_entry_count(ctx->measure_entries);
  LDKUIMeasureEntry entry = { layout, rect };
  x_array_ldk_ui_measure_entry_push(ctx->measure_entries, entry);
  return index;
}

static void s_ui_measure_entry_union_at(LDKUIContext* ctx, u32 index, LDKUIRect rect)
{
  if (ctx == NULL || ctx->measure_entries == NULL || index == UINT32_MAX)
  {
    return;
  }

  if (index >= x_array_ldk_ui_measure_entry_count(ctx->measure_entries))
  {
    return;
  }

  LDKUIMeasureEntry* entry = x_array_ldk_ui_measure_entry_get(ctx->measure_entries, index);

  if (entry == NULL)
  {
    return;
  }

  entry->rect = s_ui_rect_union_bounds(entry->rect, rect);
}


LDKUILayoutSize ldk_ui_px(float value)
{
  return (LDKUILayoutSize){ LDK_UI_SIZE_PIXELS, value };
}

LDKUILayoutSize ldk_ui_percent(float value)
{
  return (LDKUILayoutSize){ LDK_UI_SIZE_PERCENT, value };
}

LDKUILayoutSize ldk_ui_fill(void)
{
  return (LDKUILayoutSize){ LDK_UI_SIZE_FILL, 0.0f };
}

void ldk_ui_set_next_width(LDKUIContext* ctx, LDKUILayoutSize width)
{
  if (ctx == NULL)
  {
    return;
  }

  ctx->next_width = width;
  ctx->has_next_width = true;
}

void ldk_ui_set_next_height(LDKUIContext* ctx, LDKUILayoutSize height)
{
  if (ctx == NULL)
  {
    return;
  }

  ctx->next_height = height;
  ctx->has_next_height = true;
}

void ldk_ui_set_next_size(LDKUIContext* ctx, LDKUILayoutSize width, LDKUILayoutSize height)
{
  if (ctx == NULL)
  {
    return;
  }

  ctx->next_width = width;
  ctx->next_height = height;
  ctx->has_next_width = true;
  ctx->has_next_height = true;
}

LDKUIRect ldk_ui_last_rect(LDKUIContext* ctx)
{
  if (ctx == NULL)
  {
    return (LDKUIRect){0};
  }

  return ctx->last_rect;
}

LDKUIRect ldk_ui_last_bounding_rect(LDKUIContext* ctx)
{
  if (ctx == NULL)
  {
    return (LDKUIRect){0};
  }

  return ctx->last_bounding_rect;
}

LDKUIMark ldk_ui_mark(LDKUIContext* ctx)
{
  LDKUIMark mark = {0};

  if (ctx == NULL || ctx->current_layout == NULL || ctx->measure_entries == NULL)
  {
    return mark;
  }

  mark.layout = ctx->current_layout;
  mark.measure_index = x_array_ldk_ui_measure_entry_count(ctx->measure_entries);

  return mark;
}

LDKUIRect ldk_ui_measure_from(LDKUIContext* ctx, LDKUIMark mark)
{
  LDKUIRect rect = {0};

  if (ctx == NULL || ctx->measure_entries == NULL || mark.layout == NULL)
  {
    return rect;
  }

  u32 count = x_array_ldk_ui_measure_entry_count(ctx->measure_entries);

  if (mark.measure_index > count)
  {
    return rect;
  }

  for (u32 i = mark.measure_index; i < count; ++i)
  {
    LDKUIMeasureEntry* entry = x_array_ldk_ui_measure_entry_get(ctx->measure_entries, i);

    if (entry == NULL || entry->layout != mark.layout)
    {
      continue;
    }

    rect = s_ui_rect_union_bounds(rect, entry->rect);
  }

  return rect;
}

static float s_ui_resolve_size(LDKUILayoutSize size, float parent_size, float remaining_size)
{
  if (size.mode == LDK_UI_SIZE_PIXELS)
  {
    return size.value;
  }

  if (size.mode == LDK_UI_SIZE_PERCENT)
  {
    return parent_size * (size.value / 100.0f);
  }

  return remaining_size;
}

static LDKUIRect s_ui_layout_next_rect(LDKUIContext* ctx, LDKUILayoutSize default_width, LDKUILayoutSize default_height)
{
  LDKUIRect rect = {0};

  if (ctx == NULL || ctx->current_layout == NULL)
  {
    return rect;
  }

  LDKUILayout* layout = ctx->current_layout;
  LDKUILayoutSize width = ctx->has_next_width ? ctx->next_width : default_width;
  LDKUILayoutSize height = ctx->has_next_height ? ctx->next_height : default_height;

  ctx->has_next_width = false;
  ctx->has_next_height = false;

  float parent_w = layout->content_rect.w;
  float parent_h = layout->content_rect.h;
  float remaining_w = s_ui_maxf(0.0f, layout->content_rect.x + layout->content_rect.w - layout->cursor.x);
  float remaining_h = s_ui_maxf(0.0f, layout->content_rect.y + layout->content_rect.h - layout->cursor.y);

  LDKUIRect bounding_rect = {0};
  bounding_rect.x = layout->cursor.x;
  bounding_rect.y = layout->cursor.y;
  bounding_rect.w = s_ui_resolve_size(width, parent_w, remaining_w);
  bounding_rect.h = s_ui_resolve_size(height, parent_h, remaining_h);

  rect = bounding_rect;

  if (!layout->is_scrollview || (layout->scroll_flags & LDK_UI_SCROLL_HORIZONTAL) == 0)
  {
    rect.w = s_ui_minf(rect.w, remaining_w);
  }

  if (!layout->is_scrollview || (layout->scroll_flags & LDK_UI_SCROLL_VERTICAL) == 0)
  {
    rect.h = s_ui_minf(rect.h, remaining_h);
  }

  s_ui_layout_expand_bounding_rect(layout, bounding_rect);

  ctx->last_rect = rect;
  ctx->last_bounding_rect = bounding_rect;
  ctx->last_measure_entry_index = s_ui_measure_entry_add(ctx, layout, bounding_rect);

  if (layout->direction == LDK_UI_LAYOUT_VERTICAL)
  {
    layout->cursor.y += bounding_rect.h + layout->spacing;
  }
  else
  {
    layout->cursor.x += bounding_rect.w + layout->spacing;
  }

  layout->item_count += 1;

  if (ctx->debug_draw)
  {
    LDKUIRect clip = s_ui_rect_intersect(&layout->content_rect, &rect);
    s_ui_debug_add_rect(ctx, 0, LDK_UI_DEBUG_RECT_ITEM, rect, clip);
  }

  return rect;
}

static LDKUILayout* s_ui_layout_push(LDKUIContext* ctx, LDKUILayoutDirection direction, LDKUIRect rect, LDKUIRect bounding_rect, u32 measure_entry_index)
{
  LDKUILayout* parent = ctx->current_layout;
  LDKUILayout* layout = x_arena_alloc_zero(ctx->frame_arena, sizeof(LDKUILayout));

  if (layout == NULL)
  {
    return NULL;
  }

  layout->id = s_ui_id_make_in_scope(
      ctx,
      direction == LDK_UI_LAYOUT_VERTICAL
      ? (u32)LDK_UI_ITEM_LAYOUT_VERTICAL
      : (u32)LDK_UI_ITEM_LAYOUT_HORIZONTAL,
      parent != NULL ? parent->item_count : ctx->root_item_count);
  layout->direction = direction;
  layout->rect = rect;
  layout->bounding_rect = bounding_rect;
  layout->padding = direction == LDK_UI_LAYOUT_VERTICAL ? LDK_UI_DEFAULT_PADDING : 0.0f;
  layout->spacing = LDK_UI_DEFAULT_SPACING;
  layout->parent = parent;
  layout->content_rect.x = rect.x + layout->padding;
  layout->content_rect.y = rect.y + layout->padding;
  layout->content_rect.w = s_ui_maxf(0.0f, rect.w - layout->padding * 2.0f);
  layout->content_rect.h = s_ui_maxf(0.0f, rect.h - layout->padding * 2.0f);
  layout->cursor.x = layout->content_rect.x;
  layout->cursor.y = layout->content_rect.y;
  layout->content_used_right = layout->content_rect.x;
  layout->content_used_bottom = layout->content_rect.y;
  s_ui_layout_expand_bounding_rect(layout, layout->bounding_rect);
  layout->measure_entry_index = measure_entry_index;
  layout->has_measure_entry = measure_entry_index != UINT32_MAX;

  ctx->current_layout = layout;

  if (ctx->debug_draw)
  {
    LDKUIRect clip;

    if (parent != NULL)
    {
      clip = s_ui_rect_intersect(&parent->content_rect, &rect);
    }
    else if (ctx->current_window != NULL)
    {
      clip = s_ui_rect_intersect(&ctx->current_window->rect, &rect);
    }
    else
    {
      clip = rect;
    }

    s_ui_debug_add_rect(ctx, layout->id, LDK_UI_DEBUG_RECT_LAYOUT, rect, clip);
  }

  return layout;
}

static void s_ui_layout_pop(LDKUIContext* ctx)
{
  if (ctx == NULL || ctx->current_layout == NULL)
  {
    return;
  }

  LDKUILayout* layout = ctx->current_layout;
  LDKUILayout* parent = layout->parent;

  if (layout->has_measure_entry)
  {
    s_ui_measure_entry_union_at(ctx, layout->measure_entry_index, layout->bounding_rect);
  }

  if (parent != NULL)
  {
    s_ui_layout_expand_bounding_rect(parent, layout->bounding_rect);
  }

  ctx->last_rect = layout->rect;
  ctx->last_bounding_rect = layout->bounding_rect;
  ctx->last_measure_entry_index = layout->measure_entry_index;
  ctx->current_layout = parent;
}

static LDKUIRect s_ui_current_clip_rect(LDKUIContext* ctx)
{
  LDKUIRect clip;
  LDKUILayout* layout;

  if (ctx == NULL)
  {
    return (LDKUIRect){0};
  }

  if (ctx->current_layout != NULL)
  {
    clip = ctx->current_layout->content_rect;
  }
  else if (ctx->current_window != NULL)
  {
    clip = ctx->current_window->content_rect;
  }
  else
  {
    clip = ctx->viewport;
  }

  layout = ctx->current_layout;

  while (layout != NULL)
  {
    if (layout->is_scrollview || layout->is_scrollview)
    {
      clip = s_ui_rect_intersect(&clip, &layout->scroll_clip_rect);
    }

    layout = layout->parent;
  }

  if (ctx->current_window != NULL)
  {
    clip = s_ui_rect_intersect(&clip, &ctx->current_window->rect);
  }

  return clip;
}

static void s_ui_add_hit_candidate(LDKUIContext* ctx, LDKUIId item_id, LDKUIRect rect, LDKUIRect clip_rect)
{
  LDKUIHitCandidate candidate;

  if (ctx == NULL || ctx->current_window == NULL || ctx->hit_candidates == NULL)
  {
    return;
  }

  candidate.window_id = ctx->current_window->id;
  candidate.item_id = item_id;
  candidate.rect = rect;
  candidate.clip_rect = clip_rect;
  candidate.order = ctx->hit_order;
  ctx->hit_order += 1;

  x_array_ldk_ui_hit_candidate_push(ctx->hit_candidates, candidate);
  s_ui_debug_add_rect(ctx, item_id, LDK_UI_DEBUG_RECT_ITEM, rect, clip_rect);
}

static void s_ui_resolve_hot_item(LDKUIContext* ctx)
{
  LDKPoint cursor;
  LDKUIWindow* hovered_window;
  LDKUIHitCandidate* best_candidate = NULL;
  u32 count;

  if (ctx == NULL || ctx->mouse == NULL)
  {
    ctx->next_hot_window_id = 0;
    ctx->next_hot_id = 0;
    return;
  }

  hovered_window = s_ui_window_topmost_at_cursor(ctx);

  if (hovered_window == NULL)
  {
    ctx->next_hot_window_id = 0;
    ctx->next_hot_id = 0;
    return;
  }

  cursor = ldk_os_mouse_cursor((LDKMouseState*)ctx->mouse);
  count = x_array_ldk_ui_hit_candidate_count(ctx->hit_candidates);

  for (u32 i = 0; i < count; ++i)
  {
    LDKUIHitCandidate* candidate = x_array_ldk_ui_hit_candidate_get(ctx->hit_candidates, i);

    if (candidate == NULL)
    {
      continue;
    }

    if (candidate->window_id != hovered_window->id)
    {
      continue;
    }

    if (!s_ui_rect_contains(&candidate->rect, (float)cursor.x, (float)cursor.y))
    {
      continue;
    }

    if (!s_ui_rect_contains(&candidate->clip_rect, (float)cursor.x, (float)cursor.y))
    {
      continue;
    }

    if (best_candidate == NULL || candidate->order > best_candidate->order)
    {
      best_candidate = candidate;
    }
  }

  ctx->next_hot_window_id = hovered_window->id;
  ctx->next_hot_id = best_candidate != NULL ? best_candidate->item_id : 0;
}

//----------------------------------------------------------
// Widget definition helpers
//----------------------------------------------------------

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

static bool s_ui_take_next_disabled(LDKUIContext* ctx)
{
  bool disabled = false;

  if (ctx == NULL)
  {
    return false;
  }

  disabled = ctx->next_disabled;
  ctx->next_disabled = false;

  bool const* parent_disabled = x_array_ldk_ui_bool_back(ctx->disabled_stack);

  if (parent_disabled != NULL && *parent_disabled)
  {
    disabled = true;
  }

  return disabled;
}

/**
 * Resolves a widget's frame rectangle and clip information for the current frame.
 *
 * This helper centralizes the repetitive setup shared by most widgets:
 *   - generating a scoped widget ID
 *   - selecting horizontal/vertical sizing behavior
 *   - advancing the current layout cursor
 *   - computing the final clip rect
 *   - optionally registering the widget for hit testing
 */
static bool s_ui_widget_frame_box(LDKUIContext* ctx, LDKUIWidgetBox* box, LDKUIItemType type,
    LDKUILayoutSize horizontal_width, LDKUILayoutSize vertical_width, LDKUILayoutSize height, bool hit_test)
{
  if (ctx == NULL || box == NULL || ctx->current_layout == NULL || ctx->current_window == NULL)
  {
    return false;
  }

  memset(box, 0, sizeof(*box));
  box->id = s_ui_id_make_in_scope(ctx, (u32)type, ctx->current_layout->item_count);
  LDKUILayoutSize width = vertical_width;

  if (ctx->current_layout->direction == LDK_UI_LAYOUT_HORIZONTAL)
  {
    width = horizontal_width;
  }

  box->rect = s_ui_layout_next_rect(ctx, width, height);
  LDKUIRect parent_clip = s_ui_current_clip_rect(ctx);
  box->clip = s_ui_rect_intersect(&parent_clip, &box->rect);
  box->disabled = s_ui_take_next_disabled(ctx);

  if (hit_test && !box->disabled)
  {
    s_ui_add_hit_candidate(ctx, box->id, box->rect, box->clip);
  }

  return true;
}

/**
 * Computes the interaction state for a widget during the current frame
 * by interpreting the global UI input state relative to a widget
 * rectangle and ID.
 * Or in simpler terms: "What happened to this widget during this frame?"
 *
 * IMPORTANT:
 *   - The widget ID must already be registered as a hit candidate
 *     before calling this function.
 */
static LDKUIFrameState s_ui_frame_state(LDKUIContext* ctx, LDKUIId id,
    LDKUIRect rect, LDKUIRect clip, bool focusable, bool disabled)
{
  LDKUIFrameState state = {0};

  state.id = id;
  state.rect = rect;
  state.clip = clip;
  state.disabled = disabled;

  if (ctx == NULL || ctx->current_window == NULL)
  {
    return state;
  }

  if (disabled)
  {
    if (ctx->active_window_id == ctx->current_window->id && ctx->active_id == id)
    {
      ctx->active_window_id = 0;
      ctx->active_id = 0;
    }

    if (ctx->focused_window_id == ctx->current_window->id && ctx->focused_id == id)
    {
      ctx->focused_window_id = 0;
      ctx->focused_id = 0;
      ctx->current_window->focused_id = 0;
    }

    if (ctx->text_box_id == id)
    {
      ctx->text_box_id = 0;
    }

    state.visual_state = LDK_UI_CONTROL_VISUAL_STATE_DISABLED;
    return state;
  }

  state.hot = ctx->hot_window_id == ctx->current_window->id && ctx->hot_id == id;
  state.active = ctx->active_window_id == ctx->current_window->id && ctx->active_id == id;
  state.focused = ctx->focused_window_id == ctx->current_window->id && ctx->focused_id == id;

  if (ctx->mouse != NULL)
  {
    LDKPoint cursor = ldk_os_mouse_cursor((LDKMouseState*)ctx->mouse);

    state.cursor.x = (float)cursor.x;
    state.cursor.y = (float)cursor.y;
    state.local_cursor.x = state.cursor.x - rect.x;
    state.local_cursor.y = state.cursor.y - rect.y;

    state.pressed = ldk_os_mouse_button_down((LDKMouseState*)ctx->mouse, LDK_MOUSE_BUTTON_LEFT);
    state.released = ldk_os_mouse_button_up((LDKMouseState*)ctx->mouse, LDK_MOUSE_BUTTON_LEFT);
    state.dragging = false;

    if (state.active)
    {
      state.dragging = ldk_os_mouse_button_is_pressed((LDKMouseState*)ctx->mouse, LDK_MOUSE_BUTTON_LEFT);
    }
  }

  if (state.hot && state.pressed)
  {
    ctx->active_window_id = ctx->current_window->id;
    ctx->active_id = id;

    if (focusable)
    {
      ctx->focused_window_id = ctx->current_window->id;
      ctx->focused_id = id;
      ctx->current_window->focused_id = id;
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

//----------------------------------------------------------
// Frame and lifecycle
//----------------------------------------------------------

bool ldk_ui_initialize(LDKUIContext* ctx, LDKUIConfig const* config)
{
  if (ctx == NULL || config == NULL)
  {
    return false;
  }

  memset(ctx, 0, sizeof(*ctx));

  ctx->frame_arena = x_arena_create(config->frame_arena_size);
  ctx->windows = x_array_ldk_ui_window_create(config->initial_window_capacity);
  ctx->id_stack = x_array_ldk_ui_id_create(config->initial_id_stack_capacity);
  ctx->vertices = x_array_ldk_ui_vertex_create(config->initial_vertex_capacity);
  ctx->indices = x_array_ldk_ui_u32_create(config->initial_index_capacity);
  ctx->commands = x_array_ldk_ui_draw_cmd_create(config->initial_command_capacity);
  ctx->disabled_stack = x_array_ldk_ui_bool_create(8);
  ctx->hit_candidates = x_array_ldk_ui_hit_candidate_create(128);
  ctx->debug_rects = x_array_ldk_ui_debug_rect_create(256);
  ctx->scroll_content_cache = x_array_ldk_ui_scroll_content_cache_create(64);
  ctx->measure_entries = x_array_ldk_ui_measure_entry_create(256);

  ctx->font_texture_user = config->font_texture_user;
  ctx->get_font_page_texture = config->get_font_page_texture;
  ctx->font = config->font;
  ctx->next_z_order = 1;

  ldk_ui_set_theme(ctx, config->theme, NULL);

  if (ctx->font != NULL)
  {
    ldk_ttf_preload_basic_ascii(ctx->font);
  }

  return ctx->frame_arena != NULL &&
    ctx->windows != NULL &&
    ctx->id_stack != NULL &&
    ctx->vertices != NULL &&
    ctx->indices != NULL &&
    ctx->commands != NULL &&
    ctx->hit_candidates != NULL &&
    ctx->debug_rects != NULL &&
    ctx->scroll_content_cache != NULL &&
    ctx->measure_entries != NULL;
}

void ldk_ui_terminate(LDKUIContext* ctx)
{
  u32 count = 0;

  if (ctx == NULL)
  {
    return;
  }

  ldk_os_cursor_type_set(LDK_CURSOR_ARROW);

  count = x_array_ldk_ui_window_count(ctx->windows);

  for (u32 i = 0; i < count; ++i)
  {
    LDKUIWindow* window = x_array_ldk_ui_window_get(ctx->windows, i);
    s_ui_window_destroy_buffers(window);
  }

  x_array_destroy(ctx->windows);
  x_array_destroy(ctx->id_stack);
  x_array_destroy(ctx->vertices);
  x_array_destroy(ctx->indices);
  x_array_destroy(ctx->commands);
  x_array_destroy(ctx->disabled_stack);
  x_array_destroy(ctx->hit_candidates);
  x_array_destroy(ctx->debug_rects);
  x_array_destroy(ctx->scroll_content_cache);
  x_array_destroy(ctx->measure_entries);
  x_arena_destroy(ctx->frame_arena);

  memset(ctx, 0, sizeof(*ctx));
}

void ldk_ui_begin_frame(LDKUIContext* ctx, LDKMouseState const* mouse, LDKKeyboardState const* keyboard, LDKUITextInputState const* text_input, LDKUIRect viewport)
{
  LDKUIWindow* hovered = NULL;

  if (ctx == NULL)
  {
    return;
  }

  ctx->frame_index += 1;
  ctx->mouse = mouse;
  ctx->keyboard = keyboard;
  ctx->input_text = text_input;
  ctx->viewport = viewport;
  ctx->root_item_count = 0;
  ctx->current_window = NULL;
  ctx->current_layout = NULL;
  ctx->next_disabled = false;
  ctx->next_focus = false;
  ctx->has_next_width = false;
  ctx->has_next_height = false;
  ctx->hot_id = ctx->next_hot_id;
  ctx->hot_window_id = ctx->next_hot_window_id;
  ctx->next_hot_id = 0;
  ctx->next_hot_window_id = 0;
  ctx->hit_order = 0;
  ctx->last_rect = (LDKUIRect){0};
  ctx->last_bounding_rect = (LDKUIRect){0};
  ctx->last_measure_entry_index = UINT32_MAX;
  ctx->mouse_wheel_consumed = false;

  hovered = s_ui_window_topmost_at_cursor(ctx);
  ctx->hovered_window_id = hovered != NULL ? hovered->id : 0;

  x_arena_reset_keep_head(ctx->frame_arena);
  x_array_ldk_ui_id_clear(ctx->id_stack);
  x_array_ldk_ui_bool_clear(ctx->disabled_stack);
  x_array_ldk_ui_vertex_clear(ctx->vertices);
  x_array_ldk_ui_u32_clear(ctx->indices);
  x_array_ldk_ui_draw_cmd_clear(ctx->commands);
  x_array_ldk_ui_hit_candidate_clear(ctx->hit_candidates);
  x_array_ldk_ui_debug_rect_clear(ctx->debug_rects);
  x_array_ldk_ui_measure_entry_clear(ctx->measure_entries);

  s_ui_windows_clear_frame_buffers(ctx);

  // set cursor type based on previous frame checks
  ldk_os_cursor_type_set(ctx->cursor_type);
  ctx->cursor_type = LDK_CURSOR_ARROW;
}

static void s_ui_append_window_draw_data(LDKUIContext* ctx, LDKUIWindow* window)
{
  u32 base_vertex = x_array_ldk_ui_vertex_count(ctx->vertices);
  u32 base_index = x_array_ldk_ui_u32_count(ctx->indices);
  u32 vertex_count;
  u32 index_count;
  u32 command_count;

  if (ctx == NULL || window == NULL)
  {
    return;
  }

  vertex_count = x_array_ldk_ui_vertex_count(window->vertices);
  index_count = x_array_ldk_ui_u32_count(window->indices);
  command_count = x_array_ldk_ui_draw_cmd_count(window->commands);

  for (u32 i = 0; i < vertex_count; ++i)
  {
    LDKUIVertex* vertex = x_array_ldk_ui_vertex_get(window->vertices, i);

    if (vertex != NULL)
    {
      x_array_ldk_ui_vertex_push(ctx->vertices, *vertex);
    }
  }

  for (u32 i = 0; i < index_count; ++i)
  {
    u32* index = x_array_ldk_ui_u32_get(window->indices, i);

    if (index != NULL)
    {
      x_array_ldk_ui_u32_push(ctx->indices, *index + base_vertex);
    }
  }

  for (u32 i = 0; i < command_count; ++i)
  {
    LDKUIDrawCmd* command = x_array_ldk_ui_draw_cmd_get(window->commands, i);

    if (command != NULL)
    {
      LDKUIDrawCmd adjusted = *command;
      adjusted.index_offset += base_index;
      x_array_ldk_ui_draw_cmd_push(ctx->commands, adjusted);
    }
  }
}

static void s_ui_submit_windows_in_z_order(LDKUIContext* ctx)
{
  u32 count = x_array_ldk_ui_window_count(ctx->windows);
  u8* used = NULL;

  if (count == 0)
  {
    return;
  }

  used = x_arena_alloc_zero(ctx->frame_arena, sizeof(u8) * count);

  if (used == NULL)
  {
    return;
  }

  for (u32 pass = 0; pass < count; ++pass)
  {
    u32 best_index = UINT32_MAX;
    i32 best_z = INT32_MAX;

    for (u32 i = 0; i < count; ++i)
    {
      LDKUIWindow* window = x_array_ldk_ui_window_get(ctx->windows, i);

      if (used[i] || window == NULL)
      {
        continue;
      }

      if (window->last_frame_seen != ctx->frame_index)
      {
        continue;
      }

      if (best_index == UINT32_MAX || window->z_order < best_z)
      {
        best_index = i;
        best_z = window->z_order;
      }
    }

    if (best_index == UINT32_MAX)
    {
      break;
    }

    used[best_index] = 1;
    s_ui_append_window_draw_data(ctx, x_array_ldk_ui_window_get(ctx->windows, best_index));
  }
}

void ldk_ui_end_frame(LDKUIContext* ctx)
{
  if (ctx == NULL)
  {
    return;
  }

  s_ui_resolve_hot_item(ctx);
  s_ui_debug_draw_rects(ctx);

  if (ctx->mouse != NULL)
  {
    if (ldk_os_mouse_button_up((LDKMouseState*)ctx->mouse, LDK_MOUSE_BUTTON_LEFT))
    {
      if (ctx->active_id != 0)
      {
        ctx->active_id = 0;
        ctx->active_window_id = 0;
      }

      ctx->dragging_window_id = 0;
      ctx->resizing_window_id = 0;
      ctx->resizing_window_edges = LDK_UI_WINDOW_RESIZE_NONE;
    }
  }

  s_ui_submit_windows_in_z_order(ctx);
  s_ui_scroll_content_cache_gc(ctx);
  s_ui_windows_gc(ctx);

  ctx->render_data.vertices = x_array_ldk_ui_vertex_data_const(ctx->vertices);
  ctx->render_data.vertex_count = x_array_ldk_ui_vertex_count(ctx->vertices);
  ctx->render_data.indices = x_array_ldk_ui_u32_data_const(ctx->indices);
  ctx->render_data.index_count = x_array_ldk_ui_u32_count(ctx->indices);
  ctx->render_data.commands = x_array_ldk_ui_draw_cmd_data_const(ctx->commands);
  ctx->render_data.command_count = x_array_ldk_ui_draw_cmd_count(ctx->commands);
}

LDKUIRenderData const* ldk_ui_get_render_data(LDKUIContext const* ctx)
{
  if (ctx == NULL)
  {
    return NULL;
  }

  return &ctx->render_data;
}

//----------------------------------------------------------
// Theme
//----------------------------------------------------------

void ldk_ui_set_theme(LDKUIContext* ctx, LDKUIThemeType type, LDKUITheme* custom)
{
  LDKUITheme* theme;

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
}

//----------------------------------------------------------
// IDs
//----------------------------------------------------------

void ldk_ui_push_id_u32(LDKUIContext* ctx, u32 value)
{
  if (ctx != NULL)
  {
    x_array_ldk_ui_id_push(ctx->id_stack, value);
  }
}

void ldk_ui_push_id_ptr(LDKUIContext* ctx, void const* value)
{
  uintptr_t raw = (uintptr_t)value;
  LDKUIId hashed = (LDKUIId)(raw ^ (raw >> 32));

  if (ctx != NULL)
  {
    x_array_ldk_ui_id_push(ctx->id_stack, hashed);
  }
}

void ldk_ui_push_id_cstr(LDKUIContext* ctx, char const* value)
{
  if (ctx != NULL)
  {
    x_array_ldk_ui_id_push(ctx->id_stack, s_ui_id_hash_cstr(2166136261u, value));
  }
}

void ldk_ui_pop_id(LDKUIContext* ctx)
{
  if (ctx != NULL && !x_array_ldk_ui_id_is_empty(ctx->id_stack))
  {
    x_array_ldk_ui_id_pop(ctx->id_stack);
  }
}

static u32 s_ui_window_resize_edges_at_cursor(LDKUIContext* ctx, LDKUIWindow const* window, LDKPoint cursor)
{
  u32 edges = LDK_UI_WINDOW_RESIZE_NONE;

  if (ctx == NULL || window == NULL)
  {
    return LDK_UI_WINDOW_RESIZE_NONE;
  }

  if ((window->flags & LDK_UI_WINDOW_RESIZABLE) == 0)
  {
    return LDK_UI_WINDOW_RESIZE_NONE;
  }

  float border_size = ctx->theme.window_interaction_border_size;
  float x = (float)cursor.x;
  float y = (float)cursor.y;

  if (!s_ui_rect_contains(&window->rect, x, y))
  {
    return LDK_UI_WINDOW_RESIZE_NONE;
  }

  if (x <= window->rect.x + border_size)
  {
    edges |= LDK_UI_WINDOW_RESIZE_LEFT;
  }

  if (x >= window->rect.x + window->rect.w - border_size)
  {
    edges |= LDK_UI_WINDOW_RESIZE_RIGHT;
  }

  if (y <= window->rect.y + border_size)
  {
    edges |= LDK_UI_WINDOW_RESIZE_TOP;
  }

  if (y >= window->rect.y + window->rect.h - border_size)
  {
    edges |= LDK_UI_WINDOW_RESIZE_BOTTOM;
  }

  return edges;
}

static LDKUIRect s_ui_window_apply_resize(LDKUIContext* ctx, LDKUIRect rect, LDKPoint cursor)
{
  LDKUIRect result = rect;
  float dx;
  float dy;
  u32 edges;

  if (ctx == NULL)
  {
    return result;
  }

  dx = (float)cursor.x - ctx->resize_start_cursor_x;
  dy = (float)cursor.y - ctx->resize_start_cursor_y;
  edges = ctx->resizing_window_edges;
  result = ctx->resize_start_rect;

  if ((edges & LDK_UI_WINDOW_RESIZE_LEFT) != 0)
  {
    result.x = ctx->resize_start_rect.x + dx;
    result.w = ctx->resize_start_rect.w - dx;

    if (result.w < LDK_UI_MIN_WINDOW_WIDTH)
    {
      result.x = ctx->resize_start_rect.x + ctx->resize_start_rect.w - LDK_UI_MIN_WINDOW_WIDTH;
      result.w = LDK_UI_MIN_WINDOW_WIDTH;
    }
  }

  if ((edges & LDK_UI_WINDOW_RESIZE_RIGHT) != 0)
  {
    result.w = ctx->resize_start_rect.w + dx;

    if (result.w < LDK_UI_MIN_WINDOW_WIDTH)
    {
      result.w = LDK_UI_MIN_WINDOW_WIDTH;
    }
  }

  if ((edges & LDK_UI_WINDOW_RESIZE_TOP) != 0)
  {
    result.y = ctx->resize_start_rect.y + dy;
    result.h = ctx->resize_start_rect.h - dy;

    if (result.h < LDK_UI_MIN_WINDOW_HEIGHT)
    {
      result.y = ctx->resize_start_rect.y + ctx->resize_start_rect.h - LDK_UI_MIN_WINDOW_HEIGHT;
      result.h = LDK_UI_MIN_WINDOW_HEIGHT;
    }
  }

  if ((edges & LDK_UI_WINDOW_RESIZE_BOTTOM) != 0)
  {
    result.h = ctx->resize_start_rect.h + dy;

    if (result.h < LDK_UI_MIN_WINDOW_HEIGHT)
    {
      result.h = LDK_UI_MIN_WINDOW_HEIGHT;
    }
  }

  return result;
}

static u32 s_ui_window_border_color(LDKUIContext* ctx, LDKUIWindow const* window)
{
  if (ctx == NULL || window == NULL)
  {
    return 0xffffffffu;
  }

  if (ctx->focused_window_id == window->id)
  {
    return ctx->theme.colors[LDK_UI_COLOR_FOCUS];
  }

  if (ctx->hovered_window_id == window->id)
  {
    return ctx->theme.colors[LDK_UI_COLOR_CONTROL_BORDER_HOVERED];
  }

  return ctx->theme.colors[LDK_UI_COLOR_BORDER];
}

//----------------------------------------------------------
// Windows
//----------------------------------------------------------

LDKUIRect ldk_ui_begin_window(LDKUIContext* ctx, char const* title, LDKUIRect rect, u32 flags)
{
  LDKUIId id;
  LDKUIWindow* window;
  LDKUIRect out_rect = rect;
  LDKPoint cursor;
  bool inside_window;
  bool inside_title;
  bool has_title_bar;
  bool can_drag_from_background;
  bool mouse_down;
  bool mouse_pressed;
  u32 resize_edges;
  float title_h;
  u32 effective_flags;

  if (ctx == NULL)
  {
    return out_rect;
  }

  effective_flags = flags;

  if ((effective_flags & LDK_UI_WINDOW_RESIZABLE) != 0)
  {
    effective_flags |= LDK_UI_WINDOW_BORDER;
  }

  id = s_ui_id_make_window(ctx, ctx->root_item_count);
  window = s_ui_window_get_or_create(ctx, id, title);

  if (window == NULL)
  {
    return out_rect;
  }

  if (title != NULL)
  {
    strncpy(window->title, title, sizeof(window->title) - 1);
    window->title[sizeof(window->title) - 1] = 0;
  }

  cursor = (LDKPoint){0};
  mouse_down = false;
  mouse_pressed = false;

  if (ctx->mouse != NULL)
  {
    cursor = ldk_os_mouse_cursor((LDKMouseState*)ctx->mouse);
    mouse_down = ldk_os_mouse_button_down((LDKMouseState*)ctx->mouse, LDK_MOUSE_BUTTON_LEFT);
    mouse_pressed = ldk_os_mouse_button_is_pressed((LDKMouseState*)ctx->mouse, LDK_MOUSE_BUTTON_LEFT);
  }

  has_title_bar = (effective_flags & LDK_UI_WINDOW_TITLE_BAR) != 0;
  title_h = has_title_bar ? LDK_UI_TITLE_BAR_HEIGHT : 0.0f;

  window->flags = effective_flags;
  window->last_frame_seen = ctx->frame_index;
  window->rect = out_rect;
  window->title_bar_rect = (LDKUIRect){ out_rect.x, out_rect.y, out_rect.w, title_h };
  window->content_rect = (LDKUIRect){ out_rect.x, out_rect.y + title_h, out_rect.w, s_ui_maxf(0.0f, out_rect.h - title_h) };

  inside_window = ctx->hovered_window_id == window->id &&
    s_ui_rect_contains(&window->rect, (float)cursor.x, (float)cursor.y);
  inside_title = title_h > 0.0f &&
    s_ui_rect_contains(&window->title_bar_rect, (float)cursor.x, (float)cursor.y);

  resize_edges = s_ui_window_resize_edges_at_cursor(ctx, window, cursor);

  // change cursor based on resize_edge
  //if (resize_edges)
  {
    if (resize_edges == (LDK_UI_WINDOW_RESIZE_TOP | LDK_UI_WINDOW_RESIZE_LEFT) ||
        (resize_edges == (LDK_UI_WINDOW_RESIZE_BOTTOM | LDK_UI_WINDOW_RESIZE_RIGHT)))
    {
      ctx->cursor_type = LDK_CURSOR_SIZE_NWSE;
    }

    if (resize_edges == (LDK_UI_WINDOW_RESIZE_TOP | LDK_UI_WINDOW_RESIZE_RIGHT) ||
        (resize_edges == (LDK_UI_WINDOW_RESIZE_BOTTOM | LDK_UI_WINDOW_RESIZE_LEFT)))
    {
      ctx->cursor_type = LDK_CURSOR_SIZE_NESW;
    }

    else if ((resize_edges == LDK_UI_WINDOW_RESIZE_TOP) || (resize_edges == LDK_UI_WINDOW_RESIZE_BOTTOM))
    {
      ctx->cursor_type = LDK_CURSOR_SIZE_NS;
    }
    else if ((resize_edges == LDK_UI_WINDOW_RESIZE_LEFT) || (resize_edges == LDK_UI_WINDOW_RESIZE_RIGHT))
    {
      ctx->cursor_type = LDK_CURSOR_SIZE_WE;
    }
  }


  if (inside_window && mouse_down)
  {
    s_ui_window_bring_to_front(ctx, window);
  }

  if (inside_window && mouse_down && resize_edges != LDK_UI_WINDOW_RESIZE_NONE)
  {
    ctx->resizing_window_id = window->id;
    ctx->resizing_window_edges = resize_edges;
    ctx->resize_start_cursor_x = (float)cursor.x;
    ctx->resize_start_cursor_y = (float)cursor.y;
    ctx->resize_start_rect = out_rect;
    ctx->dragging_window_id = 0;
    s_ui_window_bring_to_front(ctx, window);
  }

  can_drag_from_background = !has_title_bar &&
    (ctx->hot_window_id != window->id || ctx->hot_id == 0);

  if (ctx->resizing_window_id == 0 &&
      (effective_flags & LDK_UI_WINDOW_DRAGGABLE) != 0 &&
      inside_window &&
      mouse_down &&
      (inside_title || can_drag_from_background))
  {
    ctx->dragging_window_id = window->id;
    ctx->drag_x = (float)cursor.x - out_rect.x;
    ctx->drag_y = (float)cursor.y - out_rect.y;
    s_ui_window_bring_to_front(ctx, window);
  }

  if (ctx->resizing_window_id == window->id && ctx->mouse != NULL)
  {
    if (mouse_pressed)
    {
      out_rect = s_ui_window_apply_resize(ctx, out_rect, cursor);
    }
    else
    {
      ctx->resizing_window_id = 0;
      ctx->resizing_window_edges = LDK_UI_WINDOW_RESIZE_NONE;
    }
  }

  if (ctx->dragging_window_id == window->id && ctx->mouse != NULL)
  {
    if (mouse_pressed)
    {
      out_rect.x = (float)cursor.x - ctx->drag_x;
      out_rect.y = (float)cursor.y - ctx->drag_y;
    }
    else
    {
      ctx->dragging_window_id = 0;
    }
  }

  window->rect = out_rect;
  window->title_bar_rect = (LDKUIRect){ out_rect.x, out_rect.y, out_rect.w, title_h };
  window->content_rect = (LDKUIRect){ out_rect.x, out_rect.y + title_h, out_rect.w, s_ui_maxf(0.0f, out_rect.h - title_h) };

  ctx->current_window = window;
  ctx->root_item_count += 1;

  if ((effective_flags & LDK_UI_WINDOW_NO_BG) == 0)
  {
    s_ui_render_quad(ctx, window->rect, ctx->theme.colors[LDK_UI_COLOR_WINDOW_BG], window->rect, 0);
  }

  if ((effective_flags & LDK_UI_WINDOW_TITLE_BAR) != 0)
  {
    u32 title_bg = ctx->focused_window_id == window->id
      ? ctx->theme.colors[LDK_UI_COLOR_TITLE_BAR_FOCUSED]
      : ctx->theme.colors[LDK_UI_COLOR_TITLE_BAR];
    float line_h = ctx->font != NULL ? ldk_ttf_get_line_height(ctx->font) : LDK_UI_DEFAULT_CONTROL_HEIGHT;
    float text_x = window->title_bar_rect.x + 6.0f;
    float text_y = window->title_bar_rect.y + (window->title_bar_rect.h - line_h) * 0.5f;

    s_ui_render_quad(ctx, window->title_bar_rect, title_bg, window->rect, 0);
    s_ui_render_text(ctx, window->title, text_x, text_y, ctx->theme.colors[LDK_UI_COLOR_TEXT], window->title_bar_rect);
  }

  if ((effective_flags & LDK_UI_WINDOW_BORDER) != 0)
  {
    s_ui_render_border(ctx, window->rect, ctx->theme.window_border_size,
        s_ui_window_border_color(ctx, window), window->rect);
  }

  s_ui_layout_push(ctx, LDK_UI_LAYOUT_VERTICAL, window->content_rect, window->content_rect, UINT32_MAX);

  return out_rect;
}

void ldk_ui_end_window(LDKUIContext* ctx)
{
  if (ctx == NULL)
  {
    return;
  }

  while (ctx->current_layout != NULL)
  {
    s_ui_layout_pop(ctx);
  }

  ctx->current_window = NULL;
}

//----------------------------------------------------------
// Layout
//----------------------------------------------------------

void ldk_ui_begin_vertical(LDKUIContext* ctx, LDKUILayoutSize width, LDKUILayoutSize height)
{
  LDKUIRect rect;

  if (ctx == NULL || ctx->current_layout == NULL)
  {
    return;
  }

  rect = s_ui_layout_next_rect(ctx, width, height);
  s_ui_layout_push(ctx, LDK_UI_LAYOUT_VERTICAL, rect, ctx->last_bounding_rect, ctx->last_measure_entry_index);
}

void ldk_ui_end_vertical(LDKUIContext* ctx)
{
  s_ui_layout_pop(ctx);
}

void ldk_ui_begin_horizontal(LDKUIContext* ctx, LDKUILayoutSize width, LDKUILayoutSize height)
{
  LDKUIRect rect;

  if (ctx == NULL || ctx->current_layout == NULL)
  {
    return;
  }

  rect = s_ui_layout_next_rect(ctx, width, height);
  s_ui_layout_push(ctx, LDK_UI_LAYOUT_HORIZONTAL, rect, ctx->last_bounding_rect, ctx->last_measure_entry_index);
}

void ldk_ui_end_horizontal(LDKUIContext* ctx)
{
  s_ui_layout_pop(ctx);
}


//----------------------------------------------------------
// Disabling controls
//----------------------------------------------------------

void ldk_ui_set_next_disabled(LDKUIContext* ctx, bool disabled)
{
  if (ctx == NULL)
  {
    return;
  }

  ctx->next_disabled = disabled;
}

void ldk_ui_begin_disabled(LDKUIContext* ctx, bool disabled)
{
  if (ctx == NULL)
  {
    return;
  }

  bool effective_disabled = disabled;
  bool const* parent_disabled = x_array_ldk_ui_bool_back(ctx->disabled_stack);

  if (parent_disabled != NULL && *parent_disabled)
  {
    effective_disabled = true;
  }

  x_array_ldk_ui_bool_push(ctx->disabled_stack, effective_disabled);
}

void ldk_ui_end_disabled(LDKUIContext* ctx)
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


//----------------------------------------------------------
// Scrollview
//----------------------------------------------------------

LDKUIPoint ldk_ui_begin_scrollview(LDKUIContext* ctx, LDKUIPoint scroll_pos, LDKUIRect content_rect, LDKUIScrollFlags flags)
{
  if (ctx == NULL || ctx->current_layout == NULL || ctx->current_window == NULL)
  {
    return scroll_pos;
  }

  float scrollbar_size = 12.0f;
  float scrollbar_padding = 1.0f;
  LDKUIId id = s_ui_id_make_in_scope(ctx, (u32)LDK_UI_ITEM_SCROLLVIEW, ctx->current_layout->item_count);
  LDKUIScrollContentCache* cache = s_ui_scroll_content_cache_get_or_create(ctx, id);
  LDKUIPoint scroll = scroll_pos;
  LDKUIRect rect = s_ui_layout_next_rect(ctx, ldk_ui_fill(), ldk_ui_fill());

  if (cache != NULL && cache->has_scroll)
  {
    scroll = cache->scroll;
  }
  LDKUIRect parent_clip = s_ui_current_clip_rect(ctx);
  bool allow_vertical = (flags & LDK_UI_SCROLL_VERTICAL) != 0;
  bool allow_horizontal = (flags & LDK_UI_SCROLL_HORIZONTAL) != 0;
  bool scroll_if_needed = (flags & LDK_UI_SCROLL_IF_NEEDED) != 0;
  LDKUISize content_size;
  content_size.w = allow_horizontal ? s_ui_maxf(content_rect.w, rect.w) : rect.w;
  content_size.h = allow_vertical ? s_ui_maxf(content_rect.h, rect.h) : rect.h;

  bool need_vertical = false;
  bool need_horizontal = false;
  LDKUIRect view_rect = rect;

  if (allow_vertical)
  {
    need_vertical = scroll_if_needed ? content_size.h > view_rect.h : true;
  }

  if (need_vertical)
  {
    view_rect.w = s_ui_maxf(0.0f, view_rect.w - scrollbar_size - scrollbar_padding);
  }

  if (allow_horizontal)
  {
    need_horizontal = scroll_if_needed ? content_size.w > view_rect.w : true;
  }

  if (need_horizontal)
  {
    view_rect.h = s_ui_maxf(0.0f, view_rect.h - scrollbar_size - scrollbar_padding);
  }

  if (allow_vertical && !need_vertical && content_size.h > view_rect.h)
  {
    need_vertical = true;
    view_rect.w = s_ui_maxf(0.0f, view_rect.w - scrollbar_size - scrollbar_padding);
  }

  if (allow_horizontal && !need_horizontal && content_size.w > view_rect.w)
  {
    need_horizontal = true;
    view_rect.h = s_ui_maxf(0.0f, view_rect.h - scrollbar_size - scrollbar_padding);
  }

  LDKUIRect clip = s_ui_rect_intersect(&parent_clip, &view_rect);
  LDKUIRect scrollbar_clip = s_ui_rect_intersect(&parent_clip, &rect);
  float max_x = s_ui_maxf(0.0f, content_size.w - view_rect.w);
  float max_y = s_ui_maxf(0.0f, content_size.h - view_rect.h);
  scroll.x = s_ui_clampf(scroll.x, 0.0f, max_x);
  scroll.y = s_ui_clampf(scroll.y, 0.0f, max_y);

  LDKUIRect vertical_track_rect = {0};
  LDKUIRect vertical_thumb_rect = {0};
  LDKUIRect horizontal_track_rect = {0};
  LDKUIRect horizontal_thumb_rect = {0};
  bool has_vertical_scrollbar = false;
  bool has_horizontal_scrollbar = false;

  if (need_vertical)
  {
    LDKUIRect vertical_scrollbar_rect;
    vertical_scrollbar_rect.x = view_rect.x + view_rect.w + scrollbar_padding;
    vertical_scrollbar_rect.y = view_rect.y;
    vertical_scrollbar_rect.w = scrollbar_size;
    vertical_scrollbar_rect.h = view_rect.h;
    has_vertical_scrollbar = s_ui_scrollbar_rects(vertical_scrollbar_rect, view_rect.h, content_size.h, scroll.y, false, &vertical_track_rect, &vertical_thumb_rect);
  }

  if (need_horizontal)
  {
    LDKUIRect horizontal_scrollbar_rect;
    horizontal_scrollbar_rect.x = view_rect.x;
    horizontal_scrollbar_rect.y = view_rect.y + view_rect.h + scrollbar_padding;
    horizontal_scrollbar_rect.w = view_rect.w;
    horizontal_scrollbar_rect.h = scrollbar_size;
    has_horizontal_scrollbar = s_ui_scrollbar_rects(horizontal_scrollbar_rect, view_rect.w, content_size.w, scroll.x, true, &horizontal_track_rect, &horizontal_thumb_rect);
  }

  if (ctx->mouse != NULL && ctx->hovered_window_id == ctx->current_window->id)
  {
    LDKPoint cursor = ldk_os_mouse_cursor((LDKMouseState*)ctx->mouse);

    scroll.x = s_ui_clampf(scroll.x, 0.0f, max_x);
    scroll.y = s_ui_clampf(scroll.y, 0.0f, max_y);

    if (has_vertical_scrollbar)
    {
      LDKUIId thumb_id = s_ui_id_hash_u32(id, LDK_UI_SCROLLBAR_THUMB_Y_ID);
      bool inside_thumb = s_ui_rect_contains(&vertical_thumb_rect, (float)cursor.x, (float)cursor.y);

      if (inside_thumb && ldk_os_mouse_button_down((LDKMouseState*)ctx->mouse, LDK_MOUSE_BUTTON_LEFT))
      {
        ctx->active_window_id = ctx->current_window->id;
        ctx->active_id = thumb_id;
        ctx->focused_window_id = ctx->current_window->id;
        ctx->focused_id = thumb_id;
        ctx->scrollbar_drag_offset_y = (float)cursor.y - vertical_thumb_rect.y;
      }

      if (ctx->active_window_id == ctx->current_window->id && ctx->active_id == thumb_id)
      {
        if (ldk_os_mouse_button_is_pressed((LDKMouseState*)ctx->mouse, LDK_MOUSE_BUTTON_LEFT))
        {
          float thumb_range = s_ui_maxf(0.0f, vertical_track_rect.h - vertical_thumb_rect.h);
          float local = (float)cursor.y - vertical_track_rect.y - ctx->scrollbar_drag_offset_y;
          float t = thumb_range > 0.0f ? s_ui_clampf(local / thumb_range, 0.0f, 1.0f) : 0.0f;
          scroll.y = t * max_y;
        }
      }
    }

    if (has_horizontal_scrollbar)
    {
      LDKUIId thumb_id = s_ui_id_hash_u32(id, LDK_UI_SCROLLBAR_THUMB_X_ID);
      bool inside_thumb = s_ui_rect_contains(&horizontal_thumb_rect, (float)cursor.x, (float)cursor.y);

      if (inside_thumb && ldk_os_mouse_button_down((LDKMouseState*)ctx->mouse, LDK_MOUSE_BUTTON_LEFT))
      {
        ctx->active_window_id = ctx->current_window->id;
        ctx->active_id = thumb_id;
        ctx->focused_window_id = ctx->current_window->id;
        ctx->focused_id = thumb_id;
        ctx->scrollbar_drag_offset_x = (float)cursor.x - horizontal_thumb_rect.x;
      }

      if (ctx->active_window_id == ctx->current_window->id && ctx->active_id == thumb_id)
      {
        if (ldk_os_mouse_button_is_pressed((LDKMouseState*)ctx->mouse, LDK_MOUSE_BUTTON_LEFT))
        {
          float thumb_range = s_ui_maxf(0.0f, horizontal_track_rect.w - horizontal_thumb_rect.w);
          float local = (float)cursor.x - horizontal_track_rect.x - ctx->scrollbar_drag_offset_x;
          float t = thumb_range > 0.0f ? s_ui_clampf(local / thumb_range, 0.0f, 1.0f) : 0.0f;
          scroll.x = t * max_x;
        }
      }
    }
  }

  scroll.x = s_ui_clampf(scroll.x, 0.0f, max_x);
  scroll.y = s_ui_clampf(scroll.y, 0.0f, max_y);
  vertical_track_rect = (LDKUIRect){0};
  vertical_thumb_rect = (LDKUIRect){0};
  horizontal_track_rect = (LDKUIRect){0};
  horizontal_thumb_rect = (LDKUIRect){0};
  has_vertical_scrollbar = false;
  has_horizontal_scrollbar = false;

  if (need_vertical)
  {
    LDKUIRect vertical_scrollbar_rect;
    vertical_scrollbar_rect.x = view_rect.x + view_rect.w - scrollbar_padding;
    vertical_scrollbar_rect.y = view_rect.y;
    vertical_scrollbar_rect.w = scrollbar_size;
    vertical_scrollbar_rect.h = view_rect.h;
    has_vertical_scrollbar = s_ui_scrollbar_rects(vertical_scrollbar_rect, view_rect.h, content_size.h, scroll.y, false, &vertical_track_rect, &vertical_thumb_rect);
  }

  if (need_horizontal)
  {
    LDKUIRect horizontal_scrollbar_rect;
    horizontal_scrollbar_rect.x = view_rect.x;
    horizontal_scrollbar_rect.y = view_rect.y + view_rect.h + scrollbar_padding;
    horizontal_scrollbar_rect.w = view_rect.w;
    horizontal_scrollbar_rect.h = scrollbar_size;
    has_horizontal_scrollbar = s_ui_scrollbar_rects(horizontal_scrollbar_rect, view_rect.w, content_size.w, scroll.x, true, &horizontal_track_rect, &horizontal_thumb_rect);
  }

  LDKUILayout* parent = ctx->current_layout;
  LDKUILayout* layout = x_arena_alloc_zero(ctx->frame_arena, sizeof(LDKUILayout));

  if (layout == NULL)
  {
    return scroll;
  }

  layout->id = id;
  layout->direction = LDK_UI_LAYOUT_VERTICAL;
  layout->rect = rect;
  layout->bounding_rect = rect;
  layout->padding = 0.0f;
  layout->spacing = LDK_UI_DEFAULT_SPACING;
  layout->parent = parent;
  layout->is_scrollview = true;
  layout->scrollview_id = id;
  layout->scroll = scroll;
  layout->scroll_flags = flags;
  layout->scroll_view_rect = view_rect;
  layout->scroll_clip_rect = clip;
  layout->scrollbar_clip_rect = scrollbar_clip;
  layout->vertical_scrollbar_track_rect = vertical_track_rect;
  layout->vertical_scrollbar_thumb_rect = vertical_thumb_rect;
  layout->horizontal_scrollbar_track_rect = horizontal_track_rect;
  layout->horizontal_scrollbar_thumb_rect = horizontal_thumb_rect;
  layout->has_vertical_scrollbar = has_vertical_scrollbar;
  layout->has_horizontal_scrollbar = has_horizontal_scrollbar;
  layout->has_measure_entry = false;
  layout->measure_entry_index = UINT32_MAX;
  layout->content_rect.x = view_rect.x - scroll.x;
  layout->content_rect.y = view_rect.y - scroll.y;
  layout->content_rect.w = s_ui_maxf(content_size.w, view_rect.w);
  layout->content_rect.h = s_ui_maxf(content_size.h, view_rect.h);
  layout->cursor.x = layout->content_rect.x;
  layout->cursor.y = layout->content_rect.y;
  layout->content_used_right = layout->content_rect.x;
  layout->content_used_bottom = layout->content_rect.y;

  ctx->current_layout = layout;

  if (ctx->debug_draw)
  {
    s_ui_debug_add_rect(ctx, id, LDK_UI_DEBUG_RECT_LAYOUT, rect, clip);
  }

  return scroll;
}

void ldk_ui_end_scrollview(LDKUIContext* ctx)
{
  if (ctx == NULL || ctx->current_layout == NULL)
  {
    return;
  }

  LDKUILayout* layout = ctx->current_layout;

  if (!layout->is_scrollview)
  {
    return;
  }

  if (!ctx->mouse_wheel_consumed && ctx->mouse != NULL && ctx->hovered_window_id == ctx->current_window->id)
  {
    LDKPoint cursor = ldk_os_mouse_cursor((LDKMouseState*)ctx->mouse);
    bool cursor_in_scrollview = s_ui_rect_contains(&layout->rect, (float)cursor.x, (float)cursor.y);

    if (cursor_in_scrollview)
    {
      i32 wheel_delta = ldk_os_mouse_wheel_delta((LDKMouseState*)ctx->mouse);

      if (wheel_delta != 0 && layout->has_vertical_scrollbar)
      {
        float max_y = s_ui_maxf(0.0f, layout->content_rect.h - layout->scroll_view_rect.h);
        layout->scroll.y -= ((float)wheel_delta / 120.0f) * 32.0f;
        layout->scroll.y = s_ui_clampf(layout->scroll.y, 0.0f, max_y);
        ctx->mouse_wheel_consumed = true;
      }
    }
  }

  if (layout->has_vertical_scrollbar)
  {
    LDKUIRect vertical_scrollbar_rect = {
      layout->scroll_view_rect.x + layout->scroll_view_rect.w - 1.0f,
      layout->scroll_view_rect.y,
      12.0f,
      layout->scroll_view_rect.h
    };

    s_ui_scrollbar_rects(vertical_scrollbar_rect, layout->scroll_view_rect.h, layout->content_rect.h, layout->scroll.y, false, &layout->vertical_scrollbar_track_rect, &layout->vertical_scrollbar_thumb_rect);
  }

  if (layout->has_horizontal_scrollbar)
  {
    LDKUIRect horizontal_scrollbar_rect = {
      layout->scroll_view_rect.x,
      layout->scroll_view_rect.y + layout->scroll_view_rect.h + 1.0f,
      layout->scroll_view_rect.w,
      12.0f
    };

    s_ui_scrollbar_rects(horizontal_scrollbar_rect, layout->scroll_view_rect.w, layout->content_rect.w, layout->scroll.x, true, &layout->horizontal_scrollbar_track_rect, &layout->horizontal_scrollbar_thumb_rect);
  }

  {
    LDKUIScrollContentCache* cache = s_ui_scroll_content_cache_get_or_create(ctx, layout->id);

    if (cache != NULL)
    {
      cache->scroll = layout->scroll;
      cache->content_rect = layout->bounding_rect;
      cache->has_scroll = true;
    }
  }

  if (layout->has_vertical_scrollbar)
  {
    s_ui_render_quad(ctx, layout->vertical_scrollbar_track_rect, ctx->theme.colors[LDK_UI_COLOR_SCROLLBAR_TRACK], layout->scrollbar_clip_rect, 0);
    s_ui_render_quad(ctx, layout->vertical_scrollbar_thumb_rect, ctx->theme.colors[LDK_UI_COLOR_SCROLLBAR_THUMB], layout->scrollbar_clip_rect, 0);
  }

  if (layout->has_horizontal_scrollbar)
  {
    s_ui_render_quad(ctx, layout->horizontal_scrollbar_track_rect, ctx->theme.colors[LDK_UI_COLOR_SCROLLBAR_TRACK], layout->scrollbar_clip_rect, 0);
    s_ui_render_quad(ctx, layout->horizontal_scrollbar_thumb_rect, ctx->theme.colors[LDK_UI_COLOR_SCROLLBAR_THUMB], layout->scrollbar_clip_rect, 0);
  }

  if (layout->has_measure_entry)
  {
    s_ui_measure_entry_union_at(ctx, layout->measure_entry_index, layout->rect);
  }

  if (layout->parent != NULL)
  {
    s_ui_layout_expand_bounding_rect(layout->parent, layout->rect);
  }

  ctx->last_rect = layout->rect;
  ctx->last_bounding_rect = layout->rect;
  ctx->last_measure_entry_index = layout->measure_entry_index;
  ctx->current_layout = layout->parent;
}

//----------------------------------------------------------
// Widgets
//----------------------------------------------------------

void ldk_ui_label(LDKUIContext* ctx, char const* text)
{
  if (ctx == NULL || ctx->current_layout == NULL)
  {
    return;
  }

  LDKUILayout* layout = ctx->current_layout;
  float available_w = s_ui_maxf(0.0f, layout->content_rect.x + layout->content_rect.w - layout->cursor.x);
  LDKUISize text_size = s_ui_text_measure_wrapped(ctx->font, text, available_w);

  LDKUIWidgetBox box;
  if (!s_ui_widget_frame_box(
        ctx,
        &box,
        LDK_UI_ITEM_LABEL,
        ldk_ui_px(text_size.w + 4.0f),
        ldk_ui_fill(),
        ldk_ui_px(s_ui_maxf(LDK_UI_DEFAULT_CONTROL_HEIGHT, text_size.h)),
        false))
  {
    return;
  }

  float text_y = box.rect.y + (box.rect.h - text_size.h) * 0.5f;

  s_ui_render_text_wrapped(
      ctx,
      text,
      box.rect.x,
      text_y,
      box.rect.w,
      ctx->theme.colors[LDK_UI_COLOR_TEXT],
      box.clip);
}

bool ldk_ui_button(LDKUIContext* ctx, char const* text)
{
  if (ctx == NULL || ctx->current_layout == NULL || ctx->current_window == NULL)
  {
    return false;
  }

  LDKUISize text_size = ldk_ttf_measure_text_cstr(ctx->font, text);

  LDKUIWidgetBox box;
  if (!s_ui_widget_frame_box(ctx, &box, LDK_UI_ITEM_BUTTON, ldk_ui_px(text_size.w + 16.0f), ldk_ui_fill(),
        ldk_ui_px(LDK_UI_DEFAULT_CONTROL_HEIGHT), true))
  {
    return false;
  }

  LDKUIFrameState frame = s_ui_frame_state(ctx, box.id, box.rect, box.clip, true, box.disabled);

  u32 bg = s_ui_render_control_bg_color(ctx, frame.visual_state);
  u32 border = s_ui_render_control_border_color(ctx, frame.visual_state);
  u32 text_color = s_ui_render_control_text_color(ctx, frame.visual_state);

  s_ui_render_quad(ctx, box.rect, bg, box.clip, 0);
  s_ui_render_border(ctx, box.rect, ctx->theme.control_border_size, border, box.clip);

  float text_x = box.rect.x + (box.rect.w - text_size.w) * 0.5f;
  float text_y = box.rect.y + (box.rect.h - text_size.h) * 0.5f;

  s_ui_render_text(ctx, text, text_x, text_y, text_color, box.clip);

  return frame.clicked;
}

bool ldk_ui_button_flat(LDKUIContext* ctx, char const* text)
{
  if (ctx == NULL || ctx->current_layout == NULL || ctx->current_window == NULL)
  {
    return false;
  }

  LDKUISize text_size = ldk_ttf_measure_text_cstr(ctx->font, text);

  LDKUIWidgetBox box;
  if (!s_ui_widget_frame_box(ctx, &box, LDK_UI_ITEM_BUTTON, ldk_ui_px(text_size.w + 16.0f), ldk_ui_fill(),
        ldk_ui_px(LDK_UI_DEFAULT_CONTROL_HEIGHT), true))
  {
    return false;
  }

  LDKUIFrameState frame = s_ui_frame_state(ctx, box.id, box.rect, box.clip, true, box.disabled);

  u32 text_color = s_ui_render_control_text_color(ctx, frame.visual_state);

  if (frame.visual_state == LDK_UI_CONTROL_VISUAL_STATE_HOVERED ||
      frame.visual_state == LDK_UI_CONTROL_VISUAL_STATE_ACTIVE ||
      frame.visual_state == LDK_UI_CONTROL_VISUAL_STATE_ACTIVE_HOVERED)
  {
    u32 bg = s_ui_render_control_bg_color(ctx, frame.visual_state);
    s_ui_render_quad(ctx, box.rect, bg, box.clip, 0);
  }

  float text_x = box.rect.x + (box.rect.w - text_size.w) * 0.5f;
  float text_y = box.rect.y + (box.rect.h - text_size.h) * 0.5f;

  s_ui_render_text(ctx, text, text_x, text_y, text_color, box.clip);

  return frame.clicked;
}

float ldk_ui_slider(LDKUIContext* ctx, float value, float min_value, float max_value)
{
  if (ctx == NULL || ctx->current_layout == NULL || ctx->current_window == NULL)
  {
    return value;
  }

  LDKUIWidgetBox box;
  if (!s_ui_widget_frame_box(ctx, &box, LDK_UI_ITEM_SLIDER, ldk_ui_px(140.0f), ldk_ui_fill(),
        ldk_ui_px(LDK_UI_DEFAULT_CONTROL_HEIGHT), true))
  {
    return value;
  }

  LDKUIFrameState frame = s_ui_frame_state(ctx, box.id, box.rect, box.clip, true, box.disabled);

  float base_height = s_ui_maxf(box.rect.h, 1.0f);
  float track_height_factor = s_ui_clampf(ctx->theme.slider_track_height, 0.0f, 1.0f);
  float thumb_width_factor = s_ui_clampf(ctx->theme.slider_thumb_width, 0.0f, 1.0f);

  float track_height = s_ui_maxf(1.0f, base_height * track_height_factor);
  float thumb_width = s_ui_minf(base_height * thumb_width_factor, box.rect.w);


  if (frame.active)
  {
    value = s_ui_slider_value_from_cursor(box.rect, thumb_width, frame.cursor.x, min_value, max_value);
  }

  if (max_value >= min_value)
  {
    value = s_ui_clampf(value, min_value, max_value);
  }
  else
  {
    value = s_ui_clampf(value, max_value, min_value);
  }

  float t = s_ui_slider_normalize(value, min_value, max_value);
  LDKUIRect track_rect = box.rect;
  track_rect.h = track_height;
  track_rect.y = box.rect.y + (box.rect.h - track_height) * 0.5f;

  LDKUIRect fill_rect = track_rect;
  fill_rect.w = thumb_width * 0.5f + s_ui_maxf(0.0f, track_rect.w - thumb_width) * t;

  LDKUIRect thumb_rect;
  thumb_rect.x = box.rect.x + s_ui_maxf(0.0f, box.rect.w - thumb_width) * t;
  thumb_rect.y = box.rect.y;
  thumb_rect.w = thumb_width;
  thumb_rect.h = box.rect.h;

  u32 track_color = s_ui_render_slider_track_color(ctx, frame.visual_state);
  u32 fill_color = ctx->theme.colors[LDK_UI_COLOR_SLIDER_FILL];
  u32 thumb_color = s_ui_render_slider_thumb_color(ctx, frame.visual_state);
  u32 border_color = s_ui_render_control_border_color(ctx, frame.visual_state);

  s_ui_render_quad(ctx, track_rect, track_color, box.clip, 0);
  s_ui_render_quad(ctx, fill_rect, fill_color, box.clip, 0);
  s_ui_render_quad(ctx, thumb_rect, thumb_color, box.clip, 0);
  s_ui_render_border(ctx, box.rect, ctx->theme.control_border_size, border_color, box.clip);

  return value;
}

u32 ldk_ui_text_box(LDKUIContext* ctx, char* buffer, u32 buffer_size)
{
  u32 result = LDK_UI_TEXT_BOX_NONE;

  if (ctx == NULL || ctx->current_layout == NULL || ctx->current_window == NULL)
  {
    return result;
  }

  if (buffer == NULL || buffer_size == 0)
  {
    return result;
  }

  buffer[buffer_size - 1] = 0;

  u32 buffer_len = s_ui_text_cstr_len_u32(buffer);
  LDKUISize text_size = ldk_ttf_measure_text_cstr(ctx->font, buffer);

  LDKUIWidgetBox box;
  if (!s_ui_widget_frame_box(ctx, &box, LDK_UI_ITEM_TEXT_BOX, ldk_ui_px(140.0f), ldk_ui_fill(),
        ldk_ui_px(LDK_UI_DEFAULT_CONTROL_HEIGHT), true))
  {
    return result;
  }

  LDKUIFrameState frame = s_ui_frame_state(ctx, box.id, box.rect, box.clip, true, box.disabled);

  if (frame.pressed && frame.hot)
  {
    ctx->text_box_id = box.id;
    ctx->text_cursor = s_ui_text_box_cursor_from_x(ctx, buffer, box.rect, frame.cursor.x);
    ctx->text_select_start = ctx->text_cursor;
    ctx->text_select_end = ctx->text_cursor;
  }

  if (frame.focused && ctx->text_box_id != box.id)
  {
    ctx->text_box_id = box.id;
    ctx->text_cursor = buffer_len;
    ctx->text_select_start = buffer_len;
    ctx->text_select_end = buffer_len;
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
          result |= LDK_UI_TEXT_BOX_CHANGED;
        }
      }
      else if (ctx->text_cursor < buffer_len)
      {
        u32 end = x_utf8_next(buffer, ctx->text_cursor);

        if (s_ui_text_delete_range(buffer, buffer_len, ctx->text_cursor, end))
        {
          ctx->text_select_start = ctx->text_cursor;
          ctx->text_select_end = ctx->text_cursor;
          result |= LDK_UI_TEXT_BOX_CHANGED;
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
          result |= LDK_UI_TEXT_BOX_CHANGED;
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
          result |= LDK_UI_TEXT_BOX_CHANGED;
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

        if (!x_utf8_encode(ctx->input_text->codepoints[i], encoded, &encoded_len))
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

        if (s_ui_text_insert_bytes(buffer, buffer_size, &ctx->text_cursor, encoded, encoded_len))
        {
          ctx->text_select_start = ctx->text_cursor;
          ctx->text_select_end = ctx->text_cursor;
          buffer_len = s_ui_text_cstr_len_u32(buffer);
          result |= LDK_UI_TEXT_BOX_CHANGED;
        }
      }
    }

    if (s_ui_input_keyboard_accept_pressed(ctx))
    {
      result |= LDK_UI_TEXT_BOX_COMMITTED;
    }

    if (ctx->keyboard != NULL && ldk_os_keyboard_key_down((LDKKeyboardState*)ctx->keyboard, LDK_KEYCODE_ESCAPE))
    {
      result |= LDK_UI_TEXT_BOX_CANCELED;
    }
  }

  if (frame.active)
  {
    ctx->cursor_type = LDK_CURSOR_TEXT_SELECT;
  }

  text_size = ldk_ttf_measure_text_cstr(ctx->font, buffer);

  u32 bg = s_ui_render_control_bg_color(ctx, frame.visual_state);
  u32 border = s_ui_render_control_border_color(ctx, frame.visual_state);
  u32 text_color = s_ui_render_control_text_color(ctx, frame.visual_state);

  s_ui_render_quad(ctx, box.rect, bg, box.clip, 0);
  s_ui_render_border(ctx, box.rect, ctx->theme.control_border_size, border, box.clip);

  float padding_x = 6.0f;
  float text_x = box.rect.x + padding_x;
  float text_y = box.rect.y + (box.rect.h - text_size.h) * 0.5f;

  if (frame.focused && ctx->text_select_start != ctx->text_select_end)
  {
    s_ui_render_text_highlight(ctx, buffer, ctx->text_select_start, ctx->text_select_end,
        text_x, box.rect, box.clip, ctx->theme.colors[LDK_UI_COLOR_FOCUS]);
  }

  s_ui_render_text(ctx, buffer, text_x, text_y, text_color, box.clip);

  if (frame.focused)
  {

    LDKTextSize text_size = ldk_ttf_measure_text_cstrn(ctx->font, buffer, ctx->text_cursor);
    float cursor_x = text_x + text_size.w;
    LDKUIRect cursor_rect = { cursor_x, box.rect.y + 4.0f, 1.0f, box.rect.h - 8.0f };
    s_ui_render_quad(ctx, cursor_rect, ctx->theme.colors[LDK_UI_COLOR_FOCUS], box.clip, 0);
  }

  return result;
}

