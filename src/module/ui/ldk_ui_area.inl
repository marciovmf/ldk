#include <ldk_common.h>
#include <module/ldk_ui.h>

//------------------------------------------------------------
// Areas
//------------------------------------------------------------

bool ldk_ui_begin_area_ex(
    LDKUIContext *ctx, char const *title, LDKUIIcon icon, bool expanded)
{
  LDKUIAreaStackEntry *entry;
  LDKUIRect header_rect;
  bool clicked;

  if (ctx == NULL)
  {
    return expanded;
  }

  ldk_ui_set_next_height(ctx, ldk_ui_px(LDK_UI_AREA_HEADER_HEIGHT));
  u32 result = s_ui_tree_node_ex(ctx, title, icon, expanded, 0, 0, true);
  header_rect = ctx->last_rect;
  clicked = (result & LDK_UI_TREE_NODE_RESULT_CLICKED) != 0;

  if (clicked)
  {
    expanded = !expanded;
  }

  x_array_ldk_ui_area_stack_entry_push(
      ctx->area_stack, (LDKUIAreaStackEntry){0});
  entry = x_array_ldk_ui_area_stack_entry_back(ctx->area_stack);

  if (entry == NULL)
  {
    return expanded;
  }

  entry->expanded = expanded;
  entry->opened_layout = false;
  entry->header_rect = header_rect;

  if (expanded)
  {
    LDKUILayout *parent_layout = ctx->current_layout;
    ldk_ui_begin_vertical(ctx);
    LDKUILayout *layout = ctx->current_layout;
    if (layout != NULL && layout != parent_layout)
    {
      entry->opened_layout = true;
      ldk_ui_set_padding(ctx, ctx->theme.panel_padding);
      layout->spacing = LDK_UI_DEFAULT_SPACING + 2.0f;

      // Reserve the background before children so it remains behind them.
      LDKUIRect background = header_rect;
      background.y += header_rect.h;
      background.h = 0.0f;
      entry->background_vertices = s_ui_target_vertices(ctx);
      entry->background_vertex_index =
          x_array_ldk_ui_vertex_count(entry->background_vertices);
      s_ui_render_quad(ctx, background,
          ctx->theme.colors[LDK_UI_COLOR_PANEL_BG], ctx->clip_rect, 0);
      entry->has_background =
          x_array_ldk_ui_vertex_count(entry->background_vertices) ==
              entry->background_vertex_index + 4u;
    }
  }

  return expanded;
}

bool ldk_ui_begin_area(LDKUIContext *ctx, char const *title, bool expanded)
{
  LDKUIIcon icon = {0};
  return ldk_ui_begin_area_ex(ctx, title, icon, expanded);
}

LDKUIRect ldk_ui_area_header_rect(LDKUIContext *ctx)
{
  LDKUIAreaStackEntry *entry = ctx != NULL
      ? x_array_ldk_ui_area_stack_entry_back(ctx->area_stack) : NULL;
  return entry != NULL ? entry->header_rect : (LDKUIRect){0};
}

void ldk_ui_end_area(LDKUIContext *ctx)
{
  LDKUIAreaStackEntry entry;

  if (ctx == NULL)
  {
    return;
  }

  if (x_array_ldk_ui_area_stack_entry_is_empty(ctx->area_stack))
  {
    return;
  }

  {
    u32 count = x_array_ldk_ui_area_stack_entry_count(ctx->area_stack);
    LDKUIAreaStackEntry *entry_ptr =
        x_array_ldk_ui_area_stack_entry_get(ctx->area_stack, count - 1);

    if (entry_ptr == NULL)
    {
      return;
    }

    entry = *entry_ptr;
    x_array_ldk_ui_area_stack_entry_delete_at(ctx->area_stack, count - 1);
  }

  if (entry.opened_layout)
  {
    if (ctx->current_layout != NULL)
    {
      LDKUISize size =
          s_ui_layout_requested_content_size(ctx, ctx->current_layout);
      float bottom = ctx->current_layout->rect.y + size.h;
      LDKUILayout *parent = ctx->current_layout->parent;
      if (parent != NULL && parent->direction == LDK_UI_LAYOUT_VERTICAL)
      {
        parent->cursor.y = bottom + parent->spacing;
      }
      for (u32 i = 2u; entry.has_background && i < 4u; ++i)
      {
        LDKUIVertex *vertex = x_array_ldk_ui_vertex_get(
            entry.background_vertices, entry.background_vertex_index + i);
        if (vertex != NULL)
        {
          vertex->y = bottom;
        }
      }
    }
    ldk_ui_end_vertical(ctx);
  }
}
