#include "ldk_editor_internal.h"
#include "module/ldk_ui.h"
#include <ldk_scene.h>
#include <stddef.h>
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

typedef struct LDKEditorInspectorAreaState
{
  LDKEntity entity;
  u32 collapsed_component_types[LDK_ENTITY_MAX_COMPONENTS];
  u32 collapsed_component_count;
  bool valid;
} LDKEditorInspectorAreaState;

static LDKEditorInspectorInputState s_editor_inspector_input_state = {0};
static LDKEditorInspectorAreaState s_editor_inspector_area_state = {0};
static char
    s_editor_inspector_input_buffer[LDK_EDITOR_INSPECTOR_INPUT_CAPACITY] = {0};

static void s_editor_inspector_area_state_sync(LDKEntity entity)
{
  if (s_editor_inspector_area_state.valid &&
      ldki_editor_entity_equal(s_editor_inspector_area_state.entity, entity))
  {
    return;
  }

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

    ldk_ui_set_next_width(ui, ldk_ui_px(72.0f));
    changed |= s_editor_inspector_float_input(
        ui, entity, component_type, field, 0, &value.x, readonly);
    ldk_ui_set_next_width(ui, ldk_ui_px(72.0f));
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

    ldk_ui_set_next_width(ui, ldk_ui_px(72.0f));
    changed |= s_editor_inspector_float_input(
        ui, entity, component_type, field, 0, &value.x, readonly);
    ldk_ui_set_next_width(ui, ldk_ui_px(72.0f));
    changed |= s_editor_inspector_float_input(
        ui, entity, component_type, field, 1, &value.y, readonly);
    ldk_ui_set_next_width(ui, ldk_ui_px(72.0f));
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

    ldk_ui_set_next_width(ui, ldk_ui_px(60.0f));
    changed |= s_editor_inspector_float_input(
        ui, entity, component_type, field, 0, &value.x, readonly);
    ldk_ui_set_next_width(ui, ldk_ui_px(60.0f));
    changed |= s_editor_inspector_float_input(
        ui, entity, component_type, field, 1, &value.y, readonly);
    ldk_ui_set_next_width(ui, ldk_ui_px(60.0f));
    changed |= s_editor_inspector_float_input(
        ui, entity, component_type, field, 2, &value.z, readonly);
    ldk_ui_set_next_width(ui, ldk_ui_px(60.0f));
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
    Quat value = *(Quat *)field_value;

    bool changed = false;

    ldk_ui_set_next_width(ui, ldk_ui_px(60.0f));
    changed |= s_editor_inspector_float_input(
        ui, entity, component_type, field, 0, &value.x, readonly);
    ldk_ui_set_next_width(ui, ldk_ui_px(60.0f));
    changed |= s_editor_inspector_float_input(
        ui, entity, component_type, field, 1, &value.y, readonly);
    ldk_ui_set_next_width(ui, ldk_ui_px(60.0f));
    changed |= s_editor_inspector_float_input(
        ui, entity, component_type, field, 2, &value.z, readonly);
    ldk_ui_set_next_width(ui, ldk_ui_px(60.0f));
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

void ldki_editor_inspector_show(LDKEditorContext *editor)
{
  static LDKUIPoint scroll = {0};
  LDKECS *ecs;
  LDKGame *game;
  LDKEntity entity;
  LDKEntityInfo *info;
  char entity_name[LDK_ENTITY_NAME_MAX_LEN + 32];

  if (!editor)
  {
    return;
  }

  LDKUIContext *ui = &editor->ui;
  ecs = ldk_module_get(LDK_MODULE_ECS);
  game = ldk_game_get();

  if (!ecs || !game || !ldki_editor_selected_entity_get(editor, ecs, &entity))
  {
    ldk_ui_label(ui, "No entity selected.");
    return;
  }

  info = ldk_entity_info_get(&ecs->entity, entity);
  if (!info)
  {
    editor->selected_entity = x_handle_null();
    ldk_ui_label(ui, "No entity selected.");
    return;
  }

  scroll = ldk_ui_begin_scrollview(
      ui, scroll, LDK_UI_SCROLL_VERTICAL | LDK_UI_SCROLL_IF_NEEDED);

  ldki_editor_entity_display_name(
      info, entity, entity_name, sizeof(entity_name));

  LDKUIIcon icon = {0};
  icon.size =
      ldk_sizef(LDK_UI_DEFAULT_CONTROL_HEIGHT, LDK_UI_DEFAULT_CONTROL_HEIGHT);

  const rgba32 flat_color = editor->ui.theme.colors[LDK_UI_COLOR_CONTROL_TEXT];
  const rgba32 white = 0xFFFFFFFF;

  icon.texture =
      ldk_renderer_texture_ui_handle(editor->renderer, editor->ui_atlas);
  icon.color = flat_color;
  icon.uv = ldk_editor_icon_rects[LDK_EDITOR_ICON_OBJECT];
  ldk_ui_icon_label(ui, icon, entity_name);
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
  }

  ldk_ui_end_scrollview(ui);
}
