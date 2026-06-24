#include <ldk_common.h>
#include <ldk_geom.h>
#include <module/ldk_ui.h>

#include <string.h>

#ifndef LDK_UI_SCROLLVIEW_WHEEL_STEP
#define LDK_UI_SCROLLVIEW_WHEEL_STEP 32.0f
#endif

#ifndef LDK_UI_SCROLLVIEW_WHEEL_DELTA_UNIT
#define LDK_UI_SCROLLVIEW_WHEEL_DELTA_UNIT 120.0f
#endif

//------------------------------------------------------------
// Scrollview helpers
//------------------------------------------------------------

static float s_scrollview_clampf(float value, float min_value, float max_value)
{
  if (value < min_value) {return min_value;}
  if (value > max_value) {return max_value;}
  return value;
}

static u32 s_scrollview_normalize_flags(u32 flags)
{
  if ((flags & (LDK_UI_SCROLL_VERTICAL | LDK_UI_SCROLL_HORIZONTAL)) == 0)
  {
    flags |= LDK_UI_SCROLL_VERTICAL;
  }

  return flags;
}

static float s_scrollview_clamp_scroll(float scroll,
  float view_size, float content_size)
{
  float max_scroll = s_ui_maxf(0.0f, content_size - view_size);
  if (max_scroll <= 0.0f)
  {
    return 0.0f;
  }
  return s_scrollview_clampf(scroll, 0.0f, max_scroll);
}

static bool s_scrollview_same_point(LDKUIPoint a, LDKUIPoint b)
{
  return a.x == b.x && a.y == b.y;
}

static bool s_scrollview_rect_contains_cursor(LDKUIContext* ctx, LDKUIRect rect)
{
  if (ctx == NULL || ctx->mouse == NULL)
  {
    return false;
  }

  LDKPoint cursor = ldk_os_mouse_cursor((LDKMouseState*)ctx->mouse);
  return ldk_rectf_contains(
    &rect,
    (float)cursor.x,
    (float)cursor.y);
}

static LDKUIPoint s_scrollview_apply_mouse_wheel(LDKUIContext* ctx,
  LDKUIPoint scroll, LDKUIRect view_rect, float content_w,
  float content_h, u32 flags)
{
  LDKUIRect clipped_view_rect;
  i32 wheel_delta;
  float wheel_steps;

  if (ctx == NULL || ctx->mouse == NULL)
  {
    return scroll;
  }

  clipped_view_rect = ldk_rectf_intersect(&ctx->clip_rect, &view_rect);

  if (!s_scrollview_rect_contains_cursor(ctx, clipped_view_rect))
  {
    return scroll;
  }

  wheel_delta = ldk_os_mouse_wheel_delta((LDKMouseState*)ctx->mouse);

  if (wheel_delta == 0)
  {
    return scroll;
  }

  wheel_steps = (float)wheel_delta / LDK_UI_SCROLLVIEW_WHEEL_DELTA_UNIT;

  if ((flags & LDK_UI_SCROLL_VERTICAL) != 0 && content_h > view_rect.h)
  {
    scroll.y -= wheel_steps * LDK_UI_SCROLLVIEW_WHEEL_STEP;
  }
  else if ((flags & LDK_UI_SCROLL_HORIZONTAL) != 0 && content_w > view_rect.w)
  {
    scroll.x -= wheel_steps * LDK_UI_SCROLLVIEW_WHEEL_STEP;
  }

  return scroll;
}

static bool s_scrollview_axis_visible(u32 flags, u32 axis_flag,
  float content_size, float view_size)
{
  if ((flags & axis_flag) == 0)
  {
    return false;
  }

  if ((flags & LDK_UI_SCROLL_IF_NEEDED) != 0)
  {
    return content_size > view_size;
  }

  return true;
}

static LDKUIScrollViewCache* s_scrollview_cache_find(
  LDKUIContext* ctx, LDKUIId id)
{
  if (ctx == NULL || id == 0)
  {
    return NULL;
  }

  for (u32 i = 0; i < x_array_ldk_ui_scrollview_cache_count(ctx->scrollview_cache); ++i)
  {
    LDKUIScrollViewCache* cache = x_array_ldk_ui_scrollview_cache_get(ctx->scrollview_cache, i);

    if (cache != NULL && cache->id == id)
    {
      return cache;
    }
  }

  return NULL;
}

static void s_scrollview_cache_gc(LDKUIContext* ctx)
{
  u32 i;

  if (ctx == NULL)
  {
    return;
  }

  i = 0;

  while (i < x_array_ldk_ui_scrollview_cache_count(ctx->scrollview_cache))
  {
    LDKUIScrollViewCache* cache = x_array_ldk_ui_scrollview_cache_get(ctx->scrollview_cache, i);

    if (cache == NULL)
    {
      i += 1;
      continue;
    }

    if (ctx->frame_index - cache->last_frame_touched > 2)
    {
      x_array_ldk_ui_scrollview_cache_delete_at(ctx->scrollview_cache, i);
      continue;
    }

    i += 1;
  }
}

static LDKUIScrollViewCache* s_scrollview_cache_get_or_create(
  LDKUIContext* ctx, LDKUIId id)
{

  if (ctx == NULL || id == 0)
  {
    return NULL;
  }

  LDKUIScrollViewCache* cache = s_scrollview_cache_find(ctx, id);

  if (cache != NULL)
  {
    cache->last_frame_touched = ctx->frame_index;
    return cache;
  }

  x_array_ldk_ui_scrollview_cache_push(ctx->scrollview_cache, (LDKUIScrollViewCache){0});
  cache = x_array_ldk_ui_scrollview_cache_back(ctx->scrollview_cache);

  if (cache == NULL)
  {
    return NULL;
  }

  cache->id = id;
  cache->last_frame_touched = ctx->frame_index;

  return cache;
}

static LDKUIRect s_scrollview_content_view_rect_from_view(LDKUIRect view_rect,
  float content_w, float content_h, u32 flags)
{
  LDKUIRect content_view_rect = view_rect;
  bool show_v = false;
  bool show_h = false;

  for (u32 i = 0; i < 3; ++i)
  {
    bool next_show_v;
    bool next_show_h;

    next_show_v = s_scrollview_axis_visible(
      flags,
      LDK_UI_SCROLL_VERTICAL,
      content_h,
      content_view_rect.h);

    next_show_h = s_scrollview_axis_visible(
      flags,
      LDK_UI_SCROLL_HORIZONTAL,
      content_w,
      content_view_rect.w);

    if (next_show_v == show_v && next_show_h == show_h)
    {
      break;
    }

    show_v = next_show_v;
    show_h = next_show_h;

    content_view_rect = view_rect;

    if (show_v)
    {
      content_view_rect.w = s_ui_maxf(
        0.0f,
        content_view_rect.w - LDK_UI_SCROLLBAR_SIZE);
    }

    if (show_h)
    {
      content_view_rect.h = s_ui_maxf(
        0.0f,
        content_view_rect.h - LDK_UI_SCROLLBAR_SIZE);
    }
  }

  return content_view_rect;
}

static LDKUIRect s_scrollview_content_rect_from_view(LDKUIRect content_view_rect,
  LDKUIPoint scroll, float content_w, float content_h, u32 flags)
{
  LDKUIRect content_rect;

  content_rect = content_view_rect;
  content_rect.x -= scroll.x;
  content_rect.y -= scroll.y;

  if ((flags & LDK_UI_SCROLL_HORIZONTAL) != 0)
  {
    content_rect.w = s_ui_maxf(content_view_rect.w, content_w);
  }

  if ((flags & LDK_UI_SCROLL_VERTICAL) != 0)
  {
    content_rect.h = s_ui_maxf(content_view_rect.h, content_h);
  }

  return content_rect;
}

static void s_scrollview_apply_content_rect(LDKUILayout* layout,
  LDKUIRect view_rect, LDKUIRect content_rect)
{
  LDK_ASSERT(layout);

  /*
    layout->rect is the visible scrollview box allocated by the parent.
    layout->content_rect is the scrollable content area used to place children.
    Children are rendered using rects produced from layout->content_rect, while
    ctx->clip_rect clips them to layout->rect / view_rect.
  */
  layout->rect = view_rect;
  layout->bounding_rect = view_rect;
  layout->content_rect = content_rect;
  layout->cursor.x = layout->content_rect.x;
  layout->cursor.y = layout->content_rect.y;
  layout->content_used_right = layout->content_rect.x;
  layout->content_used_bottom = layout->content_rect.y;

  layout->disable_main_axis_expand = true;
}

static void s_scrollview_prepare_layout_for_parent(LDKUILayout* layout,
    LDKUIRect view_rect)
{
  LDK_ASSERT(layout);
  layout->rect = view_rect;
  layout->bounding_rect = view_rect;
  layout->has_requested_size_override = true;
  layout->requested_size_override.w = view_rect.w;
  layout->requested_size_override.h = view_rect.h;
  layout->skip_parent_item_min_size_update = true;
  layout->disable_main_axis_expand = true;
}

static LDKUISize s_scrollview_content_size_from_measured_rect(
  LDKUIRect measured_rect)
{
  LDKUISize size = (LDKUISize){0};
  if (measured_rect.w > 0.0f || measured_rect.h > 0.0f)
  {
    size.w = measured_rect.w;
    size.h = measured_rect.h;
  }

  return size;
}

static void s_scrollview_draw_scrollbars(LDKUIContext* ctx, LDKUIId id,
  LDKUIRect view_rect, LDKUIRect content_view_rect, LDKUIPoint* scroll,
  LDKUIRect content_rect, u32 flags)
{
  if (ctx == NULL || scroll == NULL)
  {
    return;
  }

  bool show_v = s_scrollview_axis_visible(
    flags,
    LDK_UI_SCROLL_VERTICAL,
    content_rect.h,
    content_view_rect.h);

  bool show_h = s_scrollview_axis_visible(
    flags,
    LDK_UI_SCROLL_HORIZONTAL,
    content_rect.w,
    content_view_rect.w);

  if (show_v)
  {
    LDKUIRect v_rect = {0};
    v_rect.x = view_rect.x + view_rect.w - LDK_UI_SCROLLBAR_SIZE;
    v_rect.y = view_rect.y;
    v_rect.w = LDK_UI_SCROLLBAR_SIZE;
    v_rect.h = view_rect.h;

    LDKUIId v_id = s_ui_id_hash_u32(id, 0x53435659u);
    scroll->y = ldk_ui_widget_scrollbar_vertical(
      ctx,
      v_id,
      scroll->y,
      content_view_rect.h,
      content_rect.h,
      v_rect);
  }

  if (show_h)
  {
    LDKUIRect h_rect;
    h_rect.x = view_rect.x;
    h_rect.y = view_rect.y + view_rect.h - LDK_UI_SCROLLBAR_SIZE;
    h_rect.w = view_rect.w;
    h_rect.h = LDK_UI_SCROLLBAR_SIZE;

    LDKUIId h_id = s_ui_id_hash_u32(id, 0x53435658u);
    scroll->x = ldk_ui_widget_scrollbar_horizontal(
      ctx,
      h_id,
      scroll->x,
      content_view_rect.w,
      content_rect.w,
      h_rect);
  }

  if (show_v && show_h)
  {
    LDKUIRect corner_rect;
    corner_rect.x = view_rect.x + view_rect.w - LDK_UI_SCROLLBAR_SIZE;
    corner_rect.y = view_rect.y + view_rect.h - LDK_UI_SCROLLBAR_SIZE;
    corner_rect.w = LDK_UI_SCROLLBAR_SIZE;
    corner_rect.h = LDK_UI_SCROLLBAR_SIZE;

    LDKUIId corner_id = s_ui_id_hash_u32(id, 0x53435643u);
    ldk_ui_widget_panel(ctx, corner_id, corner_rect);
  }
}

//------------------------------------------------------------
// Public scrollview API
//------------------------------------------------------------

LDKUIPoint ldk_ui_begin_scrollview(
  LDKUIContext* ctx,
  LDKUIPoint scroll,
  u32 flags)
{
  LDK_ASSERT(ctx);
  LDK_ASSERT(ctx->scrollview_stack);
  LDK_ASSERT(ctx->current_layout);

  s_scrollview_cache_gc(ctx);
  flags = s_scrollview_normalize_flags(flags);
  ldk_ui_begin_vertical(ctx);

  LDKUILayout* layout = ctx->current_layout;
  LDKUIRect view_rect = layout->rect;
  LDKUIId id = s_ui_id_hash_u32(layout->id, (u32)LDK_UI_ITEM_SCROLLVIEW);
  LDKUIScrollViewCache* cache = s_scrollview_cache_get_or_create(ctx, id);

  if (cache != NULL && cache->has_scroll)
  {
    LDKUIPoint previous_returned;

    previous_returned.x = cache->returned_scroll_x;
    previous_returned.y = cache->returned_scroll_y;

    LDKUIPoint incoming_scroll = scroll;
    if (s_scrollview_same_point(incoming_scroll, previous_returned))
    {
      scroll.x = cache->scroll_x;
      scroll.y = cache->scroll_y;
    }
  }

  float content_w = cache != NULL ? cache->content_w : view_rect.w;
  float content_h = cache != NULL ? cache->content_h : view_rect.h;

  if ((flags & LDK_UI_SCROLL_HORIZONTAL) == 0)
  {
    content_w = view_rect.w;
    scroll.x = 0.0f;
  }

  if ((flags & LDK_UI_SCROLL_VERTICAL) == 0)
  {
    content_h = view_rect.h;
    scroll.y = 0.0f;
  }

  scroll = s_scrollview_apply_mouse_wheel(ctx, scroll, view_rect, content_w,
    content_h, flags);

  LDKUIRect content_view_rect = s_scrollview_content_view_rect_from_view(
    view_rect, content_w, content_h, flags);

  scroll.x = s_scrollview_clamp_scroll(scroll.x, content_view_rect.w, content_w);

  scroll.y = s_scrollview_clamp_scroll(scroll.y, content_view_rect.h, content_h);

  LDKUIRect content_rect = s_scrollview_content_rect_from_view(content_view_rect,
    scroll, content_w, content_h, flags);

  layout->id = id;
  ctx->last_id = id;

  x_array_ldk_ui_scrollview_stack_entry_push(ctx->scrollview_stack, (LDKUIScrollViewStackEntry){0});
  LDKUIScrollViewStackEntry* entry = x_array_ldk_ui_scrollview_stack_entry_back(ctx->scrollview_stack);

  if (entry == NULL)
  {
    return scroll;
  }

  entry->id = id;
  entry->layout = layout;
  entry->mark = (LDKUIMark){0};
  entry->rect = view_rect;
  entry->view_rect = view_rect;
  entry->content_rect = content_rect;
  entry->previous_clip_rect = ctx->clip_rect;
  entry->scroll = scroll;
  entry->flags = flags;

  ctx->clip_rect = ldk_rectf_intersect(&ctx->clip_rect, &content_view_rect);
  s_scrollview_apply_content_rect(layout, view_rect, content_rect);
  entry->mark = ldk_ui_mark(ctx);

  if (cache != NULL)
  {
    cache->returned_scroll_x = scroll.x;
    cache->returned_scroll_y = scroll.y;
    cache->last_frame_touched = ctx->frame_index;
  }

  return scroll;
}

void ldk_ui_end_scrollview(LDKUIContext* ctx)
{
  LDKUIPoint scroll;

  LDK_ASSERT(ctx);
  LDK_ASSERT(!x_array_ldk_ui_scrollview_stack_entry_is_empty(ctx->scrollview_stack));

  u32 stack_count = x_array_ldk_ui_scrollview_stack_entry_count(ctx->scrollview_stack);
  LDKUIScrollViewStackEntry* entry_ptr = x_array_ldk_ui_scrollview_stack_entry_get(ctx->scrollview_stack, stack_count - 1);

  if (entry_ptr == NULL)
  {
    return;
  }

  LDKUIScrollViewStackEntry entry = *entry_ptr;
  x_array_ldk_ui_scrollview_stack_entry_delete_at(ctx->scrollview_stack, stack_count - 1);

  LDKUILayout* layout = entry.layout;

  if (layout == NULL || ctx->current_layout != layout)
  {
    ctx->clip_rect = entry.previous_clip_rect;
    return;
  }

  LDKUIRect measured_rect = ldk_ui_measure_from(ctx, entry.mark);
  LDKUISize content_size = s_scrollview_content_size_from_measured_rect(
    measured_rect);
  LDKUIRect content_view_rect = s_scrollview_content_view_rect_from_view(
    entry.view_rect, content_size.w, content_size.h, entry.flags);

  scroll = entry.scroll;
  scroll.x = s_scrollview_clamp_scroll(
    scroll.x, content_view_rect.w, content_size.w);
  scroll.y = s_scrollview_clamp_scroll(
    scroll.y, content_view_rect.h, content_size.h);

  LDKUIScrollViewCache *cache = s_scrollview_cache_get_or_create(ctx, entry.id);

  if (cache != NULL)
  {
    cache->content_w = content_size.w;
    cache->content_h = content_size.h;
    cache->last_frame_touched = ctx->frame_index;
  }

  ctx->clip_rect = entry.previous_clip_rect;

  s_scrollview_prepare_layout_for_parent(layout, entry.view_rect);
  ldk_ui_end_vertical(ctx);

  measured_rect.w = content_size.w;
  measured_rect.h = content_size.h;

  s_scrollview_draw_scrollbars(ctx, entry.id, entry.view_rect, content_view_rect,
    &scroll, measured_rect, entry.flags);

  if (cache != NULL)
  {
    cache->scroll_x = scroll.x;
    cache->scroll_y = scroll.y;
    cache->has_scroll = true;
    cache->last_frame_touched = ctx->frame_index;
  }

  ctx->last_rect = entry.view_rect;
  ctx->last_bounding_rect = entry.view_rect;
  ctx->last_id = entry.id;
}
