#include <ldk_material_asset.h>
#include <ldk_mesh_asset.h>
#include "ldk_editor_internal.h"
#include "ldk_ui_drag_n_drop.h"
#include "module/ldk_ui.h"
#include <ldk_scene.h>
#include <component/ldk_mesh_source.h>
#include <module/ldk_scene_manager.h>
#include <ctype.h>
#include <errno.h>
#include <inttypes.h>
#include <math.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum
{
  LDK_EDITOR_INSPECTOR_INPUT_CAPACITY = 64,
};

static const LDKComponentFieldMeta s_editor_scene_properties_fields[] =
{
  {
    "Ambient Color",
    LDK_FIELD_U32,
    offsetof(LDKSceneProperties, ambient_color),
    LDK_FIELD_FLAG_NONE,
    LDK_FIELD_WIDGET_COLOR,
    0.0f,
    0.0f,
    NULL,
  },
  {
    "Ambient Intensity",
    LDK_FIELD_FLOAT,
    offsetof(LDKSceneProperties, ambient_intensity),
    LDK_FIELD_FLAG_NONE,
    LDK_FIELD_WIDGET_FLOAT,
    0.0f,
    0.0f,
    NULL,
  },
};

static const LDKComponentMeta s_editor_scene_properties_meta =
{
  "Scene Properties",
  0,
  sizeof(LDKSceneProperties),
  s_editor_scene_properties_fields,
  2,
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
static void s_editor_material_diagnostic(const char *message, void *user);

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
  if (field->type == LDK_FIELD_ENUM && field->enum_meta)
  {
    const LDKEnumMeta *meta = field->enum_meta;
    i64 raw = meta->read(value);
    for (u32 i = 0; i < meta->count; ++i)
    {
      if (meta->options[i].value == raw)
      {
        snprintf(out, out_size, "%s", meta->options[i].label);
        return;
      }
    }
    snprintf(out, out_size, "Unknown (%" PRId64 ")", raw);
    return;
  }
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
  case LDK_FIELD_ASSET_MATERIAL:
    snprintf(out, out_size, "<asset material>");
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

static void s_editor_inspector_mesh_selector(
    LDKEditorContext *editor, LDKMeshSource *mesh, bool readonly)
{
  LDKUIContext *ui;
  LDKAssetManager *assets;
  const char **names;
  u32 mesh_count;
  u32 selected_index;

  if (!editor || !mesh)
  {
    return;
  }

  ui = &editor->ui;
  assets = ldk_module_get(LDK_MODULE_ASSET_MANAGER);
  if (!assets || x_handle_is_null(mesh->source_asset.h))
  {
    return;
  }

  mesh_count = ldk_asset_manager_mesh_count(assets, mesh->source_asset);
  if (mesh_count == 0 || mesh->mesh_index >= mesh_count)
  {
    return;
  }

  selected_index = mesh->mesh_index;
  ldk_ui_set_next_height(ui, ldk_ui_px(LDK_UI_DEFAULT_CONTROL_HEIGHT));
  ldk_ui_begin_horizontal(ui);
  ldk_ui_set_next_width(ui, ldk_ui_px(110.0f));
  ldk_ui_label(ui, "Mesh");

  if (mesh_count == 1)
  {
    const char *name = ldk_asset_manager_mesh_name(
        assets, mesh->source_asset, 0);
    char display[LDK_MESH_NAME_CAPACITY];
    snprintf(display, sizeof(display), "%s",
        name && name[0] ? name : "<unnamed>");
    ldk_ui_begin_disabled(ui, true);
    ldk_ui_input_box(ui, display, (u32)sizeof(display));
    ldk_ui_end_disabled(ui);
    ldk_ui_end_horizontal(ui);
    return;
  }

  names = (const char **)calloc(mesh_count, sizeof(*names));
  if (!names)
  {
    ldk_ui_end_horizontal(ui);
    return;
  }

  for (u32 i = 0; i < mesh_count; i++)
  {
    names[i] = ldk_asset_manager_mesh_name(
        assets, mesh->source_asset, i);
    if (!names[i] || !names[i][0])
    {
      free(names);
      ldk_ui_end_horizontal(ui);
      return;
    }
  }

  ldk_ui_begin_disabled(ui, readonly);
  u32 next = ldk_ui_combo_box(ui, names, mesh_count, selected_index);
  ldk_ui_end_disabled(ui);
  ldk_ui_end_horizontal(ui);

  if (!readonly && next < mesh_count && next != selected_index &&
      !ldk_mesh_source_set_mesh_index(mesh, assets, next))
  {
    ldki_editor_log_error(editor, "Failed to select mesh from asset.");
  }

  free(names);
}

static bool s_editor_inspector_asset_path_validate(
    LDKEditorContext *editor, const XFSPath *path, const char *title,
    const char *message, XFSPath *out_normalized)
{
  XFSPath normalized = {0};
  XFSPath root;
  XFSPath relative = {0};

  if (!editor || !path || !out_normalized)
  {
    return false;
  }

  normalized = *path;
  root = editor->project.run_root_path;
  x_fs_path_normalize(&normalized);
  x_fs_path_normalize(&root);

  if (!editor->project.loaded || !root.length ||
      !x_fs_path_common_prefix(root.buf, normalized.buf, &relative) ||
      !relative.length || strcmp(relative.buf, ".") == 0)
  {
    ldk_os_dialog_show_error(editor->window, title, message);
    return false;
  }

  *out_normalized = normalized;
  return true;
}

static bool s_editor_inspector_mesh_asset_field(LDKEditorContext *editor,
    const char *label, LDKAssetMesh *value, bool readonly)
{
  LDKUIContext *ui;
  LDKAssetManager *assets;
  LDKAssetHandle handle;
  const LDKAssetInfo *info;
  XFSPath path = {0};
  char display[sizeof(path.buf)];
  bool assign = false;

  if (!editor || !value)
  {
    return false;
  }

  ui = &editor->ui;
  assets = ldk_module_get(LDK_MODULE_ASSET_MANAGER);
  handle.h = value->h;
  info = assets && !x_handle_is_null(value->h)
      ? ldk_asset_get_info_const(assets, handle)
      : NULL;

  snprintf(display, sizeof(display), "%s", info ? info->asset_path.buf : "");

  ldk_ui_set_next_height(ui, ldk_ui_px(LDK_UI_DEFAULT_CONTROL_HEIGHT));
  ldk_ui_begin_horizontal(ui);
  ldk_ui_set_next_width(ui, ldk_ui_px(110.0f));
  ldk_ui_label(ui, label);

  ldk_ui_begin_disabled(ui, true);
  ldk_ui_input_box(ui, display, sizeof(display));
  LDKUIRect target = ldk_ui_last_bounding_rect(ui);
  ldk_ui_end_disabled(ui);

  if (!readonly && ui->mouse && ui->active_id && ui->current_window &&
      ui->hovered_window_id == ui->current_window->id &&
      ldk_os_mouse_button_up(
          (LDKMouseState *)ui->mouse, LDK_MOUSE_BUTTON_LEFT))
  {
    LDKPoint cursor = ldk_os_mouse_cursor((LDKMouseState *)ui->mouse);
    if (ldk_rectf_contains(&target, (float)cursor.x, (float)cursor.y) &&
        ldk_rectf_contains(&ui->clip_rect, (float)cursor.x, (float)cursor.y))
    {
      u32 payload_type = 0;
      assign = ldk_ui_drag_n_drop_payload_get_and_remove(
                   &payload_type, &path) &&
          payload_type == LDK_EDITOR_DRAG_N_DROP_PAYLOAD_FILE_PATH;
    }
  }

  ldk_ui_set_next_width(ui, ldk_ui_px(28.0f));
  ldk_ui_begin_disabled(ui, readonly);
  if (ldk_ui_button(ui, "..."))
  {
    assign = ldk_os_dialog_show_open_file(editor->window, "Choose mesh",
        "Meshes\0*.mesh\0\0", path.buf, sizeof(path.buf));
  }
  ldk_ui_end_disabled(ui);
  ldk_ui_end_horizontal(ui);

  if (!assign)
  {
    return false;
  }

  XFSPath normalized;
  if (!s_editor_inspector_asset_path_validate(editor, &path, "Mesh",
          "Choose a mesh inside the project's runtree folder.", &normalized))
  {
    return false;
  }

  if (!assets)
  {
    ldki_editor_log_error(editor, "Asset manager is unavailable.");
    return false;
  }

  LDKMeshAssetResult result;
  LDKAssetMesh asset =
      ldk_asset_manager_mesh_load_shared(assets, normalized.buf, &result);
  if (x_handle_is_null(asset.h))
  {
    ldki_editor_log_error(editor, result.error);
    return false;
  }

  *value = asset;
  return true;
}

static bool s_editor_inspector_material_asset_field(LDKEditorContext *editor,
    const char *label, LDKAssetMaterial *value, bool readonly)
{
  LDKUIContext *ui;
  LDKAssetManager *assets;
  LDKAssetHandle handle;
  const LDKAssetInfo *info;
  XFSPath path = {0};
  char display[sizeof(path.buf)];
  bool assign = false;

  if (!editor || !value)
  {
    return false;
  }

  ui = &editor->ui;
  assets = ldk_module_get(LDK_MODULE_ASSET_MANAGER);
  handle.h = value->h;
  info = assets && !x_handle_is_null(value->h)
      ? ldk_asset_get_info_const(assets, handle)
      : NULL;

  snprintf(display, sizeof(display), "%s", info ? info->asset_path.buf : "");

  ldk_ui_set_next_height(ui, ldk_ui_px(LDK_UI_DEFAULT_CONTROL_HEIGHT));
  ldk_ui_begin_horizontal(ui);
  ldk_ui_set_next_width(ui, ldk_ui_px(110.0f));
  ldk_ui_label(ui, label);

  ldk_ui_begin_disabled(ui, true);
  ldk_ui_input_box(ui, display, sizeof(display));
  LDKUIRect target = ldk_ui_last_bounding_rect(ui);
  ldk_ui_end_disabled(ui);

  if (!readonly && ui->mouse && ui->active_id && ui->current_window &&
      ui->hovered_window_id == ui->current_window->id &&
      ldk_os_mouse_button_up(
          (LDKMouseState *)ui->mouse, LDK_MOUSE_BUTTON_LEFT))
  {
    LDKPoint cursor = ldk_os_mouse_cursor((LDKMouseState *)ui->mouse);
    if (ldk_rectf_contains(&target, (float)cursor.x, (float)cursor.y) &&
        ldk_rectf_contains(&ui->clip_rect, (float)cursor.x, (float)cursor.y))
    {
      u32 payload_type = 0;
      assign = ldk_ui_drag_n_drop_payload_get_and_remove(
                   &payload_type, &path) &&
          payload_type == LDK_EDITOR_DRAG_N_DROP_PAYLOAD_FILE_PATH;
    }
  }

  ldk_ui_set_next_width(ui, ldk_ui_px(28.0f));
  ldk_ui_begin_disabled(ui, readonly);
  if (ldk_ui_button(ui, "..."))
  {
    assign = ldk_os_dialog_show_open_file(editor->window, "Choose material",
        "Materials\0*.tml\0\0", path.buf, sizeof(path.buf));
  }
  ldk_ui_end_disabled(ui);
  ldk_ui_end_horizontal(ui);

  if (!assign)
  {
    return false;
  }

  XFSPath normalized;
  if (!s_editor_inspector_asset_path_validate(editor, &path, "Material",
          "Choose a material inside the project's runtree folder.",
          &normalized))
  {
    return false;
  }

  if (!assets)
  {
    ldki_editor_log_error(editor, "Asset manager is unavailable.");
    return false;
  }

  LDKMaterialIOContext context = {0};
  LDKMaterialIOResult result;
  context.assets = assets;
  context.runtree_path = editor->project.run_root_path;
  context.diagnostic = s_editor_material_diagnostic;
  context.user = editor;

  LDKAssetMaterial asset = ldk_asset_manager_material_load_shared(
      &context, normalized.buf, &result);
  if (x_handle_is_null(asset.h))
  {
    ldki_editor_log_error(editor, result.error);
    return false;
  }

  *value = asset;
  return true;
}

static void s_editor_inspector_field_draw(
    LDKEditorContext *editor, LDKEntity entity,
    u32 component_type, const LDKComponentMeta *meta,
    const LDKComponentFieldMeta *field, void *component)
{
  LDKUIContext *ui;
  char value_text[128];
  u8 *field_value;
  bool readonly;

  if (!editor || !meta || !field || !component || field->offset >= meta->size)
  {
    return;
  }

  ui = &editor->ui;

  field_value = (u8 *)component + field->offset;
  readonly = (field->flags & LDK_FIELD_FLAG_READONLY) != 0;

  ldk_ui_push_id_cstr(ui, field->name);

  if (component_type == LDK_COMPONENT_TYPE_MESH_SOURCE &&
      field->type == LDK_FIELD_U32 &&
      field->offset == offsetof(LDKMeshSource, mesh_index))
  {
    ldk_ui_pop_id(ui);
    return;
  }

  if (field->type == LDK_FIELD_ASSET_MESH)
  {
    LDKAssetMesh value = *(LDKAssetMesh *)field_value;
    bool changed = s_editor_inspector_mesh_asset_field(
        editor, field->name, &value, readonly);

    if (component_type == LDK_COMPONENT_TYPE_MESH_SOURCE &&
        field->offset == offsetof(LDKMeshSource, source_asset))
    {
      LDKMeshSource *mesh = (LDKMeshSource *)component;
      LDKAssetManager *assets = ldk_module_get(LDK_MODULE_ASSET_MANAGER);

      if (changed &&
          (!assets || !ldk_mesh_source_set_data(mesh, value) ||
              !ldk_mesh_source_materials_sync(mesh, assets)))
      {
        ldki_editor_log_error(editor, "Failed to assign mesh asset.");
      }

      s_editor_inspector_mesh_selector(editor, mesh, readonly);
    }
    else if (changed)
    {
      *(LDKAssetMesh *)field_value = value;
    }

    ldk_ui_pop_id(ui);
    return;
  }

  if (field->type == LDK_FIELD_ASSET_MATERIAL)
  {
    LDKAssetMaterial value = *(LDKAssetMaterial *)field_value;
    if (s_editor_inspector_material_asset_field(
            editor, field->name, &value, readonly))
    {
      *(LDKAssetMaterial *)field_value = value;
    }

    ldk_ui_pop_id(ui);
    return;
  }

  ldk_ui_set_next_height(ui, ldk_ui_px(LDK_UI_DEFAULT_CONTROL_HEIGHT));
  ldk_ui_begin_horizontal(ui);
  ldk_ui_set_next_width(ui, ldk_ui_px(110.0f));
  ldk_ui_label(ui, field->name);

  if (field->type == LDK_FIELD_ENUM && field->enum_meta)
  {
    const LDKEnumMeta *meta = field->enum_meta;
    const char **labels = malloc((meta->count + 1u) * sizeof(*labels));
    if (labels)
    {
      char unknown[64];
      i64 raw = meta->read(field_value);
      u32 selected = meta->count;
      for (u32 i = 0; i < meta->count; ++i)
      {
        labels[i] = meta->options[i].label;
        if (selected == meta->count && meta->options[i].value == raw)
        {
          selected = i;
        }
      }
      snprintf(unknown, sizeof(unknown), "Unknown (%" PRId64 ")", raw);
      labels[meta->count] = unknown;
      u32 count = meta->count + (selected == meta->count ? 1u : 0u);
      ldk_ui_begin_disabled(ui, readonly);
      u32 next = ldk_ui_combo_box(ui, labels, count, selected);
      ldk_ui_end_disabled(ui);
      if (!readonly && next != selected && next < meta->count)
      {
        meta->write(field_value, meta->options[next].value);
      }
      free(labels);
    }
    else
    {
      ldk_ui_label(ui, "Enum options unavailable");
    }
    ldk_ui_end_horizontal(ui);
    ldk_ui_pop_id(ui);
    return;
  }

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
    if (field->widget == LDK_FIELD_WIDGET_COLOR)
    {
      rgba32 color = *(u32 *)field_value;
      ldk_ui_begin_disabled(ui, readonly);
      if (ldk_ui_color_view(ui, color) && !readonly)
      {
        ldk_os_dialog_color_picker_show(editor->window, &color);
        *(u32 *)field_value = color;
      }
      ldk_ui_end_disabled(ui);
      break;
    }
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

    ldk_ui_set_next_width(ui, ldk_ui_px(12.0f));
    ldk_ui_label(ui, "X");
    changed |= s_editor_inspector_float_input(
        ui, entity, component_type, field, 0, &value.x, readonly);

    ldk_ui_set_next_width(ui, ldk_ui_px(12.0f));
    ldk_ui_label(ui, "Y");
    changed |= s_editor_inspector_float_input(
        ui, entity, component_type, field, 1, &value.y, readonly);

    ldk_ui_set_next_width(ui, ldk_ui_px(12.0f));
    ldk_ui_label(ui, "Z");
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
  case LDK_FIELD_ASSET_MATERIAL:
  default:
    s_editor_inspector_field_value_format(
        value_text, sizeof(value_text), field, field_value);
    ldk_ui_label(ui, value_text);
    break;
  }

  ldk_ui_end_horizontal(ui);
  ldk_ui_pop_id(ui);
}

static void s_editor_inspector_scene_properties_draw(
    LDKEditorContext *editor)
{
  const LDKSceneProperties *current;
  LDKSceneProperties properties;
  LDKComponentFieldMeta fields[2];
  LDKComponentMeta meta = s_editor_scene_properties_meta;
  LDKEntity key = x_handle_null();
  bool readonly;

  if (!editor)
  {
    return;
  }

  current = ldk_scene_properties_get();
  if (!current)
  {
    ldk_ui_label(&editor->ui, "Scene properties unavailable.");
    return;
  }

  properties = *current;
  readonly = editor->editor_state != LDK_EDITOR_STATE_STOPED;
  memcpy(fields, s_editor_scene_properties_fields, sizeof(fields));
  if (readonly)
  {
    fields[0].flags |= LDK_FIELD_FLAG_READONLY;
    fields[1].flags |= LDK_FIELD_FLAG_READONLY;
  }
  meta.fields = fields;

  ldk_ui_push_id_cstr(&editor->ui, "scene_properties");
  ldk_ui_label(&editor->ui, "Scene Properties");
  ldk_ui_horizontal_line(&editor->ui);

  for (u32 i = 0; i < meta.field_count; ++i)
  {
    s_editor_inspector_field_draw(
        editor, key, 0, &meta, &meta.fields[i], &properties);
  }

  if (!readonly && isfinite(properties.ambient_intensity) &&
      properties.ambient_intensity >= 0.0f &&
      (properties.ambient_color != current->ambient_color ||
          properties.ambient_intensity != current->ambient_intensity))
  {
    (void)ldk_scene_properties_set(&properties);
  }

  ldk_ui_pop_id(&editor->ui);
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

static const LDKMeshSourceMaterialBinding *
s_editor_mesh_material_binding_const(
    const LDKMeshSource *mesh, u32 material_slot)
{
  return ldk_mesh_source_additional_material_binding_const(
      mesh, material_slot);
}

static LDKMaterialDesc s_editor_mesh_material_desc(
    const LDKMeshSource *mesh, u32 material_slot)
{
  LDKMaterialDesc desc = {0};

  if (!mesh)
  {
    return desc;
  }

  if (material_slot == 0)
  {
    return mesh->material;
  }

  const LDKMeshSourceMaterialBinding *binding =
      s_editor_mesh_material_binding_const(mesh, material_slot);
  return binding ? binding->material : desc;
}

static LDKAssetMaterial s_editor_mesh_material_asset(
    const LDKMeshSource *mesh, u32 material_slot)
{
  if (!mesh)
  {
    return ldk_asset_material_null();
  }

  if (material_slot == 0)
  {
    return mesh->material_asset;
  }

  const LDKMeshSourceMaterialBinding *binding =
      s_editor_mesh_material_binding_const(mesh, material_slot);
  return binding ? binding->material_asset : ldk_asset_material_null();
}

static u64 s_editor_mesh_material_revision(
    const LDKMeshSource *mesh, u32 material_slot)
{
  if (!mesh)
  {
    return 0;
  }

  if (material_slot == 0)
  {
    return mesh->material_revision;
  }

  const LDKMeshSourceMaterialBinding *binding =
      s_editor_mesh_material_binding_const(mesh, material_slot);
  return binding ? binding->material_revision : 0;
}

static bool s_editor_material_asset_field(LDKEditorContext *editor,
    LDKMeshSource *mesh, u32 material_slot, const char *slot_label,
    const LDKMaterialIOContext *context)
{
  LDKUIContext *ui = &editor->ui;
  LDKAssetMaterial material_asset =
      s_editor_mesh_material_asset(mesh, material_slot);
  u64 material_revision =
      s_editor_mesh_material_revision(mesh, material_slot);
  LDKAssetHandle handle = {material_asset.h};
  const LDKAssetInfo *info = material_revision
      ? ldk_asset_get_info_const(context->assets, handle) : NULL;
  XFSPath path = {0};
  char display[sizeof(path.buf)];

  snprintf(display, sizeof(display), "%s", info ? info->asset_path.buf : "");
  s_editor_material_row_begin(ui, slot_label);
  ldk_ui_begin_disabled(ui, true);
  ldk_ui_input_box(ui, display, sizeof(display));
  LDKUIRect target = ldk_ui_last_bounding_rect(ui);
  ldk_ui_end_disabled(ui);

  bool assign = false;
  if (ui->mouse && ui->active_id && ui->current_window &&
      ui->hovered_window_id == ui->current_window->id &&
      ldk_os_mouse_button_up(
          (LDKMouseState *)ui->mouse, LDK_MOUSE_BUTTON_LEFT))
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
  {
    assign = ldk_os_dialog_show_open_file(editor->window, "Choose material",
        "Materials\0*.tml\0\0", path.buf, sizeof(path.buf));
  }
  ldk_ui_end_horizontal(ui);

  if (assign)
  {
    LDKMaterialIOResult result;
    LDKAssetMaterial asset = ldk_asset_manager_material_load_shared(
        context, path.buf, &result);
    if (x_handle_is_null(asset.h))
    {
      ldki_editor_log_error(editor, result.error);
    }
    else
    {
      return ldk_mesh_source_set_material_asset_at(
          mesh, context->assets, material_slot, asset);
    }
  }

  return false;
}

static void s_editor_inspector_material_slot(LDKEditorContext *editor,
    LDKMeshSource *mesh, u32 material_slot, const char *slot_label,
    const LDKMaterialIOContext *context)
{
  static const char *const names[] = {
      "Textured Unlit", "Textured", "Vertex Color Unlit", "Vertex Color"};
  static const LDKMaterialType types[] = {
      LDK_MATERIAL_TYPE_TEXTURED_UNLIT, LDK_MATERIAL_TYPE_TEXTURED,
      LDK_MATERIAL_TYPE_VERTEX_COLOR_UNLIT, LDK_MATERIAL_TYPE_VERTEX_COLOR};
  LDKUIContext *ui = &editor->ui;

  ldk_ui_push_id_u32(ui, material_slot);
  ldk_ui_horizontal_line(ui);
  s_editor_material_asset_field(
      editor, mesh, material_slot, slot_label, context);

  LDKAssetMaterial material_asset =
      s_editor_mesh_material_asset(mesh, material_slot);
  u64 material_revision =
      s_editor_mesh_material_revision(mesh, material_slot);
  const LDKAssetMaterialData *bound = material_revision
      ? ldk_asset_manager_material_get_const(context->assets, material_asset)
      : NULL;
  if (bound && bound->is_missing)
  {
    s_editor_material_row_begin(ui, "Status");
    ldk_ui_label(ui, "Missing material (magenta checker)");
    ldk_ui_end_horizontal(ui);
    ldk_ui_pop_id(ui);
    return;
  }

  LDKMaterialDesc desc = s_editor_mesh_material_desc(mesh, material_slot);
  u32 selected = 3;
  for (u32 i = 0; i < 4; ++i)
  {
    if (desc.type == types[i])
    {
      selected = i;
    }
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
  bool lit = desc.type == LDK_MATERIAL_TYPE_TEXTURED ||
      desc.type == LDK_MATERIAL_TYPE_VERTEX_COLOR;
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
  u32 alpha =
      (u32)(ldk_ui_slider(ui, (float)(*color & 255), 0, 255) + 0.5f);
  *color = (*color & 0xffffff00u) | alpha;
  ldk_ui_end_horizontal(ui);

  if (lit)
  {
    s_editor_material_row_begin(ui, "Specular");
    desc.surface.specular =
        ldk_ui_slider(ui, desc.surface.specular, 0.0f, 1.0f);
    ldk_ui_end_horizontal(ui);

    s_editor_material_row_begin(ui, "Shininess");
    float shininess =
        desc.surface.shininess == 0.0f ? 32.0f : desc.surface.shininess;
    desc.surface.shininess =
        ldk_ui_slider(ui, shininess, 1.0f, 256.0f);
    ldk_ui_end_horizontal(ui);

    s_editor_material_row_begin(ui, "Emission");
    desc.surface.emission =
        ldk_ui_slider(ui, desc.surface.emission, 0.0f, 4.0f);
    ldk_ui_end_horizontal(ui);
  }

  if (textured)
  {
    LDKAssetManager *assets = context->assets;
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

    if (ui->mouse && ui->active_id && ui->current_window &&
        ui->hovered_window_id == ui->current_window->id &&
        ldk_os_mouse_button_up(
            (LDKMouseState *)ui->mouse, LDK_MOUSE_BUTTON_LEFT))
    {
      LDKPoint cursor = ldk_os_mouse_cursor((LDKMouseState *)ui->mouse);
      if (ldk_rectf_contains(&target, (float)cursor.x, (float)cursor.y) &&
          ldk_rectf_contains(
              &ui->clip_rect, (float)cursor.x, (float)cursor.y))
      {
        u32 payload_type = 0;
        assign = ldk_ui_drag_n_drop_payload_get_and_remove(
                     &payload_type, &path) &&
            payload_type == LDK_EDITOR_DRAG_N_DROP_PAYLOAD_FILE_PATH;
      }
    }

    ldk_ui_set_next_width(ui, ldk_ui_px(28.0f));
    if (ldk_ui_button(ui, "..."))
    {
      assign = ldk_os_dialog_show_open_file(editor->window,
          "Choose material image",
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
        {
          ldk_os_dialog_show_error(editor->window, "Material image",
              "The selected image could not be loaded.");
        }
        else
        {
          desc.args.textured.texture = image;
        }
      }
    }
  }

  if (material_revision)
  {
    ldk_asset_manager_material_update(
        context->assets, material_asset, &desc);
    ldk_mesh_source_material_sync(mesh, context->assets);
  }
  else
  {
    ldk_mesh_source_set_material_at(mesh, material_slot, &desc);
  }

  material_asset = s_editor_mesh_material_asset(mesh, material_slot);
  material_revision = s_editor_mesh_material_revision(mesh, material_slot);
  const LDKAssetMaterialData *data = material_revision
      ? ldk_asset_manager_material_get_const(context->assets, material_asset)
      : NULL;
  s_editor_material_row_begin(
      ui, data ? (data->dirty ? "Shared *" : "Shared") : "");
  if (data && ldk_ui_button(ui, "Save Material"))
  {
    LDKMaterialIOResult result;
    if (!ldk_asset_manager_material_save(context, material_asset, &result))
    {
      ldki_editor_log_error(editor, result.error);
    }
  }

  if (ldk_ui_button(ui, "Save As..."))
  {
    XFSPath path = {0};
    if (ldk_os_dialog_show_save_file(editor->window, "Create material",
            "Materials\0*.tml\0\0", path.buf, sizeof(path.buf)))
    {
      LDKMaterialIOResult result;
      LDKAssetMaterial asset = ldk_asset_manager_material_create(
          context, path.buf, &desc, &result);
      if (x_handle_is_null(asset.h))
      {
        ldki_editor_log_error(editor, result.error);
      }
      else
      {
        ldk_mesh_source_set_material_asset_at(
            mesh, context->assets, material_slot, asset);
        if (!ldk_asset_manager_material_save(context, asset, &result))
        {
          ldki_editor_log_error(editor, result.error);
        }
      }
    }
  }
  ldk_ui_end_horizontal(ui);
  ldk_ui_pop_id(ui);
}

static void s_editor_inspector_material(
    LDKEditorContext *editor, LDKMeshSource *mesh)
{
  LDKUIContext *ui;
  LDKMaterialIOContext context = {0};
  LDKAssetManager *assets;
  u32 material_count;

  if (!editor || !mesh)
  {
    return;
  }

  ui = &editor->ui;
  assets = ldk_module_get(LDK_MODULE_ASSET_MANAGER);
  if (!assets)
  {
    return;
  }

  context.assets = assets;
  context.runtree_path = editor->project.run_root_path;
  context.diagnostic = s_editor_material_diagnostic;
  context.user = editor;

  if (!ldk_mesh_source_materials_sync(mesh, assets) ||
      !ldk_mesh_source_material_sync(mesh, assets))
  {
    return;
  }

  material_count = ldk_mesh_source_material_count(mesh);

  ldk_ui_push_id_cstr(ui, "material");
  for (u32 slot = 0; slot < material_count; slot++)
  {
    s_editor_inspector_material_slot(
        editor, mesh, slot, "Material", &context);
  }
  ldk_ui_pop_id(ui);
}

static LDKSceneSystems *s_editor_inspector_scene_systems(
    LDKEditorContext *editor)
{
  LDKSceneManager *manager;

  if (!editor)
  {
    return NULL;
  }

  if (editor->editor_state == LDK_EDITOR_STATE_STOPED)
  {
    return &editor->current_scene_systems;
  }

  manager = ldk_module_get(LDK_MODULE_SCENE_MANAGER);
  if (!manager || !ldk_scene_manager_current(manager))
  {
    // Play Current Scene uses the editor's systems without a catalog scene.
    return &editor->current_scene_systems;
  }

  return &manager->current_systems;
}

static bool s_editor_inspector_system_index(
    const LDKSceneSystems *systems, u64 id, u32 *out_index)
{
  if (!systems || id == 0)
  {
    return false;
  }

  for (u32 i = 0; i < systems->count; ++i)
  {
    if (systems->ids[i] == id)
    {
      if (out_index)
      {
        *out_index = i;
      }
      return true;
    }
  }

  return false;
}

static const LDKSystemMeta *s_editor_inspector_system_meta(
    LDKGame *game, u64 id)
{
  if (!game || !game->system_metadata_count || !game->system_metadata_get)
  {
    return NULL;
  }

  u32 count = game->system_metadata_count();
  for (u32 i = 0; i < count; ++i)
  {
    const LDKSystemMeta *meta = game->system_metadata_get(i);
    if (meta && meta->id == id)
    {
      return meta;
    }
  }

  return NULL;
}

static LDKEntity s_editor_inspector_system_key(u64 id)
{
  LDKEntity key = x_handle_null();
  key.index = (u32)id;
  key.version = (u32)(id >> 32u);
  return key;
}

static void s_editor_inspector_system_grouping_draw(
    LDKEditorContext *editor, LDKSceneSystems *systems, u64 system_id)
{
  LDKUIContext *ui;
  u64 current_id;
  u32 grouping_count;
  bool current_found;
  u32 item_count;
  const char **labels;
  u64 *ids;
  u32 selected = 0;
  u32 write_index = 1;
  bool can_edit;
  char missing_label[64];

  if (!editor || !systems)
  {
    return;
  }

  ui = &editor->ui;
  current_id = ldk_scene_systems_grouping_get(systems, system_id);
  grouping_count = ldk_ecs_grouping_count();
  current_found = current_id == 0;

  for (u32 i = 0; i < grouping_count; ++i)
  {
    LDKGroupingDesc desc = {0};
    if (ldk_ecs_grouping_at(i, &desc) && desc.id == current_id)
    {
      current_found = true;
      break;
    }
  }

  item_count = grouping_count + 1u + (!current_found ? 1u : 0u);
  labels = (const char **)calloc(item_count, sizeof(*labels));
  ids = (u64 *)calloc(item_count, sizeof(*ids));

  ldk_ui_set_next_height(ui, ldk_ui_px(LDK_UI_DEFAULT_CONTROL_HEIGHT));
  ldk_ui_begin_horizontal(ui);
  ldk_ui_set_next_width(ui, ldk_ui_px(110.0f));
  ldk_ui_label(ui, "Grouping");

  if (!labels || !ids)
  {
    free(labels);
    free(ids);
    ldk_ui_label(ui, "<grouping unavailable>");
    ldk_ui_end_horizontal(ui);
    return;
  }

  labels[0] = "<No Grouping>";
  ids[0] = 0;

  for (u32 i = 0; i < grouping_count; ++i)
  {
    LDKGroupingDesc desc = {0};
    if (!ldk_ecs_grouping_at(i, &desc))
    {
      continue;
    }

    labels[write_index] = desc.name;
    ids[write_index] = desc.id;
    if (desc.id == current_id)
    {
      selected = write_index;
    }
    ++write_index;
  }

  if (!current_found)
  {
    snprintf(missing_label, sizeof(missing_label),
        "<missing 0x%016" PRIx64 ">", current_id);
    labels[write_index] = missing_label;
    ids[write_index] = current_id;
    selected = write_index;
    ++write_index;
  }

  can_edit = editor->project.loaded &&
             editor->editor_state == LDK_EDITOR_STATE_STOPED &&
             editor->current_scene_path.length != 0;

  ldk_ui_begin_disabled(ui, !can_edit);
  u32 new_selected = ldk_ui_combo_box(ui, labels, write_index, selected);
  ldk_ui_end_disabled(ui);
  if (can_edit && new_selected < write_index && new_selected != selected &&
      !ldk_scene_systems_grouping_set(
          systems, system_id, ids[new_selected]))
  {
    ldki_editor_log_error(editor, "Failed to change scene system grouping.");
  }

  free(ids);
  free(labels);
  ldk_ui_end_horizontal(ui);
}

static bool s_editor_inspector_system_draw(
    LDKEditorContext *editor, LDKGame *game, u64 system_id)
{
  LDKSceneSystems *systems;
  const LDKSystemMeta *meta;
  u32 system_index;
  LDKUIContext *ui;
  LDKUIIcon icon = {0};
  char name[256];

  if (!editor || system_id == 0)
  {
    return false;
  }

  systems = s_editor_inspector_scene_systems(editor);
  if (!s_editor_inspector_system_index(systems, system_id, &system_index))
  {
    return false;
  }

  ui = &editor->ui;
  meta = s_editor_inspector_system_meta(game, system_id);
  if (meta && meta->name)
  {
    snprintf(name, sizeof(name), "%s", meta->name);
  }
  else
  {
    snprintf(name, sizeof(name), "<missing system> 0x%016" PRIx64, system_id);
  }

  icon.size =
      ldk_sizef(LDK_UI_DEFAULT_CONTROL_HEIGHT, LDK_UI_DEFAULT_CONTROL_HEIGHT);
  icon.texture =
      ldk_renderer_texture_ui_handle(editor->renderer, editor->ui_atlas);
  icon.color = editor->ui.theme.colors[LDK_UI_COLOR_CONTROL_TEXT];
  icon.uv = ldk_editor_icon_rects[LDK_EDITOR_ICON_SYSTEM];

  ldk_ui_push_id_cstr(ui, "system");
  ldk_ui_push_id_u32(ui, (u32)system_id);
  ldk_ui_push_id_u32(ui, (u32)(system_id >> 32u));

  ldk_ui_set_next_height(ui, ldk_ui_px(LDK_UI_DEFAULT_CONTROL_HEIGHT));
  ldk_ui_begin_horizontal(ui);
  ldk_ui_set_next_width(ui, ldk_ui_px(icon.size.w));
  ldk_ui_icon_label(ui, icon, "");
  ldk_ui_label(ui, name);
  ldk_ui_end_horizontal(ui);

  s_editor_inspector_system_grouping_draw(editor, systems, system_id);
  ldk_ui_horizontal_line(ui);

  if (!meta)
  {
    ldk_ui_label(ui, "System metadata unavailable.");
  }
  else if (meta->size == 0)
  {
    ldk_ui_label(ui, "This system has no data fields.");
  }
  else
  {
    void *data = ldk_scene_systems_data_get(systems, system_id);
    u32 data_size = systems->data_sizes ? systems->data_sizes[system_index] : 0;

    if (!data || data_size != meta->size)
    {
      ldk_ui_label(ui, "System data unavailable.");
    }
    else if (!meta->fields || meta->field_count == 0)
    {
      ldk_ui_label(ui, "No inspectable data fields.");
    }
    else
    {
      LDKComponentMeta field_meta = {0};
      LDKEntity key = s_editor_inspector_system_key(system_id);
      field_meta.name = meta->name;
      field_meta.size = meta->size;
      field_meta.fields = meta->fields;
      field_meta.field_count = meta->field_count;

      for (u32 i = 0; i < meta->field_count; ++i)
      {
        s_editor_inspector_field_draw(
            editor, key, 0, &field_meta, &meta->fields[i], data);
      }
    }
  }

  ldk_ui_pop_id(ui);
  ldk_ui_pop_id(ui);
  ldk_ui_pop_id(ui);
  return true;
}

static void s_editor_inspector_add_component_draw(
    LDKEditorContext *editor, LDKECS *ecs, LDKGame *game, LDKEntity entity)
{
  const LDKUIId popup_id = 0x41434D50u;
  LDKUIContext *ui;
  u32 available_count = 0;
  bool added = false;

  if (!editor || !ecs || !game)
  {
    return;
  }

  ui = &editor->ui;

  ldk_ui_set_next_width(ui, ldk_ui_fill());
  if (ldk_ui_button(ui, "+ Add Component"))
  {
    ldk_ui_open_popup(ui, popup_id);
  }

  if (!ldk_ui_begin_popup(ui, popup_id))
  {
    return;
  }

  if (!game->metadata_count || !game->metadata_get)
  {
    ldk_ui_label(ui, "Component metadata unavailable.");
    ldk_ui_end_popup(ui);
    return;
  }

  u32 metadata_count = game->metadata_count();
  for (u32 i = 0; i < metadata_count; ++i)
  {
    const LDKComponentMeta *meta = game->metadata_get(i);
    const char *component_name;
    u32 component_type;

    if (!meta)
    {
      continue;
    }

    component_type = ldk_scene_component_meta_runtime_type(meta);
    if (!ldk_component_is_registered(&ecs->component, component_type) ||
        ldk_entity_component_has(&ecs->entity, entity, component_type))
    {
      continue;
    }

    component_name = meta->name;
    if (!component_name || component_name[0] == 0)
    {
      component_name = ldk_component_name_get(&ecs->component, component_type);
    }

    if (!component_name || component_name[0] == 0)
    {
      continue;
    }

    ++available_count;
    ldk_ui_push_id_u32(ui, component_type);
    if (ldk_ui_button_flat(ui, component_name))
    {
      if (ldk_ecs_component_add(entity, component_type, NULL))
      {
        s_editor_inspector_component_expanded_set(component_type, true);
        ldk_ui_close_current_popup(ui);
        added = true;
      }
      else
      {
        ldk_os_dialog_show_error(editor->window, "Add Component",
            "Failed to add the selected component.");
      }
    }
    ldk_ui_pop_id(ui);

    if (added)
    {
      break;
    }
  }

  if (!added && available_count == 0)
  {
    ldk_ui_label(ui, "No components available.");
  }

  ldk_ui_end_popup(ui);
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

  if (editor->scene_properties_selected)
  {
    if (editor->current_scene_path.length == 0)
    {
      editor->scene_properties_selected = false;
    }
    else
    {
      scroll = ldk_ui_begin_scrollview(
          ui, scroll, LDK_UI_SCROLL_VERTICAL | LDK_UI_SCROLL_IF_NEEDED);
      s_editor_inspector_scene_properties_draw(editor);
      ldk_ui_end_scrollview(ui);
      return;
    }
  }

  if (editor->selected_system_id != 0)
  {
    LDKSceneSystems *systems = s_editor_inspector_scene_systems(editor);
    if (s_editor_inspector_system_index(
            systems, editor->selected_system_id, NULL))
    {
      scroll = ldk_ui_begin_scrollview(
          ui, scroll, LDK_UI_SCROLL_VERTICAL | LDK_UI_SCROLL_IF_NEEDED);
      (void)s_editor_inspector_system_draw(
          editor, game, editor->selected_system_id);
      ldk_ui_end_scrollview(ui);
      return;
    }
    editor->selected_system_id = 0;
  }

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
        s_editor_inspector_field_draw(editor, entity, component_type, meta,
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

  s_editor_inspector_add_component_draw(editor, ecs, game, entity);

  ldk_ui_end_scrollview(ui);
}
