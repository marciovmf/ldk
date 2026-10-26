#include "ldk_editor_callbacks_internal.h"
#include <ldk_material_asset.h>
#include <ldk_mesh_asset.h>
#include <ldk_skybox_asset.h>
#include <ldk_texture_atlas.h>
#include <module/ldk_asset_manager.h>
#include "ldk_editor_color_picker.h"
#include "ldk_editor_internal.h"
#include "ldk_editor_ui.h"
#include "ldk_editor_scene_ops.h"
#include "ldk_ui_drag_n_drop.h"
#include "module/ldk_entity.h"
#include "module/ldk_ui.h"
#include <ldk_scene.h>
#include <ldk_property.h>
#include <component/ldk_mesh_source.h>
#include <component/ldk_instanced_mesh_source.h>
#include <component/ldk_keyframe_animation_source.h>
#include <module/ldk_scene_manager.h>
#include <module/ldk_ecs.h>
#include <stdx/stdx_string.h>
#include <stdx/stdx_array.h>
#include <ctype.h>
#include <errno.h>
#include <inttypes.h>
#include <math.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define LDK_INSPECTOR_LIKE_BUTTON_ID 0x4150504Cu

static bool s_editor_material_image_editor(LDKEditorContext *editor,
    const char *label, const char *dialog_title, LDKAssetImage *image,
    bool readonly, bool optional, const LDKMaterialIOContext *context);

static void s_editor_inspector_prefab_save(
    LDKEditorContext *editor, LDKEntity root)
{
  XFSPath path = {0};
  XFSPath relative = {0};
  LDKSceneResult result;

  if (!editor || !editor->project.loaded ||
      editor->editor_state != LDK_EDITOR_STATE_STOPED ||
      !ldk_os_dialog_show_save_file(
          editor->window, "Save Prefab", "*.prefab", &path))
  {
    return;
  }
  x_fs_path_normalize(&path);
  x_fs_path_change_extension(&path, ".prefab");
  if (x_fs_path_relative_to(
          &editor->project.run_root_path, &path, &relative) <= 0)
  {
    ldk_os_dialog_show_error(editor->window, "Invalid prefab path",
        "Prefab files must be saved inside the project runtree.");
    return;
  }
  if (!ldk_prefab_save_tml_file(root, path.buf, &result))
  {
    ldki_editor_log_error(editor, result.error);
    return;
  }
  ldki_editor_log_info(editor, "Prefab saved.");
}

enum
{
  LDK_EDITOR_INSPECTOR_INPUT_CAPACITY = 64,
};

static const LDKComponentFieldMeta s_editor_scene_properties_fields[] =
{
  {
    "Sky Ambient Color",
    LDK_FIELD_U32,
    offsetof(LDKSceneProperties, ambient_sky_color),
    LDK_FIELD_FLAG_NONE,
    LDK_FIELD_WIDGET_COLOR,
    0.0f,
    0.0f,
    NULL,
    NULL,
    "Sky Ambient Color",
  },
  {
    "Ground Ambient Color",
    LDK_FIELD_U32,
    offsetof(LDKSceneProperties, ambient_ground_color),
    LDK_FIELD_FLAG_NONE,
    LDK_FIELD_WIDGET_COLOR,
    0.0f,
    0.0f,
    NULL,
    NULL,
    "Ground Ambient Color",
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
    NULL,
    "Ambient Intensity",
  },
  {
    "Fog Enabled",
    LDK_FIELD_BOOL,
    offsetof(LDKSceneProperties, fog_enabled),
    LDK_FIELD_FLAG_NONE,
    LDK_FIELD_WIDGET_DEFAULT,
    0.0f,
    0.0f,
    NULL,
    NULL,
    "Fog Enabled",
  },
  {
    "Fog Color",
    LDK_FIELD_U32,
    offsetof(LDKSceneProperties, fog_color),
    LDK_FIELD_FLAG_NONE,
    LDK_FIELD_WIDGET_COLOR,
    0.0f,
    0.0f,
    NULL,
    NULL,
    "Fog Color",
  },
  {
    "Fog Density",
    LDK_FIELD_FLOAT,
    offsetof(LDKSceneProperties, fog_density),
    LDK_FIELD_FLAG_NONE,
    LDK_FIELD_WIDGET_FLOAT,
    0.0f,
    0.0f,
    NULL,
    NULL,
    "Fog Density",
  },
  {
    "Fog Start",
    LDK_FIELD_FLOAT,
    offsetof(LDKSceneProperties, fog_start),
    LDK_FIELD_FLAG_NONE,
    LDK_FIELD_WIDGET_FLOAT,
    0.0f,
    0.0f,
    NULL,
    NULL,
    "Fog Start",
  },
};

static const LDKComponentMeta s_editor_scene_properties_meta =
{
  "Scene Properties",
  0,
  sizeof(LDKSceneProperties),
  s_editor_scene_properties_fields,
  7,
  NULL,
  0,
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

typedef struct LDKEditorInspectorCollapsedGroup
{
  u64 owner_id;
  u32 group_index;
  u32 domain;
} LDKEditorInspectorCollapsedGroup;

enum
{
  LDK_EDITOR_INSPECTOR_GROUP_DOMAIN_COMPONENT = 1u,
  LDK_EDITOR_INSPECTOR_GROUP_DOMAIN_SYSTEM = 2u,
  LDK_EDITOR_INSPECTOR_GROUP_DOMAIN_ARRAY = 3u,
};

static LDKEditorInspectorInputState s_editor_inspector_input_state = {0};
static LDKEditorInspectorAreaState s_editor_inspector_area_state = {0};
static LDKEditorInspectorFlagsState s_editor_inspector_flags_state = {0};
static LDKEditorInspectorEulerState s_editor_inspector_euler_state = {0};
static LDKEditorInspectorCollapsedGroup *s_editor_inspector_collapsed_groups = NULL;
static u32 s_editor_inspector_collapsed_group_count = 0u;
static u32 s_editor_inspector_collapsed_group_capacity = 0u;
static u32 s_editor_inspector_row_group_depth = 0u;
static char
    s_editor_inspector_input_buffer[LDK_EDITOR_INSPECTOR_INPUT_CAPACITY] = {0};


static LDKUIPoint s_editor_inspector_scroll_begin(
    LDKUIContext *ui, LDKUIPoint scroll)
{
  scroll = ldk_ui_begin_scrollview(
      ui, scroll, LDK_UI_SCROLL_VERTICAL | LDK_UI_SCROLL_IF_NEEDED);
#ifdef LDK_UI_AREA_HEADER_HEIGHT
  /* The themed area API also supplies panel padding. Older UI versions keep
   * the padding configured by their scrollview implementation. */
  ldk_ui_set_padding(ui, ui->theme.panel_padding);
#endif
  return scroll;
}

static void s_editor_inspector_component_title(
    const char *name, char *buffer, size_t capacity)
{
  const char *source = name != NULL ? name : "Unknown component";
  size_t used = 0u;

  if (buffer == NULL || capacity == 0u)
  {
    return;
  }

  if (strncmp(source, "LDK", 3u) == 0 && isupper((unsigned char)source[3]))
  {
    source += 3;
  }
  for (size_t i = 0u; source[i] != '\0' && used + 1u < capacity; ++i)
  {
    unsigned char letter = (unsigned char)source[i];
    if (i > 0u && isupper(letter) &&
        (islower((unsigned char)source[i - 1u]) ||
            (isupper((unsigned char)source[i - 1u]) &&
                islower((unsigned char)source[i + 1u]))) &&
        used + 2u < capacity)
    {
      buffer[used++] = ' ';
    }
    buffer[used++] = letter < 128u ? (char)toupper(letter) : (char)letter;
  }
  buffer[used] = '\0';
}

static void s_editor_inspector_input_state_clear(void);
static bool s_editor_material_asset_editor(LDKEditorContext *editor,
    LDKAssetMaterial *material_asset, bool readonly,
    const LDKMaterialIOContext *context);

static void s_editor_inspector_column_update(LDKEditorContext *editor)
{
  ldk_editor_ui_row_column_update(editor);
}

static void s_editor_inspector_row_begin(
    LDKEditorContext *editor, const char *label)
{
  ldk_editor_ui_row_begin(editor, label, s_editor_inspector_row_group_depth);
}

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


static i32 s_editor_inspector_collapsed_group_index(
    u32 domain, u64 owner_id, u32 group_index)
{
  for (u32 i = 0; i < s_editor_inspector_collapsed_group_count; ++i)
  {
    const LDKEditorInspectorCollapsedGroup *group =
        &s_editor_inspector_collapsed_groups[i];
    if (group->domain == domain && group->owner_id == owner_id &&
        group->group_index == group_index)
    {
      return (i32)i;
    }
  }
  return -1;
}

static bool s_editor_inspector_group_expanded(
    u32 domain, u64 owner_id, u32 group_index)
{
  return s_editor_inspector_collapsed_group_index(
             domain, owner_id, group_index) < 0;
}

static void s_editor_inspector_group_expanded_set(
    u32 domain, u64 owner_id, u32 group_index, bool expanded)
{
  i32 collapsed_index = s_editor_inspector_collapsed_group_index(
      domain, owner_id, group_index);

  if (!expanded)
  {
    if (collapsed_index >= 0)
    {
      return;
    }

    if (s_editor_inspector_collapsed_group_count ==
        s_editor_inspector_collapsed_group_capacity)
    {
      u32 new_capacity = s_editor_inspector_collapsed_group_capacity == 0u
          ? 32u
          : s_editor_inspector_collapsed_group_capacity * 2u;
      LDKEditorInspectorCollapsedGroup *new_groups =
          (LDKEditorInspectorCollapsedGroup *)realloc(
              s_editor_inspector_collapsed_groups,
              sizeof(*new_groups) * new_capacity);
      if (!new_groups)
      {
        return;
      }
      s_editor_inspector_collapsed_groups = new_groups;
      s_editor_inspector_collapsed_group_capacity = new_capacity;
    }

    LDKEditorInspectorCollapsedGroup *group =
        &s_editor_inspector_collapsed_groups[
            s_editor_inspector_collapsed_group_count++];
    group->domain = domain;
    group->owner_id = owner_id;
    group->group_index = group_index;
    return;
  }

  if (collapsed_index < 0)
  {
    return;
  }

  u32 index = (u32)collapsed_index;
  u32 last = --s_editor_inspector_collapsed_group_count;
  s_editor_inspector_collapsed_groups[index] =
      s_editor_inspector_collapsed_groups[last];
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
    u32 value_index, char *buffer, u32 buffer_size, const float *float_value)
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
    input_buffer = s_editor_inspector_input_buffer;
    if (float_value)
    {
      ldk_ui_format_float(input_buffer,
          LDK_EDITOR_INSPECTOR_INPUT_CAPACITY, *float_value);
    }
    else
    {
      snprintf(input_buffer, LDK_EDITOR_INSPECTOR_INPUT_CAPACITY, "%s", buffer);
    }
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
    if (ldk_entity_flags_get(&ecs->entity, entity) !=
        s_editor_inspector_flags_state.draft_value)
    {
      ldk_entity_flags_set(&ecs->entity, entity,
          s_editor_inspector_flags_state.draft_value);
      ldki_editor_scene_dirty_set(editor, true);
    }
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
  s_editor_inspector_row_begin(editor, "Flags");

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
      else if (parsed != flags)
      {
        ldk_entity_flags_set(&ecs->entity, entity, parsed);
        ldki_editor_scene_dirty_set(editor, true);
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
  case LDK_FIELD_U64:
    snprintf(out, out_size, "%" PRIu64, *(const u64 *)value);
    break;
  case LDK_FIELD_U32:
    snprintf(out, out_size, "%u", *(const u32 *)value);
    break;
  case LDK_FIELD_FLOAT:
    ldk_ui_format_float(out, (u32)out_size, *(const float *)value);
    break;
  case LDK_FIELD_STRING:
  {
    const XSmallstr *string = (const XSmallstr *)value;

    if (string->length > X_SMALLSTR_MAX_LENGTH ||
        string->buf[string->length] != 0 ||
        strlen(string->buf) != string->length)
    {
      snprintf(out, out_size, "<invalid string>");
    }
    else
    {
      snprintf(out, out_size, "%s", string->buf);
    }
    break;
  }
  case LDK_FIELD_VEC2:
  {
    const Vec2 *v = (const Vec2 *)value;
    char x[32];
    char y[32];
    ldk_ui_format_float(x, (u32)sizeof(x), v->x);
    ldk_ui_format_float(y, (u32)sizeof(y), v->y);
    snprintf(out, out_size, "%s, %s", x, y);
    break;
  }
  case LDK_FIELD_VEC3:
  {
    const Vec3 *v = (const Vec3 *)value;
    char x[32];
    char y[32];
    char z[32];
    ldk_ui_format_float(x, (u32)sizeof(x), v->x);
    ldk_ui_format_float(y, (u32)sizeof(y), v->y);
    ldk_ui_format_float(z, (u32)sizeof(z), v->z);
    snprintf(out, out_size, "%s, %s, %s", x, y, z);
    break;
  }
  case LDK_FIELD_VEC4:
  {
    const Vec4 *v = (const Vec4 *)value;
    char x[32];
    char y[32];
    char z[32];
    char w[32];
    ldk_ui_format_float(x, (u32)sizeof(x), v->x);
    ldk_ui_format_float(y, (u32)sizeof(y), v->y);
    ldk_ui_format_float(z, (u32)sizeof(z), v->z);
    ldk_ui_format_float(w, (u32)sizeof(w), v->w);
    snprintf(out, out_size, "%s, %s, %s, %s", x, y, z, w);
    break;
  }
  case LDK_FIELD_QUAT:
  {
    const Quat *q = (const Quat *)value;
    char x[32];
    char y[32];
    char z[32];
    char w[32];
    ldk_ui_format_float(x, (u32)sizeof(x), q->x);
    ldk_ui_format_float(y, (u32)sizeof(y), q->y);
    ldk_ui_format_float(z, (u32)sizeof(z), q->z);
    ldk_ui_format_float(w, (u32)sizeof(w), q->w);
    snprintf(out, out_size, "%s, %s, %s, %s", x, y, z, w);
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
  case LDK_FIELD_ASSET_FONT:
    snprintf(out, out_size, "<asset font>");
    break;
  case LDK_FIELD_ASSET_IMAGE:
    snprintf(out, out_size, "<asset image>");
    break;
  case LDK_FIELD_RESOURCE_MESH:
    snprintf(out, out_size, "<resource mesh>");
    break;
  case LDK_FIELD_ASSET_MATERIAL:
    snprintf(out, out_size, "<asset material>");
    break;
  case LDK_FIELD_ASSET_SKYBOX:
    snprintf(out, out_size, "<asset skybox>");
    break;
  case LDK_FIELD_ASSET_AUDIO:
  case LDK_FIELD_ASSET_KEYFRAME_ANIMATION:
    snprintf(out, out_size, "<asset audio>");
    break;
  default:
    snprintf(out, out_size, "<unsupported>");
    break;
  }
}

/* Generic array undo keys: 0x8..0xE = element field, 0xFF = list
 * structure. The remaining bits encode reflected list field, element index
 * and reflected element field. Indices stay stable across reversed
 * structural operations in the Undo history. */
#define LDK_EDITOR_ARRAY_FIELD_BASE 0x80000000u
#define LDK_EDITOR_ARRAY_STRUCT_BASE 0xff000000u

static size_t s_editor_inspector_undo_field_size(
    const LDKComponentFieldMeta *field);
typedef struct LDKInspectorArrayEditContext
{
  LDKEntity entity;
  u32 component_type;
  u32 array_field_index;
  u32 element_index;
  const LDKComponentMeta *element_meta;
  const LDKArrayFieldMeta *array_meta;
  void *component;
  void *element;
} LDKInspectorArrayEditContext;

/* Only active while rendering a single reflected array element. */
static LDKInspectorArrayEditContext s_editor_array_edit;

typedef struct LDKInspectorNumericRowContext
{
  LDKEntity entity;
  u32 component_type;
  const LDKComponentFieldMeta *field;
} LDKInspectorNumericRowContext;

static u32 s_editor_inspector_numeric_row_input(LDKUIContext *ui, void *user,
    u32 index, char *buffer, u32 capacity, const float *float_value)
{
  LDKInspectorNumericRowContext *context = user;
  return s_editor_inspector_field_input_box(ui, context->entity,
      context->component_type, context->field, index, buffer, capacity,
      float_value);
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

  ldk_ui_begin_disabled(ui, readonly);
  result = s_editor_inspector_field_input_box(ui, entity, component_type, field,
      value_index, buffer, (u32)sizeof(buffer), &original);
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

static bool s_editor_inspector_vector_row_input(LDKUIContext *ui, void *user,
    u32 index, float *value, bool readonly)
{
  LDKInspectorNumericRowContext *context = user;
  return s_editor_inspector_float_input(ui, context->entity,
      context->component_type, context->field, index, value, readonly);
}

/* Generic reflected edits use the same writer as animation playback.
 * Scene properties (component_type == 0) are edited in a temporary copy. */
static bool s_editor_inspector_property_write(LDKEntity entity,
    u32 component_type, const LDKComponentFieldMeta *field, const void *raw)
{
  if (!field || !raw)
  {
    return false;
  }
  if (s_editor_array_edit.element &&
      s_editor_array_edit.component_type == component_type &&
      s_editor_array_edit.element_meta &&
      field >= s_editor_array_edit.element_meta->fields &&
      field < s_editor_array_edit.element_meta->fields +
          s_editor_array_edit.element_meta->field_count)
  {
    size_t size = s_editor_inspector_undo_field_size(field);
    if (!size || field->offset > s_editor_array_edit.element_meta->size ||
        size > s_editor_array_edit.element_meta->size - field->offset)
      return false;
    void *destination = (u8 *)s_editor_array_edit.element + field->offset;
    if (memcmp(destination, raw, size) != 0)
      memcpy(destination, raw, size);
    return true;
  }
  if (!component_type)
  {
    return false;
  }
  /* An array limit is an ordinary reflected u32 field. Validate it against
   * all collections that use it, rather than special-casing a component. */
  if (field->type == LDK_FIELD_U32)
  {
    const LDKComponentMeta *meta =
        ldk_property_component_meta(component_type);
    void *component = ldk_ecs_component_get(entity, component_type);
    if (meta && component)
    {
      for (u32 i = 0; i < meta->field_count; ++i)
      {
        const LDKComponentFieldMeta *array_field = &meta->fields[i];
        if (array_field->type != LDK_FIELD_ARRAY ||
            !array_field->array_meta ||
            !array_field->array_meta->limit_field ||
            strcmp(array_field->array_meta->limit_field, field->name) != 0 ||
            array_field->offset > meta->size ||
            sizeof(XArray *) > meta->size - array_field->offset)
          continue;
        XArray *array = *(XArray **)((u8 *)component + array_field->offset);
        if (array && x_array_count(array) > *(const u32 *)raw)
          return false;
      }
    }
  }
  LDKPropertyValue value = {0};
  value.type = field->type;
  switch (field->type)
  {
  case LDK_FIELD_BOOL: value.integer = *(const bool *)raw; break;
  case LDK_FIELD_I32: value.integer = *(const i32 *)raw; break;
  case LDK_FIELD_U32: value.integer = *(const u32 *)raw; break;
  case LDK_FIELD_ENUM:
    if (!field->enum_meta || !field->enum_meta->read) return false;
    value.integer = field->enum_meta->read(raw);
    break;
  case LDK_FIELD_FLOAT: value.vector.x = *(const float *)raw; break;
  case LDK_FIELD_VEC2:
  {
    const Vec2 *v = (const Vec2 *)raw;
    value.vector = vec4_make(v->x, v->y, 0.0f, 0.0f);
    break;
  }
  case LDK_FIELD_VEC3:
  {
    const Vec3 *v = (const Vec3 *)raw;
    value.vector = vec4_make(v->x, v->y, v->z, 0.0f);
    break;
  }
  case LDK_FIELD_VEC4: value.vector = *(const Vec4 *)raw; break;
  case LDK_FIELD_QUAT:
  {
    const Quat *q = (const Quat *)raw;
    value.vector = vec4_make(q->x, q->y, q->z, q->w);
    break;
  }
  case LDK_FIELD_STRING: value.string = *(const XSmallstr *)raw; break;
  default: return false;
  }
  return ldk_property_set(entity, component_type, field->name, &value);
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
  if (component_type)
  {
    return s_editor_inspector_property_write(
        entity, component_type, field, &rotation);
  }
  *(Quat *)field_value = rotation;
  return true;
}

static LDKUIId s_editor_inspector_euler_field_draw(LDKUIContext *ui,
    LDKEntity entity, u32 component_type,
    const LDKComponentFieldMeta *field, void *component,
    void *field_value, bool readonly)
{
  static const char *const axis_names[3] = {"X", "Y", "Z"};
  LDKEditorInspectorEulerState local_state;
  LDKEditorInspectorEulerState *state = &local_state;
  Quat source = *(const Quat *)field_value;
  LDKUIId focused_input = 0;

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
    float axis_value;
    float parsed;
    u32 result;
    bool focused;

    ldk_ui_push_id_u32(ui, axis);
    ldk_ui_set_next_width(ui, ldk_ui_px(12.0f));
    ldk_ui_label(ui, axis_names[axis]);
    axis_value = s_editor_inspector_euler_axis_get(state->degrees, axis);

    ldk_ui_begin_disabled(ui, readonly);
    result = s_editor_inspector_field_input_box(ui, entity, component_type,
        field, axis, buffer, (u32)sizeof(buffer), &axis_value);
    ldk_ui_end_disabled(ui);

    focused = !readonly && ui->focused_id == ui->last_id &&
        ui->input_box_id == ui->last_id;
    if (focused)
    {
      focused_input = ui->last_id;
    }
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
  return focused_input;
}

typedef struct LDKInspectorEulerRowContext
{
  LDKEntity entity;
  u32 component_type;
  const LDKComponentFieldMeta *field;
  void *component;
  void *field_value;
  bool readonly;
} LDKInspectorEulerRowContext;

static LDKUIId s_editor_inspector_euler_row_content(LDKUIContext *ui, void *user)
{
  LDKInspectorEulerRowContext *context = user;
  return s_editor_inspector_euler_field_draw(ui, context->entity,
      context->component_type, context->field, context->component,
      context->field_value, context->readonly);
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
  s_editor_inspector_row_begin(editor, "Mesh");

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

  names = (const char **)x_arena_alloc(
      ui->frame_arena, (size_t)mesh_count * sizeof(*names));
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
}

static bool s_editor_inspector_asset_path_validate(
    LDKEditorContext *editor, const XFSPath *path, const char *title,
    const char *message, LDKAssetPath *out_asset_path)
{
  XFSPath normalized = {0};
  XFSPath root;
  XFSPath relative = {0};

  if (!editor || !path || !out_asset_path)
  {
    return false;
  }

  normalized = *path;
  if (!x_fs_path_is_absolute(&normalized))
  {
    for (size_t i = 0; normalized.buf[i]; ++i)
    {
      if (normalized.buf[i] == '\\') normalized.buf[i] = '/';
    }
    return ldk_asset_path_set(out_asset_path, normalized.buf);
  }

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

  for (size_t i = 0; relative.buf[i]; ++i)
  {
    if (relative.buf[i] == '\\')
    {
      relative.buf[i] = '/';
    }
  }

  if (!ldk_asset_path_set(out_asset_path, relative.buf))
  {
    ldk_os_dialog_show_error(editor->window, title, message);
    return false;
  }
  return true;
}

static bool s_editor_inspector_mesh_asset_field(LDKEditorContext *editor,
    const char *label, LDKAssetMesh *value, bool readonly)
{
  LDKAssetManager *assets;
  LDKAssetHandle handle;
  const LDKAssetInfo *info;
  XFSPath path = {0};
  bool assign = false;

  if (!editor || !value)
  {
    return false;
  }

  assets = ldk_module_get(LDK_MODULE_ASSET_MANAGER);
  handle.h = value->h;
  info = assets && !x_handle_is_null(value->h)
      ? ldk_asset_get_info_const(assets, handle)
      : NULL;

  assign = ldk_editor_ui_row_slot_mesh(editor, label,
      s_editor_inspector_row_group_depth, info, readonly, &path, NULL) ==
      LDK_EDITOR_UI_ROW_SLOT_ASSIGN;

  if (!assign)
  {
    return false;
  }

  LDKAssetPath asset_path;
  if (!s_editor_inspector_asset_path_validate(editor, &path, "Mesh",
          "Choose a mesh inside the project's runtree folder.", &asset_path))
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
      ldk_asset_manager_mesh_load_shared(assets, asset_path.buf, &result);
  if (x_handle_is_null(asset.h))
  {
    ldki_editor_log_error(editor, result.error);
    return false;
  }

  *value = asset;
  return true;
}

static bool s_editor_inspector_font_asset_field(LDKEditorContext *editor,
    const char *label, LDKAssetFont *value, bool readonly)
{
  LDKAssetManager *assets;
  LDKAssetHandle handle;
  const LDKAssetInfo *info;
  XFSPath path = {0};
  bool assign = false;

  if (!editor || !value)
  {
    return false;
  }

  assets = ldk_module_get(LDK_MODULE_ASSET_MANAGER);
  handle.h = value->h;
  info = assets && !x_handle_is_null(value->h)
      ? ldk_asset_get_info_const(assets, handle)
      : NULL;
  assign = ldk_editor_ui_row_slot_font(editor, label,
      s_editor_inspector_row_group_depth, info, readonly, &path, NULL) ==
      LDK_EDITOR_UI_ROW_SLOT_ASSIGN;

  if (!assign)
  {
    return false;
  }

  LDKAssetPath asset_path;
  if (!s_editor_inspector_asset_path_validate(editor, &path, "Font",
          "Choose a font inside the project's runtree folder.", &asset_path))
  {
    return false;
  }
  if (!assets)
  {
    ldki_editor_log_error(editor, "Asset manager is unavailable.");
    return false;
  }

  LDKAssetFont asset =
      ldk_asset_manager_font_load_shared(assets, asset_path.buf);
  if (x_handle_is_null(asset.h))
  {
    ldki_editor_log_error(editor, "Failed to load font asset.");
    return false;
  }

  *value = asset;
  return true;
}

static bool s_editor_inspector_audio_asset_field(LDKEditorContext *editor,
    const char *label, LDKAssetAudio *value, bool readonly)
{
  LDKAssetManager *assets;
  LDKAssetHandle handle;
  const LDKAssetInfo *info;
  XFSPath path = {0};
  bool assign = false;

  if (!editor || !value)
  {
    return false;
  }

  assets = ldk_module_get(LDK_MODULE_ASSET_MANAGER);
  handle.h = value->h;
  info = assets && !x_handle_is_null(value->h)
      ? ldk_asset_get_info_const(assets, handle)
      : NULL;
  assign = ldk_editor_ui_row_slot_audio(editor, label,
      s_editor_inspector_row_group_depth, info, readonly, &path, NULL) ==
      LDK_EDITOR_UI_ROW_SLOT_ASSIGN;

  if (!assign)
  {
    return false;
  }

  LDKAssetPath asset_path;
  if (!s_editor_inspector_asset_path_validate(editor, &path, "Audio",
          "Choose an audio file inside the project's runtree folder.",
          &asset_path))
  {
    return false;
  }

  if (!assets)
  {
    ldki_editor_log_error(editor, "Asset manager is unavailable.");
    return false;
  }

  LDKAssetAudio asset =
      ldk_asset_manager_audio_load_shared(assets, asset_path.buf);
  if (x_handle_is_null(asset.h))
  {
    ldki_editor_log_error(editor, "Failed to load audio asset.");
    return false;
  }

  *value = asset;
  return true;
}

static bool s_editor_inspector_keyframe_animation_asset_field(LDKEditorContext *editor,
    const char *label, LDKAssetKeyframeAnimation *value, bool readonly)
{
  LDKAssetManager *assets;
  LDKAssetHandle handle;
  const LDKAssetInfo *info;
  XFSPath path = {0};
  bool assign = false;

  if (!editor || !value)
  {
    return false;
  }

  assets = ldk_module_get(LDK_MODULE_ASSET_MANAGER);
  handle.h = value->h;
  info = assets && !x_handle_is_null(value->h)
      ? ldk_asset_get_info_const(assets, handle)
      : NULL;
  assign = ldk_editor_ui_row_slot_animation(editor, label,
      s_editor_inspector_row_group_depth, info, readonly, &path, NULL) ==
      LDK_EDITOR_UI_ROW_SLOT_ASSIGN;

  if (!assign)
  {
    return false;
  }

  LDKAssetPath asset_path;
  if (!s_editor_inspector_asset_path_validate(editor, &path, "Animation",
          "Choose an animation file inside the project's runtree folder.",
          &asset_path))
  {
    return false;
  }

  const char *suffix = strrchr(asset_path.buf, '.');
  if (!suffix || strcmp(suffix, ".anim") != 0)
  {
    ldki_editor_log_error(editor, "Choose a .anim asset.");
    return false;
  }

  if (!assets)
  {
    ldki_editor_log_error(editor, "Asset manager is unavailable.");
    return false;
  }

  LDKAssetKeyframeAnimation asset =
      ldk_asset_manager_keyframe_animation_load_shared(assets, asset_path.buf);
  if (x_handle_is_null(asset.h))
  {
    ldki_editor_log_error(editor, "Failed to load animation asset.");
    return false;
  }

  *value = asset;
  return true;
}

static bool s_editor_inspector_material_asset_field(LDKEditorContext *editor,
    const char *label, LDKAssetMaterial *value, bool readonly, bool optional)
{
  LDKAssetManager *assets;
  LDKAssetHandle handle;
  const LDKAssetInfo *info;
  XFSPath path = {0};
  bool assign = false;
  bool clear = false;

  if (!editor || !value)
  {
    return false;
  }

  assets = ldk_module_get(LDK_MODULE_ASSET_MANAGER);
  handle.h = value->h;
  info = assets && !x_handle_is_null(value->h)
      ? ldk_asset_get_info_const(assets, handle)
      : NULL;

  LDKEditorUIRowSlotAction action = ldk_editor_ui_row_slot_material(editor,
      label, s_editor_inspector_row_group_depth, info, readonly, optional,
      !x_handle_is_null(value->h), &path, NULL);
  clear = action == LDK_EDITOR_UI_ROW_SLOT_CLEAR;
  assign = action == LDK_EDITOR_UI_ROW_SLOT_ASSIGN;

  if (clear)
  {
    *value = ldk_asset_material_null();
    return true;
  }

  if (!assign)
  {
    return false;
  }

  LDKAssetPath asset_path;
  if (!s_editor_inspector_asset_path_validate(editor, &path, "Material",
          "Choose a material inside the project's runtree folder.",
          &asset_path))
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

  LDKAssetMaterial asset = ldk_asset_manager_material_load_shared(
      &context, asset_path.buf, &result);
  if (x_handle_is_null(asset.h))
  {
    ldki_editor_log_error(editor, result.error);
    return false;
  }

  *value = asset;
  return true;
}

static bool s_editor_inspector_skybox_asset_field(LDKEditorContext *editor,
    const char *label, LDKAssetSkybox *value, bool readonly, bool optional)
{
  LDKAssetManager *assets;
  LDKAssetHandle handle;
  const LDKAssetInfo *info;
  XFSPath path = {0};
  bool assign = false;
  bool clear = false;

  if (!editor || !value)
  {
    return false;
  }

  assets = ldk_module_get(LDK_MODULE_ASSET_MANAGER);
  handle.h = value->h;
  info = assets && !x_handle_is_null(value->h)
      ? ldk_asset_get_info_const(assets, handle)
      : NULL;

  LDKEditorUIRowSlotAction action = ldk_editor_ui_row_slot_skybox(editor,
      label, s_editor_inspector_row_group_depth, info, readonly, optional,
      !x_handle_is_null(value->h), &path, NULL);
  clear = action == LDK_EDITOR_UI_ROW_SLOT_CLEAR;
  assign = action == LDK_EDITOR_UI_ROW_SLOT_ASSIGN;

  if (clear)
  {
    *value = ldk_asset_skybox_null();
    return true;
  }

  if (!assign)
  {
    return false;
  }

  LDKAssetPath asset_path;
  if (!s_editor_inspector_asset_path_validate(editor, &path, "Skybox",
          "Choose a skybox inside the project's runtree folder.",
          &asset_path))
  {
    return false;
  }

  if (!assets)
  {
    ldki_editor_log_error(editor, "Asset manager is unavailable.");
    return false;
  }

  LDKSkyboxIOContext context = {0};
  LDKSkyboxIOResult result;
  context.assets = assets;

  LDKAssetSkybox asset = ldk_asset_manager_skybox_load_shared(
      &context, asset_path.buf, &result);
  if (x_handle_is_null(asset.h))
  {
    ldki_editor_log_error(editor, result.error);
    return false;
  }

  *value = asset;
  return true;
}

/* Both the runtime source and the Animation window use the same index. */
static void s_editor_inspector_keyframe_current_draw(
    LDKEditorContext *editor, LDKEntity entity,
    const LDKKeyFrameAnimationSource *source, bool readonly)
{
  LDKUIContext *ui = &editor->ui;
  LDKAssetManager *assets = ldk_module_get(LDK_MODULE_ASSET_MANAGER);
  s_editor_inspector_row_begin(editor, "Current Animation");
  if (!source->animation_count)
  {
    ldk_ui_label(ui, "(none)");
  }
  else
  {
    const char **names = calloc(source->animation_count, sizeof(*names));
    if (names)
    {
      for (u32 i = 0; i < source->animation_count; ++i)
      {
        LDKAssetHandle handle = {.h = source->animations[i].h};
        const LDKAssetInfo *info = assets
            ? ldk_asset_get_info_const(assets, handle) : NULL;
        names[i] = info ? info->asset_path.buf : "(missing)";
      }
      u32 current = source->current_animation >= 0 &&
          (u32)source->current_animation < source->animation_count
          ? (u32)source->current_animation : 0u;
      ldk_ui_begin_disabled(ui, readonly);
      u32 next = ldk_ui_combo_box(ui, names, source->animation_count, current);
      if (!readonly && next != current)
      {
        if (ldk_keyframe_animation_source_set_current(entity, (i32)next))
          ldki_editor_scene_dirty_set(editor, true);
      }
      ldk_ui_end_disabled(ui);
      free(names);
    }
  }
  ldk_ui_end_horizontal(ui);
}

static void s_editor_inspector_keyframe_animations_draw(
    LDKEditorContext *editor, LDKEntity entity,
    LDKKeyFrameAnimationSource *source, bool readonly)
{
  LDKUIContext *ui = &editor->ui;
  ldk_ui_label(ui, "Animations");
  for (u32 i = 0; i < source->animation_count; ++i)
  {
    char label[32];
    snprintf(label, sizeof(label), "Animation %u", i);
    ldk_ui_push_id_u32(ui, i);
    LDKAssetKeyframeAnimation asset = source->animations[i];
    if (s_editor_inspector_keyframe_animation_asset_field(
            editor, label, &asset, readonly))
    {
      if (!ldk_keyframe_animation_source_replace(entity, i, asset))
        ldki_editor_log_error(editor, "Unable to replace animation (already assigned?).");
      else
        ldki_editor_scene_dirty_set(editor, true);
    }
    ldk_ui_begin_disabled(ui, readonly);
    if (ldk_ui_button(ui, "Remove"))
    {
      if (ldk_keyframe_animation_source_remove(entity, i))
        ldki_editor_scene_dirty_set(editor, true);
      ldk_ui_end_disabled(ui);
      ldk_ui_pop_id(ui);
      break;
    }
    ldk_ui_end_disabled(ui);
    ldk_ui_pop_id(ui);
  }
  LDKAssetKeyframeAnimation addition = ldk_asset_keyframe_animation_null();
  if (s_editor_inspector_keyframe_animation_asset_field(
          editor, "Add Animation", &addition, readonly))
  {
    if (!ldk_keyframe_animation_source_add(entity, addition))
      ldki_editor_log_error(editor, "Unable to add animation (already assigned?).");
    else
      ldki_editor_scene_dirty_set(editor, true);
  }
}

static const char *s_editor_inspector_field_display_name(
    const LDKComponentFieldMeta *field)
{
  if (!field)
  {
    return "";
  }

  if (field->display_name && field->display_name[0] != 0)
  {
    return field->display_name;
  }

  return field->name ? field->name : "";
}

/* Atlas region selection uses a persistent region ID rather than the atlas
 * index. The atlas itself is an LDKAssetTextureAtlas field of the same system
 * or component, referenced by the widget's source= annotation. */
static void s_editor_inspector_atlas_region_field_draw(
    LDKEditorContext *editor, LDKEntity entity, u32 component_type,
    const LDKComponentMeta *meta, const LDKComponentFieldMeta *field,
    void *component, bool readonly)
{
  LDKUIContext *ui = &editor->ui;
  const LDKComponentFieldMeta *source = NULL;
  const LDKTextureAtlas *atlas = NULL;
  LDKAssetManager *assets = ldk_module_get(LDK_MODULE_ASSET_MANAGER);
  u64 *chosen = (u64 *)((u8 *)component + field->offset);
  LDKEditorUIUndoProperty undo_property = {0};
  if (component_type && !readonly && ldk_property_field_supported(field))
  {
    undo_property.key.owner = ((u64)entity.version << 32) | entity.index;
    undo_property.key.property = ((u64)component_type << 32) | field->offset;
    undo_property.type = field->type;
    undo_property.value = chosen;
    undo_property.size = sizeof(*chosen);
  }

  if (field->widget_source)
  {
    for (u32 i = 0; i < meta->field_count; ++i)
    {
      const LDKComponentFieldMeta *candidate = &meta->fields[i];
      if (candidate->name &&
          strcmp(candidate->name, field->widget_source) == 0 &&
          candidate->type == LDK_FIELD_ASSET_TEXTURE_ATLAS &&
          candidate->offset <= meta->size &&
          sizeof(LDKAssetTextureAtlas) <= meta->size - candidate->offset)
      {
        source = candidate;
        break;
      }
    }
  }

  if (source && assets)
  {
    const LDKAssetTextureAtlas *asset =
        (const LDKAssetTextureAtlas *)((const u8 *)component + source->offset);
    const LDKAssetTextureAtlasData *data =
        ldk_asset_manager_texture_atlas_get_const(assets, *asset);
    atlas = data ? &data->atlas : NULL;
  }

  u32 region_count = atlas ? atlas->region_count : 0u;
  const char **names = x_arena_alloc(ui->frame_arena,
      (size_t)(region_count + 2u) * sizeof(*names));
  if (!names)
  {
    s_editor_inspector_row_begin(editor,
        s_editor_inspector_field_display_name(field));
    ldk_ui_label(ui, "Atlas regions unavailable");
    ldk_ui_end_horizontal(ui);
    return;
  }

  names[0] = "(None)";
  u32 selected = *chosen ? region_count + 1u : 0u;
  char missing[96];
  for (u32 i = 0; i < region_count; ++i)
  {
    const LDKTextureAtlasRegion *region = &atlas->regions[i];
    names[i + 1u] = region->name;
    if (*chosen == region->id)
      selected = i + 1u;
  }
  if (selected == region_count + 1u)
  {
    snprintf(missing, sizeof(missing), "Missing sprite ID: %" PRIu64, *chosen);
    names[selected] = missing;
  }

  u32 count = region_count + 1u +
      (selected == region_count + 1u ? 1u : 0u);
  u32 next = ldk_editor_ui_row_atlas_region(editor,
      s_editor_inspector_field_display_name(field),
      s_editor_inspector_row_group_depth, names, count, selected,
      readonly || !atlas || !source,
      undo_property.size ? &undo_property : NULL);
  if (!readonly && atlas && source && next != selected && next <= region_count)
    *chosen = next == 0u ? 0u : atlas->regions[next - 1u].id;
}

static bool s_editor_inspector_texture_atlas_asset_field(
    LDKEditorContext *editor, const char *label,
    LDKAssetTextureAtlas *value, bool readonly)
{
  LDKAssetManager *assets = ldk_module_get(LDK_MODULE_ASSET_MANAGER);
  LDKAssetHandle handle = {value->h};
  const LDKAssetInfo *info = assets && !x_handle_is_null(value->h)
      ? ldk_asset_get_info_const(assets, handle) : NULL;
  XFSPath path = {0};
  bool assign = false;
  bool clear = false;

  LDKEditorUIRowSlotAction action = ldk_editor_ui_row_slot_texture_atlas(editor,
      label, s_editor_inspector_row_group_depth, info, readonly,
      !x_handle_is_null(value->h), &path, NULL);
  clear = action == LDK_EDITOR_UI_ROW_SLOT_CLEAR;
  assign = action == LDK_EDITOR_UI_ROW_SLOT_ASSIGN;

  if (clear)
  {
    *value = ldk_asset_texture_atlas_null();
    return true;
  }
  if (!assign)
    return false;
  LDKAssetPath asset_path;
  if (!s_editor_inspector_asset_path_validate(editor, &path, "Texture Atlas",
          "Choose an atlas inside the project's runtree folder.",
          &asset_path))
    return false;
  const char *ext = strrchr(asset_path.buf, '.');
  if (!ext || !x_slice_eq_ci(x_slice(ext), x_slice(".atlas")))
  {
    ldki_editor_log_error(editor, "Choose a .atlas asset.");
    return false;
  }
  if (!assets)
    return false;
  LDKAssetTextureAtlas selected =
      ldk_asset_manager_texture_atlas_load_shared(assets, asset_path.buf);
  if (x_handle_is_null(selected.h))
  {
    ldki_editor_log_error(editor, "Failed to load texture atlas.");
    return false;
  }
  if (selected.h.index == value->h.index &&
      selected.h.version == value->h.version)
    return false;
  *value = selected;
  return true;
}

/* Undo identities use the versioned entity handle and reflected field offset. */
static size_t s_editor_inspector_undo_field_size(
    const LDKComponentFieldMeta *field)
{
  if (!ldk_property_field_supported(field))
  {
    return 0;
  }
  switch (field->type)
  {
  case LDK_FIELD_BOOL: return sizeof(bool);
  case LDK_FIELD_I32: return sizeof(i32);
  case LDK_FIELD_U32: return sizeof(u32);
  case LDK_FIELD_U64: return sizeof(u64);
  case LDK_FIELD_FLOAT: return sizeof(float);
  case LDK_FIELD_VEC2: return sizeof(Vec2);
  case LDK_FIELD_VEC3: return sizeof(Vec3);
  case LDK_FIELD_VEC4: return sizeof(Vec4);
  case LDK_FIELD_QUAT: return sizeof(Quat);
  case LDK_FIELD_STRING: return sizeof(XSmallstr);
  case LDK_FIELD_ENUM:
    return field->enum_meta ? field->enum_meta->size : 0;
  default: return 0;
  }
}

/* Use a reflected scalar as the title of a collapsed group or array item.
 * Fall back to the ordinary title for empty, missing, or invalid values. */
static bool s_editor_inspector_headline_format(
    const LDKComponentFieldMeta *field, const void *data, size_t data_size,
    char *out, size_t out_size)
{
  if (!field || !data || !out || !out_size ||
      !(field->flags & LDK_FIELD_FLAG_HEADLINE))
  {
    return false;
  }

  size_t size = 0u;
  switch (field->type)
  {
  case LDK_FIELD_BOOL: size = sizeof(bool); break;
  case LDK_FIELD_I32: size = sizeof(i32); break;
  case LDK_FIELD_U32: size = sizeof(u32); break;
  case LDK_FIELD_U64: size = sizeof(u64); break;
  case LDK_FIELD_FLOAT: size = sizeof(float); break;
  case LDK_FIELD_VEC2: size = sizeof(Vec2); break;
  case LDK_FIELD_VEC3: size = sizeof(Vec3); break;
  case LDK_FIELD_VEC4: size = sizeof(Vec4); break;
  case LDK_FIELD_QUAT: size = sizeof(Quat); break;
  case LDK_FIELD_STRING: size = sizeof(XSmallstr); break;
  case LDK_FIELD_ENUM:
    size = field->enum_meta ? field->enum_meta->size : 0u;
    break;
  default: break;
  }
  if (!size || field->offset > data_size || size > data_size - field->offset)
  {
    return false;
  }

  out[0] = 0;
  s_editor_inspector_field_value_format(
      out, out_size, field, (const u8 *)data + field->offset);
  return out[0] != 0 && strcmp(out, "<invalid string>") != 0;
}

static LDKEditorUIUndoProperty s_editor_inspector_undo_property(
    LDKEntity entity, u32 component_type, const LDKComponentMeta *meta,
    const LDKComponentFieldMeta *field, const void *value)
{
  LDKEditorUIUndoProperty property = {0};
  size_t size = s_editor_inspector_undo_field_size(field);
  /* A zero component type identifies a scene system. Scene properties use
   * the null entity handle and are not currently tracked by undo. */
  if (component_type == 0 && x_handle_is_null(entity))
  {
    return property;
  }
  if (!size || !value || !meta || size > meta->size ||
      field->offset > meta->size - size)
  {
    return property;
  }
  property.key.owner = ((u64)entity.version << 32) | entity.index;
  if (s_editor_array_edit.element &&
      s_editor_array_edit.element_meta == meta &&
      s_editor_array_edit.array_field_index < 112u &&
      s_editor_array_edit.element_index < 65536u &&
      field >= meta->fields && field < meta->fields + meta->field_count &&
      (u32)(field - meta->fields) < 256u)
  {
    property.key.property = ((u64)component_type << 32) |
        LDK_EDITOR_ARRAY_FIELD_BASE |
        (s_editor_array_edit.array_field_index << 24) |
        (s_editor_array_edit.element_index << 8) |
        (u32)(field - meta->fields);
  }
  else
  {
    property.key.property = ((u64)component_type << 32) | field->offset;
  }
  property.type = field->type;
  property.value = value;
  property.size = size;
  return property;
}

/* System fields use the full 64-bit system ID as owner, and a zero
 * component type in the upper 32 bits of the property key. The data is
 * resolved on demand rather than retaining the address of a scene system. */
static LDKSceneSystems *s_editor_inspector_scene_systems(
    LDKEditorContext *editor);
static bool s_editor_inspector_system_index(
    const LDKSceneSystems *systems, u64 id, u32 *out_index);
static const LDKSystemMeta *s_editor_inspector_system_meta(
    LDKGame *game, u64 id);

static const LDKComponentFieldMeta *s_editor_undo_system_field_resolve(
    LDKEditorContext *editor, LDKEditorUndoKey key, LDKFieldType type,
    size_t size, void **out_field)
{
  const LDKSystemMeta *meta;
  LDKSceneSystems *systems;
  void *data;
  u32 system_index;
  u32 offset = (u32)key.property;

  if (!editor || editor->editor_state != LDK_EDITOR_STATE_STOPED ||
      key.owner == 0 || (key.property >> 32) != 0 ||
      !(meta = s_editor_inspector_system_meta(ldk_game_get(), key.owner)) ||
      !meta->fields || size == 0 || size > meta->size ||
      offset > meta->size - size ||
      !(systems = s_editor_inspector_scene_systems(editor)) ||
      !s_editor_inspector_system_index(systems, key.owner, &system_index) ||
      !systems->data_sizes || systems->data_sizes[system_index] != meta->size ||
      !(data = ldk_scene_systems_data_get(systems, key.owner)))
  {
    return NULL;
  }

  for (u32 i = 0; i < meta->field_count; ++i)
  {
    const LDKComponentFieldMeta *field = &meta->fields[i];
    if (field->offset == offset && field->type == type &&
        s_editor_inspector_undo_field_size(field) == size)
    {
      if (out_field)
      {
        *out_field = (u8 *)data + offset;
      }
      return field;
    }
  }
  return NULL;
}

static const LDKComponentFieldMeta *s_editor_undo_field_resolve(
    LDKEditorUndoKey key, LDKFieldType type, size_t size,
    LDKEntity *out_entity, u32 *out_component, void **out_field)
{
  LDKECS *ecs = ldk_module_get(LDK_MODULE_ECS);
  LDKEntity entity = {(u32)key.owner, (u32)(key.owner >> 32)};
  u32 component_type = (u32)(key.property >> 32);
  u32 offset = (u32)key.property;
  const LDKComponentMeta *meta;
  void *component;
  if (!ecs || !ldk_entity_is_alive(&ecs->entity, entity) ||
      !(meta = ldk_property_component_meta(component_type)) ||
      !(component = ldk_ecs_component_get(entity, component_type)))
  {
    return NULL;
  }
  for (u32 i = 0; i < meta->field_count; ++i)
  {
    const LDKComponentFieldMeta *field = &meta->fields[i];
    if (field->offset == offset && field->type == type &&
        s_editor_inspector_undo_field_size(field) == size &&
        size && size <= meta->size && offset <= meta->size - size)
    {
      if (out_entity) *out_entity = entity;
      if (out_component) *out_component = component_type;
      if (out_field) *out_field = (u8 *)component + offset;
      return field;
    }
  }
  return NULL;
}

/* Material undo snapshots carry complete descriptors. Their keys use a
 * reserved property domain, independent of ECS field offsets and system IDs. */
enum
{
  LDK_EDITOR_UNDO_MATERIAL_ASSET = UINT32_MAX,
  LDK_EDITOR_UNDO_MATERIAL_MESH = UINT32_MAX - 1u,
  LDK_EDITOR_UNDO_MATERIAL_INSTANCED_MESH = UINT32_MAX - 2u,
};

static LDKAssetMaterial s_editor_mesh_material_asset(
    const LDKMeshSource *mesh, u32 material_slot);
static u64 s_editor_mesh_material_revision(
    const LDKMeshSource *mesh, u32 material_slot);

static u32 s_editor_undo_material_domain(LDKEditorUndoKey key)
{
  return (u32)(key.property >> 32);
}

/* Builtin component IDs overlap the material domains. Match the stored
 * representation too, so Transform and Camera fields remain distinguishable. */
static bool s_editor_undo_material_key_matches(
    LDKEditorUndoKey key, LDKFieldType type, size_t size)
{
  u32 domain = s_editor_undo_material_domain(key);
  return type == LDK_FIELD_ASSET_MATERIAL &&
      size == sizeof(LDKMaterialDesc) &&
      (domain == LDK_EDITOR_UNDO_MATERIAL_ASSET ||
          domain == LDK_EDITOR_UNDO_MATERIAL_MESH ||
          domain == LDK_EDITOR_UNDO_MATERIAL_INSTANCED_MESH);
}

static LDKMeshSource *s_editor_undo_material_mesh(
    LDKEditorUndoKey key, u32 domain)
{
  LDKECS *ecs = ldk_module_get(LDK_MODULE_ECS);
  LDKEntity entity = {(u32)key.owner, (u32)(key.owner >> 32)};
  u32 type = domain == LDK_EDITOR_UNDO_MATERIAL_INSTANCED_MESH
      ? LDK_COMPONENT_TYPE_INSTANCED_MESH_SOURCE
      : LDK_COMPONENT_TYPE_MESH_SOURCE;
  void *component;

  if (!ecs || !ldk_entity_is_alive(&ecs->entity, entity) ||
      !(component = ldk_ecs_component_get(entity, type)))
  {
    return NULL;
  }

  return domain == LDK_EDITOR_UNDO_MATERIAL_INSTANCED_MESH
      ? &((LDKInstancedMeshSource *)component)->source
      : (LDKMeshSource *)component;
}

static const LDKMaterialDesc *s_editor_undo_material_resolve(
    LDKEditorContext *editor, LDKEditorUndoKey key,
    LDKFieldType type, size_t size)
{
  u32 domain = s_editor_undo_material_domain(key);
  LDKAssetManager *assets;
  const LDKAssetMaterialData *data;
  LDKMeshSource *mesh;
  u32 slot;

  if (!editor || editor->editor_state != LDK_EDITOR_STATE_STOPED ||
      type != LDK_FIELD_ASSET_MATERIAL || size != sizeof(LDKMaterialDesc))
  {
    return NULL;
  }

  if (domain == LDK_EDITOR_UNDO_MATERIAL_ASSET)
  {
    LDKAssetMaterial asset = ldk_asset_material_null();
    asset.h.index = (u32)key.owner;
    asset.h.version = (u32)(key.owner >> 32);
    assets = ldk_module_get(LDK_MODULE_ASSET_MANAGER);
    data = assets ? ldk_asset_manager_material_get_const(assets, asset) : NULL;
    return data && !data->is_missing ? &data->descriptor : NULL;
  }

  if (domain != LDK_EDITOR_UNDO_MATERIAL_MESH &&
      domain != LDK_EDITOR_UNDO_MATERIAL_INSTANCED_MESH)
  {
    return NULL;
  }

  slot = (u32)key.property;
  mesh = s_editor_undo_material_mesh(key, domain);
  if (!mesh || slot >= ldk_mesh_source_material_count(mesh) ||
      s_editor_mesh_material_revision(mesh, slot) != 0 ||
      !x_handle_is_null(s_editor_mesh_material_asset(mesh, slot).h))
  {
    return NULL;
  }

  if (slot == 0u)
  {
    return &mesh->material;
  }
  const LDKMeshSourceMaterialBinding *binding =
      ldk_mesh_source_additional_material_binding_const(mesh, slot);
  return binding ? &binding->material : NULL;
}

static bool s_editor_undo_material_apply(LDKEditorContext *editor,
    LDKEditorUndoKey key, LDKFieldType type, const void *value, size_t size)
{
  u32 domain = s_editor_undo_material_domain(key);
  if (!value || !s_editor_undo_material_resolve(editor, key, type, size))
  {
    return false;
  }

  if (domain == LDK_EDITOR_UNDO_MATERIAL_ASSET)
  {
    LDKAssetManager *assets = ldk_module_get(LDK_MODULE_ASSET_MANAGER);
    LDKAssetMaterial asset = ldk_asset_material_null();
    asset.h.index = (u32)key.owner;
    asset.h.version = (u32)(key.owner >> 32);
    return assets && ldk_asset_manager_material_update(
        assets, asset, (const LDKMaterialDesc *)value);
  }

  return ldk_mesh_source_set_material_at(
      s_editor_undo_material_mesh(key, domain),
      (u32)key.property, (const LDKMaterialDesc *)value);
}

typedef enum LDKEditorArrayUndoAction
{
  LDK_EDITOR_ARRAY_INSERT = 1,
  LDK_EDITOR_ARRAY_REMOVE = 2,
  LDK_EDITOR_ARRAY_SWAP = 3,
  LDK_EDITOR_ARRAY_SWAP_INVERSE = 4,
  LDK_EDITOR_ARRAY_RESTORE_ALL = 5,
  LDK_EDITOR_ARRAY_CLEAR_ALL = 6,
} LDKEditorArrayUndoAction;

typedef struct LDKEditorArrayUndoOp
{
  u32 index;
  u32 action;
} LDKEditorArrayUndoOp;

static LDKSceneSystems *s_editor_inspector_scene_systems(
    LDKEditorContext *editor);
static bool s_editor_inspector_system_index(
    const LDKSceneSystems *systems, u64 id, u32 *out_index);
static const LDKSystemMeta *s_editor_inspector_system_meta(
    LDKGame *game, u64 id);

static bool s_editor_array_undo_key(LDKEditorUndoKey key)
{
  u32 offset = (u32)key.property;
  return (offset & 0x80000000u) != 0u;
}

static void *s_editor_array_undo_resolve(LDKEditorContext *editor,
    LDKEditorUndoKey key, LDKFieldType type, size_t size, void **out_component,
    const LDKArrayFieldMeta **out_array)
{
  LDKECS *ecs = ldk_module_get(LDK_MODULE_ECS);
  LDKEntity entity = {(u32)key.owner, (u32)(key.owner >> 32)};
  u32 component_type = (u32)(key.property >> 32);
  u32 offset = (u32)key.property;
  bool structural = (offset & 0xff000000u) == LDK_EDITOR_ARRAY_STRUCT_BASE;
  u32 array_index = structural ? ((offset >> 16) & 0xffu) :
      ((offset >> 24) & 0x7fu);
  u32 element_index = (offset >> 8) & 0xffffu;
  u32 field_index = offset & 0xffu;
  const LDKComponentMeta *meta = ldk_property_component_meta(component_type);
  LDKComponentMeta system_fields = {0};
  void *component = NULL;
  if (component_type == 0u)
  {
    const LDKSystemMeta *system_meta = editor &&
        editor->editor_state == LDK_EDITOR_STATE_STOPED
        ? s_editor_inspector_system_meta(ldk_game_get(), key.owner) : NULL;
    LDKSceneSystems *systems = s_editor_inspector_scene_systems(editor);
    if (!system_meta || !systems ||
        !s_editor_inspector_system_index(systems, key.owner, NULL))
      return NULL;
    component = ldk_scene_systems_data_get(systems, key.owner);
    system_fields.size = system_meta->size;
    system_fields.fields = system_meta->fields;
    system_fields.field_count = system_meta->field_count;
    meta = &system_fields;
  }
  else
  {
    if (!ecs || !ldk_entity_is_alive(&ecs->entity, entity)) return NULL;
    component = ldk_ecs_component_get(entity, component_type);
  }
  if (!meta || !component || array_index >= meta->field_count) return NULL;
  const LDKComponentFieldMeta *array_field = &meta->fields[array_index];
  if (array_field->type != LDK_FIELD_ARRAY || !array_field->array_meta ||
      array_field->offset > meta->size ||
      sizeof(XArray *) > meta->size - array_field->offset) return NULL;
  XArray **array = (XArray **)((u8 *)component + array_field->offset);
  const LDKArrayFieldMeta *array_meta = array_field->array_meta;
  if (out_component) *out_component = component;
  if (out_array) *out_array = array_meta;
  if (structural)
    return type == LDK_FIELD_ARRAY && size > sizeof(LDKEditorArrayUndoOp)
        ? array : NULL;
  const LDKComponentMeta *element_meta = array_meta->element_meta
      ? array_meta->element_meta() : NULL;
  if (!*array || !element_meta || field_index >= element_meta->field_count ||
      element_index >= x_array_count(*array)) return NULL;
  const LDKComponentFieldMeta *field = &element_meta->fields[field_index];
  if (field->type != type || !size ||
      field->offset > element_meta->size ||
      size > element_meta->size - field->offset ||
      s_editor_inspector_undo_field_size(field) != size) return NULL;
  return (u8 *)x_array_get(*array, element_index) + field->offset;
}

static bool s_editor_array_apply_structure(XArray **array,
    const LDKArrayFieldMeta *array_meta, void *component,
    const void *bytes, size_t size)
{
  const LDKComponentMeta *element_meta = array_meta->element_meta
      ? array_meta->element_meta() : NULL;
  const LDKEditorArrayUndoOp *op = bytes;
  if (!array || !element_meta || !bytes || !element_meta->size)
    return false;
  bool whole_array = op->action == LDK_EDITOR_ARRAY_RESTORE_ALL ||
      op->action == LDK_EDITOR_ARRAY_CLEAR_ALL;
  if (whole_array)
  {
    if (op->index > (SIZE_MAX - sizeof(*op)) / element_meta->size ||
        size != sizeof(*op) + (size_t)op->index * element_meta->size)
      return false;
  }
  else if (size != sizeof(*op) + element_meta->size) return false;
  if (!*array)
  {
    *array = x_array_create(element_meta->size, 0u);
    if (!*array) return false;
  }
  u32 count = x_array_count(*array);
  if (op->action == LDK_EDITOR_ARRAY_INSERT)
  {
    if (op->index > count ||
        x_array_insert(*array, (void *)(op + 1), op->index) != XARRAY_OK)
      return false;
  }
  else if (op->action == LDK_EDITOR_ARRAY_REMOVE)
  {
    if (op->index >= count) return false;
    x_array_delete_at(*array, op->index);
  }
  else if (op->action == LDK_EDITOR_ARRAY_SWAP ||
      op->action == LDK_EDITOR_ARRAY_SWAP_INVERSE)
  {
    if (op->index >= count || op->index + 1u >= count) return false;
    u8 *a = x_array_get(*array, op->index);
    u8 *b = x_array_get(*array, op->index + 1u);
    for (size_t i = 0; i < element_meta->size; ++i)
    {
      u8 t = a[i]; a[i] = b[i]; b[i] = t;
    }
  }
  else if (op->action == LDK_EDITOR_ARRAY_RESTORE_ALL)
  {
    x_array_clear(*array);
    if (op->index && x_array_add_range(*array, op + 1, op->index) != XARRAY_OK)
      return false;
  }
  else if (op->action == LDK_EDITOR_ARRAY_CLEAR_ALL)
    x_array_clear(*array);
  else return false;
  if (array_meta->on_change) array_meta->on_change(component);
  return true;
}

/* Undo records structural edits as inverse operation pairs with a copy of
 * the element. There are no persistent pointers into XArray storage. */
static bool s_editor_array_undo_record(LDKEditorContext *editor,
    LDKEntity entity, u32 component_type, u32 array_field_index,
    u32 index, const LDKComponentMeta *element_meta,
    LDKEditorArrayUndoAction before_action,
    LDKEditorArrayUndoAction after_action, const void *element)
{
  if (!editor || !element_meta || !element || array_field_index > 255u ||
      ldk_editor_undo_has_active_change(&editor->undo)) return false;
  size_t size = sizeof(LDKEditorArrayUndoOp) + element_meta->size;
  u8 *before = malloc(size), *after = malloc(size);
  if (!before || !after) { free(before); free(after); return false; }
  LDKEditorArrayUndoOp before_op = {index, before_action};
  LDKEditorArrayUndoOp after_op = {index, after_action};
  memcpy(before, &before_op, sizeof(before_op));
  memcpy(after, &after_op, sizeof(after_op));
  memcpy(before + sizeof(before_op), element, element_meta->size);
  memcpy(after + sizeof(after_op), element, element_meta->size);
  LDKEditorUndoKey key = {
      ((u64)entity.version << 32) | entity.index,
      ((u64)component_type << 32) | LDK_EDITOR_ARRAY_STRUCT_BASE |
          (array_field_index << 16)};
  bool ok = ldk_editor_undo_begin_change(&editor->undo, key,
      LDK_FIELD_ARRAY, before, size);
  if (ok) ok = ldk_editor_undo_end_change(&editor->undo, after);
  free(before); free(after);
  return ok;
}

void *ldki_editor_undo_property_resolve(
    void *user, LDKEditorUndoKey key, LDKFieldType type, size_t size)
{
  void *value = NULL;
  if (s_editor_undo_material_key_matches(key, type, size))
  {
    return (void *)s_editor_undo_material_resolve(
        (LDKEditorContext *)user, key, type, size);
  }
  if (s_editor_array_undo_key(key))
  {
    void *array_value = s_editor_array_undo_resolve(
        (LDKEditorContext *)user, key, type, size, NULL, NULL);
    if (array_value) return array_value;
  }
  if ((key.property >> 32) == 0)
  {
    if (!s_editor_undo_system_field_resolve(
            (LDKEditorContext *)user, key, type, size, &value))
    {
      return NULL;
    }
  }
  else if (!s_editor_undo_field_resolve(
               key, type, size, NULL, NULL, &value))
  {
    return NULL;
  }
  return value;
}

bool ldki_editor_undo_property_apply(void *user, LDKEditorUndoKey key,
    LDKFieldType type, const void *value, size_t size)
{
  LDKEntity entity;
  u32 component_type;
  const LDKComponentFieldMeta *field;
  LDKPropertyValue property = {0};
  if (!value)
  {
    return false;
  }
  if (s_editor_undo_material_key_matches(key, type, size))
  {
    return s_editor_undo_material_apply(
        (LDKEditorContext *)user, key, type, value, size);
  }
  if (s_editor_array_undo_key(key))
  {
    void *component = NULL;
    const LDKArrayFieldMeta *array_meta = NULL;
    void *destination = s_editor_array_undo_resolve(
        (LDKEditorContext *)user, key, type, size, &component, &array_meta);
    if (destination && component && array_meta)
    {
      if (type == LDK_FIELD_ARRAY)
        return s_editor_array_apply_structure((XArray **)destination,
            array_meta, component, value, size);
      memcpy(destination, value, size);
      if (array_meta->on_change) array_meta->on_change(component);
      return true;
    }
  }
  if ((key.property >> 32) == 0)
  {
    /* Fall through to the ordinary system field resolver. */
    void *destination = NULL;
    field = s_editor_undo_system_field_resolve(
        (LDKEditorContext *)user, key, type, size, &destination);
    if (!field || !destination)
    {
      return false;
    }
    /* System fields are edited directly by the Inspector. Restore the same
     * representation; enums use their Comet-defined writer. */
    if (type == LDK_FIELD_ENUM)
    {
      field->enum_meta->write(
          destination, field->enum_meta->read(value));
    }
    else
    {
      memcpy(destination, value, size);
    }
    return true;
  }
  if (!(field = s_editor_undo_field_resolve(key, type, size,
          &entity, &component_type, NULL)))
  {
    return false;
  }
  property.type = type;
  switch (type)
  {
  case LDK_FIELD_BOOL: property.integer = *(const bool *)value; break;
  case LDK_FIELD_I32: property.integer = *(const i32 *)value; break;
  case LDK_FIELD_U32: property.integer = *(const u32 *)value; break;
  case LDK_FIELD_U64: property.integer = (i64)*(const u64 *)value; break;
  case LDK_FIELD_ENUM: property.integer = field->enum_meta->read(value); break;
  case LDK_FIELD_FLOAT: property.vector.x = *(const float *)value; break;
  case LDK_FIELD_VEC2:
    property.vector.x = ((const Vec2 *)value)->x;
    property.vector.y = ((const Vec2 *)value)->y;
    break;
  case LDK_FIELD_VEC3:
    property.vector.x = ((const Vec3 *)value)->x;
    property.vector.y = ((const Vec3 *)value)->y;
    property.vector.z = ((const Vec3 *)value)->z;
    break;
  case LDK_FIELD_VEC4: property.vector = *(const Vec4 *)value; break;
  case LDK_FIELD_QUAT:
    property.vector = vec4_make(((const Quat *)value)->x,
        ((const Quat *)value)->y, ((const Quat *)value)->z,
        ((const Quat *)value)->w);
    break;
  case LDK_FIELD_STRING: property.string = *(const XSmallstr *)value; break;
  default: return false;
  }
  return ldk_property_set(entity, component_type, field->name, &property);
}

static void s_editor_inspector_field_draw(
    LDKEditorContext *editor, LDKEntity entity,
    u32 component_type, const LDKComponentMeta *meta,
    const LDKComponentFieldMeta *field, void *component)
{
  LDKUIContext *ui;
  char value_text[128];
  const char *display_name;
  u8 *field_value;
  bool readonly;
  LDKEditorUIUndoProperty undo_property;
  const LDKEditorUIUndoProperty *undo_meta = NULL;

  if (!editor || !meta || !field || !component || field->offset >= meta->size)
  {
    return;
  }

  ui = &editor->ui;
  if (component_type == LDK_COMPONENT_TYPE_KEYFRAME_ANIMATION_SOURCE &&
      strcmp(field->name, "current_animation") == 0)
  {
    s_editor_inspector_keyframe_current_draw(editor, entity,
        (const LDKKeyFrameAnimationSource *)component,
        (field->flags & LDK_FIELD_FLAG_READONLY) != 0);
    return;
  }
  display_name = s_editor_inspector_field_display_name(field);
  if (component_type == LDK_COMPONENT_TYPE_TRANSFORM)
  {
    if (field->offset == offsetof(LDKTransform, local_position))
      display_name = "Position";
    else if (field->offset == offsetof(LDKTransform, local_scale))
      display_name = "Scale";
    else if (field->offset == offsetof(LDKTransform, local_rotation))
      display_name = "Rotation";
  }

  field_value = (u8 *)component + field->offset;
  readonly = (field->flags & LDK_FIELD_FLAG_READONLY) != 0;
  undo_property = s_editor_inspector_undo_property(
      entity, component_type, meta, field, field_value);
  if (undo_property.size && editor->editor_state == LDK_EDITOR_STATE_STOPED)
  {
    undo_meta = &undo_property;
  }

  ldk_ui_push_id_cstr(ui, field->name);

  if ((component_type == LDK_COMPONENT_TYPE_MESH_SOURCE ||
        component_type == LDK_COMPONENT_TYPE_INSTANCED_MESH_SOURCE) &&
      field->type == LDK_FIELD_U32 &&
      field->offset == offsetof(LDKMeshSource, mesh_index))
  {
    ldk_ui_pop_id(ui);
    return;
  }

  if (field->type == LDK_FIELD_ASSET_TEXTURE_ATLAS)
  {
    (void)s_editor_inspector_texture_atlas_asset_field(editor, display_name,
        (LDKAssetTextureAtlas *)field_value, readonly);
    ldk_ui_pop_id(ui);
    return;
  }
  if (field->widget == LDK_FIELD_WIDGET_ATLAS_REGION &&
      field->type == LDK_FIELD_U64 &&
      field->offset <= meta->size &&
      sizeof(u64) <= meta->size - field->offset)
  {
    s_editor_inspector_atlas_region_field_draw(editor, entity, component_type,
        meta, field, component, readonly);
    ldk_ui_pop_id(ui);
    return;
  }

  if (field->type == LDK_FIELD_ASSET_MESH)
  {
    LDKAssetMesh value = *(LDKAssetMesh *)field_value;
    bool changed = s_editor_inspector_mesh_asset_field(
        editor, display_name, &value, readonly);

    if ((component_type == LDK_COMPONENT_TYPE_MESH_SOURCE ||
        component_type == LDK_COMPONENT_TYPE_INSTANCED_MESH_SOURCE) &&
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

  if (field->type == LDK_FIELD_ASSET_FONT)
  {
    LDKAssetFont *value = (LDKAssetFont *)field_value;
    (void)s_editor_inspector_font_asset_field(
        editor, display_name, value, readonly);
    ldk_ui_pop_id(ui);
    return;
  }

  if (field->type == LDK_FIELD_ASSET_IMAGE)
  {
    LDKAssetImage *value = (LDKAssetImage *)field_value;
    LDKMaterialIOContext context = {0};
    context.assets = ldk_module_get(LDK_MODULE_ASSET_MANAGER);
    (void)s_editor_material_image_editor(editor, display_name,
        "Choose image", value, readonly, true, &context);
    ldk_ui_pop_id(ui);
    return;
  }

  if (field->type == LDK_FIELD_ASSET_AUDIO)
  {
    LDKAssetAudio *value = (LDKAssetAudio *)field_value;
    (void)s_editor_inspector_audio_asset_field(
        editor, display_name, value, readonly);
    ldk_ui_pop_id(ui);
    return;
  }

  if (field->type == LDK_FIELD_ASSET_KEYFRAME_ANIMATION)
  {
    LDKAssetKeyframeAnimation *value = (LDKAssetKeyframeAnimation *)field_value;
    (void)s_editor_inspector_keyframe_animation_asset_field(
        editor, display_name, value, readonly);
    ldk_ui_pop_id(ui);
    return;
  }

  if (field->type == LDK_FIELD_ASSET_MATERIAL)
  {
    LDKAssetMaterial *value = (LDKAssetMaterial *)field_value;
    LDKMaterialIOContext context = {0};

    (void)s_editor_inspector_material_asset_field(
        editor, display_name, value, readonly, true);

    context.assets = ldk_module_get(LDK_MODULE_ASSET_MANAGER);
    (void)s_editor_material_asset_editor(
        editor, value, readonly, &context);

    ldk_ui_pop_id(ui);
    return;
  }

  if (field->type == LDK_FIELD_ASSET_SKYBOX)
  {
    LDKAssetSkybox *value = (LDKAssetSkybox *)field_value;

    (void)s_editor_inspector_skybox_asset_field(
        editor, display_name, value, readonly, true);

    ldk_ui_pop_id(ui);
    return;
  }

  bool specialized_row = field->type == LDK_FIELD_BOOL ||
      field->type == LDK_FIELD_STRING ||
      (field->type == LDK_FIELD_ENUM && field->enum_meta) ||
      (field->type == LDK_FIELD_U32 &&
          field->widget == LDK_FIELD_WIDGET_COLOR) ||
      field->type == LDK_FIELD_I32 ||
      (field->type == LDK_FIELD_ENUM && !field->enum_meta) ||
      field->type == LDK_FIELD_VEC2 ||
      field->type == LDK_FIELD_VEC3 ||
      field->type == LDK_FIELD_VEC4 ||
      field->type == LDK_FIELD_QUAT ||
      (field->type == LDK_FIELD_U32 && field->widget != LDK_FIELD_WIDGET_COLOR) ||
      field->type == LDK_FIELD_FLOAT;
  if (!specialized_row)
  {
    s_editor_inspector_row_begin(editor, display_name);
  }

  if (field->type == LDK_FIELD_ENUM && field->enum_meta)
  {
    const LDKEnumMeta *meta = field->enum_meta;
    const char **labels = (const char **)x_arena_alloc(
        ui->frame_arena, (size_t)(meta->count + 1u) * sizeof(*labels));
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
      u32 next = ldk_editor_ui_row_enum(editor, display_name,
          s_editor_inspector_row_group_depth, labels, count, selected,
          readonly, undo_meta);
      if (!readonly && next != selected && next < meta->count)
      {
        if (component_type)
        {
          LDKPropertyValue chosen = {0};
          chosen.type = LDK_FIELD_ENUM;
          chosen.integer = meta->options[next].value;
          (void)ldk_property_set(entity, component_type, field->name, &chosen);
        }
        else
        {
          meta->write(field_value, meta->options[next].value);
        }
      }
    }
    else
    {
      s_editor_inspector_row_begin(editor, display_name);
      ldk_ui_label(ui, "Enum options unavailable");
      ldk_ui_end_horizontal(ui);
    }
    ldk_ui_pop_id(ui);
    return;
  }

  switch (field->type)
  {
  case LDK_FIELD_BOOL:
  {
    bool value = *(bool *)field_value;

    value = ldk_editor_ui_row_bool(editor, display_name,
        s_editor_inspector_row_group_depth, value, readonly, undo_meta);

    if (!readonly && value != *(bool *)field_value)
    {
      if (component_type)
        (void)s_editor_inspector_property_write(entity, component_type, field, &value);
      else
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
    LDKInspectorNumericRowContext context = {entity, component_type, field};
    result = ldk_editor_ui_row_int(editor, display_name,
        s_editor_inspector_row_group_depth, buffer, (u32)sizeof(buffer),
        readonly, s_editor_inspector_numeric_row_input, &context, undo_meta);

    if (!readonly && (result & LDK_UI_INPUT_BOX_CHANGED) != 0)
    {
      if (s_editor_inspector_parse_i32(buffer, &parsed))
      {
        if (component_type)
          (void)s_editor_inspector_property_write(entity, component_type, field, &parsed);
        else
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
      rgba32 color = *(rgba32 *)field_value;
      if (ldk_editor_ui_row_color(editor, display_name,
              s_editor_inspector_row_group_depth, &color, readonly, undo_meta))
      {
        if (component_type)
          (void)s_editor_inspector_property_write(entity, component_type, field, &color);
        else
          *(rgba32 *)field_value = color;
      }
      ldk_ui_pop_id(ui);
      return;
    }
    char buffer[LDK_EDITOR_INSPECTOR_INPUT_CAPACITY];
    u32 original = *(u32 *)field_value;
    u32 parsed;
    u32 result;

    snprintf(buffer, sizeof(buffer), "%u", original);
    LDKInspectorNumericRowContext context = {entity, component_type, field};
    result = ldk_editor_ui_row_uint(editor, display_name,
        s_editor_inspector_row_group_depth, buffer, (u32)sizeof(buffer),
        readonly, s_editor_inspector_numeric_row_input, &context, undo_meta);

    if (!readonly && (result & LDK_UI_INPUT_BOX_CHANGED) != 0)
    {
      if (s_editor_inspector_parse_u32(buffer, &parsed))
      {
        if (component_type)
          (void)s_editor_inspector_property_write(entity, component_type, field, &parsed);
        else
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
      value = ldk_editor_ui_row_slider_float(editor, display_name,
          s_editor_inspector_row_group_depth, value,
          field->min_value, field->max_value, readonly, undo_meta);

      if (!readonly)
      {
        if (component_type)
          (void)s_editor_inspector_property_write(entity, component_type, field, &value);
        else
          *(float *)field_value = value;
      }
    }
    else
    {
      char buffer[LDK_EDITOR_INSPECTOR_INPUT_CAPACITY];
      float parsed;
      LDKInspectorNumericRowContext context = {entity, component_type, field};
      u32 result = ldk_editor_ui_row_float(editor, display_name,
          s_editor_inspector_row_group_depth, buffer, (u32)sizeof(buffer),
          &value, readonly, s_editor_inspector_numeric_row_input, &context, undo_meta);
      if (!readonly && (result & LDK_UI_INPUT_BOX_CHANGED) != 0 &&
          s_editor_inspector_parse_float(buffer, &parsed))
      {
        value = parsed;
        if (component_type)
          (void)s_editor_inspector_property_write(entity, component_type, field, &value);
        else
          *(float *)field_value = value;
      }
    }
    break;
  }

  case LDK_FIELD_STRING:
  {
    XSmallstr value = *(const XSmallstr *)field_value;
    u32 result;

    if (value.length > X_SMALLSTR_MAX_LENGTH ||
        value.buf[value.length] != 0 ||
        strlen(value.buf) != value.length)
    {
      s_editor_inspector_row_begin(editor, display_name);
      ldk_ui_label(ui, "<invalid string>");
      break;
    }

    result = ldk_editor_ui_row_text(editor, display_name,
        s_editor_inspector_row_group_depth, value.buf,
        (u32)sizeof(value.buf), readonly, undo_meta);

    if (!readonly && (result & LDK_UI_INPUT_BOX_CHANGED) != 0)
    {
      value.buf[X_SMALLSTR_MAX_LENGTH] = 0;
      value.length = strlen(value.buf);
      if (component_type)
        (void)s_editor_inspector_property_write(entity, component_type, field, &value);
      else
        *(XSmallstr *)field_value = value;
    }
    break;
  }

  case LDK_FIELD_VEC2:
  {
    Vec2 value = *(Vec2 *)field_value;

    LDKInspectorNumericRowContext context = {entity, component_type, field};
    float *values[] = {&value.x, &value.y};
    bool changed = ldk_editor_ui_row_vec2(editor, display_name,
        s_editor_inspector_row_group_depth, values, readonly,
        s_editor_inspector_vector_row_input, &context, undo_meta);

    if (changed)
    {
      if (component_type)
        (void)s_editor_inspector_property_write(entity, component_type, field, &value);
      else
        *(Vec2 *)field_value = value;
    }
    break;
  }

  case LDK_FIELD_VEC3:
  {
    Vec3 value = *(Vec3 *)field_value;

    LDKInspectorNumericRowContext context = {entity, component_type, field};
    float *values[] = {&value.x, &value.y, &value.z};
    bool changed = ldk_editor_ui_row_vec3(editor, display_name,
        s_editor_inspector_row_group_depth, values, readonly,
        s_editor_inspector_vector_row_input, &context, undo_meta);

    if (changed)
    {
      if (component_type)
        (void)s_editor_inspector_property_write(entity, component_type, field, &value);
      else
        *(Vec3 *)field_value = value;
    }
    break;
  }

  case LDK_FIELD_VEC4:
  {
    Vec4 value = *(Vec4 *)field_value;

    LDKInspectorNumericRowContext context = {entity, component_type, field};
    float *values[] = {&value.x, &value.y, &value.z, &value.w};
    bool changed = ldk_editor_ui_row_vec4(editor, display_name,
        s_editor_inspector_row_group_depth, values, readonly,
        s_editor_inspector_vector_row_input, &context, undo_meta);

    if (changed)
    {
      if (component_type)
        (void)s_editor_inspector_property_write(entity, component_type, field, &value);
      else
        *(Vec4 *)field_value = value;
    }
    break;
  }

  case LDK_FIELD_QUAT:
  {
    if (field->widget == LDK_FIELD_WIDGET_EULER)
    {
      LDKInspectorEulerRowContext context = {
          entity, component_type, field, component, field_value, readonly};
      ldk_editor_ui_row_euler(editor, display_name,
          s_editor_inspector_row_group_depth,
          s_editor_inspector_euler_row_content, &context, undo_meta);
      break;
    }

    Quat value = *(Quat *)field_value;

    LDKInspectorNumericRowContext context = {entity, component_type, field};
    float *values[] = {&value.x, &value.y, &value.z, &value.w};
    bool changed = ldk_editor_ui_row_vec4(editor, display_name,
        s_editor_inspector_row_group_depth, values, readonly,
        s_editor_inspector_vector_row_input, &context, undo_meta);

    if (changed)
    {
      if (component_type)
        (void)s_editor_inspector_property_write(entity, component_type, field, &value);
      else
        *(Quat *)field_value = value;
    }
    break;
  }

  case LDK_FIELD_MAT4:
  case LDK_FIELD_ENTITY:
  case LDK_FIELD_ASSET_MESH:
  case LDK_FIELD_ASSET_FONT:
  case LDK_FIELD_ASSET_IMAGE:
  case LDK_FIELD_RESOURCE_MESH:
  case LDK_FIELD_ASSET_MATERIAL:
  case LDK_FIELD_ASSET_SKYBOX:
  case LDK_FIELD_ASSET_AUDIO:
  case LDK_FIELD_ASSET_KEYFRAME_ANIMATION:
  default:
    s_editor_inspector_field_value_format(
        value_text, sizeof(value_text), field, field_value);
    ldk_ui_label(ui, value_text);
    break;
  }

  if (field->type != LDK_FIELD_QUAT ||
      field->widget != LDK_FIELD_WIDGET_EULER)
  {
    ldk_ui_end_horizontal(ui);
  }
  ldk_ui_pop_id(ui);
}

static bool s_editor_inspector_group_pointer_index(
    const LDKComponentMeta *meta, const LDKComponentGroupMeta *group,
    u32 *out_index)
{
  ptrdiff_t index;

  if (!meta || !meta->groups || meta->group_count == 0u || !group ||
      !out_index)
  {
    return false;
  }

  if (group < meta->groups || group >= meta->groups + meta->group_count)
  {
    return false;
  }

  index = group - meta->groups;
  if (index < 0 || (u32)index >= meta->group_count)
  {
    return false;
  }

  *out_index = (u32)index;
  return true;
}

static u32 s_editor_inspector_group_path_build(
    const LDKComponentMeta *meta, const LDKComponentGroupMeta *group,
    u32 *path, u32 capacity)
{
  u32 count = 0u;

  while (group)
  {
    u32 index;
    if (count >= capacity ||
        !s_editor_inspector_group_pointer_index(meta, group, &index))
    {
      return 0u;
    }

    path[count++] = index;
    group = group->parent;
  }

  for (u32 i = 0; i < count / 2u; ++i)
  {
    u32 opposite = count - 1u - i;
    u32 tmp = path[i];
    path[i] = path[opposite];
    path[opposite] = tmp;
  }

  return count;
}

static void s_editor_inspector_array_draw(LDKEditorContext *editor,
    LDKEntity entity, u32 component_type, const LDKComponentMeta *component_meta,
    void *component, u32 array_index);

static void s_editor_inspector_field_or_array_draw(LDKEditorContext *editor,
    LDKEntity entity, u32 component_type, const LDKComponentMeta *meta,
    const LDKComponentFieldMeta *field, void *component)
{
  if (field->type == LDK_FIELD_ARRAY && field->array_meta)
  {
    if (field >= meta->fields && field < meta->fields + meta->field_count)
      s_editor_inspector_array_draw(editor, entity, component_type,
          meta, component, (u32)(field - meta->fields));
  }
  else s_editor_inspector_field_draw(editor, entity, component_type,
      meta, field, component);
}

static void s_editor_inspector_fields_draw(
    LDKEditorContext *editor, LDKEntity entity, u32 component_type,
    const LDKComponentMeta *meta, void *component, u32 group_domain,
    u64 group_owner_id)
{
  LDKUIContext *ui;
  u32 *previous_path = NULL;
  u32 *current_path = NULL;
  u32 previous_count = 0u;

  if (!editor || !meta || !component || !meta->fields)
  {
    return;
  }

  ui = &editor->ui;

  if (!meta->groups || meta->group_count == 0u)
  {
    s_editor_inspector_row_group_depth = 0u;
    for (u32 field_i = 0; field_i < meta->field_count; ++field_i)
    {
      s_editor_inspector_field_or_array_draw(editor, entity, component_type,
          meta, &meta->fields[field_i], component);
    }
    return;
  }

  previous_path = (u32 *)x_arena_alloc(
      ui->frame_arena, sizeof(u32) * (size_t)meta->group_count);
  current_path = (u32 *)x_arena_alloc(
      ui->frame_arena, sizeof(u32) * (size_t)meta->group_count);
  if (!previous_path || !current_path)
  {
    s_editor_inspector_row_group_depth = 0u;
    for (u32 field_i = 0; field_i < meta->field_count; ++field_i)
    {
      s_editor_inspector_field_or_array_draw(editor, entity, component_type,
          meta, &meta->fields[field_i], component);
    }
    return;
  }

  for (u32 field_i = 0; field_i < meta->field_count; ++field_i)
  {
    const LDKComponentFieldMeta *field = &meta->fields[field_i];
    u32 current_count = s_editor_inspector_group_path_build(
        meta, field->group, current_path, meta->group_count);
    u32 common_count = 0u;
    bool visible = true;

    while (common_count < previous_count && common_count < current_count &&
           previous_path[common_count] == current_path[common_count])
    {
      ++common_count;
    }

    for (u32 depth = 0; depth < common_count; ++depth)
    {
      if (!s_editor_inspector_group_expanded(
              group_domain, group_owner_id, current_path[depth]))
      {
        visible = false;
        break;
      }
    }

    for (u32 depth = common_count; depth < current_count; ++depth)
    {
      u32 group_index = current_path[depth];
      bool expanded = s_editor_inspector_group_expanded(
          group_domain, group_owner_id, group_index);

      if (visible)
      {
        const LDKComponentGroupMeta *group = &meta->groups[group_index];
        const char *group_title = group->name ? group->name : "<unnamed group>";
        char headline[128];
        bool next_expanded;

        if (!expanded)
        {
          for (u32 k = 0; k < meta->field_count; ++k)
          {
            const LDKComponentFieldMeta *candidate = &meta->fields[k];
            if (candidate->group == group &&
                s_editor_inspector_headline_format(candidate, component,
                    meta->size, headline, sizeof(headline)))
            {
              group_title = headline;
              break;
            }
          }
        }

        ldk_ui_push_id_cstr(ui, "property_group");
        ldk_ui_push_id_u32(ui, group_index);
        next_expanded = ldk_ui_tree_node(ui,
            group_title, expanded, depth, LDK_UI_TREE_NODE_NONE);
        ldk_ui_pop_id(ui);
        ldk_ui_pop_id(ui);

        if (next_expanded != expanded)
        {
          s_editor_inspector_group_expanded_set(
              group_domain, group_owner_id, group_index, next_expanded);
          expanded = next_expanded;
        }
      }

      if (!expanded)
      {
        visible = false;
      }
    }

    if (visible)
    {
      s_editor_inspector_row_group_depth = current_count;
      s_editor_inspector_field_or_array_draw(editor, entity, component_type,
          meta, field, component);
    }

    if (current_count > 0u)
    {
      memcpy(previous_path, current_path, sizeof(u32) * current_count);
    }
    previous_count = current_count;
  }

  s_editor_inspector_row_group_depth = 0u;
}

static void s_editor_inspector_scene_properties_draw(
    LDKEditorContext *editor)
{
  const LDKSceneProperties *current;
  LDKSceneProperties properties;
  LDKComponentFieldMeta fields[7];
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
    for (u32 i = 0; i < meta.field_count; ++i)
    {
      fields[i].flags |= LDK_FIELD_FLAG_READONLY;
    }
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
      isfinite(properties.fog_density) && properties.fog_density >= 0.0f &&
      isfinite(properties.fog_start) && properties.fog_start >= 0.0f &&
      (properties.ambient_sky_color != current->ambient_sky_color ||
          properties.ambient_ground_color != current->ambient_ground_color ||
          properties.ambient_intensity != current->ambient_intensity ||
          properties.fog_enabled != current->fog_enabled ||
          properties.fog_color != current->fog_color ||
          properties.fog_density != current->fog_density ||
          properties.fog_start != current->fog_start))
  {
    if (ldk_scene_properties_set(&properties))
    {
      ldki_editor_scene_dirty_set(editor, true);
    }
  }

  ldk_ui_pop_id(&editor->ui);
}

static void s_editor_material_row_begin(
    LDKEditorContext *editor, const char *title)
{
  s_editor_inspector_row_begin(editor, title);
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

static bool s_editor_material_asset_equal(
    LDKAssetMaterial a, LDKAssetMaterial b)
{
  return a.h.index == b.h.index && a.h.version == b.h.version;
}

static LDKEditorUIUndoProperty s_editor_material_undo_property(
    LDKEditorContext *editor, const LDKMaterialDesc *desc,
    LDKAssetMaterial asset, LDKEntity entity, u32 component_type, u32 slot)
{
  LDKEditorUIUndoProperty property = {0};
  u32 domain;

  if (!editor || !desc || editor->editor_state != LDK_EDITOR_STATE_STOPED)
  {
    return property;
  }

  if (!x_handle_is_null(asset.h))
  {
    domain = LDK_EDITOR_UNDO_MATERIAL_ASSET;
    property.key.owner = ((u64)asset.h.version << 32) | asset.h.index;
    slot = 0;
  }
  else if (component_type == LDK_COMPONENT_TYPE_MESH_SOURCE ||
      component_type == LDK_COMPONENT_TYPE_INSTANCED_MESH_SOURCE)
  {
    domain = component_type == LDK_COMPONENT_TYPE_MESH_SOURCE
        ? LDK_EDITOR_UNDO_MATERIAL_MESH
        : LDK_EDITOR_UNDO_MATERIAL_INSTANCED_MESH;
    property.key.owner = ((u64)entity.version << 32) | entity.index;
  }
  else
  {
    return property; /* Unsaved drafts have no restorable identity. */
  }

  property.key.property = ((u64)domain << 32) | slot;
  property.type = LDK_FIELD_ASSET_MATERIAL;
  property.value = desc;
  property.size = sizeof(*desc);
  return property;
}

/* An interaction edits a temporary descriptor. Capture its value before the
 * control writes it, but finalize only after the owning setter has run. */
static void s_editor_material_undo_update(LDKEditorContext *editor,
    const LDKEditorUIUndoProperty *property,
    const LDKMaterialDesc *initial, bool changed)
{
  LDKUIContext *ui;
  LDKEditorUndo *undo;
  bool matches;
  bool mouse_pressed;
  bool mouse_up;
  bool text_focused;
  bool enter_pressed;

  if (!editor || !property || !property->size || !initial)
  {
    return;
  }

  ui = &editor->ui;
  undo = &editor->undo;
  mouse_pressed = ui->mouse && ldk_os_mouse_button_is_pressed(
      (LDKMouseState *)ui->mouse, LDK_MOUSE_BUTTON_LEFT);
  mouse_up = ui->mouse && ldk_os_mouse_button_up(
      (LDKMouseState *)ui->mouse, LDK_MOUSE_BUTTON_LEFT);
  text_focused = ui->input_box_id != 0 &&
      ui->focused_id == ui->input_box_id;
  enter_pressed = text_focused && ui->keyboard &&
      ldk_os_keyboard_key_down(
          (LDKKeyboardState *)ui->keyboard, LDK_KEYCODE_ENTER);
  matches = ldk_editor_undo_has_active_change(undo) &&
      editor->undo_row_key.owner == property->key.owner &&
      editor->undo_row_key.property == property->key.property;

  if (changed && !matches)
  {
    if (ldk_editor_undo_has_active_change(undo) &&
        !ldk_editor_undo_end_change_resolved(undo))
    {
      ldk_editor_undo_clear(undo);
    }
    if (ldk_editor_undo_begin_change(undo, property->key,
            property->type, initial, property->size))
    {
      editor->undo_row_key = property->key;
      editor->undo_widget_id = 0;
      editor->undo_end_requested = false;
      matches = true;
    }
  }

  if (!matches)
  {
    return;
  }

  editor->undo_row_seen = true;
  if (changed && text_focused)
  {
    editor->undo_widget_id = ui->focused_id;
  }
  if (mouse_up || enter_pressed ||
      (editor->undo_widget_id &&
          editor->undo_widget_id != ui->focused_id) ||
      (!mouse_pressed && !text_focused))
  {
    editor->undo_end_requested = true;
  }
}

static bool s_editor_material_image_editor_ex(LDKEditorContext *editor,
    const char *label, const char *dialog_title, LDKAssetImage *image,
    bool readonly, bool optional, bool *expanded, bool *removed,
    const LDKMaterialIOContext *context)
{
  LDKAssetManager *assets;
  LDKAssetHandle asset;
  const LDKAssetInfo *info;
  XFSPath path = {0};

  if (!editor || !label || !dialog_title || !image || !context ||
      !context->assets)
  {
    return false;
  }

  assets = context->assets;
  asset.h = image->h;
  info = !x_handle_is_null(image->h) ? ldk_asset_get_info_const(assets, asset)
                                     : NULL;

  ldk_ui_push_id_cstr(&editor->ui, label);
  LDKEditorUIRowSlotAction action = ldk_editor_ui_row_slot_image(editor,
      label, dialog_title, s_editor_inspector_row_group_depth, info, readonly, optional,
      !x_handle_is_null(image->h) || removed != NULL, expanded, &path, NULL);
  ldk_ui_pop_id(&editor->ui);

  if (action == LDK_EDITOR_UI_ROW_SLOT_CLEAR)
  {
    if (removed)
      *removed = true;
    image->h = x_handle_null();
    return true;
  }

  if (action != LDK_EDITOR_UI_ROW_SLOT_ASSIGN)
  {
    return false;
  }

  LDKAssetPath asset_path;
  if (!s_editor_inspector_asset_path_validate(editor, &path, dialog_title,
          "Choose an image inside the project's runtree folder.", &asset_path))
  {
    return false;
  }

  LDKAssetImage selected =
      ldk_asset_manager_image_load_shared(assets, asset_path.buf);
  if (x_handle_is_null(selected.h))
  {
    ldk_os_dialog_show_error(editor->window, dialog_title,
        "The selected image could not be loaded.");
    return false;
  }

  if (selected.h.index == image->h.index &&
      selected.h.version == image->h.version)
  {
    return false;
  }

  *image = selected;
  return true;
}

static bool s_editor_material_image_editor(LDKEditorContext *editor,
    const char *label, const char *dialog_title, LDKAssetImage *image,
    bool readonly, bool optional, const LDKMaterialIOContext *context)
{
  return s_editor_material_image_editor_ex(editor, label, dialog_title,
      image, readonly, optional, NULL, NULL, context);
}

/* Shared Inspector image picker for editor tools that reference image assets. */
bool ldki_editor_image_asset_field(LDKEditorContext *editor,
    const char *label, LDKAssetImage *image, bool readonly)
{
  LDKMaterialIOContext context = {0};
  context.assets = ldk_module_get(LDK_MODULE_ASSET_MANAGER);
  context.user = editor;
  return s_editor_material_image_editor(editor, label, "Choose image",
      image, readonly, false, &context);
}

static bool s_editor_material_vec2_editor(LDKEditorContext *editor,
    const char *label, Vec2 *value, bool readonly)
{
  LDKUIContext *ui;
  char x_buffer[LDK_EDITOR_INSPECTOR_INPUT_CAPACITY];
  char y_buffer[LDK_EDITOR_INSPECTOR_INPUT_CAPACITY];
  float parsed;
  u32 result;
  bool changed = false;

  if (!editor || !label || !value)
  {
    return false;
  }

  ui = &editor->ui;
  ldk_ui_format_float(x_buffer, (u32)sizeof(x_buffer), value->x);
  ldk_ui_format_float(y_buffer, (u32)sizeof(y_buffer), value->y);

  ldk_ui_push_id_cstr(ui, label);
  s_editor_material_row_begin(editor, label);

  ldk_ui_set_next_width(ui, ldk_ui_px(12.0f));
  ldk_ui_label(ui, "X");
  ldk_ui_begin_disabled(ui, readonly);
  result = ldk_ui_input_box(ui, x_buffer, sizeof(x_buffer));
  ldk_ui_end_disabled(ui);
  if (!readonly && (result & LDK_UI_INPUT_BOX_CHANGED) != 0 &&
      s_editor_inspector_parse_float(x_buffer, &parsed) && isfinite(parsed) &&
      parsed != value->x)
  {
    value->x = parsed;
    changed = true;
  }

  ldk_ui_set_next_width(ui, ldk_ui_px(12.0f));
  ldk_ui_label(ui, "Y");
  ldk_ui_begin_disabled(ui, readonly);
  result = ldk_ui_input_box(ui, y_buffer, sizeof(y_buffer));
  ldk_ui_end_disabled(ui);
  if (!readonly && (result & LDK_UI_INPUT_BOX_CHANGED) != 0 &&
      s_editor_inspector_parse_float(y_buffer, &parsed) && isfinite(parsed) &&
      parsed != value->y)
  {
    value->y = parsed;
    changed = true;
  }

  ldk_ui_end_horizontal(ui);
  ldk_ui_pop_id(ui);
  return changed;
}

static bool s_editor_material_texture_settings_editor(
    LDKEditorContext *editor, LDKMaterialTextureSettings *settings, bool readonly)
{
  LDKUIContext *ui = &editor->ui;
  bool changed = false;
  static const char *const filter_names[] = {"Nearest", "Linear"};
  u32 filter = (u32)settings->filter;
  if (filter > (u32)LDK_MATERIAL_TEXTURE_FILTER_LINEAR)
  {
    filter = (u32)LDK_MATERIAL_TEXTURE_FILTER_NEAREST;
  }
  u32 next_filter = ldk_editor_ui_row_enum(editor, "Filter",
      s_editor_inspector_row_group_depth, filter_names, 2, filter, readonly, NULL);
  if (!readonly && next_filter < 2u && next_filter != filter)
  {
    settings->filter = (LDKMaterialTextureFilter)next_filter;
    changed = true;
  }

  static const char *const mip_filter_names[] = {"None", "Nearest", "Linear"};
  u32 mip_filter = (u32)settings->mip_filter;
  if (mip_filter > (u32)LDK_MATERIAL_TEXTURE_MIP_FILTER_LINEAR)
  {
    mip_filter = (u32)LDK_MATERIAL_TEXTURE_MIP_FILTER_NONE;
  }
  u32 next_mip_filter = ldk_editor_ui_row_enum(editor, "Mip Filter",
      s_editor_inspector_row_group_depth, mip_filter_names, 3, mip_filter,
      readonly, NULL);
  if (!readonly && next_mip_filter < 3u && next_mip_filter != mip_filter)
  {
    settings->mip_filter =
        (LDKMaterialTextureMipFilter)next_mip_filter;
    changed = true;
  }

  static const char *const wrap_names[] = {"Repeat", "Clamp", "Mirror"};
  u32 wrap_u = (u32)settings->wrap_u;
  if (wrap_u > (u32)LDK_MATERIAL_TEXTURE_WRAP_MIRROR)
  {
    wrap_u = (u32)LDK_MATERIAL_TEXTURE_WRAP_CLAMP;
  }
  u32 wrap_v = (u32)settings->wrap_v;
  s_editor_material_row_begin(editor, "Wrap");
  ldk_ui_set_next_width(ui, ldk_ui_px(12.0f));
  ldk_ui_label(ui, "U");
  ldk_ui_begin_disabled(ui, readonly);
  u32 next_wrap_u = ldk_ui_combo_box(ui, wrap_names, 3, wrap_u);
  ldk_ui_end_disabled(ui);
  ldk_ui_set_next_width(ui, ldk_ui_px(12.0f));
  ldk_ui_label(ui, "V");
  ldk_ui_begin_disabled(ui, readonly);
  u32 next_wrap_v = ldk_ui_combo_box(ui, wrap_names, 3, wrap_v);
  ldk_ui_end_disabled(ui);
  ldk_ui_end_horizontal(ui);
  if (!readonly && next_wrap_u < 3u && next_wrap_u != wrap_u)
  {
    settings->wrap_u = (LDKMaterialTextureWrap)next_wrap_u;
    changed = true;
  }
  if (!readonly && next_wrap_v < 3u && next_wrap_v != wrap_v)
  {
    settings->wrap_v = (LDKMaterialTextureWrap)next_wrap_v;
    changed = true;
  }

  if (s_editor_material_vec2_editor(
          editor, "UV Scale", &settings->uv_scale, readonly))
  {
    changed = true;
  }
  if (s_editor_material_vec2_editor(
          editor, "UV Offset", &settings->uv_offset, readonly))
  {
    changed = true;
  }

  return changed;
}

static bool s_editor_material_textures_editor(LDKEditorContext *editor,
    LDKMaterialDesc *desc, bool readonly, const LDKMaterialIOContext *context)
{
  static const char *const labels[LDK_MATERIAL_TEXTURE_SLOT_COUNT] = {
      "Albedo", "Normal", "Specular", "Texture 3", "Texture 4",
      "Texture 5", "Texture 6", "Texture 7"};
  static bool expanded[LDK_MATERIAL_TEXTURE_SLOT_COUNT] = {0};
  LDKUIContext *ui = &editor->ui;
  bool changed = false;
  bool textured = desc->type == LDK_MATERIAL_TYPE_TEXTURED ||
      desc->type == LDK_MATERIAL_TYPE_TEXTURED_UNLIT;
  bool lit = desc->type == LDK_MATERIAL_TYPE_TEXTURED ||
      desc->type == LDK_MATERIAL_TYPE_VERTEX_COLOR;
  u32 active = desc->texture_slot_mask;
  u32 available = 0;

  ldk_ui_push_id_cstr(ui, "material_textures");
  for (u32 slot = 0; slot < LDK_MATERIAL_TEXTURE_SLOT_COUNT; ++slot)
  {
    if ((textured || ((slot == 1u || slot == 2u) && lit)) &&
        !x_handle_is_null(ldk_material_texture_slot_get(desc, slot).h))
      active |= 1u << slot;
    if ((textured || ((slot == 1u || slot == 2u) && lit)) &&
        !(active & (1u << slot)))
      available |= 1u << slot;
  }
  s_editor_material_row_begin(editor, "Textures");
  ldk_ui_begin_disabled(ui, readonly || !available);
  bool open = ldk_ui_button(ui, "Add");
  const LDKUIId popup_id = ui->last_id;
  LDKUIRect button = ldk_ui_last_bounding_rect(ui);
  ldk_ui_end_disabled(ui);
  ldk_ui_end_horizontal(ui);
  if (open)
    ldk_ui_open_popup_at(ui, popup_id,
        (LDKUIPoint){button.x, button.y + button.h});
  if (ldk_ui_begin_popup(ui, popup_id))
  {
    for (u32 slot = 0; slot < LDK_MATERIAL_TEXTURE_SLOT_COUNT; ++slot)
    {
      if (!(available & (1u << slot)))
        continue;
      ldk_ui_push_id_u32(ui, slot);
      ldk_ui_set_next_width(ui, ldk_ui_px(button.w));
      ldk_ui_begin_disabled(ui, readonly);
      bool add = ldk_ui_button_flat(ui, labels[slot]);
      ldk_ui_end_disabled(ui);
      ldk_ui_pop_id(ui);
      if (add)
      {
        desc->texture_slot_mask = active | (1u << slot);
        active = desc->texture_slot_mask;
        desc->texture_settings[slot] =
            ldk_material_texture_settings_get(desc, slot);
        desc->texture_settings[slot].independent = true;
        expanded[slot] = true;
        changed = true;
        ldk_ui_close_current_popup(ui);
        break;
      }
    }
    ldk_ui_end_popup(ui);
  }

  for (u32 slot = 0; slot < LDK_MATERIAL_TEXTURE_SLOT_COUNT; ++slot)
  {
    if (!(active & (1u << slot)) ||
        !(textured || ((slot == 1u || slot == 2u) && lit)))
      continue;
    bool removed = false;
    char prompt[64];
    snprintf(prompt, sizeof(prompt), "Choose %s texture", labels[slot]);
    LDKAssetImage image = ldk_material_texture_slot_get(desc, slot);
    ldk_ui_push_id_u32(ui, slot);
    if (s_editor_material_image_editor_ex(editor, labels[slot], prompt,
            &image, readonly, true, &expanded[slot], &removed, context))
    {
      ldk_material_texture_slot_set(desc, slot, image);
      changed = true;
    }
    if (removed)
    {
      active &= ~(1u << slot);
      desc->texture_slot_mask = active;
      memset(&desc->texture_settings[slot], 0,
          sizeof(desc->texture_settings[slot]));
      expanded[slot] = false;
    }
    else if (expanded[slot])
    {
      LDKMaterialTextureSettings settings =
          ldk_material_texture_settings_get(desc, slot);
      if (s_editor_material_texture_settings_editor(editor, &settings, readonly))
      {
        settings.independent = true;
        desc->texture_settings[slot] = settings;
        changed = true;
      }
    }
    ldk_ui_pop_id(ui);
  }
  ldk_ui_pop_id(ui);
  return changed;
}

static bool s_editor_material_desc_editor(LDKEditorContext *editor,
    LDKMaterialDesc *desc, bool readonly,
    const LDKMaterialIOContext *context,
    const LDKEditorUIUndoProperty *undo_property)
{
  static const char *const names[] = {
      "Textured Unlit", "Textured", "Vertex Color Unlit", "Vertex Color"};
  static const LDKMaterialType types[] = {
      LDK_MATERIAL_TYPE_TEXTURED_UNLIT, LDK_MATERIAL_TYPE_TEXTURED,
      LDK_MATERIAL_TYPE_VERTEX_COLOR_UNLIT, LDK_MATERIAL_TYPE_VERTEX_COLOR};
  LDKUIContext *ui;
  bool changed = false;
  u32 selected = 3;
  LDKMaterialDesc initial;

  if (!editor || !desc || !context || !context->assets)
  {
    return false;
  }

  ui = &editor->ui;
  initial = *desc;
  for (u32 i = 0; i < 4; ++i)
  {
    if (desc->type == types[i])
    {
      selected = i;
    }
  }

  u32 next = ldk_editor_ui_row_enum(editor, "Type",
      s_editor_inspector_row_group_depth, names, 4, selected, readonly, NULL);
  if (!readonly && next < 4 && next != selected &&
      ldk_material_desc_defaults(types[next], desc))
  {
    changed = true;
  }

  bool textured = desc->type == LDK_MATERIAL_TYPE_TEXTURED ||
      desc->type == LDK_MATERIAL_TYPE_TEXTURED_UNLIT;
  bool lit = desc->type == LDK_MATERIAL_TYPE_TEXTURED ||
      desc->type == LDK_MATERIAL_TYPE_VERTEX_COLOR;
  rgba32 *color = textured ? &desc->args.textured.color
                           : &desc->args.vertex_color.color;

  s_editor_material_row_begin(editor, "Tint");
  if (ldki_editor_color_field(editor, color, readonly))
  {
    changed = true;
  }
  ldk_ui_end_horizontal(ui);

  s_editor_material_row_begin(editor, "Alpha");
  u32 previous_alpha = (u32)(*color & 255u);
  ldk_ui_begin_disabled(ui, readonly);
  u32 alpha =
      (u32)(ldk_ui_slider_input(ui, (float)previous_alpha, 0, 255) + 0.5f);
  ldk_ui_end_disabled(ui);
  if (!readonly && alpha != previous_alpha)
  {
    *color = (*color & 0xffffff00u) | alpha;
    changed = true;
  }
  ldk_ui_end_horizontal(ui);

  {
    static const char *const textured_alpha_mode_names[] = {
        "Opaque", "Cutout", "Blend"};
    static const char *const vertex_alpha_mode_names[] = {
        "Opaque", "Blend"};
    static const LDKMaterialAlphaMode vertex_alpha_modes[] = {
        LDK_MATERIAL_ALPHA_MODE_OPAQUE, LDK_MATERIAL_ALPHA_MODE_BLEND};

    u32 selected_alpha_mode = 0;
    const char *const *alpha_mode_names = textured
        ? textured_alpha_mode_names : vertex_alpha_mode_names;
    u32 alpha_mode_count = textured ? 3u : 2u;
    if (textured)
    {
      selected_alpha_mode = desc->alpha_mode <= LDK_MATERIAL_ALPHA_MODE_BLEND
          ? (u32)desc->alpha_mode : 0u;
    }
    else if (desc->alpha_mode == LDK_MATERIAL_ALPHA_MODE_BLEND)
    {
      selected_alpha_mode = 1u;
    }

    u32 next_alpha_mode = ldk_editor_ui_row_enum(editor, "Alpha Mode",
        s_editor_inspector_row_group_depth, alpha_mode_names,
        alpha_mode_count, selected_alpha_mode, readonly, NULL);
    if (!readonly && next_alpha_mode < alpha_mode_count &&
        next_alpha_mode != selected_alpha_mode)
    {
      desc->alpha_mode = textured
          ? (LDKMaterialAlphaMode)next_alpha_mode
          : vertex_alpha_modes[next_alpha_mode];
      changed = true;
    }

    if (desc->alpha_mode == LDK_MATERIAL_ALPHA_MODE_CUTOUT)
    {
      s_editor_material_row_begin(editor, "Alpha Cutoff");
      ldk_ui_begin_disabled(ui, readonly);
      float alpha_cutoff =
          ldk_ui_slider_input(ui, desc->alpha_cutoff, 0.0f, 1.0f);
      ldk_ui_end_disabled(ui);
      if (!readonly && alpha_cutoff != desc->alpha_cutoff)
      {
        desc->alpha_cutoff = alpha_cutoff;
        changed = true;
      }
      ldk_ui_end_horizontal(ui);
    }
  }

  if (lit)
  {
    s_editor_material_row_begin(editor, "Specular");
    ldk_ui_begin_disabled(ui, readonly);
    float specular =
        ldk_ui_slider_input(ui, desc->surface.specular, 0.0f, 1.0f);
    ldk_ui_end_disabled(ui);
    if (!readonly && specular != desc->surface.specular)
    {
      desc->surface.specular = specular;
      changed = true;
    }
    ldk_ui_end_horizontal(ui);

    s_editor_material_row_begin(editor, "Shininess");
    float shininess =
        desc->surface.shininess == 0.0f ? 32.0f : desc->surface.shininess;
    ldk_ui_begin_disabled(ui, readonly);
    float next_shininess = ldk_ui_slider_input(ui, shininess, 1.0f, 256.0f);
    ldk_ui_end_disabled(ui);
    if (!readonly && next_shininess != shininess)
    {
      desc->surface.shininess = next_shininess;
      changed = true;
    }
    ldk_ui_end_horizontal(ui);

    s_editor_material_row_begin(editor, "Emission");
    ldk_ui_begin_disabled(ui, readonly);
    float emission =
        ldk_ui_slider_input(ui, desc->surface.emission, 0.0f, 4.0f);
    ldk_ui_end_disabled(ui);
    if (!readonly && emission != desc->surface.emission)
    {
      desc->surface.emission = emission;
      changed = true;
    }
    ldk_ui_end_horizontal(ui);
  }

  if (textured || lit)
  {
    changed |= s_editor_material_textures_editor(editor, desc, readonly, context);
  }

  s_editor_material_undo_update(editor, undo_property, &initial, changed);
  return changed;
}

static bool s_editor_material_asset_editor(LDKEditorContext *editor,
    LDKAssetMaterial *material_asset, bool readonly,
    const LDKMaterialIOContext *context)
{
  typedef struct MaterialDraft
  {
    LDKAssetMaterial *binding;
    LDKMaterialDesc descriptor;
    struct MaterialDraft *next;
  } MaterialDraft;

  static MaterialDraft *drafts = NULL;

  LDKUIContext *ui;
  const LDKAssetMaterialData *data;
  LDKMaterialDesc desc;
  MaterialDraft *draft = NULL;
  MaterialDraft *previous_draft = NULL;
  bool binding_changed = false;

  if (!editor || !material_asset || !context || !context->assets)
  {
    return false;
  }

  ui = &editor->ui;

  for (draft = drafts; draft; draft = draft->next)
  {
    if (draft->binding == material_asset)
    {
      break;
    }

    previous_draft = draft;
  }

  /*
   * An assigned asset owns the descriptor. Any draft that previously
   * belonged to this binding is no longer needed.
   */
  if (!x_handle_is_null(material_asset->h))
  {
    if (draft)
    {
      if (previous_draft)
      {
        previous_draft->next = draft->next;
      }
      else
      {
        drafts = draft->next;
      }

      free(draft);
      draft = NULL;
    }

    data = ldk_asset_manager_material_get_const(
        context->assets, *material_asset);
    if (!data)
    {
      return false;
    }

    ldk_ui_horizontal_line(ui);

    if (data->is_missing)
    {
      s_editor_material_row_begin(editor, "Status");
      ldk_ui_label(ui, "Missing material (magenta checker)");
      ldk_ui_end_horizontal(ui);
      return false;
    }

    desc = data->descriptor;
    LDKEditorUIUndoProperty material_undo = s_editor_material_undo_property(
        editor, &desc, *material_asset, x_handle_null(), 0, 0);

    if (s_editor_material_desc_editor(editor, &desc, readonly,
            context, material_undo.size ? &material_undo : NULL) &&
        !ldk_asset_manager_material_update(
            context->assets, *material_asset, &desc))
    {
      ldki_editor_log_error(
          editor, "Failed to update material asset.");
    }

    data = ldk_asset_manager_material_get_const(
        context->assets, *material_asset);

    s_editor_material_row_begin(
        editor, data ? (data->dirty ? "Shared *" : "Shared") : "");

    ldk_ui_begin_disabled(ui, readonly);

    if (data && ldk_ui_button(ui, "Save Material"))
    {
      LDKMaterialIOResult result;

      if (!ldk_asset_manager_material_save(
              context, *material_asset, &result))
      {
        ldki_editor_log_error(editor, result.error);
      }
    }

    if (ldk_ui_button(ui, "Save As..."))
    {
      XFSPath path = {0};

      if (ldk_os_dialog_show_save_file(
              editor->window, "Create material", "Materials\0*.tml\0\0", &path))
      {
        LDKAssetPath asset_path;
        if (s_editor_inspector_asset_path_validate(editor, &path,
                "Create material",
                "Create the material inside the project's runtree folder.",
                &asset_path))
        {
          LDKMaterialIOResult result;
          LDKAssetMaterial asset =
              ldk_asset_manager_material_create(
                  context, asset_path.buf, &desc, &result);

          if (x_handle_is_null(asset.h))
          {
            ldki_editor_log_error(editor, result.error);
          }
          else
          {
            *material_asset = asset;
            binding_changed = true;

            if (!ldk_asset_manager_material_save(
                    context, asset, &result))
            {
              ldki_editor_log_error(editor, result.error);
            }
          }
        }
      }
    }

    ldk_ui_end_disabled(ui);
    ldk_ui_end_horizontal(ui);
    return binding_changed;
  }

  /*
   * No asset is assigned. Keep an editor-only descriptor until the user
   * chooses Save As..., at which point it becomes a real material asset.
   */
  if (!draft)
  {
    draft = calloc(1, sizeof(*draft));
    if (!draft)
    {
      return false;
    }

    draft->binding = material_asset;

    if (!ldk_material_desc_defaults(
            LDK_MATERIAL_TYPE_VERTEX_COLOR, &draft->descriptor))
    {
      free(draft);
      return false;
    }

    draft->next = drafts;
    drafts = draft;
    previous_draft = NULL;
  }

  ldk_ui_horizontal_line(ui);

  (void)s_editor_material_desc_editor(
      editor, &draft->descriptor, readonly, context, NULL);

  s_editor_material_row_begin(editor, "Draft");
  ldk_ui_begin_disabled(ui, readonly);

  if (ldk_ui_button(ui, "Save As..."))
  {
    XFSPath path = {0};

    if (ldk_os_dialog_show_save_file(
            editor->window, "Create material", "Materials\0*.tml\0\0", &path))
    {
      LDKAssetPath asset_path;
      if (s_editor_inspector_asset_path_validate(editor, &path,
              "Create material",
              "Create the material inside the project's runtree folder.",
              &asset_path))
      {
        LDKMaterialIOResult result;
        LDKAssetMaterial asset =
            ldk_asset_manager_material_create(
                context, asset_path.buf, &draft->descriptor, &result);

        if (x_handle_is_null(asset.h))
        {
          ldki_editor_log_error(editor, result.error);
        }
        else
        {
          *material_asset = asset;
          binding_changed = true;

          if (!ldk_asset_manager_material_save(
                  context, asset, &result))
          {
            ldki_editor_log_error(editor, result.error);
          }

          if (previous_draft)
          {
            previous_draft->next = draft->next;
          }
          else
          {
            drafts = draft->next;
          }

          free(draft);
        }
      }
    }
  }

  ldk_ui_end_disabled(ui);
  ldk_ui_end_horizontal(ui);

  return binding_changed;
}

static void s_editor_inspector_material_slot(LDKEditorContext *editor,
    LDKEntity entity, u32 component_type, LDKMeshSource *mesh,
    u32 material_slot, const char *slot_label,
    const LDKMaterialIOContext *context)
{
  LDKUIContext *ui = &editor->ui;
  LDKAssetMaterial material_asset;
  LDKAssetMaterial selected_asset;
  u64 material_revision;

  ldk_ui_push_id_u32(ui, material_slot);
  ldk_ui_horizontal_line(ui);

  material_asset = s_editor_mesh_material_asset(mesh, material_slot);
  selected_asset = material_asset;
  if (s_editor_inspector_material_asset_field(
          editor, slot_label, &selected_asset, false, true))
  {
    if (x_handle_is_null(selected_asset.h))
    {
      LDKMaterialDesc desc = s_editor_mesh_material_desc(mesh, material_slot);
      if (!ldk_mesh_source_set_material_at(mesh, material_slot, &desc))
      {
        ldki_editor_log_error(editor, "Failed to clear material asset.");
      }
    }
    else if (!ldk_mesh_source_set_material_asset_at(
                 mesh, context->assets, material_slot, selected_asset))
    {
      ldki_editor_log_error(editor, "Failed to assign material asset.");
    }
    material_asset = s_editor_mesh_material_asset(mesh, material_slot);
  }

  material_revision = s_editor_mesh_material_revision(mesh, material_slot);
  if (material_revision != 0)
  {
    selected_asset = material_asset;
    if (s_editor_material_asset_editor(
            editor, &selected_asset, false, context) &&
        !s_editor_material_asset_equal(selected_asset, material_asset))
    {
      if (!ldk_mesh_source_set_material_asset_at(
              mesh, context->assets, material_slot, selected_asset))
      {
        ldki_editor_log_error(editor, "Failed to assign material asset.");
      }
    }

    (void)ldk_mesh_source_material_sync(mesh, context->assets);
    ldk_ui_pop_id(ui);
    return;
  }

  LDKMaterialDesc desc = s_editor_mesh_material_desc(mesh, material_slot);
  LDKEditorUIUndoProperty material_undo = s_editor_material_undo_property(
      editor, &desc, ldk_asset_material_null(), entity,
      component_type, material_slot);
  if (s_editor_material_desc_editor(editor, &desc, false,
          context, material_undo.size ? &material_undo : NULL) &&
      !ldk_mesh_source_set_material_at(mesh, material_slot, &desc))
  {
    ldki_editor_log_error(editor, "Failed to update inline material.");
  }

  s_editor_material_row_begin(editor, "");
  if (ldk_ui_button(ui, "Save As..."))
  {
    XFSPath path = {0};
    if (ldk_os_dialog_show_save_file(
            editor->window, "Create material", "Materials\0*.tml\0\0", &path))
    {
      LDKAssetPath asset_path;
      if (s_editor_inspector_asset_path_validate(editor, &path,
              "Create material",
              "Create the material inside the project's runtree folder.",
              &asset_path))
      {
        LDKMaterialIOResult result;
        LDKAssetMaterial asset = ldk_asset_manager_material_create(
            context, asset_path.buf, &desc, &result);
        if (x_handle_is_null(asset.h))
        {
          ldki_editor_log_error(editor, result.error);
        }
        else
        {
          if (!ldk_mesh_source_set_material_asset_at(
                  mesh, context->assets, material_slot, asset))
          {
            ldki_editor_log_error(editor, "Failed to assign material asset.");
          }
          if (!ldk_asset_manager_material_save(context, asset, &result))
          {
            ldki_editor_log_error(editor, result.error);
          }
        }
      }
    }
  }
  ldk_ui_end_horizontal(ui);
  ldk_ui_pop_id(ui);
}

static void s_editor_inspector_material(
    LDKEditorContext *editor, LDKEntity entity,
    u32 component_type, LDKMeshSource *mesh)
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
        editor, entity, component_type, mesh, slot, "Material", &context);
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
  (void)systems;
  LDKSystemRegistry *registry = ldk_ecs_system_registry_get();
  LDKSystemDesc desc = {0};
  if (!editor || !registry || !registry->is_initialized ||
      !ldk_system_registry_find_by_id(registry, system_id, &desc))
  {
    return;
  }
  LDKUIContext *ui = &editor->ui;
  char label[64];
  snprintf(label, sizeof(label), "0x%016" PRIx64,
      ldk_ecs_grouping_id(desc.component_types, desc.component_count));
  ldk_editor_ui_row_label(editor, "Group ID",
      s_editor_inspector_row_group_depth, label);
  if (!desc.component_count)
  {
    ldk_editor_ui_row_label(editor, "Components",
        s_editor_inspector_row_group_depth, "None (global system)");
  }
  for (u32 i = 0; i < desc.component_count; ++i)
  {
    const char *name = ldk_component_name_get(
        ldk_ecs_component_registry_get(), desc.component_types[i]);
    snprintf(label, sizeof(label), "0x%08x", desc.component_types[i]);
    ldk_editor_ui_row_label(editor, i == 0 ? "Components" : "",
        s_editor_inspector_row_group_depth, name ? name : label);
  }
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

  if (editor->editor_state != LDK_EDITOR_STATE_STOPED)
  {
    const LDKUIId apply_id = LDK_INSPECTOR_LIKE_BUTTON_ID;
    LDKUIRect header_rect = ldk_ui_last_rect(ui);
    LDKUIRect apply_rect = header_rect;
    LDKUIIcon apply_icon = icon;
    bool can_apply = ldki_editor_scene_play_apply_available(editor);

    apply_rect.x += apply_rect.w - 24.0f - LDK_UI_DEFAULT_PADDING;
    apply_rect.w = 24.0f;
    apply_rect.h = LDK_UI_DEFAULT_CONTROL_HEIGHT;
    apply_icon.uv = ldk_editor_icon_rects[LDK_EDITOR_ICON_HEART];
    apply_icon.color = editor->ui.theme.colors[LDK_UI_COLOR_CONTROL_TEXT];

    ldk_ui_begin_disabled(ui, !can_apply);
    if (ldk_ui_widget_icon_button(
            ui, apply_id, apply_icon, "", apply_rect) &&
        !ldki_editor_scene_play_apply_system(editor, system_id))
    {
      ldki_editor_log_error(
          editor, "Failed to apply system changes to scene.");
    }
    ldk_ui_end_disabled(ui);

    /* Runtime-only per-system suspension; preserve initialization and data. */
    LDKSystemRegistry *registry = ldk_ecs_system_registry_get();
    if (registry && ldk_system_registry_system_is_started(registry, system_id))
    {
      LDKUIRect pause_rect = header_rect;
      LDKUIIcon pause_icon = icon;
      bool paused = ldk_system_registry_system_is_paused(registry, system_id);
      pause_rect.x += pause_rect.w - 48.0f -
                      2.0f * LDK_UI_DEFAULT_PADDING;
      pause_rect.w = 24.0f;
      pause_rect.h = LDK_UI_DEFAULT_CONTROL_HEIGHT;
      pause_icon.uv = ldk_editor_icon_rects[paused
          ? LDK_EDITOR_ICON_BUTTON_PLAY
          : LDK_EDITOR_ICON_BUTTON_PAUSE];
      if (ldk_ui_widget_icon_button(
              ui, 0x53504155u, pause_icon, "", pause_rect) &&
          !ldk_system_registry_system_pause_set(registry, system_id, !paused))
      {
        ldki_editor_log_error(editor, "Failed to change system pause state.");
      }
    }
  }

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
      field_meta.groups = meta->groups;
      field_meta.group_count = meta->group_count;

      s_editor_inspector_fields_draw(editor, key, 0, &field_meta, data,
          LDK_EDITOR_INSPECTOR_GROUP_DOMAIN_SYSTEM, system_id);
    }
  }

  ldk_ui_pop_id(ui);
  ldk_ui_pop_id(ui);
  ldk_ui_pop_id(ui);
  return true;
}

enum
{
  LDK_EDITOR_INSPECTOR_INSTANCE_FIELD_POSITION = 0,
  LDK_EDITOR_INSPECTOR_INSTANCE_FIELD_ROTATION,
  LDK_EDITOR_INSPECTOR_INSTANCE_FIELD_SCALE,
  LDK_EDITOR_INSPECTOR_INSTANCE_FIELD_COUNT,
  LDK_EDITOR_INSPECTOR_INSTANCE_FIELD_OFFSET_BASE = 0x80000000u,
};

static u32 s_editor_inspector_instance_field_offset(
    u32 instance_index, u32 field_index)
{
  return LDK_EDITOR_INSPECTOR_INSTANCE_FIELD_OFFSET_BASE +
         instance_index * LDK_EDITOR_INSPECTOR_INSTANCE_FIELD_COUNT +
         field_index;
}

static bool s_editor_inspector_instance_vec3_draw(LDKEditorContext *editor,
    LDKEntity entity, const char *label, u32 field_offset, Vec3 *value)
{
  LDKComponentFieldMeta field = {0};

  if (!editor || !label || !value)
  {
    return false;
  }

  field.name = label;
  field.type = LDK_FIELD_VEC3;
  field.offset = field_offset;
  field.flags = LDK_FIELD_FLAG_NONE;
  field.widget = LDK_FIELD_WIDGET_VEC3;

  LDKInspectorNumericRowContext context = {
      entity, LDK_COMPONENT_TYPE_INSTANCED_MESH_SOURCE, &field};
  float *values[] = {&value->x, &value->y, &value->z};
  return ldk_editor_ui_row_vec3(editor, label,
      s_editor_inspector_row_group_depth, values, false,
      s_editor_inspector_vector_row_input, &context, NULL);
}

static bool s_editor_inspector_instance_rotation_draw(LDKEditorContext *editor,
    LDKEntity entity, u32 field_offset, Mat4 *instance, Quat *rotation)
{
  LDKComponentFieldMeta field = {0};
  Quat original;

  if (!editor || !instance || !rotation)
  {
    return false;
  }

  field.name = "Rotation";
  field.type = LDK_FIELD_QUAT;
  field.offset = field_offset;
  field.flags = LDK_FIELD_FLAG_NONE;
  field.widget = LDK_FIELD_WIDGET_EULER;
  original = *rotation;

  LDKInspectorEulerRowContext context = {entity,
      LDK_COMPONENT_TYPE_INSTANCED_MESH_SOURCE, &field, instance,
      rotation, false};
  ldk_editor_ui_row_euler(editor, field.name,
      s_editor_inspector_row_group_depth,
      s_editor_inspector_euler_row_content, &context, NULL);

  return !s_editor_inspector_euler_quat_equal(original, *rotation);
}

static bool s_editor_inspector_instance_matrix_finite(Mat4 matrix)
{
  for (u32 i = 0; i < 16; ++i)
  {
    if (!isfinite(matrix.m[i]))
    {
      return false;
    }
  }

  return true;
}

static bool s_editor_inspector_instance_add(LDKInstancedMeshSource *source)
{
  if (!source || source->instance_count == UINT32_MAX)
  {
    return false;
  }

  return ldk_instanced_mesh_source_resize_instances(
      source, source->instance_count + 1u);
}

static bool s_editor_inspector_instance_remove(
    LDKInstancedMeshSource *source, u32 index)
{
  if (!source || index >= source->instance_count ||
      (source->instance_count &&
          (!source->instances || !source->instance_colors)))
  {
    return false;
  }

  if (index + 1u < source->instance_count)
  {
    u32 move_count = source->instance_count - index - 1u;
    memmove(&source->instances[index], &source->instances[index + 1u],
        (size_t)move_count * sizeof(*source->instances));
    memmove(&source->instance_colors[index],
        &source->instance_colors[index + 1u],
        (size_t)move_count * sizeof(*source->instance_colors));
  }

  return ldk_instanced_mesh_source_resize_instances(
      source, source->instance_count - 1u);
}

static void s_editor_inspector_instance_selection_clear(
    LDKEditorContext *editor)
{
  if (!editor)
  {
    return;
  }

  editor->selected_instance_entity = x_handle_null();
  editor->selected_instance = 0;
}

static bool s_editor_inspector_instance_selected(
    LDKEditorContext *editor, LDKEntity entity, u32 instance_index)
{
  return editor &&
         !x_handle_is_null(editor->selected_instance_entity) &&
         ldki_editor_entity_equal(editor->selected_entity, entity) &&
         ldki_editor_entity_equal(editor->selected_instance_entity, entity) &&
         editor->selected_instance == instance_index;
}

static void s_editor_inspector_instanced_mesh_instances_draw(
    LDKEditorContext *editor, LDKEntity entity, LDKInstancedMeshSource *source)
{
  LDKUIContext *ui;
  char label[64];

  if (!editor || !source)
  {
    return;
  }

  if (!x_handle_is_null(editor->selected_instance_entity) &&
      ldki_editor_entity_equal(editor->selected_instance_entity, entity) &&
      editor->selected_instance >= source->instance_count)
  {
    s_editor_inspector_instance_selection_clear(editor);
  }

  ui = &editor->ui;
  ldk_ui_horizontal_line(ui);
  ldk_ui_begin_horizontal(ui);
  snprintf(label, sizeof(label), "Instances (%u)", source->instance_count);
  ldk_ui_label(ui, label);
  if (ldk_ui_button(ui, "+ Add Instance"))
  {
    if (!s_editor_inspector_instance_add(source))
    {
      ldk_os_dialog_show_error(
          editor->window, "Add Instance", "Failed to add mesh instance.");
    }
    else
    {
      editor->selected_entity = entity;
      editor->selected_instance_entity = entity;
      editor->selected_instance = source->instance_count - 1u;
      ldki_editor_scene_dirty_set(editor, true);
      s_editor_inspector_input_state_clear();
      memset(&s_editor_inspector_euler_state, 0,
          sizeof(s_editor_inspector_euler_state));
    }
  }
  ldk_ui_end_horizontal(ui);

  for (u32 i = 0; i < source->instance_count; ++i)
  {
    Vec3 position;
    Vec3 scale;
    Quat rotation;
    bool changed = false;
    bool remove = false;
    bool selected = s_editor_inspector_instance_selected(editor, entity, i);

    ldk_ui_push_id_u32(ui, i);
    ldk_ui_begin_horizontal(ui);
    snprintf(label, sizeof(label),
        selected ? "> Instance %u" : "Instance %u", i);
    if (ldk_ui_button_flat(ui, label))
    {
      editor->selected_entity = entity;
      editor->selected_instance_entity = entity;
      editor->selected_instance = i;
    }
    remove = ldk_ui_button_flat(ui, "Remove");
    ldk_ui_end_horizontal(ui);

    if (remove)
    {
      bool selection_owned =
          !x_handle_is_null(editor->selected_instance_entity) &&
          ldki_editor_entity_equal(editor->selected_instance_entity, entity);
      u32 selected_index = editor->selected_instance;

      if (!s_editor_inspector_instance_remove(source, i))
      {
        ldk_os_dialog_show_error(editor->window, "Remove Instance",
            "Failed to remove mesh instance.");
      }
      else
      {
        ldki_editor_scene_dirty_set(editor, true);
        if (selection_owned)
        {
          if (selected_index == i)
          {
            s_editor_inspector_instance_selection_clear(editor);
          }
          else if (selected_index > i)
          {
            editor->selected_instance = selected_index - 1u;
          }
        }

        s_editor_inspector_input_state_clear();
        memset(&s_editor_inspector_euler_state, 0,
            sizeof(s_editor_inspector_euler_state));
      }
      ldk_ui_pop_id(ui);
      break;
    }

    mat4_decompose(source->instances[i], &position, &rotation, &scale);

    changed |= s_editor_inspector_instance_vec3_draw(editor, entity,
        "Position",
        s_editor_inspector_instance_field_offset(
            i, LDK_EDITOR_INSPECTOR_INSTANCE_FIELD_POSITION),
        &position);
    changed |= s_editor_inspector_instance_rotation_draw(editor, entity,
        s_editor_inspector_instance_field_offset(
            i, LDK_EDITOR_INSPECTOR_INSTANCE_FIELD_ROTATION),
        &source->instances[i], &rotation);
    changed |= s_editor_inspector_instance_vec3_draw(editor, entity, "Scale",
        s_editor_inspector_instance_field_offset(
            i, LDK_EDITOR_INSPECTOR_INSTANCE_FIELD_SCALE),
        &scale);

    if (changed)
    {
      Mat4 matrix = mat4_compose(position, rotation, scale);
      if (s_editor_inspector_instance_matrix_finite(matrix))
      {
        if (memcmp(&source->instances[i], &matrix, sizeof(matrix)) != 0)
        {
          source->instances[i] = matrix;
          ldki_editor_scene_dirty_set(editor, true);
        }
      }
      else
      {
        ldki_editor_log_error(
            editor, "Instance transform produced an invalid matrix.");
      }
    }

    if (i + 1u < source->instance_count)
    {
      ldk_ui_horizontal_line(ui);
    }
    ldk_ui_pop_id(ui);
  }
}

/* STDX arrays use a shared Inspector vocabulary: collection fold, per-item
 * fold, ordinary reflected rows, and standard structural controls. */
static bool s_editor_array_undo_clear_record(LDKEditorContext *editor,
    LDKEntity entity, u32 component_type, u32 array_field_index,
    const LDKComponentMeta *element_meta, XArray *array)
{
  if (!array || !element_meta || !element_meta->size ||
      !x_array_count(array) || array_field_index > 255u ||
      ldk_editor_undo_has_active_change(&editor->undo)) return false;
  u32 count = x_array_count(array);
  if (count > (SIZE_MAX - sizeof(LDKEditorArrayUndoOp)) / element_meta->size)
    return false;
  size_t size = sizeof(LDKEditorArrayUndoOp) +
      (size_t)count * element_meta->size;
  u8 *before = malloc(size), *after = malloc(size);
  if (!before || !after) { free(before); free(after); return false; }
  LDKEditorArrayUndoOp before_op = {count, LDK_EDITOR_ARRAY_RESTORE_ALL};
  LDKEditorArrayUndoOp after_op = {count, LDK_EDITOR_ARRAY_CLEAR_ALL};
  memcpy(before, &before_op, sizeof(before_op));
  memcpy(after, &after_op, sizeof(after_op));
  memcpy(before + sizeof(before_op), x_array_data(array),
      size - sizeof(before_op));
  memcpy(after + sizeof(after_op), x_array_data(array),
      size - sizeof(after_op));
  LDKEditorUndoKey key = {((u64)entity.version << 32) | entity.index,
      ((u64)component_type << 32) | LDK_EDITOR_ARRAY_STRUCT_BASE |
          (array_field_index << 16)};
  bool ok = ldk_editor_undo_begin_change(&editor->undo, key,
      LDK_FIELD_ARRAY, before, size);
  if (ok) ok = ldk_editor_undo_end_change(&editor->undo, after);
  free(before); free(after);
  return ok;
}

static void s_editor_inspector_array_draw(LDKEditorContext *editor,
    LDKEntity entity, u32 component_type, const LDKComponentMeta *component_meta,
    void *component, u32 array_index)
{
  if (!editor || !component_meta || !component ||
      array_index >= component_meta->field_count) return;
  const LDKComponentFieldMeta *field = &component_meta->fields[array_index];
  const LDKArrayFieldMeta *array_meta = field->array_meta;
  const LDKComponentMeta *element_meta = array_meta && array_meta->element_meta
      ? array_meta->element_meta() : NULL;
  if (!element_meta || !element_meta->size ||
      field->offset > component_meta->size ||
      sizeof(XArray *) > component_meta->size - field->offset) return;
  XArray **array = (XArray **)((u8 *)component + field->offset);
  LDKUIContext *ui = &editor->ui;
  LDKUIIcon icon = {0};
  icon.size = ldk_sizef(LDK_UI_DEFAULT_CONTROL_HEIGHT,
      LDK_UI_DEFAULT_CONTROL_HEIGHT);
  icon.texture =
      ldk_renderer_texture_ui_handle(editor->renderer, editor->ui_atlas);
  icon.color = ui->theme.colors[LDK_UI_COLOR_CONTROL_TEXT];
  u64 owner_id = (((u64)entity.version << 32) | entity.index) ^
      (((u64)component_type * 0x9e3779b97f4a7c15ULL) >> 1);
  u32 group_id = (array_index << 16) | 0xffffu;
  bool editable = editor->editor_state == LDK_EDITOR_STATE_STOPED &&
      (field->flags & LDK_FIELD_FLAG_READONLY) == 0u;
  u32 count = *array ? x_array_count(*array) : 0u;
  char title[128];
  if (array_meta->max_count)
    snprintf(title, sizeof(title), "%s (%u/%u)",
        s_editor_inspector_field_display_name(field), count,
        array_meta->max_count);
  else
    snprintf(title, sizeof(title), "%s (%u)",
        s_editor_inspector_field_display_name(field), count);
  ldk_ui_push_id_cstr(ui, field->name);
  bool expanded = s_editor_inspector_group_expanded(
      LDK_EDITOR_INSPECTOR_GROUP_DOMAIN_ARRAY, owner_id, group_id);
  ldk_ui_begin_horizontal(ui);
  ldk_ui_set_next_weight(ui, 1.0f);
  bool next = ldk_ui_tree_node(ui, title, expanded, 0u, LDK_UI_TREE_NODE_NONE);
  if (next != expanded)
    s_editor_inspector_group_expanded_set(
        LDK_EDITOR_INSPECTOR_GROUP_DOMAIN_ARRAY, owner_id, group_id, next);
  u32 limit = array_meta->max_count ? array_meta->max_count : UINT32_MAX;
  if (array_meta->limit_field)
  {
    for (u32 k = 0; k < component_meta->field_count; ++k)
    {
      const LDKComponentFieldMeta *candidate = &component_meta->fields[k];
      if (candidate->type == LDK_FIELD_U32 &&
          strcmp(candidate->name, array_meta->limit_field) == 0 &&
          candidate->offset <= component_meta->size - sizeof(u32))
      {
        u32 sibling_limit = *(u32 *)((u8 *)component + candidate->offset);
        if (sibling_limit < limit) limit = sibling_limit;
        break;
      }
    }
  }
  ldk_ui_set_next_width(ui, ldk_ui_px(28.0f));
  ldk_ui_set_next_disabled(ui, !editable || count >= limit ||
      count >= 65535u || ldk_editor_undo_has_active_change(&editor->undo));
  icon.uv = ldk_editor_icon_rects[LDK_EDITOR_ICON_PLUS_BUTTON];
  {
    if (ldk_ui_icon_button(ui, icon, NULL))
    {
      void *element = calloc(1, element_meta->size);
      if (element)
      {
        if (array_meta->element_initialize)
          array_meta->element_initialize(element);
        if (!*array) *array = x_array_create(element_meta->size, 0u);
        if (*array && x_array_add(*array, element) == XARRAY_OK)
        {
          if (s_editor_array_undo_record(editor, entity, component_type,
                  array_index, count, element_meta, LDK_EDITOR_ARRAY_REMOVE,
                  LDK_EDITOR_ARRAY_INSERT, element))
          {
            if (array_meta->on_change) array_meta->on_change(component);
            ++count;
          }
          else
            x_array_delete_at(*array, count);
        }
        free(element);
      }
    }
  }
  ldk_ui_set_next_width(ui, ldk_ui_px(28.0f));
  ldk_ui_set_next_disabled(ui, !editable || !count ||
      ldk_editor_undo_has_active_change(&editor->undo));
  icon.uv = ldk_editor_icon_rects[LDK_EDITOR_ICON_DELETE];
  if (ldk_ui_icon_button(ui, icon, NULL))
  {
    if (s_editor_array_undo_clear_record(editor, entity, component_type,
            array_index, element_meta, *array))
    {
      x_array_clear(*array);
      if (array_meta->on_change) array_meta->on_change(component);
      s_editor_inspector_input_state_clear();
      count = 0u;
    }
  }
  ldk_ui_end_horizontal(ui);
  if (!next) { ldk_ui_pop_id(ui); return; }
  for (u32 i = 0; i < count; ++i)
  {
    if (!*array) break;
    void *element = x_array_get(*array, i);
    if (!element) break;
    u32 item_group = (array_index << 16) | i;
    ldk_ui_push_id_u32(ui, i);
    bool item_open = s_editor_inspector_group_expanded(
        LDK_EDITOR_INSPECTOR_GROUP_DOMAIN_ARRAY, owner_id, item_group);
    snprintf(title, sizeof(title), "Element %u", i);
    char headline[128];
    for (u32 k = 0; k < element_meta->field_count; ++k)
    {
      if (s_editor_inspector_headline_format(&element_meta->fields[k],
              element, element_meta->size, headline, sizeof(headline)))
      {
        snprintf(title, sizeof(title), "%s", headline);
        break;
      }
    }
    bool can_modify = editable &&
        !ldk_editor_undo_has_active_change(&editor->undo);
    ldk_ui_begin_horizontal(ui);
    ldk_ui_set_next_weight(ui, 1.0f);
    next = ldk_ui_tree_node(ui, title, item_open, 1u, LDK_UI_TREE_NODE_NONE);
    if (next != item_open)
      s_editor_inspector_group_expanded_set(
          LDK_EDITOR_INSPECTOR_GROUP_DOMAIN_ARRAY, owner_id, item_group, next);

    bool move_up = false;
    bool move_down = false;
    if (!array_meta->stable)
    {
      ldk_ui_set_next_width(ui, ldk_ui_px(24.0f));
      ldk_ui_set_next_disabled(ui, !can_modify || i == 0u);
      icon.uv = ldk_editor_icon_rects[LDK_EDITOR_ICON_UP_ARROW];
      move_up = ldk_ui_icon_button(ui, icon, NULL);

      ldk_ui_set_next_width(ui, ldk_ui_px(24.0f));
      ldk_ui_set_next_disabled(ui, !can_modify || i + 1u >= count);
      icon.uv = ldk_editor_icon_rects[LDK_EDITOR_ICON_DOWN_ARROW];
      move_down = ldk_ui_icon_button(ui, icon, NULL);
    }

    bool remove = false;
    if (!array_meta->stable || i + 1u == count)
    {
      ldk_ui_set_next_width(ui, ldk_ui_px(24.0f));
      ldk_ui_set_next_disabled(ui, !can_modify);
      icon.uv = ldk_editor_icon_rects[LDK_EDITOR_ICON_MINUS_BUTTON];
      remove = ldk_ui_icon_button(ui, icon, NULL);
    }
    ldk_ui_end_horizontal(ui);

    if (next)
    {
      void *old_element = x_arena_alloc(ui->frame_arena, element_meta->size);
      if (old_element) memcpy(old_element, element, element_meta->size);
      s_editor_array_edit = (LDKInspectorArrayEditContext){entity,
          component_type, array_index, i, element_meta, array_meta,
          component, element};
      for (u32 k = 0; k < element_meta->field_count; ++k)
      {
        s_editor_inspector_row_group_depth = 2u;
        s_editor_inspector_field_draw(editor, entity, component_type,
            element_meta, &element_meta->fields[k], element);
      }
      s_editor_inspector_row_group_depth = 0u;
      memset(&s_editor_array_edit, 0, sizeof(s_editor_array_edit));
      if (old_element && memcmp(old_element, element, element_meta->size) != 0 &&
          array_meta->on_change)
        array_meta->on_change(component);
    }
    if (can_modify)
    {
      if (move_up || move_down)
      {
        u32 a = move_up ? i - 1u : i;
        if (s_editor_array_undo_record(editor, entity, component_type,
                array_index, a, element_meta, LDK_EDITOR_ARRAY_SWAP,
                LDK_EDITOR_ARRAY_SWAP_INVERSE, element))
        {
          u8 *lhs = x_array_get(*array, a);
          u8 *rhs = x_array_get(*array, a + 1u);
          for (size_t j = 0; j < element_meta->size; ++j)
          {
            u8 tmp = lhs[j]; lhs[j] = rhs[j]; rhs[j] = tmp;
          }
          if (array_meta->on_change) array_meta->on_change(component);
          s_editor_inspector_input_state_clear();
          ldk_ui_pop_id(ui);
          break;
        }
      }
      if (remove)
      {
        if (s_editor_array_undo_record(editor, entity, component_type,
                array_index, i, element_meta, LDK_EDITOR_ARRAY_INSERT,
                LDK_EDITOR_ARRAY_REMOVE, element))
        {
          x_array_delete_at(*array, i);
          if (array_meta->on_change) array_meta->on_change(component);
          s_editor_inspector_input_state_clear();
          ldk_ui_pop_id(ui);
          break;
        }
      }
    }
    ldk_ui_pop_id(ui);
  }
  ldk_ui_pop_id(ui);
}

static void s_editor_inspector_add_component_draw(
    LDKEditorContext *editor, LDKECS *ecs, LDKGame *game, LDKEntity entity)
{
  const LDKUIId popup_id = 0x41434D50u;
  LDKUIContext *ui;
  LDKUIRect button_rect;
  float popup_content_width;
  u32 available_count = 0;
  bool added = false;
  bool open_popup;

  if (!editor || !ecs || !game)
  {
    return;
  }

  ui = &editor->ui;

  ldk_ui_set_next_width(ui, ldk_ui_fill());
  open_popup = ldk_ui_button(ui, "+ Add Component");
  button_rect = ldk_ui_last_bounding_rect(ui);
  if (open_popup)
  {
    LDKUIPoint popup_position = {
        button_rect.x,
        button_rect.y + button_rect.h,
    };
    ldk_ui_open_popup_at(ui, popup_id, popup_position);
  }

  popup_content_width =
      button_rect.w > LDK_UI_DEFAULT_PADDING * 2.0f
          ? button_rect.w - LDK_UI_DEFAULT_PADDING * 2.0f
          : 0.0f;

  if (!ldk_ui_begin_popup(ui, popup_id))
  {
    return;
  }

  if (!game->metadata_count || !game->metadata_get)
  {
    ldk_ui_set_next_width(ui, ldk_ui_px(popup_content_width));
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
    LDKComponentDesc component_desc = {0};
    if (!ldk_component_desc_get(
            &ecs->component, component_type, &component_desc) ||
        (component_desc.flags & LDK_COMPONENT_FLAG_HIDE_IN_INSPECTOR) != 0u ||
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
    ldk_ui_set_next_width(ui, ldk_ui_px(popup_content_width));
    if (ldk_ui_button_flat(ui, component_name))
    {
      void *component = ldk_ecs_component_add(entity, component_type, NULL);
      bool initialized = component != NULL;

      if (initialized &&
          component_type == LDK_COMPONENT_TYPE_INSTANCED_MESH_SOURCE)
      {
        initialized = s_editor_inspector_instance_add(
            (LDKInstancedMeshSource *)component);
        if (!initialized)
        {
          (void)ldk_ecs_component_remove(entity, component_type);
        }
      }

      if (initialized)
      {
        if (component_type == LDK_COMPONENT_TYPE_INSTANCED_MESH_SOURCE)
        {
          editor->selected_entity = entity;
          editor->selected_instance_entity = entity;
          editor->selected_instance = 0;
        }

        s_editor_inspector_component_expanded_set(component_type, true);
        ldki_editor_scene_dirty_set(editor, true);
        ldk_ui_close_current_popup(ui);
        added = true;
      }
      else
      {
        ldki_editor_log_error(editor, "Failed to add the selected component.");
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
    ldk_ui_set_next_width(ui, ldk_ui_px(popup_content_width));
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
  if (!x_handle_is_null(editor->selected_instance_entity) &&
      !ldki_editor_entity_equal(
          editor->selected_instance_entity, editor->selected_entity))
  {
    s_editor_inspector_instance_selection_clear(editor);
  }
  s_editor_inspector_column_update(editor);
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
      scroll = s_editor_inspector_scroll_begin(ui, scroll);
      s_editor_inspector_scene_properties_draw(editor);
      ldk_ui_end_scrollview(ui);
      s_editor_inspector_column_update(editor);
      return;
    }
  }

  if (editor->selected_system_id != 0)
  {
    LDKSceneSystems *systems = s_editor_inspector_scene_systems(editor);
    if (s_editor_inspector_system_index(
            systems, editor->selected_system_id, NULL))
    {
      scroll = s_editor_inspector_scroll_begin(ui, scroll);
      (void)s_editor_inspector_system_draw(
          editor, game, editor->selected_system_id);
      if (editor->editor_state == LDK_EDITOR_STATE_STOPED)
      {
        (void)ldki_editor_callbacks_draw(game, ui, systems, x_handle_null(),
            editor->selected_system_id, 0u);
      }
      ldk_ui_end_scrollview(ui);
      s_editor_inspector_column_update(editor);
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
    s_editor_inspector_column_update(editor);
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
    s_editor_inspector_column_update(editor);
    return;
  }

  scroll = s_editor_inspector_scroll_begin(ui, scroll);

  const char *name = ldk_ecs_entity_name_get(entity);
  snprintf(entity_name, sizeof(entity_name), "%s", name ? name : "");

  LDKUIIcon icon = {0};
  icon.size =
      ldk_sizef(LDK_UI_DEFAULT_CONTROL_HEIGHT, LDK_UI_DEFAULT_CONTROL_HEIGHT);

  const rgba32 flat_color = editor->ui.theme.colors[LDK_UI_COLOR_CONTROL_TEXT];

  icon.texture =
      ldk_renderer_texture_ui_handle(editor->renderer, editor->ui_atlas);
  icon.color = flat_color;
  icon.uv = ldk_editor_icon_rects[LDK_EDITOR_ICON_OBJECT];
  ldk_ui_push_id_cstr(ui, "entity_name");
  ldk_ui_push_id_u32(ui, entity.index);
  ldk_ui_push_id_u32(ui, entity.version);
  ldk_ui_set_next_height(ui, ldk_ui_px(30.0f));
  ldk_ui_begin_horizontal(ui);
  ldk_ui_set_next_width(ui, ldk_ui_px(icon.size.w));
  ldk_ui_icon_label(ui, icon, "");
  u32 name_result = ldk_ui_input_box(ui, entity_name, sizeof(entity_name));
  if ((name_result & LDK_UI_INPUT_BOX_CHANGED) != 0 &&
      strcmp(name ? name : "", entity_name) != 0)
  {
    if (!ldk_ecs_entity_name_set(entity, entity_name))
      ldki_editor_log_error(editor, "Failed to rename the selected entity.");
    else
      ldki_editor_scene_dirty_set(editor, true);
  }
  ldk_ui_end_horizontal(ui);
  ldk_ui_pop_id(ui);
  ldk_ui_pop_id(ui);
  ldk_ui_pop_id(ui);

  s_editor_inspector_flags_draw(editor, ecs, entity);

  ldk_ui_begin_disabled(ui, !editor->project.loaded ||
      editor->editor_state != LDK_EDITOR_STATE_STOPED);
  if (ldk_ui_button(ui, "Save as Prefab..."))
  {
    s_editor_inspector_prefab_save(editor, entity);
  }
  ldk_ui_end_disabled(ui);

  ldk_ui_set_next_height(ui, ldk_ui_px(4.0f));
  ldk_ui_spacer(ui);

  icon.uv = ldk_editor_icon_rects[LDK_EDITOR_ICON_COMPONENT];
  s_editor_inspector_area_state_sync(entity);
  bool stop_inspector = false;

  for (u32 component_i = 0; component_i < info->components.component_count;
      component_i++)
  {
    u32 component_type = info->components.component_type[component_i];
    LDKComponentDesc component_desc = {0};
    bool has_delete_button = true;
    bool action_invoked = false;

    if ((ldk_component_desc_get(
             &ecs->component, component_type, &component_desc) &&
            (component_desc.flags &
                LDK_COMPONENT_FLAG_HIDE_IN_INSPECTOR) != 0u) ||
        ldk_entity_component_flags_has(&ecs->entity, entity, component_type,
            LDK_COMPONENT_INSTANCE_FLAG_HIDE_IN_EDITOR))
    {
      if (component_type == LDK_COMPONENT_TYPE_INSTANCED_MESH_SOURCE &&
          !x_handle_is_null(editor->selected_instance_entity) &&
          ldki_editor_entity_equal(
              editor->selected_instance_entity, entity))
      {
        s_editor_inspector_instance_selection_clear(editor);
      }
      continue;
    }

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
    else if (component_type == LDK_COMPONENT_TYPE_AUDIO_SOURCE)
    {
      icon.uv = ldk_editor_icon_rects[LDK_EDITOR_ICON_AUDIO];
      icon.color = flat_color;
    }
    else if (component_type == LDK_COMPONENT_TYPE_TEXT3D)
    {
      icon.uv = ldk_editor_icon_rects[LDK_EDITOR_ICON_TEXT];
      icon.color = flat_color;
    }
    else
      icon.uv = ldk_editor_icon_rects[LDK_EDITOR_ICON_OBJECT];

    const LDKComponentMeta *meta =
        ldk_scene_component_meta_find_by_type(game, component_type);
    const char *component_name =
        meta ? meta->name
             : ldk_component_name_get(&ecs->component, component_type);
    char component_title[128];
    s_editor_inspector_component_title(
        component_name, component_title, sizeof(component_title));
    void *component = ldk_ecs_component_get(entity, component_type);

    ldk_ui_push_id_u32(ui, component_type);
    bool expanded = s_editor_inspector_component_expanded(component_type);

    expanded = ldk_ui_begin_area_ex(ui, component_title, icon, expanded);
    s_editor_inspector_component_expanded_set(component_type, expanded);

#ifndef LDK_UI_AREA_HEADER_HEIGHT
    ldk_ui_horizontal_line(ui);
#endif

    // Header buttons positioned over the area bar.
    u32 id = component_type + component_i;
    ldk_ui_push_id_u32(ui, id); // header button id scope
#ifdef LDK_UI_AREA_HEADER_HEIGHT
    LDKUIRect delete_rect = ldk_ui_area_header_rect(ui);
#else
    LDKUIRect delete_rect = ldk_ui_last_rect(ui);
#endif
    delete_rect.x += delete_rect.w - 24.0f - LDK_UI_DEFAULT_PADDING;
#ifdef LDK_UI_AREA_HEADER_HEIGHT
    delete_rect.y += (delete_rect.h - LDK_UI_DEFAULT_CONTROL_HEIGHT) * 0.5f;
#else
    delete_rect.y -= LDK_UI_DEFAULT_CONTROL_HEIGHT + LDK_UI_DEFAULT_SPACING;
    if (expanded && meta && component)
    {
      delete_rect.x += LDK_UI_DEFAULT_PADDING;
      delete_rect.y -= LDK_UI_DEFAULT_PADDING;
    }
#endif
    delete_rect.w = 24.0f;
    delete_rect.h = LDK_UI_DEFAULT_CONTROL_HEIGHT;
    if (component_type != LDK_COMPONENT_TYPE_TRANSFORM)
    {
      LDKUIRect enabled_rect = delete_rect;
      enabled_rect.x -= 24.0f + LDK_UI_DEFAULT_SPACING;
      if (editor->editor_state != LDK_EDITOR_STATE_STOPED)
      {
        enabled_rect.x -= 24.0f + LDK_UI_DEFAULT_SPACING;
      }
      bool enabled = ldk_ecs_component_is_enabled(entity, component_type);
      bool next = ldk_ui_widget_toggle(
          ui, (LDKUIId)0x454e4142u, enabled, enabled_rect);
      if (next != enabled)
      {
        if (!ldk_ecs_component_enabled_set(entity, component_type, next))
          ldki_editor_log_error(editor, "Failed to change component enabled state.");
        else
          ldki_editor_scene_dirty_set(editor, true);
      }
    }

    if (editor->editor_state != LDK_EDITOR_STATE_STOPED)
    {
      const LDKUIId apply_id = LDK_INSPECTOR_LIKE_BUTTON_ID + id;
      LDKUIRect apply_rect = delete_rect;
      LDKUIIcon apply_icon = icon;
      bool can_apply =
          ldki_editor_scene_play_component_can_apply(editor, entity);

      if (has_delete_button)
      {
        apply_rect.x -= 24.0f + LDK_UI_DEFAULT_SPACING;
      }
      apply_icon.uv = ldk_editor_icon_rects[LDK_EDITOR_ICON_HEART];
      apply_icon.color = flat_color;

      ldk_ui_begin_disabled(ui, !can_apply);

      if (ldk_ui_widget_icon_button(
              ui, apply_id, apply_icon, "", apply_rect) &&
          !ldki_editor_scene_play_apply_component(
              editor, entity, component_type))
      {
        ldki_editor_log_error(
            editor, "Failed to apply component changes to scene.");
      }
      ldk_ui_end_disabled(ui);
    }

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
      /*
       * Inspector visibility follows Comet metadata. It intentionally does
       * not use scene serialization rules: runtime/non-serialized fields may
       * still be useful to inspect.
       */
      const LDKComponentMeta *fields_meta = meta;
      void *fields_component = component;
      u32 fields_type = component_type;
      if (component_type == LDK_COMPONENT_TYPE_INSTANCED_MESH_SOURCE)
      {
        LDKInstancedMeshSource *instances = component;
        fields_meta = ldk_scene_component_meta_find_by_type(
            game, LDK_COMPONENT_TYPE_MESH_SOURCE);
        fields_component = &instances->source;
      }
      if (fields_meta)
      {
        u64 group_owner_id = fields_meta->type != 0u
            ? (u64)fields_meta->type
            : (u64)fields_type;
        s_editor_inspector_fields_draw(editor, entity, fields_type,
            fields_meta, fields_component,
            LDK_EDITOR_INSPECTOR_GROUP_DOMAIN_COMPONENT, group_owner_id);
      }
      if (fields_type == LDK_COMPONENT_TYPE_MESH_SOURCE ||
          fields_type == LDK_COMPONENT_TYPE_INSTANCED_MESH_SOURCE)
      {
        s_editor_inspector_material(
            editor, entity, fields_type, fields_component);
      }
      if (editor->editor_state == LDK_EDITOR_STATE_STOPED)
      {
        action_invoked = ldki_editor_callbacks_draw(game, ui,
            &editor->current_scene_systems, entity, 0u, component_type);
      }
      if (!action_invoked &&
          component_type == LDK_COMPONENT_TYPE_INSTANCED_MESH_SOURCE)
      {
        s_editor_inspector_instanced_mesh_instances_draw(
            editor, entity, (LDKInstancedMeshSource *)component);
      }
      if (!action_invoked &&
          component_type == LDK_COMPONENT_TYPE_KEYFRAME_ANIMATION_SOURCE)
      {
        s_editor_inspector_keyframe_animations_draw(editor, entity,
            (LDKKeyFrameAnimationSource *)component,
            editor->editor_state != LDK_EDITOR_STATE_STOPED);
      }
    }

    if (!action_invoked && has_delete_button &&
        ldk_ui_widget_icon_button(ui, id, icon, "", delete_rect))
    {
      if (ldk_os_dialog_show_yes_no(editor->window, "Delete Component ?",
              "Are you sure you want to delete this component instance ?"))
      {
        if (!ldk_ecs_component_remove(entity, component_type))
        {
          ldk_os_dialog_show_error(editor->window, "Error", "Error removing component.");
        }
        else
        {
          ldki_editor_scene_dirty_set(editor, true);
        }
      }
      
    }

    ldk_ui_end_area(ui);
    // The spacer already contributes two layout gaps: 8 px in total.
    ldk_ui_set_next_height(ui, ldk_ui_px(0.0f));
    ldk_ui_spacer(ui);
    ldk_ui_pop_id(ui);
    ldk_ui_pop_id(ui);
    if (action_invoked)
    {
      stop_inspector = true;
      break;
    }
  }

  if (!stop_inspector)
  {
    s_editor_inspector_add_component_draw(editor, ecs, game, entity);
  }

  ldk_ui_end_scrollview(ui);
  s_editor_inspector_column_update(editor);
}
