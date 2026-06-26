#include <ldk_common.h>
#include <ldk_geom.h>
#include <module/ldk_ui.h>

static i32 s_ui_popup_open_index(LDKUIContext *ctx, LDKUIId id)
{
  if (ctx == NULL || ctx->open_popups == NULL || id == 0)
  {
    return -1;
  }

  for (u32 i = 0; i < x_array_ldk_ui_id_count(ctx->open_popups); ++i)
  {
    LDKUIId *open_id = x_array_ldk_ui_id_get(ctx->open_popups, i);

    if (open_id != NULL && *open_id == id)
    {
      return (i32)i;
    }
  }

  return -1;
}

static LDKUIPopupCache *s_ui_popup_cache_find(LDKUIContext *ctx, LDKUIId id)
{
  if (ctx == NULL || ctx->popup_cache == NULL || id == 0)
  {
    return NULL;
  }

  for (u32 i = 0; i < x_array_ldk_ui_popup_cache_count(ctx->popup_cache); ++i)
  {
    LDKUIPopupCache *cache = x_array_ldk_ui_popup_cache_get(ctx->popup_cache, i);

    if (cache != NULL && cache->id == id)
    {
      return cache;
    }
  }

  return NULL;
}

static LDKUIRect s_ui_popup_default_rect(LDKUIPoint position)
{
  return (LDKUIRect){
      position.x, position.y, 160.0f, LDK_UI_DEFAULT_CONTROL_HEIGHT};
}

static LDKUIPoint s_ui_popup_default_position(LDKUIContext *ctx)
{
  LDKUIPoint position = {0};

  if (ctx == NULL)
  {
    return position;
  }

  position.x = ctx->last_rect.x;
  position.y = ctx->last_rect.y + ctx->last_rect.h;

  if (ctx->last_rect.w <= 0.0f && ctx->last_rect.h <= 0.0f && ctx->mouse != NULL)
  {
    LDKPoint cursor = ldk_os_mouse_cursor((LDKMouseState *)ctx->mouse);
    position.x = (float)cursor.x;
    position.y = (float)cursor.y;
  }

  return position;
}

static LDKUIPopupCache *s_ui_popup_cache_get_or_create(
    LDKUIContext *ctx, LDKUIId id)
{
  LDKUIPopupCache *cache = s_ui_popup_cache_find(ctx, id);

  if (cache != NULL)
  {
    return cache;
  }

  if (ctx == NULL || ctx->popup_cache == NULL || id == 0)
  {
    return NULL;
  }

  x_array_ldk_ui_popup_cache_push(ctx->popup_cache, (LDKUIPopupCache){0});
  cache = x_array_ldk_ui_popup_cache_back(ctx->popup_cache);

  if (cache == NULL)
  {
    return NULL;
  }

  cache->id = id;
  cache->position = s_ui_popup_default_position(ctx);
  cache->rect = s_ui_popup_default_rect(cache->position);
  cache->last_frame_touched = ctx->frame_index;
  cache->has_rect = false;

  return cache;
}

void ldk_ui_open_popup(LDKUIContext *ctx, LDKUIId id)
{
  ldk_ui_open_popup_at(ctx, id, s_ui_popup_default_position(ctx));
}

void ldk_ui_open_popup_at(LDKUIContext *ctx, LDKUIId id, LDKUIPoint position)
{
  LDKUIPopupCache *cache;

  if (ctx == NULL || ctx->open_popups == NULL || id == 0)
  {
    return;
  }

  cache = s_ui_popup_cache_get_or_create(ctx, id);

  if (cache != NULL)
  {
    cache->position = position;
    cache->rect.x = position.x;
    cache->rect.y = position.y;

    if (!cache->has_rect)
    {
      cache->rect = s_ui_popup_default_rect(position);
    }

    cache->last_frame_touched = ctx->frame_index;
  }

  if (s_ui_popup_open_index(ctx, id) >= 0)
  {
    return;
  }

  x_array_ldk_ui_id_push(ctx->open_popups, id);
}

void ldk_ui_close_popup(LDKUIContext *ctx, LDKUIId id)
{
  if (ctx == NULL || ctx->open_popups == NULL || id == 0)
  {
    return;
  }

  i32 index = s_ui_popup_open_index(ctx, id);

  if (index < 0)
  {
    return;
  }

  x_array_ldk_ui_id_delete_at(ctx->open_popups, (u32)index);
}

void ldk_ui_close_current_popup(LDKUIContext *ctx)
{
  if (ctx == NULL || ctx->popup_stack == NULL)
  {
    return;
  }

  LDKUIPopupStackEntry *entry =
      x_array_ldk_ui_popup_stack_entry_back(ctx->popup_stack);

  if (entry == NULL)
  {
    return;
  }

  ldk_ui_close_popup(ctx, entry->id);
}

void ldk_ui_close_all_popups(LDKUIContext *ctx)
{
  if (ctx == NULL || ctx->open_popups == NULL)
  {
    return;
  }

  x_array_ldk_ui_id_clear(ctx->open_popups);
}

bool ldk_ui_popup_is_open(LDKUIContext *ctx, LDKUIId id)
{
  return s_ui_popup_open_index(ctx, id) >= 0;
}

bool ldk_ui_begin_popup(LDKUIContext *ctx, LDKUIId id)
{
  LDKUIPopupCache *cache;
  LDKUIRect rect;
  u32 measure_entry_index = UINT32_MAX;

  if (ctx == NULL || ctx->popup_stack == NULL || id == 0)
  {
    return false;
  }

  if (!ldk_ui_popup_is_open(ctx, id))
  {
    return false;
  }

  cache = s_ui_popup_cache_get_or_create(ctx, id);

  if (cache == NULL)
  {
    return false;
  }

  if (!cache->has_rect)
  {
    cache->rect = s_ui_popup_default_rect(cache->position);
  }

  cache->last_frame_touched = ctx->frame_index;
  rect = cache->rect;

  if (ctx->popup_frame_entries != NULL)
  {
    LDKUIPopupFrameEntry frame_entry;

    frame_entry.id = id;
    frame_entry.rect = rect;

    x_array_ldk_ui_popup_frame_entry_push(
        ctx->popup_frame_entries, frame_entry);
  }

  if (ctx->measure_entries != NULL)
  {
    measure_entry_index = x_array_ldk_ui_measure_entry_count(ctx->measure_entries);
    x_array_ldk_ui_measure_entry_push(
        ctx->measure_entries, (LDKUIMeasureEntry){0});
  }

  LDKUIPopupStackEntry entry = {0};
  entry.id = id;
  entry.rect = rect;
  entry.previous_clip_rect = ctx->clip_rect;
  entry.previous_layout = ctx->current_layout;
  entry.measure_entry_index = measure_entry_index;

  x_array_ldk_ui_popup_stack_entry_push(ctx->popup_stack, entry);

  ctx->clip_rect = s_ui_rect_intersect(&ctx->viewport, &rect);
  ctx->current_layout = NULL;

  LDKUIId layout_id = s_ui_id_hash_u32(id, 0x504f5055u);
  LDKUILayout *layout = s_ui_layout_push_with_id(ctx, LDK_UI_LAYOUT_VERTICAL,
      rect, rect, measure_entry_index, layout_id);

  if (layout != NULL)
  {
    layout->disable_main_axis_expand = true;
  }

  LDKUIId panel_id = s_ui_id_hash_u32(id, 0x504f5042u);
  ldk_ui_widget_panel(ctx, panel_id, rect);

  return true;
}

void ldk_ui_end_popup(LDKUIContext *ctx)
{
  if (ctx == NULL || ctx->popup_stack == NULL)
  {
    return;
  }

  if (x_array_ldk_ui_popup_stack_entry_is_empty(ctx->popup_stack))
  {
    return;
  }

  LDKUIPopupStackEntry *entry =
      x_array_ldk_ui_popup_stack_entry_back(ctx->popup_stack);

  if (entry == NULL)
  {
    return;
  }

  LDKUIId id = entry->id;
  LDKUIRect rect = entry->rect;
  LDKUIRect previous_clip_rect = entry->previous_clip_rect;
  LDKUILayout *previous_layout = entry->previous_layout;
  u32 measure_entry_index = entry->measure_entry_index;
  u32 count = x_array_ldk_ui_popup_stack_entry_count(ctx->popup_stack);

  s_ui_layout_pop(ctx);

  if (measure_entry_index != UINT32_MAX && ctx->measure_entries != NULL &&
      measure_entry_index < x_array_ldk_ui_measure_entry_count(ctx->measure_entries))
  {
    LDKUIMeasureEntry *measure_entry =
        x_array_ldk_ui_measure_entry_get(ctx->measure_entries, measure_entry_index);

    if (measure_entry != NULL)
    {
      rect.w = measure_entry->rect.w;
      rect.h = measure_entry->rect.h;
    }
  }

  LDKUIPopupCache *cache = s_ui_popup_cache_find(ctx, id);

  if (cache != NULL)
  {
    rect.x = cache->position.x;
    rect.y = cache->position.y;
    rect.w = s_ui_maxf(rect.w, 1.0f);
    rect.h = s_ui_maxf(rect.h, 1.0f);

    cache->rect = rect;
    cache->has_rect = true;
    cache->last_frame_touched = ctx->frame_index;
  }

  ctx->clip_rect = previous_clip_rect;
  ctx->current_layout = previous_layout;

  x_array_ldk_ui_popup_stack_entry_delete_at(ctx->popup_stack, count - 1);
}
