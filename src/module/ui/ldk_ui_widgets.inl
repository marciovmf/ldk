//------------------------------------------------------------
// Base Widgets
//------------------------------------------------------------

static const float s_ui_resize_handle_hit_size = 8.0f;

void ldk_ui_format_float(char *buffer, u32 buffer_size, float value)
{
  if (!buffer || buffer_size == 0)
  {
    return;
  }
  snprintf(buffer, buffer_size, "%.4f", (double)value);
}

/**
 * Measures the rendered size of a null-terminated string.
 * @arg ctx UI context that owns the font used for measurement.
 * @arg text Null-terminated text to measure.
 * @return Measured text size, or an empty size for an invalid request.
 */

static LDKUISize s_ui_widget_text_size(LDKUIContext *ctx, char const *text)
{
  LDKUISize size = {0};

  if (ctx == NULL || ctx->font == NULL || text == NULL)
  {
    return size;
  }

  ctx->profile_text_measure_count += 1;
  ctx->profile_widget_text_measure_count += 1;
  size = ldk_ttf_measure_text_cstr(ctx->font, text);

  return size;
}

void ldk_ui_widget_panel(LDKUIContext *ctx, LDKUIId id, LDKUIRect rect)
{
  LDKUIWidgetBox box = {0};
  u32 bg;
  u32 border;

  if (!s_ui_widget_box_from_explicit_rect(ctx, &box, id, rect, false))
  {
    return;
  }

  if (s_ui_widget_box_can_skip(ctx, &box, false))
  {
    return;
  }

  bg = ctx->theme.colors[LDK_UI_COLOR_PANEL_BG];
  border = ctx->theme.colors[LDK_UI_COLOR_BORDER];

  s_ui_render_quad(ctx, box.rect, bg, box.clip, 0);
  s_ui_render_border(
      ctx, box.rect, ctx->theme.control_border_size, border, box.clip);
}

static void s_ui_widget_label_with_text_size(LDKUIContext *ctx, LDKUIId id,
    char const *text, LDKUISize text_size, LDKUIRect rect)
{
  LDKUIWidgetBox box = {0};
  float text_y;

  if (!s_ui_widget_box_from_explicit_rect(ctx, &box, id, rect, false))
  {
    return;
  }

  if (s_ui_widget_box_can_skip(ctx, &box, false))
  {
    return;
  }

  text_y = box.rect.y + (box.rect.h - text_size.h) * 0.5f;

  s_ui_render_text_wrapped(ctx, text, box.rect.x, text_y, box.rect.w,
      ctx->theme.colors[LDK_UI_COLOR_TEXT], box.clip);
}

void ldk_ui_widget_label(
    LDKUIContext *ctx, LDKUIId id, char const *text, LDKUIRect rect)
{
  LDKUISize text_size;

  if (text == NULL)
  {
    text = "";
  }

  text_size = s_ui_widget_text_size(ctx, text);
  s_ui_widget_label_with_text_size(ctx, id, text, text_size, rect);
}

void ldk_ui_widget_image(LDKUIContext *ctx, LDKUIId id,
    LDKUITextureHandle texture, LDKUIRect uv, LDKUIRect rect)
{
  LDKUIWidgetBox box = {0};

  if (!s_ui_widget_box_from_explicit_rect(ctx, &box, id, rect, false))
  {
    return;
  }

  if (s_ui_widget_box_can_skip(ctx, &box, false))
  {
    return;
  }

  s_ui_render_quad_uv(ctx, box.rect, uv, 0xffffffffu, box.clip, texture);
}

void ldk_ui_widget_gradient(LDKUIContext *ctx, LDKUIId id,
    rgba32 top_left, rgba32 top_right, rgba32 bottom_right,
    rgba32 bottom_left, LDKUIRect rect)
{
  LDKUIWidgetBox box = {0};
  XArray_ldk_ui_vertex *vertices;
  XArray_ldk_ui_u32 *indices;
  u32 index_offset;
  u32 base_index;

  if (!s_ui_widget_box_from_explicit_rect(ctx, &box, id, rect, false))
  {
    return;
  }

  if (s_ui_widget_box_can_skip(ctx, &box, false))
  {
    return;
  }

  vertices = s_ui_target_vertices(ctx);
  indices = s_ui_target_indices(ctx);
  index_offset = x_array_ldk_ui_u32_count(indices);
  base_index = x_array_ldk_ui_vertex_count(vertices);

  top_left = LDK_RGBA32(top_left);
  top_right = LDK_RGBA32(top_right);
  bottom_right = LDK_RGBA32(bottom_right);
  bottom_left = LDK_RGBA32(bottom_left);

  x_array_ldk_ui_vertex_push(vertices,
      (LDKUIVertex){box.rect.x, box.rect.y, 0.0f, 0.0f, top_left});
  x_array_ldk_ui_vertex_push(vertices,
      (LDKUIVertex){box.rect.x + box.rect.w, box.rect.y, 1.0f, 0.0f,
          top_right});
  x_array_ldk_ui_vertex_push(vertices,
      (LDKUIVertex){box.rect.x + box.rect.w, box.rect.y + box.rect.h, 1.0f,
          1.0f, bottom_right});
  x_array_ldk_ui_vertex_push(vertices,
      (LDKUIVertex){box.rect.x, box.rect.y + box.rect.h, 0.0f, 1.0f,
          bottom_left});

  x_array_ldk_ui_u32_push(indices, base_index + 0u);
  x_array_ldk_ui_u32_push(indices, base_index + 1u);
  x_array_ldk_ui_u32_push(indices, base_index + 2u);
  x_array_ldk_ui_u32_push(indices, base_index + 2u);
  x_array_ldk_ui_u32_push(indices, base_index + 3u);
  x_array_ldk_ui_u32_push(indices, base_index + 0u);

  s_ui_render_add_draw_cmd(ctx, 0, box.clip, index_offset, 6u);
}

void ldk_ui_widget_icon_label(LDKUIContext *ctx, LDKUIId id, LDKUIIcon icon,
    char const *text, LDKUIRect rect)
{
  LDKUIWidgetBox box = {0};
  u32 text_color;

  if (text == NULL)
  {
    text = "";
  }

  if (!s_ui_widget_box_from_explicit_rect(ctx, &box, id, rect, false))
  {
    return;
  }

  if (s_ui_widget_box_can_skip(ctx, &box, false))
  {
    return;
  }

  text_color = box.disabled ? ctx->theme.colors[LDK_UI_COLOR_TEXT_DISABLED]
                            : ctx->theme.colors[LDK_UI_COLOR_TEXT];

  s_ui_render_icon_label(ctx, icon, text, box.rect, text_color, box.clip);
}

bool ldk_ui_widget_button(
    LDKUIContext *ctx, LDKUIId id, char const *text, LDKUIRect rect)
{
  LDKUIWidgetBox box = {0};
  LDKUIFrameState frame;
  LDKUISize text_size;
  u32 bg;
  u32 border;
  u32 text_color;
  float text_x;
  float text_y;

  if (text == NULL)
  {
    text = "";
  }

  if (!s_ui_widget_box_from_explicit_rect(ctx, &box, id, rect, true))
  {
    return false;
  }

  if (s_ui_widget_box_can_skip(ctx, &box, true))
  {
    return false;
  }

  frame = s_ui_frame_state(ctx, box.id, box.rect, box.clip, true, true, box.disabled);
  text_size = s_ui_widget_text_size(ctx, text);

  bg = s_ui_render_control_bg_color(ctx, frame.visual_state);
  border = s_ui_render_control_border_color(ctx, frame.visual_state);
  text_color = s_ui_render_control_text_color(ctx, frame.visual_state);

  s_ui_render_quad(ctx, box.rect, bg, box.clip, 0);
  s_ui_render_border(
      ctx, box.rect, ctx->theme.control_border_size, border, box.clip);

  text_x = box.rect.x + (box.rect.w - text_size.w) * 0.5f;
  text_y = box.rect.y + (box.rect.h - text_size.h) * 0.5f;

  s_ui_render_text(ctx, text, text_x, text_y, text_color, box.clip);

  return frame.clicked;
}

bool ldk_ui_widget_color_view(
  LDKUIContext *ctx, LDKUIId id, rgba32 color, LDKUIRect rect)
{
  LDKUIWidgetBox box = {0};
  LDKUIFrameState frame;
  LDKUISize text_size;
  u32 bg;
  u32 border;

  if (!s_ui_widget_box_from_explicit_rect(ctx, &box, id, rect, true))
  {
    return false;
  }

  if (s_ui_widget_box_can_skip(ctx, &box, true))
  {
    return false;
  }

  frame = s_ui_frame_state(ctx, box.id, box.rect, box.clip, true, true, box.disabled);
  //bg = s_ui_render_control_bg_color(ctx, color);
  border = s_ui_render_control_border_color(ctx, frame.visual_state);

  s_ui_render_quad(ctx, box.rect, color, box.clip, 0);
  s_ui_render_border(
      ctx, box.rect, ctx->theme.control_border_size, border, box.clip);
  return frame.clicked;
}

bool ldk_ui_widget_button_flat(
    LDKUIContext *ctx, LDKUIId id, char const *text, LDKUIRect rect)
{
  LDKUIWidgetBox box = {0};
  LDKUIFrameState frame;
  LDKUISize text_size;
  u32 text_color;
  float text_x;
  float text_y;

  if (text == NULL)
  {
    text = "";
  }

  if (!s_ui_widget_box_from_explicit_rect(ctx, &box, id, rect, true))
  {
    return false;
  }

  if (s_ui_widget_box_can_skip(ctx, &box, true))
  {
    return false;
  }

  frame = s_ui_frame_state(ctx, box.id, box.rect, box.clip, true, true, box.disabled);
  text_size = s_ui_widget_text_size(ctx, text);
  text_color = s_ui_render_control_text_color(ctx, frame.visual_state);

  if (frame.visual_state == LDK_UI_CONTROL_VISUAL_STATE_HOVERED ||
      frame.visual_state == LDK_UI_CONTROL_VISUAL_STATE_ACTIVE ||
      frame.visual_state == LDK_UI_CONTROL_VISUAL_STATE_ACTIVE_HOVERED)
  {
    u32 bg = s_ui_render_control_bg_color(ctx, frame.visual_state);
    s_ui_render_quad(ctx, box.rect, bg, box.clip, 0);
  }

  text_x = box.rect.x + LDK_UI_DEFAULT_SPACING;
  text_y = box.rect.y + (box.rect.h - text_size.h) * 0.5f;

  s_ui_render_text(ctx, text, text_x, text_y, text_color, box.clip);

  return frame.clicked;
}

bool ldk_ui_widget_icon_button(LDKUIContext *ctx, LDKUIId id, LDKUIIcon icon,
    char const *text, LDKUIRect rect)
{
  LDKUIWidgetBox box = {0};

  if (text == NULL)
  {
    text = "";
  }

  if (!s_ui_widget_box_from_explicit_rect(ctx, &box, id, rect, true))
  {
    return false;
  }

  if (s_ui_widget_box_can_skip(ctx, &box, true))
  {
    return false;
  }

  LDKUIFrameState frame =
      s_ui_frame_state(ctx, box.id, box.rect, box.clip, true, true, box.disabled);

  u32 bg = s_ui_render_control_bg_color(ctx, frame.visual_state);
  u32 border = s_ui_render_control_border_color(ctx, frame.visual_state);
  u32 text_color = s_ui_render_control_text_color(ctx, frame.visual_state);

  s_ui_render_quad(ctx, box.rect, bg, box.clip, 0);
  s_ui_render_border(
      ctx, box.rect, ctx->theme.control_border_size, border, box.clip);

  LDKUISize text_size = s_ui_widget_text_size(ctx, text);

  float content_width = text_size.w;

  if (s_ui_icon_valid(icon))
  {
    content_width += icon.size.w;

    if (text[0] != '\0')
    {
      content_width += LDK_UI_DEFAULT_SPACING;
    }
  }

  float content_height = text_size.h;

  if (s_ui_icon_valid(icon))
  {
    content_height = s_ui_maxf(content_height, icon.size.h);
  }

  LDKUIRect content_rect;
  content_rect.x = box.rect.x + (box.rect.w - content_width) * 0.5f;
  content_rect.y = box.rect.y + (box.rect.h - content_height) * 0.5f;
  content_rect.w = content_width;
  content_rect.h = content_height;

  s_ui_render_icon_label_nowrap(
      ctx, icon, text, content_rect, text_color, box.clip);

  return frame.clicked;
}

bool ldk_ui_widget_tab(LDKUIContext *ctx, LDKUIId id, LDKUIIcon icon,
    char const *text, LDKUIRect rect, bool active)
{
  LDKUIWidgetBox box = {0};

  if (text == NULL)
  {
    text = "";
  }

  if (!s_ui_widget_box_from_explicit_rect(ctx, &box, id, rect, true))
  {
    return false;
  }

  if (s_ui_widget_box_can_skip(ctx, &box, true))
  {
    return false;
  }

  LDKUIFrameState frame =
      s_ui_frame_state(ctx, box.id, box.rect, box.clip, true, true, box.disabled);

  u32 bg = active ? ctx->theme.colors[LDK_UI_COLOR_TAB_ACTIVE_BG]
                  : ctx->theme.colors[LDK_UI_COLOR_TAB_BG];
  u32 border = active ? ctx->theme.colors[LDK_UI_COLOR_TAB_ACTIVE_BORDER]
                      : ctx->theme.colors[LDK_UI_COLOR_TAB_BORDER];
  u32 text_color = active ? ctx->theme.colors[LDK_UI_COLOR_TAB_ACTIVE_TEXT]
                          : ctx->theme.colors[LDK_UI_COLOR_TAB_TEXT];

  if (!active && frame.visual_state == LDK_UI_CONTROL_VISUAL_STATE_HOVERED)
  {
    bg = ctx->theme.colors[LDK_UI_COLOR_TAB_BG_HOVERED];
    border = ctx->theme.colors[LDK_UI_COLOR_TAB_BORDER_HOVERED];
    text_color = ctx->theme.colors[LDK_UI_COLOR_TAB_TEXT_HOVERED];
  }

  s_ui_render_quad(ctx, box.rect, bg, box.clip, 0);

  if (!active && ctx->theme.control_border_size > 0.0f)
  {
    LDKUIRect edge_rect = box.rect;
    edge_rect.h = s_ui_minf(ctx->theme.control_border_size, box.rect.h);
    edge_rect.y = box.rect.y + box.rect.h - edge_rect.h;
    s_ui_render_quad(ctx, edge_rect, border, box.clip, 0);
  }

  LDKUIRect content_rect;
  content_rect.x = box.rect.x + LDK_UI_TAB_BAR_TAB_PADDING;
  content_rect.y = box.rect.y;
  content_rect.w =
      s_ui_maxf(0.0f, box.rect.w - LDK_UI_TAB_BAR_TAB_PADDING * 2.0f);
  content_rect.h = box.rect.h;

  s_ui_render_icon_label_nowrap(
      ctx, icon, text, content_rect, text_color, box.clip);

  return frame.clicked;
}

bool ldk_ui_widget_toggle(
    LDKUIContext *ctx, LDKUIId id, bool value, LDKUIRect rect)
{
  LDKUIWidgetBox box = {0};
  LDKUIFrameState frame;
  u32 bg;
  u32 border;
  u32 text_color;
  float check_size;
  LDKUIRect check_rect;

  if (!s_ui_widget_box_from_explicit_rect(ctx, &box, id, rect, true))
  {
    return value;
  }

  if (s_ui_widget_box_can_skip(ctx, &box, true))
  {
    return value;
  }

  frame = s_ui_frame_state(ctx, box.id, box.rect, box.clip, true, true, box.disabled);

  if (frame.clicked)
  {
    value = !value;
  }

  bg = s_ui_render_control_bg_color(ctx, frame.visual_state);
  border = s_ui_render_control_border_color(ctx, frame.visual_state);
  text_color = s_ui_render_control_text_color(ctx, frame.visual_state);

  s_ui_render_quad(ctx, box.rect, bg, box.clip, 0);
  s_ui_render_border(
      ctx, box.rect, ctx->theme.control_border_size, border, box.clip);

  LDKUIIcon toggle_icon =
      s_ui_theme_icon(ctx, value ? LDK_UI_THEME_ICON_TOGGLE_CHECKED
                                 : LDK_UI_THEME_ICON_TOGGLE_UNCHECKED);

  if (s_ui_icon_valid(toggle_icon))
  {
    check_rect.w = s_ui_minf(toggle_icon.size.w, box.rect.w);
    check_rect.h = s_ui_minf(toggle_icon.size.h, box.rect.h);
    check_rect.x = box.rect.x + (box.rect.w - check_rect.w) * 0.5f;
    check_rect.y = box.rect.y + (box.rect.h - check_rect.h) * 0.5f;

    s_ui_render_icon(ctx, toggle_icon, check_rect, text_color, box.clip);
  }
  else if (value)
  {
    check_size = s_ui_maxf(0.0f, s_ui_minf(box.rect.w, box.rect.h) - 8.0f);
    check_rect.x = box.rect.x + (box.rect.w - check_size) * 0.5f;
    check_rect.y = box.rect.y + (box.rect.h - check_size) * 0.5f;
    check_rect.w = check_size;
    check_rect.h = check_size;

    s_ui_render_quad(ctx, check_rect, text_color, box.clip, 0);
  }

  return value;
}

static float s_ui_widget_slider_value(LDKUIContext *ctx,
    LDKUIWidgetBox const *box, LDKUIFrameState const *frame, float value,
    float min_value, float max_value)
{
  float base_height = s_ui_maxf(box->rect.h, 1.0f);
  float thumb_width_factor =
      s_ui_clampf(ctx->theme.slider_thumb_width, 0.0f, 1.0f);
  float thumb_width = s_ui_minf(base_height * thumb_width_factor, box->rect.w);

  if (frame->active)
  {
    value = s_ui_slider_value_from_cursor(
        box->rect, thumb_width, frame->cursor.x, min_value, max_value);
  }

  return s_ui_clampf(
      value, s_ui_minf(min_value, max_value), s_ui_maxf(min_value, max_value));
}

static void s_ui_widget_slider_render(LDKUIContext *ctx,
    LDKUIWidgetBox const *box, LDKUIFrameState const *frame, float value,
    float min_value, float max_value)
{
  float base_height;
  float track_height_factor;
  float thumb_width_factor;
  float track_height;
  float thumb_width;
  float t;
  LDKUIRect track_rect;
  LDKUIRect fill_rect;
  LDKUIRect thumb_rect;
  u32 track_color;
  u32 fill_color;
  u32 thumb_color;
  u32 border_color;

  base_height = s_ui_maxf(box->rect.h, 1.0f);
  track_height_factor = s_ui_clampf(ctx->theme.slider_track_height, 0.0f, 1.0f);
  thumb_width_factor = s_ui_clampf(ctx->theme.slider_thumb_width, 0.0f, 1.0f);

  track_height = s_ui_maxf(1.0f, base_height * track_height_factor);
  thumb_width = s_ui_minf(base_height * thumb_width_factor, box->rect.w);

  t = s_ui_slider_normalize(value, min_value, max_value);

  track_rect = box->rect;
  track_rect.h = track_height;
  track_rect.y = box->rect.y + (box->rect.h - track_height) * 0.5f;

  fill_rect = track_rect;
  fill_rect.w =
      thumb_width * 0.5f + s_ui_maxf(0.0f, track_rect.w - thumb_width) * t;

  thumb_rect.x = box->rect.x + s_ui_maxf(0.0f, box->rect.w - thumb_width) * t;
  thumb_rect.y = box->rect.y;
  thumb_rect.w = thumb_width;
  thumb_rect.h = box->rect.h;

  track_color = s_ui_render_slider_track_color(ctx, frame->visual_state);
  fill_color = ctx->theme.colors[LDK_UI_COLOR_SLIDER_FILL];
  thumb_color = s_ui_render_slider_thumb_color(ctx, frame->visual_state);
  border_color = s_ui_render_control_border_color(ctx, frame->visual_state);

  s_ui_render_quad(ctx, track_rect, track_color, box->clip, 0);
  s_ui_render_quad(ctx, fill_rect, fill_color, box->clip, 0);
  s_ui_render_quad(ctx, thumb_rect, thumb_color, box->clip, 0);
  s_ui_render_border(
      ctx, thumb_rect, ctx->theme.control_border_size, border_color, box->clip);
}

float ldk_ui_widget_slider(LDKUIContext *ctx, LDKUIId id, float value,
    float min_value, float max_value, LDKUIRect rect)
{
  LDKUIWidgetBox box = {0};
  LDKUIFrameState frame;

  if (!s_ui_widget_box_from_explicit_rect(ctx, &box, id, rect, true))
  {
    return value;
  }

  if (s_ui_widget_box_can_skip(ctx, &box, true))
  {
    return value;
  }

  frame = s_ui_frame_state(
      ctx, box.id, box.rect, box.clip, true, false, box.disabled);
  value =
      s_ui_widget_slider_value(ctx, &box, &frame, value, min_value, max_value);
  s_ui_widget_slider_render(ctx, &box, &frame, value, min_value, max_value);

  return value;
}

static bool s_ui_slider_input_parse(char const *text, float *value)
{
  char *end;
  float parsed;

  errno = 0;
  parsed = strtof(text, &end);
  if (end == text || errno == ERANGE || !isfinite(parsed))
  {
    return false;
  }

  while (isspace((unsigned char)*end))
  {
    end += 1;
  }

  if (*end != 0)
  {
    return false;
  }

  *value = parsed;
  return true;
}

float ldk_ui_widget_slider_input(LDKUIContext *ctx, LDKUIId id, float value,
    float min_value, float max_value, LDKUIRect rect)
{
  LDKUIWidgetBox box = {0};
  LDKUIWidgetBox slider_box = {0};
  LDKUIFrameState slider_frame;
  LDKUIRect slider_rect = rect;
  LDKUIRect input_rect = rect;
  LDKUIId input_id;
  char buffer[sizeof(ctx->slider_input.buffer)] = {0};
  float initial_value;
  float spacing;
  float value_tolerance;
  const u32 edit_flags = LDK_UI_INPUT_BOX_CHANGED | LDK_UI_INPUT_BOX_COMMITTED;
  const u32 finish_flags =
      LDK_UI_INPUT_BOX_COMMITTED | LDK_UI_INPUT_BOX_CANCELED;
  u32 result;
  bool editing;
  bool focus_requested;

  if (id == 0 || rect.w <= 0.0f || rect.h <= 0.0f ||
      !s_ui_widget_box_from_explicit_rect(ctx, &box, id, rect, false))
  {
    return value;
  }

  input_id = s_ui_id_hash_u32(id, LDK_UI_ITEM_INPUT_BOX);
  if (input_id == 0)
  {
    input_id = 1;
  }

  const float LDK_UI_SLIDER_INPUT_BOX_WIDTH = 90.0f;
  input_rect.w = s_ui_minf(LDK_UI_SLIDER_INPUT_BOX_WIDTH , rect.w * 0.5f);
  input_rect.x = rect.x + rect.w - input_rect.w;
  spacing = s_ui_minf(LDK_UI_DEFAULT_SPACING, rect.w * 0.05f);
  slider_rect.w = rect.w - input_rect.w - spacing;

  focus_requested = ctx->next_focus;
  ctx->next_focus = false;
  ldk_ui_begin_disabled(ctx, box.disabled);
  s_ui_widget_box_from_explicit_rect(ctx, &slider_box, id, slider_rect, true);
  slider_frame = s_ui_frame_state(ctx, slider_box.id, slider_box.rect,
      slider_box.clip, true, false, slider_box.disabled);
  value = s_ui_widget_slider_value(
      ctx, &slider_box, &slider_frame, value, min_value, max_value);

  value_tolerance = 2.0f * FLT_EPSILON *
                    s_ui_maxf(fabsf(value), fabsf(ctx->slider_input.value));
  editing = ctx->slider_input.id == input_id && ctx->focused_id == input_id &&
            fabsf(ctx->slider_input.value - value) <= value_tolerance;
  initial_value = editing ? ctx->slider_input.initial_value : value;
  if (editing)
  {
    memcpy(buffer, ctx->slider_input.buffer, sizeof(buffer));
  }
  else
  {
    ldk_ui_format_float(buffer, (u32)sizeof(buffer), value);
  }

  ctx->next_focus = focus_requested;
  result = ldk_ui_widget_input_box(
      ctx, input_id, buffer, (u32)sizeof(buffer), input_rect);
  if (ctx->focused_id == input_id)
  {
    float parsed;

    if ((result & LDK_UI_INPUT_BOX_CANCELED) != 0)
    {
      value = initial_value;
    }
    else if ((result & edit_flags) != 0 &&
             s_ui_slider_input_parse(buffer, &parsed))
    {
      value = s_ui_clampf(parsed, s_ui_minf(min_value, max_value),
          s_ui_maxf(min_value, max_value));
    }

    if ((result & finish_flags) != 0)
    {
      ldk_ui_format_float(buffer, (u32)sizeof(buffer), value);
      initial_value = value;
    }

    ctx->slider_input.id = input_id;
    memcpy(ctx->slider_input.buffer, buffer, sizeof(buffer));
    ctx->slider_input.initial_value = initial_value;
    ctx->slider_input.value = value;
  }
  else if (ctx->slider_input.id == input_id)
  {
    ctx->slider_input.id = 0;
  }

  s_ui_widget_slider_render(
      ctx, &slider_box, &slider_frame, value, min_value, max_value);
  ldk_ui_end_disabled(ctx);
  ctx->last_id = id;
  ctx->last_rect = rect;
  ctx->last_bounding_rect = rect;

  return value;
}

/**
 * Handles a scrollbar rendered inside an explicit rectangle.
 * @arg ctx UI context that owns interaction, theme, and rendering state.
 * @arg id Identifier assigned to the scrollbar.
 * @arg scroll Current scroll offset.
 * @arg visible_size Size of the visible content along the scrolling axis.
 * @arg content_size Total content size along the scrolling axis.
 * @arg rect Screen-space rectangle allocated to the scrollbar.
 * @arg horizontal Whether the scrollbar operates on the horizontal axis.
 * @return Updated, clamped scroll offset.
 */
static float s_ui_widget_scrollbar(LDKUIContext *ctx, LDKUIId id, float scroll,
    float visible_size, float content_size, LDKUIRect rect, bool horizontal)
{
  LDKUIWidgetBox box = {0};
  LDKUIFrameState frame;
  LDKUIRect track_rect;
  LDKUIRect thumb_rect;
  float max_scroll;
  float thumb_size;
  float thumb_range;

  if (!s_ui_widget_box_from_explicit_rect(ctx, &box, id, rect, true))
  {
    return scroll;
  }

  if (s_ui_widget_box_can_skip(ctx, &box, true))
  {
    return scroll;
  }

  max_scroll = s_ui_maxf(0.0f, content_size - visible_size);

  if (max_scroll <= 0.0f)
  {
    scroll = 0.0f;
  }
  else
  {
    scroll = s_ui_clampf(scroll, 0.0f, max_scroll);
  }

  s_ui_scrollbar_rects(box.rect, visible_size, content_size, scroll, horizontal,
      &track_rect, &thumb_rect);

  frame = s_ui_frame_state(ctx, box.id, box.rect, box.clip, true, false, box.disabled);

  if (frame.hot && frame.pressed)
  {
    if (horizontal)
    {
      if (s_ui_rect_contains(&thumb_rect, frame.cursor.x, frame.cursor.y))
      {
        ctx->scrollbar_drag_offset_x = frame.cursor.x - thumb_rect.x;
      }
      else
      {
        ctx->scrollbar_drag_offset_x = thumb_rect.w * 0.5f;
      }
    }
    else
    {
      if (s_ui_rect_contains(&thumb_rect, frame.cursor.x, frame.cursor.y))
      {
        ctx->scrollbar_drag_offset_y = frame.cursor.y - thumb_rect.y;
      }
      else
      {
        ctx->scrollbar_drag_offset_y = thumb_rect.h * 0.5f;
      }
    }
  }

  if (frame.active && max_scroll > 0.0f)
  {
    if (horizontal)
    {
      thumb_size = thumb_rect.w;
      thumb_range = s_ui_maxf(1.0f, track_rect.w - thumb_size);
      scroll = ((frame.cursor.x - track_rect.x - ctx->scrollbar_drag_offset_x) /
                   thumb_range) *
               max_scroll;
    }
    else
    {
      thumb_size = thumb_rect.h;
      thumb_range = s_ui_maxf(1.0f, track_rect.h - thumb_size);
      scroll = ((frame.cursor.y - track_rect.y - ctx->scrollbar_drag_offset_y) /
                   thumb_range) *
               max_scroll;
    }

    scroll = s_ui_clampf(scroll, 0.0f, max_scroll);
    s_ui_scrollbar_rects(box.rect, visible_size, content_size, scroll,
        horizontal, &track_rect, &thumb_rect);
  }

  s_ui_render_quad(ctx, track_rect,
      ctx->theme.colors[LDK_UI_COLOR_SCROLLBAR_TRACK], box.clip, 0);
  u32 thumb_color = ctx->theme.colors[LDK_UI_COLOR_SCROLLBAR_THUMB];
  if (frame.active)
  {
    thumb_color = ctx->theme.colors[LDK_UI_COLOR_SCROLLBAR_THUMB_ACTIVE];
  }
  else if (frame.hot)
  {
    thumb_color = ctx->theme.colors[LDK_UI_COLOR_SCROLLBAR_THUMB_HOVERED];
  }
  s_ui_render_quad(ctx, thumb_rect, thumb_color, box.clip, 0);

  return scroll;
}

float ldk_ui_widget_scrollbar_vertical(LDKUIContext *ctx, LDKUIId id,
    float scroll, float visible_size, float content_size, LDKUIRect rect)
{
  return s_ui_widget_scrollbar(
      ctx, id, scroll, visible_size, content_size, rect, false);
}

float ldk_ui_widget_scrollbar_horizontal(LDKUIContext *ctx, LDKUIId id,
    float scroll, float visible_size, float content_size, LDKUIRect rect)
{
  return s_ui_widget_scrollbar(
      ctx, id, scroll, visible_size, content_size, rect, true);
}

/**
 * Selects the background color used by input boxes.
 * @arg ctx UI context that owns the active theme.
 * @arg state Current visual state of the input box.
 * @return Dedicated input background, or the legacy control background when
 *         input-specific styling is not enabled by the theme.
 */
static u32 s_ui_render_input_bg_color(
    LDKUIContext *ctx, LDKUIControlVisualState state)
{
  if (ctx->theme.input_border_size <= 0.0f)
  {
    return s_ui_render_control_bg_color(ctx, state);
  }

  if (state == LDK_UI_CONTROL_VISUAL_STATE_ACTIVE_HOVERED)
  {
    return ctx->theme.colors[LDK_UI_COLOR_INPUT_BG_ACTIVE_HOVERED];
  }

  if (state == LDK_UI_CONTROL_VISUAL_STATE_ACTIVE)
  {
    return ctx->theme.colors[LDK_UI_COLOR_INPUT_BG_ACTIVE];
  }

  if (state == LDK_UI_CONTROL_VISUAL_STATE_HOVERED)
  {
    return ctx->theme.colors[LDK_UI_COLOR_INPUT_BG_HOVERED];
  }

  return ctx->theme.colors[LDK_UI_COLOR_INPUT_BG];
}

static u32 s_ui_render_input_border_color(LDKUIContext *ctx,
    LDKUIControlVisualState state, bool focused)
{
  if (state == LDK_UI_CONTROL_VISUAL_STATE_DISABLED)
  {
    return ctx->theme.colors[LDK_UI_COLOR_CONTROL_BORDER_DISABLED];
  }
  if (focused)
  {
    return ctx->theme.colors[LDK_UI_COLOR_CONTROL_BORDER_ACTIVE];
  }
  if (state == LDK_UI_CONTROL_VISUAL_STATE_HOVERED ||
      state == LDK_UI_CONTROL_VISUAL_STATE_ACTIVE_HOVERED)
  {
    return ctx->theme.colors[LDK_UI_COLOR_CONTROL_BORDER_HOVERED];
  }
  return ctx->theme.colors[LDK_UI_COLOR_INPUT_BORDER];
}

typedef enum LDKUIInputVisualMode
{
  LDK_UI_INPUT_VISUAL_BOX = 0,
  LDK_UI_INPUT_VISUAL_LABEL = 1,
} LDKUIInputVisualMode;


/*
 * Text inputs and read-only selectable text intentionally share the same
 * selection owner/cursor/range stored in LDKUIContext.  The input widget adds
 * editing on top; selectable text only uses the interaction/navigation part.
 */
static bool s_ui_text_selection_owned_by(LDKUIContext *ctx, LDKUIId id)
{
  return ctx != NULL && ctx->input_box_id == id;
}

static void s_ui_text_selection_begin(
    LDKUIContext *ctx, LDKUIId id, u32 cursor)
{
  if (ctx == NULL)
  {
    return;
  }

  ctx->input_box_id = id;
  ctx->text_cursor = cursor;
  ctx->text_select_start = cursor;
  ctx->text_select_end = cursor;
}

static void s_ui_text_selection_extend(
    LDKUIContext *ctx, LDKUIId id, u32 cursor)
{
  if (!s_ui_text_selection_owned_by(ctx, id))
  {
    return;
  }

  ctx->text_cursor = cursor;
  ctx->text_select_end = cursor;
}

static void s_ui_text_selection_clamp(
    LDKUIContext *ctx, LDKUIId id, u32 text_length)
{
  if (!s_ui_text_selection_owned_by(ctx, id))
  {
    return;
  }

  if (ctx->text_cursor > text_length)
  {
    ctx->text_cursor = text_length;
  }

  if (ctx->text_select_start > text_length)
  {
    ctx->text_select_start = text_length;
  }

  if (ctx->text_select_end > text_length)
  {
    ctx->text_select_end = text_length;
  }
}

static bool s_ui_text_selection_copy_requested(LDKUIContext *ctx)
{
  if (ctx == NULL || ctx->keyboard == NULL)
  {
    return false;
  }

  return ldk_os_keyboard_key_down(
             (LDKKeyboardState *)ctx->keyboard, LDK_KEYCODE_C) &&
         ldk_os_keyboard_key_is_pressed(
             (LDKKeyboardState *)ctx->keyboard, LDK_KEYCODE_CONTROL);
}

static void s_ui_text_selection_copy(LDKUIContext *ctx, LDKUIId id,
    char const *text, u32 text_length)
{
  u32 start;
  u32 end;
  u32 length;
  char *copy;

  if (!s_ui_text_selection_owned_by(ctx, id) || text == NULL ||
      ctx->clipboard_window == NULL ||
      ctx->text_select_start == ctx->text_select_end ||
      !s_ui_text_selection_copy_requested(ctx))
  {
    return;
  }

  start = ctx->text_select_start;
  end = ctx->text_select_end;

  if (start > end)
  {
    u32 temp = start;
    start = end;
    end = temp;
  }

  if (start > text_length)
  {
    start = text_length;
  }

  if (end > text_length)
  {
    end = text_length;
  }

  if (start >= end)
  {
    return;
  }

  length = end - start;
  copy = (char *)x_arena_alloc(ctx->frame_arena, (size_t)length + 1u);
  if (copy == NULL)
  {
    return;
  }

  memcpy(copy, text + start, length);
  copy[length] = 0;
  ldk_os_clipboard_text_set(ctx->clipboard_window, copy);
}

static void s_ui_text_selection_keyboard_update(LDKUIContext *ctx,
    LDKUIId id, char const *text, u32 text_length)
{
  bool shift;
  bool move_left;
  bool move_right;
  bool move_home;
  bool move_end;

  if (!s_ui_text_selection_owned_by(ctx, id) || text == NULL)
  {
    return;
  }

  shift = s_ui_input_keyboard_shift_pressed(ctx);
  move_left = s_ui_input_keyboard_left_pressed(ctx);
  move_right = s_ui_input_keyboard_right_pressed(ctx);
  move_home = s_ui_input_keyboard_home_pressed(ctx);
  move_end = s_ui_input_keyboard_end_pressed(ctx);

  if (move_left || move_right || move_home || move_end)
  {
    bool has_selection = ctx->text_select_start != ctx->text_select_end;
    u32 selection_start = ctx->text_select_start;
    u32 selection_end = ctx->text_select_end;

    if (selection_start > selection_end)
    {
      u32 temp = selection_start;
      selection_start = selection_end;
      selection_end = temp;
    }

    if (move_left)
    {
      if (has_selection && !shift)
      {
        ctx->text_cursor = selection_start;
      }
      else
      {
        ctx->text_cursor =
            s_ui_input_text_cursor_prev(text, ctx->text_cursor);
      }
    }
    else if (move_right)
    {
      if (has_selection && !shift)
      {
        ctx->text_cursor = selection_end;
      }
      else
      {
        ctx->text_cursor =
            s_ui_input_text_cursor_next(text, ctx->text_cursor);
      }
    }
    else if (move_home)
    {
      ctx->text_cursor = 0;
    }
    else if (move_end)
    {
      ctx->text_cursor = text_length;
    }

    if (shift)
    {
      ctx->text_select_end = ctx->text_cursor;
    }
    else
    {
      ctx->text_select_start = ctx->text_cursor;
      ctx->text_select_end = ctx->text_cursor;
    }
  }

  if (s_ui_input_keyboard_ctrla_pressed(ctx))
  {
    ctx->text_cursor = text_length;
    ctx->text_select_start = 0;
    ctx->text_select_end = text_length;
  }

  s_ui_text_selection_copy(ctx, id, text, text_length);
}

static u32 s_ui_text_cursor_from_x_range(LDKUIContext *ctx,
    char const *text, char const *line_start, char const *line_end, float x)
{
  u32 start;
  u32 end;
  u32 cursor;
  u32 best;
  float best_distance = 1000000000.0f;

  if (ctx == NULL || ctx->font == NULL || text == NULL ||
      line_start == NULL || line_end == NULL || line_start > line_end)
  {
    return 0;
  }

  start = (u32)(line_start - text);
  end = (u32)(line_end - text);
  cursor = start;
  best = start;

  if (x <= 0.0f)
  {
    return start;
  }

  while (cursor <= end)
  {
    LDKTextSize size =
        ldk_ttf_measure_text_cstrn(ctx->font, line_start, cursor - start);
    float distance = fabsf(size.w - x);

    if (distance < best_distance)
    {
      best_distance = distance;
      best = cursor;
    }

    if (cursor == end)
    {
      break;
    }

    u32 next = x_utf8_next(text, cursor);
    if (next <= cursor || next > end)
    {
      cursor = end;
    }
    else
    {
      cursor = next;
    }
  }

  return best;
}

static u32 s_ui_text_wrapped_cursor_from_point(LDKUIContext *ctx,
    char const *text, LDKUIRect rect, LDKUIPoint point)
{
  u32 len;
  char const *cursor;
  char const *text_end;
  float line_height;
  float line_y;

  if (ctx == NULL || ctx->font == NULL || text == NULL)
  {
    return 0;
  }

  len = s_ui_text_cstr_len_u32(text);
  if (len == 0)
  {
    return 0;
  }

  if (point.y < rect.y)
  {
    return 0;
  }

  if (point.y >= rect.y + rect.h)
  {
    return len;
  }

  cursor = text;
  text_end = text + len;
  line_height = ldk_ttf_get_line_height(ctx->font);
  line_y = rect.y;

  while (cursor < text_end && *cursor != '\0')
  {
    char const *line_start = NULL;
    char const *line_end = NULL;
    char const *next = NULL;

    if (!ldk_text_wrapped_next_line(ctx->font, cursor, text_end, rect.w,
            &line_start, &line_end, &next, NULL))
    {
      break;
    }

    if (point.y < line_y + line_height)
    {
      return s_ui_text_cursor_from_x_range(
          ctx, text, line_start, line_end, point.x - rect.x);
    }

    line_y += line_height;

    if (next == NULL || next <= cursor)
    {
      break;
    }

    cursor = next;
  }

  return len;
}

static bool s_ui_text_keyboard_up_pressed(LDKUIContext *ctx)
{
  return ctx != NULL && ctx->keyboard != NULL &&
         ldk_os_keyboard_key_down(
             (LDKKeyboardState *)ctx->keyboard, LDK_KEYCODE_UP);
}

static bool s_ui_text_keyboard_down_pressed(LDKUIContext *ctx)
{
  return ctx != NULL && ctx->keyboard != NULL &&
         ldk_os_keyboard_key_down(
             (LDKKeyboardState *)ctx->keyboard, LDK_KEYCODE_DOWN);
}

static u32 s_ui_text_wrapped_cursor_vertical_move(LDKUIContext *ctx,
    char const *text, LDKUIRect rect, u32 text_cursor, i32 direction)
{
  u32 text_length;
  char const *cursor;
  char const *text_end;
  char const *previous_start = NULL;
  char const *previous_end = NULL;

  if (ctx == NULL || ctx->font == NULL || text == NULL || direction == 0)
  {
    return text_cursor;
  }

  text_length = s_ui_text_cstr_len_u32(text);
  if (text_cursor > text_length)
  {
    text_cursor = text_length;
  }

  cursor = text;
  text_end = text + text_length;

  while (cursor < text_end && *cursor != '\0')
  {
    char const *line_start = NULL;
    char const *line_end = NULL;
    char const *next = NULL;
    u32 line_start_offset;
    u32 line_end_offset;
    u32 next_offset;

    if (!ldk_text_wrapped_next_line(ctx->font, cursor, text_end, rect.w,
            &line_start, &line_end, &next, NULL))
    {
      break;
    }

    line_start_offset = (u32)(line_start - text);
    line_end_offset = (u32)(line_end - text);
    next_offset = next != NULL ? (u32)(next - text) : line_end_offset;

    if (text_cursor >= line_start_offset &&
        (text_cursor <= line_end_offset || text_cursor < next_offset))
    {
      u32 cursor_in_line = text_cursor;
      float x;

      if (cursor_in_line < line_start_offset)
      {
        cursor_in_line = line_start_offset;
      }
      if (cursor_in_line > line_end_offset)
      {
        cursor_in_line = line_end_offset;
      }

      x = ldk_ttf_measure_text_cstrn(
              ctx->font, line_start, cursor_in_line - line_start_offset)
              .w;

      if (direction < 0)
      {
        if (previous_start == NULL || previous_end == NULL)
        {
          return text_cursor;
        }

        return s_ui_text_cursor_from_x_range(
            ctx, text, previous_start, previous_end, x);
      }

      if (next != NULL && next < text_end && *next != '\0')
      {
        char const *next_start = NULL;
        char const *next_end = NULL;
        char const *after_next = NULL;

        if (ldk_text_wrapped_next_line(ctx->font, next, text_end, rect.w,
                &next_start, &next_end, &after_next, NULL))
        {
          return s_ui_text_cursor_from_x_range(
              ctx, text, next_start, next_end, x);
        }
      }

      return text_cursor;
    }

    previous_start = line_start;
    previous_end = line_end;

    if (next == NULL || next <= cursor)
    {
      break;
    }

    cursor = next;
  }

  return text_cursor;
}

static void s_ui_text_selection_keyboard_vertical_update(LDKUIContext *ctx,
    LDKUIId id, char const *text, LDKUIRect rect)
{
  bool move_up;
  bool move_down;
  bool shift;
  u32 cursor;

  if (!s_ui_text_selection_owned_by(ctx, id) || text == NULL)
  {
    return;
  }

  move_up = s_ui_text_keyboard_up_pressed(ctx);
  move_down = s_ui_text_keyboard_down_pressed(ctx);
  if (!move_up && !move_down)
  {
    return;
  }

  shift = s_ui_input_keyboard_shift_pressed(ctx);
  cursor = s_ui_text_wrapped_cursor_vertical_move(
      ctx, text, rect, ctx->text_cursor, move_up ? -1 : 1);

  ctx->text_cursor = cursor;
  if (shift)
  {
    ctx->text_select_end = cursor;
  }
  else
  {
    ctx->text_select_start = cursor;
    ctx->text_select_end = cursor;
  }
}

static void s_ui_render_text_highlight_wrapped(LDKUIContext *ctx,
    char const *text, u32 start, u32 end, float x, float y, float max_width,
    LDKUIRect clip, u32 color)
{
  u32 text_length;
  char const *cursor;
  char const *text_end;
  float line_height;
  float line_y;

  if (ctx == NULL || ctx->font == NULL || text == NULL || start == end)
  {
    return;
  }

  if (start > end)
  {
    u32 temp = start;
    start = end;
    end = temp;
  }

  text_length = s_ui_text_cstr_len_u32(text);
  if (start > text_length)
  {
    start = text_length;
  }
  if (end > text_length)
  {
    end = text_length;
  }

  cursor = text;
  text_end = text + text_length;
  line_height = ldk_ttf_get_line_height(ctx->font);
  line_y = y;

  while (cursor < text_end && *cursor != '\0')
  {
    char const *line_start = NULL;
    char const *line_end = NULL;
    char const *next = NULL;
    u32 line_start_offset;
    u32 line_end_offset;

    if (!ldk_text_wrapped_next_line(ctx->font, cursor, text_end, max_width,
            &line_start, &line_end, &next, NULL))
    {
      break;
    }

    line_start_offset = (u32)(line_start - text);
    line_end_offset = (u32)(line_end - text);

    if (end > line_start_offset && start < line_end_offset)
    {
      u32 local_start = start > line_start_offset ? start : line_start_offset;
      u32 local_end = end < line_end_offset ? end : line_end_offset;

      if (local_end > local_start)
      {
        LDKTextSize before = ldk_ttf_measure_text_cstrn(
            ctx->font, line_start, local_start - line_start_offset);
        LDKTextSize selected = ldk_ttf_measure_text_cstrn(
            ctx->font, line_start, local_end - line_start_offset);
        LDKUIRect highlight_rect = {x + before.w, line_y + 2.0f,
            selected.w - before.w, s_ui_maxf(1.0f, line_height - 4.0f)};

        s_ui_render_quad(ctx, highlight_rect, color, clip, 0);
      }
    }

    line_y += line_height;

    if (next == NULL || next <= cursor)
    {
      break;
    }

    cursor = next;
  }
}

/**
 * Handles editing, interaction, and rendering for a single-line text input.
 * @arg ctx UI context that owns input, focus, theme, and rendering state.
 * @arg id Identifier assigned to the input widget.
 * @arg buffer Mutable null-terminated text buffer edited by the widget.
 * @arg buffer_size Total capacity of the text buffer in bytes.
 * @arg rect Screen-space rectangle occupied by the input widget.
 * @arg visual_mode Visual presentation used to render the input widget.
 * @return Bitwise combination of LDKUIInputBoxResult values.
 */
static u32 s_ui_widget_input(LDKUIContext *ctx, LDKUIId id, char *buffer,
    u32 buffer_size, LDKUIRect rect, LDKUIInputVisualMode visual_mode)
{
  u32 result = LDK_UI_INPUT_BOX_NONE;
  LDKUIWidgetBox box = {0};
  LDKUIFrameState frame;
  LDKUIRect parent_clip;
  u32 buffer_len;
  LDKUISize text_size;
  u32 bg;
  u32 border;
  u32 text_color;
  float border_size;
  float text_x;
  float text_y;
  u32 previous_text_cursor;
  u32 previous_text_select_start;
  u32 previous_text_select_end;

  if (buffer == NULL || buffer_size == 0)
  {
    return result;
  }

  buffer[buffer_size - 1] = 0;

  parent_clip = s_ui_current_clip_rect(ctx);

  if (!s_ui_widget_box_from_explicit_rect(ctx, &box, id, rect, true))
  {
    return result;
  }

  if (s_ui_widget_box_can_skip(ctx, &box, true))
  {
    return result;
  }

  buffer_len = s_ui_text_cstr_len_u32(buffer);
  frame = s_ui_frame_state(ctx, box.id, box.rect, box.clip, true, false, box.disabled);

  previous_text_cursor = ctx->text_cursor;
  previous_text_select_start = ctx->text_select_start;
  previous_text_select_end = ctx->text_select_end;

  if (frame.pressed && frame.hot)
  {
    float pressed_text_x = s_ui_input_box_text_x(ctx, buffer, ctx->text_cursor,
        box.rect, frame.focused && ctx->input_box_id == box.id);
    u32 cursor = s_ui_input_box_cursor_from_x(
        ctx, buffer, pressed_text_x, frame.cursor.x);
    s_ui_text_selection_begin(ctx, box.id, cursor);
    s_ui_input_cursor_blink_reset(ctx);
  }

  if (frame.focused && ctx->input_box_id != box.id)
  {
    s_ui_text_selection_begin(ctx, box.id, buffer_len);
    s_ui_input_cursor_blink_reset(ctx);
  }

  if (frame.active && frame.dragging &&
      s_ui_text_selection_owned_by(ctx, box.id))
  {
    float drag_text_x = s_ui_input_box_text_x(
        ctx, buffer, ctx->text_cursor, box.rect, true);
    u32 cursor = s_ui_input_box_cursor_from_x(
        ctx, buffer, drag_text_x, frame.cursor.x);
    s_ui_text_selection_extend(ctx, box.id, cursor);
  }

  if (frame.focused)
  {
    s_ui_text_selection_clamp(ctx, box.id, buffer_len);
    s_ui_text_selection_keyboard_update(ctx, box.id, buffer, buffer_len);

    if (s_ui_input_keyboard_delete_pressed(ctx))
    {
      if (ctx->text_select_start != ctx->text_select_end)
      {
        u32 start = ctx->text_select_start;
        u32 end = ctx->text_select_end;

        if (start > end)
        {
          u32 temp = start;
          start = end;
          end = temp;
        }

        if (s_ui_text_delete_range(buffer, buffer_len, start, end))
        {
          ctx->text_cursor = start;
          ctx->text_select_start = start;
          ctx->text_select_end = start;
          result |= LDK_UI_INPUT_BOX_CHANGED;
        }
      }
      else if (ctx->text_cursor < buffer_len)
      {
        u32 end = s_ui_input_text_cursor_next(buffer, ctx->text_cursor);

        if (s_ui_text_delete_range(buffer, buffer_len, ctx->text_cursor, end))
        {
          ctx->text_select_start = ctx->text_cursor;
          ctx->text_select_end = ctx->text_cursor;
          result |= LDK_UI_INPUT_BOX_CHANGED;
        }
      }

      buffer_len = s_ui_text_cstr_len_u32(buffer);
    }

    if (s_ui_input_keyboard_backspace_pressed(ctx))
    {
      if (ctx->text_select_start != ctx->text_select_end)
      {
        u32 start = ctx->text_select_start;
        u32 end = ctx->text_select_end;

        if (start > end)
        {
          u32 temp = start;
          start = end;
          end = temp;
        }

        if (s_ui_text_delete_range(buffer, buffer_len, start, end))
        {
          ctx->text_cursor = start;
          ctx->text_select_start = start;
          ctx->text_select_end = start;
          result |= LDK_UI_INPUT_BOX_CHANGED;
        }
      }
      else if (ctx->text_cursor > 0)
      {
        u32 start = s_ui_input_text_cursor_prev(buffer, ctx->text_cursor);

        if (s_ui_text_delete_range(buffer, buffer_len, start, ctx->text_cursor))
        {
          ctx->text_cursor = start;
          ctx->text_select_start = start;
          ctx->text_select_end = start;
          result |= LDK_UI_INPUT_BOX_CHANGED;
        }
      }

      buffer_len = s_ui_text_cstr_len_u32(buffer);
    }

    if (ctx->input_text != NULL)
    {
      for (u32 i = 0; i < ctx->input_text->codepoint_count; ++i)
      {
        char encoded[4] = {0};
        u32 encoded_len = 0;

        if (!x_utf8_encode(
                ctx->input_text->codepoints[i], encoded, &encoded_len))
        {
          continue;
        }

        if (ctx->text_select_start != ctx->text_select_end)
        {
          u32 start = ctx->text_select_start;
          u32 end = ctx->text_select_end;

          if (start > end)
          {
            u32 temp = start;
            start = end;
            end = temp;
          }

          if (s_ui_text_delete_range(buffer, buffer_len, start, end))
          {
            ctx->text_cursor = start;
            ctx->text_select_start = start;
            ctx->text_select_end = start;
            buffer_len = s_ui_text_cstr_len_u32(buffer);
          }
        }

        if (s_ui_text_insert_bytes(
                buffer, buffer_size, &ctx->text_cursor, encoded, encoded_len))
        {
          ctx->text_select_start = ctx->text_cursor;
          ctx->text_select_end = ctx->text_cursor;
          buffer_len = s_ui_text_cstr_len_u32(buffer);
          result |= LDK_UI_INPUT_BOX_CHANGED;
        }
      }
    }

    if (s_ui_input_keyboard_enter_pressed(ctx))
    {
      result |= LDK_UI_INPUT_BOX_COMMITTED;
    }

    if (ctx->keyboard != NULL &&
        ldk_os_keyboard_key_down(
            (LDKKeyboardState *)ctx->keyboard, LDK_KEYCODE_ESCAPE))
    {
      result |= LDK_UI_INPUT_BOX_CANCELED;
    }
  }

  if (frame.active)
  {
    ctx->cursor_type = LDK_CURSOR_TEXT_SELECT;
  }

  if (ctx->text_cursor != previous_text_cursor ||
      ctx->text_select_start != previous_text_select_start ||
      ctx->text_select_end != previous_text_select_end ||
      (result & LDK_UI_INPUT_BOX_CHANGED) != 0)
  {
    s_ui_input_cursor_blink_reset(ctx);
  }

  ctx->profile_input_text_measure_count += 1;
  text_size = s_ui_widget_text_size(ctx, buffer);

  bg = s_ui_render_input_bg_color(ctx, frame.visual_state);
  border = s_ui_render_input_border_color(
      ctx, frame.visual_state, frame.focused);
  text_color = s_ui_render_control_text_color(ctx, frame.visual_state);
  border_size = ctx->theme.input_border_size > 0.0f
                    ? ctx->theme.input_border_size
                    : ctx->theme.control_border_size;

  if (visual_mode == LDK_UI_INPUT_VISUAL_BOX)
  {
    s_ui_render_quad(ctx, box.rect, bg, parent_clip, 0);
    s_ui_render_border(ctx, box.rect, border_size, border, parent_clip);
  }

  text_x = s_ui_input_box_text_x(
      ctx, buffer, ctx->text_cursor, box.rect, frame.focused);
  text_y = box.rect.y + (box.rect.h - text_size.h) * 0.5f;

  if (frame.focused && ctx->text_select_start != ctx->text_select_end)
  {
    s_ui_render_text_highlight(ctx, buffer, ctx->text_select_start,
        ctx->text_select_end, text_x, box.rect, box.clip,
        ctx->theme.colors[LDK_UI_COLOR_FOCUS]);
  }

  s_ui_render_text(ctx, buffer, text_x, text_y, text_color, box.clip);

  if (frame.focused &&
      (!ctx->theme.text_cursor_blink || ctx->text_cursor_blink_visible))
  {
    LDKTextSize cursor_text_size =
        ldk_ttf_measure_text_cstrn(ctx->font, buffer, ctx->text_cursor);
    float cursor_x = text_x + cursor_text_size.w;
    float cursor_width = ctx->theme.text_cursor_width;
    float cursor_padding_y = ctx->theme.text_cursor_padding_y;

    if (cursor_width <= 0.0f)
    {
      cursor_width = 1.0f;
    }

    if (cursor_padding_y < 0.0f)
    {
      cursor_padding_y = 0.0f;
    }

    if (cursor_padding_y * 2.0f > box.rect.h)
    {
      cursor_padding_y = box.rect.h * 0.5f;
    }

    LDKUIRect cursor_rect = {cursor_x, box.rect.y + cursor_padding_y,
        cursor_width, box.rect.h - cursor_padding_y * 2.0f};
    s_ui_render_quad(
        ctx, cursor_rect, ctx->theme.colors[LDK_UI_COLOR_TEXT], box.clip, 0);
  }

  return result;
}

u32 ldk_ui_widget_input_box(LDKUIContext *ctx, LDKUIId id, char *buffer,
    u32 buffer_size, LDKUIRect rect)
{
  return s_ui_widget_input(
      ctx, id, buffer, buffer_size, rect, LDK_UI_INPUT_VISUAL_BOX);
}

u32 ldk_ui_widget_input_label(LDKUIContext *ctx, LDKUIId id, char *buffer,
    u32 buffer_size, LDKUIRect rect)
{
  return s_ui_widget_input(
      ctx, id, buffer, buffer_size, rect, LDK_UI_INPUT_VISUAL_LABEL);
}


static void s_ui_widget_selectable_text(LDKUIContext *ctx, LDKUIId id,
    char const *text, LDKUIRect rect)
{
  LDKUIWidgetBox box = {0};
  LDKUIFrameState frame;
  u32 text_length;

  if (text == NULL)
  {
    text = "";
  }

  if (!s_ui_widget_box_from_explicit_rect(ctx, &box, id, rect, true))
  {
    return;
  }

  if (s_ui_widget_box_can_skip(ctx, &box, true))
  {
    return;
  }

  text_length = s_ui_text_cstr_len_u32(text);
  frame = s_ui_frame_state(ctx, box.id, box.rect, box.clip, true, false, box.disabled);

  if (frame.pressed && frame.hot)
  {
    u32 cursor =
        s_ui_text_wrapped_cursor_from_point(ctx, text, box.rect, frame.cursor);
    s_ui_text_selection_begin(ctx, box.id, cursor);
  }

  if (frame.focused && !s_ui_text_selection_owned_by(ctx, box.id))
  {
    s_ui_text_selection_begin(ctx, box.id, 0);
  }

  if (frame.active && frame.dragging &&
      s_ui_text_selection_owned_by(ctx, box.id))
  {
    u32 cursor =
        s_ui_text_wrapped_cursor_from_point(ctx, text, box.rect, frame.cursor);
    s_ui_text_selection_extend(ctx, box.id, cursor);
  }

  if (frame.focused && s_ui_text_selection_owned_by(ctx, box.id))
  {
    s_ui_text_selection_clamp(ctx, box.id, text_length);
    s_ui_text_selection_keyboard_update(ctx, box.id, text, text_length);
    s_ui_text_selection_keyboard_vertical_update(
        ctx, box.id, text, box.rect);
  }

  if (frame.hot || frame.active)
  {
    ctx->cursor_type = LDK_CURSOR_TEXT_SELECT;
  }

  if (frame.focused && s_ui_text_selection_owned_by(ctx, box.id) &&
      ctx->text_select_start != ctx->text_select_end)
  {
    s_ui_render_text_highlight_wrapped(ctx, text, ctx->text_select_start,
        ctx->text_select_end, box.rect.x, box.rect.y, box.rect.w, box.clip,
        ctx->theme.colors[LDK_UI_COLOR_FOCUS]);
  }

  s_ui_render_text_wrapped(ctx, text, box.rect.x, box.rect.y, box.rect.w,
      ctx->theme.colors[LDK_UI_COLOR_TEXT], box.clip);
}

static float s_ui_widget_resize_handle(LDKUIContext *ctx, LDKUIId id,
    float value, float min_value, float max_value, LDKUIRect rect,
    bool vertical)
{
  LDKUIWidgetBox box = {0};
  LDKUIFrameState frame;
  LDKUIRect hit_rect = rect;
  LDKUIRect line_rect = rect;
  float center;
  float cursor;
  u32 color;

  if (ctx == NULL || id == 0)
  {
    return value;
  }

  if (max_value < min_value)
  {
    max_value = min_value;
  }
  value = s_ui_clampf(value, min_value, max_value);

  if (vertical)
  {
    center = rect.x + rect.w * 0.5f;
    hit_rect.x = center - s_ui_resize_handle_hit_size * 0.5f;
    hit_rect.w = s_ui_resize_handle_hit_size;
    line_rect.x = center - 0.5f;
    line_rect.w = 1.0f;
  }
  else
  {
    center = rect.y + rect.h * 0.5f;
    hit_rect.y = center - s_ui_resize_handle_hit_size * 0.5f;
    hit_rect.h = s_ui_resize_handle_hit_size;
    line_rect.y = center - 0.5f;
    line_rect.h = 1.0f;
  }

  if (!s_ui_widget_box_from_explicit_rect(ctx, &box, id, hit_rect, true))
  {
    return value;
  }

  if (s_ui_widget_box_can_skip(ctx, &box, false))
  {
    ctx->last_rect = rect;
    ctx->last_bounding_rect = rect;
    return value;
  }

  ctx->last_rect = rect;
  ctx->last_bounding_rect = rect;

  frame = s_ui_frame_state(
      ctx, box.id, box.rect, box.clip, false, false, box.disabled);

  if (frame.hot || frame.active)
  {
    ctx->cursor_type = vertical ? LDK_CURSOR_SIZE_WE : LDK_CURSOR_SIZE_NS;
  }

  cursor = vertical ? frame.cursor.x : frame.cursor.y;
  if (frame.hot && frame.pressed)
  {
    ctx->resize_handle_drag_start_cursor = cursor;
    ctx->resize_handle_drag_start_value = value;
  }

  if (frame.active && frame.dragging)
  {
    value = s_ui_clampf(ctx->resize_handle_drag_start_value +
                            cursor - ctx->resize_handle_drag_start_cursor,
        min_value, max_value);
  }

  color = ctx->theme.colors[LDK_UI_COLOR_SEPARATOR];
  if (frame.active)
  {
    color = ctx->theme.colors[LDK_UI_COLOR_CONTROL_BORDER_ACTIVE];
  }
  else if (frame.hot)
  {
    color = ctx->theme.colors[LDK_UI_COLOR_CONTROL_BORDER_HOVERED];
  }

  s_ui_render_quad(ctx, line_rect, color, box.clip, 0);
  return value;
}

float ldk_ui_widget_resize_handle_horizontal(LDKUIContext *ctx, LDKUIId id,
    float value, float min_value, float max_value, LDKUIRect rect)
{
  return s_ui_widget_resize_handle(
      ctx, id, value, min_value, max_value, rect, false);
}

float ldk_ui_widget_resize_handle_vertical(LDKUIContext *ctx, LDKUIId id,
    float value, float min_value, float max_value, LDKUIRect rect)
{
  return s_ui_widget_resize_handle(
      ctx, id, value, min_value, max_value, rect, true);
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

void ldk_ui_icon_label(LDKUIContext *ctx, LDKUIIcon icon, char const *text)
{
  LDKUISize text_size;
  LDKUISize min_size;
  LDKUILayoutRequest request;
  LDKUIRect rect;
  LDKUIId id;
  float spacing;

  if (text == NULL)
  {
    text = "";
  }

  spacing = LDK_UI_DEFAULT_SPACING;
  text_size = s_ui_layout_text_size(ctx, text);

  min_size.w = text_size.w;
  min_size.h = s_ui_maxf(LDK_UI_DEFAULT_CONTROL_HEIGHT, text_size.h);

  if (s_ui_icon_valid(icon))
  {
    min_size.w += icon.size.w + spacing;
    min_size.h = s_ui_maxf(min_size.h, icon.size.h);
  }

  request =
      s_ui_layout_request_make(LDK_UI_ITEM_ICON_LABEL, min_size, 1.0f, false);

  if (!s_ui_layout_rect_from_request(ctx, request, &rect, &id))
  {
    return;
  }

  ldk_ui_widget_icon_label(ctx, id, icon, text, rect);
}

bool ldk_ui_icon_button(LDKUIContext *ctx, LDKUIIcon icon, char const *text)
{
  if (text == NULL)
  {
    text = "";
  }

  LDKUISize text_size = s_ui_layout_text_size(ctx, text);

  LDKUISize min_size;
  min_size.w = LDK_UI_DEFAULT_SPACING * 4.0f + text_size.w;
  min_size.h = s_ui_maxf(LDK_UI_DEFAULT_CONTROL_HEIGHT, text_size.h);

  if (s_ui_icon_valid(icon))
  {
    min_size.w += icon.size.w;

    if (text[0] != '\0')
    {
      min_size.w += LDK_UI_DEFAULT_SPACING;
    }

    min_size.h = s_ui_maxf(min_size.h, icon.size.h);
  }

  LDKUILayoutRequest request =
      s_ui_layout_request_make(LDK_UI_ITEM_ICON_BUTTON, min_size, 1.0f, true);

  LDKUIRect rect;
  LDKUIId id;

  if (!s_ui_layout_rect_from_request(ctx, request, &rect, &id))
  {
    return false;
  }

  return ldk_ui_widget_icon_button(ctx, id, icon, text, rect);
}

u32 ldk_ui_widget_combo_box(LDKUIContext *ctx, LDKUIId id,
    const char *const *items, u32 item_count, u32 selected_index,
    LDKUIRect rect)
{
  const LDKUIId POPUP_TAG = 0x434F4D42u; // "COMB"
  if (!ctx || !id || !items || !item_count)
  {
    return selected_index;
  }
  if (selected_index >= item_count)
  {
    selected_index = 0;
  }
  const char *selected_text = items[selected_index] ?
      items[selected_index] : "";
  LDKUIIcon icon =
      s_ui_theme_icon(ctx, LDK_UI_THEME_ICON_TREE_NODE_EXPANDED);
  LDKUIId popup_id = s_ui_id_hash_u32(id, POPUP_TAG);
  LDKUIWidgetBox box = {0};

  if (!s_ui_widget_box_from_explicit_rect(ctx, &box, id, rect, true))
  {
    return selected_index;
  }

  LDKUIFrameState frame =
      s_ui_frame_state(ctx, box.id, box.rect, box.clip, true, true, box.disabled);
  u32 bg = s_ui_render_control_bg_color(ctx, frame.visual_state);
  u32 border = s_ui_render_control_border_color(ctx, frame.visual_state);
  u32 text_color = s_ui_render_control_text_color(ctx, frame.visual_state);
  LDKUISize selected_text_size = s_ui_widget_text_size(ctx, selected_text);

  s_ui_render_quad(ctx, box.rect, bg, box.clip, 0);
  s_ui_render_border(
      ctx, box.rect, ctx->theme.control_border_size, border, box.clip);

  LDKUIRect text_rect = box.rect;
  text_rect.x += LDK_UI_DEFAULT_SPACING * 2.0f;
  text_rect.w =
      s_ui_maxf(0.0f, box.rect.x + box.rect.w -
                          LDK_UI_DEFAULT_SPACING * 2.0f - text_rect.x);

  if (s_ui_icon_valid(icon))
  {
    LDKUIRect icon_rect;
    icon_rect.w = s_ui_minf(icon.size.w, box.rect.w);
    icon_rect.h = s_ui_minf(icon.size.h, box.rect.h);
    icon_rect.x =
        s_ui_maxf(box.rect.x, box.rect.x + box.rect.w -
                                  LDK_UI_DEFAULT_SPACING * 2.0f - icon_rect.w);
    icon_rect.y = box.rect.y + (box.rect.h - icon_rect.h) * 0.5f;

    text_rect.w =
        s_ui_maxf(0.0f, icon_rect.x - LDK_UI_DEFAULT_SPACING - text_rect.x);

    s_ui_render_icon(ctx, icon, icon_rect, text_color, box.clip);
  }

  LDKUIRect text_clip = s_ui_rect_intersect(&box.clip, &text_rect);
  float text_y =
      box.rect.y + (box.rect.h - selected_text_size.h) * 0.5f;

  s_ui_render_text(
      ctx, selected_text, text_rect.x, text_y, text_color, text_clip);

  if (frame.clicked)
  {
    if (ldk_ui_popup_is_open(ctx, popup_id))
    {
      ldk_ui_close_popup(ctx, popup_id);
    }
    else
    {
      LDKUIPoint popup_position = {
          rect.x,
          rect.y + rect.h,
      };

      ldk_ui_open_popup_at(ctx, popup_id, popup_position);
    }
  }

  if (ldk_ui_begin_popup(ctx, popup_id))
  {
    for (u32 i = 0; i < item_count; ++i)
    {
      const char *item_text = items[i] != NULL ? items[i] : "";

      /*
       * Make the popup at least as wide as the combo box.
       * The popup layout will still measure its final height.
       */
      ldk_ui_set_next_width(ctx, ldk_ui_px(rect.w));

      if (ldk_ui_button_flat(ctx, item_text))
      {
        selected_index = i;
        ldk_ui_close_current_popup(ctx);
      }
    }

    ldk_ui_end_popup(ctx);

    /*
     * ldk_ui_end_popup() changes the last-item information to the popup.
     * Restore the combo box as the last submitted widget.
     */
    ctx->last_rect = rect;
    ctx->last_bounding_rect = rect;
    ctx->last_id = id;
  }

  return selected_index;
}

u32 ldk_ui_combo_box(LDKUIContext *ctx, const char *const *items,
    u32 item_count, u32 selected_index)
{
  if (ctx == NULL || items == NULL || item_count == 0)
  {
    return selected_index;
  }

  if (selected_index >= item_count)
  {
    selected_index = 0;
  }

  const char *selected_text =
      items[selected_index] != NULL ? items[selected_index] : "";

  ctx->profile_text_measure_count += 1;
  LDKTextSize text_size = ldk_ttf_measure_text_cstr(ctx->font, selected_text);
  LDKUIIcon icon =
      s_ui_theme_icon(ctx, LDK_UI_THEME_ICON_TREE_NODE_EXPANDED);
  float icon_width = s_ui_icon_valid(icon) ? icon.size.w : 0.0f;

  LDKUISize min_size = {
      text_size.w + icon_width + LDK_UI_DEFAULT_SPACING * 5.0f,
      LDK_UI_DEFAULT_CONTROL_HEIGHT,
  };

  if (s_ui_icon_valid(icon))
  {
    min_size.h = s_ui_maxf(min_size.h, icon.size.h);
  }

  LDKUILayoutRequest request =
      s_ui_layout_request_make(LDK_UI_ITEM_COMBO_BOX, min_size, 1.0f, true);

  LDKUIRect rect;
  LDKUIId id;

  if (!s_ui_layout_rect_from_request(ctx, request, &rect, &id))
  {
    return selected_index;
  }

  return ldk_ui_widget_combo_box(ctx, id, items, item_count,
      selected_index, rect);
}

LDK_API void ldk_ui_clipboard_window_set(LDKUIContext *ctx, LDKWindow window)
{
  if (ctx == NULL)
  {
    return;
  }

  ctx->clipboard_window = window;
}

LDK_API void ldk_ui_selectable_text(LDKUIContext *ctx, char const *text)
{
  LDKUISize min_size;
  LDKUILayoutRequest request;
  LDKUIRect rect;
  LDKUIId id;
  float max_width = 0.0f;

  if (ctx == NULL)
  {
    return;
  }

  if (text == NULL)
  {
    text = "";
  }

  if (ctx->current_layout != NULL)
  {
    max_width = ctx->current_layout->content_rect.w;
  }

  LDKSizef text_size =
      ldk_ttf_measure_text_cstr_wrapped(ctx->font, text, max_width);
  min_size.w = 0.0f;
  min_size.h = s_ui_maxf(LDK_UI_DEFAULT_CONTROL_HEIGHT, text_size.h);

  /* Reuse the label item kind.  Selectability is a widget behavior; no new
   * layout primitive is necessary. */
  request = s_ui_layout_request_make(LDK_UI_ITEM_LABEL, min_size, 1.0f, true);

  if (!s_ui_layout_rect_from_request(ctx, request, &rect, &id))
  {
    return;
  }

  s_ui_widget_selectable_text(ctx, id, text, rect);
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

  s_ui_widget_label_with_text_size(ctx, id, text, text_size, rect);
}

bool ldk_ui_color_view(LDKUIContext *ctx, rgba32 color)
{
  LDKUISize text_size;
  LDKUISize min_size;
  LDKUILayoutRequest request;
  LDKUIRect rect;
  LDKUIId id;

  min_size.w = 64.0f;
  min_size.h = LDK_UI_DEFAULT_CONTROL_HEIGHT;

  request = s_ui_layout_request_make(LDK_UI_ITEM_BUTTON, min_size, 1.0f, true);

  if (!s_ui_layout_rect_from_request(ctx, request, &rect, &id))
  {
    return false;
  }

  return ldk_ui_widget_color_view(ctx, id, color, rect);
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

static float s_ui_tab_bar_item_width(
    LDKUIContext *ctx, LDKUITabBarItem const *item)
{
  float width = LDK_UI_TAB_BAR_TAB_PADDING * 2.0f;

  if (ctx == NULL || item == NULL)
  {
    return width;
  }

  bool has_icon = s_ui_icon_valid(item->icon);
  bool has_label = item->label != NULL && item->label[0] != '\0';

  if (has_icon)
  {
    width += item->icon.size.w;
  }

  if (has_label)
  {
    LDKUISize text_size = s_ui_widget_text_size(ctx, item->label);

    if (has_icon)
    {
      width += LDK_UI_DEFAULT_SPACING;
    }

    width += text_size.w;
  }

  return width;
}

static LDKUIId s_ui_tab_bar_item_id(
    LDKUIId bar_id, LDKUITabBarItem const *item, u32 index)
{
  LDKUIId id = bar_id;

  if (item != NULL && item->id != 0)
  {
    id = s_ui_id_hash_u32(id, item->id);
  }
  else
  {
    id = s_ui_id_hash_u32(id, index + 1);
  }

  return id;
}

LDKUITabBarResult ldk_ui_tab_bar(LDKUIContext *ctx,
    LDKUITabBarItem const *items, u32 item_count, u32 active_index)
{
  LDKUITabBarResult result = {0};
  result.active_index = active_index;
  result.pressed_index = UINT32_MAX;
  result.changed = false;

  if (ctx == NULL || items == NULL || item_count == 0)
  {
    return result;
  }

  if (result.active_index >= item_count)
  {
    result.active_index = 0;
  }

  LDKUIIcon menu_icon = s_ui_theme_icon(ctx, LDK_UI_THEME_ICON_MORE_VERT);

  float menu_button_width = LDK_UI_DEFAULT_SPACING;

  if (s_ui_icon_valid(menu_icon))
  {
    menu_button_width += menu_icon.size.w / 3.0f;
  }

  u32 draw_active_index = result.active_index;
  float bar_height =
      LDK_UI_TAB_BAR_TAB_HEIGHT + LDK_UI_TAB_BAR_LINE_THICKNESS;
  float total_width = menu_button_width + LDK_UI_TAB_BAR_SPACING;

  for (u32 i = 0; i < item_count; ++i)
  {
    total_width += s_ui_tab_bar_item_width(ctx, &items[i]);

    if (i + 1 < item_count)
    {
      total_width += LDK_UI_TAB_BAR_SPACING;
    }
  }

  LDKUISize min_size;
  min_size.w = total_width;
  min_size.h = bar_height;

  LDKUILayoutRequest request =
      s_ui_layout_request_make(LDK_UI_ITEM_TAB_BAR, min_size, 0.0f, false);

  request.has_height = true;
  request.height = ldk_ui_px(bar_height);

  LDKUIRect bar_rect;
  LDKUIId bar_id;

  if (!s_ui_layout_rect_from_request(ctx, request, &bar_rect, &bar_id))
  {
    return result;
  }

  LDKUIRect parent_clip = s_ui_current_clip_rect(ctx);
  LDKUIRect bar_clip = s_ui_rect_intersect(&parent_clip, &bar_rect);

  s_ui_render_quad(
      ctx, bar_rect, ctx->theme.colors[LDK_UI_COLOR_TAB_BAR_BG], bar_clip, 0);

  LDKUIRect menu_button_rect;
  menu_button_rect.x = bar_rect.x;
  menu_button_rect.y = bar_rect.y;
  menu_button_rect.w = s_ui_minf(menu_button_width, bar_rect.w);
  menu_button_rect.h = LDK_UI_TAB_BAR_TAB_HEIGHT;

  LDKUIId menu_button_id = s_ui_id_hash_u32(bar_id, 0x5441424du);
  LDKUIId popup_id = s_ui_id_hash_u32(bar_id, 0x54414250u);

  if (ldk_ui_widget_icon_button(
          ctx, menu_button_id, menu_icon, "", menu_button_rect))
  {
    LDKUIPoint popup_position;
    popup_position.x = menu_button_rect.x;
    popup_position.y = menu_button_rect.y + menu_button_rect.h;

    ldk_ui_open_popup_at(ctx, popup_id, popup_position);
  }

  float tabs_x = bar_rect.x + menu_button_width + LDK_UI_TAB_BAR_SPACING;
  float bar_right = bar_rect.x + bar_rect.w;
  LDKUIRect active_rect = {0};
  bool active_rect_visible = false;

  float cursor_x = tabs_x;

  for (u32 i = 0; i < item_count; ++i)
  {
    float tab_width = s_ui_tab_bar_item_width(ctx, &items[i]);

    if (cursor_x >= bar_right)
    {
      break;
    }

    LDKUIRect tab_rect;
    tab_rect.x = cursor_x;
    tab_rect.y = bar_rect.y;
    tab_rect.w = s_ui_minf(tab_width, bar_right - cursor_x);
    tab_rect.h = LDK_UI_TAB_BAR_TAB_HEIGHT;

    if (i == draw_active_index)
    {
      active_rect = tab_rect;
      active_rect_visible = true;
    }
    else
    {
      LDKUIId tab_id = s_ui_tab_bar_item_id(bar_id, &items[i], i);
      bool clicked = ldk_ui_widget_tab(
          ctx, tab_id, items[i].icon, items[i].label, tab_rect, false);

      if (ctx->active_id == tab_id)
      {
        result.pressed_index = i;
      }

      if (clicked)
      {
        result.active_index = i;
        result.changed = i != draw_active_index;
      }
    }

    cursor_x += tab_width + LDK_UI_TAB_BAR_SPACING;
  }

  if (active_rect_visible)
  {
    LDKUITabBarItem const *item = &items[draw_active_index];
    LDKUIId tab_id = s_ui_tab_bar_item_id(bar_id, item, draw_active_index);
    bool clicked = ldk_ui_widget_tab(
        ctx, tab_id, item->icon, item->label, active_rect, true);

    if (ctx->active_id == tab_id)
    {
      result.pressed_index = draw_active_index;
    }

    if (clicked)
    {
      result.active_index = draw_active_index;
      result.changed = false;
    }
  }

  float line_y = bar_rect.y + LDK_UI_TAB_BAR_TAB_HEIGHT;

  LDKUIRect separator;
  separator.x = bar_rect.x;
  separator.y = line_y;
  separator.w = bar_rect.w;
  separator.h = LDK_UI_TAB_BAR_LINE_THICKNESS;

  s_ui_render_quad(ctx, separator,
      ctx->theme.colors[LDK_UI_COLOR_TAB_BAR_SEPARATOR], bar_clip, 0);

  if (ldk_ui_begin_popup(ctx, popup_id))
  {
    for (u32 i = 0; i < item_count; ++i)
    {
      ldk_ui_push_id_u32(ctx, items[i].id != 0 ? items[i].id : i + 1);

      if (i == result.active_index)
      {
        ldk_ui_set_next_disabled(ctx, true);
      }

      char const *label = items[i].label != NULL ? items[i].label : "";

      if (ldk_ui_button_flat(ctx, label))
      {
        if (result.active_index != i)
        {
          result.active_index = i;
          result.changed = i != draw_active_index;
        }

        ldk_ui_close_current_popup(ctx);
      }

      ldk_ui_pop_id(ctx);
    }

    ldk_ui_end_popup(ctx);
  }

  ctx->last_id = bar_id;
  ctx->last_rect = bar_rect;
  ctx->last_bounding_rect = bar_rect;

  return result;
}

float ldk_ui_slider(
    LDKUIContext *ctx, float value, float min_value, float max_value)
{
  LDKUILayoutRequest request;
  LDKUIRect rect;
  LDKUIId id;
  LDKUISize min_size;

  min_size.w = LDK_UI_DEFAULT_CONTROL_WIDTH;
  min_size.h = LDK_UI_DEFAULT_CONTROL_HEIGHT;

  request = s_ui_layout_request_make(LDK_UI_ITEM_SLIDER, min_size, 1.0f, true);

  if (!s_ui_layout_rect_from_request(ctx, request, &rect, &id))
  {
    return value;
  }

  return ldk_ui_widget_slider(ctx, id, value, min_value, max_value, rect);
}

float ldk_ui_slider_input(
    LDKUIContext *ctx, float value, float min_value, float max_value)
{
  LDKUILayoutRequest request;
  LDKUIRect rect;
  LDKUIId id;
  LDKUISize min_size;

  min_size.w = 120.0f;
  min_size.h = LDK_UI_DEFAULT_CONTROL_HEIGHT;
  request =
      s_ui_layout_request_make(LDK_UI_ITEM_SLIDER_INPUT, min_size, 1.0f, true);

  if (!s_ui_layout_rect_from_request(ctx, request, &rect, &id))
  {
    return value;
  }

  return ldk_ui_widget_slider_input(ctx, id, value, min_value, max_value, rect);
}

u32 ldk_ui_input_box(LDKUIContext *ctx, char *buffer, u32 buffer_size)
{
  LDKUILayoutRequest request;
  LDKUIRect rect;
  LDKUIId id;
  LDKUISize min_size;

  min_size.w = LDK_UI_DEFAULT_CONTROL_WIDTH;
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

  min_size.w = LDK_UI_DEFAULT_CONTROL_WIDTH;
  min_size.h = LDK_UI_DEFAULT_CONTROL_HEIGHT;

  request =
      s_ui_layout_request_make(LDK_UI_ITEM_INPUT_BOX, min_size, 1.0f, true);

  if (!s_ui_layout_rect_from_request(ctx, request, &rect, &id))
  {
    return LDK_UI_INPUT_BOX_NONE;
  }

  return ldk_ui_widget_input_label(ctx, id, buffer, buffer_size, rect);
}

static float s_ui_resize_handle(LDKUIContext *ctx, float value,
    float min_value, float max_value, bool vertical)
{
  LDKUILayoutRequest request;
  LDKUIRect rect;
  LDKUIId id;
  LDKUISize min_size = {0};

  if (vertical)
  {
    min_size.h = 1.0f;
  }
  else
  {
    min_size.w = 1.0f;
  }

  request = s_ui_layout_request_make(
      LDK_UI_ITEM_RESIZE_HANDLE, min_size, 0.0f, true);

  if (!s_ui_layout_rect_from_request(ctx, request, &rect, &id))
  {
    return value;
  }

  if (vertical)
  {
    return ldk_ui_widget_resize_handle_vertical(
        ctx, id, value, min_value, max_value, rect);
  }

  return ldk_ui_widget_resize_handle_horizontal(
      ctx, id, value, min_value, max_value, rect);
}

float ldk_ui_resize_handle_horizontal(
    LDKUIContext *ctx, float value, float min_value, float max_value)
{
  return s_ui_resize_handle(ctx, value, min_value, max_value, false);
}

float ldk_ui_resize_handle_vertical(
    LDKUIContext *ctx, float value, float min_value, float max_value)
{
  return s_ui_resize_handle(ctx, value, min_value, max_value, true);
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

  s_ui_render_quad(ctx, rect, ctx->theme.colors[LDK_UI_COLOR_SEPARATOR],
      s_ui_current_clip_rect(ctx), 0);
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

