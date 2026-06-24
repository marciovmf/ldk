
typedef enum LDKUIWindowResizeEdge
{
  LDK_UI_WINDOW_RESIZE_NONE = 0,
  LDK_UI_WINDOW_RESIZE_LEFT = 1 << 0,
  LDK_UI_WINDOW_RESIZE_RIGHT = 1 << 1,
  LDK_UI_WINDOW_RESIZE_TOP = 1 << 2,
  LDK_UI_WINDOW_RESIZE_BOTTOM = 1 << 3,
} LDKUIWindowResizeEdge;

static void s_ui_window_destroy_buffers(LDKUIWindow *window)
{
  if (window == NULL)
  {
    return;
  }

  if (window->vertices != NULL)
  {
    x_array_destroy(window->vertices);
    window->vertices = NULL;
  }

  if (window->indices != NULL)
  {
    x_array_destroy(window->indices);
    window->indices = NULL;
  }

  if (window->commands != NULL)
  {
    x_array_destroy(window->commands);
    window->commands = NULL;
  }
}

static void s_ui_windows_destroy_all(LDKUIContext *ctx)
{
  if (ctx == NULL)
  {
    return;
  }

  for (u32 i = 0; i < x_array_ldk_ui_window_count(ctx->windows); ++i)
  {
    LDKUIWindow *window = x_array_ldk_ui_window_get(ctx->windows, i);

    if (window != NULL)
    {
      s_ui_window_destroy_buffers(window);
    }
  }

  x_array_ldk_ui_window_clear(ctx->windows);
}

static void s_ui_windows_clear_frame_buffers(LDKUIContext *ctx)
{
  if (ctx == NULL)
  {
    return;
  }

  for (u32 i = 0; i < x_array_ldk_ui_window_count(ctx->windows); ++i)
  {
    LDKUIWindow *window = x_array_ldk_ui_window_get(ctx->windows, i);

    if (window == NULL)
    {
      continue;
    }

    if (window->vertices != NULL)
    {
      x_array_ldk_ui_vertex_clear(window->vertices);
    }

    if (window->indices != NULL)
    {
      x_array_ldk_ui_u32_clear(window->indices);
    }

    if (window->commands != NULL)
    {
      x_array_ldk_ui_draw_cmd_clear(window->commands);
    }
  }
}

static void s_ui_windows_refresh_z_order(LDKUIContext *ctx)
{
  if (ctx == NULL)
  {
    return;
  }

  for (u32 i = 0; i < x_array_ldk_ui_window_count(ctx->windows); ++i)
  {
    LDKUIWindow *window = x_array_ldk_ui_window_get(ctx->windows, i);

    if (window != NULL)
    {
      window->z_order = (i32)i;
    }
  }

  ctx->next_z_order = (i32)x_array_ldk_ui_window_count(ctx->windows);
}

static void s_ui_append_window_draw_data(LDKUIContext *ctx, LDKUIWindow *window)
{
  u32 base_vertex;
  u32 base_index;
  u32 vertex_count;
  u32 index_count;
  u32 command_count;

  if (ctx == NULL || window == NULL)
  {
    return;
  }

  if (window->last_frame_seen != ctx->frame_index)
  {
    return;
  }

  if (window->vertices == NULL || window->indices == NULL ||
      window->commands == NULL)
  {
    return;
  }

  base_vertex = x_array_ldk_ui_vertex_count(ctx->vertices);
  base_index = x_array_ldk_ui_u32_count(ctx->indices);
  vertex_count = x_array_ldk_ui_vertex_count(window->vertices);
  index_count = x_array_ldk_ui_u32_count(window->indices);
  command_count = x_array_ldk_ui_draw_cmd_count(window->commands);

  for (u32 i = 0; i < vertex_count; ++i)
  {
    LDKUIVertex *vertex = x_array_ldk_ui_vertex_get(window->vertices, i);

    if (vertex != NULL)
    {
      x_array_ldk_ui_vertex_push(ctx->vertices, *vertex);
    }
  }

  for (u32 i = 0; i < index_count; ++i)
  {
    u32 *index = x_array_ldk_ui_u32_get(window->indices, i);

    if (index != NULL)
    {
      x_array_ldk_ui_u32_push(ctx->indices, *index + base_vertex);
    }
  }

  for (u32 i = 0; i < command_count; ++i)
  {
    LDKUIDrawCmd *command = x_array_ldk_ui_draw_cmd_get(window->commands, i);

    if (command != NULL)
    {
      LDKUIDrawCmd adjusted = *command;
      adjusted.index_offset += base_index;
      x_array_ldk_ui_draw_cmd_push(ctx->commands, adjusted);
    }
  }
}

static void s_ui_submit_windows_in_z_order(LDKUIContext *ctx)
{
  if (ctx == NULL)
  {
    return;
  }

  for (u32 i = 0; i < x_array_ldk_ui_window_count(ctx->windows); ++i)
  {
    LDKUIWindow *window = x_array_ldk_ui_window_get(ctx->windows, i);

    if (window != NULL)
    {
      s_ui_append_window_draw_data(ctx, window);
    }
  }
}

static LDKUIId s_ui_window_id_make(
    LDKUIContext *ctx, char const *title, u32 item_index)
{
  LDKUIId id;

  id = s_ui_id_make_in_scope(ctx, (u32)LDK_UI_ITEM_WINDOW, item_index);
  id = s_ui_id_hash_cstr(id, title);

  if (ctx != NULL)
  {
    ctx->last_id = id;
  }

  return id;
}

static LDKUIWindow *s_ui_window_find(LDKUIContext *ctx, LDKUIId id)
{
  if (ctx == NULL || id == 0)
  {
    return NULL;
  }

  for (u32 i = 0; i < x_array_ldk_ui_window_count(ctx->windows); ++i)
  {
    LDKUIWindow *window = x_array_ldk_ui_window_get(ctx->windows, i);

    if (window != NULL && window->id == id)
    {
      return window;
    }
  }

  return NULL;
}

static LDKUIWindow *s_ui_window_get_or_create(
    LDKUIContext *ctx, LDKUIId id, char const *title)
{
  LDKUIWindow *window;

  if (ctx == NULL || id == 0)
  {
    return NULL;
  }

  window = s_ui_window_find(ctx, id);

  if (window != NULL)
  {
    return window;
  }

  x_array_ldk_ui_window_push(ctx->windows, (LDKUIWindow){0});
  window = x_array_ldk_ui_window_back(ctx->windows);

  if (window == NULL)
  {
    return NULL;
  }

  memset(window, 0, sizeof(*window));

  window->id = id;
  window->z_order = (i32)(x_array_ldk_ui_window_count(ctx->windows) - 1);
  ctx->next_z_order = (i32)x_array_ldk_ui_window_count(ctx->windows);
  window->vertices = x_array_ldk_ui_vertex_create(256);
  window->indices = x_array_ldk_ui_u32_create(512);
  window->commands = x_array_ldk_ui_draw_cmd_create(64);

  if (window->vertices == NULL || window->indices == NULL ||
      window->commands == NULL)
  {
    u32 count = x_array_ldk_ui_window_count(ctx->windows);
    s_ui_window_destroy_buffers(window);

    if (count > 0)
    {
      x_array_ldk_ui_window_delete_at(ctx->windows, count - 1);
    }

    s_ui_windows_refresh_z_order(ctx);
    return NULL;
  }

  ctx->focused_window_id = window->id;
  ctx->active_window_id = window->id;
  ctx->focused_id = window->focused_id;
  s_ui_windows_refresh_z_order(ctx);

  if (title != NULL)
  {
    strncpy(window->title, title, sizeof(window->title) - 1);
    window->title[sizeof(window->title) - 1] = 0;
  }

  return window;
}

static void s_ui_window_cache_gc(LDKUIContext *ctx)
{
  u32 i = 0;

  if (ctx == NULL)
  {
    return;
  }

  while (i < x_array_ldk_ui_window_count(ctx->windows))
  {
    LDKUIWindow *window = x_array_ldk_ui_window_get(ctx->windows, i);

    if (window == NULL)
    {
      i += 1;
      continue;
    }

    if (ctx->frame_index - window->last_frame_seen > 2)
    {
      if (ctx->hovered_window_id == window->id)
      {
        ctx->hovered_window_id = 0;
      }

      if (ctx->focused_window_id == window->id)
      {
        ctx->focused_window_id = 0;
      }

      if (ctx->active_window_id == window->id)
      {
        ctx->active_window_id = 0;
      }

      if (ctx->dragging_window_id == window->id)
      {
        ctx->dragging_window_id = 0;
      }

      if (ctx->resizing_window_id == window->id)
      {
        ctx->resizing_window_id = 0;
        ctx->resizing_window_edges = LDK_UI_WINDOW_RESIZE_NONE;
      }

      s_ui_window_destroy_buffers(window);
      x_array_ldk_ui_window_delete_at(ctx->windows, i);
      s_ui_windows_refresh_z_order(ctx);
      continue;
    }

    i += 1;
  }
}

static LDKUIWindow *s_ui_window_topmost_at_cursor(LDKUIContext *ctx)
{
  LDKPoint cursor;

  if (ctx == NULL || ctx->mouse == NULL)
  {
    return NULL;
  }

  cursor = ldk_os_mouse_cursor((LDKMouseState *)ctx->mouse);

  for (u32 i = x_array_ldk_ui_window_count(ctx->windows); i > 0; --i)
  {
    LDKUIWindow *window = x_array_ldk_ui_window_get(ctx->windows, i - 1);

    if (window == NULL)
    {
      continue;
    }

    if (s_ui_rect_contains(&window->rect, (float)cursor.x, (float)cursor.y))
    {
      return window;
    }
  }

  return NULL;
}

static void s_ui_window_prepare_frame(LDKUIContext *ctx)
{
  LDKUIWindow *hovered_window;
  bool mouse_pressed = false;

  if (ctx == NULL)
  {
    return;
  }

  if (ctx->window_frame_index == ctx->frame_index)
  {
    return;
  }

  ctx->window_frame_index = ctx->frame_index;
  ctx->current_window = NULL;
  x_array_ldk_ui_window_stack_entry_clear(ctx->window_stack);
  ctx->last_window_close_requested = false;

  if (ctx->next_z_order == 0)
  {
    ctx->next_z_order = 1;
  }

  s_ui_window_cache_gc(ctx);

  hovered_window = s_ui_window_topmost_at_cursor(ctx);
  ctx->hovered_window_id = hovered_window != NULL ? hovered_window->id : 0;

  if (ctx->mouse != NULL)
  {
    mouse_pressed = ldk_os_mouse_button_is_pressed(
        (LDKMouseState *)ctx->mouse, LDK_MOUSE_BUTTON_LEFT);
  }

  if (!mouse_pressed)
  {
    ctx->dragging_window_id = 0;
    ctx->resizing_window_id = 0;
    ctx->resizing_window_edges = LDK_UI_WINDOW_RESIZE_NONE;
  }
}

static u32 s_ui_window_index_of(LDKUIContext *ctx, LDKUIId id)
{
  if (ctx == NULL || id == 0)
  {
    return UINT32_MAX;
  }

  for (u32 i = 0; i < x_array_ldk_ui_window_count(ctx->windows); ++i)
  {
    LDKUIWindow *window = x_array_ldk_ui_window_get(ctx->windows, i);

    if (window != NULL && window->id == id)
    {
      return i;
    }
  }

  return UINT32_MAX;
}

static LDKUIWindow *s_ui_window_move_to_end(
    LDKUIContext *ctx, LDKUIWindow *window)
{
  u32 index;
  LDKUIWindow saved;

  if (ctx == NULL || window == NULL)
  {
    return window;
  }

  index = s_ui_window_index_of(ctx, window->id);

  if (index == UINT32_MAX)
  {
    return window;
  }

  if (index + 1 == x_array_ldk_ui_window_count(ctx->windows))
  {
    s_ui_windows_refresh_z_order(ctx);
    return x_array_ldk_ui_window_get(ctx->windows, index);
  }

  saved = *x_array_ldk_ui_window_get(ctx->windows, index);
  x_array_ldk_ui_window_delete_at(ctx->windows, index);
  x_array_ldk_ui_window_push(ctx->windows, saved);
  s_ui_windows_refresh_z_order(ctx);

  return x_array_ldk_ui_window_get(
      ctx->windows, x_array_ldk_ui_window_count(ctx->windows) - 1);
}

static LDKUIWindow *s_ui_window_bring_to_front(
    LDKUIContext *ctx, LDKUIWindow *window)
{
  if (ctx == NULL || window == NULL)
  {
    return window;
  }

  window = s_ui_window_move_to_end(ctx, window);

  ctx->focused_window_id = window->id;
  ctx->active_window_id = window->id;
  ctx->focused_id = window->focused_id;

  return window;
}

static float s_ui_window_border_size(LDKUIContext *ctx, u32 flags)
{
  if (ctx == NULL)
  {
    return 0.0f;
  }

  if ((flags & LDK_UI_WINDOW_BORDER) == 0)
  {
    return 0.0f;
  }

  return ctx->theme.window_border_size;
}

static LDKUIRect s_ui_window_title_bar_rect(LDKUIRect rect, u32 flags)
{
  LDKUIRect title_bar_rect = {0};

  if ((flags & LDK_UI_WINDOW_TITLE_BAR) == 0)
  {
    return title_bar_rect;
  }

  title_bar_rect.x = rect.x;
  title_bar_rect.y = rect.y;
  title_bar_rect.w = rect.w;
  title_bar_rect.h = LDK_UI_TITLE_BAR_HEIGHT;

  return title_bar_rect;
}

static LDKUIRect s_ui_window_content_rect(
    LDKUIContext *ctx, LDKUIRect rect, u32 flags)
{
  LDKUIRect content_rect;
  float border_size;
  float title_h;

  border_size = s_ui_window_border_size(ctx, flags);
  title_h =
      (flags & LDK_UI_WINDOW_TITLE_BAR) != 0 ? LDK_UI_TITLE_BAR_HEIGHT : 0.0f;

  content_rect.x = rect.x + border_size;
  content_rect.y = rect.y + title_h + border_size;
  content_rect.w = s_ui_maxf(0.0f, rect.w - border_size * 2.0f);
  content_rect.h = s_ui_maxf(0.0f, rect.h - title_h - border_size * 2.0f);

  return content_rect;
}

static LDKUIRect s_ui_window_close_button_rect(LDKUIWindow const *window)
{
  LDKUIRect rect = {0};
  float size = 16.0f;

  if (window == NULL)
  {
    return rect;
  }

  rect.x = window->title_bar_rect.x + window->title_bar_rect.w - size - 4.0f;
  rect.y = window->title_bar_rect.y + (window->title_bar_rect.h - size) * 0.5f;
  rect.w = size;
  rect.h = size;

  return rect;
}

static u32 s_ui_window_resize_edges_at_cursor(
    LDKUIContext *ctx, LDKUIWindow const *window, LDKPoint cursor)
{
  u32 edges = LDK_UI_WINDOW_RESIZE_NONE;
  float border_size;
  float x;
  float y;

  if (ctx == NULL || window == NULL)
  {
    return LDK_UI_WINDOW_RESIZE_NONE;
  }

  if ((window->flags & LDK_UI_WINDOW_RESIZABLE) == 0)
  {
    return LDK_UI_WINDOW_RESIZE_NONE;
  }

  if (!s_ui_rect_contains(&window->rect, (float)cursor.x, (float)cursor.y))
  {
    return LDK_UI_WINDOW_RESIZE_NONE;
  }

  border_size = ctx->theme.window_interaction_border_size;
  x = (float)cursor.x;
  y = (float)cursor.y;

  if (x <= window->rect.x + border_size)
  {
    edges |= LDK_UI_WINDOW_RESIZE_LEFT;
  }

  if (x >= window->rect.x + window->rect.w - border_size)
  {
    edges |= LDK_UI_WINDOW_RESIZE_RIGHT;
  }

  if (y <= window->rect.y + border_size)
  {
    edges |= LDK_UI_WINDOW_RESIZE_TOP;
  }

  if (y >= window->rect.y + window->rect.h - border_size)
  {
    edges |= LDK_UI_WINDOW_RESIZE_BOTTOM;
  }

  return edges;
}

static void s_ui_window_set_resize_cursor(LDKUIContext *ctx, u32 resize_edges)
{
  if (ctx == NULL || resize_edges == LDK_UI_WINDOW_RESIZE_NONE)
  {
    return;
  }

  if (resize_edges == (LDK_UI_WINDOW_RESIZE_TOP | LDK_UI_WINDOW_RESIZE_LEFT) ||
      resize_edges ==
          (LDK_UI_WINDOW_RESIZE_BOTTOM | LDK_UI_WINDOW_RESIZE_RIGHT))
  {
    ctx->cursor_type = LDK_CURSOR_SIZE_NWSE;
  }
  else if (resize_edges ==
               (LDK_UI_WINDOW_RESIZE_TOP | LDK_UI_WINDOW_RESIZE_RIGHT) ||
           resize_edges ==
               (LDK_UI_WINDOW_RESIZE_BOTTOM | LDK_UI_WINDOW_RESIZE_LEFT))
  {
    ctx->cursor_type = LDK_CURSOR_SIZE_NESW;
  }
  else if (resize_edges == LDK_UI_WINDOW_RESIZE_TOP ||
           resize_edges == LDK_UI_WINDOW_RESIZE_BOTTOM)
  {
    ctx->cursor_type = LDK_CURSOR_SIZE_NS;
  }
  else if (resize_edges == LDK_UI_WINDOW_RESIZE_LEFT ||
           resize_edges == LDK_UI_WINDOW_RESIZE_RIGHT)
  {
    ctx->cursor_type = LDK_CURSOR_SIZE_WE;
  }
}

static LDKUIRect s_ui_window_apply_resize(
    LDKUIContext *ctx, LDKUIRect rect, LDKPoint cursor)
{
  LDKUIRect result;
  float dx;
  float dy;
  u32 edges;

  if (ctx == NULL)
  {
    return rect;
  }

  result = ctx->resize_start_rect;
  dx = (float)cursor.x - ctx->resize_start_cursor_x;
  dy = (float)cursor.y - ctx->resize_start_cursor_y;
  edges = ctx->resizing_window_edges;

  if ((edges & LDK_UI_WINDOW_RESIZE_LEFT) != 0)
  {
    result.x = ctx->resize_start_rect.x + dx;
    result.w = ctx->resize_start_rect.w - dx;

    if (result.w < LDK_UI_MIN_WINDOW_WIDTH)
    {
      result.x = ctx->resize_start_rect.x + ctx->resize_start_rect.w -
                 LDK_UI_MIN_WINDOW_WIDTH;
      result.w = LDK_UI_MIN_WINDOW_WIDTH;
    }
  }

  if ((edges & LDK_UI_WINDOW_RESIZE_RIGHT) != 0)
  {
    result.w = ctx->resize_start_rect.w + dx;

    if (result.w < LDK_UI_MIN_WINDOW_WIDTH)
    {
      result.w = LDK_UI_MIN_WINDOW_WIDTH;
    }
  }

  if ((edges & LDK_UI_WINDOW_RESIZE_TOP) != 0)
  {
    result.y = ctx->resize_start_rect.y + dy;
    result.h = ctx->resize_start_rect.h - dy;

    if (result.h < LDK_UI_MIN_WINDOW_HEIGHT)
    {
      result.y = ctx->resize_start_rect.y + ctx->resize_start_rect.h -
                 LDK_UI_MIN_WINDOW_HEIGHT;
      result.h = LDK_UI_MIN_WINDOW_HEIGHT;
    }
  }

  if ((edges & LDK_UI_WINDOW_RESIZE_BOTTOM) != 0)
  {
    result.h = ctx->resize_start_rect.h + dy;

    if (result.h < LDK_UI_MIN_WINDOW_HEIGHT)
    {
      result.h = LDK_UI_MIN_WINDOW_HEIGHT;
    }
  }

  return result;
}

static u32 s_ui_window_border_color(
    LDKUIContext *ctx, LDKUIWindow const *window)
{
  if (ctx == NULL || window == NULL)
  {
    return 0xffffffffu;
  }

  if (ctx->focused_window_id == window->id)
  {
    return ctx->theme.colors[LDK_UI_COLOR_FOCUS];
  }

  if (ctx->hovered_window_id == window->id)
  {
    return ctx->theme.colors[LDK_UI_COLOR_CONTROL_BORDER_HOVERED];
  }

  return ctx->theme.colors[LDK_UI_COLOR_BORDER];
}

static void s_ui_window_draw_title_bar(LDKUIContext *ctx, LDKUIWindow *window)
{
  u32 title_bg;
  float line_h;
  float text_x;
  float text_y;
  LDKUIRect text_clip;

  if (ctx == NULL || window == NULL)
  {
    return;
  }

  if ((window->flags & LDK_UI_WINDOW_TITLE_BAR) == 0)
  {
    return;
  }

  title_bg = ctx->focused_window_id == window->id
                 ? ctx->theme.colors[LDK_UI_COLOR_TITLE_BAR_FOCUSED]
                 : ctx->theme.colors[LDK_UI_COLOR_TITLE_BAR];

  line_h = ctx->font != NULL ? ldk_ttf_get_line_height(ctx->font)
                             : LDK_UI_DEFAULT_CONTROL_HEIGHT;

  text_x = window->title_bar_rect.x + 6.0f;
  text_y =
      window->title_bar_rect.y + (window->title_bar_rect.h - line_h) * 0.5f;
  text_clip = window->title_bar_rect;

  if ((window->flags & LDK_UI_WINDOW_CLOSE_BUTTON) != 0)
  {
    text_clip.w = s_ui_maxf(0.0f, text_clip.w - 24.0f);
  }

  s_ui_render_quad(ctx, window->title_bar_rect, title_bg, window->rect, 0);
  s_ui_render_text(ctx, window->title, text_x, text_y,
      ctx->theme.colors[LDK_UI_COLOR_TEXT], text_clip);
}

static bool s_ui_window_draw_close_button(
    LDKUIContext *ctx, LDKUIWindow *window)
{
  LDKUIRect close_rect;
  LDKUIRect saved_last_rect;
  LDKUIRect saved_last_bounding_rect;
  LDKUIId saved_last_id;
  LDKUIId id;
  bool clicked;

  if (ctx == NULL || window == NULL)
  {
    return false;
  }

  if ((window->flags & LDK_UI_WINDOW_CLOSE_BUTTON) == 0)
  {
    return false;
  }

  if ((window->flags & LDK_UI_WINDOW_TITLE_BAR) == 0)
  {
    return false;
  }

  close_rect = s_ui_window_close_button_rect(window);
  id = s_ui_id_hash_u32(window->id, 0x434c4f53u);

  saved_last_rect = ctx->last_rect;
  saved_last_bounding_rect = ctx->last_bounding_rect;
  saved_last_id = ctx->last_id;

  clicked = ldk_ui_widget_button_flat(ctx, id, "x", close_rect);

  ctx->last_rect = saved_last_rect;
  ctx->last_bounding_rect = saved_last_bounding_rect;
  ctx->last_id = saved_last_id;

  return clicked;
}

static void s_ui_window_draw(LDKUIContext *ctx, LDKUIWindow *window)
{
  if (ctx == NULL || window == NULL)
  {
    return;
  }

  if ((window->flags & LDK_UI_WINDOW_NO_BG) == 0)
  {
    s_ui_render_quad(ctx, window->rect,
        ctx->theme.colors[LDK_UI_COLOR_WINDOW_BG], window->rect, 0);
  }

  s_ui_window_draw_title_bar(ctx, window);

  if ((window->flags & LDK_UI_WINDOW_BORDER) != 0)
  {
    s_ui_render_border(ctx, window->rect, ctx->theme.window_border_size,
        s_ui_window_border_color(ctx, window), window->rect);
  }
}

static void s_ui_window_push_content_layout(
    LDKUIContext *ctx, LDKUIWindow *window)
{
  LDKUIId layout_id;

  if (ctx == NULL || window == NULL)
  {
    return;
  }

  layout_id = s_ui_id_hash_u32(window->id, 0x4c41594fu);

  s_ui_layout_push_with_id(ctx, LDK_UI_LAYOUT_VERTICAL, window->content_rect,
      window->content_rect, UINT32_MAX, layout_id);

  if ((window->flags & LDK_UI_WINDOW_NO_PADDING) != 0 && ctx->current_layout != NULL)
  {
    ctx->current_layout->padding = 0.0f;
    ctx->current_layout->content_rect = window->content_rect;
    ctx->current_layout->cursor.x = window->content_rect.x;
    ctx->current_layout->cursor.y = window->content_rect.y;
    ctx->current_layout->content_used_right = window->content_rect.x;
    ctx->current_layout->content_used_bottom = window->content_rect.y;
  }
}

static LDKUIRect s_ui_begin_window_internal(
  LDKUIContext *ctx, char const *title, LDKUIRect rect, u32 flags)
{
  LDKUIId id;
  LDKUIWindow *window;
  LDKPoint cursor = {0};
  bool inside_window = false;
  bool inside_title = false;
  bool inside_close = false;
  bool mouse_down = false;
  bool mouse_pressed = false;
  bool can_drag_from_background = false;
  u32 resize_edges = LDK_UI_WINDOW_RESIZE_NONE;

  if (ctx == NULL)
  {
    return rect;
  }

  s_ui_layout_prepare_frame(ctx);
  s_ui_window_prepare_frame(ctx);

  if ((flags & LDK_UI_WINDOW_RESIZABLE) != 0)
  {
    flags |= LDK_UI_WINDOW_BORDER;
  }

  id = s_ui_window_id_make(ctx, title, ctx->root_item_count);
  window = s_ui_window_get_or_create(ctx, id, title);

  if (window == NULL)
  {
    return rect;
  }

  if (title != NULL)
  {
    strncpy(window->title, title, sizeof(window->title) - 1);
    window->title[sizeof(window->title) - 1] = 0;
  }

  if ((flags & LDK_UI_WINDOW_RESIZABLE) != 0)
  {
    if (rect.w < LDK_UI_MIN_WINDOW_WIDTH)
    {
      rect.w = LDK_UI_MIN_WINDOW_WIDTH;
    }

    if (rect.h < LDK_UI_MIN_WINDOW_HEIGHT)
    {
      rect.h = LDK_UI_MIN_WINDOW_HEIGHT;
    }
  }

  window->flags = flags;
  window->last_frame_seen = ctx->frame_index;
  window->close_requested = false;
  window->rect = rect;
  window->title_bar_rect = s_ui_window_title_bar_rect(rect, flags);
  window->content_rect = s_ui_window_content_rect(ctx, rect, flags);

  if (ctx->mouse != NULL)
  {
    cursor = ldk_os_mouse_cursor((LDKMouseState *)ctx->mouse);
    mouse_down = ldk_os_mouse_button_down(
      (LDKMouseState *)ctx->mouse, LDK_MOUSE_BUTTON_LEFT);
    mouse_pressed = ldk_os_mouse_button_is_pressed(
      (LDKMouseState *)ctx->mouse, LDK_MOUSE_BUTTON_LEFT);
  }

  inside_window =
    ctx->hovered_window_id == window->id &&
    s_ui_rect_contains(&window->rect, (float)cursor.x, (float)cursor.y);

  inside_title = (flags & LDK_UI_WINDOW_TITLE_BAR) != 0 &&
    s_ui_rect_contains(
      &window->title_bar_rect, (float)cursor.x, (float)cursor.y);

  if ((flags & LDK_UI_WINDOW_CLOSE_BUTTON) != 0 &&
      (flags & LDK_UI_WINDOW_TITLE_BAR) != 0)
  {
    LDKUIRect close_rect = s_ui_window_close_button_rect(window);
    inside_close =
      s_ui_rect_contains(&close_rect, (float)cursor.x, (float)cursor.y);
  }

  resize_edges = s_ui_window_resize_edges_at_cursor(ctx, window, cursor);
  s_ui_window_set_resize_cursor(ctx, resize_edges);

  if (inside_window && mouse_down)
  {
    window = s_ui_window_bring_to_front(ctx, window);
  }

  if (inside_window && !inside_close && mouse_down &&
      resize_edges != LDK_UI_WINDOW_RESIZE_NONE)
  {
    ctx->resizing_window_id = window->id;
    ctx->resizing_window_edges = resize_edges;
    ctx->resize_start_cursor_x = (float)cursor.x;
    ctx->resize_start_cursor_y = (float)cursor.y;
    ctx->resize_start_rect = rect;
    ctx->dragging_window_id = 0;
    window = s_ui_window_bring_to_front(ctx, window);
  }

  can_drag_from_background = (flags & LDK_UI_WINDOW_TITLE_BAR) == 0;

  if (ctx->resizing_window_id == 0 && (flags & LDK_UI_WINDOW_DRAGGABLE) != 0 &&
      inside_window && !inside_close && mouse_down &&
      (inside_title || can_drag_from_background))
  {
    ctx->dragging_window_id = window->id;
    ctx->drag_x = (float)cursor.x - rect.x;
    ctx->drag_y = (float)cursor.y - rect.y;
    window = s_ui_window_bring_to_front(ctx, window);
  }

  if (ctx->resizing_window_id == window->id && ctx->mouse != NULL)
  {
    if (mouse_pressed)
    {
      rect = s_ui_window_apply_resize(ctx, rect, cursor);
    }
    else
    {
      ctx->resizing_window_id = 0;
      ctx->resizing_window_edges = LDK_UI_WINDOW_RESIZE_NONE;
    }
  }

  if (ctx->dragging_window_id == window->id && ctx->mouse != NULL)
  {
    if (mouse_pressed)
    {
      rect.x = (float)cursor.x - ctx->drag_x;
      rect.y = (float)cursor.y - ctx->drag_y;
    }
    else
    {
      ctx->dragging_window_id = 0;
    }
  }

  window->rect = rect;
  window->title_bar_rect = s_ui_window_title_bar_rect(rect, flags);
  window->content_rect = s_ui_window_content_rect(ctx, rect, flags);

  {
    LDKUIWindowStackEntry entry = {0};
    entry.window = window;
    entry.previous_window = ctx->current_window;
    entry.previous_clip_rect = ctx->clip_rect;
    entry.layout_stack_count = x_array_ldk_ui_layout_count(ctx->layout_stack);
    x_array_ldk_ui_window_stack_entry_push(ctx->window_stack, entry);
  }

  ctx->current_window = window;
  ctx->root_item_count += 1;
  ctx->last_rect = window->rect;
  ctx->last_bounding_rect = window->rect;
  ctx->last_id = window->id;

  s_ui_window_draw(ctx, window);

  if (s_ui_window_draw_close_button(ctx, window))
  {
    window->close_requested = true;
    ctx->last_window_close_requested = true;
  }

  ctx->clip_rect = s_ui_rect_intersect(&ctx->clip_rect, &window->content_rect);

  s_ui_window_push_content_layout(ctx, window);

  return rect;
}

LDKUIRect ldk_ui_begin_window_fixed(
  LDKUIContext *ctx, char const *title, LDKUIRect rect, u32 flags)
{
  return s_ui_begin_window_internal(ctx, title, rect, flags);
}

LDKUIRect ldk_ui_begin_window(
  LDKUIContext *ctx, char const *title, LDKUIRect rect, u32 flags)
{
  return s_ui_begin_window_internal(ctx, title, rect, flags);
}

bool ldk_ui_begin_window_open(LDKUIContext *ctx, char const *title,
                              LDKUIRect *rect, bool *open, u32 flags)
{
  LDKUIRect new_rect;

  if (ctx == NULL || rect == NULL)
  {
    return false;
  }

  if (open != NULL && !*open)
  {
    return false;
  }

  new_rect = s_ui_begin_window_internal(ctx, title, *rect, flags);
  *rect = new_rect;

  if (ctx->last_window_close_requested)
  {
    if (open != NULL)
    {
      *open = false;
    }

    ldk_ui_end_window(ctx);
    return false;
  }

  return true;
}

bool ldk_ui_window_close_requested(LDKUIContext *ctx)
{
  if (ctx == NULL)
  {
    return false;
  }

  return ctx->last_window_close_requested;
}

void ldk_ui_end_window(LDKUIContext *ctx)
{
  LDKUIWindowStackEntry entry;

  if (ctx == NULL)
  {
    return;
  }

  if (x_array_ldk_ui_window_stack_entry_is_empty(ctx->window_stack))
  {
    return;
  }

  {
    u32 count = x_array_ldk_ui_window_stack_entry_count(ctx->window_stack);
    LDKUIWindowStackEntry *entry_ptr =
      x_array_ldk_ui_window_stack_entry_get(ctx->window_stack, count - 1);

    if (entry_ptr == NULL)
    {
      return;
    }

    entry = *entry_ptr;
    x_array_ldk_ui_window_stack_entry_delete_at(ctx->window_stack, count - 1);
  }

  while (
    x_array_ldk_ui_layout_count(ctx->layout_stack) > entry.layout_stack_count)
  {
    s_ui_layout_pop(ctx);
  }

  ctx->clip_rect = entry.previous_clip_rect;
  ctx->current_window = entry.previous_window;

  if (entry.window != NULL)
  {
    ctx->last_rect = entry.window->rect;
    ctx->last_bounding_rect = entry.window->rect;
    ctx->last_id = entry.window->id;
  }
}
