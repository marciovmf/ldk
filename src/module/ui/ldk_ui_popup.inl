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

void ldk_ui_open_popup(LDKUIContext *ctx, LDKUIId id)
{
  if (ctx == NULL || ctx->open_popups == NULL || id == 0)
  {
    return;
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

bool ldk_ui_begin_popup(LDKUIContext *ctx, LDKUIId id, LDKUIRect rect)
{
  if (ctx == NULL || ctx->popup_stack == NULL || id == 0)
  {
    return false;
  }

  if (!ldk_ui_popup_is_open(ctx, id))
  {
    return false;
  }

  if (ctx->popup_frame_entries != NULL)
  {
    LDKUIPopupFrameEntry frame_entry;

    frame_entry.id = id;
    frame_entry.rect = rect;

    x_array_ldk_ui_popup_frame_entry_push(
        ctx->popup_frame_entries, frame_entry);
  }

  LDKUIPopupStackEntry entry = {0};
  entry.id = id;
  entry.rect = rect;
  entry.previous_clip_rect = ctx->clip_rect;
  entry.previous_layout = ctx->current_layout;

  x_array_ldk_ui_popup_stack_entry_push(ctx->popup_stack, entry);

  ctx->clip_rect = s_ui_rect_intersect(&ctx->viewport, &rect);
  ctx->current_layout = NULL;

  LDKUIId layout_id = s_ui_id_hash_u32(id, 0x504f5055u);
  s_ui_layout_push_with_id(
      ctx, LDK_UI_LAYOUT_VERTICAL, rect, rect, UINT32_MAX, layout_id);

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

  LDKUIRect previous_clip_rect = entry->previous_clip_rect;
  LDKUILayout *previous_layout = entry->previous_layout;
  u32 count = x_array_ldk_ui_popup_stack_entry_count(ctx->popup_stack);

  s_ui_layout_pop(ctx);

  ctx->clip_rect = previous_clip_rect;
  ctx->current_layout = previous_layout;

  x_array_ldk_ui_popup_stack_entry_delete_at(ctx->popup_stack, count - 1);
}
