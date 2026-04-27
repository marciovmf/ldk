#include "ldk_ttf.h"
#include "stdx/stdx_array.h"
#include <ldk_common.h>
#include <ldk_ui.h>
#include <stdx/stdx_io.h>
#include <string.h>
#include <math.h>

#define LDK_RGBA_TO_ABGR(c) \
  ((((c) & 0x000000FFu) << 24) | \
   (((c) & 0x0000FF00u) << 8)  | \
   (((c) & 0x00FF0000u) >> 8)  | \
   (((c) & 0xFF000000u) >> 24))

typedef enum LDKUIInternalIdPart
{
  LDK_UI_INTERNAL_ID_SCROLLBAR_THUMB_Y = 1,
  LDK_UI_INTERNAL_ID_SCROLLBAR_TRACK_Y = 2,
  LDK_UI_INTERNAL_ID_SCROLLBAR_THUMB_X = 3,
  LDK_UI_INTERNAL_ID_SCROLLBAR_TRACK_X = 4,
} LDKUIInternalIdPart;

static float s_ui_maxf(float a, float b) { return (a > b) ? a : b; }

static float s_ui_minf(float a, float b) { return (a < b) ? a : b; }

static float s_ui_clampf(float x, float a, float b)
{
  float value = x;
  if (value < a) { value = a; }
  if (value > b) { value = b; }
  return value;
}

static bool s_ui_rect_contains(LDKUIRect rect, float x, float y)
{
  if (x < rect.x) { return false; }
  if (y < rect.y) { return false; }
  if (x >= rect.x + rect.w) { return false; }
  if (y >= rect.y + rect.h) { return false; }
  return true;
}

static LDKUIRect s_ui_rect_intersect(LDKUIRect a, LDKUIRect b)
{
  LDKUIRect rect;
  float x0 = s_ui_maxf(a.x, b.x);
  float y0 = s_ui_maxf(a.y, b.y);
  float x1 = s_ui_minf(a.x + a.w, b.x + b.w);
  float y1 = s_ui_minf(a.y + a.h, b.y + b.h);

  rect.x = x0;
  rect.y = y0;
  rect.w = s_ui_maxf(0.0f, x1 - x0);
  rect.h = s_ui_maxf(0.0f, y1 - y0);

  return rect;
}

static bool s_ui_widget_state_contains(LDKUIWidgetState const* state, float x, float y)
{
  if (state == NULL)
  {
    return false;
  }

  if (!s_ui_rect_contains(state->rect, x, y))
  {
    return false;
  }

  if (!s_ui_rect_contains(state->clip_rect, x, y))
  {
    return false;
  }

  return true;
}

static LDKUIId s_ui_hash_u32(LDKUIId hash, u32 value)
{
  hash ^= value + 0x9e3779b9u + (hash << 6) + (hash >> 2);
  return hash;
}

static LDKUIId s_ui_hash_cstr(LDKUIId hash, char const* text)
{
  char const* cursor = text;

  if (cursor == NULL)
  {
    return hash;
  }

  while (*cursor != 0)
  {
    hash = s_ui_hash_u32(hash, (u32)(uint8_t)*cursor);
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
static LDKUIId s_ui_make_id(LDKUIContext* ctx, u32 item_type)
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
      hash = s_ui_hash_u32(hash, *value);
    }
  }

  hash = s_ui_hash_u32(hash, parent_id);
  hash = s_ui_hash_u32(hash, item_type);
  hash = s_ui_hash_u32(hash, item_count);
  ctx->last_id = hash;

  return hash;
}

/**
 * Requests a renderable texture handle for a font atlas page.
 *
 * The UI does not own font page textures. The engine, renderer, or font cache
 * provides this callback and handles GPU resource creation/update.
 */
static LDKUITextureHandle s_ui_get_font_page_texture(LDKUIContext* ctx, u32 page_index)
{
  if (ctx == NULL)
  {
    return 0;
  }

  if (ctx->get_font_page_texture == NULL)
  {
    return 0;
  }

  return ctx->get_font_page_texture(ctx->font_texture_user, ctx->font, page_index);
}

static LDKUISize s_ui_measure_text(LDKFontInstance* font, char const* text)
{
  LDKUISize size = {0};

  if (font == NULL || text == NULL)
  {
    return size;
  }

  size.w = ldk_font_measure_text_cstr(font, text);
  size.h = ldk_font_get_line_height(font);

  return size;
}

static LDKUISize s_ui_measure_label_impl(LDKFontInstance* font, char const* text)
{
  LDKUISize size = s_ui_measure_text(font, text);
  return size;
}

static LDKUISize s_ui_measure_button_impl(LDKFontInstance* font, char const* text)
{
  LDKUISize size = s_ui_measure_text(font, text);
  size.w += 16.0f;
  size.h += 10.0f;
  return size;
}

static LDKUISize s_ui_measure_slider_impl(LDKUIContext* ctx, char const* text, LDKUIItemType type)
{
  LDKUISize label_size = s_ui_measure_text(ctx->font, text);
  LDKUISize size = { 0.0f, 0.0f };
  float track_height = s_ui_maxf(ctx->theme.slider_track_height, 1.0f);
  float thumb_width = s_ui_maxf(ctx->theme.slider_thumb_width, 1.0f);

  if (type == LDK_UI_ITEM_SLIDER_BAR)
  {
    track_height = s_ui_maxf(ctx->theme.slider_bar_track_height, 1.0f);
    thumb_width = 0.0f;
  }

  size.w = s_ui_maxf(140.0f, label_size.w + 80.0f);
  size.h = s_ui_maxf(22.0f, s_ui_maxf(track_height, thumb_width));

  return size;
}

/**
 * Finds widget state by ID using a linear search.
 *
 * Acceptable for small UIs (hundreds of widgets).
 * Consider a hash table if scaling to thousands.
 */
static LDKUIWidgetState* s_ui_widget_state_find(LDKUIContext* ctx, LDKUIId id)
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

static LDKUIWidgetState* s_ui_widget_state_get_or_create(LDKUIContext* ctx, LDKUIId id)
{
  LDKUIWidgetState* state = s_ui_widget_state_find(ctx, id);

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
static void s_ui_widget_states_gc(LDKUIContext* ctx)
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

static void s_ui_layout_update_state_rect(LDKUIContext* ctx, LDKUIItem* item, LDKUIRect clip_rect)
{
  LDKUIWidgetState* state;

  if (ctx == NULL || item == NULL)
  {
    return;
  }

  state = s_ui_widget_state_get_or_create(ctx, item->id);

  if (state == NULL)
  {
    return;
  }

  state->rect = item->rect;
  state->clip_rect = clip_rect;
}

static void s_ui_layout_update_state_content_size(LDKUIContext* ctx, LDKUIId id, LDKUISize content_size)
{
  LDKUIWidgetState* state;

  if (ctx == NULL)
  {
    return;
  }

  state = s_ui_widget_state_get_or_create(ctx, id);

  if (state == NULL)
  {
    return;
  }

  state->content_size.w = content_size.w;
  state->content_size.h = content_size.h;
}

static bool s_ui_window_can_interact(LDKUIContext* ctx, LDKUIWindow* window)
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

static bool s_ui_keyboard_accept_pressed(LDKUIContext* ctx)
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

static bool s_ui_keyboard_left_pressed(LDKUIContext* ctx)
{
  if (ctx->keyboard == NULL)
  {
    return false;
  }

  return ldk_os_keyboard_key_down((LDKKeyboardState*)ctx->keyboard, LDK_KEYCODE_LEFT);
}

static bool s_ui_keyboard_right_pressed(LDKUIContext* ctx)
{
  if (ctx->keyboard == NULL)
  {
    return false;
  }

  return ldk_os_keyboard_key_down((LDKKeyboardState*)ctx->keyboard, LDK_KEYCODE_RIGHT);
}

/**
 * Returns true if the widget was clicked this frame.
 *
 * Interaction is evaluated using the widget's rect from the previous frame,
 * since the current frame layout has not yet been resolved at call time.
 */
static bool s_ui_widget_submit_pressed(LDKUIContext* ctx, LDKUIItem* item)
{
  if (item == NULL)
  {
    return false;
  }

  LDKUIId id = item->id;
  LDKUIWidgetState* state = s_ui_widget_state_find(ctx, id);
  LDKPoint cursor;
  bool inside;

  if (state == NULL)
  {
    return false;
  }

  if (!s_ui_window_can_interact(ctx, ctx->current_window) && ctx->active_id != id)
  {
    return false;
  }

  cursor = ldk_os_mouse_cursor((LDKMouseState*)ctx->mouse);
  inside = s_ui_widget_state_contains(state, (float)cursor.x, (float)cursor.y);

  if (inside)
  {
    ctx->hot_id = id;
  }

  if (inside && ldk_os_mouse_button_down((LDKMouseState*)ctx->mouse, LDK_MOUSE_BUTTON_LEFT))
  {
    ctx->active_id = id;
    ctx->focused_id = id;
    ctx->focused_window = ctx->current_window;
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
  ctx->focused_id = id;
  ctx->focused_window = ctx->current_window;
  return true;
}

/**
 * Returns true if the slider value changed this frame.
 *
 * Interaction is evaluated using the widget's rect from the previous frame,
 * since the current frame layout has not yet been resolved at call time.
 */
static bool s_ui_widget_submit_slider(LDKUIContext* ctx, LDKUIItem* item, float* value, float min_value, float max_value)
{
  if (item == NULL || value == NULL)
  {
    return false;
  }

  LDKUIWidgetState* state = s_ui_widget_state_find(ctx, item->id);

  if (state == NULL)
  {
    return false;
  }

  if (!s_ui_window_can_interact(ctx, ctx->current_window) && ctx->active_id != item->id)
  {
    return false;
  }

  LDKPoint cursor = ldk_os_mouse_cursor((LDKMouseState*)ctx->mouse);
  bool inside = s_ui_widget_state_contains(state, (float)cursor.x, (float)cursor.y);

  if (inside)
  {
    ctx->hot_id = item->id;
  }

  if (inside && ldk_os_mouse_button_down((LDKMouseState*)ctx->mouse, LDK_MOUSE_BUTTON_LEFT))
  {
    ctx->active_id = item->id;
    ctx->focused_id = item->id;
    ctx->focused_window = ctx->current_window;
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
    thumb_width = s_ui_minf(ctx->theme.slider_thumb_width, state->rect.w);
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

static LDKUIItem* s_ui_item_create(LDKUIContext* ctx)
{
  LDKUIItem* item = x_arena_alloc_zero(ctx->frame_arena, sizeof(LDKUIItem));

  if (item == NULL)
  {
    return NULL;
  }

  return item;
}

static void s_ui_layout_append_item(LDKUILayoutNode* layout, LDKUIItem* item)
{
  if (layout == NULL)
  {
    return;
  }

  if (item == NULL)
  {
    return;
  }

  item->next_sibling = NULL;

  if (layout->last_item != NULL)
  {
    layout->last_item->next_sibling = item;
  }
  else
  {
    layout->first_item = item;
  }

  layout->last_item = item;
  layout->child_count += 1;
}

static void s_ui_reset_next_layout(LDKUIContext* ctx)
{
  memset(&ctx->next_layout, 0, sizeof(ctx->next_layout));
}

static void s_ui_apply_next_layout(LDKUIContext* ctx, LDKUIItem* item)
{
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

  s_ui_reset_next_layout(ctx);
}

static LDKUIWindow* s_ui_window_find(LDKUIContext* ctx, LDKUIId id)
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

static LDKUIWindow* s_ui_window_get_or_create(LDKUIContext* ctx, LDKUIId id, char const* title)
{
  LDKUIWindow* window = s_ui_window_find(ctx, id);

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

static void s_ui_window_bring_to_front(LDKUIContext* ctx, LDKUIWindow* window)
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

static void s_ui_add_draw_cmd(LDKUIContext* ctx, LDKUITextureHandle texture, LDKUIRect clip_rect, u32 index_offset, u32 index_count)
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

static void s_ui_emit_quad(LDKUIContext* ctx, LDKUIRect rect, u32 color, LDKUIRect clip_rect, LDKUITextureHandle texture)
{
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

  s_ui_add_draw_cmd(ctx, texture, clip_rect, index_offset, 6);
}

#ifdef LDK_UI_DEBUG_DRAW
static void s_ui_emit_debug_rect(LDKUIContext* ctx, LDKUIRect rect, u32 color)
{
  float t = 1.0f;

  LDKUIRect top = { rect.x, rect.y, rect.w, t };
  LDKUIRect bottom = { rect.x, rect.y + rect.h - t, rect.w, t };
  LDKUIRect left = { rect.x, rect.y, t, rect.h };
  LDKUIRect right = { rect.x + rect.w - t, rect.y, t, rect.h };

  LDKUIRect no_clip = { -100000.0f, -100000.0f, 200000.0f, 200000.0f };

  s_ui_emit_quad(ctx, top, color, no_clip, 0);
  s_ui_emit_quad(ctx, bottom, color, no_clip, 0);
  s_ui_emit_quad(ctx, left, color, no_clip, 0);
  s_ui_emit_quad(ctx, right, color, no_clip, 0);
}
#endif

static bool s_ui_scrollbar_rects(LDKUIRect rect, float visible_size, float content_size, float scroll, bool horizontal, LDKUIRect* track_rect, LDKUIRect* thumb_rect)
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

static void s_ui_resolve_interaction(LDKUIContext* ctx, LDKUIItem* item, LDKUIRect clip_rect)
{
  LDKPoint cursor = ldk_os_mouse_cursor((LDKMouseState*)ctx->mouse);

  bool inside;

  if (!s_ui_window_can_interact(ctx, ctx->current_window) && ctx->active_id != item->id)
  {
    return;
  }

  inside = s_ui_rect_contains(item->rect, (float)cursor.x, (float)cursor.y) &&
    s_ui_rect_contains(clip_rect, (float)cursor.x, (float)cursor.y);

  if (inside)
  {
    ctx->hot_id = item->id;
  }

  if (inside && ctx->active_id == 0 && ldk_os_mouse_button_down((LDKMouseState*)ctx->mouse, LDK_MOUSE_BUTTON_LEFT))
  {
    ctx->active_id = item->id;
  }
}

static void ldk_ui_draw_text(LDKUIContext* ctx, char const* text, float x, float y, u32 color, LDKUIRect clip_rect)
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

    LDKUITextureHandle texture = s_ui_get_font_page_texture(ctx, glyph->page_index);
    s_ui_add_draw_cmd(ctx, texture, clip_rect, index_offset, 6);

    pen_x += (float)glyph->advance_x;
    prev_codepoint = codepoint;
  }
}

//
// Widget renderting 
//

static void s_ui_emit_label(LDKUIContext* ctx, LDKUIItem* item, LDKUIRect clip_rect)
{
  ldk_ui_draw_text(
      ctx,
      item->text,
      item->rect.x,
      item->rect.y,
      ctx->theme.colors[LDK_UI_COLOR_TEXT],
      clip_rect);
}

static void s_ui_emit_button(LDKUIContext* ctx, LDKUIItem* item, LDKUIRect clip_rect)
{
  u32 color = ctx->theme.colors[LDK_UI_COLOR_CONTROL_BG];
  LDKUISize text_size;

  if (ctx->active_id == item->id)
  {
    color = ctx->theme.colors[LDK_UI_COLOR_CONTROL_BG_ACTIVE];
  }
  else if (ctx->hot_id == item->id)
  {
    color = ctx->theme.colors[LDK_UI_COLOR_CONTROL_BG_HOVERED];
  }

  s_ui_emit_quad(ctx, item->rect, color, clip_rect, 0);

  text_size = s_ui_measure_text(ctx->font, item->text);

  ldk_ui_draw_text(
      ctx,
      item->text,
      item->rect.x + (item->rect.w - text_size.w) * 0.5f,
      item->rect.y + (item->rect.h - text_size.h) * 0.5f,
      ctx->theme.colors[LDK_UI_COLOR_TEXT],
      clip_rect);
}

static void s_ui_emit_toggle_button(LDKUIContext* ctx, LDKUIItem* item, LDKUIRect clip_rect)
{
  bool toggled = item->data.toggle_button.value;
  u32 color = ctx->theme.colors[LDK_UI_COLOR_CONTROL_BG];
  LDKUISize text_size;

  if (toggled)
  {
    color = ctx->theme.colors[LDK_UI_COLOR_FOCUS];

    if (ctx->hot_id == item->id)
    {
      color = ctx->theme.colors[LDK_UI_COLOR_CONTROL_BG_ACTIVE_HOVERED];
    }
  }
  else
  {
    if (ctx->active_id == item->id)
    {
      color = ctx->theme.colors[LDK_UI_COLOR_CONTROL_BG_ACTIVE];
    }
    else if (ctx->hot_id == item->id)
    {
      color = ctx->theme.colors[LDK_UI_COLOR_CONTROL_BG_HOVERED];
    }
  }

  s_ui_emit_quad(ctx, item->rect, color, clip_rect, 0);

  text_size = s_ui_measure_text(ctx->font, item->text);

  ldk_ui_draw_text(
      ctx,
      item->text,
      item->rect.x + (item->rect.w - text_size.w) * 0.5f,
      item->rect.y + (item->rect.h - text_size.h) * 0.5f,
      ctx->theme.colors[LDK_UI_COLOR_TEXT],
      clip_rect);
}

static float s_ui_slider_normalized_value(LDKUIItem* item)
{
  float range = item->data.slider.max_value - item->data.slider.min_value;

  if (range <= 0.0f)
  {
    return 0.0f;
  }

  return s_ui_clampf((item->data.slider.value - item->data.slider.min_value) / range, 0.0f, 1.0f);
}

static void s_ui_emit_slider_bar(LDKUIContext* ctx, LDKUIItem* item, LDKUIRect clip_rect)
{
  float t = s_ui_slider_normalized_value(item);
  LDKUIRect track_rect = item->rect;
  LDKUIRect fill_rect = item->rect;
  u32 track_color = ctx->theme.colors[LDK_UI_COLOR_SLIDER_BAR_TRACK];
  u32 fill_color = ctx->theme.colors[LDK_UI_COLOR_SLIDER_BAR_FILL];
  char value_text[32];
  LDKUISize text_size;

  if (ctx->active_id == item->id)
  {
    track_color = ctx->theme.colors[LDK_UI_COLOR_SLIDER_BAR_TRACK_ACTIVE];
  }
  else if (ctx->hot_id == item->id)
  {
    track_color = ctx->theme.colors[LDK_UI_COLOR_SLIDER_BAR_TRACK_HOVERED];
  }

  track_rect = item->rect;
  fill_rect = track_rect;
  fill_rect.w = track_rect.w * t;

  s_ui_emit_quad(ctx, track_rect, track_color, clip_rect, 0);
  s_ui_emit_quad(ctx, fill_rect, fill_color, clip_rect, 0);

  snprintf(value_text, sizeof(value_text), "%.2f", item->data.slider.value);

  text_size = s_ui_measure_text(ctx->font, value_text);

  ldk_ui_draw_text(
      ctx,
      value_text,
      item->rect.x + (item->rect.w - text_size.w) * 0.5f,
      item->rect.y + (item->rect.h - text_size.h) * 0.5f,
      ctx->theme.colors[LDK_UI_COLOR_TEXT],
      clip_rect);
}

static void s_ui_emit_slider(LDKUIContext* ctx, LDKUIItem* item, LDKUIRect clip_rect)
{
  float t = s_ui_slider_normalized_value(item);
  float track_height = s_ui_minf(ctx->theme.slider_track_height, item->rect.h);
  float thumb_width = s_ui_minf(ctx->theme.slider_thumb_width, item->rect.w);
  LDKUIRect track_rect = item->rect;
  LDKUIRect fill_rect;
  LDKUIRect thumb_rect;
  u32 track_color = ctx->theme.colors[LDK_UI_COLOR_SLIDER_TRACK];
  u32 fill_color = ctx->theme.colors[LDK_UI_COLOR_SLIDER_FILL];
  u32 thumb_color = ctx->theme.colors[LDK_UI_COLOR_SLIDER_THUMB];

  if (ctx->active_id == item->id)
  {
    track_color = ctx->theme.colors[LDK_UI_COLOR_SLIDER_TRACK_ACTIVE];
    thumb_color = ctx->theme.colors[LDK_UI_COLOR_SLIDER_THUMB_ACTIVE];
  }
  else if (ctx->hot_id == item->id)
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

  s_ui_emit_quad(ctx, track_rect, track_color, clip_rect, 0);
  s_ui_emit_quad(ctx, fill_rect, fill_color, clip_rect, 0);
  s_ui_emit_quad(ctx, thumb_rect, thumb_color, clip_rect, 0);
}

static void s_ui_emit_scroll_area(LDKUIContext* ctx, LDKUIItem* item, LDKUIRect clip_rect)
{
  if (item->data.scroll_area.has_vertical_scrollbar)
  {
    LDKUIId thumb_id = s_ui_hash_u32(item->id, (u32)LDK_UI_INTERNAL_ID_SCROLLBAR_THUMB_Y);
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

    s_ui_emit_quad(ctx, item->data.scroll_area.vertical_track_rect, track_color, clip_rect, 0);
    s_ui_emit_quad(ctx, item->data.scroll_area.vertical_thumb_rect, thumb_color, clip_rect, 0);
  }

  if (item->data.scroll_area.has_horizontal_scrollbar)
  {
    LDKUIId thumb_id = s_ui_hash_u32(item->id, (u32)LDK_UI_INTERNAL_ID_SCROLLBAR_THUMB_X);
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

    s_ui_emit_quad(ctx, item->data.scroll_area.horizontal_track_rect, track_color, clip_rect, 0);
    s_ui_emit_quad(ctx, item->data.scroll_area.horizontal_thumb_rect, thumb_color, clip_rect, 0);
  }
}

static void s_ui_emit_color_view(LDKUIContext* ctx, LDKUIItem* item, LDKUIRect clip_rect)
{
  char const* text = item->data.color_view.label;
  LDKUISize text_size;

  s_ui_emit_quad(ctx, item->rect, item->data.color_view.color, clip_rect, 0);

  text_size = s_ui_measure_text(ctx->font, text);

  ldk_ui_draw_text(
      ctx,
      text,
      item->rect.x + (item->rect.w - text_size.w) * 0.5f,
      item->rect.y + (item->rect.h - text_size.h) * 0.5f,
      ctx->theme.colors[LDK_UI_COLOR_TEXT],
      clip_rect);
}

static void s_ui_emit_item(LDKUIContext* ctx, LDKUIItem* item, LDKUIRect clip_rect)
{
  if (item->type == LDK_UI_ITEM_LABEL)
    s_ui_emit_label(ctx, item, clip_rect);
  else if (item->type == LDK_UI_ITEM_BUTTON)
    s_ui_emit_button(ctx, item, clip_rect);
  else if (item->type == LDK_UI_ITEM_TOGGLE_BUTTON)
    s_ui_emit_toggle_button(ctx, item, clip_rect);
  else if (item->type == LDK_UI_ITEM_SLIDER_BAR)
    s_ui_emit_slider_bar(ctx, item, clip_rect);
  else if (item->type == LDK_UI_ITEM_SLIDER)
    s_ui_emit_slider(ctx, item, clip_rect);
  else if (item->type == LDK_UI_ITEM_SCROLL_AREA)
    s_ui_emit_scroll_area(ctx, item, clip_rect);
  else if (item->type == LDK_UI_ITEM_COLOR_VIEW)
    s_ui_emit_color_view(ctx, item, clip_rect);

#ifdef LDK_UI_DEBUG_DRAW
  if (ctx->hot_id == item->id)
  {
    LDKPoint cursor = ldk_os_mouse_cursor((LDKMouseState*)ctx->mouse);
    LDKUIRect debug_clip = s_ui_rect_intersect(clip_rect, item->rect);

    if (s_ui_rect_contains(debug_clip, (float)cursor.x, (float)cursor.y))
    {
      s_ui_emit_debug_rect(ctx, item->rect, 0x00ff00ffu);
      s_ui_emit_debug_rect(ctx, clip_rect, 0xff00ffffu);
      s_ui_emit_debug_rect(ctx, debug_clip, 0xffff00ffu);
    }
  }
#endif
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
  LDKUIRect scroll_clip = s_ui_rect_intersect(node_clip, item->rect);
  LDKUIRect content_rect = item->rect;
  bool allow_vertical = (item->data.scroll_area.flags & LDK_UI_SCROLL_VERTICAL) != 0;
  bool allow_horizontal = (item->data.scroll_area.flags & LDK_UI_SCROLL_HORIZONTAL) != 0;
  bool has_vertical_scrollbar = false;
  bool has_horizontal_scrollbar = false;

  if (allow_vertical)
  {
    has_vertical_scrollbar = s_ui_scrollbar_rects(
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
    has_horizontal_scrollbar = s_ui_scrollbar_rects(
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
    LDKUIId thumb_id = s_ui_hash_u32(item->id, (u32)LDK_UI_INTERNAL_ID_SCROLLBAR_THUMB_Y);

    if (s_ui_rect_contains(item->data.scroll_area.vertical_thumb_rect, (float)cursor.x, (float)cursor.y))
    {
      ctx->hot_id = thumb_id;
    }

    content_rect.w = s_ui_maxf(0.0f, content_rect.w - item->data.scroll_area.vertical_track_rect.w);
  }

  if (has_horizontal_scrollbar)
  {
    LDKPoint cursor = ldk_os_mouse_cursor((LDKMouseState*)ctx->mouse);
    LDKUIId thumb_id = s_ui_hash_u32(item->id, (u32)LDK_UI_INTERNAL_ID_SCROLLBAR_THUMB_X);

    if (s_ui_rect_contains(item->data.scroll_area.horizontal_thumb_rect, (float)cursor.x, (float)cursor.y))
    {
      ctx->hot_id = thumb_id;
    }

    content_rect.h = s_ui_maxf(0.0f, content_rect.h - item->data.scroll_area.horizontal_track_rect.h);
  }

  scroll_clip = s_ui_rect_intersect(node_clip, content_rect);

  content_rect.x -= item->data.scroll_area.scroll.x;
  content_rect.y -= item->data.scroll_area.scroll.y;
  content_rect.w = s_ui_maxf(content_rect.w, content_size.w);
  content_rect.h = s_ui_maxf(content_rect.h, content_size.h);

  s_ui_layout_update_state_content_size(ctx, item->id, content_size);
  s_ui_layout_resolve_node(ctx, item->data.scroll_area.node, content_rect, scroll_clip);
  s_ui_emit_item(ctx, item, node_clip);
}

static void s_ui_layout_resolve_item(LDKUIContext* ctx, LDKUIItem* item, LDKUIRect node_clip)
{
  s_ui_layout_update_state_rect(ctx, item, node_clip);
  s_ui_resolve_interaction(ctx, item, node_clip);

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

  s_ui_emit_item(ctx, item, node_clip);
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
  LDKUIRect node_clip = s_ui_rect_intersect(inherited_clip, rect);

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

static LDKUIWindow* s_ui_window_at_cursor_topmost(LDKUIContext* ctx)
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

    if (s_ui_rect_contains(window_rect, (float)cursor.x, (float)cursor.y))
    {
      return window;
    }
  }

  return NULL;
}

void ldk_ui_set_theme(LDKUIContext* ctx, LDKUIThemeType type, LDKUITheme* custom)
{
  LDK_ASSERT(ctx != NULL);
  LDKUITheme* theme = &ctx->theme;

  if (type == LDK_UI_THEME_DEFAULT_DARK)
  {
    LDKUIColor text          = LDK_RGBA_TO_ABGR(0xd8d8d8ffu);
    LDKUIColor text_disabled = LDK_RGBA_TO_ABGR(0x707070ffu);
    LDKUIColor bg            = LDK_RGBA_TO_ABGR(0x333333ffu);
    LDKUIColor panel         = LDK_RGBA_TO_ABGR(0x252525ffu);
    LDKUIColor control       = LDK_RGBA_TO_ABGR(0x444444ffu);
    LDKUIColor hover         = LDK_RGBA_TO_ABGR(0x505050ffu);
    LDKUIColor active        = LDK_RGBA_TO_ABGR(0x38b8a4ffu);
    LDKUIColor active_hover  = LDK_RGBA_TO_ABGR(0x43c8b3ffu);
    LDKUIColor border        = LDK_RGBA_TO_ABGR(0x1c1c1cffu);
    LDKUIColor accent        = LDK_RGBA_TO_ABGR(0x38b8a4ffu);
    LDKUIColor dark_track    = LDK_RGBA_TO_ABGR(0x202020ffu);
    LDKUIColor title         = LDK_RGBA_TO_ABGR(0x3f3f3fffu);
    LDKUIColor title_focus   = LDK_RGBA_TO_ABGR(0x2a2a2affu);
    LDKUIColor scrollbar     = LDK_RGBA_TO_ABGR(0x202020ffu);
    LDKUIColor thumb         = LDK_RGBA_TO_ABGR(0x555555ffu);
    LDKUIColor thumb_hover   = LDK_RGBA_TO_ABGR(0x666666ffu);
    LDKUIColor thumb_active  = LDK_RGBA_TO_ABGR(0x38b8a4ffu);

    theme->colors[LDK_UI_COLOR_TEXT]                       = text;
    theme->colors[LDK_UI_COLOR_TEXT_DISABLED]              = text_disabled;
    theme->colors[LDK_UI_COLOR_WINDOW_BG]                  = bg;
    theme->colors[LDK_UI_COLOR_PANEL_BG]                   = panel;
    theme->colors[LDK_UI_COLOR_CONTROL_BG]                 = control;
    theme->colors[LDK_UI_COLOR_CONTROL_BG_HOVERED]         = hover;
    theme->colors[LDK_UI_COLOR_CONTROL_BG_ACTIVE]          = active;
    theme->colors[LDK_UI_COLOR_CONTROL_BG_ACTIVE_HOVERED]  = active_hover;
    theme->colors[LDK_UI_COLOR_BORDER]                     = border;
    theme->colors[LDK_UI_COLOR_FOCUS]                      = accent;
    theme->colors[LDK_UI_COLOR_SLIDER_BAR_TRACK]           = dark_track;
    theme->colors[LDK_UI_COLOR_SLIDER_BAR_TRACK_HOVERED]   = hover;
    theme->colors[LDK_UI_COLOR_SLIDER_BAR_TRACK_ACTIVE]    = control;
    theme->colors[LDK_UI_COLOR_SLIDER_BAR_FILL]            = accent;
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
    theme->slider_bar_track_height                         = 22.0f;
    theme->slider_track_height                             = 6.0f;
    theme->slider_thumb_width                              = 14.0f;
  }
  else if (type == LDK_UI_THEME_DEFAULT_LIGHT)
  {
    LDKUIColor text          = LDK_RGBA_TO_ABGR(0x202020ffu);
    LDKUIColor text_disabled = LDK_RGBA_TO_ABGR(0xa0a0a0ffu);
    LDKUIColor bg            = LDK_RGBA_TO_ABGR(0xf0f0f0ffu);
    LDKUIColor panel         = LDK_RGBA_TO_ABGR(0xe0e0e0ffu);
    LDKUIColor control       = LDK_RGBA_TO_ABGR(0xd0d0d0ffu);
    LDKUIColor hover         = LDK_RGBA_TO_ABGR(0xc0c0c0ffu);
    LDKUIColor active        = LDK_RGBA_TO_ABGR(0xb0b0b0ffu);
    LDKUIColor active_hover  = LDK_RGBA_TO_ABGR(0xb4b4b4ffu);
    LDKUIColor border        = LDK_RGBA_TO_ABGR(0xa0a0a0ffu);
    LDKUIColor accent        = LDK_RGBA_TO_ABGR(0x4f8cc9ffu);
    LDKUIColor track         = LDK_RGBA_TO_ABGR(0xc0c0c0ffu);
    LDKUIColor title         = LDK_RGBA_TO_ABGR(0xdcdcdcffu);
    LDKUIColor title_focus   = LDK_RGBA_TO_ABGR(0xbfcfffffu);
    LDKUIColor scrollbar     = LDK_RGBA_TO_ABGR(0xc8c8c8ffu);
    LDKUIColor thumb         = LDK_RGBA_TO_ABGR(0xa8a8a8ffu);
    LDKUIColor thumb_hover   = LDK_RGBA_TO_ABGR(0x909090ffu);
    LDKUIColor thumb_active  = accent;

    theme->colors[LDK_UI_COLOR_TEXT]                       = text;
    theme->colors[LDK_UI_COLOR_TEXT_DISABLED]              = text_disabled;
    theme->colors[LDK_UI_COLOR_WINDOW_BG]                  = bg;
    theme->colors[LDK_UI_COLOR_PANEL_BG]                   = panel;
    theme->colors[LDK_UI_COLOR_CONTROL_BG]                 = control;
    theme->colors[LDK_UI_COLOR_CONTROL_BG_HOVERED]         = hover;
    theme->colors[LDK_UI_COLOR_CONTROL_BG_ACTIVE]          = active;
    theme->colors[LDK_UI_COLOR_CONTROL_BG_ACTIVE_HOVERED]  = active_hover;
    theme->colors[LDK_UI_COLOR_BORDER]                     = border;
    theme->colors[LDK_UI_COLOR_FOCUS]                      = accent;
    theme->colors[LDK_UI_COLOR_SLIDER_BAR_TRACK]           = track;
    theme->colors[LDK_UI_COLOR_SLIDER_BAR_TRACK_HOVERED]   = hover;
    theme->colors[LDK_UI_COLOR_SLIDER_BAR_TRACK_ACTIVE]    = active;
    theme->colors[LDK_UI_COLOR_SLIDER_BAR_FILL]            = accent;
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
    theme->slider_bar_track_height                         = 22.0f;
    theme->slider_track_height                             = 6.0f;
    theme->slider_thumb_width                              = 14.0f;
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

//
// Life cycle
//
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

  ctx->font_texture_user = config->font_texture_user;
  ctx->get_font_page_texture = config->get_font_page_texture;

  ldk_ui_set_theme(ctx, config->theme, NULL);

  // Initialize font
  XFile* file = x_io_open(config->font, "rb");
  size_t font_file_size = 0;
  const char* font_file_data = x_io_read_all(file, &font_file_size);
  LDKFontFace* face = ldk_font_face_create(font_file_data, (u32) font_file_size);
  LDKFontAtlasDesc atlas_desc = {0};
  atlas_desc.page_width = 512;
  atlas_desc.page_height = 512;
  ctx->font = ldk_font_get_instance(face, (float) config->font_size, &atlas_desc);
  ldk_font_preload_basic_ascii(ctx->font);
  x_io_close(file);

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
  x_arena_destroy(ctx->frame_arena);

  memset(ctx, 0, sizeof(*ctx));
}

//
// Frame
//

void ldk_ui_begin_frame(LDKUIContext* ctx, LDKMouseState const* mouse, LDKKeyboardState const* keyboard, LDKUIRect viewport)
{
  u32 count = x_array_ldk_ui_window_count(ctx->windows);
  ctx->mouse = mouse;
  ctx->keyboard = keyboard;
  ctx->viewport = viewport;
  ctx->root_item_count = 0;
  ctx->hot_id = 0;
  ctx->hovered_window = NULL;
  ctx->current_window = NULL;
  ctx->current_layout = NULL;

  s_ui_reset_next_layout(ctx);

  x_arena_reset_keep_head(ctx->frame_arena);
  x_array_ldk_ui_id_clear(ctx->id_stack);
  x_array_ldk_ui_vertex_clear(ctx->vertices);
  x_array_ldk_ui_u32_clear(ctx->indices);
  x_array_ldk_ui_draw_cmd_clear(ctx->commands);
  ctx->hovered_window = s_ui_window_at_cursor_topmost(ctx);

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

      s_ui_emit_quad(ctx, window_rect, window_bg, window_rect, 0);

      if (window->title_bar_rect.h > 0.0f)
      {
        s_ui_emit_quad(ctx, window->title_bar_rect, title_bg, window_rect, 0);
        float text_x = window->title_bar_rect.x + 6.0f;
        float font_line_height = ldk_font_get_line_height(ctx->font);
        float text_y = window->title_bar_rect.y + (window->title_bar_rect.h - font_line_height) * 0.5f;
        ldk_ui_draw_text(
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

  s_ui_widget_states_gc(ctx);
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
  return &ctx->render_data;
}


//
// Explicit ID
//

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
  LDKUIId hashed = s_ui_hash_cstr(2166136261u, value);
  x_array_ldk_ui_id_push(ctx->id_stack, hashed);
}

void ldk_ui_pop_id(LDKUIContext* ctx)
{
  if (!x_array_ldk_ui_id_is_empty(ctx->id_stack))
  {
    x_array_ldk_ui_id_pop(ctx->id_stack);
  }
}

//
// Next-item layout hints
//

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

//
// Layout
//

void ldk_ui_begin_vertical(LDKUIContext* ctx)
{
  LDK_ASSERT(ctx->current_layout != NULL);

  LDKUILayoutNode* node = s_ui_layout_node_create(
      ctx,
      LDK_UI_LAYOUT_VERTICAL,
      ctx->current_layout,
      ctx->current_window
      );
  LDKUIItem* item = s_ui_item_create(ctx);

  if (node == NULL || item == NULL)
  {
    return;
  }

  item->type = LDK_UI_ITEM_LAYOUT;
  item->id = s_ui_make_id(ctx, (u32)LDK_UI_ITEM_LAYOUT);
  item->data.layout.node = node;
  node->id = item->id;
  item->preferred_width = 0.0f;
  item->preferred_height = 0.0f;

  item->expand_width = true;
  item->expand_height = true;

  s_ui_apply_next_layout(ctx, item);

  if (ctx->current_layout != NULL)
  {
    s_ui_layout_append_item(ctx->current_layout, item);
  }

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
  LDKUIItem* item = s_ui_item_create(ctx);

  if (node == NULL || item == NULL)
  {
    return;
  }

  item->type = LDK_UI_ITEM_LAYOUT;
  item->id = s_ui_make_id(ctx, (u32)LDK_UI_ITEM_LAYOUT);
  item->data.layout.node = node;
  node->id = item->id;
  item->preferred_width = 0.0f;
  item->preferred_height = 0.0f;

  item->expand_width = true;
  item->expand_height = true;

  s_ui_apply_next_layout(ctx, item);

  if (ctx->current_layout != NULL)
  {
    s_ui_layout_append_item(ctx->current_layout, item);
  }

  ctx->current_layout = node;
}

void ldk_ui_end_horizontal(LDKUIContext* ctx)
{
  if (ctx->current_layout != NULL)
  {
    ctx->current_layout = ctx->current_layout->parent;
  }
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
  LDKUIId resolved_id = s_ui_make_id(ctx, id);
  LDKUIWindow* window = s_ui_window_get_or_create(ctx, resolved_id, title);
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

  inside_window = s_ui_rect_contains(rect, (float)cursor.x, (float)cursor.y);
  inside_title_bar = header_height > 0.0f &&
    s_ui_rect_contains(title_bar_rect, (float)cursor.x, (float)cursor.y);

  if (ctx->hovered_window == window && inside_window)
  {
    if (ldk_os_mouse_button_down((LDKMouseState*)ctx->mouse, LDK_MOUSE_BUTTON_LEFT))
    {
      s_ui_window_bring_to_front(ctx, window);
      window = x_array_ldk_ui_window_back(ctx->windows);
      ctx->focused_window = window;
      ctx->focused_id = 0;
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
      ctx->focused_id = 0;
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

bool ldk_ui_begin_pane(LDKUIContext* ctx, LDKUIRect rect)
{
  LDKUIId id = s_ui_make_id(ctx, LDK_UI_ITEM_LAYOUT);
  return s_ui_begin_root_container(ctx, id, NULL, rect, false, false, NULL);
}

void ldk_ui_end_pane(LDKUIContext* ctx)
{
  ctx->current_layout = NULL;
  ctx->current_window = NULL;
}

LDKUIRect ldk_ui_begin_window(LDKUIContext* ctx, char const* title, LDKUIRect rect)
{
  LDKUIId id = s_ui_make_id(ctx, LDK_UI_ITEM_LAYOUT);
  LDKUIRect out_rect = rect;
  s_ui_begin_root_container(ctx, id, title, rect, true, true, &out_rect);
  return out_rect;
}

void ldk_ui_end_window(LDKUIContext* ctx)
{
  ldk_ui_end_pane(ctx);
}

void ldk_ui_label(LDKUIContext* ctx, char const* text)
{
  LDKUIItem* item = s_ui_item_create(ctx);
  LDKUISize size;

  if (item == NULL)
  {
    return;
  }

  size = s_ui_measure_label_impl(ctx->font, text);

  item->type = LDK_UI_ITEM_LABEL;
  item->id = s_ui_make_id(ctx, (u32)LDK_UI_ITEM_LABEL);
  item->text = text;
  item->preferred_width = size.w;
  item->preferred_height = size.h;
  item->min_width = size.w;
  item->min_height = size.h;

  s_ui_apply_next_layout(ctx, item);
  s_ui_layout_append_item(ctx->current_layout, item);
}

bool ldk_ui_button(LDKUIContext* ctx, char const* text)
{
  LDKUIItem* item = s_ui_item_create(ctx);
  LDKUISize size;

  if (item == NULL)
  {
    return false;
  }

  size = s_ui_measure_button_impl(ctx->font, text);

  item->type = LDK_UI_ITEM_BUTTON;
  item->id = s_ui_make_id(ctx, (u32)LDK_UI_ITEM_BUTTON);
  item->text = text;
  item->preferred_width = size.w;
  item->preferred_height = size.h;
  item->min_width = size.w;
  item->min_height = size.h;
  item->expand_width = true;

  item->clicked = s_ui_widget_submit_pressed(ctx, item);

  if (ctx->focused_id == item->id &&
      ctx->focused_window == ctx->current_window &&
      s_ui_keyboard_accept_pressed(ctx))
  {
    item->clicked = true;
  }

  s_ui_apply_next_layout(ctx, item);
  s_ui_layout_append_item(ctx->current_layout, item);

  return item->clicked;
}

bool ldk_ui_toggle_button(LDKUIContext* ctx, char const* text, bool value)
{
  LDKUIItem* item = s_ui_item_create(ctx);
  LDKUISize size;

  if (item == NULL)
  {
    return value;
  }

  size = s_ui_measure_button_impl(ctx->font, text);

  item->type = LDK_UI_ITEM_TOGGLE_BUTTON;
  item->id = s_ui_make_id(ctx, (u32)LDK_UI_ITEM_TOGGLE_BUTTON);
  item->text = text;
  item->preferred_width = size.w;
  item->preferred_height = size.h;
  item->min_width = size.w;
  item->min_height = size.h;
  item->expand_width = true;
  item->data.toggle_button.value = value;

  item->clicked = s_ui_widget_submit_pressed(ctx, item);

  if (ctx->focused_id == item->id &&
      ctx->focused_window == ctx->current_window &&
      s_ui_keyboard_accept_pressed(ctx))
  {
    item->clicked = true;
  }

  if (item->clicked)
  {
    value = !value;
    item->changed = true;
  }

  item->data.toggle_button.value = value;

  s_ui_apply_next_layout(ctx, item);
  s_ui_layout_append_item(ctx->current_layout, item);

  return value;
}

static float s_ui_slider_submit(LDKUIContext* ctx, char const* text, float value, float min_value, float max_value, LDKUIItemType type)
{
  LDKUIItem* item = s_ui_item_create(ctx);
  LDKUISize size;

  if (item == NULL)
  {
    return value;
  }

  size = s_ui_measure_slider_impl(ctx, text, type);

  item->type = type;
  item->id = s_ui_make_id(ctx, (u32)type);
  item->text = text;
  item->preferred_width = size.w;
  item->preferred_height = size.h;
  item->min_width = 80.0f;
  item->min_height = size.h;
  item->expand_width = true;
  item->data.slider.value = value;
  item->data.slider.min_value = min_value;
  item->data.slider.max_value = max_value;

  item->changed = s_ui_widget_submit_slider(ctx, item, &value, min_value, max_value);

  if (ctx->focused_id == item->id &&
      ctx->focused_window == ctx->current_window)
  {
    float step = (max_value - min_value) / 100.0f;

    if (step <= 0.0f)
    {
      step = 1.0f;
    }

    if (s_ui_keyboard_left_pressed(ctx))
    {
      float new_value = s_ui_clampf(value - step, min_value, max_value);

      if (new_value != value)
      {
        value = new_value;
        item->changed = true;
      }
    }

    if (s_ui_keyboard_right_pressed(ctx))
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

  s_ui_apply_next_layout(ctx, item);
  s_ui_layout_append_item(ctx->current_layout, item);

  return value;
}

float ldk_ui_slider_bar(LDKUIContext* ctx, char const* text, float value, float min_value, float max_value)
{
  return s_ui_slider_submit(ctx, text, value, min_value, max_value, LDK_UI_ITEM_SLIDER_BAR);
}

float ldk_ui_slider(LDKUIContext* ctx, char const* text, float value, float min_value, float max_value)
{
  return s_ui_slider_submit(ctx, text, value, min_value, max_value, LDK_UI_ITEM_SLIDER);
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

  item = s_ui_item_create(ctx);

  if (item == NULL)
  {
    return;
  }

  item->id = s_ui_make_id(ctx, LDK_UI_ITEM_SPACER);
  item->type = LDK_UI_ITEM_SPACER;
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

  s_ui_apply_next_layout(ctx, item);
  s_ui_layout_append_item(ctx->current_layout, item);
}

void ldk_ui_color_view(LDKUIContext* ctx, LDKUIColor color)
{
  LDKUIItem* item = s_ui_item_create(ctx);
  LDKUISize size;

  if (item == NULL)
  {
    return ;
  }

  snprintf((char*)item->data.color_view.label,
      sizeof(item->data.color_view.label),
      "#%x", color);

  size = s_ui_measure_button_impl(ctx->font, item->data.color_view.label);
  item->type = LDK_UI_ITEM_COLOR_VIEW;
  item->id = s_ui_make_id(ctx, (u32)item->type);
  item->text = NULL;
  item->preferred_width = size.w;
  item->preferred_height = size.h;
  item->min_width = size.w;
  item->min_height = size.h;
  item->expand_width = true;
  item->data.color_view.color = LDK_RGBA_TO_ABGR(color);
  item->clicked = false;

  if (ctx->focused_id == item->id &&
      ctx->focused_window == ctx->current_window &&
      s_ui_keyboard_accept_pressed(ctx))
  {
    item->clicked = true;
  }

  s_ui_apply_next_layout(ctx, item);
  s_ui_layout_append_item(ctx->current_layout, item);
}


void ldk_ui_input_text(LDKUIContext* ctx, u32 codepoint)
{
  (void)ctx;
  (void)codepoint;

  //TODO: Implement this later
}

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

  id = s_ui_make_id(ctx, (u32)LDK_UI_ITEM_SCROLL_AREA);
  state = s_ui_widget_state_find(ctx, id);

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
      has_vertical_scrollbar = s_ui_scrollbar_rects(
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
      has_horizontal_scrollbar = s_ui_scrollbar_rects(
          state->rect,
          state->rect.w,
          state->content_size.w,
          scroll.x,
          true,
          &horizontal_track_rect,
          &horizontal_thumb_rect);
    }

    if (s_ui_window_can_interact(ctx, ctx->current_window))
    {
      if (s_ui_rect_contains(state->clip_rect, (float)cursor.x, (float)cursor.y))
      {
        i32 wheel_delta = ldk_os_mouse_wheel_delta((LDKMouseState*)ctx->mouse);

        if (wheel_delta != 0 && (flags & LDK_UI_SCROLL_VERTICAL) != 0)
        {
          scroll.y -= ((float)wheel_delta / 120.0f) * 32.0f;
        }
      }
    }

    if (has_vertical_scrollbar)
    {
      LDKUIId thumb_id = s_ui_hash_u32(id, (u32)LDK_UI_INTERNAL_ID_SCROLLBAR_THUMB_Y);
      bool inside_thumb = s_ui_rect_contains(vertical_thumb_rect, (float)cursor.x, (float)cursor.y);

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

    if (has_horizontal_scrollbar)
    {
      LDKUIId thumb_id = s_ui_hash_u32(id, (u32)LDK_UI_INTERNAL_ID_SCROLLBAR_THUMB_X);
      bool inside_thumb = s_ui_rect_contains(horizontal_thumb_rect, (float)cursor.x, (float)cursor.y);

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

  item = s_ui_item_create(ctx);

  if (node == NULL || item == NULL)
  {
    return scroll;
  }

  item->type = LDK_UI_ITEM_SCROLL_AREA;
  item->id = id;
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

  s_ui_apply_next_layout(ctx, item);

  if (ctx->current_layout != NULL)
  {
    s_ui_layout_append_item(ctx->current_layout, item);
  }

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
