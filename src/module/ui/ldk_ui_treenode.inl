#include <ldk_common.h>
#include <module/ldk_ui.h>

//------------------------------------------------------------
// Tree nodes
//------------------------------------------------------------

#ifndef LDK_UI_TREE_NODE_INDENT_WIDTH
#define LDK_UI_TREE_NODE_INDENT_WIDTH 8.0f
#endif

#ifndef LDK_UI_TREE_NODE_CHEVRON_WIDTH
#define LDK_UI_TREE_NODE_CHEVRON_WIDTH 10.0f
#endif

u32 ldk_ui_tree_node_ex(LDKUIContext *ctx, char const *title, LDKUIIcon icon,
    bool expanded, u32 depth, u32 flags)
{
  if (ctx == NULL)
  {
    return LDK_UI_TREE_NODE_RESULT_NONE;
  }

  u32 result = LDK_UI_TREE_NODE_RESULT_NONE;

  char const *safe_title = title != NULL ? title : "";
  LDKUISize text_size = s_ui_layout_text_size(ctx, safe_title);
  float indent_width = (float)depth * LDK_UI_TREE_NODE_INDENT_WIDTH;
  bool leaf = (flags & LDK_UI_TREE_NODE_LEAF) != 0;

  LDKUIIcon chevron_icon =
      s_ui_theme_icon(ctx, expanded ? LDK_UI_THEME_ICON_TREE_NODE_EXPANDED
                                    : LDK_UI_THEME_ICON_TREE_NODE_COLLAPSED);

  bool chevron_icon_valid = !leaf && s_ui_icon_valid(chevron_icon);
  bool icon_valid = s_ui_icon_valid(icon);

  float chevron_width =
      chevron_icon_valid ? chevron_icon.size.w : LDK_UI_TREE_NODE_CHEVRON_WIDTH;

  float label_width = text_size.w;
  if (icon_valid)
  {
    label_width += icon.size.w + LDK_UI_DEFAULT_SPACING;
  }

  LDKUISize min_size = {0};
  min_size.w = indent_width + chevron_width + LDK_UI_DEFAULT_SPACING +
               label_width + LDK_UI_DEFAULT_SPACING;
  min_size.h = LDK_UI_DEFAULT_CONTROL_HEIGHT;

  if (chevron_icon_valid && chevron_icon.size.h > min_size.h)
  {
    min_size.h = chevron_icon.size.h;
  }

  if (icon_valid && icon.size.h > min_size.h)
  {
    min_size.h = icon.size.h;
  }

  LDKUILayoutRequest request =
      s_ui_layout_request_make(LDK_UI_ITEM_BUTTON, min_size, 0.0f, true);

  LDKUIRect rect = {0};
  LDKUIId id = 0;

  if (!s_ui_layout_rect_from_request(ctx, request, &rect, &id))
  {
    return result;
  }

  LDKUIWidgetBox box = {0};

  if (!s_ui_widget_box_from_explicit_rect(ctx, &box, id, rect, true))
  {
    return result;
  }

  LDKUIFrameState frame =
      s_ui_frame_state(ctx, box.id, box.rect, box.clip, true, box.disabled);

  if ((flags & LDK_UI_TREE_NODE_SELECTED))
  {
    s_ui_render_quad(
        ctx, box.rect, ctx->theme.colors[LDK_UI_COLOR_FOCUS], box.clip, 0);
  }

  if (frame.visual_state == LDK_UI_CONTROL_VISUAL_STATE_HOVERED ||
      frame.visual_state == LDK_UI_CONTROL_VISUAL_STATE_ACTIVE ||
      frame.visual_state == LDK_UI_CONTROL_VISUAL_STATE_ACTIVE_HOVERED)
  {
    u32 bg = s_ui_render_control_bg_color(ctx, frame.visual_state);
    s_ui_render_quad(ctx, box.rect, bg, box.clip, 0);
  }

  u32 text_color = s_ui_render_control_text_color(ctx, frame.visual_state);

  float chevron_x = box.rect.x + indent_width;
  float label_x = chevron_x + chevron_width + LDK_UI_DEFAULT_SPACING;

  LDKUIRect chevron_hit_rect = {0};
  chevron_hit_rect.x = chevron_x;
  chevron_hit_rect.y = box.rect.y;
  chevron_hit_rect.w = chevron_width;
  chevron_hit_rect.h = box.rect.h;

  if (!leaf)
  {
    if (chevron_icon_valid)
    {
      LDKUIRect icon_rect = {0};
      icon_rect.x = chevron_x;
      icon_rect.y = box.rect.y + (box.rect.h - chevron_icon.size.h) * 0.5f;
      icon_rect.w = chevron_icon.size.w;
      icon_rect.h = chevron_icon.size.h;

      s_ui_render_icon(ctx, chevron_icon, icon_rect, box.clip);
    }
    else
    {
      char const *chevron = expanded ? "v" : ">";
      float chevron_text_y = box.rect.y + (box.rect.h - text_size.h) * 0.5f;

      s_ui_render_text(
          ctx, chevron, chevron_x, chevron_text_y, text_color, box.clip);
    }
  }

  LDKUIRect label_rect = {0};
  label_rect.x = label_x;
  label_rect.y = box.rect.y;
  label_rect.w = s_ui_maxf(0.0f, box.rect.x + box.rect.w - label_x);
  label_rect.h = box.rect.h;

  s_ui_render_icon_label(
      ctx, icon, safe_title, label_rect, text_color, box.clip);

  if (frame.clicked)
  {
    result |= LDK_UI_TREE_NODE_RESULT_CLICKED;

    if (!leaf &&
        s_ui_rect_contains(&chevron_hit_rect, frame.cursor.x, frame.cursor.y))
    {
      result |= LDK_UI_TREE_NODE_RESULT_TOGGLED;
    }
  }

  return result;
}

bool ldk_ui_tree_node(
    LDKUIContext *ctx, char const *title, bool expanded, u32 depth, u32 flags)
{
  LDKUIIcon icon = {0};
  u32 result = ldk_ui_tree_node_ex(ctx, title, icon, expanded, depth, flags);

  if (result & LDK_UI_TREE_NODE_RESULT_TOGGLED)
  {
    expanded = !expanded;
  }

  return expanded;
}
