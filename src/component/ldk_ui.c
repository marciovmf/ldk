#include <ldk_common.h>
#include "ldk_ui.h"

#include <string.h>
#include <math.h>

static float ldk_ui_maxf(float a, float b)
{
  if (a > b)
  {
    return a;
  }

  return b;
}

static float ldk_ui_minf(float a, float b)
{
  if (a < b)
  {
    return a;
  }

  return b;
}

static float ldk_ui_clampf(float x, float a, float b)
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

static bool ldk_ui_rect_contains(LDKUIRect rect, float x, float y)
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

static LDKUIId ldk_ui_hash_u32(LDKUIId hash, u32 value)
{
  hash ^= value + 0x9e3779b9u + (hash << 6) + (hash >> 2);
  return hash;
}

static LDKUIId ldk_ui_hash_cstr(LDKUIId hash, char const* text)
{
  char const* cursor = text;

  if (cursor == NULL)
  {
    return hash;
  }

  while (*cursor != 0)
  {
    hash = ldk_ui_hash_u32(hash, (u32)(uint8_t)*cursor);
    cursor += 1;
  }

  return hash;
}

static LDKUIId ldk_ui_make_id(LDKUIContext* ctx, char const* id)
{
  LDKUIId hash = 2166136261u;
  u32 count = x_array_ldk_ui_id_count(ctx->id_stack);

  if (ctx->current_window != NULL)
  {
    hash = ldk_ui_hash_u32(hash, ctx->current_window->id);
  }

  for (u32 i = 0; i < count; ++i)
  {
    LDKUIId* value = x_array_ldk_ui_id_get(ctx->id_stack, i);

    if (value != NULL)
    {
      hash = ldk_ui_hash_u32(hash, *value);
    }
  }

  hash = ldk_ui_hash_cstr(hash, id);
  ctx->last_id = hash;

  return hash;
}

static LDKUISize ldk_ui_measure_text(char const* text)
{
  LDKUISize size = { 0.0f, 0.0f };
  size_t length = 0;

  if (text != NULL)
  {
    length = strlen(text);
  }

  size.w = (float)length * 8.0f;
  size.h = 16.0f;

  return size;
}

static LDKUISize ldk_ui_measure_label_impl(char const* text)
{
  LDKUISize size = ldk_ui_measure_text(text);
  return size;
}

static LDKUISize ldk_ui_measure_button_impl(char const* text)
{
  LDKUISize size = ldk_ui_measure_text(text);
  size.w += 16.0f;
  size.h += 10.0f;
  return size;
}

static LDKUISize ldk_ui_measure_slider_impl(char const* text)
{
  LDKUISize label_size = ldk_ui_measure_text(text);
  LDKUISize size = { 0.0f, 0.0f };
  size.w = ldk_ui_maxf(140.0f, label_size.w + 80.0f);
  size.h = 22.0f;
  return size;
}

static LDKUILayoutNode* ldk_ui_layout_node_create(
    LDKUIContext* ctx,
    LDKUILayoutDirection direction,
    LDKUILayoutNode* parent,
    LDKUIWindow* window
    )
{
  LDKUILayoutNode* node = x_arena_alloc_zero(ctx->frame_arena, sizeof(LDKUILayoutNode));

  if (node == NULL)
  {
    return NULL;
  }

  node->direction = direction;
  node->spacing = 4.0f;
  node->padding = 4.0f;
  node->parent = parent;
  node->window = window;
  node->items = x_array_ldk_ui_item_ptr_create(8);

  return node;
}

static LDKUIItem* ldk_ui_item_create(LDKUIContext* ctx)
{
  LDKUIItem* item = x_arena_alloc_zero(ctx->frame_arena, sizeof(LDKUIItem));

  if (item == NULL)
  {
    return NULL;
  }

  return item;
}

static void ldk_ui_layout_append_item(LDKUILayoutNode* layout, LDKUIItem* item)
{
  if (layout == NULL)
  {
    return;
  }

  if (item == NULL)
  {
    return;
  }

  x_array_ldk_ui_item_ptr_push(layout->items, item);
}

static void ldk_ui_reset_next_layout(LDKUIContext* ctx)
{
  memset(&ctx->next_layout, 0, sizeof(ctx->next_layout));
}

static void ldk_ui_apply_next_layout(LDKUIContext* ctx, LDKUIItem* item)
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

  ldk_ui_reset_next_layout(ctx);
}

static LDKUIWindow* ldk_ui_window_find(LDKUIContext* ctx, LDKUIId id)
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

static LDKUIWindow* ldk_ui_window_get_or_create(
    LDKUIContext* ctx,
    LDKUIId id,
    char const* title,
    LDKUIRect rect
    )
{
  LDKUIWindow* window = ldk_ui_window_find(ctx, id);

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

static void ldk_ui_window_bring_to_front(LDKUIContext* ctx, LDKUIWindow* window)
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

static void ldk_ui_add_draw_cmd(
    LDKUIContext* ctx,
    LDKUITextureHandle texture,
    LDKUIRect clip_rect,
    u32 index_offset,
    u32 index_count
    )
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

static void ldk_ui_emit_quad(
    LDKUIContext* ctx,
    LDKUIRect rect,
    u32 color,
    LDKUIRect clip_rect,
    LDKUITextureHandle texture
    )
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

  ldk_ui_add_draw_cmd(ctx, texture, clip_rect, index_offset, 6);
}

static bool ldk_ui_item_is_clicked(LDKUIContext* ctx, LDKUIItem* item)
{
  LDKPoint cursor = ldk_os_mouse_cursor((LDKMouseState*)ctx->mouse);
  bool inside = ldk_ui_rect_contains(item->rect, (float)cursor.x, (float)cursor.y);

  if (!inside)
  {
    return false;
  }

  if (!ldk_os_mouse_button_up((LDKMouseState*)ctx->mouse, LDK_MOUSE_BUTTON_LEFT))
  {
    return false;
  }

  if (ctx->active_id != item->id)
  {
    return false;
  }

  return true;
}

static void ldk_ui_resolve_interaction(LDKUIContext* ctx, LDKUIItem* item)
{
  LDKPoint cursor = ldk_os_mouse_cursor((LDKMouseState*)ctx->mouse);
  bool inside = ldk_ui_rect_contains(item->rect, (float)cursor.x, (float)cursor.y);

  if (inside)
  {
    ctx->hot_id = item->id;
  }

  if (inside && ldk_os_mouse_button_down((LDKMouseState*)ctx->mouse, LDK_MOUSE_BUTTON_LEFT))
  {
    ctx->active_id = item->id;
  }

  item->clicked = ldk_ui_item_is_clicked(ctx, item);

  if (item->type == LDK_UI_ITEM_TOGGLE_BUTTON)
  {
    if (item->clicked && item->data.toggle_button.value != NULL)
    {
      *item->data.toggle_button.value = !*item->data.toggle_button.value;
      item->changed = true;
    }
  }

  if (item->type == LDK_UI_ITEM_SLIDER_FLOAT)
  {
    bool active = ctx->active_id == item->id;
    bool down = ldk_os_mouse_button_is_pressed((LDKMouseState*)ctx->mouse, LDK_MOUSE_BUTTON_LEFT);

    if (active && down && item->data.slider_float.value != NULL)
    {
      float local = (float)cursor.x - item->rect.x;
      float denom = ldk_ui_maxf(item->rect.w, 1.0f);
      float t = ldk_ui_clampf(local / denom, 0.0f, 1.0f);
      float value = item->data.slider_float.min_value + (item->data.slider_float.max_value - item->data.slider_float.min_value) * t;

      if (*item->data.slider_float.value != value)
      {
        *item->data.slider_float.value = value;
        item->changed = true;
      }
    }
  }
}

static void ldk_ui_emit_item(LDKUIContext* ctx, LDKUIItem* item, LDKUIRect clip_rect)
{
  u32 color = 0xff606060u;

  if (item->type == LDK_UI_ITEM_LABEL)
  {
    return;
  }

  if (item->type == LDK_UI_ITEM_BUTTON)
  {
    if (ctx->active_id == item->id)
    {
      color = 0xff909090u;
    }
    else if (ctx->hot_id == item->id)
    {
      color = 0xff7a7a7au;
    }

    ldk_ui_emit_quad(ctx, item->rect, color, clip_rect, 0);
    return;
  }

  if (item->type == LDK_UI_ITEM_TOGGLE_BUTTON)
  {
    bool toggled = false;

    if (item->data.toggle_button.value != NULL)
    {
      toggled = *item->data.toggle_button.value;
    }

    color = toggled ? 0xff5f8f5fu : 0xff5f5f5fu;
    ldk_ui_emit_quad(ctx, item->rect, color, clip_rect, 0);
    return;
  }

  if (item->type == LDK_UI_ITEM_SLIDER_FLOAT)
  {
    float t = 0.0f;
    LDKUIRect fill_rect = item->rect;

    if (item->data.slider_float.value != NULL)
    {
      float range = item->data.slider_float.max_value - item->data.slider_float.min_value;

      if (range > 0.0f)
      {
        t = (*item->data.slider_float.value - item->data.slider_float.min_value) / range;
        t = ldk_ui_clampf(t, 0.0f, 1.0f);
      }
    }

    ldk_ui_emit_quad(ctx, item->rect, 0xff404040u, clip_rect, 0);

    fill_rect.w = item->rect.w * t;
    ldk_ui_emit_quad(ctx, fill_rect, 0xff8080c0u, clip_rect, 0);
    return;
  }
}

static LDKUISize ldk_ui_layout_measure_node(LDKUILayoutNode* node)
{
  LDKUISize size = { 0.0f, 0.0f };
  u32 count = x_array_ldk_ui_item_ptr_count(node->items);

  if (node->direction == LDK_UI_LAYOUT_VERTICAL)
  {
    float width = 0.0f;
    float height = node->padding * 2.0f;

    if (count > 0)
    {
      height += node->spacing * (float)(count - 1);
    }

    for (u32 i = 0; i < count; ++i)
    {
      LDKUIItem** ptr = x_array_ldk_ui_item_ptr_get(node->items, i);

      if (ptr == NULL || *ptr == NULL)
      {
        continue;
      }

      LDKUIItem* item = *ptr;

      if (item->type == LDK_UI_ITEM_LAYOUT && item->data.layout.node != NULL)
      {
        LDKUISize child_size = ldk_ui_layout_measure_node(item->data.layout.node);
        item->preferred_width = child_size.w;
        item->preferred_height = child_size.h;
      }

      width = ldk_ui_maxf(width, item->preferred_width);
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

  for (u32 i = 0; i < count; ++i)
  {
    LDKUIItem** ptr = x_array_ldk_ui_item_ptr_get(node->items, i);

    if (ptr == NULL || *ptr == NULL)
    {
      continue;
    }

    LDKUIItem* item = *ptr;

    if (item->type == LDK_UI_ITEM_LAYOUT && item->data.layout.node != NULL)
    {
      LDKUISize child_size = ldk_ui_layout_measure_node(item->data.layout.node);
      item->preferred_width = child_size.w;
      item->preferred_height = child_size.h;
    }

    width += item->preferred_width;
    height = ldk_ui_maxf(height, item->preferred_height);
  }

  size.w = width;
  size.h = height + node->padding * 2.0f;
  return size;
}

static void ldk_ui_layout_resolve_node(LDKUIContext* ctx, LDKUILayoutNode* node, LDKUIRect rect)
{
  u32 count = x_array_ldk_ui_item_ptr_count(node->items);
  float inner_x = rect.x + node->padding;
  float inner_y = rect.y + node->padding;
  float inner_w = rect.w - node->padding * 2.0f;
  float inner_h = rect.h - node->padding * 2.0f;

  node->rect = rect;

  if (node->direction == LDK_UI_LAYOUT_VERTICAL)
  {
    float fixed_height = 0.0f;
    u32 expand_count = 0;

    for (u32 i = 0; i < count; ++i)
    {
      LDKUIItem** ptr = x_array_ldk_ui_item_ptr_get(node->items, i);

      if (ptr == NULL || *ptr == NULL)
      {
        continue;
      }

      LDKUIItem* item = *ptr;

      if (item->expand_height)
      {
        expand_count += 1;
      }
      else
      {
        fixed_height += ldk_ui_maxf(item->preferred_height, item->min_height);
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

    for (u32 i = 0; i < count; ++i)
    {
      LDKUIItem** ptr = x_array_ldk_ui_item_ptr_get(node->items, i);

      if (ptr == NULL || *ptr == NULL)
      {
        continue;
      }

      LDKUIItem* item = *ptr;
      float item_h = ldk_ui_maxf(item->preferred_height, item->min_height);
      float item_w = item->expand_width ? inner_w : ldk_ui_minf(ldk_ui_maxf(item->preferred_width, item->min_width), inner_w);

      if (item->expand_height)
      {
        item_h = ldk_ui_maxf(expand_height, item->min_height);
      }

      item->rect.x = inner_x;
      item->rect.y = cursor_y;
      item->rect.w = item_w;
      item->rect.h = item_h;

      ldk_ui_resolve_interaction(ctx, item);

      if (item->type == LDK_UI_ITEM_LAYOUT && item->data.layout.node != NULL)
      {
        ldk_ui_layout_resolve_node(ctx, item->data.layout.node, item->rect);
      }
      else
      {
        ldk_ui_emit_item(ctx, item, rect);
      }

      cursor_y += item_h + node->spacing;
    }

    return;
  }

  float fixed_width = 0.0f;
  u32 expand_count = 0;

  for (u32 i = 0; i < count; ++i)
  {
    LDKUIItem** ptr = x_array_ldk_ui_item_ptr_get(node->items, i);

    if (ptr == NULL || *ptr == NULL)
    {
      continue;
    }

    LDKUIItem* item = *ptr;

    if (item->expand_width)
    {
      expand_count += 1;
    }
    else
    {
      fixed_width += ldk_ui_maxf(item->preferred_width, item->min_width);
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

  for (u32 i = 0; i < count; ++i)
  {
    LDKUIItem** ptr = x_array_ldk_ui_item_ptr_get(node->items, i);

    if (ptr == NULL || *ptr == NULL)
    {
      continue;
    }

    LDKUIItem* item = *ptr;
    float item_w = ldk_ui_maxf(item->preferred_width, item->min_width);
    float item_h = item->expand_height ? inner_h : ldk_ui_minf(ldk_ui_maxf(item->preferred_height, item->min_height), inner_h);

    if (item->expand_width)
    {
      item_w = ldk_ui_maxf(expand_width, item->min_width);
    }

    item->rect.x = cursor_x;
    item->rect.y = inner_y;
    item->rect.w = item_w;
    item->rect.h = item_h;

    ldk_ui_resolve_interaction(ctx, item);

    if (item->type == LDK_UI_ITEM_LAYOUT && item->data.layout.node != NULL)
    {
      ldk_ui_layout_resolve_node(ctx, item->data.layout.node, item->rect);
    }
    else
    {
      ldk_ui_emit_item(ctx, item, rect);
    }

    cursor_x += item_w + node->spacing;
  }
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

  bool success = ctx->frame_arena != NULL;
  return success;
}

void ldk_ui_terminate(LDKUIContext* ctx)
{
  u32 count = x_array_ldk_ui_window_count(ctx->windows);

  for (u32 i = 0; i < count; ++i)
  {
    LDKUIWindow* window = x_array_ldk_ui_window_get(ctx->windows, i);

    if (window != NULL && window->root_layout != NULL && window->root_layout->items != NULL)
    {
      x_array_ldk_ui_item_ptr_destroy(window->root_layout->items);
      window->root_layout->items = NULL;
    }
  }

  x_array_ldk_ui_window_destroy(ctx->windows);
  x_array_ldk_ui_id_destroy(ctx->id_stack);
  x_array_ldk_ui_vertex_destroy(ctx->vertices);
  x_array_ldk_ui_u32_destroy(ctx->indices);
  x_array_ldk_ui_draw_cmd_destroy(ctx->commands);
  x_arena_destroy(ctx->frame_arena);

  memset(ctx, 0, sizeof(*ctx));
}

void ldk_ui_begin_frame(
    LDKUIContext* ctx,
    LDKMouseState const* mouse,
    LDKKeyboardState const* keyboard,
    LDKUIRect viewport
    )
{
  u32 count = x_array_ldk_ui_window_count(ctx->windows);

  ctx->mouse = mouse;
  ctx->keyboard = keyboard;
  ctx->viewport = viewport;
  ctx->hot_id = 0;
  ctx->hovered_window = NULL;
  ctx->active_window = NULL;
  ctx->current_window = NULL;
  ctx->current_layout = NULL;

  ldk_ui_reset_next_layout(ctx);

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
  }

  if (ldk_os_mouse_button_up((LDKMouseState*)ctx->mouse, LDK_MOUSE_BUTTON_LEFT))
  {
    ctx->active_id = 0;
  }
}

void ldk_ui_end_frame(LDKUIContext* ctx)
{
  u32 count = x_array_ldk_ui_window_count(ctx->windows);

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

    window->title_bar_rect = window->rect;
    window->title_bar_rect.h = 24.0f;

    window->content_rect.x = window->rect.x;
    window->content_rect.y = window->rect.y + 24.0f;
    window->content_rect.w = window->rect.w;
    window->content_rect.h = window->rect.h - 24.0f;

    ldk_ui_emit_quad(ctx, window->rect, 0xff2a2a2au, window->rect, 0);
    ldk_ui_emit_quad(ctx, window->title_bar_rect, 0xff3a3a3au, window->rect, 0);

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

    ldk_ui_layout_measure_node(window->root_layout);
    ldk_ui_layout_resolve_node(ctx, window->root_layout, window->content_rect);
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
  LDKUIId hashed = ldk_ui_hash_cstr(2166136261u, value);
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

void ldk_ui_begin_vertical(LDKUIContext* ctx)
{
  LDKUILayoutNode* node = ldk_ui_layout_node_create(
      ctx,
      LDK_UI_LAYOUT_VERTICAL,
      ctx->current_layout,
      ctx->current_window
      );
  LDKUIItem* item = ldk_ui_item_create(ctx);

  if (node == NULL || item == NULL)
  {
    return;
  }

  item->type = LDK_UI_ITEM_LAYOUT;
  item->id = ldk_ui_make_id(ctx, LDK_DEFAULT_ID());
  item->data.layout.node = node;
  item->expand_width = true;
  item->preferred_width = 0.0f;
  item->preferred_height = 0.0f;

  ldk_ui_apply_next_layout(ctx, item);

  if (ctx->current_layout != NULL)
  {
    ldk_ui_layout_append_item(ctx->current_layout, item);
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
  LDKUILayoutNode* node = ldk_ui_layout_node_create(
      ctx,
      LDK_UI_LAYOUT_HORIZONTAL,
      ctx->current_layout,
      ctx->current_window
      );
  LDKUIItem* item = ldk_ui_item_create(ctx);

  if (node == NULL || item == NULL)
  {
    return;
  }

  item->type = LDK_UI_ITEM_LAYOUT;
  item->id = ldk_ui_make_id(ctx, LDK_DEFAULT_ID());
  item->data.layout.node = node;
  item->expand_width = true;
  item->preferred_width = 0.0f;
  item->preferred_height = 0.0f;

  ldk_ui_apply_next_layout(ctx, item);

  if (ctx->current_layout != NULL)
  {
    ldk_ui_layout_append_item(ctx->current_layout, item);
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

bool ldk_ui_begin_window(
    LDKUIContext* ctx,
    char const* title,
    LDKUIRect rect
    )
{
  return ldk_ui_begin_window_ex(ctx, LDK_DEFAULT_ID(), title, rect);
}

bool ldk_ui_begin_window_ex(
    LDKUIContext* ctx,
    char const* id,
    char const* title,
    LDKUIRect rect
    )
{
  LDKUIId resolved_id = ldk_ui_make_id(ctx, id);
  LDKUIWindow* window = ldk_ui_window_get_or_create(ctx, resolved_id, title, rect);
  LDKUILayoutNode* root = NULL;
  LDKPoint cursor;

  if (window == NULL)
  {
    return false;
  }

  cursor = ldk_os_mouse_cursor((LDKMouseState*)ctx->mouse);
  window->rect = rect;

  if (title != NULL)
  {
    strncpy(window->title, title, sizeof(window->title) - 1);
    window->title[sizeof(window->title) - 1] = 0;
  }

  window->title_bar_rect = rect;
  window->title_bar_rect.h = 24.0f;
  window->content_rect.x = rect.x;
  window->content_rect.y = rect.y + 24.0f;
  window->content_rect.w = rect.w;
  window->content_rect.h = rect.h - 24.0f;

  if (ldk_ui_rect_contains(window->title_bar_rect, (float)cursor.x, (float)cursor.y))
  {
    window->is_hovered = true;
    ctx->hovered_window = window;

    if (ldk_os_mouse_button_down((LDKMouseState*)ctx->mouse, LDK_MOUSE_BUTTON_LEFT))
    {
      window->is_dragging = true;
      window->drag_offset_x = (float)cursor.x - window->rect.x;
      window->drag_offset_y = (float)cursor.y - window->rect.y;
      ctx->active_window = window;
      ldk_ui_window_bring_to_front(ctx, window);
    }
  }

  root = ldk_ui_layout_node_create(ctx, LDK_UI_LAYOUT_VERTICAL, NULL, window);

  if (root == NULL)
  {
    return false;
  }

  window->root_layout = root;
  ctx->current_window = window;
  ctx->current_layout = root;

  return true;
}

void ldk_ui_end_window(LDKUIContext* ctx)
{
  ctx->current_layout = NULL;
  ctx->current_window = NULL;
}

void ldk_ui_label(LDKUIContext* ctx, char const* text)
{
  LDKUIItem* item = ldk_ui_item_create(ctx);
  LDKUISize size;

  if (item == NULL)
  {
    return;
  }

  size = ldk_ui_measure_label_impl(text);

  item->type = LDK_UI_ITEM_LABEL;
  item->text = text;
  item->preferred_width = size.w;
  item->preferred_height = size.h;
  item->min_width = size.w;
  item->min_height = size.h;

  ldk_ui_apply_next_layout(ctx, item);
  ldk_ui_layout_append_item(ctx->current_layout, item);
}

bool ldk_ui_button(LDKUIContext* ctx, char const* text)
{
  return ldk_ui_button_ex(ctx, LDK_DEFAULT_ID(), text);
}

bool ldk_ui_button_ex(LDKUIContext* ctx, char const* id, char const* text)
{
  LDKUIItem* item = ldk_ui_item_create(ctx);
  LDKUISize size;

  if (item == NULL)
  {
    return false;
  }

  size = ldk_ui_measure_button_impl(text);

  item->type = LDK_UI_ITEM_BUTTON;
  item->id = ldk_ui_make_id(ctx, id);
  item->text = text;
  item->preferred_width = size.w;
  item->preferred_height = size.h;
  item->min_width = size.w;
  item->min_height = size.h;

  ldk_ui_apply_next_layout(ctx, item);
  ldk_ui_layout_append_item(ctx->current_layout, item);

  return false;
}

bool ldk_ui_toggle_button(
    LDKUIContext* ctx,
    char const* text,
    bool* value
    )
{
  return ldk_ui_toggle_button_ex(ctx, LDK_DEFAULT_ID(), text, value);
}

bool ldk_ui_toggle_button_ex(
    LDKUIContext* ctx,
    char const* id,
    char const* text,
    bool* value
    )
{
  LDKUIItem* item = ldk_ui_item_create(ctx);
  LDKUISize size;

  if (item == NULL)
  {
    return false;
  }

  size = ldk_ui_measure_button_impl(text);

  item->type = LDK_UI_ITEM_TOGGLE_BUTTON;
  item->id = ldk_ui_make_id(ctx, id);
  item->text = text;
  item->preferred_width = size.w;
  item->preferred_height = size.h;
  item->min_width = size.w;
  item->min_height = size.h;
  item->data.toggle_button.value = value;

  ldk_ui_apply_next_layout(ctx, item);
  ldk_ui_layout_append_item(ctx->current_layout, item);

  return false;
}

bool ldk_ui_slider_float(
    LDKUIContext* ctx,
    char const* text,
    float* value,
    float min_value,
    float max_value
    )
{
  return ldk_ui_slider_float_ex(ctx, LDK_DEFAULT_ID(), text, value, min_value, max_value);
}

bool ldk_ui_slider_float_ex(
    LDKUIContext* ctx,
    char const* id,
    char const* text,
    float* value,
    float min_value,
    float max_value
    )
{
  LDKUIItem* item = ldk_ui_item_create(ctx);
  LDKUISize size;

  if (item == NULL)
  {
    return false;
  }

  size = ldk_ui_measure_slider_impl(text);

  item->type = LDK_UI_ITEM_SLIDER_FLOAT;
  item->id = ldk_ui_make_id(ctx, id);
  item->text = text;
  item->preferred_width = size.w;
  item->preferred_height = size.h;
  item->min_width = 80.0f;
  item->min_height = size.h;
  item->expand_width = true;
  item->data.slider_float.value = value;
  item->data.slider_float.min_value = min_value;
  item->data.slider_float.max_value = max_value;

  ldk_ui_apply_next_layout(ctx, item);
  ldk_ui_layout_append_item(ctx->current_layout, item);

  return false;
}

void ldk_ui_input_text(LDKUIContext* ctx, u32 codepoint)
{
  (void)ctx;
  (void)codepoint;
}
