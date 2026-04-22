#include "ldk_font.h"
#include <ldk_common.h>
#include <ldk_ui.h>
#include <stdx/stdx_io.h>
#include <string.h>
#include <math.h>

static float s_ui_maxf(float a, float b)
{
  if (a > b)
  {
    return a;
  }

  return b;
}

static float s_ui_minf(float a, float b)
{
  if (a < b)
  {
    return a;
  }

  return b;
}

static float s_ui_clampf(float x, float a, float b)
{
  float value = x;

  if (value < a)
  {
    value = a;
  }

  if (value > b)
  {
    value = b;
  }

  return value;
}

static bool s_ui_rect_contains(LDKUIRect rect, float x, float y)
{
  if (x < rect.x)
  {
    return false;
  }

  if (y < rect.y)
  {
    return false;
  }

  if (x >= rect.x + rect.w)
  {
    return false;
  }

  if (y >= rect.y + rect.h)
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

static LDKUISize s_ui_measure_text(LDKFontInstance* font, char const* text)
{
  LDKUISize size = {0};
  LDKFontMetrics metrics;
  float width = 0.0f;

  if (font == NULL || text == NULL)
  {
    return size;
  }

  width = ldk_font_measure_text_cstr(font, text);
  metrics = ldk_font_get_metrics(font);

  size.w = width;
  size.h = metrics.line_height;
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

static LDKUISize s_ui_measure_slider_impl(LDKFontInstance* font, char const* text)
{
  LDKUISize label_size = s_ui_measure_text(font, text);
  LDKUISize size = { 0.0f, 0.0f };
  size.w = s_ui_maxf(140.0f, label_size.w + 80.0f);
  size.h = 22.0f;
  return size;
}

static LDKUIWidgetState* s_ui_widget_state_find(LDKUIContext* ctx, LDKUIId id)
{
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
    return state;
  }

  LDKUIWidgetState new_state;
  memset(&new_state, 0, sizeof(new_state));
  new_state.id = id;
  x_array_ldk_ui_widget_state_push(ctx->widget_states, new_state);

  return x_array_ldk_ui_widget_state_back(ctx->widget_states);
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

static bool s_ui_widget_submit_pressed(LDKUIContext* ctx, LDKUIId id)
{
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
  inside = s_ui_rect_contains(state->rect, (float)cursor.x, (float)cursor.y);

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

static bool s_ui_widget_submit_slider(LDKUIContext* ctx, LDKUIId id, float* value, float min_value, float max_value)
{
  LDKUIWidgetState* state = s_ui_widget_state_find(ctx, id);
  LDKPoint cursor;
  bool inside;
  bool down;

  if (state == NULL || value == NULL)
  {
    return false;
  }

  if (!s_ui_window_can_interact(ctx, ctx->current_window) && ctx->active_id != id)
  {
    return false;
  }

  cursor = ldk_os_mouse_cursor((LDKMouseState*)ctx->mouse);
  inside = s_ui_rect_contains(state->rect, (float)cursor.x, (float)cursor.y);

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

  down = ldk_os_mouse_button_is_pressed((LDKMouseState*)ctx->mouse, LDK_MOUSE_BUTTON_LEFT);

  if (ctx->active_id != id || !down)
  {
    if (ldk_os_mouse_button_up((LDKMouseState*)ctx->mouse, LDK_MOUSE_BUTTON_LEFT) && ctx->active_id == id)
    {
      ctx->active_id = 0;
    }

    return false;
  }

  float local = (float)cursor.x - state->rect.x;
  float denom = s_ui_maxf(state->rect.w, 1.0f);
  float t = s_ui_clampf(local / denom, 0.0f, 1.0f);
  float new_value = min_value + (max_value - min_value) * t;

  if (*value == new_value)
  {
    return false;
  }

  *value = new_value;
  return true;
}

static void s_ui_layout_update_state_rect(LDKUIContext* ctx, LDKUIItem* item)
{
  LDKUIWidgetState* state = s_ui_widget_state_get_or_create(ctx, item->id);

  if (state == NULL)
  {
    return;
  }

  state->rect = item->rect;
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

static LDKUIWindow* s_ui_window_get_or_create(LDKUIContext* ctx, LDKUIId id, char const* title, LDKUIRect rect)
{
  LDKUIWindow* window = s_ui_window_find(ctx, id);

  if (window != NULL)
  {
    return window;
  }

  LDKUIWindow new_window;
  memset(&new_window, 0, sizeof(new_window));

  new_window.id = id;
  new_window.rect = rect;
  new_window.is_open = true;
  new_window.z_order = x_array_ldk_ui_window_count(ctx->windows);

  if (title != NULL)
  {
    strncpy(new_window.title, title, sizeof(new_window.title) - 1);
    new_window.title[sizeof(new_window.title) - 1] = 0;
  }

  x_array_ldk_ui_window_push(ctx->windows, new_window);

  return x_array_ldk_ui_window_back(ctx->windows);
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

  count = x_array_ldk_ui_window_count(ctx->windows);

  for (u32 i = 0; i < count; ++i)
  {
    LDKUIWindow* current = x_array_ldk_ui_window_get(ctx->windows, i);

    if (current != NULL)
    {
      current->z_order = i;
    }
  }

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

static void s_ui_resolve_interaction(LDKUIContext* ctx, LDKUIItem* item)
{
  LDKPoint cursor = ldk_os_mouse_cursor((LDKMouseState*)ctx->mouse);
  bool inside;

  if (!s_ui_window_can_interact(ctx, ctx->current_window) && ctx->active_id != item->id)
  {
    return;
  }

  inside = s_ui_rect_contains(item->rect, (float)cursor.x, (float)cursor.y);

  if (inside)
  {
    ctx->hot_id = item->id;
  }

  if (inside && ldk_os_mouse_button_down((LDKMouseState*)ctx->mouse, LDK_MOUSE_BUTTON_LEFT))
  {
    ctx->active_id = item->id;
  }
}

static void s_ui_emit_item(LDKUIContext* ctx, LDKUIItem* item, LDKUIRect clip_rect)
{
  u32 color = ctx->theme.colors[LDK_UI_COLOR_CONTROL_BG];

  if (item->type == LDK_UI_ITEM_LABEL)
  {
    return;
  }

  if (item->type == LDK_UI_ITEM_BUTTON)
  {
    if (ctx->active_id == item->id)
    {
      color = ctx->theme.colors[LDK_UI_COLOR_CONTROL_BG_ACTIVE];
    }
    else if (ctx->hot_id == item->id)
    {
      color = ctx->theme.colors[LDK_UI_COLOR_CONTROL_BG_HOVERED];
    }

    s_ui_emit_quad(ctx, item->rect, color, clip_rect, 0);
    return;
  }

  if (item->type == LDK_UI_ITEM_TOGGLE_BUTTON)
  {
    bool toggled = false;

    if (item->data.toggle_button.value != NULL)
    {
      toggled = *item->data.toggle_button.value;
    }

    color = ctx->theme.colors[LDK_UI_COLOR_CONTROL_BG];

    if (toggled)
    {
      color = ctx->theme.colors[LDK_UI_COLOR_FOCUS];
    }

    if (ctx->active_id == item->id)
    {
      color = ctx->theme.colors[LDK_UI_COLOR_CONTROL_BG_ACTIVE];
    }
    else if (ctx->hot_id == item->id)
    {
      color = ctx->theme.colors[LDK_UI_COLOR_CONTROL_BG_HOVERED];
    }

    s_ui_emit_quad(ctx, item->rect, color, clip_rect, 0);
    return;
  }

  if (item->type == LDK_UI_ITEM_SLIDER_FLOAT)
  {
    float t = 0.0f;
    LDKUIRect fill_rect = item->rect;
    u32 track_color = ctx->theme.colors[LDK_UI_COLOR_SLIDER_TRACK];
    u32 fill_color = ctx->theme.colors[LDK_UI_COLOR_SLIDER_FILL];

    if (item->data.slider_float.value != NULL)
    {
      float range = item->data.slider_float.max_value - item->data.slider_float.min_value;

      if (range > 0.0f)
      {
        t = (*item->data.slider_float.value - item->data.slider_float.min_value) / range;
        t = s_ui_clampf(t, 0.0f, 1.0f);
      }
    }

    if (ctx->active_id == item->id)
    {
      track_color = ctx->theme.colors[LDK_UI_COLOR_CONTROL_BG_ACTIVE];
    }
    else if (ctx->hot_id == item->id)
    {
      track_color = ctx->theme.colors[LDK_UI_COLOR_CONTROL_BG_HOVERED];
    }

    s_ui_emit_quad(ctx, item->rect, track_color, clip_rect, 0);

    fill_rect.w = item->rect.w * t;
    s_ui_emit_quad(ctx, fill_rect, fill_color, clip_rect, 0);
    return;
  }
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

static void s_ui_layout_resolve_node(LDKUIContext* ctx, LDKUILayoutNode* node, LDKUIRect rect)
{
  u32 count = 0;
  float inner_x = rect.x + node->padding;
  float inner_y = rect.y + node->padding;
  float inner_w = rect.w - node->padding * 2.0f;
  float inner_h = rect.h - node->padding * 2.0f;
  LDKUIItem* item = NULL;

  node->rect = rect;
  count = node->child_count;

  if (node->direction == LDK_UI_LAYOUT_VERTICAL)
  {
    float fixed_height = 0.0f;
    u32 expand_count = 0;

    for (item = node->first_item; item != NULL; item = item->next_sibling)
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

    float remaining = inner_h - fixed_height;

    if (remaining < 0.0f)
    {
      remaining = 0.0f;
    }

    float expand_height = 0.0f;

    if (expand_count > 0)
    {
      expand_height = remaining / (float)expand_count;
    }

    float cursor_y = inner_y;

    for (item = node->first_item; item != NULL; item = item->next_sibling)
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

      s_ui_layout_update_state_rect(ctx, item);
      s_ui_resolve_interaction(ctx, item);

      if (item->type == LDK_UI_ITEM_LAYOUT && item->data.layout.node != NULL)
      {
        s_ui_layout_resolve_node(ctx, item->data.layout.node, item->rect);
      }
      else
      {
        s_ui_emit_item(ctx, item, rect);
      }

      cursor_y += item_h + node->spacing;
    }

    return;
  }

  float fixed_width = 0.0f;
  u32 expand_count = 0;

  for (item = node->first_item; item != NULL; item = item->next_sibling)
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

  float remaining = inner_w - fixed_width;

  if (remaining < 0.0f)
  {
    remaining = 0.0f;
  }

  float expand_width = 0.0f;

  if (expand_count > 0)
  {
    expand_width = remaining / (float)expand_count;
  }

  float cursor_x = inner_x;

  for (item = node->first_item; item != NULL; item = item->next_sibling)
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

    s_ui_layout_update_state_rect(ctx, item);
    s_ui_resolve_interaction(ctx, item);

    if (item->type == LDK_UI_ITEM_LAYOUT && item->data.layout.node != NULL)
    {
      s_ui_layout_resolve_node(ctx, item->data.layout.node, item->rect);
    }
    else
    {
      s_ui_emit_item(ctx, item, rect);
    }

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

    if (!window->is_open)
    {
      continue;
    }

    if (s_ui_rect_contains(window->rect, (float)cursor.x, (float)cursor.y))
    {
      return window;
    }
  }

  return NULL;
}

static void s_init_theme_dark(LDKUITheme* theme)
{
  LDK_ASSERT(theme != NULL);

  theme->colors[LDK_UI_COLOR_TEXT]               = 0xffe0e0e0u;
  theme->colors[LDK_UI_COLOR_TEXT_DISABLED]      = 0xff808080u;

  theme->colors[LDK_UI_COLOR_WINDOW_BG]          = 0xff202020u;
  theme->colors[LDK_UI_COLOR_PANEL_BG]           = 0xff2a2a2au;

  theme->colors[LDK_UI_COLOR_CONTROL_BG]         = 0xff404040u;
  theme->colors[LDK_UI_COLOR_CONTROL_BG_HOVERED] = 0xff505050u;
  theme->colors[LDK_UI_COLOR_CONTROL_BG_ACTIVE]  = 0xff606060u;

  theme->colors[LDK_UI_COLOR_BORDER]             = 0xff101010u;
  theme->colors[LDK_UI_COLOR_FOCUS]              = 0xff5f8f5fu;

  theme->colors[LDK_UI_COLOR_SLIDER_TRACK]       = 0xff303030u;
  theme->colors[LDK_UI_COLOR_SLIDER_FILL]        = 0xff8080c0u;

  theme->colors[LDK_UI_COLOR_TITLE_BAR]          = 0xff3a3a3au;
  theme->colors[LDK_UI_COLOR_TITLE_BAR_FOCUSED]  = 0xff4a4a6au;
}

static void s_init_theme_light(LDKUITheme* theme)
{
  LDK_ASSERT(theme != NULL);

  theme->colors[LDK_UI_COLOR_TEXT]               = 0xff202020u;
  theme->colors[LDK_UI_COLOR_TEXT_DISABLED]      = 0xffa0a0a0u;

  theme->colors[LDK_UI_COLOR_WINDOW_BG]          = 0xfff0f0f0u;
  theme->colors[LDK_UI_COLOR_PANEL_BG]           = 0xffe0e0e0u;

  theme->colors[LDK_UI_COLOR_CONTROL_BG]         = 0xffd0d0d0u;
  theme->colors[LDK_UI_COLOR_CONTROL_BG_HOVERED] = 0xffc0c0c0u;
  theme->colors[LDK_UI_COLOR_CONTROL_BG_ACTIVE]  = 0xffb0b0b0u;

  theme->colors[LDK_UI_COLOR_BORDER]             = 0xffa0a0a0u;
  theme->colors[LDK_UI_COLOR_FOCUS]              = 0xff4f7fd0u;

  theme->colors[LDK_UI_COLOR_SLIDER_TRACK]       = 0xffc0c0c0u;
  theme->colors[LDK_UI_COLOR_SLIDER_FILL]        = 0xff6f8fd0u;

  theme->colors[LDK_UI_COLOR_TITLE_BAR]          = 0xffdcdcdcu;
  theme->colors[LDK_UI_COLOR_TITLE_BAR_FOCUSED]  = 0xffbfcfffu;
}

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

  if (config->theme == LDK_UI_THEME_DARK)
    s_init_theme_dark(&ctx->theme);
  else if (config->theme == LDK_UI_THEME_LIGHT)
    s_init_theme_dark(&ctx->theme);
  // else assume the theme is initialized

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
  x_array_ldk_ui_window_destroy(ctx->windows);
  x_array_ldk_ui_id_destroy(ctx->id_stack);
  x_array_ldk_ui_vertex_destroy(ctx->vertices);
  x_array_ldk_ui_u32_destroy(ctx->indices);
  x_array_ldk_ui_draw_cmd_destroy(ctx->commands);
  x_array_ldk_ui_widget_state_destroy(ctx->widget_states);
  x_arena_destroy(ctx->frame_arena);

  memset(ctx, 0, sizeof(*ctx));
}

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

  for (u32 i = 0; i < count; ++i)
  {
    LDKUIWindow* window = x_array_ldk_ui_window_get(ctx->windows, i);

    if (window == NULL)
    {
      continue;
    }

    window->root_layout = NULL;
    window->is_hovered = false;
    window->is_active = false;
    window->is_focused = false;
  }

  ctx->hovered_window = s_ui_window_at_cursor_topmost(ctx);

  if (ctx->hovered_window != NULL)
  {
    ctx->hovered_window->is_hovered = true;
  }

  if (ctx->focused_window != NULL)
  {
    ctx->focused_window->is_focused = true;
    ctx->focused_window->is_active = true;
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

    if (!window->is_open)
    {
      continue;
    }

    if (window->root_layout == NULL)
    {
      continue;
    }

    if (window->is_dragging && ldk_os_mouse_button_is_pressed((LDKMouseState*)ctx->mouse, LDK_MOUSE_BUTTON_LEFT))
    {
      LDKPoint cursor = ldk_os_mouse_cursor((LDKMouseState*)ctx->mouse);
      window->rect.x = (float)cursor.x - window->drag_offset_x;
      window->rect.y = (float)cursor.y - window->drag_offset_y;
    }
    else
    {
      window->is_dragging = false;
    }

    {
      float header_height = window->title_bar_rect.h;
      u32 window_bg = ctx->theme.colors[LDK_UI_COLOR_WINDOW_BG];
      u32 title_bg = ctx->theme.colors[LDK_UI_COLOR_TITLE_BAR];

      if (window->is_focused)
      {
        title_bg = ctx->theme.colors[LDK_UI_COLOR_TITLE_BAR_FOCUSED];
      }

      window->title_bar_rect.x = window->rect.x;
      window->title_bar_rect.y = window->rect.y;
      window->title_bar_rect.w = window->rect.w;

      window->content_rect.x = window->rect.x;
      window->content_rect.y = window->rect.y + header_height;
      window->content_rect.w = window->rect.w;
      window->content_rect.h = window->rect.h - header_height;

      s_ui_emit_quad(ctx, window->rect, window_bg, window->rect, 0);

      if (header_height > 0.0f)
      {
        s_ui_emit_quad(ctx, window->title_bar_rect, title_bg, window->rect, 0);
      }
    }

    s_ui_layout_measure_node(window->root_layout);
    s_ui_layout_resolve_node(ctx, window->root_layout, window->content_rect);
  }

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

static bool ldk_ui_begin_pane_internal(LDKUIContext* ctx, LDKUIId id, char const* title, LDKUIRect rect, bool toolbar, bool draggable)
{
  LDKUIId resolved_id = s_ui_make_id(ctx, id);
  LDKUIWindow* window = s_ui_window_get_or_create(ctx, resolved_id, title, rect);
  LDKUILayoutNode* root = NULL;
  LDKPoint cursor;
  bool inside_window;
  bool inside_title_bar;
  float header_height = toolbar ? 24.0f : 0.0f;

  if (window == NULL)
  {
    return false;
  }

  cursor = ldk_os_mouse_cursor((LDKMouseState*)ctx->mouse);

  if (title != NULL)
  {
    strncpy(window->title, title, sizeof(window->title) - 1);
    window->title[sizeof(window->title) - 1] = 0;
  }

  window->title_bar_rect.x = window->rect.x;
  window->title_bar_rect.y = window->rect.y;
  window->title_bar_rect.w = window->rect.w;
  window->title_bar_rect.h = header_height;

  window->content_rect.x = window->rect.x;
  window->content_rect.y = window->rect.y + header_height;
  window->content_rect.w = window->rect.w;
  window->content_rect.h = window->rect.h - header_height;

  inside_window = s_ui_rect_contains(window->rect, (float)cursor.x, (float)cursor.y);
  inside_title_bar = header_height > 0.0f &&
    s_ui_rect_contains(window->title_bar_rect, (float)cursor.x, (float)cursor.y);

  if (ctx->hovered_window == window && inside_window)
  {
    window->is_hovered = true;
    ctx->hovered_window = window;

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
      window->is_dragging = true;
      window->drag_offset_x = (float)cursor.x - window->rect.x;
      window->drag_offset_y = (float)cursor.y - window->rect.y;
      ctx->focused_window = window;
      ctx->focused_id = 0;
    }
  }

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

  return true;
}

bool ldk_ui_begin_pane(LDKUIContext* ctx, LDKUIRect rect)
{
  LDKUIId id = s_ui_make_id(ctx, LDK_UI_ITEM_LAYOUT);
  return ldk_ui_begin_pane_internal(ctx, id, NULL, rect, false, false);
}

void ldk_ui_end_pane(LDKUIContext* ctx)
{
  ctx->current_layout = NULL;
  ctx->current_window = NULL;
}

bool ldk_ui_begin_window(LDKUIContext* ctx, char const* title, LDKUIRect rect)
{
  LDKUIId id = s_ui_make_id(ctx, LDK_UI_ITEM_LAYOUT);
  return ldk_ui_begin_pane_internal(ctx, id, title, rect, true, true);
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

  item->clicked = s_ui_widget_submit_pressed(ctx, item->id);

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

bool ldk_ui_toggle_button(LDKUIContext* ctx, char const* text, bool* value)
{
  LDKUIItem* item = s_ui_item_create(ctx);
  LDKUISize size;

  if (item == NULL)
  {
    return false;
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

  item->clicked = s_ui_widget_submit_pressed(ctx, item->id);

  if (ctx->focused_id == item->id &&
      ctx->focused_window == ctx->current_window &&
      s_ui_keyboard_accept_pressed(ctx))
  {
    item->clicked = true;
  }

  if (item->clicked && value != NULL)
  {
    *value = !*value;
    item->changed = true;
  }

  s_ui_apply_next_layout(ctx, item);
  s_ui_layout_append_item(ctx->current_layout, item);

  return item->changed;
}

bool ldk_ui_slider_float(LDKUIContext* ctx, char const* text, float* value, float min_value, float max_value)
{
  LDKUIItem* item = s_ui_item_create(ctx);
  LDKUISize size;

  if (item == NULL)
  {
    return false;
  }

  size = s_ui_measure_slider_impl(ctx->font, text);

  item->type = LDK_UI_ITEM_SLIDER_FLOAT;
  item->id = s_ui_make_id(ctx, (u32)LDK_UI_ITEM_SLIDER_FLOAT);
  item->text = text;
  item->preferred_width = size.w;
  item->preferred_height = size.h;
  item->min_width = 80.0f;
  item->min_height = size.h;
  item->expand_width = true;
  item->data.slider_float.value = value;
  item->data.slider_float.min_value = min_value;
  item->data.slider_float.max_value = max_value;

  item->changed = s_ui_widget_submit_slider(ctx, item->id, value, min_value, max_value);

  if (ctx->focused_id == item->id &&
      ctx->focused_window == ctx->current_window &&
      value != NULL)
  {
    float step = (max_value - min_value) / 100.0f;

    if (step <= 0.0f)
    {
      step = 1.0f;
    }

    if (s_ui_keyboard_left_pressed(ctx))
    {
      float new_value = s_ui_clampf(*value - step, min_value, max_value);

      if (new_value != *value)
      {
        *value = new_value;
        item->changed = true;
      }
    }

    if (s_ui_keyboard_right_pressed(ctx))
    {
      float new_value = s_ui_clampf(*value + step, min_value, max_value);

      if (new_value != *value)
      {
        *value = new_value;
        item->changed = true;
      }
    }
  }

  s_ui_apply_next_layout(ctx, item);
  s_ui_layout_append_item(ctx->current_layout, item);

  return item->changed;
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

void ldk_ui_input_text(LDKUIContext* ctx, u32 codepoint)
{
  (void)ctx;
  (void)codepoint;

  //TODO: Implement this later
}

