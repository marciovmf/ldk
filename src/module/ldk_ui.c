/**
 * This is an immediate-mode UI with a deferred layout pass.
 *
 * Widgets are submitted every frame in the order they are called by the user.
 * During submission, each widget creates an item, receives layout hints from
 * the current "next" state, and is appended to the current layout node.
 *
 * Most widget calls return their interaction result immediately. Since final
 * geometry for the current frame is not known yet, interaction during submit
 * uses the cached widget rect from the previous frame.
 *
 * At the end of the frame, the UI walks the submitted layout tree and resolves
 * final geometry. Once an item has a valid rect, the system can update its
 * cached state, resolve geometry-dependent interaction such as hover, and emit
 * draw commands.
 *
 * The frame flow is:
 *
 *   1. begin_frame
 *      - stores input pointers
 *      - clears transient per-frame state
 *      - resets the frame arena and draw buffers
 *      - finds the topmost window under the cursor using last frame geometry
 *
 *   2. submit
 *      - user calls windows, layouts, and widgets
 *      - items are allocated from the frame arena
 *      - widgets are appended to the current layout
 *      - immediate widget results use cached state from the previous frame
 *
 *   3. end_frame
 *      - windows are processed in z-order
 *      - layout nodes are measured
 *      - item rects are resolved
 *      - widget state caches are updated
 *      - geometry-dependent interaction is resolved
 *      - draw commands are emitted
 *      - stale widget/window state is garbage-collected
 *
 *   4. render
 *      - the caller consumes LDKUIRenderData
 *      - the UI does not render directly
 *
 * The important rule is:
 *
 *   Submit-time code sees last frame geometry.
 *   Resolve-time code sees current frame geometry.
 *
 * This one-frame cache is intentional. It allows the public API to remain
 * immediate-mode while still supporting layout containers whose final sizes
 * can only be known after all children have been submitted.
 */

#include <ldk_common.h>
#include <ldk_geom.h>
#include <ldk_os.h>
#include <ldk_ttf.h>
#include <module/ldk_ui.h>
#include <stdx/stdx_math.h>
#include <stdx/stdx_array.h>
#include <string.h>
#include <math.h>
#include <stdio.h>

#define LDK_UI_DEFAULT_CONTROL_HEIGHT 22.0f

typedef enum LDKUIInternalIdPart
{
  LDK_UI_INTERNAL_ID_SCROLLBAR_THUMB_Y = 1,
  LDK_UI_INTERNAL_ID_SCROLLBAR_TRACK_Y = 2,
  LDK_UI_INTERNAL_ID_SCROLLBAR_THUMB_X = 3,
  LDK_UI_INTERNAL_ID_SCROLLBAR_TRACK_X = 4,
} LDKUIInternalIdPart;

typedef enum LDKUIControlVisualState
{
  LDK_UI_CONTROL_VISUAL_STATE_IDLE = 0,
  LDK_UI_CONTROL_VISUAL_STATE_HOVERED = 1,
  LDK_UI_CONTROL_VISUAL_STATE_ACTIVE = 2,
  LDK_UI_CONTROL_VISUAL_STATE_ACTIVE_HOVERED = 3,
  LDK_UI_CONTROL_VISUAL_STATE_DISABLED = 4,
} LDKUIControlVisualState;


#define s_ui_maxf float_max
#define s_ui_minf float_min
#define s_ui_clampf float_clamp
#define s_ui_rect_contains ldk_rectf_contains
#define s_ui_rect_intersect ldk_rectf_intersect

static void s_ui_render_item(LDKUIContext* ctx, LDKUIItem* item, LDKUIRect clip_rect);

//----------------------------------------------------------
// ID and Hashing helpers
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

/**
 * Generates a stable ID for the current widget.
 *
 * The ID is computed by hashing the current window ID, the ID stack,
 * and the provided identifier string. This ensures that IDs are stable
 * across frames as long as the UI structure and pushed IDs remain consistent.
 */
static LDKUIId s_ui_id_make(LDKUIContext* ctx, u32 item_type)
{
  LDKUIId hash = 2166136261u;
  LDKUIId parent_id = 0;
  u32 item_count = 0;
  u32 scope_count = 0;

  LDK_ASSERT(ctx != NULL);

  if (ctx->current_layout != NULL)
  {
    parent_id = ctx->current_layout->id;
    item_count = ctx->current_layout->child_count;
  }
  else
  {
    item_count = ctx->root_item_count;
  }

  scope_count = x_array_ldk_ui_id_count(ctx->id_stack);

  for (u32 i = 0; i < scope_count; ++i)
  {
    LDKUIId* value = x_array_ldk_ui_id_get(ctx->id_stack, i);

    if (value != NULL)
    {
      hash = s_ui_id_hash_u32(hash, *value);
    }
  }

  hash = s_ui_id_hash_u32(hash, parent_id);
  hash = s_ui_id_hash_u32(hash, item_type);
  hash = s_ui_id_hash_u32(hash, item_count);
  ctx->last_id = hash;

  return hash;
}


//----------------------------------------------------------
// Text helpers
//----------------------------------------------------------

static LDKUISize s_ui_text_measure(LDKFontInstance* font, char const* text)
{
  LDKUISize size = {0};

  if (font == NULL || text == NULL)
  {
    return size;
  }

  size  = ldk_font_measure_text_cstr(font, text);

  return size;
}

static float s_ui_text_measure_bytes(LDKFontInstance* font, char const* text, u32 byte_count)
{
  float width = 0.0f;
  u32 prev_codepoint = 0;
  char const* cursor = text;
  char const* end = text + byte_count;

  if (font == NULL || text == NULL)
  {
    return 0.0f;
  }

  while (cursor < end && *cursor != '\0')
  {
    u32 codepoint = 0;
    char const* before = cursor;
    LDKGlyph const* glyph = NULL;

    if (!ldk_font_utf8_decode(&cursor, &codepoint))
    {
      break;
    }

    if (cursor > end)
    {
      cursor = before;
      break;
    }

    if (codepoint == '\n')
    {
      break;
    }

    glyph = ldk_font_get_glyph(font, codepoint);

    if (glyph == NULL || !glyph->valid)
    {
      prev_codepoint = 0;
      continue;
    }

    if (prev_codepoint != 0)
    {
      width += ldk_font_get_kerning(font, prev_codepoint, codepoint);
    }

    width += (float)glyph->advance_x;
    prev_codepoint = codepoint;
  }

  return width;
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

static u32 s_ui_text_utf8_prev(char const* text, u32 cursor)
{
  u32 i = cursor;

  if (text == NULL || cursor == 0)
  {
    return 0;
  }

  i -= 1;

  while (i > 0 && (((u8)text[i] & 0xc0u) == 0x80u))
  {
    i -= 1;
  }

  return i;
}

static u32 s_ui_text_utf8_next(char const* text, u32 cursor)
{
  u32 len = s_ui_text_cstr_len_u32(text);
  u32 i = cursor;

  if (text == NULL || cursor >= len)
  {
    return len;
  }

  i += 1;

  while (i < len && (((u8)text[i] & 0xc0u) == 0x80u))
  {
    i += 1;
  }

  return i;
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
    float cursor_x = s_ui_text_measure_bytes(ctx->font, text, cursor);
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

    cursor = s_ui_text_utf8_next(text, cursor);
  }

  return best;
}

static bool s_ui_text_codepoint_to_utf8(u32 codepoint, char out_text[4], u32* out_len)
{
  if (out_text == NULL || out_len == NULL)
  {
    return false;
  }

  if (codepoint <= 0x7fu)
  {
    out_text[0] = (char)codepoint;
    *out_len = 1;
    return true;
  }

  if (codepoint <= 0x7ffu)
  {
    out_text[0] = (char)(0xc0u | (codepoint >> 6));
    out_text[1] = (char)(0x80u | (codepoint & 0x3fu));
    *out_len = 2;
    return true;
  }

  if (codepoint <= 0xffffu)
  {
    out_text[0] = (char)(0xe0u | (codepoint >> 12));
    out_text[1] = (char)(0x80u | ((codepoint >> 6) & 0x3fu));
    out_text[2] = (char)(0x80u | (codepoint & 0x3fu));
    *out_len = 3;
    return true;
  }

  if (codepoint <= 0x10ffffu)
  {
    out_text[0] = (char)(0xf0u | (codepoint >> 18));
    out_text[1] = (char)(0x80u | ((codepoint >> 12) & 0x3fu));
    out_text[2] = (char)(0x80u | ((codepoint >> 6) & 0x3fu));
    out_text[3] = (char)(0x80u | (codepoint & 0x3fu));
    *out_len = 4;
    return true;
  }

  return false;
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

//----------------------------------------------------------
// Layout measuring helpers
//----------------------------------------------------------

static LDKUISize s_ui_layout_measure_button_impl(LDKFontInstance* font, char const* text)
{
  LDKUISize size = s_ui_text_measure(font, text);
  size.w += 16.0f;
  size.h += 10.0f;
  return size;
}

static LDKUISize s_ui_layout_measure_text_box_impl(LDKUIContext* ctx, char const* text)
{
  LDKUISize text_size = s_ui_text_measure(ctx->font, text);
  LDKUISize size = {0};

  size.w = s_ui_maxf(140.0f, text_size.w + 12.0f);
  size.h = s_ui_maxf(LDK_UI_DEFAULT_CONTROL_HEIGHT, text_size.h + 8.0f);

  return size;
}

static LDKUISize s_ui_layout_measure_slider_impl(LDKUIContext* ctx, char const* text, LDKUIItemType type)
{
  LDKUISize label_size = s_ui_text_measure(ctx->font, text);
  LDKUISize size = { 0.0f, 0.0f };
  float base_height = LDK_UI_DEFAULT_CONTROL_HEIGHT;
  float track_height = base_height * s_ui_clampf(ctx->theme.slider_track_height, 0.0f, 1.0f);
  float thumb_width = base_height * s_ui_clampf(ctx->theme.slider_thumb_width, 0.0f, 1.0f);
  size.w = s_ui_maxf(140.0f, label_size.w + 80.0f);
  size.h = s_ui_maxf(base_height, s_ui_maxf(track_height, thumb_width));

  return size;
}

static bool s_ui_layout_scrollbar_rects(LDKUIRect rect, float visible_size, float content_size, float scroll, bool horizontal, LDKUIRect* track_rect, LDKUIRect* thumb_rect)
{
  float thickness = 12.0f;
  float max_scroll = s_ui_maxf(0.0f, content_size - visible_size);

  if (track_rect == NULL || thumb_rect == NULL)
  {
    return false;
  }

  memset(track_rect, 0, sizeof(*track_rect));
  memset(thumb_rect, 0, sizeof(*thumb_rect));

  if (max_scroll <= 0.0f)
  {
    return false;
  }

  if (horizontal)
  {
    float thumb_w = 0.0f;
    float thumb_range = 0.0f;
    float t = 0.0f;

    track_rect->x = rect.x;
    track_rect->y = rect.y + rect.h - thickness;
    track_rect->w = rect.w;
    track_rect->h = thickness;

    thumb_w = (visible_size / content_size) * track_rect->w;
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
    float thumb_h = 0.0f;
    float thumb_range = 0.0f;
    float t = 0.0f;

    track_rect->x = rect.x + rect.w - thickness;
    track_rect->y = rect.y;
    track_rect->w = thickness;
    track_rect->h = rect.h;

    thumb_h = (visible_size / content_size) * track_rect->h;
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

static LDKUISize s_ui_layout_measure_node(LDKUILayoutNode* node)
{
  LDKUISize size = { 0.0f, 0.0f };
  u32 count = 0;
  LDKUIItem* item = NULL;

  if (node == NULL)
  {
    return size;
  }

  count = node->child_count;

  if (node->direction == LDK_UI_LAYOUT_VERTICAL)
  {
    float width = 0.0f;
    float height = node->padding * 2.0f;

    if (count > 0)
    {
      height += node->spacing * (float)(count - 1);
    }

    for (item = node->first_item; item != NULL; item = item->next_sibling)
    {
      if (item->type == LDK_UI_ITEM_LAYOUT && item->data.layout.node != NULL)
      {
        LDKUISize child_size = s_ui_layout_measure_node(item->data.layout.node);
        item->preferred_width = child_size.w;
        item->preferred_height = child_size.h;
      }

      width = s_ui_maxf(width, item->preferred_width);
      height += item->preferred_height;
    }

    size.w = width + node->padding * 2.0f;
    size.h = height;
    return size;
  }

  float width = node->padding * 2.0f;
  float height = 0.0f;

  if (count > 0)
  {
    width += node->spacing * (float)(count - 1);
  }

  for (item = node->first_item; item != NULL; item = item->next_sibling)
  {
    if (item->type == LDK_UI_ITEM_LAYOUT && item->data.layout.node != NULL)
    {
      LDKUISize child_size = s_ui_layout_measure_node(item->data.layout.node);
      item->preferred_width = child_size.w;
      item->preferred_height = child_size.h;
    }

    width += item->preferred_width;
    height = s_ui_maxf(height, item->preferred_height);
  }

  size.w = width;
  size.h = height + node->padding * 2.0f;
  return size;
}


//----------------------------------------------------------
// State helpers
//----------------------------------------------------------

static bool s_ui_state_widget_contains(LDKUIWidgetState const* state, float x, float y)
{
  if (state == NULL)
  {
    return false;
  }

  if (!s_ui_rect_contains(&state->rect, x, y))
  {
    return false;
  }

  if (!s_ui_rect_contains(&state->clip_rect, x, y))
  {
    return false;
  }

  return true;
}

/**
 * Finds widget state by ID using a linear search.
 *
 * Acceptable for small UIs (hundreds of widgets).
 * Consider a hash table if scaling to thousands.
 */
static LDKUIWidgetState* s_ui_state_widget_find(LDKUIContext* ctx, LDKUIId id)
{
  //We look for a widget via linear search. This is acceptable for small UIs with hundreds of widgets,
  //for thousands of widgets we're probably switch to a hash table.
  u32 count = x_array_ldk_ui_widget_state_count(ctx->widget_states);

  for (u32 i = 0; i < count; ++i)
  {
    LDKUIWidgetState* state = x_array_ldk_ui_widget_state_get(ctx->widget_states, i);

    if (state == NULL)
    {
      continue;
    }

    if (state->id == id)
    {
      return state;
    }
  }

  return NULL;
}

static LDKUIWidgetState* s_ui_state_widget_get_or_create(LDKUIContext* ctx, LDKUIId id)
{
  LDKUIWidgetState* state = s_ui_state_widget_find(ctx, id);

  if (state != NULL)
  {
    state->last_frame_touched = ctx->frame_index;
    return state;
  }

  LDKUIWidgetState new_state;
  memset(&new_state, 0, sizeof(new_state));
  new_state.id = id;
  new_state.last_frame_touched = ctx->frame_index;
  x_array_ldk_ui_widget_state_push(ctx->widget_states, new_state);

  return x_array_ldk_ui_widget_state_back(ctx->widget_states);
}

/**
 * In an immediate-mode UI, widgets can appear and disappear between frames.
 * This may leave stale entries in the widget_states cache.
 * This function removes any state that was not accessed in the current frame.
 */
static void s_ui_state_widgets_gc(LDKUIContext* ctx)
{
  u32 i = 0;
  u32 widget_state_count = x_array_ldk_ui_widget_state_count(ctx->widget_states); 
  while (i < widget_state_count)
  {
    LDKUIWidgetState* state = x_array_ldk_ui_widget_state_get(ctx->widget_states, i);

    if (state == NULL)
    {
      i += 1;
      continue;
    }

    if (state->last_frame_touched != ctx->frame_index)
    {
      x_array_ldk_ui_widget_state_delete_at(ctx->widget_states, i);
      continue;
    }

    i += 1;
  }
}

/**
 * Removes windows that were not submitted during the current frame.
 *
 * Windows are persistent only while the user keeps submitting them.
 * If a window's root_layout is NULL at the end of the frame, it did not
 * appear in this frame and must not keep participating in hit-testing,
 * focus, or z-order.
 */
static void s_ui_state_windows_gc(LDKUIContext* ctx)
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

    if (window->root_layout == NULL)
    {
      if (ctx->hovered_window == window)
      {
        ctx->hovered_window = NULL;
      }

      if (ctx->focused_window == window)
      {
        ctx->focused_window = NULL;
        ctx->focused_id = 0;
      }

      if (ctx->current_window == window)
      {
        ctx->current_window = NULL;
      }

      x_array_ldk_ui_window_delete_at(ctx->windows, i);
      continue;
    }

    i += 1;
  }
}

static void s_ui_state_update_item_rect(LDKUIContext* ctx, LDKUIItem* item, LDKUIRect clip_rect)
{
  LDKUIWidgetState* state;

  if (ctx == NULL || item == NULL)
  {
    return;
  }

  state = s_ui_state_widget_get_or_create(ctx, item->id);

  if (state == NULL)
  {
    return;
  }

  state->rect = item->rect;
  state->clip_rect = clip_rect;
}

static void s_ui_state_update_content_size(LDKUIContext* ctx, LDKUIId id, LDKUISize content_size)
{
  LDKUIWidgetState* state;

  if (ctx == NULL)
  {
    return;
  }

  state = s_ui_state_widget_get_or_create(ctx, id);

  if (state == NULL)
  {
    return;
  }

  state->content_size.w = content_size.w;
  state->content_size.h = content_size.h;
}

//----------------------------------------------------------
// Interact helpers
//----------------------------------------------------------

static bool s_ui_interact_window_can_interact(LDKUIContext* ctx, LDKUIWindow* window)
{
  if (window == NULL)
  {
    return false;
  }

  if (ctx->focused_window == window)
  {
    return true;
  }

  return false;
}

static void s_ui_interact_focus_item(LDKUIContext* ctx, LDKUIItem const* item)
{
  if (ctx == NULL || item == NULL || ctx->current_window == NULL)
  {
    return;
  }

  ctx->focused_window = ctx->current_window;
  ctx->current_window->focused_id = item->id;
  ctx->focused_id = item->id;
}

static bool s_ui_interact_item_is_focused(LDKUIContext* ctx, LDKUIItem const* item)
{
  if (ctx == NULL || item == NULL || ctx->current_window == NULL)
  {
    return false;
  }

  if (ctx->focused_window != ctx->current_window)
  {
    return false;
  }

  return ctx->current_window->focused_id == item->id;
}

static void s_ui_interact_clear_item_focus(LDKUIContext* ctx, LDKUIItem const* item)
{
  if (ctx == NULL || item == NULL)
  {
    return;
  }

  if (ctx->focused_id == item->id)
  {
    ctx->focused_id = 0;
  }

  if (ctx->current_window != NULL && ctx->current_window->focused_id == item->id)
  {
    ctx->current_window->focused_id = 0;
  }
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

static void s_ui_interact_clear_item_if_disabled(LDKUIContext* ctx, LDKUIItem const* item)
{
  if (ctx == NULL || item == NULL)
  {
    return;
  }

  if (!item->disabled)
  {
    return;
  }

  if (ctx->active_id == item->id)
  {
    ctx->active_id = 0;
  }

  s_ui_interact_clear_item_focus(ctx, item);
}

/**
 * Returns true if the widget was clicked this frame.
 *
 * Interaction is evaluated using the widget's rect from the previous frame,
 * since the current frame layout has not yet been resolved at call time.
 */
static bool s_ui_interact_cached_press(LDKUIContext* ctx, LDKUIItem* item)
{
  if (item == NULL)
  {
    return false;
  }

  if (item->disabled)
  {
    s_ui_interact_clear_item_if_disabled(ctx, item);
    return false;
  }

  LDKUIId id = item->id;
  LDKUIWidgetState* state = s_ui_state_widget_find(ctx, id);
  LDKPoint cursor;
  bool inside;

  if (state == NULL)
  {
    return false;
  }

  if (!s_ui_interact_window_can_interact(ctx, ctx->current_window) && ctx->active_id != id)
  {
    return false;
  }

  cursor = ldk_os_mouse_cursor((LDKMouseState*)ctx->mouse);
  inside = s_ui_state_widget_contains(state, (float)cursor.x, (float)cursor.y);

  if (inside)
  {
    ctx->hot_id = id;
  }

  if (inside && ldk_os_mouse_button_down((LDKMouseState*)ctx->mouse, LDK_MOUSE_BUTTON_LEFT))
  {
    ctx->active_id = id;
    s_ui_interact_focus_item(ctx, item);
  }

  if (!inside)
  {
    return false;
  }

  if (!ldk_os_mouse_button_up((LDKMouseState*)ctx->mouse, LDK_MOUSE_BUTTON_LEFT))
  {
    return false;
  }

  if (ctx->active_id != id)
  {
    return false;
  }

  ctx->active_id = 0;
  s_ui_interact_focus_item(ctx, item);
  return true;
}

/**
 * Returns true if the slider value changed this frame.
 *
 * Interaction is evaluated using the widget's rect from the previous frame,
 * since the current frame layout has not yet been resolved at call time.
 */
static bool s_ui_interact_cached_slider(LDKUIContext* ctx, LDKUIItem* item, float* value, float min_value, float max_value)
{
  if (item == NULL || value == NULL)
  {
    return false;
  }

  if (item->disabled)
  {
    s_ui_interact_clear_item_if_disabled(ctx, item);
    return false;
  }

  LDKUIWidgetState* state = s_ui_state_widget_find(ctx, item->id);

  if (state == NULL)
  {
    return false;
  }

  if (!s_ui_interact_window_can_interact(ctx, ctx->current_window) && ctx->active_id != item->id)
  {
    return false;
  }

  LDKPoint cursor = ldk_os_mouse_cursor((LDKMouseState*)ctx->mouse);
  bool inside = s_ui_state_widget_contains(state, (float)cursor.x, (float)cursor.y);

  if (inside)
  {
    ctx->hot_id = item->id;
  }

  if (inside && ldk_os_mouse_button_down((LDKMouseState*)ctx->mouse, LDK_MOUSE_BUTTON_LEFT))
  {
    ctx->active_id = item->id;
    s_ui_interact_focus_item(ctx, item);
  }

  bool down = ldk_os_mouse_button_is_pressed((LDKMouseState*)ctx->mouse, LDK_MOUSE_BUTTON_LEFT);

  if (ctx->active_id != item->id || !down)
  {
    if (ldk_os_mouse_button_up((LDKMouseState*)ctx->mouse, LDK_MOUSE_BUTTON_LEFT) && ctx->active_id == item->id)
    {
      ctx->active_id = 0;
    }

    return false;
  }

  float thumb_width = 0.0f;

  if (item->type == LDK_UI_ITEM_SLIDER)
  {
    float thumb_width_factor = s_ui_clampf(ctx->theme.slider_thumb_width, 0.0f, 1.0f);
    thumb_width = s_ui_minf(state->rect.h * thumb_width_factor, state->rect.w);
  }

  float usable_width = s_ui_maxf(state->rect.w - thumb_width, 1.0f);
  float local = (float)cursor.x - state->rect.x - thumb_width * 0.5f;
  float t = s_ui_clampf(local / usable_width, 0.0f, 1.0f);
  float new_value = min_value + (max_value - min_value) * t;

  if (*value == new_value)
  {
    return false;
  }

  *value = new_value;
  return true;
}

static void s_ui_interact_resolved_item(LDKUIContext* ctx, LDKUIItem* item, LDKUIRect clip_rect)
{
  LDKPoint cursor = ldk_os_mouse_cursor((LDKMouseState*)ctx->mouse);

  bool inside;

  if (!s_ui_interact_window_can_interact(ctx, ctx->current_window) && ctx->active_id != item->id)
  {
    return;
  }

  inside = s_ui_rect_contains(&item->rect, (float)cursor.x, (float)cursor.y) &&
    s_ui_rect_contains(&clip_rect, (float)cursor.x, (float)cursor.y);

  if (inside)
  {
    ctx->hot_id = item->id;
  }

  if (inside && ctx->active_id == 0 && ldk_os_mouse_button_down((LDKMouseState*)ctx->mouse, LDK_MOUSE_BUTTON_LEFT))
  {
    ctx->active_id = item->id;
  }
}


//----------------------------------------------------------
// Layout Resolving helpers
//----------------------------------------------------------

/**
 * Resolves layout and geometry for a layout node and its children.
 *
 * Each node arranges its children vertically or horizontally in order,
 * applying spacing and padding.
 *
 * Final rectangles (item->rect) are computed from preferred/min sizes
 * and expansion rules, distributing remaining space among expandable items.
 *
 * Layout is resolved top-down after all widgets are declared, producing
 * concrete geometry and interaction results for the current frame.
 */
static void s_ui_layout_resolve_node(LDKUIContext* ctx, LDKUILayoutNode* node, LDKUIRect rect, LDKUIRect inherited_clip);

static void s_ui_layout_resolve_scroll_area(LDKUIContext* ctx, LDKUIItem* item, LDKUIRect node_clip)
{
  LDKUISize content_size = s_ui_layout_measure_node(item->data.scroll_area.node);
  LDKUIRect scroll_clip = s_ui_rect_intersect(&node_clip, &item->rect);
  LDKUIRect content_rect = item->rect;
  bool allow_vertical = (item->data.scroll_area.flags & LDK_UI_SCROLL_VERTICAL) != 0;
  bool allow_horizontal = (item->data.scroll_area.flags & LDK_UI_SCROLL_HORIZONTAL) != 0;
  bool has_vertical_scrollbar = false;
  bool has_horizontal_scrollbar = false;

  if (allow_vertical)
  {
    has_vertical_scrollbar = s_ui_layout_scrollbar_rects(
        item->rect,
        item->rect.h,
        content_size.h,
        item->data.scroll_area.scroll.y,
        false,
        &item->data.scroll_area.vertical_track_rect,
        &item->data.scroll_area.vertical_thumb_rect);
  }

  if (allow_horizontal)
  {
    has_horizontal_scrollbar = s_ui_layout_scrollbar_rects(
        item->rect,
        item->rect.w,
        content_size.w,
        item->data.scroll_area.scroll.x,
        true,
        &item->data.scroll_area.horizontal_track_rect,
        &item->data.scroll_area.horizontal_thumb_rect);
  }

  item->data.scroll_area.has_vertical_scrollbar = has_vertical_scrollbar;
  item->data.scroll_area.has_horizontal_scrollbar = has_horizontal_scrollbar;

  if (has_vertical_scrollbar)
  {
    LDKPoint cursor = ldk_os_mouse_cursor((LDKMouseState*)ctx->mouse);
    LDKUIId thumb_id = s_ui_id_hash_u32(item->id, (u32)LDK_UI_INTERNAL_ID_SCROLLBAR_THUMB_Y);

    if (s_ui_rect_contains(&item->data.scroll_area.vertical_thumb_rect, (float)cursor.x, (float)cursor.y))
    {
      ctx->hot_id = thumb_id;
    }

    content_rect.w = s_ui_maxf(0.0f, content_rect.w - item->data.scroll_area.vertical_track_rect.w);
  }

  if (has_horizontal_scrollbar)
  {
    LDKPoint cursor = ldk_os_mouse_cursor((LDKMouseState*)ctx->mouse);
    LDKUIId thumb_id = s_ui_id_hash_u32(item->id, (u32)LDK_UI_INTERNAL_ID_SCROLLBAR_THUMB_X);

    if (s_ui_rect_contains(&item->data.scroll_area.horizontal_thumb_rect, (float)cursor.x, (float)cursor.y))
    {
      ctx->hot_id = thumb_id;
    }

    content_rect.h = s_ui_maxf(0.0f, content_rect.h - item->data.scroll_area.horizontal_track_rect.h);
  }

  scroll_clip = s_ui_rect_intersect(&node_clip, &content_rect);

  content_rect.x -= item->data.scroll_area.scroll.x;
  content_rect.y -= item->data.scroll_area.scroll.y;
  content_rect.w = s_ui_maxf(content_rect.w, content_size.w);
  content_rect.h = s_ui_maxf(content_rect.h, content_size.h);

  s_ui_state_update_content_size(ctx, item->id, content_size);
  s_ui_layout_resolve_node(ctx, item->data.scroll_area.node, content_rect, scroll_clip);
  s_ui_render_item(ctx, item, node_clip);
}

/**
 * Called during layout resolution after the item rect becomes valid.
 *
 * This phase also performs:
 * - interaction resolution
 * - widget state update
 * - rendering
 *
 * since all of them depend on final geometry.
 */
static void s_ui_layout_resolve_item(LDKUIContext* ctx, LDKUIItem* item, LDKUIRect node_clip)
{
  s_ui_state_update_item_rect(ctx, item, node_clip);
  s_ui_interact_resolved_item(ctx, item, node_clip);

  if (item->type == LDK_UI_ITEM_LAYOUT && item->data.layout.node != NULL)
  {
    s_ui_layout_resolve_node(ctx, item->data.layout.node, item->rect, node_clip);
    return;
  }

  if (item->type == LDK_UI_ITEM_SCROLL_AREA && item->data.scroll_area.node != NULL)
  {
    s_ui_layout_resolve_scroll_area(ctx, item, node_clip);
    return;
  }

  s_ui_render_item(ctx, item, node_clip);
}

/**
 * Resolves layout and geometry for a layout node and its children.
 *
 * Each node arranges its children vertically or horizontally in order,
 * applying spacing and padding.
 *
 * Final rectangles (item->rect) are computed from preferred/min sizes
 * and expansion rules, distributing remaining space among expandable items.
 *
 * Layout is resolved top-down after all widgets are declared, producing
 * concrete geometry and interaction results for the current frame.
 */
static void s_ui_layout_resolve_node(LDKUIContext* ctx, LDKUILayoutNode* node, LDKUIRect rect, LDKUIRect inherited_clip)
{
  u32 count = 0;
  float inner_x = rect.x + node->padding;
  float inner_y = rect.y + node->padding;
  float inner_w = rect.w - node->padding * 2.0f;
  float inner_h = rect.h - node->padding * 2.0f;
  LDKUIRect node_clip = s_ui_rect_intersect(&inherited_clip, &rect);

  node->rect = rect;
  count = node->child_count;

  if (node->direction == LDK_UI_LAYOUT_VERTICAL)
  {
    float fixed_height = 0.0f;
    u32 expand_count = 0;
    float remaining;
    float expand_height = 0.0f;
    float cursor_y;

    for (LDKUIItem* item = node->first_item; item != NULL; item = item->next_sibling)
    {
      if (item->expand_height)
      {
        expand_count += 1;
      }
      else
      {
        fixed_height += s_ui_maxf(item->preferred_height, item->min_height);
      }
    }

    if (count > 0)
    {
      fixed_height += node->spacing * (float)(count - 1);
    }

    remaining = inner_h - fixed_height;

    if (remaining < 0.0f)
    {
      remaining = 0.0f;
    }

    if (expand_count > 0)
    {
      expand_height = remaining / (float)expand_count;
    }

    cursor_y = inner_y;

    for (LDKUIItem* item = node->first_item; item != NULL; item = item->next_sibling)
    {
      float item_h = s_ui_maxf(item->preferred_height, item->min_height);
      float item_w = item->expand_width
        ? inner_w
        : s_ui_minf(s_ui_maxf(item->preferred_width, item->min_width), inner_w);

      if (item->expand_height)
      {
        item_h = s_ui_maxf(expand_height, item->min_height);
      }

      item->rect.x = inner_x;
      item->rect.y = cursor_y;
      item->rect.w = item_w;
      item->rect.h = item_h;

      s_ui_layout_resolve_item(ctx, item, node_clip);

      cursor_y += item_h + node->spacing;
    }

    return;
  }

  float fixed_width = 0.0f;
  u32 expand_count = 0;
  float remaining;
  float expand_width = 0.0f;
  float cursor_x;

  for (LDKUIItem* item = node->first_item; item != NULL; item = item->next_sibling)
  {
    if (item->expand_width)
    {
      expand_count += 1;
    }
    else
    {
      fixed_width += s_ui_maxf(item->preferred_width, item->min_width);
    }
  }

  if (count > 0)
  {
    fixed_width += node->spacing * (float)(count - 1);
  }

  remaining = inner_w - fixed_width;

  if (remaining < 0.0f)
  {
    remaining = 0.0f;
  }

  if (expand_count > 0)
  {
    expand_width = remaining / (float)expand_count;
  }

  cursor_x = inner_x;

  for (LDKUIItem* item = node->first_item; item != NULL; item = item->next_sibling)
  {
    float item_w = s_ui_maxf(item->preferred_width, item->min_width);
    float item_h = item->expand_height
      ? inner_h
      : s_ui_minf(s_ui_maxf(item->preferred_height, item->min_height), inner_h);

    if (item->expand_width)
    {
      item_w = s_ui_maxf(expand_width, item->min_width);
    }

    item->rect.x = cursor_x;
    item->rect.y = inner_y;
    item->rect.w = item_w;
    item->rect.h = item_h;

    s_ui_layout_resolve_item(ctx, item, node_clip);

    cursor_x += item_w + node->spacing;
  }
}


//----------------------------------------------------------
// Layout measuring helpers
//----------------------------------------------------------

static LDKUILayoutNode* s_ui_layout_node_create(LDKUIContext* ctx, LDKUILayoutDirection direction, LDKUILayoutNode* parent, LDKUIWindow* window)
{
  LDKUILayoutNode* node = x_arena_alloc_zero(ctx->frame_arena, sizeof(LDKUILayoutNode));

  if (node == NULL)
  {
    return NULL;
  }

  node->id = 0;
  node->direction = direction;
  node->spacing = 4.0f;
  node->padding = (direction == LDK_UI_LAYOUT_HORIZONTAL) ? 0.0f : 4.0f;
  node->parent = parent;
  node->window = window;
  node->child_count = 0;
  node->first_item = NULL;
  node->last_item = NULL;

  return node;
}



//----------------------------------------------------------
// Item submission
//----------------------------------------------------------

static bool s_ui_state_current_disabled_scope(LDKUIContext const* ctx)
{
  if (ctx == NULL)
  {
    return false;
  }

  bool const* disabled = x_array_ldk_ui_bool_back(ctx->disabled_stack);

  if (disabled == NULL)
  {
    return false;
  }

  return *disabled;
}

static bool s_ui_submit_next_item_disabled(LDKUIContext const* ctx)
{
  if (ctx == NULL)
  {
    return false;
  }

  if (ctx->next_disabled)
  {
    return true;
  }

  return s_ui_state_current_disabled_scope(ctx);
}

static LDKUIItem* s_ui_submit_item_create(LDKUIContext* ctx, LDKUIItemType type)
{
  if (ctx == NULL)
  {
    return NULL;
  }

  LDKUIItem* item = x_arena_alloc_zero(ctx->frame_arena, sizeof(LDKUIItem));

  if (item == NULL)
  {
    ctx->next_disabled = false;
    return NULL;
  }

  item->type = type;
  item->id = s_ui_id_make(ctx, (u32)type);
  item->disabled = s_ui_submit_next_item_disabled(ctx);
  ctx->next_disabled = false;

  if (ctx->next_focus)
  {
    if (!item->disabled && ctx->current_window != NULL)
    {
      ctx->current_window->focused_id = item->id;
      ctx->focused_id = item->id;
    }

    ctx->next_focus = false;
  }

  return item;
}

static void s_ui_submit_reset_next_layout(LDKUIContext* ctx)
{
  memset(&ctx->next_layout, 0, sizeof(ctx->next_layout));
}

static void s_ui_submit_item(LDKUIContext* ctx, LDKUIItem* item)
{
  if (ctx == NULL || item == NULL)
  {
    return;
  }

  { // Apply Next Layout
    if (ctx->next_layout.has_width)
    {
      item->preferred_width = ctx->next_layout.width;
    }

    if (ctx->next_layout.has_height)
    {
      item->preferred_height = ctx->next_layout.height;
    }

    if (ctx->next_layout.has_min_width)
    {
      item->min_width = ctx->next_layout.min_width;
    }

    if (ctx->next_layout.has_min_height)
    {
      item->min_height = ctx->next_layout.min_height;
    }

    if (ctx->next_layout.has_expand_width)
    {
      item->expand_width = ctx->next_layout.expand_width;
    }

    if (ctx->next_layout.has_expand_height)
    {
      item->expand_height = ctx->next_layout.expand_height;
    }

    s_ui_submit_reset_next_layout(ctx);
  }

  { // Append item to layout
    item->next_sibling = NULL;

    if (ctx->current_layout->last_item != NULL)
    {
      ctx->current_layout->last_item->next_sibling = item;
    }
    else
    {
      ctx->current_layout->first_item = item;
    }

    ctx->current_layout->last_item = item;
    ctx->current_layout->child_count += 1;
  }
}


//----------------------------------------------------------
// Window/Panel management
//----------------------------------------------------------

static LDKUIWindow* s_ui_state_window_find(LDKUIContext* ctx, LDKUIId id)
{
  u32 count = x_array_ldk_ui_window_count(ctx->windows);

  for (u32 i = 0; i < count; ++i)
  {
    LDKUIWindow* window = x_array_ldk_ui_window_get(ctx->windows, i);

    if (window == NULL)
    {
      continue;
    }
    if (window->id == id)
    {
      return window;
    }
  }

  return NULL;
}

static LDKUIWindow* s_ui_state_window_get_or_create(LDKUIContext* ctx, LDKUIId id, char const* title)
{
  LDKUIWindow* window = s_ui_state_window_find(ctx, id);

  if (window != NULL)
  {
    return window;
  }

  LDKUIWindow new_window;
  memset(&new_window, 0, sizeof(new_window));

  new_window.id = id;

  if (title != NULL)
  {
    strncpy(new_window.title, title, sizeof(new_window.title) - 1);
    new_window.title[sizeof(new_window.title) - 1] = 0;
  }

  x_array_ldk_ui_window_push(ctx->windows, new_window);

  return x_array_ldk_ui_window_back(ctx->windows);
}

static LDKUIRect s_ui_window_rect(LDKUIWindow const* window)
{
  LDKUIRect rect = {0};

  if (window == NULL)
  {
    return rect;
  }
  rect.x = window->title_bar_rect.x;
  rect.y = window->title_bar_rect.y;
  rect.w = window->title_bar_rect.w;
  rect.h = window->title_bar_rect.h + window->content_rect.h;

  return rect;
}

static void s_ui_state_window_bring_to_front(LDKUIContext* ctx, LDKUIWindow* window)
{
  u32 count = x_array_ldk_ui_window_count(ctx->windows);
  u32 found_index = UINT32_MAX;

  if (window == NULL)
  {
    return;
  }
  for (u32 i = 0; i < count; ++i)
  {
    LDKUIWindow* current = x_array_ldk_ui_window_get(ctx->windows, i);

    if (current == window)
    {
      found_index = i;
      break;
    }
  }

  if (found_index == UINT32_MAX)
  {
    return;
  }

  if (found_index == count - 1)
  {
    return;
  }

  LDKUIWindow copy = *window;

  x_array_ldk_ui_window_delete_at(ctx->windows, found_index);
  x_array_ldk_ui_window_push(ctx->windows, copy);
  ctx->current_window = x_array_ldk_ui_window_back(ctx->windows);
}

static void s_ui_render_add_draw_cmd(LDKUIContext* ctx, LDKUITextureHandle texture, LDKUIRect clip_rect, u32 index_offset, u32 index_count)
{
  LDKUIDrawCmd* back = x_array_ldk_ui_draw_cmd_back(ctx->commands);

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

  x_array_ldk_ui_draw_cmd_push(ctx->commands, cmd);
}

/**
 * Begins a root container.
 *
 * A root container is positioned explicitly (not by layout) and is used for
 * top-level UI elements like windows, popups, and overlays.
 *
 * The input `rect` is the source of truth. If `draggable` is enabled, the
 * rect may be modified based on user input and written to `out_rect`.
 *
 * This function also handles top-level interaction (hover, focus, z-order)
 * and creates the root layout node for its contents.
 */
static bool s_ui_begin_root_container(LDKUIContext* ctx, LDKUIId id, char const* title, LDKUIRect rect, bool toolbar, bool draggable, LDKUIRect* out_rect)
{
  LDKUIId resolved_id = s_ui_id_make(ctx, id);
  LDKUIWindow* window = s_ui_state_window_get_or_create(ctx, resolved_id, title);
  LDKUIRect result_rect = rect;
  float header_height = toolbar ? 24.0f : 0.0f;
  LDKUIRect title_bar_rect = {0};
  LDKPoint cursor;
  bool inside_window;
  bool inside_title_bar;
  LDKUILayoutNode* root;

  if (window == NULL)
  {
    return false;
  }
  cursor = ldk_os_mouse_cursor((LDKMouseState*)ctx->mouse);

  //
  // Controls
  //
  if (title != NULL)
  {
    strncpy(window->title, title, sizeof(window->title) - 1);
    window->title[sizeof(window->title) - 1] = 0;
  }

  title_bar_rect.x = rect.x;
  title_bar_rect.y = rect.y;
  title_bar_rect.w = rect.w;
  title_bar_rect.h = header_height;

  inside_window = s_ui_rect_contains(&rect, (float)cursor.x, (float)cursor.y);
  inside_title_bar = header_height > 0.0f &&
    s_ui_rect_contains(&title_bar_rect, (float)cursor.x, (float)cursor.y);

  if (ctx->hovered_window == window && inside_window)
  {
    if (ldk_os_mouse_button_down((LDKMouseState*)ctx->mouse, LDK_MOUSE_BUTTON_LEFT))
    {
      s_ui_state_window_bring_to_front(ctx, window);
      window = x_array_ldk_ui_window_back(ctx->windows);
      ctx->focused_window = window;
      ctx->focused_id = window->focused_id;
    }
  }

  if (ctx->hovered_window == window && draggable && inside_title_bar)
  {
    if (ldk_os_mouse_button_down((LDKMouseState*)ctx->mouse, LDK_MOUSE_BUTTON_LEFT))
    {
      ctx->dragging_item = resolved_id;
      ctx->drag_x = (float)cursor.x - rect.x;
      ctx->drag_y = (float)cursor.y - rect.y;
      ctx->focused_window = window;
      ctx->focused_id = window->focused_id;
    }
  }

  if (ctx->dragging_item == resolved_id)
  {
    if (ldk_os_mouse_button_is_pressed((LDKMouseState*)ctx->mouse, LDK_MOUSE_BUTTON_LEFT))
    {
      result_rect.x = (float)cursor.x - ctx->drag_x;
      result_rect.y = (float)cursor.y - ctx->drag_y;
    }
    else
    {
      ctx->dragging_item = 0;
    }
  }

  window->title_bar_rect.x = result_rect.x;
  window->title_bar_rect.y = result_rect.y;
  window->title_bar_rect.w = result_rect.w;
  window->title_bar_rect.h = header_height;

  window->content_rect.x = result_rect.x;
  window->content_rect.y = result_rect.y + header_height;
  window->content_rect.w = result_rect.w;
  window->content_rect.h = result_rect.h - header_height;

  root = s_ui_layout_node_create(ctx, LDK_UI_LAYOUT_VERTICAL, NULL, window);

  if (root == NULL)
  {
    return false;
  }

  root->id = resolved_id;
  window->root_layout = root;
  ctx->current_window = window;
  ctx->current_layout = root;
  ctx->root_item_count++;

  if (out_rect != NULL)
  {
    *out_rect = result_rect;
  }

  return true;
}



//----------------------------------------------------------
// Geometry Submition
//----------------------------------------------------------

static void s_ui_render_quad(LDKUIContext* ctx, LDKUIRect rect, u32 color, LDKUIRect clip_rect, LDKUITextureHandle texture)
{
  color = LDK_RGBA32(color);
  u32 index_offset = x_array_ldk_ui_u32_count(ctx->indices);
  u32 base_index = x_array_ldk_ui_vertex_count(ctx->vertices);

  LDKUIVertex v0 = { rect.x, rect.y, 0.0f, 0.0f, color };
  LDKUIVertex v1 = { rect.x + rect.w, rect.y, 1.0f, 0.0f, color };
  LDKUIVertex v2 = { rect.x + rect.w, rect.y + rect.h, 1.0f, 1.0f, color };
  LDKUIVertex v3 = { rect.x, rect.y + rect.h, 0.0f, 1.0f, color };

  u32 i0 = base_index + 0;
  u32 i1 = base_index + 1;
  u32 i2 = base_index + 2;
  u32 i3 = base_index + 3;

  x_array_ldk_ui_vertex_push(ctx->vertices, v0);
  x_array_ldk_ui_vertex_push(ctx->vertices, v1);
  x_array_ldk_ui_vertex_push(ctx->vertices, v2);
  x_array_ldk_ui_vertex_push(ctx->vertices, v3);

  x_array_ldk_ui_u32_push(ctx->indices, i0);
  x_array_ldk_ui_u32_push(ctx->indices, i1);
  x_array_ldk_ui_u32_push(ctx->indices, i2);
  x_array_ldk_ui_u32_push(ctx->indices, i2);
  x_array_ldk_ui_u32_push(ctx->indices, i3);
  x_array_ldk_ui_u32_push(ctx->indices, i0);

  s_ui_render_add_draw_cmd(ctx, texture, clip_rect, index_offset, 6);
}

static LDKUIControlVisualState s_ui_render_control_visual_state(LDKUIContext* ctx, LDKUIItem const* item)
{
  if (item->disabled)
  {
    return LDK_UI_CONTROL_VISUAL_STATE_DISABLED;
  }

  if (ctx->active_id == item->id && ctx->hot_id == item->id)
  {
    return LDK_UI_CONTROL_VISUAL_STATE_ACTIVE_HOVERED;
  }

  if (ctx->active_id == item->id)
  {
    return LDK_UI_CONTROL_VISUAL_STATE_ACTIVE;
  }

  if (ctx->hot_id == item->id)
  {
    return LDK_UI_CONTROL_VISUAL_STATE_HOVERED;
  }

  return LDK_UI_CONTROL_VISUAL_STATE_IDLE;
}

static u32 s_ui_render_control_text_color(LDKUIContext* ctx, LDKUIControlVisualState state)
{
  if (state == LDK_UI_CONTROL_VISUAL_STATE_DISABLED)
  {
    return ctx->theme.colors[LDK_UI_COLOR_CONTROL_TEXT_DISABLED];
  }

  if (state == LDK_UI_CONTROL_VISUAL_STATE_ACTIVE_HOVERED)
  {
    return ctx->theme.colors[LDK_UI_COLOR_CONTROL_TEXT_ACTIVE_HOVERED];
  }

  if (state == LDK_UI_CONTROL_VISUAL_STATE_ACTIVE)
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

  if (state == LDK_UI_CONTROL_VISUAL_STATE_ACTIVE_HOVERED)
  {
    return ctx->theme.colors[LDK_UI_COLOR_CONTROL_BORDER_ACTIVE_HOVERED];
  }

  if (state == LDK_UI_CONTROL_VISUAL_STATE_ACTIVE)
  {
    return ctx->theme.colors[LDK_UI_COLOR_CONTROL_BORDER_ACTIVE];
  }

  if (state == LDK_UI_CONTROL_VISUAL_STATE_HOVERED)
  {
    return ctx->theme.colors[LDK_UI_COLOR_CONTROL_BORDER_HOVERED];
  }

  return ctx->theme.colors[LDK_UI_COLOR_CONTROL_BORDER];
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

static void s_ui_render_border(LDKUIContext* ctx, LDKUIRect rect, float size, u32 color, LDKUIRect clip_rect)
{
  float border_size = s_ui_maxf(size, 0.0f);

  if (border_size <= 0.0f)
  {
    return;
  }

  LDKUIRect top = { rect.x, rect.y, rect.w, border_size };
  LDKUIRect bottom = { rect.x, rect.y + rect.h - border_size, rect.w, border_size };
  LDKUIRect left = { rect.x, rect.y, border_size, rect.h };
  LDKUIRect right = { rect.x + rect.w - border_size, rect.y, border_size, rect.h };

  s_ui_render_quad(ctx, top, color, clip_rect, 0);
  s_ui_render_quad(ctx, bottom, color, clip_rect, 0);
  s_ui_render_quad(ctx, left, color, clip_rect, 0);
  s_ui_render_quad(ctx, right, color, clip_rect, 0);
}

#ifdef LDK_UI_DEBUG_DRAW
static void s_ui_render_debug_rect(LDKUIContext* ctx, LDKUIRect rect, u32 color)
{
  float t = 1.0f;

  LDKUIRect top = { rect.x, rect.y, rect.w, t };
  LDKUIRect bottom = { rect.x, rect.y + rect.h - t, rect.w, t };
  LDKUIRect left = { rect.x, rect.y, t, rect.h };
  LDKUIRect right = { rect.x + rect.w - t, rect.y, t, rect.h };

  LDKUIRect no_clip = { -100000.0f, -100000.0f, 200000.0f, 200000.0f };

  s_ui_render_quad(ctx, top, color, no_clip, 0);
  s_ui_render_quad(ctx, bottom, color, no_clip, 0);
  s_ui_render_quad(ctx, left, color, no_clip, 0);
  s_ui_render_quad(ctx, right, color, no_clip, 0);
}
#endif

static void s_ui_render_text(LDKUIContext* ctx, char const* text, float x, float y, u32 color, LDKUIRect clip_rect)
{
  LDKFontInstance* font;
  LDKFontMetrics metrics;
  float pen_x;
  float pen_y;
  u32 prev_codepoint;
  char const* cursor;
  color = LDK_RGBA32(color);

  if (ctx == NULL || text == NULL)
  {
    return;
  }

  font = ctx->font;

  if (font == NULL)
  {
    return;
  }

  metrics = ldk_font_get_metrics(font);
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

    if (!ldk_font_utf8_decode(&cursor, &codepoint))
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

    glyph = ldk_font_get_glyph(font, codepoint);

    if (glyph == NULL || !glyph->valid)
    {
      prev_codepoint = 0;
      continue;
    }

    if (prev_codepoint != 0)
    {
      pen_x += ldk_font_get_kerning(font, prev_codepoint, codepoint);
    }

    if (!ldk_font_get_page_info(font, glyph->page_index, &page))
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

    index_offset = x_array_ldk_ui_u32_count(ctx->indices);
    base_index = x_array_ldk_ui_vertex_count(ctx->vertices);

    x_array_ldk_ui_vertex_push(ctx->vertices, (LDKUIVertex){ gx0, gy0, u0, v0, color });
    x_array_ldk_ui_vertex_push(ctx->vertices, (LDKUIVertex){ gx1, gy0, u1, v0, color });
    x_array_ldk_ui_vertex_push(ctx->vertices, (LDKUIVertex){ gx1, gy1, u1, v1, color });
    x_array_ldk_ui_vertex_push(ctx->vertices, (LDKUIVertex){ gx0, gy1, u0, v1, color });

    x_array_ldk_ui_u32_push(ctx->indices, base_index + 0);
    x_array_ldk_ui_u32_push(ctx->indices, base_index + 1);
    x_array_ldk_ui_u32_push(ctx->indices, base_index + 2);
    x_array_ldk_ui_u32_push(ctx->indices, base_index + 2);
    x_array_ldk_ui_u32_push(ctx->indices, base_index + 3);
    x_array_ldk_ui_u32_push(ctx->indices, base_index + 0);

    //Requests a renderable texture handle for a font atlas page.
    //The UI does not own font page textures. Instead we request it from this callback
    LDKUITextureHandle texture = 0;
    if (ctx->get_font_page_texture != NULL)
      texture = ctx->get_font_page_texture(ctx->font_texture_user, ctx->font, glyph->page_index);

    s_ui_render_add_draw_cmd(ctx, texture, clip_rect, index_offset, 6);

    pen_x += (float)glyph->advance_x;
    prev_codepoint = codepoint;
  }
}

static void s_ui_render_label(LDKUIContext* ctx, LDKUIItem* item, LDKUIRect clip_rect)
{
  s_ui_render_text(
      ctx,
      item->text,
      item->rect.x,
      item->rect.y,
      ctx->theme.colors[LDK_UI_COLOR_TEXT],
      clip_rect);
}

static void s_ui_render_button(LDKUIContext* ctx, LDKUIItem* item, LDKUIRect clip_rect)
{
  LDKUIControlVisualState state = s_ui_render_control_visual_state(ctx, item);
  u32 color = s_ui_render_control_bg_color(ctx, state);
  u32 text_color = s_ui_render_control_text_color(ctx, state);
  u32 border_color = s_ui_render_control_border_color(ctx, state);
  LDKUISize text_size;

  s_ui_render_quad(ctx, item->rect, color, clip_rect, 0);
  s_ui_render_border(ctx, item->rect, ctx->theme.control_border_size, border_color, clip_rect);

  text_size = s_ui_text_measure(ctx->font, item->text);

  s_ui_render_text(
      ctx,
      item->text,
      item->rect.x + (item->rect.w - text_size.w) * 0.5f,
      item->rect.y + (item->rect.h - text_size.h) * 0.5f,
      text_color,
      clip_rect);
}

static void s_ui_render_horizontal_line(LDKUIContext* ctx, LDKUIItem* item, LDKUIRect clip_rect)
{
  float line_height = 1.0f;
  LDKUIRect line_rect = item->rect;
  line_rect.y = item->rect.y + (item->rect.h - line_height) * 0.5f;
  line_rect.h = line_height;
  s_ui_render_quad(ctx, line_rect, ctx->theme.colors[LDK_UI_COLOR_SEPARATOR], clip_rect, 0);
}

static void s_ui_render_toggle_button(LDKUIContext* ctx, LDKUIItem* item, LDKUIRect clip_rect)
{
  bool toggled = item->data.toggle_button.value;
  LDKUIControlVisualState state = s_ui_render_control_visual_state(ctx, item);
  u32 color = s_ui_render_control_bg_color(ctx, state);
  u32 text_color = s_ui_render_control_text_color(ctx, state);
  u32 border_color = s_ui_render_control_border_color(ctx, state);
  LDKUISize text_size;

  if (!item->disabled && toggled)
  {
    color = ctx->theme.colors[LDK_UI_COLOR_FOCUS];

    if (state == LDK_UI_CONTROL_VISUAL_STATE_HOVERED)
    {
      color = ctx->theme.colors[LDK_UI_COLOR_CONTROL_BG_ACTIVE_HOVERED];
    }
  }

  s_ui_render_quad(ctx, item->rect, color, clip_rect, 0);
  s_ui_render_border(ctx, item->rect, ctx->theme.control_border_size, border_color, clip_rect);

  text_size = s_ui_text_measure(ctx->font, item->text);

  s_ui_render_text(
      ctx,
      item->text,
      item->rect.x + (item->rect.w - text_size.w) * 0.5f,
      item->rect.y + (item->rect.h - text_size.h) * 0.5f,
      text_color,
      clip_rect);
}

static float s_ui_render_slider_normalized_value(LDKUIItem* item)
{
  float range = item->data.slider.max_value - item->data.slider.min_value;

  if (range <= 0.0f)
  {
    return 0.0f;
  }

  return s_ui_clampf((item->data.slider.value - item->data.slider.min_value) / range, 0.0f, 1.0f);
}

static void s_ui_render_slider(LDKUIContext* ctx, LDKUIItem* item, LDKUIRect clip_rect)
{
  float t = s_ui_render_slider_normalized_value(item);
  float track_height_factor = s_ui_clampf(ctx->theme.slider_track_height, 0.0f, 1.0f);
  float thumb_width_factor = s_ui_clampf(ctx->theme.slider_thumb_width, 0.0f, 1.0f);
  float track_height = item->rect.h * track_height_factor;
  float thumb_width = s_ui_minf(item->rect.h * thumb_width_factor, item->rect.w);
  LDKUIRect track_rect = item->rect;
  LDKUIRect fill_rect = item->rect;
  LDKUIRect thumb_rect = item->rect;
  u32 track_color = ctx->theme.colors[LDK_UI_COLOR_SLIDER_TRACK];
  u32 fill_color = ctx->theme.colors[LDK_UI_COLOR_SLIDER_FILL];
  u32 thumb_color = ctx->theme.colors[LDK_UI_COLOR_SLIDER_THUMB];
  LDKUIControlVisualState state = s_ui_render_control_visual_state(ctx, item);
  u32 border_color = s_ui_render_control_border_color(ctx, state);

  if (state == LDK_UI_CONTROL_VISUAL_STATE_ACTIVE || state == LDK_UI_CONTROL_VISUAL_STATE_ACTIVE_HOVERED)
  {
    track_color = ctx->theme.colors[LDK_UI_COLOR_SLIDER_TRACK_ACTIVE];
    thumb_color = ctx->theme.colors[LDK_UI_COLOR_SLIDER_THUMB_ACTIVE];
  }
  else if (state == LDK_UI_CONTROL_VISUAL_STATE_HOVERED)
  {
    track_color = ctx->theme.colors[LDK_UI_COLOR_SLIDER_TRACK_HOVERED];
    thumb_color = ctx->theme.colors[LDK_UI_COLOR_SLIDER_THUMB_HOVERED];
  }

  track_rect.h = track_height;
  track_rect.y = item->rect.y + (item->rect.h - track_height) * 0.5f;

  fill_rect = track_rect;
  fill_rect.w = thumb_width * 0.5f + (track_rect.w - thumb_width) * t;

  thumb_rect.x = item->rect.x + (item->rect.w - thumb_width) * t;
  thumb_rect.y = item->rect.y;
  thumb_rect.w = thumb_width;
  thumb_rect.h = item->rect.h;

  s_ui_render_quad(ctx, track_rect, track_color, clip_rect, 0);
  s_ui_render_quad(ctx, fill_rect, fill_color, clip_rect, 0);
  s_ui_render_quad(ctx, thumb_rect, thumb_color, clip_rect, 0);
  s_ui_render_border(ctx, item->rect, ctx->theme.control_border_size, border_color, clip_rect);
}

static void s_ui_render_scroll_area(LDKUIContext* ctx, LDKUIItem* item, LDKUIRect clip_rect)
{
  if (item->data.scroll_area.has_vertical_scrollbar)
  {
    LDKUIId thumb_id = s_ui_id_hash_u32(item->id, (u32)LDK_UI_INTERNAL_ID_SCROLLBAR_THUMB_Y);
    u32 track_color = ctx->theme.colors[LDK_UI_COLOR_SCROLLBAR_TRACK];
    u32 thumb_color = ctx->theme.colors[LDK_UI_COLOR_SCROLLBAR_THUMB];

    if (ctx->active_id == thumb_id)
    {
      thumb_color = ctx->theme.colors[LDK_UI_COLOR_SCROLLBAR_THUMB_ACTIVE];
    }
    else if (ctx->hot_id == thumb_id)
    {
      thumb_color = ctx->theme.colors[LDK_UI_COLOR_SCROLLBAR_THUMB_HOVERED];
    }

    s_ui_render_quad(ctx, item->data.scroll_area.vertical_track_rect, track_color, clip_rect, 0);
    s_ui_render_quad(ctx, item->data.scroll_area.vertical_thumb_rect, thumb_color, clip_rect, 0);
  }

  if (item->data.scroll_area.has_horizontal_scrollbar)
  {
    LDKUIId thumb_id = s_ui_id_hash_u32(item->id, (u32)LDK_UI_INTERNAL_ID_SCROLLBAR_THUMB_X);
    u32 track_color = ctx->theme.colors[LDK_UI_COLOR_SCROLLBAR_TRACK];
    u32 thumb_color = ctx->theme.colors[LDK_UI_COLOR_SCROLLBAR_THUMB];

    if (ctx->active_id == thumb_id)
    {
      thumb_color = ctx->theme.colors[LDK_UI_COLOR_SCROLLBAR_THUMB_ACTIVE];
    }
    else if (ctx->hot_id == thumb_id)
    {
      thumb_color = ctx->theme.colors[LDK_UI_COLOR_SCROLLBAR_THUMB_HOVERED];
    }

    s_ui_render_quad(ctx, item->data.scroll_area.horizontal_track_rect, track_color, clip_rect, 0);
    s_ui_render_quad(ctx, item->data.scroll_area.horizontal_thumb_rect, thumb_color, clip_rect, 0);
  }
}

static void s_ui_render_color_view(LDKUIContext* ctx, LDKUIItem* item, LDKUIRect clip_rect)
{
  LDKUIControlVisualState state = s_ui_render_control_visual_state(ctx, item);
  u32 color = s_ui_render_control_bg_color(ctx, state);
  u32 text_color =  s_ui_text_contrast_for_bg(item->data.color_view.color);
  s_ui_render_control_text_color(ctx, state);
  u32 border_color = s_ui_render_control_border_color(ctx, state);
  LDKUISize text_size;

  const char* text = item->data.color_view.label;
  s_ui_render_quad(ctx, item->rect, item->data.color_view.color, clip_rect, 0);
  s_ui_render_border(ctx, item->rect, ctx->theme.control_border_size, border_color, clip_rect);
  text_size = s_ui_text_measure(ctx->font, text);

  s_ui_render_text(
      ctx,
      text,
      item->rect.x + (item->rect.w - text_size.w) * 0.5f,
      item->rect.y + (item->rect.h - text_size.h) * 0.5f,
      text_color,
      clip_rect);
}

static void s_ui_render_text_box(LDKUIContext* ctx, LDKUIItem* item, LDKUIRect clip_rect)
{
  float padding_x = 6.0f;
  LDKUISize text_size = s_ui_text_measure(ctx->font, item->text);
  LDKUIRect inner_clip = s_ui_rect_intersect(&clip_rect, &item->rect);
  LDKUIRect cursor_rect = {0};
  u32 bg_color = ctx->theme.colors[LDK_UI_COLOR_CONTROL_BG];
  LDKUIControlVisualState state = s_ui_render_control_visual_state(ctx, item);
  u32 border_color = s_ui_render_control_border_color(ctx, state);
  u32 text_color = s_ui_render_control_text_color(ctx, state);
  u32 selection_color = ctx->theme.colors[LDK_UI_COLOR_FOCUS];
  u32 cursor_color = text_color;
  float text_x = item->rect.x + padding_x;
  float text_y = item->rect.y + (item->rect.h - text_size.h) * 0.5f;
  u32 select_start = item->data.text_box.select_start;
  u32 select_end = item->data.text_box.select_end;

  if (state == LDK_UI_CONTROL_VISUAL_STATE_HOVERED)
  {
    bg_color = ctx->theme.colors[LDK_UI_COLOR_CONTROL_BG_HOVERED];
  }

  if (!item->disabled && item->data.text_box.focused)
  {
    border_color = ctx->theme.colors[LDK_UI_COLOR_FOCUS];
  }

  s_ui_render_quad(ctx, item->rect, bg_color, clip_rect, 0);

  if (select_start != select_end)
  {
    if (select_start > select_end)
    {
      u32 temp = select_start;
      select_start = select_end;
      select_end = temp;
    }

    {
      float selection_x0 = text_x + s_ui_text_measure_bytes(ctx->font, item->text, select_start);
      float selection_x1 = text_x + s_ui_text_measure_bytes(ctx->font, item->text, select_end);
      LDKUIRect selection_rect = { selection_x0, item->rect.y + 3.0f, selection_x1 - selection_x0, item->rect.h - 6.0f };

      if (selection_rect.w > 0.0f && ctx->text_box_id == item->id)
      {
        s_ui_render_quad(ctx, selection_rect, selection_color, inner_clip, 0);
      }
    }
  }

  s_ui_render_text(
      ctx,
      item->text,
      text_x,
      text_y,
      text_color,
      inner_clip);

  if (item->data.text_box.focused)
  {
    float cursor_x = text_x + s_ui_text_measure_bytes(ctx->font, item->text, item->data.text_box.cursor);
    cursor_rect.x = cursor_x;
    cursor_rect.y = item->rect.y + 3.0f;
    cursor_rect.w = 1.0f;
    cursor_rect.h = item->rect.h - 6.0f;
    s_ui_render_quad(ctx, cursor_rect, cursor_color, inner_clip, 0);
  }

  s_ui_render_border(ctx, item->rect, ctx->theme.control_border_size, border_color, clip_rect);
}

static LDKUIWindow* s_ui_interact_window_at_cursor_topmost(LDKUIContext* ctx)
{
  LDKPoint cursor = ldk_os_mouse_cursor((LDKMouseState*)ctx->mouse);
  u32 count = x_array_ldk_ui_window_count(ctx->windows);

  for (u32 i = count; i > 0; --i)
  {
    LDKUIWindow* window = x_array_ldk_ui_window_get(ctx->windows, i - 1);

    if (window == NULL)
    {
      continue;
    }

    if (window->root_layout == NULL)
    {
      continue;
    }

    LDKUIRect window_rect = s_ui_window_rect(window);

    if (s_ui_rect_contains(&window_rect, (float)cursor.x, (float)cursor.y))
    {
      return window;
    }
  }

  return NULL;
}

static void s_ui_render_item(LDKUIContext* ctx, LDKUIItem* item, LDKUIRect clip_rect)
{
  if (item->type == LDK_UI_ITEM_LABEL)
  {
    s_ui_render_label(ctx, item, clip_rect);
#ifdef LDK_UI_DEBUG_DRAW
    // labes dont get HOT so we need to be specific here
    s_ui_render_debug_rect(ctx, item->rect, 0x00ff00ffu);
#endif
  }
  else if (item->type == LDK_UI_ITEM_BUTTON)
    s_ui_render_button(ctx, item, clip_rect);
  else if (item->type == LDK_UI_ITEM_TOGGLE_BUTTON)
    s_ui_render_toggle_button(ctx, item, clip_rect);
  else if (item->type == LDK_UI_ITEM_SLIDER)
    s_ui_render_slider(ctx, item, clip_rect);
  else if (item->type == LDK_UI_ITEM_SCROLL_AREA)
    s_ui_render_scroll_area(ctx, item, clip_rect);
  else if (item->type == LDK_UI_ITEM_COLOR_VIEW)
    s_ui_render_color_view(ctx, item, clip_rect);
  else if (item->type == LDK_UI_ITEM_TEXT_BOX)
    s_ui_render_text_box(ctx, item, clip_rect);
  else if (item->type == LDK_UI_ITEM_HORIZONTAL_LINE)
    s_ui_render_horizontal_line(ctx, item, clip_rect);

#ifdef LDK_UI_DEBUG_DRAW
  if (ctx->hot_id == item->id)
  {
    LDKPoint cursor = ldk_os_mouse_cursor((LDKMouseState*)ctx->mouse);
    LDKUIRect debug_clip = s_ui_rect_intersect(&clip_rect, &item->rect);

    if (s_ui_rect_contains(&debug_clip, (float)cursor.x, (float)cursor.y))
    {
      s_ui_render_debug_rect(ctx, item->rect, 0x00ff00ffu);
      s_ui_render_debug_rect(ctx, clip_rect, 0xff00ffffu);
      s_ui_render_debug_rect(ctx, debug_clip, 0xffff00ffu);
    }
  }
#endif
}

//----------------------------------------------------------
// Public API: Lifecycle
//----------------------------------------------------------

bool ldk_ui_initialize(LDKUIContext* ctx, LDKUIConfig const* config)
{
  memset(ctx, 0, sizeof(*ctx));

  ctx->frame_arena = x_arena_create(config->frame_arena_size);
  ctx->windows = x_array_ldk_ui_window_create(config->initial_window_capacity);
  ctx->id_stack = x_array_ldk_ui_id_create(config->initial_id_stack_capacity);
  ctx->vertices = x_array_ldk_ui_vertex_create(config->initial_vertex_capacity);
  ctx->indices = x_array_ldk_ui_u32_create(config->initial_index_capacity);
  ctx->commands = x_array_ldk_ui_draw_cmd_create(config->initial_command_capacity);
  ctx->widget_states = x_array_ldk_ui_widget_state_create(256);
  ctx->disabled_stack = x_array_ldk_ui_bool_create(8);

  ctx->font_texture_user = config->font_texture_user;
  ctx->get_font_page_texture = config->get_font_page_texture;
  ldk_ui_set_theme(ctx, config->theme, NULL);

  ctx->font = config->font;
  ldk_font_preload_basic_ascii(ctx->font);

  bool success = ctx->frame_arena != NULL;
  return success;
}

void ldk_ui_terminate(LDKUIContext* ctx)
{

  // TODO: Destroy font
  //ldk_font_face_destroy();

  x_array_destroy(ctx->windows);
  x_array_destroy(ctx->id_stack);
  x_array_destroy(ctx->vertices);
  x_array_destroy(ctx->indices);
  x_array_destroy(ctx->commands);
  x_array_destroy(ctx->disabled_stack);
  x_arena_destroy(ctx->frame_arena);

  memset(ctx, 0, sizeof(*ctx));
}

void ldk_ui_begin_frame(LDKUIContext* ctx, LDKMouseState const* mouse, LDKKeyboardState const* keyboard, LDKUITextInputState const* text_input, LDKUIRect viewport)
{
  u32 count = x_array_ldk_ui_window_count(ctx->windows);
  ctx->mouse = mouse;
  ctx->keyboard = keyboard;
  ctx->input_text = text_input;
  ctx->viewport = viewport;
  ctx->root_item_count = 0;
  ctx->hot_id = 0;
  ctx->hovered_window = NULL;
  ctx->current_window = NULL;
  ctx->current_layout = NULL;
  ctx->next_disabled = false;
  ctx->next_focus = false;

  s_ui_submit_reset_next_layout(ctx);

  x_array_ldk_ui_bool_clear(ctx->disabled_stack);

  x_arena_reset_keep_head(ctx->frame_arena);
  x_array_ldk_ui_id_clear(ctx->id_stack);
  x_array_ldk_ui_vertex_clear(ctx->vertices);
  x_array_ldk_ui_u32_clear(ctx->indices);
  x_array_ldk_ui_draw_cmd_clear(ctx->commands);
  ctx->hovered_window = s_ui_interact_window_at_cursor_topmost(ctx);

  for (u32 i = 0; i < count; ++i)
  {
    LDKUIWindow* window = x_array_ldk_ui_window_get(ctx->windows, i);

    if (window == NULL)
    {
      continue;
    }

    if (window->root_layout == NULL)
    {
      continue;
    }
    window->root_layout = NULL;
  }
}

void ldk_ui_end_frame(LDKUIContext* ctx)
{
  u32 count = x_array_ldk_ui_window_count(ctx->windows);

  LDK_ASSERT(ctx != NULL);
  LDK_ASSERT(ctx->current_window == NULL);
  LDK_ASSERT(ctx->current_layout == NULL);
  LDK_ASSERT(x_array_ldk_ui_id_count(ctx->id_stack) == 0);

  for (u32 i = 0; i < count; ++i)
  {
    LDKUIWindow* window = x_array_ldk_ui_window_get(ctx->windows, i);

    if (window == NULL)
    {
      continue;
    }

    if (window->root_layout == NULL)
    {
      continue;
    }


    {
      LDKUIRect window_rect = s_ui_window_rect(window);
      u32 window_bg = ctx->theme.colors[LDK_UI_COLOR_WINDOW_BG];
      u32 title_bg = ctx->theme.colors[LDK_UI_COLOR_TITLE_BAR];

      if (ctx->focused_window == window)
      {
        title_bg = ctx->theme.colors[LDK_UI_COLOR_TITLE_BAR_FOCUSED];
      }

      s_ui_render_quad(ctx, window_rect, window_bg, window_rect, 0);
      s_ui_render_border(ctx, window_rect, ctx->theme.window_border_size, ctx->theme.colors[LDK_UI_COLOR_BORDER], window_rect);

      if (window->title_bar_rect.h > 0.0f)
      {
        s_ui_render_quad(ctx, window->title_bar_rect, title_bg, window_rect, 0);
        float text_x = window->title_bar_rect.x + 6.0f;
        float font_line_height = ldk_font_get_line_height(ctx->font);
        float text_y = window->title_bar_rect.y + (window->title_bar_rect.h - font_line_height) * 0.5f;
        s_ui_render_text(
            ctx,
            window->title,
            text_x, text_y,
            ctx->theme.colors[LDK_UI_COLOR_TEXT],
            window->title_bar_rect);
      }
    }
    s_ui_layout_measure_node(window->root_layout);
    s_ui_layout_resolve_node(ctx, window->root_layout, window->content_rect, window->content_rect);
  }

  s_ui_state_widgets_gc(ctx);
  s_ui_state_windows_gc(ctx);

  ctx->render_data.vertices = x_array_ldk_ui_vertex_data_const(ctx->vertices);
  ctx->render_data.vertex_count = x_array_ldk_ui_vertex_count(ctx->vertices);
  ctx->render_data.indices = x_array_ldk_ui_u32_data_const(ctx->indices);
  ctx->render_data.index_count = x_array_ldk_ui_u32_count(ctx->indices);
  ctx->render_data.commands = x_array_ldk_ui_draw_cmd_data_const(ctx->commands);
  ctx->render_data.command_count = x_array_ldk_ui_draw_cmd_count(ctx->commands);
}

LDKUIRenderData const* ldk_ui_get_render_data(LDKUIContext const* ctx)
{
  return &ctx->render_data;
}


//----------------------------------------------------------
// Public API: Explicit IDs
//----------------------------------------------------------

void ldk_ui_push_id_u32(LDKUIContext* ctx, u32 value)
{
  x_array_ldk_ui_id_push(ctx->id_stack, value);
}

void ldk_ui_push_id_ptr(LDKUIContext* ctx, void const* value)
{
  uintptr_t raw = (uintptr_t)value;
  LDKUIId hashed = (LDKUIId)(raw ^ (raw >> 32));

  x_array_ldk_ui_id_push(ctx->id_stack, hashed);
}

void ldk_ui_push_id_cstr(LDKUIContext* ctx, char const* value)
{
  LDKUIId hashed = s_ui_id_hash_cstr(2166136261u, value);
  x_array_ldk_ui_id_push(ctx->id_stack, hashed);
}

void ldk_ui_pop_id(LDKUIContext* ctx)
{
  if (!x_array_ldk_ui_id_is_empty(ctx->id_stack))
  {
    x_array_ldk_ui_id_pop(ctx->id_stack);
  }
}

//----------------------------------------------------------
// Public API: Theme
//----------------------------------------------------------

void ldk_ui_set_theme(LDKUIContext* ctx, LDKUIThemeType type, LDKUITheme* custom)
{
  LDK_ASSERT(ctx != NULL);
  LDKUITheme* theme = &ctx->theme;

  if (type == LDK_UI_THEME_DEFAULT_DARK)
  {
    rgba32 text          = (0xd8d8d8FFu);
    rgba32 text_disabled = (0x707070FFu);
    rgba32 bg            = (0x333333EEu);
    rgba32 panel         = (0x252525EEu);
    rgba32 control       = (0x444444FFu);
    rgba32 hover         = (0x505050FFu);
    rgba32 active        = (0x38b8a4FFu);
    rgba32 active_hover  = (0x43c8b3FFu);
    rgba32 border        = (0x1c1c1cFFu);
    rgba32 accent        = (0x38b8a4FFu);
    rgba32 dark_track    = (0x202020FFu);
    rgba32 title         = (0x3f3f3fFFu);
    rgba32 title_focus   = (0x2a2a2aFFu);
    rgba32 scrollbar     = (0x202020FFu);
    rgba32 thumb         = (0x555555FFu);
    rgba32 thumb_hover   = (0x666666FFu);
    rgba32 thumb_active  = (0x38b8a4FFu);

    theme->colors[LDK_UI_COLOR_TEXT]                       = text;
    theme->colors[LDK_UI_COLOR_TEXT_DISABLED]              = text_disabled;
    theme->colors[LDK_UI_COLOR_WINDOW_BG]                  = bg;
    theme->colors[LDK_UI_COLOR_PANEL_BG]                   = panel;
    theme->colors[LDK_UI_COLOR_CONTROL_BG]                 = control;
    theme->colors[LDK_UI_COLOR_CONTROL_BG_HOVERED]         = hover;
    theme->colors[LDK_UI_COLOR_CONTROL_BG_ACTIVE]          = active;
    theme->colors[LDK_UI_COLOR_CONTROL_BG_ACTIVE_HOVERED]  = active_hover;
    theme->colors[LDK_UI_COLOR_CONTROL_TEXT]               = text;
    theme->colors[LDK_UI_COLOR_CONTROL_TEXT_HOVERED]       = text;
    theme->colors[LDK_UI_COLOR_CONTROL_TEXT_ACTIVE]        = text;
    theme->colors[LDK_UI_COLOR_CONTROL_TEXT_ACTIVE_HOVERED] = text;
    theme->colors[LDK_UI_COLOR_CONTROL_TEXT_DISABLED]      = text_disabled;
    theme->colors[LDK_UI_COLOR_CONTROL_BORDER]             = border;
    theme->colors[LDK_UI_COLOR_CONTROL_BORDER_HOVERED]     = border;
    theme->colors[LDK_UI_COLOR_CONTROL_BORDER_ACTIVE]      = accent;
    theme->colors[LDK_UI_COLOR_CONTROL_BORDER_ACTIVE_HOVERED] = accent;
    theme->colors[LDK_UI_COLOR_CONTROL_BORDER_DISABLED]    = text_disabled;
    theme->colors[LDK_UI_COLOR_BORDER]                     = border;
    theme->colors[LDK_UI_COLOR_FOCUS]                      = accent;
    theme->colors[LDK_UI_COLOR_SLIDER_TRACK]               = dark_track;
    theme->colors[LDK_UI_COLOR_SLIDER_TRACK_HOVERED]       = hover;
    theme->colors[LDK_UI_COLOR_SLIDER_TRACK_ACTIVE]        = control;
    theme->colors[LDK_UI_COLOR_SLIDER_FILL]                = 0x0;
    theme->colors[LDK_UI_COLOR_SLIDER_THUMB]               = thumb;
    theme->colors[LDK_UI_COLOR_SLIDER_THUMB_HOVERED]       = thumb_hover;
    theme->colors[LDK_UI_COLOR_SLIDER_THUMB_ACTIVE]        = thumb_active;
    theme->colors[LDK_UI_COLOR_TITLE_BAR]                  = title;
    theme->colors[LDK_UI_COLOR_TITLE_BAR_FOCUSED]          = title_focus;
    theme->colors[LDK_UI_COLOR_SCROLLBAR_TRACK]            = scrollbar;
    theme->colors[LDK_UI_COLOR_SCROLLBAR_THUMB]            = thumb;
    theme->colors[LDK_UI_COLOR_SCROLLBAR_THUMB_HOVERED]    = thumb_hover;
    theme->colors[LDK_UI_COLOR_SCROLLBAR_THUMB_ACTIVE]     = thumb_active;
    theme->colors[LDK_UI_COLOR_SEPARATOR]                  = thumb_hover;

    theme->control_border_size                             = 1.0f;
    theme->window_border_size                              = 1.0f;
    theme->window_interaction_border_size                  = 8.0f;
    theme->slider_track_height                             = 0.27272728f;
    theme->slider_thumb_width                              = 0.63636363f;
  }
  else if (type == LDK_UI_THEME_DEFAULT_LIGHT)
  {
    rgba32 text          = (0x202020ffu);
    rgba32 text_disabled = (0xa0a0a0ffu);
    rgba32 bg            = (0xf0f0f0ffu);
    rgba32 panel         = (0xe0e0e0ffu);
    rgba32 control       = (0xd0d0d0ffu);
    rgba32 hover         = (0xc0c0c0ffu);
    rgba32 active        = (0xb0b0b0ffu);
    rgba32 active_hover  = (0xb4b4b4ffu);
    rgba32 border        = (0xa0a0a0ffu);
    rgba32 accent        = (0x4f8cc9ffu);
    rgba32 track         = (0xc0c0c0ffu);
    rgba32 title         = (0xdcdcdcffu);
    rgba32 title_focus   = (0xbfcfffffu);
    rgba32 scrollbar     = (0xc8c8c8ffu);
    rgba32 thumb         = (0xa8a8a8ffu);
    rgba32 thumb_hover   = (0x909090ffu);
    rgba32 thumb_active  = accent;

    theme->colors[LDK_UI_COLOR_TEXT]                       = text;
    theme->colors[LDK_UI_COLOR_TEXT_DISABLED]              = text_disabled;
    theme->colors[LDK_UI_COLOR_WINDOW_BG]                  = bg;
    theme->colors[LDK_UI_COLOR_PANEL_BG]                   = panel;
    theme->colors[LDK_UI_COLOR_CONTROL_BG]                 = control;
    theme->colors[LDK_UI_COLOR_CONTROL_BG_HOVERED]         = hover;
    theme->colors[LDK_UI_COLOR_CONTROL_BG_ACTIVE]          = active;
    theme->colors[LDK_UI_COLOR_CONTROL_BG_ACTIVE_HOVERED]  = active_hover;
    theme->colors[LDK_UI_COLOR_CONTROL_TEXT]               = text;
    theme->colors[LDK_UI_COLOR_CONTROL_TEXT_HOVERED]       = text;
    theme->colors[LDK_UI_COLOR_CONTROL_TEXT_ACTIVE]        = text;
    theme->colors[LDK_UI_COLOR_CONTROL_TEXT_ACTIVE_HOVERED] = text;
    theme->colors[LDK_UI_COLOR_CONTROL_TEXT_DISABLED]      = text_disabled;
    theme->colors[LDK_UI_COLOR_CONTROL_BORDER]             = border;
    theme->colors[LDK_UI_COLOR_CONTROL_BORDER_HOVERED]     = border;
    theme->colors[LDK_UI_COLOR_CONTROL_BORDER_ACTIVE]      = accent;
    theme->colors[LDK_UI_COLOR_CONTROL_BORDER_ACTIVE_HOVERED] = accent;
    theme->colors[LDK_UI_COLOR_CONTROL_BORDER_DISABLED]    = text_disabled;
    theme->colors[LDK_UI_COLOR_BORDER]                     = border;
    theme->colors[LDK_UI_COLOR_FOCUS]                      = accent;
    theme->colors[LDK_UI_COLOR_SLIDER_TRACK]               = track;
    theme->colors[LDK_UI_COLOR_SLIDER_TRACK_HOVERED]       = hover;
    theme->colors[LDK_UI_COLOR_SLIDER_TRACK_ACTIVE]        = active;
    theme->colors[LDK_UI_COLOR_SLIDER_FILL]                = accent;
    theme->colors[LDK_UI_COLOR_SLIDER_THUMB]               = accent;
    theme->colors[LDK_UI_COLOR_SLIDER_THUMB_HOVERED]       = active_hover;
    theme->colors[LDK_UI_COLOR_SLIDER_THUMB_ACTIVE]        = accent;
    theme->colors[LDK_UI_COLOR_TITLE_BAR]                  = title;
    theme->colors[LDK_UI_COLOR_TITLE_BAR_FOCUSED]          = title_focus;
    theme->colors[LDK_UI_COLOR_SCROLLBAR_TRACK]            = scrollbar;
    theme->colors[LDK_UI_COLOR_SCROLLBAR_THUMB]            = thumb;
    theme->colors[LDK_UI_COLOR_SCROLLBAR_THUMB_HOVERED]    = thumb_hover;
    theme->colors[LDK_UI_COLOR_SCROLLBAR_THUMB_ACTIVE]     = thumb_active;
    theme->colors[LDK_UI_COLOR_SEPARATOR]                  = thumb_hover;
    theme->control_border_size                             = 1.0f;
    theme->window_border_size                              = 1.0f;
    theme->window_interaction_border_size                  = 8.0f;
    theme->slider_track_height                             = 0.27272728f;
    theme->slider_thumb_width                              = 0.63636363f;
  }
  else if (type == LDK_UI_THEME_CUSTOM && custom != NULL)
  {
    memcpy(&ctx->theme, custom, sizeof(*custom));
  }
  else
  {
    ldk_ui_set_theme(ctx, LDK_UI_THEME_DEFAULT_LIGHT, NULL);
  }
}

//----------------------------------------------------------
// Public API: Next item layout hints
//----------------------------------------------------------

void ldk_ui_set_next_width(LDKUIContext* ctx, float width)
{
  ctx->next_layout.has_width = true;
  ctx->next_layout.width = width;
}

void ldk_ui_set_next_height(LDKUIContext* ctx, float height)
{
  ctx->next_layout.has_height = true;
  ctx->next_layout.height = height;
}

void ldk_ui_set_next_preferred_width(LDKUIContext* ctx, float width)
{
  ldk_ui_set_next_width(ctx, width);
}

void ldk_ui_set_next_preferred_height(LDKUIContext* ctx, float height)
{
  ldk_ui_set_next_height(ctx, height);
}

void ldk_ui_set_next_preferred_size(LDKUIContext* ctx, float width, float height)
{
  ldk_ui_set_next_preferred_width(ctx, width);
  ldk_ui_set_next_preferred_height(ctx, height);
}

void ldk_ui_set_next_min_width(LDKUIContext* ctx, float width)
{
  ctx->next_layout.has_min_width = true;
  ctx->next_layout.min_width = width;
}

void ldk_ui_set_next_min_height(LDKUIContext* ctx, float height)
{
  ctx->next_layout.has_min_height = true;
  ctx->next_layout.min_height = height;
}

void ldk_ui_set_next_expand_width(LDKUIContext* ctx, bool expand)
{
  ctx->next_layout.has_expand_width = true;
  ctx->next_layout.expand_width = expand;
}

void ldk_ui_set_next_expand_height(LDKUIContext* ctx, bool expand)
{
  ctx->next_layout.has_expand_height = true;
  ctx->next_layout.expand_height = expand;
}

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

void ldk_ui_set_next_fixed_width(LDKUIContext* ui, float width)
{
  ldk_ui_set_next_preferred_width(ui, width);
  ldk_ui_set_next_min_width(ui, width);
  ldk_ui_set_next_expand_width(ui, false);
}

void ldk_ui_set_next_fixed_height(LDKUIContext* ui, float height)
{
  ldk_ui_set_next_preferred_height(ui, height);
  ldk_ui_set_next_min_height(ui, height);
  ldk_ui_set_next_expand_height(ui, false);
}

void ldk_ui_set_next_fixed_size(LDKUIContext* ui, float width, float height)
{
  ldk_ui_set_next_fixed_width(ui, width);
  ldk_ui_set_next_fixed_height(ui, height);
}

//----------------------------------------------------------
// Public API: Vertical/Horizontal layout
//----------------------------------------------------------

void ldk_ui_begin_vertical(LDKUIContext* ctx)
{
  LDK_ASSERT(ctx->current_layout != NULL);

  LDKUILayoutNode* node = s_ui_layout_node_create(
      ctx,
      LDK_UI_LAYOUT_VERTICAL,
      ctx->current_layout,
      ctx->current_window
      );
  LDKUIItem* item = s_ui_submit_item_create(ctx, LDK_UI_ITEM_LAYOUT);

  if (node == NULL || item == NULL)
  {
    return;
  }

  item->data.layout.node = node;
  node->id = item->id;
  item->preferred_width = 0.0f;
  item->preferred_height = 0.0f;

  item->expand_width = true;
  item->expand_height = false;

  s_ui_submit_item(ctx, item);

  ctx->current_layout = node;
}

void ldk_ui_end_vertical(LDKUIContext* ctx)
{
  if (ctx->current_layout != NULL)
  {
    ctx->current_layout = ctx->current_layout->parent;
  }
}

void ldk_ui_begin_horizontal(LDKUIContext* ctx)
{
  LDK_ASSERT(ctx->current_layout != NULL);
  LDKUILayoutNode* node = s_ui_layout_node_create(
      ctx,
      LDK_UI_LAYOUT_HORIZONTAL,
      ctx->current_layout,
      ctx->current_window
      );
  LDKUIItem* item = s_ui_submit_item_create(ctx, LDK_UI_ITEM_LAYOUT);

  if (node == NULL || item == NULL)
  {
    return;
  }

  item->data.layout.node = node;
  node->id = item->id;
  item->preferred_width = 0.0f;
  item->preferred_height = 0.0f;

  item->expand_width = true;
  item->expand_height = false;

  s_ui_submit_item(ctx, item);

  ctx->current_layout = node;
}

void ldk_ui_end_horizontal(LDKUIContext* ctx)
{
  if (ctx->current_layout != NULL)
  {
    ctx->current_layout = ctx->current_layout->parent;
  }
}


//----------------------------------------------------------
// Public API: Window and Pane creation
//----------------------------------------------------------

bool ldk_ui_begin_pane(LDKUIContext* ctx, LDKUIRect rect)
{
  LDKUIId id = s_ui_id_make(ctx, LDK_UI_ITEM_LAYOUT);
  return s_ui_begin_root_container(ctx, id, NULL, rect, false, false, NULL);
}

void ldk_ui_end_pane(LDKUIContext* ctx)
{
  ctx->current_layout = NULL;
  ctx->current_window = NULL;
}

LDKUIRect ldk_ui_begin_window(LDKUIContext* ctx, char const* title, LDKUIRect rect)
{
  LDKUIId id = s_ui_id_make(ctx, LDK_UI_ITEM_LAYOUT);
  LDKUIRect out_rect = rect;
  s_ui_begin_root_container(ctx, id, title, rect, true, true, &out_rect);
  return out_rect;
}

void ldk_ui_end_window(LDKUIContext* ctx)
{
  ldk_ui_end_pane(ctx);
}


//----------------------------------------------------------
// Public API: Common widgets
//----------------------------------------------------------

LDKUIPoint ldk_ui_begin_scroll_area(LDKUIContext* ctx, LDKUIPoint scroll, LDKUIScrollFlags flags)
{
  LDKUILayoutNode* node = NULL;
  LDKUIItem* item = NULL;
  LDKUIId id = 0;
  LDKUIWidgetState* state = NULL;

  LDKUIRect vertical_track_rect = {0};
  LDKUIRect vertical_thumb_rect = {0};
  LDKUIRect horizontal_track_rect = {0};
  LDKUIRect horizontal_thumb_rect = {0};

  bool has_vertical_scrollbar = false;
  bool has_horizontal_scrollbar = false;

  LDK_ASSERT(ctx->current_layout != NULL);

  item = s_ui_submit_item_create(ctx, LDK_UI_ITEM_SCROLL_AREA);

  if (item == NULL)
  {
    return scroll;
  }

  id = item->id;
  bool disabled = item->disabled;
  state = s_ui_state_widget_find(ctx, id);

  if ((flags & LDK_UI_SCROLL_HORIZONTAL) == 0)
  {
    scroll.x = 0.0f;
  }

  if ((flags & LDK_UI_SCROLL_VERTICAL) == 0)
  {
    scroll.y = 0.0f;
  }

  if (state != NULL)
  {
    LDKPoint cursor = ldk_os_mouse_cursor((LDKMouseState*)ctx->mouse);
    float max_x = s_ui_maxf(0.0f, state->content_size.w - state->rect.w);
    float max_y = s_ui_maxf(0.0f, state->content_size.h - state->rect.h);

    scroll.x = s_ui_clampf(scroll.x, 0.0f, max_x);
    scroll.y = s_ui_clampf(scroll.y, 0.0f, max_y);

    if ((flags & LDK_UI_SCROLL_VERTICAL) != 0)
    {
      has_vertical_scrollbar = s_ui_layout_scrollbar_rects(
          state->rect,
          state->rect.h,
          state->content_size.h,
          scroll.y,
          false,
          &vertical_track_rect,
          &vertical_thumb_rect);
    }

    if ((flags & LDK_UI_SCROLL_HORIZONTAL) != 0)
    {
      has_horizontal_scrollbar = s_ui_layout_scrollbar_rects(
          state->rect,
          state->rect.w,
          state->content_size.w,
          scroll.x,
          true,
          &horizontal_track_rect,
          &horizontal_thumb_rect);
    }

    if (disabled)
    {
      LDKUIId vertical_thumb_id = s_ui_id_hash_u32(id, (u32)LDK_UI_INTERNAL_ID_SCROLLBAR_THUMB_Y);
      LDKUIId horizontal_thumb_id = s_ui_id_hash_u32(id, (u32)LDK_UI_INTERNAL_ID_SCROLLBAR_THUMB_X);

      if (ctx->active_id == vertical_thumb_id || ctx->active_id == horizontal_thumb_id)
      {
        ctx->active_id = 0;
      }
    }

    if (!disabled && s_ui_interact_window_can_interact(ctx, ctx->current_window))
    {
      if (s_ui_rect_contains(&state->clip_rect, (float)cursor.x, (float)cursor.y))
      {
        i32 wheel_delta = ldk_os_mouse_wheel_delta((LDKMouseState*)ctx->mouse);

        if (wheel_delta != 0 && (flags & LDK_UI_SCROLL_VERTICAL) != 0)
        {
          scroll.y -= ((float)wheel_delta / 120.0f) * 32.0f;
        }
      }
    }

    if (!disabled && has_vertical_scrollbar)
    {
      LDKUIId thumb_id = s_ui_id_hash_u32(id, (u32)LDK_UI_INTERNAL_ID_SCROLLBAR_THUMB_Y);
      bool inside_thumb = s_ui_rect_contains(&vertical_thumb_rect, (float)cursor.x, (float)cursor.y);

      if (inside_thumb)
      {
        ctx->hot_id = thumb_id;
      }

      if (inside_thumb && ldk_os_mouse_button_down((LDKMouseState*)ctx->mouse, LDK_MOUSE_BUTTON_LEFT))
      {
        ctx->active_id = thumb_id;
      }

      if (ctx->active_id == thumb_id)
      {
        if (ldk_os_mouse_button_is_pressed((LDKMouseState*)ctx->mouse, LDK_MOUSE_BUTTON_LEFT))
        {
          float thumb_range = s_ui_maxf(0.0f, vertical_track_rect.h - vertical_thumb_rect.h);
          float local = (float)cursor.y - vertical_track_rect.y - vertical_thumb_rect.h * 0.5f;
          float t = 0.0f;

          if (thumb_range > 0.0f)
          {
            t = s_ui_clampf(local / thumb_range, 0.0f, 1.0f);
          }

          scroll.y = t * max_y;
        }
        else
        {
          ctx->active_id = 0;
        }
      }
    }

    if (!disabled && has_horizontal_scrollbar)
    {
      LDKUIId thumb_id = s_ui_id_hash_u32(id, (u32)LDK_UI_INTERNAL_ID_SCROLLBAR_THUMB_X);
      bool inside_thumb = s_ui_rect_contains(&horizontal_thumb_rect, (float)cursor.x, (float)cursor.y);

      if (inside_thumb)
      {
        ctx->hot_id = thumb_id;
      }

      if (inside_thumb && ldk_os_mouse_button_down((LDKMouseState*)ctx->mouse, LDK_MOUSE_BUTTON_LEFT))
      {
        ctx->active_id = thumb_id;
      }

      if (ctx->active_id == thumb_id)
      {
        if (ldk_os_mouse_button_is_pressed((LDKMouseState*)ctx->mouse, LDK_MOUSE_BUTTON_LEFT))
        {
          float thumb_range = s_ui_maxf(0.0f, horizontal_track_rect.w - horizontal_thumb_rect.w);
          float local = (float)cursor.x - horizontal_track_rect.x - horizontal_thumb_rect.w * 0.5f;
          float t = 0.0f;

          if (thumb_range > 0.0f)
          {
            t = s_ui_clampf(local / thumb_range, 0.0f, 1.0f);
          }

          scroll.x = t * max_x;
        }
        else
        {
          ctx->active_id = 0;
        }
      }
    }

    scroll.x = s_ui_clampf(scroll.x, 0.0f, max_x);
    scroll.y = s_ui_clampf(scroll.y, 0.0f, max_y);
  }
  else
  {
    scroll.x = s_ui_maxf(0.0f, scroll.x);
    scroll.y = s_ui_maxf(0.0f, scroll.y);
  }

  node = s_ui_layout_node_create(
      ctx,
      LDK_UI_LAYOUT_VERTICAL,
      ctx->current_layout,
      ctx->current_window);

  if (node == NULL)
  {
    return scroll;
  }

  item->data.scroll_area.node = node;
  item->data.scroll_area.scroll = scroll;
  item->data.scroll_area.flags = flags;
  item->data.scroll_area.has_vertical_scrollbar = has_vertical_scrollbar;
  item->data.scroll_area.has_horizontal_scrollbar = has_horizontal_scrollbar;
  item->data.scroll_area.vertical_track_rect = vertical_track_rect;
  item->data.scroll_area.vertical_thumb_rect = vertical_thumb_rect;
  item->data.scroll_area.horizontal_track_rect = horizontal_track_rect;
  item->data.scroll_area.horizontal_thumb_rect = horizontal_thumb_rect;

  node->id = item->id;

  item->preferred_width = 0.0f;
  item->preferred_height = 0.0f;
  item->min_width = 0.0f;
  item->min_height = 0.0f;
  item->expand_width = true;
  item->expand_height = true;

  s_ui_submit_item(ctx, item);

  ctx->current_layout = node;

  return scroll;
}

LDK_API void ldk_ui_end_scroll_area(LDKUIContext* ctx)
{
  if (ctx->current_layout != NULL)
  {
    ctx->current_layout = ctx->current_layout->parent;
  }
}

LDKUIPoint ldk_ui_begin_vertical_scroll_area(LDKUIContext* ctx, LDKUIPoint scroll)
{
  return ldk_ui_begin_scroll_area(ctx, scroll, LDK_UI_SCROLL_VERTICAL);
}

void ldk_ui_end_vertical_scroll_area(LDKUIContext* ctx)
{
  ldk_ui_end_scroll_area(ctx);
}

LDKUIPoint ldk_ui_begin_horizontal_scroll_area(LDKUIContext* ctx, LDKUIPoint scroll)
{
  return ldk_ui_begin_scroll_area(ctx, scroll, LDK_UI_SCROLL_HORIZONTAL);
}

void ldk_ui_end_horizontal_scroll_area(LDKUIContext* ctx)
{
  ldk_ui_end_scroll_area(ctx);
}

void ldk_ui_label(LDKUIContext* ctx, char const* text)
{
  LDKUIItem* item = s_ui_submit_item_create(ctx, LDK_UI_ITEM_LABEL);
  LDKUISize size;

  if (item == NULL)
  {
    return;
  }

  size = s_ui_text_measure(ctx->font, text);

  item->text = text;
  item->preferred_width = size.w;
  item->preferred_height = size.h;
  item->min_width = size.w;
  item->min_height = size.h;

  s_ui_submit_item(ctx, item);
}

bool ldk_ui_button(LDKUIContext* ctx, char const* text)
{
  LDKUIItem* item = s_ui_submit_item_create(ctx, LDK_UI_ITEM_BUTTON);
  LDKUISize size;

  if (item == NULL)
  {
    return false;
  }

  size = s_ui_layout_measure_button_impl(ctx->font, text);

  item->text = text;
  item->preferred_width = size.w;
  item->preferred_height = size.h;
  item->min_width = size.w;
  item->min_height = size.h;
  item->expand_width = true;

  item->clicked = s_ui_interact_cached_press(ctx, item);

  if (!item->disabled &&
      s_ui_interact_item_is_focused(ctx, item) &&
      s_ui_input_keyboard_accept_pressed(ctx))
  {
    item->clicked = true;
  }

  s_ui_submit_item(ctx, item);

  return item->clicked;
}

bool ldk_ui_toggle_button(LDKUIContext* ctx, char const* text, bool value)
{
  LDKUIItem* item = s_ui_submit_item_create(ctx, LDK_UI_ITEM_TOGGLE_BUTTON);
  LDKUISize size;

  if (item == NULL)
  {
    return value;
  }

  size = s_ui_layout_measure_button_impl(ctx->font, text);

  item->text = text;
  item->preferred_width = size.w;
  item->preferred_height = size.h;
  item->min_width = size.w;
  item->min_height = size.h;
  item->expand_width = true;
  item->data.toggle_button.value = value;

  item->clicked = s_ui_interact_cached_press(ctx, item);

  if (!item->disabled &&
      s_ui_interact_item_is_focused(ctx, item) &&
      s_ui_input_keyboard_accept_pressed(ctx))
  {
    item->clicked = true;
  }

  if (item->clicked)
  {
    value = !value;
    item->changed = true;
  }

  item->data.toggle_button.value = value;

  s_ui_submit_item(ctx, item);

  return value;
}

float ldk_ui_slider(LDKUIContext* ctx, char const* text, float value, float min_value, float max_value)
{
  LDKUIItem* item = s_ui_submit_item_create(ctx, LDK_UI_ITEM_SLIDER);
  LDKUISize size;

  if (item == NULL)
  {
    return value;
  }

  size = s_ui_layout_measure_slider_impl(ctx, text, LDK_UI_ITEM_SLIDER);

  item->text = text;
  item->preferred_width = size.w;
  item->preferred_height = size.h;
  item->min_width = 80.0f;
  item->min_height = size.h;
  item->expand_width = true;
  item->data.slider.value = value;
  item->data.slider.min_value = min_value;
  item->data.slider.max_value = max_value;

  item->changed = s_ui_interact_cached_slider(ctx, item, &value, min_value, max_value);

  if (!item->disabled &&
      s_ui_interact_item_is_focused(ctx, item))
  {
    float step = (max_value - min_value) / 100.0f;

    if (step <= 0.0f)
    {
      step = 1.0f;
    }

    if (s_ui_input_keyboard_left_pressed(ctx))
    {
      float new_value = s_ui_clampf(value - step, min_value, max_value);

      if (new_value != value)
      {
        value = new_value;
        item->changed = true;
      }
    }

    if (s_ui_input_keyboard_right_pressed(ctx))
    {
      float new_value = s_ui_clampf(value + step, min_value, max_value);

      if (new_value != value)
      {
        value = new_value;
        item->changed = true;
      }
    }
  }

  item->data.slider.value = value;

  s_ui_submit_item(ctx, item);

  return value;
}

void ldk_ui_spacer(LDKUIContext* ctx)
{
  LDKUIItem* item = NULL;

  if (ctx == NULL)
  {
    return;
  }

  if (ctx->current_layout == NULL)
  {
    return;
  }

  item = s_ui_submit_item_create(ctx, LDK_UI_ITEM_SPACER);

  if (item == NULL)
  {
    return;
  }

  item->preferred_width = 0.0f;
  item->preferred_height = 0.0f;
  item->min_width = 0.0f;
  item->min_height = 0.0f;
  item->max_width = 0.0f;
  item->max_height = 0.0f;
  item->has_max_width = false;
  item->has_max_height = false;
  item->clicked = false;
  item->changed = false;
  item->text = NULL;

  if (ctx->current_layout->direction == LDK_UI_LAYOUT_HORIZONTAL)
  {
    item->expand_width = true;
    item->expand_height = false;
  }
  else
  {
    item->expand_width = false;
    item->expand_height = true;
  }

  s_ui_submit_item(ctx, item);
}

void ldk_ui_color_view(LDKUIContext* ctx, rgba32 color)
{
  LDKUIItem* item = s_ui_submit_item_create(ctx, LDK_UI_ITEM_COLOR_VIEW);
  LDKUISize size;

  if (item == NULL)
  {
    return ;
  }

  snprintf((char*)item->data.color_view.label,
      sizeof(item->data.color_view.label), "#%x", color);

  size = s_ui_layout_measure_button_impl(ctx->font, item->data.color_view.label);
  item->text = NULL;
  item->preferred_width = 8.0f;
  item->preferred_height = 8.0f;
  item->min_width = 8.0f;
  item->min_height = 8.0f;
  item->expand_width = true;
  item->expand_height = false;
  item->data.color_view.color = color;
  item->clicked = false;

  if (!item->disabled && s_ui_interact_item_is_focused(ctx, item)
      && s_ui_input_keyboard_accept_pressed(ctx))
  {
    item->clicked = true;
  }

  s_ui_submit_item(ctx, item);
}

u32 ldk_ui_text_box(LDKUIContext* ctx, char* buffer, u32 buffer_size)
{
  LDKUIItem* item = s_ui_submit_item_create(ctx, LDK_UI_ITEM_TEXT_BOX);
  LDKUISize size = {0};
  LDKUIWidgetState* state = NULL;
  u32 result = LDK_UI_TEXT_BOX_NONE;
  u32 buffer_len = 0;
  bool focused = false;

  if (ctx == NULL || item == NULL || buffer == NULL || buffer_size == 0)
  {
    return result;
  }

  buffer[buffer_size - 1] = 0;
  buffer_len = s_ui_text_cstr_len_u32(buffer);
  size = s_ui_layout_measure_text_box_impl(ctx, buffer);

  item->text = buffer;
  item->preferred_width = size.w;
  item->preferred_height = size.h;
  item->min_width = 80.0f;
  item->min_height = size.h;
  item->expand_width = true;

  state = s_ui_state_widget_find(ctx, item->id);

  s_ui_interact_clear_item_if_disabled(ctx, item);

  if (!item->disabled && state != NULL && s_ui_interact_window_can_interact(ctx, ctx->current_window))
  {
    LDKPoint cursor = ldk_os_mouse_cursor((LDKMouseState*)ctx->mouse);
    bool inside = s_ui_state_widget_contains(state, (float)cursor.x, (float)cursor.y);

    if (inside)
    {
      ctx->hot_id = item->id;
    }

    if (inside && ldk_os_mouse_button_down((LDKMouseState*)ctx->mouse, LDK_MOUSE_BUTTON_LEFT))
    {
      ctx->active_id = item->id;
      s_ui_interact_focus_item(ctx, item);
      ctx->text_box_id = item->id;
      ctx->text_cursor = s_ui_text_box_cursor_from_x(ctx, buffer, state->rect, (float)cursor.x);
      ctx->text_select_start = ctx->text_cursor;
      ctx->text_select_end = ctx->text_cursor;
    }
  }

  if (!item->disabled && ldk_os_mouse_button_up((LDKMouseState*)ctx->mouse, LDK_MOUSE_BUTTON_LEFT) && ctx->active_id == item->id)
  {
    ctx->active_id = 0;
  }

  focused = !item->disabled && s_ui_interact_item_is_focused(ctx, item);

  if (focused && ctx->text_box_id != item->id)
  {
    ctx->text_box_id = item->id;
    ctx->text_cursor = buffer_len;
    ctx->text_select_start = buffer_len;
    ctx->text_select_end = buffer_len;
  }

  if (focused)
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
      ctx->text_cursor = s_ui_text_utf8_prev(buffer, ctx->text_cursor);

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
      ctx->text_cursor = s_ui_text_utf8_next(buffer, ctx->text_cursor);

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
        u32 start = s_ui_text_utf8_prev(buffer, ctx->text_cursor);

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
        u32 end = s_ui_text_utf8_next(buffer, ctx->text_cursor);

        if (s_ui_text_delete_range(buffer, buffer_len, ctx->text_cursor, end))
        {
          ctx->text_select_start = ctx->text_cursor;
          ctx->text_select_end = ctx->text_cursor;
          result |= LDK_UI_TEXT_BOX_CHANGED;
        }
      }

      buffer_len = s_ui_text_cstr_len_u32(buffer);
    }

    if (ctx->input_text != NULL && ctx->input_text->codepoint_count > 0)
    {
      for (u32 i = 0; i < ctx->input_text->codepoint_count; ++i)
      {
        char encoded[4] = {0};
        u32 encoded_len = 0;

        if (!s_ui_text_codepoint_to_utf8(ctx->input_text->codepoints[i], encoded, &encoded_len))
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

  item->changed = (result & LDK_UI_TEXT_BOX_CHANGED) != 0;
  item->data.text_box.cursor = ctx->text_cursor;
  item->data.text_box.select_start = ctx->text_select_start;
  item->data.text_box.select_end = ctx->text_select_end;
  item->data.text_box.focused = focused;

  s_ui_submit_item(ctx, item);

  return result;
}

void ldk_ui_horizontal_line(LDKUIContext* ctx)
{
  LDKUIItem* item = s_ui_submit_item_create(ctx, LDK_UI_ITEM_HORIZONTAL_LINE);

  if (item == NULL)
  {
    return;
  }

  item->preferred_width = 0.0f;
  item->preferred_height = LDK_UI_DEFAULT_CONTROL_HEIGHT;
  item->min_width = 0.0f;
  item->min_height = 1.0f;
  item->expand_width = true;
  item->expand_height = false;

  s_ui_submit_item(ctx, item);
}
