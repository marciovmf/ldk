#include "ldk_ui_drag_n_drop.h"

#include <string.h>

#define LDK_UI_DRAG_N_DROP_PREVIEW_SIZE 32.0f
#define LDK_UI_DRAG_N_DROP_PREVIEW_OFFSET 8.0f
#define LDK_UI_DRAG_N_DROP_ACTIVATION_THRESHOLD 6.0f

typedef struct LDKUIDragNDropPayload
{
  u32 type;
  XSmallstr data;
  LDKPoint origin;
  bool valid;
  bool origin_valid;
  bool active;
} LDKUIDragNDropPayload;

static LDKUIDragNDropPayload s_drag_n_drop_payload;

static bool s_drag_n_drop_update(LDKUIContext *ui)
{
  LDKMouseState *mouse;
  LDKPoint cursor;
  float dx;
  float dy;
  float threshold;

  if (!s_drag_n_drop_payload.valid || ui == NULL || ui->mouse == NULL)
  {
    return false;
  }

  mouse = (LDKMouseState *)ui->mouse;
  if (!ldk_os_mouse_button_is_pressed(mouse, LDK_MOUSE_BUTTON_LEFT))
  {
    return s_drag_n_drop_payload.active;
  }

  cursor = ldk_os_mouse_cursor(mouse);

  if (!s_drag_n_drop_payload.origin_valid)
  {
    s_drag_n_drop_payload.origin = cursor;
    s_drag_n_drop_payload.origin_valid = true;
    return false;
  }

  if (s_drag_n_drop_payload.active)
  {
    return true;
  }

  dx = (float)(cursor.x - s_drag_n_drop_payload.origin.x);
  dy = (float)(cursor.y - s_drag_n_drop_payload.origin.y);
  threshold = LDK_UI_DRAG_N_DROP_ACTIVATION_THRESHOLD;

  if (dx * dx + dy * dy >= threshold * threshold)
  {
    s_drag_n_drop_payload.active = true;
  }

  return s_drag_n_drop_payload.active;
}

void ldk_ui_drag_n_drop_payload_set(u32 type, const XSmallstr *payload)
{
  if (payload == NULL)
  {
    return;
  }

  s_drag_n_drop_payload = (LDKUIDragNDropPayload){0};
  s_drag_n_drop_payload.type = type;
  s_drag_n_drop_payload.data = *payload;
  s_drag_n_drop_payload.valid = true;
}

void ldk_ui_drag_n_drop_preview_draw(LDKUIContext *ui, LDKUIIcon icon)
{
  LDKMouseState *mouse;
  LDKPoint cursor;
  LDKUIRect rect;
  LDKUIDrawCmd command;
  u32 base_index;
  u32 index_offset;
  rgba32 color;

  if (!s_drag_n_drop_update(ui) || ui->popup_vertices == NULL ||
      ui->popup_indices == NULL || ui->popup_commands == NULL ||
      icon.texture == 0 || icon.uv.w <= 0.0f || icon.uv.h <= 0.0f ||
      icon.size.w <= 0.0f || icon.size.h <= 0.0f ||
      ui->viewport.w <= 0.0f || ui->viewport.h <= 0.0f)
  {
    return;
  }

  mouse = (LDKMouseState *)ui->mouse;
  cursor = ldk_os_mouse_cursor(mouse);

  rect.x = (float)cursor.x + LDK_UI_DRAG_N_DROP_PREVIEW_OFFSET;
  rect.y = (float)cursor.y + LDK_UI_DRAG_N_DROP_PREVIEW_OFFSET;
  rect.w = LDK_UI_DRAG_N_DROP_PREVIEW_SIZE;
  rect.h = LDK_UI_DRAG_N_DROP_PREVIEW_SIZE;

  base_index = x_array_ldk_ui_vertex_count(ui->popup_vertices);
  index_offset = x_array_ldk_ui_u32_count(ui->popup_indices);
  color = LDK_RGBA32(icon.color);

  x_array_ldk_ui_vertex_push(ui->popup_vertices,
      (LDKUIVertex){rect.x, rect.y, icon.uv.x, icon.uv.y, color});
  x_array_ldk_ui_vertex_push(ui->popup_vertices,
      (LDKUIVertex){rect.x + rect.w, rect.y, icon.uv.x + icon.uv.w, icon.uv.y,
          color});
  x_array_ldk_ui_vertex_push(ui->popup_vertices,
      (LDKUIVertex){rect.x + rect.w, rect.y + rect.h, icon.uv.x + icon.uv.w,
          icon.uv.y + icon.uv.h, color});
  x_array_ldk_ui_vertex_push(ui->popup_vertices,
      (LDKUIVertex){rect.x, rect.y + rect.h, icon.uv.x,
          icon.uv.y + icon.uv.h, color});

  x_array_ldk_ui_u32_push(ui->popup_indices, base_index + 0);
  x_array_ldk_ui_u32_push(ui->popup_indices, base_index + 1);
  x_array_ldk_ui_u32_push(ui->popup_indices, base_index + 2);
  x_array_ldk_ui_u32_push(ui->popup_indices, base_index + 2);
  x_array_ldk_ui_u32_push(ui->popup_indices, base_index + 3);
  x_array_ldk_ui_u32_push(ui->popup_indices, base_index + 0);

  command.texture = (LDKUITextureHandle)icon.texture;
  command.clip_rect = ui->viewport;
  command.index_offset = index_offset;
  command.index_count = 6;
  x_array_ldk_ui_draw_cmd_push(ui->popup_commands, command);
}

bool ldk_ui_drag_n_drop_payload_get_and_remove(
    u32 *out_type, XSmallstr *out_payload)
{
  bool active;

  if (!s_drag_n_drop_payload.valid)
  {
    return false;
  }

  active = s_drag_n_drop_payload.active;

  if (active && out_type != NULL)
  {
    *out_type = s_drag_n_drop_payload.type;
  }

  if (active && out_payload != NULL)
  {
    *out_payload = s_drag_n_drop_payload.data;
  }

  memset(&s_drag_n_drop_payload, 0, sizeof(s_drag_n_drop_payload));
  return active;
}
