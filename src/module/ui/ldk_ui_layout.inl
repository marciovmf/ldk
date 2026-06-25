#include <ldk_common.h>
#include <ldk_geom.h>
#include <ldk_ttf.h>
#include <module/ldk_ui.h>
#include <stdx/stdx_math.h>
#include <string.h>

static LDKUIId s_ui_id_make_in_scope(
    LDKUIContext *ctx, u32 item_type, u32 item_index)
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
    LDKUIId *value = x_array_ldk_ui_id_get(ctx->id_stack, i);

    if (value != NULL)
    {
      hash = s_ui_id_hash_u32(hash, *value);
    }
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

static LDKUILayoutRequest s_ui_layout_apply_next_hints(
    LDKUIContext *ctx, LDKUILayoutRequest request)
{
  bool main_axis_fixed = false;

  if (ctx == NULL)
  {
    return request;
  }

  if (ctx->has_next_min_size)
  {
    request.min_size.w = s_ui_maxf(request.min_size.w, ctx->next_min_size.w);
    request.min_size.h = s_ui_maxf(request.min_size.h, ctx->next_min_size.h);

    ctx->has_next_min_size = false;
    ctx->next_min_size = (LDKUISize){0};
  }

  if (ctx->has_next_width)
  {
    if (ctx->next_width.mode == LDK_UI_SIZE_FILL)
    {
      if (ctx->current_layout != NULL &&
          ctx->current_layout->direction == LDK_UI_LAYOUT_HORIZONTAL)
      {
        request.weight = 1.0f;
      }
    }
    else
    {
      request.width = ctx->next_width;
      request.has_width = true;

      if (ctx->current_layout != NULL &&
          ctx->current_layout->direction == LDK_UI_LAYOUT_HORIZONTAL)
      {
        main_axis_fixed = true;
      }
    }

    ctx->has_next_width = false;
    ctx->next_width = (LDKUILayoutSize){0};
  }

  if (ctx->has_next_height)
  {
    if (ctx->next_height.mode == LDK_UI_SIZE_FILL)
    {
      if (ctx->current_layout != NULL &&
          ctx->current_layout->direction == LDK_UI_LAYOUT_VERTICAL)
      {
        request.weight = 1.0f;
      }
    }
    else
    {
      request.height = ctx->next_height;
      request.has_height = true;

      if (ctx->current_layout != NULL &&
          ctx->current_layout->direction == LDK_UI_LAYOUT_VERTICAL)
      {
        main_axis_fixed = true;
      }
    }

    ctx->has_next_height = false;
    ctx->next_height = (LDKUILayoutSize){0};
  }

  if (ctx->has_next_weight)
  {
    request.weight = ctx->next_weight;

    ctx->has_next_weight = false;
    ctx->next_weight = 0.0f;
  }
  else if (main_axis_fixed)
  {
    request.weight = 0.0f;
  }

  return request;
}

//------------------------------------------------------------
// Rect helpers
//------------------------------------------------------------

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

  return (LDKUIRect){x0, y0, x1 - x0, y1 - y0};
}

//------------------------------------------------------------
// Frame preparation
//------------------------------------------------------------

static void s_ui_layout_item_cache_gc(LDKUIContext *ctx)
{
  u32 i = 0;

  if (ctx == NULL)
  {
    return;
  }

  const u32 count =
      x_array_ldk_ui_layout_item_cache_count(ctx->layout_item_cache);
  while (i < count)
  {
    LDKUILayoutItemCache *cache =
        x_array_ldk_ui_layout_item_cache_get(ctx->layout_item_cache, i);

    if (cache == NULL)
    {
      i += 1;
      continue;
    }

    if (ctx->frame_index - cache->last_frame_touched > 2)
    {
      x_array_ldk_ui_layout_item_cache_delete_at(ctx->layout_item_cache, i);
      continue;
    }

    i += 1;
  }
}

static void s_ui_layout_prepare_frame(LDKUIContext *ctx)
{
  if (ctx == NULL)
  {
    return;
  }

  if (ctx->layout_frame_index == ctx->frame_index)
  {
    return;
  }

  ctx->layout_frame_index = ctx->frame_index;
  ctx->current_layout = NULL;
  x_array_ldk_ui_layout_clear(ctx->layout_stack);
  x_array_ldk_ui_layout_item_clear(ctx->layout_items);
  x_array_ldk_ui_measure_entry_clear(ctx->measure_entries);
  ctx->root_item_count = 0;
  ctx->last_measure_entry_index = UINT32_MAX;
  ctx->has_next_width = false;
  ctx->has_next_height = false;
  ctx->has_next_min_size = false;
  ctx->has_next_weight = false;
  ctx->next_weight = 0.0f;
  ctx->next_width = (LDKUILayoutSize){LDK_UI_SIZE_PIXELS, 0.0f};
  ctx->next_height = (LDKUILayoutSize){LDK_UI_SIZE_PIXELS, 0.0f};
  ctx->next_min_size = (LDKUISize){0};

  s_ui_layout_item_cache_gc(ctx);
}

//------------------------------------------------------------
// Measurement
//------------------------------------------------------------

static u32 s_ui_measure_entry_add(
    LDKUIContext *ctx, LDKUILayout *layout, LDKUIRect rect)
{
  u32 index = 0;

  if (ctx == NULL || layout == NULL)
  {
    return UINT32_MAX;
  }

  index = x_array_ldk_ui_measure_entry_count(ctx->measure_entries);
  x_array_ldk_ui_measure_entry_push(
      ctx->measure_entries, (LDKUIMeasureEntry){layout, rect});

  return index;
}

static void s_ui_measure_entry_set_at(
    LDKUIContext *ctx, u32 index, LDKUIRect rect)
{
  if (ctx == NULL || index == UINT32_MAX)
  {
    return;
  }

  if (index >= x_array_ldk_ui_measure_entry_count(ctx->measure_entries))
  {
    return;
  }

  {
    LDKUIMeasureEntry *entry =
        x_array_ldk_ui_measure_entry_get(ctx->measure_entries, index);

    if (entry != NULL)
    {
      entry->rect = rect;
    }
  }
}

static LDKUISize s_ui_layout_text_size(LDKUIContext *ctx, char const *text)
{
  LDKTextSize text_size = {0};
  LDKUISize size = {0};

  if (ctx == NULL || ctx->font == NULL || text == NULL)
  {
    return size;
  }

  text_size = ldk_ttf_measure_text_cstr(ctx->font, text);
  size.w = text_size.w;
  size.h = text_size.h;

  return size;
}

//------------------------------------------------------------
// Public size API
//------------------------------------------------------------

LDKUILayoutSize ldk_ui_px(float value)
{
  return (LDKUILayoutSize){LDK_UI_SIZE_PIXELS, value};
}

LDKUILayoutSize ldk_ui_percent(float value)
{
  return (LDKUILayoutSize){LDK_UI_SIZE_PERCENT, value};
}

LDKUILayoutSize ldk_ui_fill(void)
{
  return (LDKUILayoutSize){LDK_UI_SIZE_FILL, 0.0f};
}

void ldk_ui_set_next_width(LDKUIContext *ctx, LDKUILayoutSize width)
{
  if (ctx == NULL)
  {
    return;
  }

  s_ui_layout_prepare_frame(ctx);

  ctx->next_width = width;
  ctx->has_next_width = true;
}

void ldk_ui_set_next_height(LDKUIContext *ctx, LDKUILayoutSize height)
{
  if (ctx == NULL)
  {
    return;
  }

  s_ui_layout_prepare_frame(ctx);

  ctx->next_height = height;
  ctx->has_next_height = true;
}

void ldk_ui_set_next_size(
    LDKUIContext *ctx, LDKUILayoutSize width, LDKUILayoutSize height)
{
  if (ctx == NULL)
  {
    return;
  }

  s_ui_layout_prepare_frame(ctx);

  ctx->next_width = width;
  ctx->next_height = height;
  ctx->has_next_width = true;
  ctx->has_next_height = true;
}

void ldk_ui_set_next_min_size(LDKUIContext *ctx, float width, float height)
{
  if (ctx == NULL)
  {
    return;
  }

  s_ui_layout_prepare_frame(ctx);

  ctx->next_min_size = (LDKUISize){width, height};
  ctx->has_next_min_size = true;
}

void ldk_ui_set_next_weight(LDKUIContext *ctx, float weight)
{
  if (ctx == NULL)
  {
    return;
  }

  s_ui_layout_prepare_frame(ctx);

  ctx->next_weight = weight;
  ctx->has_next_weight = true;
}

LDKUIRect ldk_ui_last_rect(LDKUIContext *ctx)
{
  if (ctx == NULL)
  {
    return (LDKUIRect){0};
  }

  return ctx->last_rect;
}

LDKUIRect ldk_ui_last_bounding_rect(LDKUIContext *ctx)
{
  if (ctx == NULL)
  {
    return (LDKUIRect){0};
  }

  return ctx->last_bounding_rect;
}

LDKUIMark ldk_ui_mark(LDKUIContext *ctx)
{
  LDKUIMark mark = {0};

  if (ctx == NULL)
  {
    return mark;
  }

  s_ui_layout_prepare_frame(ctx);

  if (ctx->current_layout == NULL)
  {
    return mark;
  }

  mark.layout = ctx->current_layout;
  mark.measure_index = x_array_ldk_ui_measure_entry_count(ctx->measure_entries);

  return mark;
}

LDKUIRect ldk_ui_measure_from(LDKUIContext *ctx, LDKUIMark mark)
{
  LDKUIRect rect = {0};

  if (ctx == NULL || mark.layout == NULL)
  {
    return rect;
  }

  s_ui_layout_prepare_frame(ctx);

  if (mark.measure_index >
      x_array_ldk_ui_measure_entry_count(ctx->measure_entries))
  {
    return rect;
  }

  for (u32 i = mark.measure_index;
      i < x_array_ldk_ui_measure_entry_count(ctx->measure_entries); ++i)
  {
    LDKUIMeasureEntry *entry =
        x_array_ldk_ui_measure_entry_get(ctx->measure_entries, i);

    if (entry == NULL)
    {
      continue;
    }

    if (entry->layout != mark.layout)
    {
      continue;
    }

    rect = s_ui_rect_union_bounds(rect, entry->rect);
  }

  return rect;
}

static LDKUISize s_ui_layout_requested_content_size(
    LDKUIContext *ctx, LDKUILayout *layout);

//------------------------------------------------------------
// Layout core
//------------------------------------------------------------

static void s_ui_layout_expand_bounding_rect(
    LDKUILayout *layout, LDKUIRect rect)
{
  if (layout == NULL)
  {
    return;
  }

  layout->bounding_rect = s_ui_rect_union_bounds(layout->bounding_rect, rect);
  layout->content_used_right =
      s_ui_maxf(layout->content_used_right, s_ui_rect_right(rect));
  layout->content_used_bottom =
      s_ui_maxf(layout->content_used_bottom, s_ui_rect_bottom(rect));
}

static float s_ui_resolve_size(
    LDKUILayoutSize size, float parent_size, float remaining_size)
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

static float s_ui_layout_request_width(
    LDKUILayout const *layout, LDKUILayoutRequest const *request)
{
  if (layout == NULL || request == NULL)
  {
    return 0.0f;
  }

  if (request->has_width)
  {
    return s_ui_resolve_size(
        request->width, layout->content_rect.w, layout->content_rect.w);
  }

  return request->min_size.w;
}

static float s_ui_layout_request_height(
    LDKUILayout const *layout, LDKUILayoutRequest const *request)
{
  if (layout == NULL || request == NULL)
  {
    return 0.0f;
  }

  if (request->has_height)
  {
    return s_ui_resolve_size(
        request->height, layout->content_rect.h, layout->content_rect.h);
  }

  return request->min_size.h;
}

static LDKUIRect s_ui_layout_next_rect(LDKUIContext *ctx,
    LDKUILayoutSize default_width, LDKUILayoutSize default_height)
{
  LDKUIRect rect = {0};
  LDKUILayout *layout;
  LDKUILayoutSize width;
  LDKUILayoutSize height;
  float parent_w;
  float parent_h;
  float remaining_w;
  float remaining_h;
  LDKUIRect bounding_rect;

  if (ctx == NULL)
  {
    return rect;
  }

  s_ui_layout_prepare_frame(ctx);

  if (ctx->current_layout == NULL)
  {
    return rect;
  }

  layout = ctx->current_layout;
  width = ctx->has_next_width ? ctx->next_width : default_width;
  height = ctx->has_next_height ? ctx->next_height : default_height;

  ctx->has_next_width = false;
  ctx->has_next_height = false;

  parent_w = layout->content_rect.w;
  parent_h = layout->content_rect.h;
  remaining_w = s_ui_maxf(
      0.0f, layout->content_rect.x + layout->content_rect.w - layout->cursor.x);
  remaining_h = s_ui_maxf(
      0.0f, layout->content_rect.y + layout->content_rect.h - layout->cursor.y);

  bounding_rect.x = layout->cursor.x;
  bounding_rect.y = layout->cursor.y;
  bounding_rect.w = s_ui_resolve_size(width, parent_w, remaining_w);
  bounding_rect.h = s_ui_resolve_size(height, parent_h, remaining_h);

  rect = bounding_rect;
  rect.w = s_ui_minf(rect.w, remaining_w);
  rect.h = s_ui_minf(rect.h, remaining_h);

  s_ui_layout_expand_bounding_rect(layout, bounding_rect);

  ctx->last_rect = rect;
  ctx->last_bounding_rect = bounding_rect;
  ctx->last_measure_entry_index =
      s_ui_measure_entry_add(ctx, layout, bounding_rect);

  if (layout->direction == LDK_UI_LAYOUT_VERTICAL)
  {
    layout->cursor.y += bounding_rect.h + layout->spacing;
  }
  else
  {
    layout->cursor.x += bounding_rect.w + layout->spacing;
  }

  layout->item_count += 1;

  return rect;
}

static LDKUILayoutItemCache *s_ui_layout_item_cache_find(
    LDKUIContext *ctx, LDKUIId layout_id, u32 item_index)
{
  if (ctx == NULL || layout_id == 0)
  {
    return NULL;
  }

  for (u32 i = 0;
      i < x_array_ldk_ui_layout_item_cache_count(ctx->layout_item_cache); ++i)
  {
    LDKUILayoutItemCache *cache =
        x_array_ldk_ui_layout_item_cache_get(ctx->layout_item_cache, i);

    if (cache != NULL && cache->layout_id == layout_id &&
        cache->item_index == item_index)
    {
      return cache;
    }
  }

  return NULL;
}

static LDKUILayoutItemCache *s_ui_layout_item_cache_get_or_create(
    LDKUIContext *ctx, LDKUIId layout_id, u32 item_index)
{
  LDKUILayoutItemCache *cache;

  if (ctx == NULL || layout_id == 0)
  {
    return NULL;
  }

  cache = s_ui_layout_item_cache_find(ctx, layout_id, item_index);

  if (cache != NULL)
  {
    cache->last_frame_touched = ctx->frame_index;
    return cache;
  }

  x_array_ldk_ui_layout_item_cache_push(
      ctx->layout_item_cache, (LDKUILayoutItemCache){0});
  cache = x_array_ldk_ui_layout_item_cache_back(ctx->layout_item_cache);

  if (cache == NULL)
  {
    return NULL;
  }

  cache->layout_id = layout_id;
  cache->item_index = item_index;
  cache->last_frame_touched = ctx->frame_index;

  return cache;
}

static void s_ui_layout_item_record(LDKUIContext *ctx, LDKUILayout *layout,
    u32 item_index, LDKUILayoutRequest request, LDKUIRect fallback_rect)
{
  if (ctx == NULL || layout == NULL)
  {
    return;
  }

  LDKUILayoutItem new_item = {0};
  new_item.layout_id = layout->id;
  new_item.item_index = item_index;
  new_item.request = request;
  new_item.fallback_rect = fallback_rect;
  x_array_ldk_ui_layout_item_push(ctx->layout_items, new_item);
}

static LDKUILayoutItem *s_ui_layout_item_find(
    LDKUIContext *ctx, LDKUIId layout_id, u32 item_index)
{
  if (ctx == NULL || layout_id == 0)
  {
    return NULL;
  }

  for (u32 i = 0; i < x_array_ldk_ui_layout_item_count(ctx->layout_items); ++i)
  {
    LDKUILayoutItem *item =
        x_array_ldk_ui_layout_item_get(ctx->layout_items, i);

    if (item == NULL)
    {
      continue;
    }

    if (item->layout_id == layout_id && item->item_index == item_index)
    {
      return item;
    }
  }

  return NULL;
}

static void s_ui_layout_accept_rect(
    LDKUIContext *ctx, LDKUILayout *layout, LDKUIRect rect)
{
  if (ctx == NULL || layout == NULL)
  {
    return;
  }

  s_ui_layout_expand_bounding_rect(layout, rect);

  ctx->last_rect = rect;
  ctx->last_bounding_rect = rect;
  ctx->last_measure_entry_index = s_ui_measure_entry_add(ctx, layout, rect);

  if (layout->direction == LDK_UI_LAYOUT_VERTICAL)
  {
    layout->cursor.y = rect.y + rect.h + layout->spacing;
  }
  else
  {
    layout->cursor.x = rect.x + rect.w + layout->spacing;
  }

  layout->item_count += 1;
}

static LDKUIRect s_ui_layout_legacy_rect_from_request(
    LDKUIContext *ctx, LDKUILayoutRequest request)
{
  LDKUILayoutSize width;
  LDKUILayoutSize height;

  if (ctx == NULL || ctx->current_layout == NULL)
  {
    return (LDKUIRect){0};
  }

  if (ctx->current_layout->direction == LDK_UI_LAYOUT_HORIZONTAL)
  {
    width = request.has_width ? request.width : ldk_ui_px(request.min_size.w);
    height = request.has_height ? request.height : ldk_ui_fill();
  }
  else
  {
    width = request.has_width ? request.width : ldk_ui_fill();
    height =
        request.has_height ? request.height : ldk_ui_px(request.min_size.h);
  }

  return s_ui_layout_next_rect(ctx, width, height);
}

static LDKUIRect s_ui_layout_next_rect_from_request(
    LDKUIContext *ctx, LDKUILayoutRequest request)
{
  LDKUILayout *layout;
  u32 item_index;
  LDKUILayoutItemCache *cache;
  LDKUIRect fallback_rect;

  if (ctx == NULL || ctx->current_layout == NULL)
  {
    return (LDKUIRect){0};
  }

  request = s_ui_layout_apply_next_hints(ctx, request);

  layout = ctx->current_layout;
  item_index = layout->item_count;
  cache = s_ui_layout_item_cache_find(ctx, layout->id, item_index);

  if (cache != NULL)
  {
    LDKUIRect rect = cache->rect;
    rect.x += layout->content_rect.x;
    rect.y += layout->content_rect.y;

    s_ui_layout_accept_rect(ctx, layout, rect);
    s_ui_layout_item_record(ctx, layout, item_index, request, rect);
    return rect;
  }

  fallback_rect = s_ui_layout_legacy_rect_from_request(ctx, request);
  s_ui_layout_item_record(ctx, layout, item_index, request, fallback_rect);

  return fallback_rect;
}

static void s_ui_layout_solve_and_cache(LDKUIContext *ctx, LDKUILayout *layout)
{
  u32 item_count = 0;
  float min_total = 0.0f;
  float weight_total = 0.0f;
  float spacing_total;
  float available;
  float extra;
  float cursor;

  if (ctx == NULL || layout == NULL)
  {
    return;
  }

  for (u32 i = 0; i < x_array_ldk_ui_layout_item_count(ctx->layout_items); ++i)
  {
    LDKUILayoutItem *item =
        x_array_ldk_ui_layout_item_get(ctx->layout_items, i);

    if (item == NULL)
    {
      continue;
    }

    if (item->layout_id != layout->id)
    {
      continue;
    }

    item_count += 1;

    if (layout->direction == LDK_UI_LAYOUT_HORIZONTAL)
    {
      min_total += s_ui_layout_request_width(layout, &item->request);
    }
    else
    {
      min_total += s_ui_layout_request_height(layout, &item->request);
    }

    if (item->request.weight > 0.0f)
    {
      weight_total += item->request.weight;
    }
  }

  if (item_count == 0)
  {
    return;
  }

  spacing_total = layout->spacing * (float)(item_count - 1);
  available = layout->direction == LDK_UI_LAYOUT_HORIZONTAL
                  ? layout->content_rect.w
                  : layout->content_rect.h;
  extra = s_ui_maxf(0.0f, available - spacing_total - min_total);

  if (layout->disable_main_axis_expand)
  {
    extra = 0.0f;
  }

  cursor = layout->direction == LDK_UI_LAYOUT_HORIZONTAL
               ? layout->content_rect.x
               : layout->content_rect.y;

  for (u32 i = 0; i < x_array_ldk_ui_layout_item_count(ctx->layout_items); ++i)
  {
    LDKUILayoutItem *item =
        x_array_ldk_ui_layout_item_get(ctx->layout_items, i);

    if (item == NULL)
    {
      continue;
    }
    float main_size;
    LDKUIRect rect = {0};
    LDKUILayoutItemCache *cache;

    if (item->layout_id != layout->id)
    {
      continue;
    }

    if (layout->direction == LDK_UI_LAYOUT_HORIZONTAL)
    {
      main_size = s_ui_layout_request_width(layout, &item->request);

      if (weight_total > 0.0f && item->request.weight > 0.0f)
      {
        main_size += extra * (item->request.weight / weight_total);
      }

      rect.x = cursor;
      rect.y = layout->content_rect.y;
      rect.w = main_size;
      rect.h = item->request.has_height
                   ? s_ui_layout_request_height(layout, &item->request)
                   : layout->content_rect.h;

      cursor += rect.w + layout->spacing;
    }
    else
    {
      main_size = s_ui_layout_request_height(layout, &item->request);

      if (weight_total > 0.0f && item->request.weight > 0.0f)
      {
        main_size += extra * (item->request.weight / weight_total);
      }

      rect.x = layout->content_rect.x;
      rect.y = cursor;
      rect.w = item->request.has_width
                   ? s_ui_layout_request_width(layout, &item->request)
                   : layout->content_rect.w;
      rect.h = main_size;

      cursor += rect.h + layout->spacing;
    }

    cache =
        s_ui_layout_item_cache_get_or_create(ctx, layout->id, item->item_index);

    if (cache != NULL)
    {
      LDKUIRect cached_rect = rect;
      cached_rect.x -= layout->content_rect.x;
      cached_rect.y -= layout->content_rect.y;
      cache->rect = cached_rect;
      cache->last_frame_touched = ctx->frame_index;
    }
  }
}

static LDKUISize s_ui_layout_requested_content_size(
    LDKUIContext *ctx, LDKUILayout *layout)
{
  LDKUISize size = {0};
  u32 item_count = 0;

  if (ctx == NULL || layout == NULL)
  {
    return size;
  }

  for (u32 i = 0; i < x_array_ldk_ui_layout_item_count(ctx->layout_items); ++i)
  {
    LDKUILayoutItem *item =
        x_array_ldk_ui_layout_item_get(ctx->layout_items, i);

    if (item == NULL)
    {
      continue;
    }
    float item_width;
    float item_height;

    if (item->layout_id != layout->id)
    {
      continue;
    }

    item_width = s_ui_layout_request_width(layout, &item->request);
    item_height = s_ui_layout_request_height(layout, &item->request);

    if (layout->direction == LDK_UI_LAYOUT_HORIZONTAL)
    {
      size.w += item_width;
      size.h = s_ui_maxf(size.h, item_height);
    }
    else
    {
      size.w = s_ui_maxf(size.w, item_width);
      size.h += item_height;
    }

    item_count += 1;
  }

  if (item_count > 1)
  {
    float spacing_total = layout->spacing * (float)(item_count - 1);

    if (layout->direction == LDK_UI_LAYOUT_HORIZONTAL)
    {
      size.w += spacing_total;
    }
    else
    {
      size.h += spacing_total;
    }
  }

  size.w += layout->padding * 2.0f;
  size.h += layout->padding * 2.0f;

  return size;
}

static LDKUILayout *s_ui_layout_push_with_id(LDKUIContext *ctx,
    LDKUILayoutDirection direction, LDKUIRect rect, LDKUIRect bounding_rect,
    u32 measure_entry_index, LDKUIId id)
{
  LDKUILayout *parent;
  LDKUILayout *layout;

  if (ctx == NULL)
  {
    return NULL;
  }

  s_ui_layout_prepare_frame(ctx);

  parent = ctx->current_layout;
  x_array_ldk_ui_layout_push(ctx->layout_stack, (LDKUILayout){0});
  layout = x_array_ldk_ui_layout_back(ctx->layout_stack);

  if (layout == NULL)
  {
    return NULL;
  }

  memset(layout, 0, sizeof(*layout));
  layout->id = id;
  layout->direction = direction;
  layout->rect = rect;
  layout->bounding_rect = bounding_rect;
  layout->padding =
      direction == LDK_UI_LAYOUT_VERTICAL ? LDK_UI_DEFAULT_PADDING : 0.0f;
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
  layout->measure_entry_index = measure_entry_index;
  layout->has_measure_entry = measure_entry_index != UINT32_MAX;

  s_ui_layout_expand_bounding_rect(layout, layout->bounding_rect);

  ctx->current_layout = layout;

  return layout;
}

static void s_ui_layout_pop(LDKUIContext *ctx)
{
  LDKUILayout *layout;
  LDKUILayout *parent;
  LDKUISize requested_size;

  if (ctx == NULL)
  {
    return;
  }

  s_ui_layout_prepare_frame(ctx);

  if (ctx->current_layout == NULL)
  {
    return;
  }

  layout = ctx->current_layout;
  parent = layout->parent;

  s_ui_layout_solve_and_cache(ctx, layout);

  requested_size = s_ui_layout_requested_content_size(ctx, layout);

  if (layout->has_requested_size_override)
  {
    requested_size = layout->requested_size_override;
  }

  if (!layout->skip_parent_item_min_size_update && parent != NULL &&
      parent->item_count > 0)
  {
    u32 parent_item_index = parent->item_count - 1;
    LDKUILayoutItem *parent_item =
        s_ui_layout_item_find(ctx, parent->id, parent_item_index);

    if (parent_item != NULL)
    {
      parent_item->request.min_size = requested_size;
    }
  }

  if (layout->has_measure_entry)
  {
    LDKUIRect measured_rect = layout->rect;
    measured_rect.w = requested_size.w;
    measured_rect.h = requested_size.h;
    s_ui_measure_entry_set_at(ctx, layout->measure_entry_index, measured_rect);
  }

  if (parent != NULL)
  {
    LDKUIRect measured_rect = layout->rect;
    measured_rect.w = requested_size.w;
    measured_rect.h = requested_size.h;
    s_ui_layout_expand_bounding_rect(parent, measured_rect);
  }

  ctx->last_rect = layout->rect;
  ctx->last_bounding_rect = layout->bounding_rect;
  ctx->last_measure_entry_index = layout->measure_entry_index;
  ctx->current_layout = parent;

  if (!x_array_ldk_ui_layout_is_empty(ctx->layout_stack))
  {
    u32 count = x_array_ldk_ui_layout_count(ctx->layout_stack);
    x_array_ldk_ui_layout_delete_at(ctx->layout_stack, count - 1);
  }
}

static LDKUILayoutRequest s_ui_layout_request_make(
    LDKUIItemType type, LDKUISize min_size, float weight, bool hit_test)
{
  LDKUILayoutRequest request = {0};

  request.type = type;
  request.min_size = min_size;
  request.weight = weight;
  request.hit_test = hit_test;

  return request;
}

static bool s_ui_layout_rect_from_request(LDKUIContext *ctx,
    LDKUILayoutRequest request, LDKUIRect *out_rect, LDKUIId *out_id)
{
  u32 item_index;
  LDKUIRect rect;
  LDKUIId id;

  if (ctx == NULL || out_rect == NULL || out_id == NULL)
  {
    return false;
  }

  s_ui_layout_prepare_frame(ctx);

  if (ctx->current_layout == NULL)
  {
    return false;
  }

  item_index = ctx->current_layout->item_count;
  id = s_ui_id_make_in_scope(ctx, (u32)request.type, item_index);
  rect = s_ui_layout_next_rect_from_request(ctx, request);

  *out_rect = rect;
  *out_id = id;

  return true;
}

//------------------------------------------------------------
// Layout public API
//------------------------------------------------------------

static void s_ui_begin_layout(LDKUIContext *ctx, LDKUILayoutDirection direction)
{
  LDKUIItemType item_type;

  if (ctx == NULL)
  {
    return;
  }

  s_ui_layout_prepare_frame(ctx);

  item_type = direction == LDK_UI_LAYOUT_VERTICAL
                  ? LDK_UI_ITEM_LAYOUT_VERTICAL
                  : LDK_UI_ITEM_LAYOUT_HORIZONTAL;

  if (ctx->current_layout == NULL)
  {
    LDKUIRect rect = ctx->viewport;
    LDKUIId id =
        s_ui_id_make_in_scope(ctx, (u32)item_type, ctx->root_item_count);
    ctx->root_item_count += 1;
    s_ui_layout_push_with_id(ctx, direction, rect, rect, UINT32_MAX, id);
    return;
  }

  LDKUISize min_size = {0.0f, 0.0f};

  if (direction == LDK_UI_LAYOUT_HORIZONTAL)
  {
    min_size.h = LDK_UI_DEFAULT_CONTROL_HEIGHT;
  }
  else
  {
    min_size.w = LDK_UI_DEFAULT_CONTROL_HEIGHT;
  }

  LDKUILayoutRequest request =
      s_ui_layout_request_make(item_type, min_size, 1.0f, false);
  LDKUIRect rect = {0};
  LDKUIId id = 0;
  u32 measure_entry_index;
  LDKUIRect bounding_rect;

  if (!s_ui_layout_rect_from_request(ctx, request, &rect, &id))
  {
    return;
  }

  measure_entry_index = ctx->last_measure_entry_index;
  bounding_rect = ctx->last_bounding_rect;

  s_ui_layout_push_with_id(
      ctx, direction, rect, bounding_rect, measure_entry_index, id);
}

void ldk_ui_begin_vertical(LDKUIContext *ctx)
{
  s_ui_begin_layout(ctx, LDK_UI_LAYOUT_VERTICAL);
}

void ldk_ui_begin_horizontal(LDKUIContext *ctx)
{
  s_ui_begin_layout(ctx, LDK_UI_LAYOUT_HORIZONTAL);
}

void ldk_ui_end(LDKUIContext *ctx)
{
  s_ui_layout_pop(ctx);
}

void ldk_ui_end_vertical(LDKUIContext *ctx)
{
  ldk_ui_end(ctx);
}

void ldk_ui_end_horizontal(LDKUIContext *ctx)
{
  ldk_ui_end(ctx);
}

//------------------------------------------------------------
// Layout widget wrappers
//------------------------------------------------------------

void ldk_ui_image(
    LDKUIContext *ctx, LDKUITextureHandle texture, LDKUIRect uv, LDKUISize size)
{
  LDKUILayoutRequest request;
  LDKUIRect rect;
  LDKUIId id;

  request = s_ui_layout_request_make(LDK_UI_ITEM_IMAGE, size, 0.0f, false);

  if (!s_ui_layout_rect_from_request(ctx, request, &rect, &id))
  {
    return;
  }

  ldk_ui_widget_image(ctx, id, texture, uv, rect);
}

void ldk_ui_icon(LDKUIContext *ctx, LDKUIIcon icon)
{
  ldk_ui_image(ctx, icon.texture, icon.uv, icon.size);
}

bool ldk_ui_icon_button(LDKUIContext *ctx, LDKUIIcon icon)
{
  LDKUILayoutRequest request;
  LDKUIRect rect;
  LDKUIRect icon_rect;
  LDKUIRect saved_last_rect;
  LDKUIRect saved_last_bounding_rect;
  LDKUIId saved_last_id;
  LDKUIId id;
  LDKUISize min_size;
  bool clicked;

  min_size.w = icon.size.w;
  min_size.h = s_ui_maxf(LDK_UI_DEFAULT_CONTROL_HEIGHT, icon.size.h);

  request =
      s_ui_layout_request_make(LDK_UI_ITEM_ICON_BUTTON, min_size, 1.0f, true);

  if (!s_ui_layout_rect_from_request(ctx, request, &rect, &id))
  {
    return false;
  }

  clicked = ldk_ui_widget_button(ctx, id, "", rect);

  icon_rect.x = rect.x + (rect.w - icon.size.w) * 0.5f;
  icon_rect.y = rect.y + (rect.h - icon.size.h) * 0.5f;
  icon_rect.w = icon.size.w;
  icon_rect.h = icon.size.h;

  saved_last_rect = ctx->last_rect;
  saved_last_bounding_rect = ctx->last_bounding_rect;
  saved_last_id = ctx->last_id;

  ldk_ui_widget_image(
      ctx, s_ui_id_hash_u32(id, 0x49434f4eu), icon.texture, icon.uv, icon_rect);

  ctx->last_rect = saved_last_rect;
  ctx->last_bounding_rect = saved_last_bounding_rect;
  ctx->last_id = saved_last_id;

  return clicked;
}

void ldk_ui_label(LDKUIContext *ctx, char const *text)
{
  LDKUISize text_size;
  LDKUISize min_size;
  LDKUILayoutRequest request;
  LDKUIRect rect;
  LDKUIId id;

  if (text == NULL)
  {
    text = "";
  }

  text_size = s_ui_layout_text_size(ctx, text);
  min_size.w = text_size.w + 4.0f;
  min_size.h = s_ui_maxf(LDK_UI_DEFAULT_CONTROL_HEIGHT, text_size.h);

  request = s_ui_layout_request_make(LDK_UI_ITEM_LABEL, min_size, 1.0f, false);

  if (!s_ui_layout_rect_from_request(ctx, request, &rect, &id))
  {
    return;
  }

  ldk_ui_widget_label(ctx, id, text, rect);
}

bool ldk_ui_button(LDKUIContext *ctx, char const *text)
{
  LDKUISize text_size;
  LDKUISize min_size;
  LDKUILayoutRequest request;
  LDKUIRect rect;
  LDKUIId id;

  if (text == NULL)
  {
    text = "";
  }

  text_size = s_ui_layout_text_size(ctx, text);
  min_size.w = text_size.w + 16.0f;
  min_size.h = LDK_UI_DEFAULT_CONTROL_HEIGHT;

  request = s_ui_layout_request_make(LDK_UI_ITEM_BUTTON, min_size, 1.0f, true);

  if (!s_ui_layout_rect_from_request(ctx, request, &rect, &id))
  {
    return false;
  }

  return ldk_ui_widget_button(ctx, id, text, rect);
}

bool ldk_ui_toggle(LDKUIContext *ctx, bool value)
{
  LDKUISize text_size;
  LDKUISize min_size;
  LDKUILayoutRequest request;
  LDKUIRect rect;
  LDKUIId id;

  // if (text == NULL)
  //{
  //   text = "";
  // }

  // text_size = s_ui_layout_text_size(ctx, text);
  // min_size.w = text_size.w + 16.0f;
  min_size.w = 32;
  min_size.h = LDK_UI_DEFAULT_CONTROL_HEIGHT;

  request = s_ui_layout_request_make(LDK_UI_ITEM_TOGGLE, min_size, 1.0f, true);

  if (!s_ui_layout_rect_from_request(ctx, request, &rect, &id))
  {
    return false;
  }

  return ldk_ui_widget_toggle(ctx, id, value, rect);
}

bool ldk_ui_button_flat(LDKUIContext *ctx, char const *text)
{
  LDKUISize text_size;
  LDKUISize min_size;
  LDKUILayoutRequest request;
  LDKUIRect rect;
  LDKUIId id;

  if (text == NULL)
  {
    text = "";
  }

  text_size = s_ui_layout_text_size(ctx, text);
  min_size.w = text_size.w + 16.0f;
  min_size.h = LDK_UI_DEFAULT_CONTROL_HEIGHT;

  request = s_ui_layout_request_make(LDK_UI_ITEM_BUTTON, min_size, 1.0f, true);

  if (!s_ui_layout_rect_from_request(ctx, request, &rect, &id))
  {
    return false;
  }

  return ldk_ui_widget_button_flat(ctx, id, text, rect);
}

float ldk_ui_slider(LDKUIContext *ctx, float value, float min_value, float max_value)
{
  LDKUILayoutRequest request;
  LDKUIRect rect;
  LDKUIId id;
  LDKUISize min_size;

  min_size.w = 140.0f;
  min_size.h = LDK_UI_DEFAULT_CONTROL_HEIGHT;

  request = s_ui_layout_request_make(LDK_UI_ITEM_SLIDER, min_size, 1.0f, true);

  if (!s_ui_layout_rect_from_request(ctx, request, &rect, &id))
  {
    return value;
  }

  return ldk_ui_widget_slider(ctx, id, value, min_value, max_value, rect);
}

u32 ldk_ui_input_box(LDKUIContext *ctx, char *buffer, u32 buffer_size)
{
  LDKUILayoutRequest request;
  LDKUIRect rect;
  LDKUIId id;
  LDKUISize min_size;

  min_size.w = 140.0f;
  min_size.h = LDK_UI_DEFAULT_CONTROL_HEIGHT;

  request =
      s_ui_layout_request_make(LDK_UI_ITEM_INPUT_BOX, min_size, 1.0f, true);

  if (!s_ui_layout_rect_from_request(ctx, request, &rect, &id))
  {
    return LDK_UI_INPUT_BOX_NONE;
  }

  return ldk_ui_widget_input_box(ctx, id, buffer, buffer_size, rect);
}

u32 ldk_ui_input_label(LDKUIContext *ctx, char *buffer, u32 buffer_size)
{
  LDKUILayoutRequest request;
  LDKUIRect rect;
  LDKUIId id;
  LDKUISize min_size;

  min_size.w = 140.0f;
  min_size.h = LDK_UI_DEFAULT_CONTROL_HEIGHT;

  request =
      s_ui_layout_request_make(LDK_UI_ITEM_INPUT_BOX, min_size, 1.0f, true);

  if (!s_ui_layout_rect_from_request(ctx, request, &rect, &id))
  {
    return LDK_UI_INPUT_BOX_NONE;
  }

  return ldk_ui_widget_input_label(ctx, id, buffer, buffer_size, rect);
}

void ldk_ui_horizontal_line(LDKUIContext *ctx)
{
  LDKUILayoutRequest request;
  LDKUIRect rect;
  LDKUIId id;
  LDKUISize min_size;

  min_size.w = 1.0f;
  min_size.h = 1.0f;

  request =
      s_ui_layout_request_make(LDK_UI_ITEM_SEPARATOR, min_size, 0.0f, false);

  if (!s_ui_layout_rect_from_request(ctx, request, &rect, &id))
  {
    return;
  }

  ldk_ui_widget_panel(ctx, id, rect);
}

void ldk_ui_spacer(LDKUIContext *ctx)
{
  LDKUILayoutRequest request;
  LDKUIRect rect;
  LDKUIId id;

  request = s_ui_layout_request_make(
      LDK_UI_ITEM_SPACER, (LDKUISize){0.0f, 0.0f}, 1.0f, false);

  if (!s_ui_layout_rect_from_request(ctx, request, &rect, &id))
  {
    return;
  }

  (void)rect;
  (void)id;
}
