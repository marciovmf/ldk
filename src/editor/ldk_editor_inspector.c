#include <ldk_material_asset.h>
#include "ldk_editor_internal.h"
#include "ldk_ui_drag_n_drop.h"
#include "module/ldk_ui.h"
#include <ldk_scene.h>
#include <component/ldk_mesh_source.h>
#include <ctype.h>
#include <errno.h>
#include <math.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum
{
  LDK_EDITOR_INSPECTOR_INPUT_CAPACITY = 64,
};

typedef struct LDKEditorInspectorInputState
{
  LDKEntity entity;
  u32 component_type;
  u32 field_offset;
  u32 value_index;
  LDKUIId widget_id;
  char buffer[LDK_EDITOR_INSPECTOR_INPUT_CAPACITY];
  bool valid;
} LDKEditorInspectorInputState;

typedef struct LDKEditorInspectorEulerState
{
  LDKEntity entity;
  u32 component_type;
  u32 field_offset;
  const void *component;
  Vec3 degrees;
  Quat rotation;
  Vec3 edit_original_degrees;
  Quat edit_original_rotation;
  LDKUIId edit_widget_id;
  bool editing;
  bool valid;
} LDKEditorInspectorEulerState;

typedef struct LDKEditorInspectorAreaState
{
  LDKEntity entity;
  u32 collapsed_component_types[LDK_ENTITY_MAX_COMPONENTS];
  u32 collapsed_component_count;
  bool valid;
} LDKEditorInspectorAreaState;

typedef struct LDKEditorInspectorFlagsState
{
  LDKEntity entity;
  LDKUIId input_id;
  char input[16];
  u16 draft_value;
  bool valid;
  bool selector_open;
} LDKEditorInspectorFlagsState;

static LDKEditorInspectorInputState s_editor_inspector_input_state = {0};
static LDKEditorInspectorAreaState s_editor_inspector_area_state = {0};
static LDKEditorInspectorFlagsState s_editor_inspector_flags_state = {0};
static LDKEditorInspectorEulerState s_editor_inspector_euler_state = {0};
static char
    s_editor_inspector_input_buffer[LDK_EDITOR_INSPECTOR_INPUT_CAPACITY] = {0};

static void s_editor_inspector_input_state_clear(void);

static void s_editor_inspector_area_state_sync(LDKEntity entity)
{
  if (s_editor_inspector_area_state.valid &&
      ldki_editor_entity_equal(s_editor_inspector_area_state.entity, entity))
  {
    return;
  }

  memset(&s_editor_inspector_euler_state, 0,
      sizeof(s_editor_inspector_euler_state));
  s_editor_inspector_input_state_clear();
  memset(
      &s_editor_inspector_area_state, 0, sizeof(s_editor_inspector_area_state));
  s_editor_inspector_area_state.entity = entity;
  s_editor_inspector_area_state.valid = true;
}

static i32 s_editor_inspector_collapsed_component_index(u32 component_type)
{
  for (u32 i = 0; i < s_editor_inspector_area_state.collapsed_component_count;
      i++)
  {
    if (s_editor_inspector_area_state.collapsed_component_types[i] ==
        component_type)
    {
      return (i32)i;
    }
  }

  return -1;
}

static bool s_editor_inspector_component_expanded(u32 component_type)
{
  return s_editor_inspector_collapsed_component_index(component_type) < 0;
}

static void s_editor_inspector_component_expanded_set(
    u32 component_type, bool expanded)
{
  i32 collapsed_index =
      s_editor_inspector_collapsed_component_index(component_type);

  if (!expanded)
  {
    if (collapsed_index < 0 &&
        s_editor_inspector_area_state.collapsed_component_count <
            LDK_ENTITY_MAX_COMPONENTS)
    {
      u32 index = s_editor_inspector_area_state.collapsed_component_count++;
      s_editor_inspector_area_state.collapsed_component_types[index] =
          component_type;
    }
    return;
  }

  if (collapsed_index < 0)
  {
    return;
  }

  u32 index = (u32)collapsed_index;
  u32 last_index = --s_editor_inspector_area_state.collapsed_component_count;
  s_editor_inspector_area_state.collapsed_component_types[index] =
      s_editor_inspector_area_state.collapsed_component_types[last_index];
}

static void s_editor_inspector_input_state_clear(void)
{
  memset(&s_editor_inspector_input_state, 0,
      sizeof(s_editor_inspector_input_state));
}

static bool s_editor_inspector_input_state_matches(LDKEntity entity,
    u32 component_type, const LDKComponentFieldMeta *field, u32 value_index)
{
  return s_editor_inspector_input_state.valid &&
         ldki_editor_entity_equal(
             s_editor_inspector_input_state.entity, entity) &&
         s_editor_inspector_input_state.component_type == component_type &&
         s_editor_inspector_input_state.field_offset == field->offset &&
         s_editor_inspector_input_state.value_index == value_index;
}

static u32 s_editor_inspector_input_box(
    LDKUIContext *ui, char *buffer, u32 buffer_size)
{
  return ldk_ui_input_box(ui, buffer, buffer_size);
}

static u32 s_editor_inspector_field_input_box(LDKUIContext *ui,
    LDKEntity entity, u32 component_type, const LDKComponentFieldMeta *field,
    u32 value_index, char *buffer, u32 buffer_size)
{
  bool state_matches;
  char *input_buffer;
  u32 result;

  if (!ui || !field || !buffer || buffer_size == 0)
  {
    return LDK_UI_INPUT_BOX_NONE;
  }

  state_matches = s_editor_inspector_input_state_matches(
      entity, component_type, field, value_index);

  if (state_matches && s_editor_inspector_input_state.widget_id != 0 &&
      ui->focused_id != s_editor_inspector_input_state.widget_id)
  {
    s_editor_inspector_input_state_clear();
    state_matches = false;
  }

  if (state_matches)
  {
    input_buffer = s_editor_inspector_input_state.buffer;
  }
  else
  {
    snprintf(s_editor_inspector_input_buffer,
        sizeof(s_editor_inspector_input_buffer), "%s", buffer);
    input_buffer = s_editor_inspector_input_buffer;
  }

  result = s_editor_inspector_input_box(
      ui, input_buffer, LDK_EDITOR_INSPECTOR_INPUT_CAPACITY);

  if ((result & LDK_UI_INPUT_BOX_CHANGED) != 0)
  {
    snprintf(buffer, buffer_size, "%s", input_buffer);
  }

  if ((result & LDK_UI_INPUT_BOX_CANCELED) != 0)
  {
    s_editor_inspector_input_state_clear();
  }
  else if (ui->focused_id == ui->last_id)
  {
    if (!state_matches)
    {
      s_editor_inspector_input_state.entity = entity;
      s_editor_inspector_input_state.component_type = component_type;
      s_editor_inspector_input_state.field_offset = field->offset;
      s_editor_inspector_input_state.value_index = value_index;
      s_editor_inspector_input_state.widget_id = ui->last_id;
      snprintf(s_editor_inspector_input_state.buffer,
          sizeof(s_editor_inspector_input_state.buffer), "%s", input_buffer);
      s_editor_inspector_input_state.valid = true;
    }
  }

  return result;
}

static bool s_editor_inspector_parse_i32(const char *text, i32 *out_value)
{
  char *end = NULL;
  long value;

  if (!text || !out_value)
  {
    return false;
  }

  value = strtol(text, &end, 10);

  if (end == text)
  {
    return false;
  }

  *out_value = (i32)value;
  return true;
}

static bool s_editor_inspector_parse_u32(const char *text, u32 *out_value)
{
  char *end = NULL;
  unsigned long value;

  if (!text || !out_value)
  {
    return false;
  }

  value = strtoul(text, &end, 10);

  if (end == text)
  {
    return false;
  }

  *out_value = (u32)value;
  return true;
}

static bool s_editor_inspector_parse_float(const char *text, float *out_value)
{
  char *end = NULL;
  float value;

  if (!text || !out_value)
  {
    return false;
  }

  value = strtof(text, &end);

  if (end == text)
  {
    return false;
  }

  *out_value = value;
  return true;
}

static bool s_editor_inspector_parse_flags(const char *text, u16 *out_value)
{
  char *end = NULL;
  unsigned long value;

  if (!text || !out_value)
  {
    return false;
  }

  value = strtoul(text, &end, 0);

  if (end == text || value > 0xfffful)
  {
    return false;
  }

  *out_value = (u16)value;
  return true;
}

static void s_editor_inspector_flags_format(
    char *out, size_t out_size, u16 value)
{
  snprintf(out, out_size, "0x%04X", (u32)value);
}

static bool s_editor_inspector_flag_button(
    LDKUIContext *ui, const char *label, bool active)
{
  rgba32 bg;
  rgba32 bg_hovered;
  rgba32 border;
  rgba32 border_hovered;
  rgba32 text;
  rgba32 text_hovered;
  bool clicked;

  if (!active)
  {
    return ldk_ui_button(ui, label);
  }

  bg = ui->theme.colors[LDK_UI_COLOR_CONTROL_BG];
  bg_hovered = ui->theme.colors[LDK_UI_COLOR_CONTROL_BG_HOVERED];
  border = ui->theme.colors[LDK_UI_COLOR_CONTROL_BORDER];
  border_hovered = ui->theme.colors[LDK_UI_COLOR_CONTROL_BORDER_HOVERED];
  text = ui->theme.colors[LDK_UI_COLOR_CONTROL_TEXT];
  text_hovered = ui->theme.colors[LDK_UI_COLOR_CONTROL_TEXT_HOVERED];

  ui->theme.colors[LDK_UI_COLOR_CONTROL_BG] =
      ui->theme.colors[LDK_UI_COLOR_CONTROL_BG_ACTIVE];
  ui->theme.colors[LDK_UI_COLOR_CONTROL_BG_HOVERED] =
      ui->theme.colors[LDK_UI_COLOR_CONTROL_BG_ACTIVE_HOVERED];
  ui->theme.colors[LDK_UI_COLOR_CONTROL_BORDER] =
      ui->theme.colors[LDK_UI_COLOR_CONTROL_BORDER_ACTIVE];
  ui->theme.colors[LDK_UI_COLOR_CONTROL_BORDER_HOVERED] =
      ui->theme.colors[LDK_UI_COLOR_CONTROL_BORDER_ACTIVE_HOVERED];
  ui->theme.colors[LDK_UI_COLOR_CONTROL_TEXT] =
      ui->theme.colors[LDK_UI_COLOR_CONTROL_TEXT_ACTIVE];
  ui->theme.colors[LDK_UI_COLOR_CONTROL_TEXT_HOVERED] =
      ui->theme.colors[LDK_UI_COLOR_CONTROL_TEXT_ACTIVE_HOVERED];

  clicked = ldk_ui_button(ui, label);

  ui->theme.colors[LDK_UI_COLOR_CONTROL_BG] = bg;
  ui->theme.colors[LDK_UI_COLOR_CONTROL_BG_HOVERED] = bg_hovered;
  ui->theme.colors[LDK_UI_COLOR_CONTROL_BORDER] = border;
  ui->theme.colors[LDK_UI_COLOR_CONTROL_BORDER_HOVERED] = border_hovered;
  ui->theme.colors[LDK_UI_COLOR_CONTROL_TEXT] = text;
  ui->theme.colors[LDK_UI_COLOR_CONTROL_TEXT_HOVERED] = text_hovered;

  return clicked;
}

static void s_editor_inspector_flags_state_sync(LDKEntity entity, u16 value)
{
  if (s_editor_inspector_flags_state.valid &&
      ldki_editor_entity_equal(s_editor_inspector_flags_state.entity, entity))
  {
    return;
  }

  memset(&s_editor_inspector_flags_state, 0,
      sizeof(s_editor_inspector_flags_state));
  s_editor_inspector_flags_state.entity = entity;
  s_editor_inspector_flags_state.draft_value = value;
  s_editor_inspector_flags_state.valid = true;
  s_editor_inspector_flags_format(s_editor_inspector_flags_state.input,
      sizeof(s_editor_inspector_flags_state.input), value);
}

static void s_editor_inspector_flags_selector(
    LDKEditorContext *editor, LDKECS *ecs, LDKEntity entity)
{
  LDKUIContext *ui = &editor->ui;

  ldk_ui_push_id_cstr(ui, "entity_flags_selector");

  for (u32 row = 0; row < 8; row++)
  {
    ldk_ui_begin_horizontal(ui);

    for (u32 column = 0; column < 2; column++)
    {
      u32 bit = row * 2u + column;
      u16 mask = (u16)(1u << bit);
      char label[LDK_EDITOR_TAG_NAME_CAPACITY + 16];

      snprintf(label, sizeof(label), "%u: %s", bit,
          ldki_editor_tag_name_get(editor, bit));

      ldk_ui_push_id_u32(ui, bit);
      ldk_ui_set_next_width(ui, ldk_ui_fill());
      if (s_editor_inspector_flag_button(ui, label,
              (s_editor_inspector_flags_state.draft_value & mask) != 0))
      {
        s_editor_inspector_flags_state.draft_value ^= mask;
        s_editor_inspector_flags_format(s_editor_inspector_flags_state.input,
            sizeof(s_editor_inspector_flags_state.input),
            s_editor_inspector_flags_state.draft_value);
      }
      ldk_ui_pop_id(ui);
    }

    ldk_ui_end_horizontal(ui);
  }

  ldk_ui_begin_horizontal(ui);
  ldk_ui_spacer(ui);
  ldk_ui_set_next_width(ui, ldk_ui_px(80.0f));
  bool cancel = ldk_ui_button(ui, "Cancel");
  ldk_ui_set_next_width(ui, ldk_ui_px(80.0f));
  bool ok = ldk_ui_button(ui, "OK");
  ldk_ui_end_horizontal(ui);

  if (cancel)
  {
    s_editor_inspector_flags_state.selector_open = false;
    s_editor_inspector_flags_format(s_editor_inspector_flags_state.input,
        sizeof(s_editor_inspector_flags_state.input),
        ldk_entity_flags_get(&ecs->entity, entity));
  }
  else if (ok)
  {
    ldk_entity_flags_set(&ecs->entity, entity,
        s_editor_inspector_flags_state.draft_value);
    s_editor_inspector_flags_state.selector_open = false;
    s_editor_inspector_flags_format(s_editor_inspector_flags_state.input,
        sizeof(s_editor_inspector_flags_state.input),
        s_editor_inspector_flags_state.draft_value);
  }

  ldk_ui_pop_id(ui);
}

static void s_editor_inspector_flags_draw(
    LDKEditorContext *editor, LDKECS *ecs, LDKEntity entity)
{
  LDKUIContext *ui = &editor->ui;
  u16 flags = ldk_entity_flags_get(&ecs->entity, entity);
  u16 shown_value;
  u32 result;

  s_editor_inspector_flags_state_sync(entity, flags);
  shown_value = s_editor_inspector_flags_state.selector_open
      ? s_editor_inspector_flags_state.draft_value
      : flags;

  if (s_editor_inspector_flags_state.input_id == 0 ||
      ui->focused_id != s_editor_inspector_flags_state.input_id)
  {
    s_editor_inspector_flags_format(s_editor_inspector_flags_state.input,
        sizeof(s_editor_inspector_flags_state.input), shown_value);
  }

  ldk_ui_push_id_cstr(ui, "entity_flags");
  ldk_ui_push_id_u32(ui, entity.index);
  ldk_ui_push_id_u32(ui, entity.version);
  ldk_ui_set_next_height(ui, ldk_ui_px(LDK_UI_DEFAULT_CONTROL_HEIGHT));
  ldk_ui_begin_horizontal(ui);
  ldk_ui_set_next_width(ui, ldk_ui_px(110.0f));
  ldk_ui_label(ui, "Flags");

  result = ldk_ui_input_box(ui, s_editor_inspector_flags_state.input,
      (u32)sizeof(s_editor_inspector_flags_state.input));
  s_editor_inspector_flags_state.input_id = ui->last_id;

  if ((result & LDK_UI_INPUT_BOX_CHANGED) != 0)
  {
    u16 parsed;

    if (s_editor_inspector_parse_flags(
            s_editor_inspector_flags_state.input, &parsed))
    {
      if (s_editor_inspector_flags_state.selector_open)
      {
        s_editor_inspector_flags_state.draft_value = parsed;
      }
      else
      {
        ldk_entity_flags_set(&ecs->entity, entity, parsed);
      }
    }
  }

  if ((result & (LDK_UI_INPUT_BOX_COMMITTED | LDK_UI_INPUT_BOX_CANCELED)) != 0)
  {
    u16 value = s_editor_inspector_flags_state.selector_open
        ? s_editor_inspector_flags_state.draft_value
        : ldk_entity_flags_get(&ecs->entity, entity);
    s_editor_inspector_flags_format(s_editor_inspector_flags_state.input,
        sizeof(s_editor_inspector_flags_state.input), value);
  }

  ldk_ui_set_next_width(ui, ldk_ui_px(28.0f));
  if (ldk_ui_button(ui, "..."))
  {
    if (s_editor_inspector_flags_state.selector_open)
    {
      s_editor_inspector_flags_state.selector_open = false;
      s_editor_inspector_flags_format(s_editor_inspector_flags_state.input,
          sizeof(s_editor_inspector_flags_state.input),
          ldk_entity_flags_get(&ecs->entity, entity));
    }
    else
    {
      s_editor_inspector_flags_state.draft_value =
          ldk_entity_flags_get(&ecs->entity, entity);
      s_editor_inspector_flags_state.selector_open = true;
      s_editor_inspector_flags_format(s_editor_inspector_flags_state.input,
          sizeof(s_editor_inspector_flags_state.input),
          s_editor_inspector_flags_state.draft_value);
    }
  }
  ldk_ui_end_horizontal(ui);
  ldk_ui_pop_id(ui);
  ldk_ui_pop_id(ui);
  ldk_ui_pop_id(ui);

  if (s_editor_inspector_flags_state.selector_open)
  {
    s_editor_inspector_flags_selector(editor, ecs, entity);
  }
}

static void s_editor_inspector_field_value_format(char *out, size_t out_size,
    const LDKComponentFieldMeta *field, const void *value)
{
  switch (field->type)
  {
  case LDK_FIELD_BOOL:
    snprintf(out, out_size, "%s", *(const bool *)value ? "true" : "false");
    break;
  case LDK_FIELD_I32:
  case LDK_FIELD_ENUM:
    snprintf(out, out_size, "%d", *(const i32 *)value);
    break;
  case LDK_FIELD_U32:
    snprintf(out, out_size, "%u", *(const u32 *)value);
    break;
  case LDK_FIELD_FLOAT:
    snprintf(out, out_size, "%.9g", (double)*(const float *)value);
    break;
  case LDK_FIELD_VEC2:
  {
    const Vec2 *v = (const Vec2 *)value;
    snprintf(out, out_size, "%.9g, %.9g", (double)v->x, (double)v->y);
    break;
  }
  case LDK_FIELD_VEC3:
  {
    const Vec3 *v = (const Vec3 *)value;
    snprintf(out, out_size, "%.9g, %.9g, %.9g", (double)v->x, (double)v->y,
        (double)v->z);
    break;
  }
  case LDK_FIELD_VEC4:
  {
    const Vec4 *v = (const Vec4 *)value;
    snprintf(out, out_size, "%.9g, %.9g, %.9g, %.9g", (double)v->x,
        (double)v->y, (double)v->z, (double)v->w);
    break;
  }
  case LDK_FIELD_QUAT:
  {
    const Quat *q = (const Quat *)value;
    snprintf(out, out_size, "%.9g, %.9g, %.9g, %.9g", (double)q->x,
        (double)q->y, (double)q->z, (double)q->w);
    break;
  }
  case LDK_FIELD_MAT4:
    snprintf(out, out_size, "<mat4>");
    break;
  case LDK_FIELD_ENTITY:
  {
    LDKEntity entity;
    memcpy(&entity, value, sizeof(entity));
    // snprintf(out, out_size, "0x%016" PRIx64, s_editor_entity_id(entity));
    break;
  }
  case LDK_FIELD_ASSET_MESH:
    snprintf(out, out_size, "<asset mesh>");
    break;
  case LDK_FIELD_RESOURCE_MESH:
    snprintf(out, out_size, "<resource mesh>");
    break;
  default:
    snprintf(out, out_size, "<unsupported>");
    break;
  }
}

static bool s_editor_inspector_float_input(LDKUIContext *ui, LDKEntity entity,
    u32 component_type, const LDKComponentFieldMeta *field, u32 value_index,
    float *value, bool readonly)
{
  char buffer[LDK_EDITOR_INSPECTOR_INPUT_CAPACITY];
  float original;
  float parsed;
  u32 result;

  if (!value)
  {
    return false;
  }

  original = *value;
  snprintf(buffer, sizeof(buffer), "%.9g", (double)original);

  ldk_ui_begin_disabled(ui, readonly);
  result = s_editor_inspector_field_input_box(ui, entity, component_type, field,
      value_index, buffer, (u32)sizeof(buffer));
  ldk_ui_end_disabled(ui);

  if (readonly || (result & LDK_UI_INPUT_BOX_CHANGED) == 0)
  {
    return false;
  }

  if (!s_editor_inspector_parse_float(buffer, &parsed))
  {
    *value = original;
    return false;
  }

  *value = parsed;
  return true;
}

static bool s_editor_inspector_transform_field_apply(LDKEntity entity,
    u32 component_type, const LDKComponentFieldMeta *field, const void *value)
{
  if (component_type != LDK_COMPONENT_TYPE_TRANSFORM || !field || !value)
  {
    return false;
  }

  if (field->offset == offsetof(LDKTransform, local_position))
  {
    ldk_transform_set_local_position(entity, *(const Vec3 *)value);
    return true;
  }

  if (field->offset == offsetof(LDKTransform, local_rotation))
  {
    ldk_transform_set_local_rotation(entity, *(const Quat *)value);
    return true;
  }

  if (field->offset == offsetof(LDKTransform, local_scale))
  {
    ldk_transform_set_local_scale(entity, *(const Vec3 *)value);
    return true;
  }

  return false;
}

static bool s_editor_inspector_euler_quat_equal(Quat a, Quat b)
{
  return a.x == b.x && a.y == b.y && a.z == b.z && a.w == b.w;
}

static bool s_editor_inspector_euler_quat_finite(Quat q)
{
  return isfinite(q.x) && isfinite(q.y) && isfinite(q.z) && isfinite(q.w);
}

static Vec3 s_editor_inspector_euler_radians(Quat rotation)
{
  double x = (double)rotation.x;
  double y = (double)rotation.y;
  double z = (double)rotation.z;
  double w = (double)rotation.w;
  double length = sqrt(x * x + y * y + z * z + w * w);
  double sinp;

  if (!isfinite(length) || length <= (double)STDXM_EPS)
  {
    return vec3_make(0.0f, 0.0f, 0.0f);
  }

  x /= length;
  y /= length;
  z /= length;
  w /= length;
  sinp = 2.0 * (w * y - z * x);
  sinp = fmax(-1.0, fmin(1.0, sinp));

  if (1.0 - fabs(sinp) > 1e-10)
  {
    return quat_to_euler_xyz(quat_norm(rotation));
  }

  return vec3_make((float)atan2(copysign(1.0, sinp) *
                                    2.0 * (x * y - w * z),
                                1.0 - 2.0 * (x * x + z * z)),
      (float)copysign((double)STDXM_PI * 0.5, sinp), 0.0f);
}

static Vec3 s_editor_inspector_euler_degrees(Quat rotation)
{
  Vec3 radians = s_editor_inspector_euler_radians(rotation);
  return vec3_make(rad_to_deg(radians.x), rad_to_deg(radians.y),
      rad_to_deg(radians.z));
}

static Quat s_editor_inspector_euler_from_degrees(Vec3 degrees)
{
  float x = deg_to_rad(remainderf(degrees.x, 360.0f));
  float y = deg_to_rad(remainderf(degrees.y, 360.0f));
  float z = deg_to_rad(remainderf(degrees.z, 360.0f));
  Quat qx = quat_axis_angle(vec3_make(1.0f, 0.0f, 0.0f), x);
  Quat qy = quat_axis_angle(vec3_make(0.0f, 1.0f, 0.0f), y);
  Quat qz = quat_axis_angle(vec3_make(0.0f, 0.0f, 1.0f), z);

  return quat_norm(quat_mul(qz, quat_mul(qy, qx)));
}

static float s_editor_inspector_euler_axis_get(Vec3 degrees, u32 axis)
{
  if (axis == 0)
  {
    return degrees.x;
  }
  if (axis == 1)
  {
    return degrees.y;
  }
  return degrees.z;
}

static void s_editor_inspector_euler_axis_set(
    Vec3 *degrees, u32 axis, float value)
{
  if (axis == 0)
  {
    degrees->x = value;
  }
  else if (axis == 1)
  {
    degrees->y = value;
  }
  else
  {
    degrees->z = value;
  }
}

static bool s_editor_inspector_euler_parse_float(
    const char *text, float *out_value)
{
  char *end = NULL;
  float value;

  if (!text || !out_value)
  {
    return false;
  }

  errno = 0;
  value = strtof(text, &end);
  if (end == text || errno == ERANGE || !isfinite(value))
  {
    return false;
  }

  while (isspace((unsigned char)*end))
  {
    end++;
  }

  if (*end != '\0')
  {
    return false;
  }

  *out_value = value;
  return true;
}

static bool s_editor_inspector_euler_state_matches(
    const LDKEditorInspectorEulerState *state, LDKEntity entity,
    u32 component_type, const LDKComponentFieldMeta *field,
    const void *component)
{
  return state->valid && ldki_editor_entity_equal(state->entity, entity) &&
         state->component_type == component_type &&
         state->field_offset == field->offset &&
         state->component == component;
}

static void s_editor_inspector_euler_state_init(
    LDKEditorInspectorEulerState *state, LDKEntity entity,
    u32 component_type, const LDKComponentFieldMeta *field,
    const void *component, Quat rotation)
{
  memset(state, 0, sizeof(*state));
  state->entity = entity;
  state->component_type = component_type;
  state->field_offset = field->offset;
  state->component = component;
  state->rotation = rotation;
  state->degrees = s_editor_inspector_euler_degrees(rotation);
  state->valid = true;
}

static bool s_editor_inspector_euler_rotation_apply(LDKEntity entity,
    u32 component_type, const LDKComponentFieldMeta *field,
    void *field_value, Quat rotation)
{
  if (component_type == LDK_COMPONENT_TYPE_TRANSFORM &&
      field->offset == offsetof(LDKTransform, local_rotation))
  {
    return ldk_transform_set_local_rotation(entity, rotation);
  }

  *(Quat *)field_value = rotation;
  return true;
}

static void s_editor_inspector_euler_field_draw(LDKUIContext *ui,
    LDKEntity entity, u32 component_type,
    const LDKComponentFieldMeta *field, void *component,
    void *field_value, bool readonly)
{
  static const char *const axis_names[3] = {"X", "Y", "Z"};
  LDKEditorInspectorEulerState local_state;
  LDKEditorInspectorEulerState *state = &local_state;
  Quat source = *(const Quat *)field_value;

  if (s_editor_inspector_euler_state_matches(
          &s_editor_inspector_euler_state, entity, component_type, field,
          component))
  {
    state = &s_editor_inspector_euler_state;
    if (!s_editor_inspector_euler_quat_equal(source, state->rotation))
    {
      /* A gizmo, script, or another editor operation changed the source. */
      s_editor_inspector_euler_state_init(
          state, entity, component_type, field, component, source);
      if (s_editor_inspector_input_state_matches(
              entity, component_type, field,
              s_editor_inspector_input_state.value_index))
      {
        s_editor_inspector_input_state_clear();
      }
    }
  }
  else
  {
    s_editor_inspector_euler_state_init(
        state, entity, component_type, field, component, source);
  }

  if (readonly || (state->editing &&
                      ui->focused_id != state->edit_widget_id))
  {
    state->editing = false;
  }

  for (u32 axis = 0; axis < 3; axis++)
  {
    char buffer[LDK_EDITOR_INSPECTOR_INPUT_CAPACITY];
    float parsed;
    u32 result;
    bool focused;

    ldk_ui_push_id_u32(ui, axis);
    ldk_ui_set_next_width(ui, ldk_ui_px(12.0f));
    ldk_ui_label(ui, axis_names[axis]);
    ldk_ui_set_next_width(ui, ldk_ui_px(60.0f));
    snprintf(buffer, sizeof(buffer), "%.9g",
        (double)s_editor_inspector_euler_axis_get(state->degrees, axis));

    ldk_ui_begin_disabled(ui, readonly);
    result = s_editor_inspector_field_input_box(ui, entity, component_type,
        field, axis, buffer, (u32)sizeof(buffer));
    ldk_ui_end_disabled(ui);

    focused = ui->focused_id == ui->last_id;
    if (!readonly &&
        (focused || (result & LDK_UI_INPUT_BOX_CHANGED) != 0))
    {
      if (state != &s_editor_inspector_euler_state)
      {
        s_editor_inspector_euler_state = *state;
        state = &s_editor_inspector_euler_state;
      }

      if (!state->editing || state->edit_widget_id != ui->last_id)
      {
        state->edit_original_degrees = state->degrees;
        state->edit_original_rotation = state->rotation;
        state->edit_widget_id = ui->last_id;
        state->editing = true;
      }
    }

    if (!readonly && (result & LDK_UI_INPUT_BOX_CHANGED) != 0 &&
        s_editor_inspector_euler_parse_float(buffer, &parsed) &&
        parsed != s_editor_inspector_euler_axis_get(state->degrees, axis))
    {
      Vec3 degrees = state->degrees;
      Quat rotation;

      s_editor_inspector_euler_axis_set(&degrees, axis, parsed);
      rotation = s_editor_inspector_euler_from_degrees(degrees);
      if (s_editor_inspector_euler_quat_finite(rotation) &&
          (s_editor_inspector_euler_quat_equal(
               *(const Quat *)field_value, rotation) ||
              s_editor_inspector_euler_rotation_apply(
                  entity, component_type, field, field_value, rotation)))
      {
        state->degrees = degrees;
        state->rotation = *(const Quat *)field_value;
      }
    }

    if ((result & LDK_UI_INPUT_BOX_CANCELED) != 0 && state->editing &&
        state->edit_widget_id == ui->last_id)
    {
      if (s_editor_inspector_euler_quat_equal(
              *(const Quat *)field_value, state->rotation) &&
          (s_editor_inspector_euler_quat_equal(
               *(const Quat *)field_value, state->edit_original_rotation) ||
              s_editor_inspector_euler_rotation_apply(entity, component_type,
                  field, field_value, state->edit_original_rotation)))
      {
        state->degrees = state->edit_original_degrees;
        state->rotation = *(const Quat *)field_value;
      }
      state->editing = false;
    }
    else if ((result & LDK_UI_INPUT_BOX_COMMITTED) != 0 &&
             state->editing && state->edit_widget_id == ui->last_id)
    {
      state->editing = false;
      s_editor_inspector_input_state_clear();
    }

    ldk_ui_pop_id(ui);
  }
}

static void s_editor_inspector_field_draw(LDKUIContext *ui, LDKEntity entity,
    u32 component_type, const LDKComponentMeta *meta,
    const LDKComponentFieldMeta *field, void *component)
{
  char value_text[128];
  u8 *field_value;
  bool readonly;

  if (!ui || !meta || !field || !component || field->offset >= meta->size)
  {
    return;
  }

  field_value = (u8 *)component + field->offset;
  readonly = (field->flags & LDK_FIELD_FLAG_READONLY) != 0;

  ldk_ui_push_id_cstr(ui, field->name);
  ldk_ui_set_next_height(ui, ldk_ui_px(LDK_UI_DEFAULT_CONTROL_HEIGHT));
  ldk_ui_begin_horizontal(ui);
  ldk_ui_set_next_width(ui, ldk_ui_px(110.0f));
  ldk_ui_label(ui, field->name);

  switch (field->type)
  {
  case LDK_FIELD_BOOL:
  {
    bool value = *(bool *)field_value;

    ldk_ui_begin_disabled(ui, readonly);
    value = ldk_ui_toggle(ui, value);
    ldk_ui_end_disabled(ui);

    if (!readonly)
    {
      *(bool *)field_value = value;
    }
    break;
  }

  case LDK_FIELD_I32:
  case LDK_FIELD_ENUM:
  {
    char buffer[LDK_EDITOR_INSPECTOR_INPUT_CAPACITY];
    i32 original = *(i32 *)field_value;
    i32 parsed;
    u32 result;

    snprintf(buffer, sizeof(buffer), "%d", original);
    ldk_ui_begin_disabled(ui, readonly);
    result = s_editor_inspector_field_input_box(
        ui, entity, component_type, field, 0, buffer, (u32)sizeof(buffer));
    ldk_ui_end_disabled(ui);

    if (!readonly && (result & LDK_UI_INPUT_BOX_CHANGED) != 0)
    {
      if (s_editor_inspector_parse_i32(buffer, &parsed))
      {
        *(i32 *)field_value = parsed;
      }
      else
      {
        *(i32 *)field_value = original;
      }
    }
    break;
  }

  case LDK_FIELD_U32:
  {
    char buffer[LDK_EDITOR_INSPECTOR_INPUT_CAPACITY];
    u32 original = *(u32 *)field_value;
    u32 parsed;
    u32 result;

    snprintf(buffer, sizeof(buffer), "%u", original);
    ldk_ui_begin_disabled(ui, readonly);
    result = s_editor_inspector_field_input_box(
        ui, entity, component_type, field, 0, buffer, (u32)sizeof(buffer));
    ldk_ui_end_disabled(ui);

    if (!readonly && (result & LDK_UI_INPUT_BOX_CHANGED) != 0)
    {
      if (s_editor_inspector_parse_u32(buffer, &parsed))
      {
        *(u32 *)field_value = parsed;
      }
      else
      {
        *(u32 *)field_value = original;
      }
    }
    break;
  }

  case LDK_FIELD_FLOAT:
  {
    float value = *(float *)field_value;

    if (field->widget == LDK_FIELD_WIDGET_SLIDER &&
        field->min_value < field->max_value)
    {
      ldk_ui_begin_disabled(ui, readonly);
      value = ldk_ui_slider(ui, value, field->min_value, field->max_value);
      ldk_ui_end_disabled(ui);

      if (!readonly)
      {
        *(float *)field_value = value;
      }
    }
    else
    {
      if (s_editor_inspector_float_input(
              ui, entity, component_type, field, 0, &value, readonly))
      {
        *(float *)field_value = value;
      }
    }
    break;
  }

  case LDK_FIELD_VEC2:
  {
    Vec2 value = *(Vec2 *)field_value;

    bool changed = false;

    changed |= s_editor_inspector_float_input(
        ui, entity, component_type, field, 0, &value.x, readonly);
    changed |= s_editor_inspector_float_input(
        ui, entity, component_type, field, 1, &value.y, readonly);

    if (changed)
    {
      *(Vec2 *)field_value = value;
    }
    break;
  }

  case LDK_FIELD_VEC3:
  {
    Vec3 value = *(Vec3 *)field_value;

    bool changed = false;

    changed |= s_editor_inspector_float_input(
        ui, entity, component_type, field, 0, &value.x, readonly);
    changed |= s_editor_inspector_float_input(
        ui, entity, component_type, field, 1, &value.y, readonly);
    changed |= s_editor_inspector_float_input(
        ui, entity, component_type, field, 2, &value.z, readonly);

    if (changed && !s_editor_inspector_transform_field_apply(
                       entity, component_type, field, &value))
    {
      *(Vec3 *)field_value = value;
    }
    break;
  }

  case LDK_FIELD_VEC4:
  {
    Vec4 value = *(Vec4 *)field_value;

    bool changed = false;

    changed |= s_editor_inspector_float_input(
        ui, entity, component_type, field, 0, &value.x, readonly);
    changed |= s_editor_inspector_float_input(
        ui, entity, component_type, field, 1, &value.y, readonly);
    changed |= s_editor_inspector_float_input(
        ui, entity, component_type, field, 2, &value.z, readonly);
    changed |= s_editor_inspector_float_input(
        ui, entity, component_type, field, 3, &value.w, readonly);

    if (changed)
    {
      *(Vec4 *)field_value = value;
    }
    break;
  }

  case LDK_FIELD_QUAT:
  {

    printf("WIDGET TYPE = %d\n", field->widget);
    if (field->widget == LDK_FIELD_WIDGET_EULER)
    {
      s_editor_inspector_euler_field_draw(ui, entity, component_type, field,
          component, field_value, readonly);
      break;
    }

    Quat value = *(Quat *)field_value;

    bool changed = false;

    changed |= s_editor_inspector_float_input(
        ui, entity, component_type, field, 0, &value.x, readonly);
    changed |= s_editor_inspector_float_input(
        ui, entity, component_type, field, 1, &value.y, readonly);
    changed |= s_editor_inspector_float_input(
        ui, entity, component_type, field, 2, &value.z, readonly);
    changed |= s_editor_inspector_float_input(
        ui, entity, component_type, field, 3, &value.w, readonly);

    if (changed && !s_editor_inspector_transform_field_apply(
                       entity, component_type, field, &value))
    {
      *(Quat *)field_value = value;
    }
    break;
  }

  case LDK_FIELD_MAT4:
  case LDK_FIELD_ENTITY:
  case LDK_FIELD_ASSET_MESH:
  case LDK_FIELD_RESOURCE_MESH:
  default:
    s_editor_inspector_field_value_format(
        value_text, sizeof(value_text), field, field_value);
    ldk_ui_label(ui, value_text);
    break;
  }

  ldk_ui_end_horizontal(ui);
  ldk_ui_pop_id(ui);
}

static void s_editor_material_row_begin(LDKUIContext *ui, const char *title)
{
  ldk_ui_set_next_height(ui, ldk_ui_px(LDK_UI_DEFAULT_CONTROL_HEIGHT));
  ldk_ui_begin_horizontal(ui);
  ldk_ui_set_next_width(ui, ldk_ui_px(110.0f));
  ldk_ui_label(ui, title);
}

static void s_editor_material_diagnostic(const char *message, void *user)
{
  ldki_editor_console_append(user, LDK_EDITOR_CONSOLE_ENTRY_ERROR, message);
}

static bool s_editor_material_asset_field(LDKEditorContext *editor,
    LDKMeshSource *mesh, const LDKMaterialIOContext *context)
{
  LDKUIContext *ui = &editor->ui;
  LDKAssetHandle handle = {mesh->material_asset.h};
  const LDKAssetInfo *info = mesh->material_revision
      ? ldk_asset_get_info_const(context->assets, handle) : NULL;
  XFSPath path = {0};
  char display[sizeof(path.buf)];
  snprintf(display, sizeof(display), "%s", info ? info->asset_path.buf : "");
  s_editor_material_row_begin(ui, "Material");
  ldk_ui_begin_disabled(ui, true);
  ldk_ui_input_box(ui, display, sizeof(display));
  LDKUIRect target = ldk_ui_last_bounding_rect(ui);
  ldk_ui_end_disabled(ui);
  bool assign = false;
  if (ui->mouse && ui->active_id && ui->current_window &&
      ui->hovered_window_id == ui->current_window->id &&
      ldk_os_mouse_button_up((LDKMouseState *)ui->mouse, LDK_MOUSE_BUTTON_LEFT))
  {
    LDKPoint cursor = ldk_os_mouse_cursor((LDKMouseState *)ui->mouse);
    if (ldk_rectf_contains(&target, (float)cursor.x, (float)cursor.y) &&
        ldk_rectf_contains(&ui->clip_rect, (float)cursor.x, (float)cursor.y))
    {
      u32 type = 0;
      assign = ldk_ui_drag_n_drop_payload_get_and_remove(&type, &path) &&
          type == LDK_EDITOR_DRAG_N_DROP_PAYLOAD_FILE_PATH;
    }
  }
  ldk_ui_set_next_width(ui, ldk_ui_px(28.0f));
  if (ldk_ui_button(ui, "..."))
    assign = ldk_os_dialog_show_open_file(editor->window, "Choose material",
        "Materials\0*.tml\0\0", path.buf, sizeof(path.buf));
  ldk_ui_end_horizontal(ui);
  if (assign)
  {
    LDKMaterialIOResult result;
    LDKAssetMaterial asset = ldk_asset_manager_material_load_shared(
        context, path.buf, &result);
    if (x_handle_is_null(asset.h))
      ldki_editor_log_error(editor, result.error);
    else
      return ldk_mesh_source_set_material_asset(mesh, context->assets, asset);
  }
  return false;
}

static void s_editor_inspector_material(
    LDKEditorContext *editor, LDKMeshSource *mesh)
{
  static const char *const names[] = {
      "Textured Unlit", "Textured", "Vertex Color Unlit", "Vertex Color"};
  static const LDKMaterialType types[] = {
      LDK_MATERIAL_TYPE_TEXTURED_UNLIT, LDK_MATERIAL_TYPE_TEXTURED,
      LDK_MATERIAL_TYPE_VERTEX_COLOR_UNLIT, LDK_MATERIAL_TYPE_VERTEX_COLOR};
  LDKUIContext *ui = &editor->ui;
  LDKMaterialIOContext context = {0};
  context.assets = ldk_module_get(LDK_MODULE_ASSET_MANAGER);
  context.runtree_path = editor->project.run_root_path;
  context.diagnostic = s_editor_material_diagnostic;
  context.user = editor;
  ldk_mesh_source_material_sync(mesh, context.assets);
  ldk_ui_push_id_cstr(ui, "material");
  ldk_ui_horizontal_line(ui);
  s_editor_material_asset_field(editor, mesh, &context);
  const LDKAssetMaterialData *bound = mesh->material_revision
      ? ldk_asset_manager_material_get_const(context.assets, mesh->material_asset)
      : NULL;
  if (bound && bound->is_missing)
  {
    s_editor_material_row_begin(ui, "Status");
    ldk_ui_label(ui, "Missing material (magenta checker)");
    ldk_ui_end_horizontal(ui);
    ldk_ui_pop_id(ui);
    return;
  }
  LDKMaterialDesc desc = mesh->material;
  u32 selected = 3;
  for (u32 i = 0; i < 4; ++i)
  {
    if (desc.type == types[i])
      selected = i;
  }

  s_editor_material_row_begin(ui, "Type");
  u32 next = ldk_ui_combo_box(ui, names, 4, selected);
  ldk_ui_end_horizontal(ui);
  if (next < 4 && next != selected)
  {
    ldk_material_desc_defaults(types[next], &desc);
  }

  bool textured = desc.type == LDK_MATERIAL_TYPE_TEXTURED ||
      desc.type == LDK_MATERIAL_TYPE_TEXTURED_UNLIT;
  rgba32 *color = textured ? &desc.args.textured.color
                          : &desc.args.vertex_color.color;
  char color_label[40];
  s_editor_material_row_begin(ui, "Tint");
  snprintf(color_label, sizeof(color_label), "#%08X...", (u32)*color);
  if (ldk_ui_color_view(ui, *color))
  {
    ldk_os_dialog_color_picker_show(editor->window, color);
  }
  ldk_ui_end_horizontal(ui);
  s_editor_material_row_begin(ui, "Alpha");
  u32 alpha = (u32)(ldk_ui_slider(ui, (float)(*color & 255), 0, 255) + 0.5f);
  *color = (*color & 0xffffff00u) | alpha;
  ldk_ui_end_horizontal(ui);

  if (textured)
  {
    LDKAssetManager *assets = ldk_module_get(LDK_MODULE_ASSET_MANAGER);
    LDKAssetHandle asset = {desc.args.textured.texture.h};
    const LDKAssetInfo *info = ldk_asset_get_info_const(assets, asset);
    XFSPath path = {0};
    char display_path[sizeof(path.buf)];
    snprintf(display_path, sizeof(display_path), "%s",
        info ? (info->asset_path.length ? info->asset_path.buf
                                       : "Generated image") : "");
    s_editor_material_row_begin(ui, "Texture");
    ldk_ui_begin_disabled(ui, true);
    ldk_ui_input_box(ui, display_path, sizeof(display_path));
    LDKUIRect target = ldk_ui_last_bounding_rect(ui);
    ldk_ui_end_disabled(ui);
    bool assign = false;
    /* Disabled text is read-only. Resolve its drop explicitly because disabled
     * input controls do not participate in the UI's hot-id hit testing. */
    if (ui->mouse && ui->active_id && ui->current_window &&
        ui->hovered_window_id == ui->current_window->id &&
        ldk_os_mouse_button_up(
            (LDKMouseState *)ui->mouse, LDK_MOUSE_BUTTON_LEFT))
    {
      LDKPoint cursor = ldk_os_mouse_cursor((LDKMouseState *)ui->mouse);
      if (ldk_rectf_contains(&target, (float)cursor.x, (float)cursor.y) &&
          ldk_rectf_contains(&ui->clip_rect, (float)cursor.x, (float)cursor.y))
      {
        u32 payload_type = 0;
        assign = ldk_ui_drag_n_drop_payload_get_and_remove(&payload_type, &path) &&
            payload_type == LDK_EDITOR_DRAG_N_DROP_PAYLOAD_FILE_PATH;
      }
    }
    ldk_ui_set_next_width(ui, ldk_ui_px(28.0f));
    if (ldk_ui_button(ui, "..."))
    {
      assign = ldk_os_dialog_show_open_file(editor->window, "Choose material image",
          "Images\0*.png;*.jpg;*.jpeg;*.bmp;*.tga\0\0",
          path.buf, sizeof(path.buf));
    }
    ldk_ui_end_horizontal(ui);
    if (assign)
    {
        XFSPath normalized = {0};
        XFSPath root = editor->project.run_root_path;
        XFSPath relative = {0};
        x_fs_path_set(&normalized, path.buf);
        x_fs_path_normalize(&normalized);
        x_fs_path_normalize(&root);
        if (!editor->project.loaded || !root.length ||
            !x_fs_path_common_prefix(root.buf, normalized.buf, &relative) ||
            !relative.length || strcmp(relative.buf, ".") == 0)
        {
          ldk_os_dialog_show_error(editor->window, "Material image",
              "Choose an image inside the project's runtree folder.");
        }
        else
        {
          LDKAssetImage image =
              ldk_asset_manager_image_load_shared(assets, normalized.buf);
          if (x_handle_is_null(image.h))
            ldk_os_dialog_show_error(editor->window, "Material image",
                "The selected image could not be loaded.");
          else
            desc.args.textured.texture = image;
        }
    }
  }

  if (mesh->material_revision)
  {
    ldk_asset_manager_material_update(context.assets, mesh->material_asset, &desc);
    ldk_mesh_source_material_sync(mesh, context.assets);
  }
  else
    ldk_mesh_source_set_material(mesh, &desc);

  const LDKAssetMaterialData *data = mesh->material_revision
      ? ldk_asset_manager_material_get_const(context.assets, mesh->material_asset)
      : NULL;
  s_editor_material_row_begin(ui, data ? (data->dirty ? "Shared *" : "Shared") : "");
  if (data && ldk_ui_button(ui, "Save Material"))
  {
    LDKMaterialIOResult result;
    if (!ldk_asset_manager_material_save(&context, mesh->material_asset, &result))
      ldki_editor_log_error(editor, result.error);
  }
  if (ldk_ui_button(ui, "Save As..."))
  {
    XFSPath path = {0};
    if (ldk_os_dialog_show_save_file(editor->window, "Create material",
            "Materials\0*.tml\0\0", path.buf, sizeof(path.buf)))
    {
      LDKMaterialIOResult result;
      LDKAssetMaterial asset = ldk_asset_manager_material_create(
          &context, path.buf, &desc, &result);
      if (x_handle_is_null(asset.h))
        ldki_editor_log_error(editor, result.error);
      else
      {
        ldk_mesh_source_set_material_asset(mesh, context.assets, asset);
        if (!ldk_asset_manager_material_save(&context, asset, &result))
          ldki_editor_log_error(editor, result.error);
      }
    }
  }
  ldk_ui_end_horizontal(ui);
  ldk_ui_pop_id(ui);
}

void ldki_editor_inspector_show(LDKEditorContext *editor)
{
  static LDKUIPoint scroll = {0};
  LDKECS *ecs;
  LDKGame *game;
  LDKEntity entity;
  LDKEntityInfo *info;
  char entity_name[LDK_ENTITY_NAME_MAX_LEN];

  if (!editor)
  {
    return;
  }

  LDKUIContext *ui = &editor->ui;
  ecs = ldk_module_get(LDK_MODULE_ECS);
  game = ldk_game_get();

  if (!ecs || !game || !ldki_editor_selected_entity_get(editor, ecs, &entity))
  {
    memset(&s_editor_inspector_euler_state, 0,
        sizeof(s_editor_inspector_euler_state));
    s_editor_inspector_input_state_clear();
    ldk_ui_label(ui, "No entity selected.");
    return;
  }

  info = ldk_entity_info_get(&ecs->entity, entity);
  if (!info)
  {
    editor->selected_entity = x_handle_null();
    memset(&s_editor_inspector_euler_state, 0,
        sizeof(s_editor_inspector_euler_state));
    s_editor_inspector_input_state_clear();
    ldk_ui_label(ui, "No entity selected.");
    return;
  }

  scroll = ldk_ui_begin_scrollview(
      ui, scroll, LDK_UI_SCROLL_VERTICAL | LDK_UI_SCROLL_IF_NEEDED);

  const char *name = ldk_ecs_entity_name_get(entity);
  snprintf(entity_name, sizeof(entity_name), "%s", name ? name : "");

  LDKUIIcon icon = {0};
  icon.size =
      ldk_sizef(LDK_UI_DEFAULT_CONTROL_HEIGHT, LDK_UI_DEFAULT_CONTROL_HEIGHT);

  const rgba32 flat_color = editor->ui.theme.colors[LDK_UI_COLOR_CONTROL_TEXT];
  const rgba32 white = 0xFFFFFFFF;

  icon.texture =
      ldk_renderer_texture_ui_handle(editor->renderer, editor->ui_atlas);
  icon.color = flat_color;
  icon.uv = ldk_editor_icon_rects[LDK_EDITOR_ICON_OBJECT];
  ldk_ui_push_id_cstr(ui, "entity_name");
  ldk_ui_push_id_u32(ui, entity.index);
  ldk_ui_push_id_u32(ui, entity.version);
  ldk_ui_set_next_height(ui, ldk_ui_px(LDK_UI_DEFAULT_CONTROL_HEIGHT));
  ldk_ui_begin_horizontal(ui);
  ldk_ui_set_next_width(ui, ldk_ui_px(icon.size.w));
  ldk_ui_icon_label(ui, icon, "");
  u32 name_result = ldk_ui_input_box(ui, entity_name, sizeof(entity_name));
  if ((name_result & LDK_UI_INPUT_BOX_CHANGED) != 0)
  {
    if (!ldk_ecs_entity_name_set(entity, entity_name))
      ldki_editor_log_error(editor, "Failed to rename the selected entity.");
  }
  ldk_ui_end_horizontal(ui);
  ldk_ui_pop_id(ui);
  ldk_ui_pop_id(ui);
  ldk_ui_pop_id(ui);

  s_editor_inspector_flags_draw(editor, ecs, entity);

  ldk_ui_horizontal_line(ui);

  icon.uv = ldk_editor_icon_rects[LDK_EDITOR_ICON_COMPONENT];
  s_editor_inspector_area_state_sync(entity);

  for (u32 component_i = 0; component_i < info->components.component_count;
      component_i++)
  {
    u32 component_type = info->components.component_type[component_i];
    bool has_delete_button = true;

    // custom icons for native components
    if (component_type == LDK_COMPONENT_TYPE_TRANSFORM)
    {
      icon.uv = ldk_editor_icon_rects[LDK_EDITOR_ICON_TRANSFORM];
      icon.color = flat_color;
      has_delete_button = false;
    }
    else if (component_type == LDK_COMPONENT_TYPE_CAMERA)
    {
      icon.uv = ldk_editor_icon_rects[LDK_EDITOR_ICON_GIZMO_CAMERA];
      icon.color = flat_color;
    }
    else if (component_type == LDK_COMPONENT_TYPE_MESH_SOURCE)
    {
      icon.uv = ldk_editor_icon_rects[LDK_EDITOR_ICON_MESH];
      icon.color = flat_color;
    }
    else
      icon.uv = ldk_editor_icon_rects[LDK_EDITOR_ICON_OBJECT];

    const LDKComponentMeta *meta =
        ldk_scene_component_meta_find_by_type(game, component_type);
    const char *component_name =
        meta ? meta->name
             : ldk_component_name_get(&ecs->component, component_type);
    void *component = ldk_ecs_component_get(entity, component_type);

    ldk_ui_push_id_u32(ui, component_type);
    bool expanded = s_editor_inspector_component_expanded(component_type);

    expanded = ldk_ui_begin_area_ex(ui,
        component_name ? component_name : "<unknown component>", icon,
        expanded);
    s_editor_inspector_component_expanded_set(component_type, expanded);

    ldk_ui_horizontal_line(ui);

    // Delete button positioned over the area bar
    u32 id = component_type + component_i;
    ldk_ui_push_id_u32(ui, id);         // delete button id
    LDKUIRect r = ldk_ui_last_rect(ui); // area rect
    r.x += r.w - 24.0f - LDK_UI_DEFAULT_PADDING;
    r.y -= LDK_UI_DEFAULT_CONTROL_HEIGHT + LDK_UI_DEFAULT_SPACING;
    r.w = 24.0f;
    r.h = LDK_UI_DEFAULT_CONTROL_HEIGHT;

    icon.uv = ldk_editor_icon_rects[LDK_EDITOR_ICON_DELETE];
    icon.color = flat_color;

    if (expanded && !meta)
    {
      ldk_ui_icon_label(ui, icon, "Component metadata unavailable.");
    }
    else if (expanded && !component)
    {
      ldk_ui_icon_label(ui, icon, "Component data unavailable.");
    }
    else if (expanded)
    {
      // expanded area rect is padded. We must account for that
      r.x += LDK_UI_DEFAULT_PADDING;
      r.y -= LDK_UI_DEFAULT_PADDING;

      /*
       * Inspector visibility follows Comet metadata. It intentionally does
       * not use scene serialization rules: runtime/non-serialized fields may
       * still be useful to inspect.
       */
      for (u32 field_i = 0; field_i < meta->field_count; field_i++)
      {
        s_editor_inspector_field_draw(ui, entity, component_type, meta,
            &meta->fields[field_i], component);
      }
      if (component_type == LDK_COMPONENT_TYPE_MESH_SOURCE)
      {
        s_editor_inspector_material(editor, component);
      }
    }

    if (has_delete_button && ldk_ui_widget_icon_button(ui, id, icon, "", r))
    {
      if (ldk_os_dialog_show_yes_no(editor->window, "Delete Component ?",
              "Are you sure you want to delete this component instance ?"))
      {
        if (!ldk_ecs_component_remove(entity, component_type))
        {
          ldk_os_dialog_show_error(editor->window, "Error", "Error removing component.");
        }
      }
      
    }

    ldk_ui_end_area(ui);
    ldk_ui_spacer(ui);
    ldk_ui_pop_id(ui);
    ldk_ui_pop_id(ui);
  }

  ldk_ui_end_scrollview(ui);
}
