/* Animation authoring window. Included by ldk_editor_dock.c. */
#include <ctype.h>
#include <ldk_keyframe_animation.h>
#include <ldk_property.h>
#include "ldk_editor_color_picker.h"
#include <errno.h>
#include <component/ldk_transform.h>
#include <component/ldk_keyframe_animation_source.h>
#include <module/ldk_asset_manager.h>
#include <module/ldk_ecs.h>
#include <module/ldk_scenegraph.h>

/* Preview state deliberately lives outside the clip. */
typedef struct LDKEditorAnimationSavedProperty
{
  LDKEntity entity;
  u32 component_type;
  char property_name[LDK_KEYFRAME_PROPERTY_NAME_CAPACITY];
  LDKPropertyValue value;
} LDKEditorAnimationSavedProperty;

typedef struct LDKEditorAnimationExpandedTrack
{
  u64 path;
  u32 component_type;
  char property_name[LDK_KEYFRAME_PROPERTY_NAME_CAPACITY];
  bool expanded;
} LDKEditorAnimationExpandedTrack;

typedef struct LDKEditorAnimationPopupComponent
{
  LDKEntity entity;
  u32 component_type;
  bool expanded;
} LDKEditorAnimationPopupComponent;

typedef struct LDKEditorAnimationPopupNode
{
  LDKEntity entity;
  bool entity_expanded;
  bool transform_expanded;
} LDKEditorAnimationPopupNode;

typedef struct LDKEditorAnimationRow
{
  i32 track; /* -3: event, -2: events header, -1: key summary. */
  i32 axis;  /* Event index for -3; -1 parent; 0..2: X/Y/Z. */
} LDKEditorAnimationRow;

typedef struct LDKEditorAnimationState
{
  LDKKeyframeAnimation clip;
  LDKEntity root;
  i32 source_animation_index;
  char path[256];
  char event_text[128];
  char event_number[32];
  char duration_text[48];
  bool duration_text_editing;
  LDKUIRect timeline_sheet_rect;
  LDKUIRect timeline_graph_rect;
  LDKUIRect timeline_visible_rect;
  float playhead;
  u32 selected_track;
  u32 selected_key;
  i32 selected_event;
  bool key_selected;
  bool dragging_key;
  bool scrubbing;
  float property_width;
  u64 last_sheet_click_ticks;
  float last_sheet_click_x;
  float last_sheet_click_y;
  i32 last_sheet_click_track;
  i32 last_sheet_click_axis;
  bool events_editor_open;
  float drag_start_x;
  float drag_start_time;
  float timeline_zoom;
  float timeline_offset;
  LDKUIPoint sheet_scroll;
  LDKUIPoint property_popup_scroll;
  LDKEditorAnimationExpandedTrack *expanded_tracks;
  u32 expanded_count;
  u32 expanded_capacity;
  LDKEditorAnimationPopupNode *popup_nodes;
  LDKEditorAnimationPopupComponent *popup_components;
  u32 popup_component_count;
  u32 popup_component_capacity;
  u32 popup_node_count;
  u32 popup_node_capacity;
  LDKUIId axis_edit_id;
  char axis_edit_text[X_SMALLSTR_MAX_LENGTH + 1];
  bool initialized;
  bool playing;
  bool preview;
  bool looping;
  bool dirty;
  LDKEditorAnimationSavedProperty *saved;
  u32 saved_count;
  u32 saved_capacity;
} LDKEditorAnimationState;

static LDKEditorAnimationState s_editor_animation;

static void s_editor_animation_ui_state_clear(void)
{
  LDKEditorAnimationState *state = &s_editor_animation;
  free(state->expanded_tracks);
  free(state->popup_nodes);
  free(state->popup_components);
  state->popup_components = NULL;
  state->popup_component_count = state->popup_component_capacity = 0;
  state->expanded_tracks = NULL;
  state->expanded_count = state->expanded_capacity = 0;
  state->popup_nodes = NULL;
  state->popup_node_count = state->popup_node_capacity = 0;
  state->axis_edit_id = 0;
  state->axis_edit_text[0] = '\0';
  state->property_popup_scroll = (LDKUIPoint){0};
  state->last_sheet_click_ticks = 0;
  state->timeline_sheet_rect = (LDKUIRect){0};
  state->timeline_graph_rect = (LDKUIRect){0};
  state->timeline_visible_rect = (LDKUIRect){0};
}

static bool s_editor_animation_expanded(const LDKKeyframeTrack *track)
{
  const LDKEditorAnimationState *state = &s_editor_animation;
  for (u32 i = 0; i < state->expanded_count; ++i)
  {
    const LDKEditorAnimationExpandedTrack *entry = &state->expanded_tracks[i];
    if (entry->path == track->target_path &&
        entry->component_type == track->component_type &&
        strcmp(entry->property_name, track->property_name) == 0)
    {
      return entry->expanded;
    }
  }
  return false;
}

static void s_editor_animation_expand(const LDKKeyframeTrack *track,
    bool expanded)
{
  LDKEditorAnimationState *state = &s_editor_animation;
  for (u32 i = 0; i < state->expanded_count; ++i)
  {
    LDKEditorAnimationExpandedTrack *entry = &state->expanded_tracks[i];
    if (entry->path == track->target_path &&
        entry->component_type == track->component_type &&
        strcmp(entry->property_name, track->property_name) == 0)
    {
      entry->expanded = expanded;
      return;
    }
  }
  if (state->expanded_count == state->expanded_capacity)
  {
    u32 capacity = state->expanded_capacity ? state->expanded_capacity * 2 : 16;
    void *memory = realloc(state->expanded_tracks,
        (size_t)capacity * sizeof(*state->expanded_tracks));
    if (!memory)
    {
      return;
    }
    state->expanded_tracks = memory;
    state->expanded_capacity = capacity;
  }
  LDKEditorAnimationExpandedTrack *entry =
      &state->expanded_tracks[state->expanded_count++];
  *entry = (LDKEditorAnimationExpandedTrack){0};
  entry->path = track->target_path;
  entry->component_type = track->component_type;
  snprintf(entry->property_name, sizeof(entry->property_name), "%s",
      track->property_name);
  entry->expanded = expanded;
}

static LDKEditorAnimationPopupComponent *s_editor_animation_popup_component(
    LDKEntity entity, u32 component_type)
{
  LDKEditorAnimationState *state = &s_editor_animation;
  for (u32 i = 0; i < state->popup_component_count; ++i)
  {
    LDKEditorAnimationPopupComponent *entry = &state->popup_components[i];
    if (entry->entity.index == entity.index &&
        entry->entity.version == entity.version &&
        entry->component_type == component_type)
    {
      return entry;
    }
  }
  if (state->popup_component_count == state->popup_component_capacity)
  {
    u32 capacity = state->popup_component_capacity ?
        state->popup_component_capacity * 2 : 16;
    void *memory = realloc(state->popup_components,
        (size_t)capacity * sizeof(*state->popup_components));
    if (!memory)
    {
      return NULL;
    }
    state->popup_components = memory;
    state->popup_component_capacity = capacity;
  }
  LDKEditorAnimationPopupComponent *entry =
      &state->popup_components[state->popup_component_count++];
  *entry = (LDKEditorAnimationPopupComponent){entity, component_type, true};
  return entry;
}

static LDKEditorAnimationPopupNode *s_editor_animation_popup_node(
    LDKEntity entity, bool root)
{
  LDKEditorAnimationState *state = &s_editor_animation;
  for (u32 i = 0; i < state->popup_node_count; ++i)
  {
    LDKEditorAnimationPopupNode *node = &state->popup_nodes[i];
    if (node->entity.index == entity.index &&
        node->entity.version == entity.version)
    {
      return node;
    }
  }
  if (state->popup_node_count == state->popup_node_capacity)
  {
    u32 capacity = state->popup_node_capacity ?
        state->popup_node_capacity * 2 : 16;
    void *memory = realloc(state->popup_nodes,
        (size_t)capacity * sizeof(*state->popup_nodes));
    if (!memory)
    {
      return NULL;
    }
    state->popup_nodes = memory;
    state->popup_node_capacity = capacity;
  }
  LDKEditorAnimationPopupNode *node =
      &state->popup_nodes[state->popup_node_count++];
  *node = (LDKEditorAnimationPopupNode){entity, root, root};
  return node;
}

static void s_editor_animation_restore(void)
{
  LDKEditorAnimationState *state = &s_editor_animation;
  if (!state->preview)
  {
    return;
  }
  for (u32 i = 0; i < state->saved_count; i++)
  {
    const LDKEditorAnimationSavedProperty *s = &state->saved[i];
    (void)ldk_property_set(s->entity, s->component_type,
        s->property_name, &s->value);
  }
  if (ldk_ecs_component_get(state->root, LDK_COMPONENT_TYPE_TRANSFORM))
  {
    (void)ldk_scenegraph_update_entity(state->root);
  }
  free(state->saved);
  state->saved = NULL;
  state->saved_count = state->saved_capacity = 0;
  state->preview = false;
  state->playing = false;
}

static bool s_editor_animation_preview_begin(void)
{
  LDKEditorAnimationState *state = &s_editor_animation;
  if (state->preview)
  {
    return true;
  }
  state->preview = true;
  for (u32 i = 0; i < state->clip.track_count; ++i)
  {
    const LDKKeyframeTrack *track = &state->clip.tracks[i];
    LDKEntity entity;
    LDKPropertyValue value;
    if (!ldk_keyframe_target_resolve(state->root,
            track->target_path, &entity) ||
        !ldk_keyframe_animation_value_from_entity(entity, track, &value))
    {
      continue;
    }
    if (state->saved_count == state->saved_capacity)
    {
      u32 next = state->saved_capacity ? state->saved_capacity * 2 : 16;
      void *memory = realloc(state->saved, next * sizeof(*state->saved));
      if (!memory)
      {
        s_editor_animation_restore();
        return false;
      }
      state->saved = memory;
      state->saved_capacity = next;
    }
    LDKEditorAnimationSavedProperty *item =
        &state->saved[state->saved_count++];
    *item = (LDKEditorAnimationSavedProperty){0};
    item->entity = entity;
    item->component_type = track->component_type;
    snprintf(item->property_name, sizeof(item->property_name), "%s",
        track->property_name);
    item->value = value;
  }
  return true;
}

static void s_editor_animation_set_time(float time)
{
  LDKEditorAnimationState *state = &s_editor_animation;
  state->playhead = time;
  if (s_editor_animation_preview_begin())
  {
    (void)ldk_keyframe_animation_apply(&state->clip,
        state->root, state->playhead);
    (void)ldk_scenegraph_update_entity(state->root);
  }
}

static float s_editor_animation_span(void);
static void s_editor_animation_view_clamp(void);

static void s_editor_animation_tick(float dt)
{
  LDKEditorAnimationState *state = &s_editor_animation;
  if (!state->playing || !state->preview ||
      !isfinite(dt) || dt <= 0.0f)
  {
    return;
  }
  if (state->clip.duration <= 0.0f)
  {
    state->playing = false;
    return;
  }
  float next_time = state->playhead + dt;
  if (next_time >= state->clip.duration)
  {
    if (state->looping)
    {
      next_time = fmodf(next_time, state->clip.duration);
    }
    else
    {
      next_time = state->clip.duration;
      state->playing = false;
    }
  }
  s_editor_animation_set_time(next_time);
  if (state->timeline_zoom > 1.0f &&
      state->playhead > state->timeline_offset + s_editor_animation_span())
  {
    state->timeline_offset = state->playhead -
        s_editor_animation_span() * 0.15f;
    s_editor_animation_view_clamp();
  }
}

static const char *s_editor_animation_property_label(
    const LDKKeyframeTrack *track)
{
  const LDKComponentFieldMeta *field = ldk_property_field_find(
      track->component_type, track->property_name);
  return field && field->display_name && field->display_name[0] ?
      field->display_name : track->property_name;
}

static u32 s_editor_animation_axis_count(const LDKKeyframeTrack *track)
{
  switch (track->value_type)
  {
  case LDK_FIELD_VEC2: return 2;
  case LDK_FIELD_VEC3: return 3;
  case LDK_FIELD_VEC4: return 4;
  case LDK_FIELD_QUAT:
  {
    const LDKComponentFieldMeta *field = ldk_property_field_find(
        track->component_type, track->property_name);
    return field && field->widget == LDK_FIELD_WIDGET_EULER ? 3 : 4;
  }
  case LDK_FIELD_U32:
  {
    const LDKComponentFieldMeta *field = ldk_property_field_find(
        track->component_type, track->property_name);
    return field && field->widget == LDK_FIELD_WIDGET_COLOR ? 4 : 1;
  }
  default: return 1;
  }
}


/* UI popups for rarely used animation commands. */
enum
{
  LDK_EDITOR_ANIM_ADD_PROPERTY_POPUP_ID = 0x414E4901u,
  LDK_EDITOR_ANIM_EDIT_POPUP_ID = 0x414E4902u,
  LDK_EDITOR_ANIM_EVENTS_POPUP_ID = 0x414E4903u,
  LDK_EDITOR_ANIM_VIEW_POPUP_ID = 0x414E4904u,
};

/* The Dopesheet is an editor-only view over the public clip API. */
#define LDK_EDITOR_ANIM_ROW_HEIGHT 24.0f
#define LDK_EDITOR_ANIM_RULER_HEIGHT 27.0f

static float s_editor_animation_clamp(float value, float min, float max)
{
  return value < min ? min : value > max ? max : value;
}

static void s_editor_animation_reset_selection(void)
{
  s_editor_animation.selected_track = 0;
  s_editor_animation.selected_key = 0;
  s_editor_animation.selected_event = -1;
  s_editor_animation.key_selected = false;
  s_editor_animation.dragging_key = false;
  s_editor_animation.scrubbing = false;
}

static float s_editor_animation_span(void)
{
  const LDKEditorAnimationState *state = &s_editor_animation;
  float duration = fmaxf(state->clip.duration, 0.1f);
  float zoom = state->timeline_zoom > 0.0f ? state->timeline_zoom : 1.0f;
  return duration / zoom;
}

static void s_editor_animation_view_clamp(void)
{
  LDKEditorAnimationState *state = &s_editor_animation;
  if (!isfinite(state->timeline_zoom) || state->timeline_zoom < 1.0f)
  {
    state->timeline_zoom = 1.0f;
  }
  if (state->timeline_zoom > 64.0f)
  {
    state->timeline_zoom = 64.0f;
  }
  float duration = fmaxf(state->clip.duration, 0.1f);
  state->timeline_offset = s_editor_animation_clamp(
      state->timeline_offset, 0.0f, duration - s_editor_animation_span());
}

static void s_editor_animation_zoom(float factor)
{
  LDKEditorAnimationState *state = &s_editor_animation;
  float middle = state->timeline_offset + s_editor_animation_span() * 0.5f;
  state->timeline_zoom *= factor;
  s_editor_animation_view_clamp();
  state->timeline_offset = middle - s_editor_animation_span() * 0.5f;
  s_editor_animation_view_clamp();
}

/* Keep the time under the pointer stationary while zooming. */
static void s_editor_animation_wheel_zoom(float steps, float anchor)
{
  LDKEditorAnimationState *state = &s_editor_animation;
  float old_span = s_editor_animation_span();
  float anchor_time = state->timeline_offset + anchor * old_span;
  state->timeline_zoom *= powf(1.25f, steps);
  s_editor_animation_view_clamp();
  state->timeline_offset = anchor_time - anchor * s_editor_animation_span();
  s_editor_animation_view_clamp();
}

static float s_editor_animation_time_x(
    LDKUIRect graph, const LDKEditorAnimationState *state, float time)
{
  return graph.x + (time - state->timeline_offset) *
      graph.w / s_editor_animation_span();
}

static float s_editor_animation_x_time(
    LDKUIRect graph, const LDKEditorAnimationState *state, float x)
{
  float time = state->timeline_offset +
      (x - graph.x) * s_editor_animation_span() / graph.w;
  return s_editor_animation_clamp(time, 0.0f, state->clip.duration);
}

/* Resolve labels only in the editor; runtime animation uses numeric hashes. */
static bool s_editor_animation_target_walk(LDKEntity entity, u64 parent_path,
    u64 target_path, char *text, size_t capacity, u32 depth)
{
  if (depth > 128)
  {
    return false;
  }
  const LDKTransform *transform = ldk_ecs_component_get_const(
      entity, LDK_COMPONENT_TYPE_TRANSFORM);
  if (!transform)
  {
    return false;
  }
  LDKEntity child = transform->first_child;
  while (!x_handle_is_null(child))
  {
    const LDKTransform *child_transform = ldk_ecs_component_get_const(
        child, LDK_COMPONENT_TYPE_TRANSFORM);
    if (!child_transform)
    {
      break;
    }
    LDKEntity next = child_transform->next_sibling;
    u64 hash = ldk_ecs_entity_name_hash_get(child);
    if (hash)
    {
      u64 path = ldk_keyframe_path_child(parent_path, hash);
      size_t length = strlen(text);
      const char *name = ldk_ecs_entity_name_get(child);
      if (!name || !name[0])
      {
        name = "(unnamed)";
      }
      if (length + 3 < capacity)
      {
        snprintf(text + length, capacity - length, "/%s", name);
        if (path == target_path || s_editor_animation_target_walk(
                child, path, target_path, text, capacity, depth + 1))
        {
          return true;
        }
        text[length] = '\0';
      }
    }
    child = next;
  }
  return false;
}

static void s_editor_animation_track_label(
    const LDKEditorAnimationState *state, u32 index,
    char *buffer, size_t capacity)
{
  const LDKKeyframeTrack *track = &state->clip.tracks[index];
  char name[160] = "Root";
  const char *root_name = ldk_ecs_entity_name_get(state->root);
  if (root_name && root_name[0])
  {
    snprintf(name, sizeof(name), "%s", root_name);
  }
  if (track->target_path != ldk_keyframe_path_root() &&
      !s_editor_animation_target_walk(state->root,
          ldk_keyframe_path_root(), track->target_path,
          name, sizeof(name), 0))
  {
    snprintf(name, sizeof(name), "Unresolved %016" PRIx64,
        track->target_path);
  }
  const LDKComponentMeta *meta =
      ldk_property_component_meta(track->component_type);
  snprintf(buffer, capacity, "%s : %s.%s", name,
      meta && meta->name ? meta->name : "Component",
      s_editor_animation_property_label(track));
}

static bool s_editor_animation_target_entity_walk(LDKEntity entity,
    u64 parent_path, u64 target_path, LDKEntity *result, u32 depth)
{
  if (depth > 128)
  {
    return false;
  }
  const LDKTransform *transform = ldk_ecs_component_get_const(
      entity, LDK_COMPONENT_TYPE_TRANSFORM);
  if (!transform)
  {
    return false;
  }
  LDKEntity child = transform->first_child;
  while (!x_handle_is_null(child))
  {
    const LDKTransform *next_transform = ldk_ecs_component_get_const(
        child, LDK_COMPONENT_TYPE_TRANSFORM);
    if (!next_transform)
    {
      break;
    }
    LDKEntity next = next_transform->next_sibling;
    u64 hash = ldk_ecs_entity_name_hash_get(child);
    if (hash)
    {
      u64 path = ldk_keyframe_path_child(parent_path, hash);
      if (path == target_path)
      {
        *result = child;
        return true;
      }
      if (s_editor_animation_target_entity_walk(
              child, path, target_path, result, depth + 1))
      {
        return true;
      }
    }
    child = next;
  }
  return false;
}

static bool s_editor_animation_track_entity(u32 index, LDKEntity *entity)
{
  LDKEditorAnimationState *state = &s_editor_animation;
  if (index >= state->clip.track_count ||
      !ldk_ecs_component_get_const(state->root, LDK_COMPONENT_TYPE_TRANSFORM))
  {
    return false;
  }
  if (state->clip.tracks[index].target_path == ldk_keyframe_path_root())
  {
    *entity = state->root;
    return true;
  }
  return s_editor_animation_target_entity_walk(state->root,
      ldk_keyframe_path_root(), state->clip.tracks[index].target_path,
      entity, 0);
}

static void s_editor_animation_draw_rect(
    LDKUIContext *ui, LDKUIRect rect, rgba32 color)
{
  if (rect.w > 0.0f && rect.h > 0.0f)
  {
    ldk_ui_widget_gradient(ui, 0, color, color, color, color, rect);
  }
}

/* Dragging alters only key time, retaining its original value. */
static void s_editor_animation_move_key(float new_time)
{
  LDKEditorAnimationState *state = &s_editor_animation;
  if (!state->key_selected || state->selected_track >= state->clip.track_count)
  {
    return;
  }
  LDKKeyframeTrack *track = &state->clip.tracks[state->selected_track];
  if (state->selected_key >= track->count)
  {
    return;
  }
  float old_time = track->keys[state->selected_key].time;
  new_time = s_editor_animation_clamp(new_time, 0.0f, state->clip.duration);
  new_time = floorf(new_time * 100.0f + 0.5f) / 100.0f;
  if (fabsf(old_time - new_time) < 0.0001f)
  {
    return;
  }
  for (u32 i = 0; i < track->count; ++i)
  {
    if (i != state->selected_key &&
        fabsf(track->keys[i].time - new_time) < 0.0001f)
    {
      return; /* Moving a key must not silently overwrite another. */
    }
  }
  LDKPropertyValue value = track->keys[state->selected_key].value;
  if (!ldk_keyframe_animation_key_remove(
          &state->clip, state->selected_track, state->selected_key))
  {
    return;
  }
  if (!ldk_keyframe_animation_key_set(
          &state->clip, state->selected_track, new_time, value))
  {
    /* Preserve the original value if insertion fails. */
    (void)ldk_keyframe_animation_key_set(
        &state->clip, state->selected_track, old_time, value);
    state->key_selected = false;
    return;
  }
  track = &state->clip.tracks[state->selected_track];
  for (u32 i = 0; i < track->count; ++i)
  {
    if (track->keys[i].time == new_time)
    {
      state->selected_key = i;
      break;
    }
  }
  state->dirty = true;
  s_editor_animation_set_time(new_time);
}

/* Channels shown in the editor are views over one typed property key. */
static bool s_editor_animation_track_value(u32 index, LDKPropertyValue *out)
{
  LDKEditorAnimationState *state = &s_editor_animation;
  if (ldk_keyframe_animation_sample(&state->clip, index,
          state->playhead, out))
  {
    return true;
  }
  LDKEntity entity;
  return s_editor_animation_track_entity(index, &entity) &&
      ldk_keyframe_animation_value_from_entity(
          entity, &state->clip.tracks[index], out);
}

static Vec3 s_editor_animation_euler_degrees(Vec4 value)
{
  Quat q = quat_norm(quat_make(value.x, value.y, value.z, value.w));
  Vec3 angles = quat_to_euler_xyz(q);
  return vec3_make(rad_to_deg(angles.x), rad_to_deg(angles.y),
      rad_to_deg(angles.z));
}

static float s_editor_animation_axis_value(u32 track_index, u32 axis)
{
  LDKPropertyValue value;
  if (!s_editor_animation_track_value(track_index, &value))
  {
    return 0;
  }
  const LDKKeyframeTrack *track = &s_editor_animation.clip.tracks[track_index];
  const LDKComponentFieldMeta *field = ldk_property_field_find(
      track->component_type, track->property_name);
  if (track->value_type == LDK_FIELD_QUAT && field &&
      field->widget == LDK_FIELD_WIDGET_EULER)
  {
    Vec3 degrees = s_editor_animation_euler_degrees(value.vector);
    return axis == 0 ? degrees.x : axis == 1 ? degrees.y : degrees.z;
  }
  if (track->value_type == LDK_FIELD_U32 && field &&
      field->widget == LDK_FIELD_WIDGET_COLOR)
  {
    /* rgba32 uses 0xRRGGBBAA, so R occupies the highest byte. */
    return (float)(((u32)value.integer >> ((3u - axis) * 8u)) & 255u);
  }
  switch (track->value_type)
  {
  case LDK_FIELD_FLOAT:
  case LDK_FIELD_VEC2:
  case LDK_FIELD_VEC3:
  case LDK_FIELD_VEC4:
  case LDK_FIELD_QUAT:
    return axis == 0 ? value.vector.x : axis == 1 ? value.vector.y :
        axis == 2 ? value.vector.z : value.vector.w;
  default:
    return (float)value.integer;
  }
}

static bool s_editor_animation_axis_set(u32 track_index, u32 axis, float next)
{
  LDKEditorAnimationState *state = &s_editor_animation;
  if (!isfinite(next) || track_index >= state->clip.track_count)
  {
    return false;
  }
  const LDKKeyframeTrack *track = &state->clip.tracks[track_index];
  if (axis >= s_editor_animation_axis_count(track))
  {
    return false;
  }
  LDKPropertyValue value;
  if (!s_editor_animation_track_value(track_index, &value))
  {
    return false;
  }
  const LDKComponentFieldMeta *field = ldk_property_field_find(
      track->component_type, track->property_name);
  if (track->value_type == LDK_FIELD_QUAT && field &&
      field->widget == LDK_FIELD_WIDGET_EULER)
  {
    Vec3 degrees = s_editor_animation_euler_degrees(value.vector);
    if (axis == 0) degrees.x = next;
    else if (axis == 1) degrees.y = next;
    else degrees.z = next;
    Quat qx = quat_axis_angle(vec3_make(1, 0, 0),
        deg_to_rad(remainderf(degrees.x, 360.0f)));
    Quat qy = quat_axis_angle(vec3_make(0, 1, 0),
        deg_to_rad(remainderf(degrees.y, 360.0f)));
    Quat qz = quat_axis_angle(vec3_make(0, 0, 1),
        deg_to_rad(remainderf(degrees.z, 360.0f)));
    Quat q = quat_norm(quat_mul(qz, quat_mul(qy, qx)));
    value.vector = vec4_make(q.x, q.y, q.z, q.w);
  }
  else if (track->value_type == LDK_FIELD_U32 && field &&
      field->widget == LDK_FIELD_WIDGET_COLOR)
  {
    u32 shift = (3u - axis) * 8u;
    u32 mask = 255u << shift;
    u32 channel = (u32)fmaxf(0, fminf(255, roundf(next)));
    value.integer = ((u32)value.integer & ~mask) | (channel << shift);
  }
  else if (track->value_type == LDK_FIELD_FLOAT ||
      track->value_type == LDK_FIELD_VEC2 ||
      track->value_type == LDK_FIELD_VEC3 ||
      track->value_type == LDK_FIELD_VEC4 ||
      track->value_type == LDK_FIELD_QUAT)
  {
    if (axis == 0) value.vector.x = next;
    else if (axis == 1) value.vector.y = next;
    else if (axis == 2) value.vector.z = next;
    else value.vector.w = next;
  }
  else
  {
    if (track->value_type == LDK_FIELD_BOOL)
    {
      value.integer = next != 0;
    }
    else if (track->value_type == LDK_FIELD_I32 ||
        track->value_type == LDK_FIELD_U32 ||
        track->value_type == LDK_FIELD_ENUM)
    {
      value.integer = (i64)next;
    }
    else
    {
      return false;
    }
  }
  if (!ldk_keyframe_animation_key_set(&state->clip,
          track_index, state->playhead, value))
  {
    return false;
  }
  state->selected_track = track_index;
  state->key_selected = false;
  state->dirty = true;
  s_editor_animation_set_time(state->playhead);
  return true;
}

static bool s_editor_animation_key_at_cursor(u32 track_index)
{
  LDKEditorAnimationState *state = &s_editor_animation;
  LDKPropertyValue value;
  if (!s_editor_animation_track_value(track_index, &value) ||
      !ldk_keyframe_animation_key_set(&state->clip, track_index,
          state->playhead, value))
  {
    return false;
  }
  state->selected_track = track_index;
  state->key_selected = false;
  state->dirty = true;
  s_editor_animation_set_time(state->playhead);
  return true;
}

/* Removing a key affects only the selected track; no confirmation needed. */
static bool s_editor_animation_delete_selected_key(void)
{
  LDKEditorAnimationState *state = &s_editor_animation;
  if (!state->key_selected || state->selected_track >= state->clip.track_count)
  {
    return false;
  }
  const LDKKeyframeTrack *track = &state->clip.tracks[state->selected_track];
  if (state->selected_key >= track->count ||
      !ldk_keyframe_animation_key_remove(&state->clip,
          state->selected_track, state->selected_key))
  {
    return false;
  }
  state->key_selected = false;
  state->dragging_key = false;
  state->dirty = true;
  s_editor_animation_set_time(state->playhead);
  return true;
}

/* A track removes all its keys, so require explicit user confirmation. */
static bool s_editor_animation_remove_track_confirm(
    LDKEditorContext *editor, u32 track_index)
{
  LDKEditorAnimationState *state = &s_editor_animation;
  if (track_index >= state->clip.track_count ||
      !ldk_os_dialog_show_yes_no(editor->window, "Delete animation track?",
          "Are you sure you want to delete this track and all its keyframes?"))
  {
    return false;
  }
  s_editor_animation_restore();
  if (!ldk_keyframe_animation_track_remove(&state->clip, track_index))
  {
    return false;
  }
  s_editor_animation_reset_selection();
  state->last_sheet_click_ticks = 0;
  state->dirty = true;
  s_editor_animation_set_time(state->playhead);
  return true;
}

static bool s_editor_animation_float_parse(const char *text, float *out)
{
  char *end;
  float number;
  if (!text || !out)
  {
    return false;
  }
  number = strtof(text, &end);
  if (end == text || !isfinite(number))
  {
    return false;
  }
  while (isspace((unsigned char)*end))
  {
    ++end;
  }
  if (*end != '\0')
  {
    return false;
  }
  *out = number;
  return true;
}

/* The same native LDK input widget and float formatting as the Inspector.
 * Preserve partial edits ("-", "-." etc.) until the text becomes valid. */
static void s_editor_animation_axis_input(LDKUIContext *ui,
    u32 track_index, u32 axis, LDKUIRect rect)
{
  LDKEditorAnimationState *state = &s_editor_animation;
  const LDKKeyframeTrack *track = &state->clip.tracks[track_index];
  const LDKComponentFieldMeta *field = ldk_property_field_find(
      track->component_type, track->property_name);
  u64 combined = track->target_path ^ (track->target_path >> 32);
  u32 hash = (u32)combined * 16777619u +
      track->component_type * 1103515245u + axis * 2747636419u;
  for (const char *p = track->property_name; *p; ++p)
    hash = (hash ^ (unsigned char)*p) * 16777619u;
  LDKUIId id = (LDKUIId)(0x5EAF7001u ^ hash);
  if (!id) id = 1;

  LDKPropertyValue value;
  if (!s_editor_animation_track_value(track_index, &value))
    return;
  if (track->value_type == LDK_FIELD_BOOL)
  {
    bool enabled = value.integer != 0;
    bool next = ldk_ui_widget_toggle(ui, id, enabled, rect);
    if (next != enabled)
    {
      (void)s_editor_animation_axis_set(track_index, axis,
          next ? 1.0f : 0.0f);
    }
    return;
  }
  if (track->value_type == LDK_FIELD_ENUM && field && field->enum_meta)
  {
    const LDKEnumMeta *meta = field->enum_meta;
    if (!meta->options || !meta->count || meta->count == UINT32_MAX)
    {
      ldk_ui_widget_label(ui, 0, "(no options)", rect);
      return;
    }
    u32 selected = meta->count;
    for (u32 i = 0; i < meta->count; ++i)
    {
      if (meta->options[i].value == value.integer)
      {
        selected = i;
        break;
      }
    }
    /* Match the Inspector: expose an unknown underlying enum value without
     * silently changing it to the first enumerator. */
    const char **labels = x_arena_alloc(ui->frame_arena,
        ((size_t)meta->count + 1u) * sizeof(*labels));
    if (!labels)
    {
      ldk_ui_widget_label(ui, 0, "(unavailable)", rect);
      return;
    }
    for (u32 i = 0; i < meta->count; ++i)
    {
      labels[i] = meta->options[i].label;
    }
    char unknown[64];
    snprintf(unknown, sizeof(unknown), "Unknown (%" PRId64 ")",
        value.integer);
    labels[meta->count] = unknown;
    u32 count = meta->count + (selected == meta->count ? 1u : 0u);
    u32 next = ldk_ui_widget_combo_box(ui, id, labels, count, selected, rect);
    if (next != selected && next < meta->count)
    {
      value.integer = meta->options[next].value;
      if (ldk_keyframe_animation_key_set(&state->clip,
              track_index, state->playhead, value))
      {
        state->selected_track = track_index;
        state->key_selected = false;
        state->dirty = true;
        s_editor_animation_set_time(state->playhead);
      }
    }
    return;
  }
  char text[X_SMALLSTR_MAX_LENGTH + 1];
  if (state->axis_edit_id == id)
  {
    snprintf(text, sizeof(text), "%s", state->axis_edit_text);
  }
  else if (track->value_type == LDK_FIELD_STRING)
  {
    snprintf(text, sizeof(text), "%s", value.string.buf);
  }
  else if (track->value_type == LDK_FIELD_I32 ||
      track->value_type == LDK_FIELD_U32 || track->value_type == LDK_FIELD_ENUM)
  {
    snprintf(text, sizeof(text), "%" PRId64, value.integer);
  }
  else
  {
    ldk_ui_format_float(text, (u32)sizeof(text),
        s_editor_animation_axis_value(track_index, axis));
  }
  u32 result = ldk_ui_widget_input_box(ui, id, text, sizeof(text), rect);
  bool focused = ui->focused_id == id;
  if (focused)
  {
    state->playing = false;
    state->axis_edit_id = id;
    snprintf(state->axis_edit_text, sizeof(state->axis_edit_text), "%s", text);
  }
  if (result & LDK_UI_INPUT_BOX_CANCELED)
  {
    state->axis_edit_id = 0;
  }
  else
  {
    if (result & LDK_UI_INPUT_BOX_CHANGED)
    {
      if (track->value_type == LDK_FIELD_STRING)
      {
        value.string.length = strlen(text);
        if (value.string.length <= X_SMALLSTR_MAX_LENGTH)
        {
          memcpy(value.string.buf, text, value.string.length + 1);
          if (ldk_keyframe_animation_key_set(&state->clip,
                  track_index, state->playhead, value))
          {
            state->dirty = true;
            s_editor_animation_set_time(state->playhead);
          }
        }
      }
      else if (track->value_type == LDK_FIELD_I32 ||
          track->value_type == LDK_FIELD_U32 || track->value_type == LDK_FIELD_ENUM)
      {
        char *end = NULL;
        errno = 0;
        long long number = strtoll(text, &end, 10);
        if (end != text && !errno && !*end &&
            !(track->value_type == LDK_FIELD_U32 && (number < 0 ||
                (unsigned long long)number > UINT32_MAX)) &&
            !(track->value_type == LDK_FIELD_I32 &&
                (number < INT32_MIN || number > INT32_MAX)))
        {
          value.integer = number;
          if (ldk_keyframe_animation_key_set(&state->clip,
                  track_index, state->playhead, value))
          {
            state->dirty = true;
            s_editor_animation_set_time(state->playhead);
          }
        }
      }
      else
      {
        float number;
        if (s_editor_animation_float_parse(text, &number))
          (void)s_editor_animation_axis_set(track_index, axis, number);
      }
    }
    if (!focused || (result & LDK_UI_INPUT_BOX_COMMITTED))
      state->axis_edit_id = 0;
  }
}

static u32 s_editor_animation_rows_build(LDKEditorAnimationRow *rows)
{
  LDKEditorAnimationState *state = &s_editor_animation;
  u32 count = 0;
  rows[count++] = (LDKEditorAnimationRow){-2, -1};
  for (u32 e = 0; e < state->clip.event_count; ++e)
  {
    rows[count++] = (LDKEditorAnimationRow){-3, (i32)e};
  }
  rows[count++] = (LDKEditorAnimationRow){-1, -1};
  for (u32 i = 0; i < state->clip.track_count; ++i)
  {
    const LDKKeyframeTrack *track = &state->clip.tracks[i];
    rows[count++] = (LDKEditorAnimationRow){(i32)i, -1};
    if (s_editor_animation_expanded(track))
    {
      for (u32 axis = 0; axis < s_editor_animation_axis_count(track); ++axis)
      {
        rows[count++] = (LDKEditorAnimationRow){(i32)i, (i32)axis};
      }
    }
  }
  return count;
}

/* Sibling names with identical hashes cannot be resolved by a relative path. */
static bool s_editor_animation_sibling_hash_ambiguous(LDKEntity child,
    LDKEntity first_sibling)
{
  u64 hash = ldk_ecs_entity_name_hash_get(child);
  if (!hash)
  {
    return true;
  }
  u32 matches = 0;
  for (LDKEntity cursor = first_sibling; !x_handle_is_null(cursor);)
  {
    const LDKTransform *transform = ldk_ecs_component_get_const(
        cursor, LDK_COMPONENT_TYPE_TRANSFORM);
    if (!transform)
    {
      break;
    }
    if (ldk_ecs_entity_name_hash_get(cursor) == hash && ++matches > 1)
    {
      return true;
    }
    cursor = transform->next_sibling;
  }
  return false;
}

static void s_editor_animation_add_property_tree(LDKUIContext *ui,
    LDKEntity entity, u64 path, u32 depth, bool ambiguous)
{
  if (depth > 128)
    return;
  const LDKTransform *transform = ldk_ecs_component_get_const(
      entity, LDK_COMPONENT_TYPE_TRANSFORM);
  if (!transform)
    return;
  LDKEditorAnimationState *state = &s_editor_animation;
  LDKEditorAnimationPopupNode *node = s_editor_animation_popup_node(
      entity, depth == 0);
  if (!node)
    return;
  bool entity_open = node->entity_expanded;
  ldk_ui_push_id_u32(ui, entity.index);
  ldk_ui_push_id_u32(ui, entity.version);
  const char *name = ldk_ecs_entity_name_get(entity);
  if (!name || !name[0])
    name = depth == 0 ? "Root" : "(unnamed)";
  u32 toggled = ldk_ui_tree_node_ex(ui, name, (LDKUIIcon){0},
      entity_open, depth, LDK_UI_TREE_NODE_NONE);
  if (toggled & LDK_UI_TREE_NODE_RESULT_CLICKED)
    entity_open = !entity_open;
  node = s_editor_animation_popup_node(entity, false);
  if (node)
    node->entity_expanded = entity_open;
  if (entity_open)
  {
    u32 count = ldk_ecs_entity_component_count(entity);
    for (u32 ci = 0; ci < count; ++ci)
    {
      u32 component_type;
      if (!ldk_ecs_entity_component_type_at(entity, ci, &component_type))
        continue;
      const LDKComponentMeta *meta =
          ldk_property_component_meta(component_type);
      if (!meta)
        continue;
      bool has_fields = false;
      for (u32 fi = 0; fi < meta->field_count; ++fi)
        has_fields |= !(meta->fields[fi].flags & LDK_FIELD_FLAG_READONLY);
      if (!has_fields)
        continue;
      LDKEditorAnimationPopupComponent *comp =
          s_editor_animation_popup_component(entity, component_type);
      if (!comp)
        continue;
      bool expanded = comp->expanded;
      ldk_ui_push_id_u32(ui, component_type);
      toggled = ldk_ui_tree_node_ex(ui, meta->name, (LDKUIIcon){0},
          expanded, depth + 1, LDK_UI_TREE_NODE_NONE);
      if (toggled & LDK_UI_TREE_NODE_RESULT_CLICKED)
        expanded = !expanded;
      comp = s_editor_animation_popup_component(entity, component_type);
      if (comp)
        comp->expanded = expanded;
      if (expanded)
      {
        for (u32 fi = 0; fi < meta->field_count; ++fi)
        {
          const LDKComponentFieldMeta *field = &meta->fields[fi];
          if (field->flags & LDK_FIELD_FLAG_READONLY)
            continue;
          bool supported = ldk_property_field_supported(field);
          bool exists = ldk_keyframe_animation_track_find(
              &state->clip, path, component_type, field->name) >= 0;
          ldk_ui_push_id_u32(ui, fi);
          ldk_ui_set_next_width(ui, ldk_ui_px(300.0f));
          ldk_ui_begin_disabled(ui, exists || ambiguous || !supported);
          char label[192];
          snprintf(label, sizeof(label), "%*s%s%s", (int)((depth + 2) * 2),
              "", field->display_name && field->display_name[0] ?
                  field->display_name : field->name,
              !supported ? " (unsupported type)" :
                  ambiguous ? " (duplicate name)" :
                      exists ? " (added)" : "");
          if (ldk_ui_button_flat(ui, label))
          {
            s_editor_animation_restore();
            i32 index = ldk_keyframe_animation_track_add(
                &state->clip, path, component_type, field->name);
            if (index >= 0)
            {
              state->selected_track = (u32)index;
              state->key_selected = false;
              state->dirty = true;
              s_editor_animation_expand(&state->clip.tracks[index], true);
            }
            ldk_ui_close_current_popup(ui);
          }
          ldk_ui_end_disabled(ui);
          ldk_ui_pop_id(ui);
        }
      }
      ldk_ui_pop_id(ui);
    }
    LDKEntity child = transform->first_child;
    while (!x_handle_is_null(child))
    {
      const LDKTransform *child_transform = ldk_ecs_component_get_const(
          child, LDK_COMPONENT_TYPE_TRANSFORM);
      if (!child_transform)
        break;
      LDKEntity next = child_transform->next_sibling;
      u64 name_hash = ldk_ecs_entity_name_hash_get(child);
      if (name_hash)
      {
        s_editor_animation_add_property_tree(ui, child,
            ldk_keyframe_path_child(path, name_hash), depth + 1,
            ambiguous || s_editor_animation_sibling_hash_ambiguous(
                child, transform->first_child));
      }
      child = next;
    }
  }
  ldk_ui_pop_id(ui);
  ldk_ui_pop_id(ui);
}

static void s_editor_animation_dopesheet(LDKEditorContext *editor)
{
  LDKUIContext *ui = &editor->ui;
  LDKEditorAnimationState *state = &s_editor_animation;
  s_editor_animation_view_clamp();
  u32 max_rows = 2 + state->clip.event_count + state->clip.track_count * 5;
  LDKEditorAnimationRow *rows = calloc(max_rows, sizeof(*rows));
  if (!rows)
  {
    return;
  }
  u32 row_count = s_editor_animation_rows_build(rows);
  const float row_h = LDK_EDITOR_ANIM_ROW_HEIGHT;
  const float ruler_h = LDK_EDITOR_ANIM_RULER_HEIGHT;
  /* The Add Property button belongs directly below the track rows. */
  const float content_h = ruler_h + row_h * row_count + 44.0f;

  ldk_ui_set_next_height(ui, ldk_ui_fill());
  ldk_ui_set_next_width(ui, ldk_ui_fill());
  /* Let the native scrollview handle vertical wheel scrolling across both
   * property labels and the time axis. Ctrl+wheel zooms the time axis. */
  const LDKMouseState *original_mouse = (LDKMouseState *)ui->mouse;
  LDKMouseState wheel_suppressed = {0};
  bool wheel_handled = false;
  if (original_mouse && ui->current_window &&
      ui->hovered_window_id == ui->current_window->id &&
      state->timeline_graph_rect.w > 0.0f)
  {
    LDKPoint cursor = ldk_os_mouse_cursor((LDKMouseState *)original_mouse);
    i32 wheel = ldk_os_mouse_wheel_delta((LDKMouseState *)original_mouse);
    if (wheel && ldk_rectf_contains(&state->timeline_graph_rect,
            (float)cursor.x, (float)cursor.y) &&
        ldk_rectf_contains(&state->timeline_visible_rect,
            (float)cursor.x, (float)cursor.y))
    {
      float steps = s_editor_animation_clamp((float)wheel / 120.0f,
          -8.0f, 8.0f);
      bool zoom = ui->keyboard && ldk_os_keyboard_key_is_pressed(
          (LDKKeyboardState *)ui->keyboard, LDK_KEYCODE_CONTROL);
      if (zoom)
      {
        float anchor = s_editor_animation_clamp(
            ((float)cursor.x - state->timeline_graph_rect.x) /
            state->timeline_graph_rect.w, 0.0f, 1.0f);
        s_editor_animation_wheel_zoom(steps, anchor);
        wheel_handled = true;
      }
    }
  }
  if (wheel_handled)
  {
    wheel_suppressed = *original_mouse;
    wheel_suppressed.wheel_delta = 0;
    ui->mouse = &wheel_suppressed;
  }
  state->sheet_scroll = ldk_ui_begin_scrollview(ui, state->sheet_scroll,
      LDK_UI_SCROLL_VERTICAL | LDK_UI_SCROLL_IF_NEEDED);
  ui->mouse = original_mouse;
  ldk_ui_set_padding(ui, 0);

  /* Use the same horizontal layout and resize handle as the other editor
   * panels. The Dopesheet's custom drawing only owns its cells and ruler. */
  ldk_ui_set_next_width(ui, ldk_ui_fill());
  ldk_ui_set_next_height(ui, ldk_ui_px(content_h));
  ldk_ui_begin_horizontal(ui);
  ldk_ui_set_padding(ui, 0);
  float available_width = ui->current_layout->content_rect.w;
  float min_label_width = fminf(225.0f, available_width * 0.6f);
  float max_label_width = fmaxf(80.0f, available_width - 90.0f);
  if (state->property_width <= 0.0f)
  {
    state->property_width = 420.0f;
  }
  state->property_width = s_editor_animation_clamp(state->property_width,
      min_label_width, max_label_width);

  ldk_ui_set_next_width(ui, ldk_ui_px(state->property_width));
  ldk_ui_set_next_height(ui, ldk_ui_px(content_h));
  ldk_ui_spacer(ui);
  LDKUIRect labels = ldk_ui_last_rect(ui);

  ldk_ui_set_next_width(ui, ldk_ui_px(8.0f));
  ldk_ui_set_next_height(ui, ldk_ui_px(content_h));
  state->property_width = ldk_ui_resize_handle_vertical(ui,
      state->property_width, min_label_width, max_label_width);
  LDKUIRect divider = ldk_ui_last_rect(ui);

  ldk_ui_set_next_width(ui, ldk_ui_fill());
  ldk_ui_set_next_height(ui, ldk_ui_px(content_h));
  ldk_ui_spacer(ui);
  LDKUIRect graph = ldk_ui_last_rect(ui);
  ldk_ui_end_horizontal(ui);

  LDKUIRect sheet = {labels.x, labels.y,
      graph.x + graph.w - labels.x, content_h};
  LDKUIRect old_clip = ui->clip_rect;
  /* Use one time-axis rectangle for ticks, keys, events, mouse hit tests and
   * dragging. The resize handle and UI spacing are excluded by taking the
   * actual graph's origin, never the label width as a pixel offset. Preserve
   * the existing left inset and reserve extra room after the final key. */
  LDKUIRect time_graph = graph;
  float time_left = fmaxf(graph.x, divider.x + divider.w) + 5.0f;
  float time_right = fminf(graph.x + graph.w, old_clip.x + old_clip.w) - 16.0f;
  time_graph.x = time_left;
  time_graph.w = fmaxf(1.0f, time_right - time_left);
  state->timeline_sheet_rect = sheet;
  state->timeline_graph_rect = time_graph;
  state->timeline_visible_rect = ldk_rectf_intersect(&old_clip, &sheet);
  ui->clip_rect = ldk_rectf_intersect(&old_clip, &sheet);
  float label_width = labels.w;
  float first_row = sheet.y + ruler_h;

  rgba32 bg = ui->theme.colors[LDK_UI_COLOR_PANEL_BG];
  rgba32 alternate = ui->theme.colors[LDK_UI_COLOR_CONTROL_BG];
  rgba32 focus = ui->theme.colors[LDK_UI_COLOR_FOCUS];
  rgba32 key_color = ui->theme.colors[LDK_UI_COLOR_SLIDER_THUMB];
  rgba32 grid = ui->theme.colors[LDK_UI_COLOR_SEPARATOR];
  s_editor_animation_draw_rect(ui, labels, bg);
  s_editor_animation_draw_rect(ui, graph, bg);
  s_editor_animation_draw_rect(ui,
      (LDKUIRect){labels.x, labels.y, labels.w, ruler_h}, alternate);
  s_editor_animation_draw_rect(ui,
      (LDKUIRect){graph.x, graph.y, graph.w, ruler_h}, alternate);
  ldk_ui_widget_label(ui, 0, "Properties",
      (LDKUIRect){sheet.x + 6, sheet.y + 2, label_width - 8, ruler_h - 4});

  float span = s_editor_animation_span();
  float desired = span * 80.0f / time_graph.w;
  float scale = powf(10.0f, floorf(log10f(fmaxf(desired, 0.0001f))));
  float step = desired <= scale ? scale :
      desired <= 2.0f * scale ? 2.0f * scale :
      desired <= 5.0f * scale ? 5.0f * scale : 10.0f * scale;
  float first_tick = ceilf(state->timeline_offset / step) * step;
  for (u32 tick = 0; tick < 128; ++tick)
  {
    float time = first_tick + tick * step;
    if (time > state->timeline_offset + span + step * 0.01f)
    {
      break;
    }
    float x = s_editor_animation_time_x(time_graph, state, time);
    s_editor_animation_draw_rect(ui,
        (LDKUIRect){x, sheet.y + ruler_h, 1, content_h - ruler_h}, grid);
    char time_label[32];
    snprintf(time_label, sizeof(time_label), "%.2fs", time);
    ldk_ui_widget_label(ui, 0, time_label,
        (LDKUIRect){x + 2, sheet.y + 1, 72, ruler_h - 2});
  }

  float delete_button_x = sheet.x + label_width - 26.0f;
  float input_x = delete_button_x - 83.0f;
  bool label_control_pressed = false;
  i32 remove_track_request = -1;
  LDKUIIcon delete_icon = {0};
  delete_icon.size = ldk_sizef(16.0f, 16.0f);
  delete_icon.texture = ldk_renderer_texture_ui_handle(
      editor->renderer, editor->ui_atlas);
  delete_icon.color = ui->theme.colors[LDK_UI_COLOR_CONTROL_TEXT];
  delete_icon.uv = ldk_editor_icon_rects[LDK_EDITOR_ICON_DELETE];
  bool add_property_request = false;
  for (u32 row = 0; row < row_count; ++row)
  {
    float y = first_row + row * row_h;
    LDKEditorAnimationRow item = rows[row];
    if (row & 1)
    {
      s_editor_animation_draw_rect(ui,
          (LDKUIRect){labels.x, y, labels.w, row_h}, alternate);
      s_editor_animation_draw_rect(ui,
          (LDKUIRect){graph.x, y, graph.w, row_h}, alternate);
    }
    if (item.track >= 0 && (u32)item.track == state->selected_track)
    {
      s_editor_animation_draw_rect(ui,
          (LDKUIRect){sheet.x, y, label_width, row_h},
          ui->theme.colors[LDK_UI_COLOR_CONTROL_BG_ACTIVE]);
    }
    s_editor_animation_draw_rect(ui,
        (LDKUIRect){labels.x, y + row_h - 1, labels.w, 1}, grid);
    s_editor_animation_draw_rect(ui,
        (LDKUIRect){graph.x, y + row_h - 1, graph.w, 1}, grid);
    LDKUIRect label_clip = ui->clip_rect;
    ui->clip_rect = ldk_rectf_intersect(&label_clip,
        &(LDKUIRect){sheet.x, y, label_width, row_h});

    if (item.track < 0)
    {
      char label[64];
      if (item.track == -2)
      {
        snprintf(label, sizeof(label), "Events (%u)", state->clip.event_count);
      }
      else if (item.track == -3)
      {
        const LDKKeyframeEvent *event = &state->clip.events[item.axis];
        if (event->type == LDK_KEYFRAME_EVENT_INTEGER)
        {
          snprintf(label, sizeof(label), "  int event: %d", event->number);
        }
        else
        {
          snprintf(label, sizeof(label), "  str event: %.42s",
              event->text ? event->text : "");
        }
      }
      else
      {
        snprintf(label, sizeof(label), "All keyframes");
      }
      ldk_ui_widget_label(ui, 0, label,
          (LDKUIRect){sheet.x + 7, y + 1, label_width - 10, row_h - 2});
    }
    else
    {
      u32 t = (u32)item.track;
      const LDKKeyframeTrack *track = &state->clip.tracks[t];
      if (item.axis == -1)
      {
        bool expanded = s_editor_animation_expanded(track);
        /* Explicit rectangles keep the left panel aligned with graph rows. */
        LDKUIId button_id = 0x414E2000u + t;
        if (ldk_ui_widget_button_flat(ui, button_id,
                expanded ? "v" : ">",
                (LDKUIRect){sheet.x + 3, y + 2, 18, row_h - 4}))
        {
          s_editor_animation_expand(track, !expanded);
          label_control_pressed = true;
        }
        char label[256];
        s_editor_animation_track_label(state, t, label, sizeof(label));
        const LDKComponentFieldMeta *property = ldk_property_field_find(
            track->component_type, track->property_name);
        bool color_track = track->value_type == LDK_FIELD_U32 &&
            property && property->widget == LDK_FIELD_WIDGET_COLOR;
        ldk_ui_widget_label(ui, 0, label,
            (LDKUIRect){sheet.x + 25, y + 1,
                fmaxf(8.0f, label_width - (color_track ? 94.0f : 60.0f)),
                row_h - 2});
        if (color_track)
        {
          LDKPropertyValue color_value;
          if (s_editor_animation_track_value(t, &color_value))
          {
            rgba32 color = (rgba32)(u32)color_value.integer;
            /* Stable, track-specific ID: the native picker owns the popup. */
            u32 color_id = 0xC0120001u ^ (u32)track->target_path ^
                (u32)(track->target_path >> 32) ^
                (track->component_type * 16777619u);
            for (const char *name = track->property_name; *name; ++name)
              color_id = (color_id ^ (u8)*name) * 16777619u;
            if (!color_id) color_id = 0xC0120001u;
            if (ldki_editor_color_swatch_widget(editor, color_id, &color,
                    false, (LDKUIRect){delete_button_x - 27.0f, y + 3,
                        21.0f, row_h - 6.0f}))
            {
              color_value.integer = (u32)color;
              if (ldk_keyframe_animation_key_set(&state->clip,
                      t, state->playhead, color_value))
              {
                state->playing = false;
                state->selected_track = t;
                state->key_selected = false;
                state->dirty = true;
                s_editor_animation_set_time(state->playhead);
              }
            }
          }
        }
        LDKUIId delete_id = 0x414E4000u + t;
        if (ldk_ui_widget_icon_button(ui, delete_id, delete_icon, "",
                (LDKUIRect){delete_button_x, y + 2, 22, row_h - 4}))
        {
          remove_track_request = (i32)t;
          label_control_pressed = true;
        }
      }
      else
      {
        char label[56];
        static const char *const axes[] = {"X", "Y", "Z", "W"};
        const LDKComponentFieldMeta *field = ldk_property_field_find(
            track->component_type, track->property_name);
        const bool is_color = track->value_type == LDK_FIELD_U32 &&
            field && field->widget == LDK_FIELD_WIDGET_COLOR;
        const char *channel = s_editor_animation_axis_count(track) == 1 ?
            s_editor_animation_property_label(track) :
            is_color ? (const char *const[]){"R", "G", "B", "A"}[item.axis] :
            axes[item.axis];
        snprintf(label, sizeof(label), "%s", channel);
        ldk_ui_widget_label(ui, 0, label,
            (LDKUIRect){sheet.x + 27, y + 1,
                fmaxf(8.0f, input_x - sheet.x - 29.0f), row_h - 2});
        if (input_x > sheet.x + 30.0f)
        {
          s_editor_animation_axis_input(ui, t, (u32)item.axis,
              (LDKUIRect){input_x, y + 2, 78, row_h - 4});
        }
      }
    }
    ui->clip_rect = label_clip;

    if (item.track == -2 || item.track == -3)
    {
      u32 begin = item.track == -3 ? (u32)item.axis : 0;
      u32 end = item.track == -3 ? begin + 1 : state->clip.event_count;
      for (u32 i = begin; i < end; ++i)
      {
        float x = s_editor_animation_time_x(
            time_graph, state, state->clip.events[i].time);
        if (x >= time_graph.x - 4 && x <= time_graph.x + time_graph.w + 4)
        {
          rgba32 event_color = state->selected_event == (i32)i ? key_color : focus;
          s_editor_animation_draw_rect(ui,
              (LDKUIRect){x - 2, y + 4, 5, row_h - 8}, event_color);
        }
      }
    }
    else
    {
      u32 begin = item.track == -1 ? 0 : (u32)item.track;
      u32 end = item.track == -1 ? state->clip.track_count : begin + 1;
      for (u32 t = begin; t < end; ++t)
      {
        const LDKKeyframeTrack *track = &state->clip.tracks[t];
        for (u32 k = 0; k < track->count; ++k)
        {
          float x = s_editor_animation_time_x(
              time_graph, state, track->keys[k].time);
          if (x < time_graph.x - 5 || x > time_graph.x + time_graph.w + 5)
          {
            continue;
          }
          bool selected = item.track >= 0 && state->key_selected &&
              state->selected_track == t && state->selected_key == k;
          if (state->dragging_key && selected)
          {
            LDKPoint pointer = ldk_os_mouse_cursor(
                (LDKMouseState *)ui->mouse);
            x += (float)pointer.x - state->drag_start_x;
          }
          s_editor_animation_draw_rect(ui,
              (LDKUIRect){x - 4, y + row_h * 0.5f - 4, 9, 9},
              selected ? focus : key_color);
        }
      }
    }
  }
  /* A single Add Property control, immediately below the last track. */
  float add_property_width = fminf(310.0f, fmaxf(1.0f, labels.w - 16.0f));
  LDKUIRect add_property_rect = {
      labels.x + (labels.w - add_property_width) * 0.5f,
      first_row + row_count * row_h + 7.0f,
      add_property_width, 28.0f};
  if (ldk_ui_widget_button(ui, 0x414E4150u,
          "Add Property", add_property_rect))
  {
    add_property_request = true;
  }

  float cursor_x = s_editor_animation_time_x(time_graph, state, state->playhead);
  if (cursor_x >= time_graph.x && cursor_x <= time_graph.x + time_graph.w)
  {
    s_editor_animation_draw_rect(ui,
        (LDKUIRect){cursor_x, sheet.y, 2, content_h}, focus);
    s_editor_animation_draw_rect(ui,
        (LDKUIRect){cursor_x - 4, sheet.y, 10, 7}, focus);
  }
  ui->clip_rect = old_clip;
  ldk_ui_end_scrollview(ui);
  if (add_property_request)
  {
    ldk_ui_open_popup_at(ui, LDK_EDITOR_ANIM_ADD_PROPERTY_POPUP_ID,
        (LDKUIPoint){add_property_rect.x,
            add_property_rect.y + add_property_rect.h});
  }

  if (remove_track_request >= 0)
  {
    (void)s_editor_animation_remove_track_confirm(
        editor, (u32)remove_track_request);
    free(rows);
    return;
  }

  LDKMouseState *mouse = (LDKMouseState *)ui->mouse;
  if (!mouse)
  {
    free(rows);
    return;
  }
  LDKPoint pointer = ldk_os_mouse_cursor(mouse);
  float mx = (float)pointer.x;
  float my = (float)pointer.y;
  LDKUIRect visible = ldk_rectf_intersect(&old_clip, &sheet);
  bool hovered = ui->current_window &&
      ui->hovered_window_id == ui->current_window->id &&
      ldk_rectf_contains(&visible, mx, my);
  bool down = ldk_os_mouse_button_down(mouse, LDK_MOUSE_BUTTON_LEFT);
  bool held = ldk_os_mouse_button_is_pressed(mouse, LDK_MOUSE_BUTTON_LEFT);
  bool up = ldk_os_mouse_button_up(mouse, LDK_MOUSE_BUTTON_LEFT);

  bool finishing_key_drag = state->dragging_key && up;
  if (finishing_key_drag)
  {
    float new_time = state->drag_start_time +
        (mx - state->drag_start_x) * span / time_graph.w;
    s_editor_animation_move_key(new_time);
    state->dragging_key = false;
  }
  if (state->scrubbing)
  {
    if (held)
    {
      s_editor_animation_set_time(
          s_editor_animation_x_time(time_graph, state, mx));
    }
    else
    {
      state->scrubbing = false;
    }
  }
  /* Record clicks on release, like the other editor controls. A click in a
   * keyframe row is not a scrub gesture; two releases in the same cell add
   * one key without relying on the mouse-down/drag state between them. */
  if (hovered && up && !finishing_key_drag &&
      mx >= time_graph.x && mx <= time_graph.x + time_graph.w && my >= first_row)
  {
    u32 released_row = (u32)((my - first_row) / row_h);
    if (released_row < row_count && rows[released_row].track >= 0)
    {
      LDKEditorAnimationRow cell = rows[released_row];
      const LDKKeyframeTrack *track = &state->clip.tracks[cell.track];
      bool existing_key = false;
      for (u32 k = 0; k < track->count; ++k)
      {
        float x = s_editor_animation_time_x(
            time_graph, state, track->keys[k].time);
        if (fabsf(x - mx) < 7.0f)
        {
          existing_key = true;
          break;
        }
      }
      if (!existing_key)
      {
        u64 now = ldk_os_time_ticks_get();
        bool double_click = state->last_sheet_click_ticks != 0 &&
            state->last_sheet_click_track == cell.track &&
            state->last_sheet_click_axis == cell.axis &&
            fabsf(state->last_sheet_click_x - mx) <= 7.0f &&
            fabsf(state->last_sheet_click_y - my) <= 7.0f &&
            ldk_os_time_ticks_interval_get_seconds(
                state->last_sheet_click_ticks, now) <= 0.35;
        state->last_sheet_click_ticks = double_click ? 0 : now;
        state->last_sheet_click_track = cell.track;
        state->last_sheet_click_axis = cell.axis;
        state->last_sheet_click_x = mx;
        state->last_sheet_click_y = my;
        if (double_click)
        {
          float key_time = s_editor_animation_x_time(time_graph, state, mx);
          key_time = floorf(key_time * 100.0f + 0.5f) / 100.0f;
          state->playing = false;
          s_editor_animation_set_time(key_time);
          if (s_editor_animation_key_at_cursor((u32)cell.track))
          {
            const LDKKeyframeTrack *updated = &state->clip.tracks[cell.track];
            for (u32 k = 0; k < updated->count; ++k)
            {
              if (fabsf(updated->keys[k].time - key_time) < 0.0001f)
              {
                state->selected_key = k;
                state->key_selected = true;
                break;
              }
            }
          }
        }
      }
      else
      {
        state->last_sheet_click_ticks = 0;
      }
    }
  }
  if (!hovered || !down || label_control_pressed)
  {
    free(rows);
    return;
  }
  if (my < first_row || mx < time_graph.x)
  {
    if (mx >= time_graph.x)
    {
      state->last_sheet_click_ticks = 0;
      state->playing = false;
      state->scrubbing = true;
      s_editor_animation_set_time(
          s_editor_animation_x_time(time_graph, state, mx));
    }
    else if (my >= first_row)
    {
      u32 row = (u32)((my - first_row) / row_h);
      if (row < row_count && rows[row].track >= 0)
      {
        /* The input and key controls handle their own clicks. */
        if (rows[row].axis < 0 || mx < input_x - 3)
        {
          state->selected_track = (u32)rows[row].track;
          state->key_selected = false;
        }
      }
    }
    free(rows);
    return;
  }
  u32 row = (u32)((my - first_row) / row_h);
  if (row >= row_count)
  {
    free(rows);
    return;
  }
  LDKEditorAnimationRow item = rows[row];
  state->scrubbing = false;
  if (item.track == -2 || item.track == -3)
  {
    state->selected_event = -1;
    u32 begin = item.track == -3 ? (u32)item.axis : 0;
    u32 end = item.track == -3 ? begin + 1 : state->clip.event_count;
    for (u32 i = begin; i < end; ++i)
    {
      float x = s_editor_animation_time_x(
          time_graph, state, state->clip.events[i].time);
      if (fabsf(x - mx) < 7.0f)
      {
        state->selected_event = (i32)i;
        break;
      }
    }
    state->playing = false;
    if (state->selected_event >= 0)
    {
      s_editor_animation_set_time(
          state->clip.events[state->selected_event].time);
      free(rows);
      return;
    }
  }
  else
  {
    u32 begin = item.track == -1 ? 0 : (u32)item.track;
    u32 end = item.track == -1 ? state->clip.track_count : begin + 1;
    for (u32 t = begin; t < end; ++t)
    {
      const LDKKeyframeTrack *track = &state->clip.tracks[t];
      for (u32 k = 0; k < track->count; ++k)
      {
        float x = s_editor_animation_time_x(
            time_graph, state, track->keys[k].time);
        if (fabsf(x - mx) < 7.0f)
        {
          state->last_sheet_click_ticks = 0;
          state->selected_track = t;
          state->selected_key = k;
          state->key_selected = true;
          state->playing = false;
          state->dragging_key = item.track >= 0;
          state->drag_start_x = mx;
          state->drag_start_time = track->keys[k].time;
          s_editor_animation_set_time(track->keys[k].time);
          free(rows);
          return;
        }
      }
    }
    if (item.track >= 0)
    {
      state->selected_track = (u32)item.track;
      state->key_selected = false;
    }
    else
    {
      state->last_sheet_click_ticks = 0;
    }
  }
  /* Empty cells select a time but do not start a drag/scrub gesture:
   * the next mouse-down must remain eligible for a double click. */
  state->playing = false;
  state->scrubbing = false;
  s_editor_animation_set_time(s_editor_animation_x_time(time_graph, state, mx));
  free(rows);
}

static void s_editor_animation_window(LDKEditor *opaque, void *data)
{
  (void)data;
  LDKEditorContext *editor = (LDKEditorContext *)opaque;
  LDKEditorAnimationState *state = &s_editor_animation;
  LDKUIContext *ui = &editor->ui;
  LDKECS *ecs = (LDKECS *)ldk_module_get(LDK_MODULE_ECS);
  LDKEntity selected = x_handle_null();
  char description[144];
  bool has_selection = ecs &&
      ldki_editor_selected_entity_get(editor, ecs, &selected);
  if (!state->initialized)
  {
    ldk_keyframe_animation_init(&state->clip);
    state->root = x_handle_null();
    state->source_animation_index = -1;
    state->looping = true;
    state->timeline_zoom = 1.0f;
    s_editor_animation_reset_selection();
    state->initialized = true;
  }
  if (editor->editor_state != LDK_EDITOR_STATE_STOPED && state->preview)
  {
    s_editor_animation_restore();
  }

  /* A child can be selected to add tracks; the nearest source is the root. */
  LDKEntity root = x_handle_null();
  if (has_selection)
  {
    LDKEntity cursor = selected;
    for (u32 depth = 0; depth < 128 && !x_handle_is_null(cursor); ++depth)
    {
      if (ldk_ecs_component_get_const(cursor,
              LDK_COMPONENT_TYPE_KEYFRAME_ANIMATION_SOURCE))
      {
        root = cursor;
        break;
      }
      const LDKTransform *transform = ldk_ecs_component_get_const(
          cursor, LDK_COMPONENT_TYPE_TRANSFORM);
      if (!transform)
      {
        break;
      }
      cursor = transform->parent;
    }
  }
  const LDKKeyFrameAnimationSource *source =
      !x_handle_is_null(root) ? ldk_ecs_component_get_const(
          root, LDK_COMPONENT_TYPE_KEYFRAME_ANIMATION_SOURCE) : NULL;
  LDKAssetManager *assets = ldk_module_get(LDK_MODULE_ASSET_MANAGER);
  const LDKAssetInfo *active_info = NULL;
  if (source && source->current_animation >= 0 &&
      (u32)source->current_animation < source->animation_count && assets)
  {
    LDKAssetHandle handle = {
        .h = source->animations[source->current_animation].h};
    active_info = ldk_asset_get_info_const(assets, handle);
    if (active_info && active_info->type != LDK_ASSET_TYPE_KEYFRAME_ANIMATION)
    {
      active_info = NULL;
    }
  }
  const char *path = active_info ? active_info->asset_path.buf : "";
  i32 index = active_info ? source->current_animation : -1;
  bool changed = root.index != state->root.index ||
      root.version != state->root.version ||
      state->source_animation_index != index ||
      strcmp(state->path, path) != 0;
  if (changed)
  {
    /* Changing the entity or clip must never silently discard edit work. */
    if (state->dirty && state->path[0] &&
        editor->editor_state == LDK_EDITOR_STATE_STOPED)
    {
      if (!ldk_keyframe_animation_save(&state->clip, state->path) ||
          (assets && !ldk_asset_manager_keyframe_animation_reload(
              assets, state->path)))
      {
        ldki_editor_log_error(editor, "Save the current animation before switching.");
        return;
      }
      state->dirty = false;
    }
    s_editor_animation_restore();
    s_editor_animation_ui_state_clear();
    ldk_keyframe_animation_clear(&state->clip);
    state->root = root;
    state->source_animation_index = index;
    snprintf(state->path, sizeof(state->path), "%s", path);
    state->playhead = 0.0f;
    state->duration_text_editing = false;
    state->duration_text[0] = '\0';
    s_editor_animation_reset_selection();
    state->dirty = false;
    if (path[0] && !ldk_keyframe_animation_load(&state->clip, path))
    {
      ldki_editor_log_error(editor, "Unable to load selected animation.");
      state->path[0] = 0;
      state->source_animation_index = -1;
    }
  }

  /* Delete affects keyframes only while the Animation window owns focus. */
  if (editor->editor_state == LDK_EDITOR_STATE_STOPED &&
      ldki_editor_window_is_focused(editor, LDK_EDITOR_WINDOW_ANIMATION) &&
      ui->keyboard && ui->input_box_id == 0 &&
      state->axis_edit_id == 0 &&
      ldk_os_keyboard_key_down((LDKKeyboardState *)ui->keyboard,
          LDK_KEYCODE_DELETE))
  {
    (void)s_editor_animation_delete_selected_key();
  }

  /* Compact transport bar; the Dopesheet owns most of the window. */
  ldk_ui_set_next_height(ui, ldk_ui_px(29.0f));
  ldk_ui_begin_horizontal(ui);
  ldk_ui_set_padding(ui, 0.0f);
  ldk_ui_set_next_width(ui, ldk_ui_px(64.0f));
  ldk_ui_label(ui, "Preview");
  ldk_ui_set_next_width(ui, ldk_ui_px(36.0f));
  if (ldk_ui_button_flat(ui, "|<"))
  {
    state->playing = false;
    s_editor_animation_set_time(0.0f);
  }
  ldk_ui_set_next_width(ui, ldk_ui_px(36.0f));
  if (ldk_ui_button_flat(ui, "<"))
  {
    state->playing = false;
    float previous = 0.0f;
    for (u32 t = 0; t < state->clip.track_count; ++t)
    {
      const LDKKeyframeTrack *track = &state->clip.tracks[t];
      for (u32 k = 0; k < track->count; ++k)
      {
        float time = track->keys[k].time;
        if (time < state->playhead - 0.0001f && time > previous)
        {
          previous = time;
        }
      }
    }
    s_editor_animation_set_time(previous);
  }
  ldk_ui_set_next_width(ui, ldk_ui_px(55.0f));
  if (ldk_ui_button_flat(ui, state->playing ? "Pause" : "Play"))
  {
    if (state->playing)
    {
      state->playing = false;
    }
    else if (!x_handle_is_null(state->root) &&
        s_editor_animation_preview_begin())
    {
      if (state->playhead >= state->clip.duration)
      {
        state->playhead = 0.0f;
      }
      s_editor_animation_set_time(state->playhead);
      state->playing = true;
    }
  }
  ldk_ui_set_next_width(ui, ldk_ui_px(46.0f));
  if (ldk_ui_button_flat(ui, "Stop"))
  {
    s_editor_animation_restore();
    state->playhead = 0.0f;
  }
  ldk_ui_set_next_width(ui, ldk_ui_px(36.0f));
  if (ldk_ui_button_flat(ui, ">"))
  {
    state->playing = false;
    float next = state->clip.duration;
    for (u32 t = 0; t < state->clip.track_count; ++t)
    {
      const LDKKeyframeTrack *track = &state->clip.tracks[t];
      for (u32 k = 0; k < track->count; ++k)
      {
        float time = track->keys[k].time;
        if (time > state->playhead + 0.0001f && time < next)
        {
          next = time;
        }
      }
    }
    s_editor_animation_set_time(next);
  }
  ldk_ui_set_next_width(ui, ldk_ui_px(36.0f));
  if (ldk_ui_button_flat(ui, ">|"))
  {
    state->playing = false;
    s_editor_animation_set_time(state->clip.duration);
  }
  snprintf(description, sizeof(description), "%.2fs / %.2fs%s",
      state->playhead, state->clip.duration, state->dirty ? " *" : "");
  ldk_ui_set_next_width(ui, ldk_ui_px(112.0f));
  ldk_ui_label(ui, description);
  ldk_ui_set_next_weight(ui, 1.0f);
  ldk_ui_spacer(ui);
  ldk_ui_set_next_width(ui, ldk_ui_px(64.0f));
  if (ldk_ui_button(ui, "Save") && state->path[0])
  {
    if (ldk_keyframe_animation_save(&state->clip, state->path))
    {
      if (assets && !ldk_asset_manager_keyframe_animation_reload(
              assets, state->path))
      {
        ldki_editor_log_error(editor,
            "Saved .anim but could not refresh its loaded asset.");
      }
      state->dirty = false;
    }
    else
    {
      ldki_editor_log_error(editor, "Unable to save .anim asset.");
    }
  }
  ldk_ui_end_horizontal(ui);

  /* The second toolbar keeps the clip picker and frequent edits visible. */
  ldk_ui_set_next_height(ui, ldk_ui_px(29.0f));
  ldk_ui_begin_horizontal(ui);
  ldk_ui_set_padding(ui, 0.0f);
  if (source && source->animation_count)
  {
    const char **items = calloc(source->animation_count, sizeof(*items));
    if (items)
    {
      for (u32 i = 0; i < source->animation_count; ++i)
      {
        LDKAssetHandle handle = {.h = source->animations[i].h};
        const LDKAssetInfo *info = assets
            ? ldk_asset_get_info_const(assets, handle) : NULL;
        items[i] = info ? info->asset_path.buf : "(missing)";
      }
      u32 current = source->current_animation >= 0 &&
          (u32)source->current_animation < source->animation_count
          ? (u32)source->current_animation : 0u;
      ldk_ui_set_next_width(ui, ldk_ui_px(240.0f));
      u32 next = ldk_ui_combo_box(ui, items, source->animation_count, current);
      if (next != current && editor->editor_state == LDK_EDITOR_STATE_STOPED)
      {
        if (state->dirty && state->path[0] &&
            (!ldk_keyframe_animation_save(&state->clip, state->path) ||
             (assets && !ldk_asset_manager_keyframe_animation_reload(
                 assets, state->path))))
        {
          ldki_editor_log_error(editor,
              "Unable to save animation before switching.");
        }
        else
        {
          state->dirty = false;
          s_editor_animation_restore();
          (void)ldk_keyframe_animation_source_set_current(root, (i32)next);
        }
      }
      free(items);
    }
  }
  else
  {
    ldk_ui_set_next_width(ui, ldk_ui_px(240.0f));
    ldk_ui_label(ui, "(no animation selected)");
  }
  if (!active_info || !state->path[0])
  {
    ldk_ui_end_horizontal(ui);
    ldk_ui_label(ui,
        "Select an entity with an Animation Source and an assigned .anim asset.");
    return;
  }
  if (editor->editor_state != LDK_EDITOR_STATE_STOPED)
  {
    ldk_ui_end_horizontal(ui);
    ldk_ui_label(ui, "Stop the game to preview or edit animation.");
    return;
  }
  ldk_ui_set_next_width(ui, ldk_ui_px(37.0f));
  ldk_ui_label(ui, "Loop");
  ldk_ui_set_next_width(ui, ldk_ui_px(32.0f));
  state->looping = ldk_ui_toggle(ui, state->looping);
  ldk_ui_set_next_width(ui, ldk_ui_px(63.0f));
  ldk_ui_label(ui, "Duration");
  ldk_ui_set_next_width(ui, ldk_ui_px(78.0f));
  ldk_ui_spacer(ui);
  LDKUIRect duration_rect = ldk_ui_last_rect(ui);
  const LDKUIId duration_id = 0x414E4455u;
  if (!state->duration_text_editing)
  {
    snprintf(state->duration_text, sizeof(state->duration_text), "%.4f",
        (double)state->clip.duration);
  }
  u32 duration_result = ldk_ui_widget_input_box(ui, duration_id,
      state->duration_text, sizeof(state->duration_text), duration_rect);
  bool duration_focused = ui->focused_id == duration_id;
  if (duration_focused)
  {
    state->duration_text_editing = true;
  }
  if ((duration_result & LDK_UI_INPUT_BOX_CANCELED) != 0)
  {
    state->duration_text_editing = false;
  }
  else if (state->duration_text_editing &&
      ((duration_result & LDK_UI_INPUT_BOX_COMMITTED) != 0 ||
       !duration_focused))
  {
    float duration = 0.0f;
    if (s_editor_animation_float_parse(state->duration_text, &duration) &&
        duration >= 0.1f && duration != state->clip.duration)
    {
      state->clip.duration = duration;
      state->playhead = fminf(state->playhead, duration);
      state->dirty = true;
      s_editor_animation_view_clamp();
      if (state->preview)
      {
        s_editor_animation_set_time(state->playhead);
      }
    }
    state->duration_text_editing = false;
  }
  ldk_ui_set_next_width(ui, ldk_ui_px(36.0f));
  ldk_ui_label(ui, "Time");
  ldk_ui_set_next_width(ui, ldk_ui_px(104.0f));
  float slide_min = 0.1f;
  float slide_max = fmaxf(state->clip.duration, slide_min);
  float slider_time = ldk_ui_slider(ui,
      s_editor_animation_clamp(state->playhead, slide_min, slide_max),
      slide_min, slide_max);
  if (fabsf(slider_time - s_editor_animation_clamp(state->playhead,
          slide_min, slide_max)) > 0.0001f)
  {
    state->playing = false;
    s_editor_animation_set_time(slider_time);
  }
  ldk_ui_set_next_width(ui, ldk_ui_px(56.0f));
  if (ldk_ui_button_flat(ui, "Edit..."))
  {
    ldk_ui_open_popup(ui, LDK_EDITOR_ANIM_EDIT_POPUP_ID);
  }
  ldk_ui_set_next_width(ui, ldk_ui_px(70.0f));
  if (ldk_ui_button_flat(ui, "Events..."))
  {
    ldk_ui_open_popup(ui, LDK_EDITOR_ANIM_EVENTS_POPUP_ID);
  }
  ldk_ui_set_next_width(ui, ldk_ui_px(58.0f));
  if (ldk_ui_button_flat(ui, "View..."))
  {
    ldk_ui_open_popup(ui, LDK_EDITOR_ANIM_VIEW_POPUP_ID);
  }
  ldk_ui_end_horizontal(ui);

  s_editor_animation_dopesheet(editor);

  if (ldk_ui_begin_popup(ui, LDK_EDITOR_ANIM_ADD_PROPERTY_POPUP_ID))
  {
    if (!x_handle_is_null(state->root) &&
        ldk_ecs_component_get_const(state->root, LDK_COMPONENT_TYPE_TRANSFORM))
    {
      ldk_ui_set_next_width(ui, ldk_ui_px(280.0f));
      ldk_ui_label(ui, "Choose a component property");
      ldk_ui_set_next_width(ui, ldk_ui_px(295.0f));
      ldk_ui_set_next_height(ui, ldk_ui_px(320.0f));
      state->property_popup_scroll = ldk_ui_begin_scrollview(ui,
          state->property_popup_scroll,
          LDK_UI_SCROLL_VERTICAL | LDK_UI_SCROLL_IF_NEEDED);
      s_editor_animation_add_property_tree(ui, state->root,
          ldk_keyframe_path_root(), 0, false);
      ldk_ui_end_scrollview(ui);
    }
    else
    {
      ldk_ui_label(ui, "Select an entity with an Animation Source.");
    }
    ldk_ui_end_popup(ui);
  }

  if (ldk_ui_begin_popup(ui, LDK_EDITOR_ANIM_EDIT_POPUP_ID))
  {
    bool track_valid = state->selected_track < state->clip.track_count;
    if (ldk_ui_button_flat(ui, "Delete selected key") &&
        track_valid && state->key_selected)
    {
      (void)s_editor_animation_delete_selected_key();
      ldk_ui_close_current_popup(ui);
    }
    if (ldk_ui_button_flat(ui, "Remove selected track") && track_valid)
    {
      u32 target_track = state->selected_track;
      ldk_ui_close_current_popup(ui);
      (void)s_editor_animation_remove_track_confirm(editor, target_track);
    }
    ldk_ui_end_popup(ui);
  }

  if (ldk_ui_begin_popup(ui, LDK_EDITOR_ANIM_VIEW_POPUP_ID))
  {
    if (ldk_ui_button_flat(ui, "Previous range"))
    {
      state->timeline_offset -= s_editor_animation_span() * 0.25f;
    }
    if (ldk_ui_button_flat(ui, "Next range"))
    {
      state->timeline_offset += s_editor_animation_span() * 0.25f;
    }
    if (ldk_ui_button_flat(ui, "Zoom out"))
    {
      s_editor_animation_zoom(1.0f / 1.5f);
    }
    if (ldk_ui_button_flat(ui, "Zoom in"))
    {
      s_editor_animation_zoom(1.5f);
    }
    if (ldk_ui_button_flat(ui, "Fit"))
    {
      state->timeline_zoom = 1.0f;
      state->timeline_offset = 0.0f;
    }
    s_editor_animation_view_clamp();
    ldk_ui_end_popup(ui);
  }

  if (ldk_ui_begin_popup(ui, LDK_EDITOR_ANIM_EVENTS_POPUP_ID))
  {
    ldk_ui_label(ui, "Events (runtime only; preview does not dispatch)");
    ldk_ui_begin_horizontal(ui);
    ldk_ui_set_next_width(ui, ldk_ui_px(120.0f));
    ldk_ui_input_box(ui, state->event_number, sizeof(state->event_number));
    if (ldk_ui_button(ui, "Add integer event"))
    {
      char *end;
      long number = strtol(state->event_number, &end, 10);
      if (end != state->event_number && !*end &&
          number >= INT32_MIN && number <= INT32_MAX &&
          ldk_keyframe_animation_event_add_integer(
              &state->clip, state->playhead, (i32)number))
      {
        state->dirty = true;
      }
    }
    ldk_ui_end_horizontal(ui);
    ldk_ui_begin_horizontal(ui);
    ldk_ui_set_next_width(ui, ldk_ui_px(120.0f));
    ldk_ui_input_box(ui, state->event_text, sizeof(state->event_text));
    if (ldk_ui_button(ui, "Add string event") &&
        ldk_keyframe_animation_event_add_string(
            &state->clip, state->playhead, state->event_text))
    {
      state->dirty = true;
    }
    ldk_ui_end_horizontal(ui);
    if (state->selected_event >= 0 &&
        (u32)state->selected_event < state->clip.event_count)
    {
      const LDKKeyframeEvent *event =
          &state->clip.events[state->selected_event];
      snprintf(description, sizeof(description), "Event at %.2fs: %s",
          event->time,
          event->type == LDK_KEYFRAME_EVENT_INTEGER ? "integer" : "string");
      ldk_ui_label(ui, description);
    }
    ldk_ui_end_popup(ui);
  }
}

static void s_editor_animation_terminate(void)
{
  s_editor_animation_restore();
  s_editor_animation_ui_state_clear();
  if (s_editor_animation.initialized)
  {
    ldk_keyframe_animation_clear(&s_editor_animation.clip);
  }
  memset(&s_editor_animation, 0, sizeof(s_editor_animation));
}
