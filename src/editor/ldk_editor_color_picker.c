#include "ldk_editor_color_picker.h"
#include "ldk_editor_internal.h"

#include <ldk_os.h>
#include <module/ldk_ui.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

static const float s_color_picker_width = 220.0f;
static const float s_color_picker_sv_height = 170.0f;
static const float s_color_picker_hue_height = 18.0f;
static const float s_color_picker_swatch_width = 28.0f;

typedef enum LDKEditorColorPickerDrag
{
  LDK_EDITOR_COLOR_PICKER_DRAG_NONE = 0,
  LDK_EDITOR_COLOR_PICKER_DRAG_SV,
  LDK_EDITOR_COLOR_PICKER_DRAG_HUE,
} LDKEditorColorPickerDrag;

typedef struct LDKEditorColorPickerState
{
  LDKUIId popup_id;
  LDKUIId popup_hex_id;
  LDKUIId viewer_hex_owner;
  LDKUIId viewer_hex_id;
  float hue;
  float saturation;
  float value;
  char popup_hex[16];
  char viewer_hex[16];
  u32 mode;
  LDKEditorColorPickerDrag drag;
  bool viewer_hex_valid;
} LDKEditorColorPickerState;

static LDKEditorColorPickerState s_color_picker = {0};

static float s_clamp01(float value)
{
  if (value < 0.0f)
  {
    return 0.0f;
  }
  if (value > 1.0f)
  {
    return 1.0f;
  }
  return value;
}

static u8 s_byte(float value)
{
  return (u8)(s_clamp01(value) * 255.0f + 0.5f);
}

static rgba32 s_pack(u8 r, u8 g, u8 b, u8 a)
{
  return ((rgba32)r << 24) | ((rgba32)g << 16) | ((rgba32)b << 8) |
         (rgba32)a;
}

static void s_unpack(rgba32 color, u8 *r, u8 *g, u8 *b, u8 *a)
{
  if (r)
  {
    *r = (u8)((color >> 24) & 0xffu);
  }
  if (g)
  {
    *g = (u8)((color >> 16) & 0xffu);
  }
  if (b)
  {
    *b = (u8)((color >> 8) & 0xffu);
  }
  if (a)
  {
    *a = (u8)(color & 0xffu);
  }
}

static void s_hsv_to_rgb(
    float hue, float saturation, float value, float *r, float *g, float *b)
{
  float h = hue;
  float s = s_clamp01(saturation);
  float v = s_clamp01(value);
  float rr = v;
  float gg = v;
  float bb = v;

  if (h < 0.0f || h > 1.0f)
  {
    h -= floorf(h);
  }

  if (s > 0.00001f)
  {
    float scaled;
    float fraction;
    float p;
    float q;
    float t;
    i32 sector;

    if (h >= 1.0f)
    {
      h = 0.0f;
    }

    scaled = h * 6.0f;
    sector = (i32)floorf(scaled);
    fraction = scaled - (float)sector;
    p = v * (1.0f - s);
    q = v * (1.0f - s * fraction);
    t = v * (1.0f - s * (1.0f - fraction));

    switch (sector % 6)
    {
    case 0:
      rr = v;
      gg = t;
      bb = p;
      break;
    case 1:
      rr = q;
      gg = v;
      bb = p;
      break;
    case 2:
      rr = p;
      gg = v;
      bb = t;
      break;
    case 3:
      rr = p;
      gg = q;
      bb = v;
      break;
    case 4:
      rr = t;
      gg = p;
      bb = v;
      break;
    default:
      rr = v;
      gg = p;
      bb = q;
      break;
    }
  }

  if (r)
  {
    *r = rr;
  }
  if (g)
  {
    *g = gg;
  }
  if (b)
  {
    *b = bb;
  }
}

static void s_rgb_to_hsv(float r, float g, float b, float previous_hue,
    float *hue, float *saturation, float *value)
{
  float maximum = fmaxf(r, fmaxf(g, b));
  float minimum = fminf(r, fminf(g, b));
  float delta = maximum - minimum;
  float h = previous_hue;

  if (delta > 0.00001f)
  {
    if (maximum == r)
    {
      h = (g - b) / delta;
      if (h < 0.0f)
      {
        h += 6.0f;
      }
    }
    else if (maximum == g)
    {
      h = 2.0f + (b - r) / delta;
    }
    else
    {
      h = 4.0f + (r - g) / delta;
    }
    h /= 6.0f;
  }

  *hue = s_clamp01(h);
  *saturation = maximum > 0.0f ? s_clamp01(delta / maximum) : 0.0f;
  *value = s_clamp01(maximum);
}

static void s_state_from_color(rgba32 color)
{
  u8 r;
  u8 g;
  u8 b;

  s_unpack(color, &r, &g, &b, NULL);
  s_rgb_to_hsv((float)r / 255.0f, (float)g / 255.0f,
      (float)b / 255.0f, s_color_picker.hue, &s_color_picker.hue,
      &s_color_picker.saturation, &s_color_picker.value);
}

static void s_color_from_state(rgba32 *color)
{
  float r;
  float g;
  float b;
  u8 alpha = (u8)(*color & 0xffu);

  s_hsv_to_rgb(s_color_picker.hue, s_color_picker.saturation,
      s_color_picker.value, &r, &g, &b);
  *color = s_pack(s_byte(r), s_byte(g), s_byte(b), alpha);
}

static rgba32 s_hue_color(float hue)
{
  float r;
  float g;
  float b;
  s_hsv_to_rgb(hue, 1.0f, 1.0f, &r, &g, &b);
  return s_pack(s_byte(r), s_byte(g), s_byte(b), 255u);
}

static void s_hex_format(char *out, size_t out_size, rgba32 color)
{
  snprintf(out, out_size, "#%08X", color);
}

static i32 s_hex_nibble(char c)
{
  if (c >= '0' && c <= '9')
  {
    return c - '0';
  }
  if (c >= 'a' && c <= 'f')
  {
    return c - 'a' + 10;
  }
  if (c >= 'A' && c <= 'F')
  {
    return c - 'A' + 10;
  }
  return -1;
}

static bool s_hex_parse(const char *text, rgba32 *out_color)
{
  const char *digits;
  size_t length;
  rgba32 value = 0;

  if (!text || !out_color)
  {
    return false;
  }

  digits = text[0] == '#' ? text + 1 : text;
  length = strlen(digits);
  if (length != 6 && length != 8)
  {
    return false;
  }

  for (size_t i = 0; i < length; ++i)
  {
    i32 nibble = s_hex_nibble(digits[i]);
    if (nibble < 0)
    {
      return false;
    }
    value = (value << 4) | (rgba32)nibble;
  }

  if (length == 6)
  {
    value = (value << 8) | 0xffu;
  }

  *out_color = value;
  return true;
}

static void s_gradient(LDKUIContext *ui, LDKUIId id, LDKUIRect rect,
    rgba32 top_left, rgba32 top_right, rgba32 bottom_right,
    rgba32 bottom_left)
{
  ldk_ui_widget_gradient(
      ui, id, top_left, top_right, bottom_right, bottom_left, rect);
}

static void s_solid(
    LDKUIContext *ui, LDKUIId id, LDKUIRect rect, rgba32 color)
{
  s_gradient(ui, id, rect, color, color, color, color);
}

static void s_marker(
    LDKUIContext *ui, LDKUIId id, float center_x, float center_y)
{
  const float size = 12.0f;
  const float thickness = 2.0f;
  const rgba32 white = 0xffffffffu;
  LDKUIRect top = {center_x - size * 0.5f, center_y - size * 0.5f, size,
      thickness};
  LDKUIRect bottom = {center_x - size * 0.5f,
      center_y + size * 0.5f - thickness, size, thickness};
  LDKUIRect left = {center_x - size * 0.5f, center_y - size * 0.5f, thickness,
      size};
  LDKUIRect right = {center_x + size * 0.5f - thickness,
      center_y - size * 0.5f, thickness, size};

  s_solid(ui, id ^ 1u, top, white);
  s_solid(ui, id ^ 2u, bottom, white);
  s_solid(ui, id ^ 3u, left, white);
  s_solid(ui, id ^ 4u, right, white);
}

static bool s_mouse_inside(LDKUIContext *ui, LDKUIRect rect)
{
  LDKPoint cursor;

  if (!ui || !ui->mouse)
  {
    return false;
  }

  cursor = ldk_os_mouse_cursor((LDKMouseState *)ui->mouse);
  return (float)cursor.x >= rect.x && (float)cursor.x <= rect.x + rect.w &&
         (float)cursor.y >= rect.y && (float)cursor.y <= rect.y + rect.h &&
         (float)cursor.x >= ui->clip_rect.x &&
         (float)cursor.x <= ui->clip_rect.x + ui->clip_rect.w &&
         (float)cursor.y >= ui->clip_rect.y &&
         (float)cursor.y <= ui->clip_rect.y + ui->clip_rect.h;
}

static bool s_drag_sv(LDKUIContext *ui, LDKUIRect rect)
{
  bool down;
  bool pressed;
  bool changed = false;

  if (!ui || !ui->mouse || rect.w <= 0.0f || rect.h <= 0.0f)
  {
    return false;
  }

  down = ldk_os_mouse_button_down(
      (LDKMouseState *)ui->mouse, LDK_MOUSE_BUTTON_LEFT);
  pressed = ldk_os_mouse_button_is_pressed(
      (LDKMouseState *)ui->mouse, LDK_MOUSE_BUTTON_LEFT);

  if (down && s_mouse_inside(ui, rect))
  {
    s_color_picker.drag = LDK_EDITOR_COLOR_PICKER_DRAG_SV;
    ui->focused_id = 0;
    ui->input_box_id = 0;
  }

  if (s_color_picker.drag == LDK_EDITOR_COLOR_PICKER_DRAG_SV &&
      (down || pressed))
  {
    LDKPoint cursor = ldk_os_mouse_cursor((LDKMouseState *)ui->mouse);
    float saturation = s_clamp01(((float)cursor.x - rect.x) / rect.w);
    float value =
        1.0f - s_clamp01(((float)cursor.y - rect.y) / rect.h);

    changed = fabsf(s_color_picker.saturation - saturation) > 0.0001f ||
              fabsf(s_color_picker.value - value) > 0.0001f;
    s_color_picker.saturation = saturation;
    s_color_picker.value = value;
  }

  if (s_color_picker.drag == LDK_EDITOR_COLOR_PICKER_DRAG_SV &&
      ldk_os_mouse_button_up(
          (LDKMouseState *)ui->mouse, LDK_MOUSE_BUTTON_LEFT))
  {
    s_color_picker.drag = LDK_EDITOR_COLOR_PICKER_DRAG_NONE;
  }

  return changed;
}

static bool s_drag_hue(LDKUIContext *ui, LDKUIRect rect)
{
  bool down;
  bool pressed;
  bool changed = false;

  if (!ui || !ui->mouse || rect.w <= 0.0f)
  {
    return false;
  }

  down = ldk_os_mouse_button_down(
      (LDKMouseState *)ui->mouse, LDK_MOUSE_BUTTON_LEFT);
  pressed = ldk_os_mouse_button_is_pressed(
      (LDKMouseState *)ui->mouse, LDK_MOUSE_BUTTON_LEFT);

  if (down && s_mouse_inside(ui, rect))
  {
    s_color_picker.drag = LDK_EDITOR_COLOR_PICKER_DRAG_HUE;
    ui->focused_id = 0;
    ui->input_box_id = 0;
  }

  if (s_color_picker.drag == LDK_EDITOR_COLOR_PICKER_DRAG_HUE &&
      (down || pressed))
  {
    LDKPoint cursor = ldk_os_mouse_cursor((LDKMouseState *)ui->mouse);
    float hue = s_clamp01(((float)cursor.x - rect.x) / rect.w);
    changed = fabsf(s_color_picker.hue - hue) > 0.0001f;
    s_color_picker.hue = hue;
  }

  if (s_color_picker.drag == LDK_EDITOR_COLOR_PICKER_DRAG_HUE &&
      ldk_os_mouse_button_up(
          (LDKMouseState *)ui->mouse, LDK_MOUSE_BUTTON_LEFT))
  {
    s_color_picker.drag = LDK_EDITOR_COLOR_PICKER_DRAG_NONE;
  }

  return changed;
}

static void s_draw_sv(LDKUIContext *ui, LDKUIRect rect, LDKUIId id)
{
  rgba32 hue = s_hue_color(s_color_picker.hue);
  const rgba32 white = 0xffffffffu;
  const rgba32 transparent_black = 0x00000000u;
  const rgba32 black = 0x000000ffu;
  float marker_x = rect.x + rect.w * s_color_picker.saturation;
  float marker_y = rect.y + rect.h * (1.0f - s_color_picker.value);

  s_gradient(ui, id ^ 0x10u, rect, white, hue, hue, white);
  s_gradient(ui, id ^ 0x20u, rect, transparent_black, transparent_black, black,
      black);
  s_marker(ui, id ^ 0x30u, marker_x, marker_y);
}

static void s_draw_hue(LDKUIContext *ui, LDKUIRect rect, LDKUIId id)
{
  static const rgba32 colors[] = {0xff0000ffu, 0xffff00ffu, 0x00ff00ffu,
      0x00ffffffu, 0x0000ffffu, 0xff00ffffu, 0xff0000ffu};
  const u32 segment_count = 6u;

  for (u32 i = 0; i < segment_count; ++i)
  {
    float x0 = rect.x + rect.w * ((float)i / (float)segment_count);
    float x1 = rect.x + rect.w * ((float)(i + 1u) / (float)segment_count);
    LDKUIRect segment = {x0, rect.y, x1 - x0, rect.h};
    s_gradient(ui, id ^ (0x100u + i), segment, colors[i], colors[i + 1u],
        colors[i + 1u], colors[i]);
  }

  s_marker(ui, id ^ 0x200u, rect.x + rect.w * s_color_picker.hue,
      rect.y + rect.h * 0.5f);
}

static bool s_slider_row(LDKUIContext *ui, const char *label, float *value,
    float minimum, float maximum, const char *value_text, bool readonly)
{
  float previous = *value;

  ldk_ui_begin_horizontal(ui);
  ldk_ui_set_next_width(ui, ldk_ui_px(22.0f));
  ldk_ui_label(ui, label);
  ldk_ui_set_next_width(ui, ldk_ui_fill());
  ldk_ui_begin_disabled(ui, readonly);
  *value = ldk_ui_slider(ui, *value, minimum, maximum);
  ldk_ui_end_disabled(ui);
  ldk_ui_set_next_width(ui, ldk_ui_px(52.0f));
  ldk_ui_label(ui, value_text);
  ldk_ui_end_horizontal(ui);

  return !readonly && fabsf(previous - *value) > 0.0001f;
}

static bool s_popup(LDKEditorContext *editor, rgba32 *color, bool readonly)
{
  LDKUIContext *ui = &editor->ui;
  LDKUIRect sv_rect;
  LDKUIRect hue_rect;
  LDKUITabBarItem tabs[2] = {0};
  bool changed = false;

  if (!ldk_ui_begin_popup(ui, s_color_picker.popup_id))
  {
    s_color_picker.drag = LDK_EDITOR_COLOR_PICKER_DRAG_NONE;
    return false;
  }

  ldk_ui_set_next_size(
      ui, ldk_ui_px(s_color_picker_width), ldk_ui_px(s_color_picker_sv_height));
  ldk_ui_spacer(ui);
  sv_rect = ldk_ui_last_rect(ui);
  if (!readonly && s_drag_sv(ui, sv_rect))
  {
    s_color_from_state(color);
    changed = true;
  }
  s_draw_sv(ui, sv_rect, s_color_picker.popup_id ^ 0x53560000u);

  ldk_ui_set_next_size(ui, ldk_ui_px(s_color_picker_width),
      ldk_ui_px(s_color_picker_hue_height));
  ldk_ui_spacer(ui);
  hue_rect = ldk_ui_last_rect(ui);
  if (!readonly && s_drag_hue(ui, hue_rect))
  {
    s_color_from_state(color);
    changed = true;
  }
  s_draw_hue(ui, hue_rect, s_color_picker.popup_id ^ 0x48550000u);

  tabs[0].id = 1u;
  tabs[0].label = "HSV";
  tabs[1].id = 2u;
  tabs[1].label = "RGB";
  s_color_picker.mode = ldk_ui_tab_bar(ui, tabs, 2u, s_color_picker.mode).active_index;

  if (s_color_picker.mode == 0u)
  {
    float hue = s_color_picker.hue;
    float saturation = s_color_picker.saturation;
    float value = s_color_picker.value;
    char hue_text[32];
    char saturation_text[16];
    char value_text[16];
    bool hue_changed;
    bool hsv_changed;

    snprintf(hue_text, sizeof(hue_text), "%.0f deg", hue * 360.0f);
    snprintf(saturation_text, sizeof(saturation_text), "%.0f%%",
        saturation * 100.0f);
    snprintf(value_text, sizeof(value_text), "%.0f%%", value * 100.0f);

    hue_changed = s_slider_row(
        ui, "H", &hue, 0.0f, 1.0f, hue_text, readonly);
    hsv_changed = hue_changed;
    hsv_changed |= s_slider_row(
        ui, "S", &saturation, 0.0f, 1.0f, saturation_text, readonly);
    hsv_changed |=
        s_slider_row(ui, "V", &value, 0.0f, 1.0f, value_text, readonly);

    if (hsv_changed)
    {
      s_color_picker.hue = s_clamp01(hue);
      s_color_picker.saturation = s_clamp01(saturation);
      s_color_picker.value = s_clamp01(value);
      s_color_from_state(color);
      changed = true;
    }
  }
  else
  {
    u8 r;
    u8 g;
    u8 b;
    float rf;
    float gf;
    float bf;
    char r_text[16];
    char g_text[16];
    char b_text[16];
    bool rgb_changed = false;

    s_unpack(*color, &r, &g, &b, NULL);
    rf = (float)r;
    gf = (float)g;
    bf = (float)b;
    snprintf(r_text, sizeof(r_text), "%u", (u32)r);
    snprintf(g_text, sizeof(g_text), "%u", (u32)g);
    snprintf(b_text, sizeof(b_text), "%u", (u32)b);

    rgb_changed |=
        s_slider_row(ui, "R", &rf, 0.0f, 255.0f, r_text, readonly);
    rgb_changed |=
        s_slider_row(ui, "G", &gf, 0.0f, 255.0f, g_text, readonly);
    rgb_changed |=
        s_slider_row(ui, "B", &bf, 0.0f, 255.0f, b_text, readonly);

    if (rgb_changed)
    {
      *color = s_pack((u8)(rf + 0.5f), (u8)(gf + 0.5f), (u8)(bf + 0.5f),
          (u8)(*color & 0xffu));
      s_state_from_color(*color);
      changed = true;
    }
  }

  {
    u8 r;
    u8 g;
    u8 b;
    u8 a;
    float alpha;
    char alpha_text[16];

    s_unpack(*color, &r, &g, &b, &a);
    alpha = (float)a;
    snprintf(alpha_text, sizeof(alpha_text), "%u", (u32)a);
    if (s_slider_row(
            ui, "A", &alpha, 0.0f, 255.0f, alpha_text, readonly))
    {
      *color = s_pack(r, g, b, (u8)(alpha + 0.5f));
      changed = true;
    }
  }

  if (ui->focused_id != s_color_picker.popup_hex_id)
  {
    s_hex_format(s_color_picker.popup_hex, sizeof(s_color_picker.popup_hex),
        *color);
  }

  ldk_ui_begin_disabled(ui, readonly);
  u32 hex_result = ldk_ui_input_box(
      ui, s_color_picker.popup_hex, (u32)sizeof(s_color_picker.popup_hex));
  ldk_ui_end_disabled(ui);
  s_color_picker.popup_hex_id = ui->last_id;

  if (!readonly && (hex_result & LDK_UI_INPUT_BOX_CHANGED) != 0)
  {
    rgba32 parsed;
    if (s_hex_parse(s_color_picker.popup_hex, &parsed))
    {
      *color = parsed;
      s_state_from_color(*color);
      changed = true;
    }
  }

  if ((hex_result & (LDK_UI_INPUT_BOX_COMMITTED)) !=
      0)
  {
    s_hex_format(s_color_picker.popup_hex, sizeof(s_color_picker.popup_hex),
        *color);
  }

  ldk_ui_end_popup(ui);
  return changed;
}

static void s_open(LDKEditorContext *editor, LDKUIId popup_id, rgba32 color)
{
  LDKUIContext *ui = &editor->ui;

  if (s_color_picker.popup_id != 0 && s_color_picker.popup_id != popup_id)
  {
    ldk_ui_close_popup(ui, s_color_picker.popup_id);
  }

  s_color_picker.popup_id = popup_id;
  s_color_picker.popup_hex_id = 0;
  s_color_picker.drag = LDK_EDITOR_COLOR_PICKER_DRAG_NONE;
  s_state_from_color(color);
  s_hex_format(s_color_picker.popup_hex, sizeof(s_color_picker.popup_hex), color);
  ldk_ui_open_popup(ui, popup_id);
}

bool ldki_editor_color_field(
    LDKEditorContext *editor, rgba32 *color, bool readonly)
{
  LDKUIContext *ui;
  LDKUIId swatch_id;
  LDKUIId popup_id;
  rgba32 previous;
  char local_hex[16];
  char *hex_buffer;
  bool editing_this_field;
  u32 hex_result;
  LDKUIId input_id;

  if (!editor || !color)
  {
    return false;
  }

  ui = &editor->ui;
  previous = *color;
  ldk_ui_push_id_cstr(ui, "editor_color_field");

  ldk_ui_set_next_width(ui, ldk_ui_px(s_color_picker_swatch_width));
  ldk_ui_begin_disabled(ui, readonly);
  bool swatch_clicked = ldk_ui_color_view(ui, *color);
  ldk_ui_end_disabled(ui);
  swatch_id = ui->last_id;
  popup_id = swatch_id ^ 0x434f4c52u;
  if (popup_id == 0)
  {
    popup_id = 0x434f4c52u;
  }

  if (swatch_clicked && !readonly)
  {
    ui->focused_id = 0;
    ui->input_box_id = 0;
    s_color_picker.viewer_hex_valid = false;
    s_open(editor, popup_id, *color);
  }

  editing_this_field = s_color_picker.viewer_hex_valid &&
      s_color_picker.viewer_hex_owner == swatch_id &&
      s_color_picker.viewer_hex_id != 0 &&
      ui->focused_id == s_color_picker.viewer_hex_id;

  if (editing_this_field)
  {
    hex_buffer = s_color_picker.viewer_hex;
  }
  else
  {
    s_hex_format(local_hex, sizeof(local_hex), *color);
    hex_buffer = local_hex;
  }

  ldk_ui_set_next_width(ui, ldk_ui_fill());
  ldk_ui_begin_disabled(ui, readonly);
  hex_result = ldk_ui_input_box(ui, hex_buffer, 16u);
  ldk_ui_end_disabled(ui);
  input_id = ui->last_id;

  if (!readonly && (hex_result & LDK_UI_INPUT_BOX_CHANGED) != 0)
  {
    rgba32 parsed;
    if (s_hex_parse(hex_buffer, &parsed))
    {
      *color = parsed;
      if (s_color_picker.popup_id == popup_id &&
          ldk_ui_popup_is_open(ui, popup_id))
      {
        s_state_from_color(*color);
      }
    }
  }

  if (ui->focused_id == input_id)
  {
    if (!editing_this_field)
    {
      snprintf(s_color_picker.viewer_hex, sizeof(s_color_picker.viewer_hex),
          "%s", hex_buffer);
    }
    s_color_picker.viewer_hex_owner = swatch_id;
    s_color_picker.viewer_hex_id = input_id;
    s_color_picker.viewer_hex_valid = true;
  }
  else if (editing_this_field)
  {
    s_color_picker.viewer_hex_valid = false;
  }

  if ((hex_result & (LDK_UI_INPUT_BOX_COMMITTED)) !=
          0 &&
      s_color_picker.viewer_hex_owner == swatch_id)
  {
    s_hex_format(s_color_picker.viewer_hex, sizeof(s_color_picker.viewer_hex),
        *color);
  }

  if (s_color_picker.popup_id == popup_id &&
      ldk_ui_popup_is_open(ui, popup_id))
  {
    (void)s_popup(editor, color, readonly);
  }
  else if (s_color_picker.popup_id == popup_id)
  {
    s_color_picker.drag = LDK_EDITOR_COLOR_PICKER_DRAG_NONE;
  }

  ldk_ui_pop_id(ui);
  return *color != previous;
}
