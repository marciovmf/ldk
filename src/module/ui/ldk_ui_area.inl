#include <ldk_common.h>
#include <module/ldk_ui.h>

//------------------------------------------------------------
// Areas
//------------------------------------------------------------

bool ldk_ui_begin_area(LDKUIContext *ctx, char const *title, bool expanded)
{
  LDKUIAreaStackEntry *entry;
  bool clicked;

  if (ctx == NULL)
  {
    return expanded;
  }

  ldk_ui_set_next_height(ctx, ldk_ui_px(LDK_UI_DEFAULT_CONTROL_HEIGHT));
  clicked = ldk_ui_button_flat(ctx, title);

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

  if (expanded)
  {
    ldk_ui_begin_vertical(ctx);
    entry->opened_layout = true;
  }

  return expanded;
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
    ldk_ui_end_vertical(ctx);
  }
}
