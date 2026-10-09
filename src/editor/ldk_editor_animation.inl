/* Animation authoring window. Included by ldk_editor_dock.c. */
#include <ldk_keyframe_animation.h>
#include <component/ldk_transform.h>
#include <component/ldk_keyframe_animation_source.h>
#include <module/ldk_asset_manager.h>
#include <module/ldk_ecs.h>
#include <module/ldk_scenegraph.h>

/* Preview state deliberately lives outside the clip. */
typedef struct LDKEditorAnimationSavedTransform
{
  LDKEntity entity;
  Vec3 position;
  Quat rotation;
  Vec3 scale;
} LDKEditorAnimationSavedTransform;

typedef struct LDKEditorAnimationState
{
  LDKKeyframeAnimation clip;
  LDKEntity root;
  i32 source_animation_index;
  char path[256];
  char event_text[128];
  char event_number[32];
  float playhead;
  u32 selected_track;
  u32 selected_key;
  i32 selected_event;
  bool key_selected;
  bool dragging_key;
  bool scrubbing;
  bool events_editor_open;
  float drag_start_x;
  float drag_start_time;
  float timeline_zoom;
  float timeline_offset;
  LDKUIPoint sheet_scroll;
  bool initialized;
  bool playing;
  bool preview;
  bool looping;
  bool dirty;
  LDKEditorAnimationSavedTransform *saved;
  u32 saved_count;
  u32 saved_capacity;
} LDKEditorAnimationState;

static LDKEditorAnimationState s_editor_animation;

static void s_editor_animation_restore(void)
{
  LDKEditorAnimationState *state = &s_editor_animation;
  if (!state->preview)
  {
    return;
  }
  for (u32 i = 0; i < state->saved_count; i++)
  {
    const LDKEditorAnimationSavedTransform *s = &state->saved[i];
    if (ldk_ecs_component_get(s->entity, LDK_COMPONENT_TYPE_TRANSFORM))
    {
      ldk_transform_set_local_position(s->entity, s->position);
      ldk_transform_set_local_rotation(s->entity, s->rotation);
      ldk_transform_set_local_scale(s->entity, s->scale);
    }
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

static bool s_editor_animation_capture_tree(LDKEntity entity, u32 depth)
{
  LDKEditorAnimationState *state = &s_editor_animation;
  const LDKTransform *transform = ldk_ecs_component_get_const(
      entity, LDK_COMPONENT_TYPE_TRANSFORM);
  if (!transform || depth > 128)
  {
    return false;
  }
  if (state->saved_count == state->saved_capacity)
  {
    u32 capacity = state->saved_capacity ? state->saved_capacity * 2 : 16;
    LDKEditorAnimationSavedTransform *entries = realloc(state->saved,
        (size_t)capacity * sizeof(*entries));
    if (!entries)
    {
      return false;
    }
    state->saved = entries;
    state->saved_capacity = capacity;
  }
  LDKEditorAnimationSavedTransform *s = &state->saved[state->saved_count++];
  s->entity = entity;
  s->position = transform->local_position;
  s->rotation = transform->local_rotation;
  s->scale = transform->local_scale;
  LDKEntity child = transform->first_child;
  while (!x_handle_is_null(child))
  {
    const LDKTransform *t = ldk_ecs_component_get_const(
        child, LDK_COMPONENT_TYPE_TRANSFORM);
    if (!t)
    {
      return false;
    }
    LDKEntity next = t->next_sibling;
    if (!s_editor_animation_capture_tree(child, depth + 1))
    {
      return false;
    }
    child = next;
  }
  return true;
}

static bool s_editor_animation_preview_begin(void)
{
  LDKEditorAnimationState *state = &s_editor_animation;
  if (state->preview)
  {
    return true;
  }
  if (!s_editor_animation_capture_tree(state->root, 0))
  {
    s_editor_animation_restore();
    free(state->saved);
    state->saved = NULL;
    state->saved_count = state->saved_capacity = 0;
    return false;
  }
  state->preview = true;
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
    /* The editor preview is evaluated after the normal scenegraph pass. */
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

static const char *s_editor_animation_channel_name(
    LDKKeyframeTransformChannel channel)
{
  switch (channel)
  {
  case LDK_KEYFRAME_TRANSFORM_POSITION: return "Position";
  case LDK_KEYFRAME_TRANSFORM_ROTATION: return "Rotation";
  case LDK_KEYFRAME_TRANSFORM_SCALE: return "Scale";
  default: return "Unknown";
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
  if (track->target_path != ldk_keyframe_path_root() &&
      !s_editor_animation_target_walk(state->root,
          ldk_keyframe_path_root(), track->target_path,
          name, sizeof(name), 0))
  {
    snprintf(name, sizeof(name), "Unresolved %016" PRIx64,
        track->target_path);
  }
  snprintf(buffer, capacity, "%s  /  %s", name,
      s_editor_animation_channel_name(track->channel));
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
  Vec4 value = track->keys[state->selected_key].value;
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

static void s_editor_animation_dopesheet(LDKUIContext *ui)
{
  LDKEditorAnimationState *state = &s_editor_animation;
  s_editor_animation_view_clamp();
  const u32 row_count = state->clip.track_count + 2; /* Events + summary. */
  const float row_h = LDK_EDITOR_ANIM_ROW_HEIGHT;
  const float ruler_h = LDK_EDITOR_ANIM_RULER_HEIGHT;
  const float content_h = ruler_h + row_h * row_count + 6.0f;

  ldk_ui_set_next_height(ui, ldk_ui_fill());
  ldk_ui_set_next_width(ui, ldk_ui_fill());
  state->sheet_scroll = ldk_ui_begin_scrollview(ui, state->sheet_scroll,
      LDK_UI_SCROLL_VERTICAL | LDK_UI_SCROLL_IF_NEEDED);
  ldk_ui_set_padding(ui, 0);
  ldk_ui_set_next_width(ui, ldk_ui_fill());
  ldk_ui_set_next_height(ui, ldk_ui_px(content_h));
  ldk_ui_spacer(ui);
  LDKUIRect sheet = ldk_ui_last_rect(ui);
  LDKUIRect old_clip = ui->clip_rect;
  ui->clip_rect = ldk_rectf_intersect(&old_clip, &sheet);
  float label_width = s_editor_animation_clamp(
      sheet.w * 0.36f, 135.0f, 260.0f);
  if (label_width > sheet.w - 55.0f)
  {
    label_width = fmaxf(0.0f, sheet.w - 55.0f);
  }
  LDKUIRect graph = {sheet.x + label_width + 9.0f,
      sheet.y, fmaxf(20.0f, sheet.w - label_width - 23.0f), content_h};
  float first_row = sheet.y + ruler_h;

  rgba32 bg = ui->theme.colors[LDK_UI_COLOR_PANEL_BG];
  rgba32 alternate = ui->theme.colors[LDK_UI_COLOR_CONTROL_BG];
  rgba32 focus = ui->theme.colors[LDK_UI_COLOR_FOCUS];
  rgba32 key_color = ui->theme.colors[LDK_UI_COLOR_SLIDER_THUMB];
  rgba32 grid = ui->theme.colors[LDK_UI_COLOR_SEPARATOR];
  s_editor_animation_draw_rect(ui, sheet, bg);
  s_editor_animation_draw_rect(ui,
      (LDKUIRect){sheet.x, sheet.y, sheet.w, ruler_h}, alternate);
  s_editor_animation_draw_rect(ui,
      (LDKUIRect){graph.x - 7, sheet.y, 1, content_h}, grid);
  ldk_ui_widget_label(ui, 0, "Properties",
      (LDKUIRect){sheet.x + 6, sheet.y + 2, label_width - 8, ruler_h - 4});

  /* Ruler ticks and vertical guides use seconds rather than frame indices. */
  float span = s_editor_animation_span();
  float desired = span * 80.0f / graph.w;
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
    float x = s_editor_animation_time_x(graph, state, time);
    s_editor_animation_draw_rect(ui,
        (LDKUIRect){x, sheet.y + ruler_h, 1, content_h - ruler_h}, grid);
    char time_label[32];
    snprintf(time_label, sizeof(time_label), "%.2fs", time);
    ldk_ui_widget_label(ui, 0, time_label,
        (LDKUIRect){x + 2, sheet.y + 1, 72, ruler_h - 2});
  }

  for (u32 row = 0; row < row_count; ++row)
  {
    float y = first_row + row * row_h;
    LDKUIRect row_rect = {sheet.x, y, sheet.w, row_h};
    if (row & 1)
    {
      s_editor_animation_draw_rect(ui, row_rect, alternate);
    }
    if (row >= 2 && row - 2 == state->selected_track &&
        state->clip.track_count)
    {
      s_editor_animation_draw_rect(ui,
          (LDKUIRect){sheet.x, y, label_width, row_h},
          ui->theme.colors[LDK_UI_COLOR_CONTROL_BG_ACTIVE]);
    }
    s_editor_animation_draw_rect(ui,
        (LDKUIRect){sheet.x, y + row_h - 1, sheet.w, 1}, grid);
    char label[256];
    if (row == 0)
    {
      snprintf(label, sizeof(label), "Events (%u)", state->clip.event_count);
    }
    else if (row == 1)
    {
      snprintf(label, sizeof(label), "All keyframes");
    }
    else
    {
      s_editor_animation_track_label(state, row - 2,
          label, sizeof(label));
    }
    LDKUIRect label_clip = ui->clip_rect;
    ui->clip_rect = ldk_rectf_intersect(&label_clip,
        &(LDKUIRect){sheet.x, y, label_width, row_h});
    ldk_ui_widget_label(ui, 0, label,
        (LDKUIRect){sheet.x + 7, y + 1, label_width - 10, row_h - 2});
    ui->clip_rect = label_clip;

    if (row == 0)
    {
      for (u32 i = 0; i < state->clip.event_count; ++i)
      {
        float x = s_editor_animation_time_x(
            graph, state, state->clip.events[i].time);
        if (x >= graph.x - 4 && x <= graph.x + graph.w + 4)
        {
          s_editor_animation_draw_rect(ui,
              (LDKUIRect){x - 2, y + 4, 5, row_h - 8}, focus);
        }
      }
    }
    else
    {
      u32 begin = row == 1 ? 0 : row - 2;
      u32 end = row == 1 ? state->clip.track_count : begin + 1;
      for (u32 t = begin; t < end; ++t)
      {
        const LDKKeyframeTrack *track = &state->clip.tracks[t];
        for (u32 k = 0; k < track->count; ++k)
        {
          float x = s_editor_animation_time_x(
              graph, state, track->keys[k].time);
          if (x < graph.x - 5 || x > graph.x + graph.w + 5)
          {
            continue;
          }
          bool selected = row >= 2 && state->key_selected &&
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
  float cursor_x = s_editor_animation_time_x(graph, state, state->playhead);
  if (cursor_x >= graph.x && cursor_x <= graph.x + graph.w)
  {
    s_editor_animation_draw_rect(ui,
        (LDKUIRect){cursor_x, sheet.y, 2, content_h}, focus);
    s_editor_animation_draw_rect(ui,
        (LDKUIRect){cursor_x - 4, sheet.y, 10, 7}, focus);
  }
  ui->clip_rect = old_clip;

  ldk_ui_end_scrollview(ui);

  /* Canvas interaction is managed as one widget to avoid per-key UI state. */
  LDKMouseState *mouse = (LDKMouseState *)ui->mouse;
  if (!mouse)
  {
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

  if (state->dragging_key && up)
  {
    float new_time = state->drag_start_time +
        (mx - state->drag_start_x) * span / graph.w;
    s_editor_animation_move_key(new_time);
    state->dragging_key = false;
  }
  if (state->scrubbing)
  {
    if (held)
    {
      s_editor_animation_set_time(
          s_editor_animation_x_time(graph, state, mx));
    }
    else
    {
      state->scrubbing = false;
    }
  }
  if (!hovered || !down)
  {
    return;
  }
  if (my < first_row || mx < graph.x)
  {
    if (mx >= graph.x)
    {
      state->playing = false;
      state->scrubbing = true;
      s_editor_animation_set_time(
          s_editor_animation_x_time(graph, state, mx));
    }
    else if (my >= first_row + 2 * row_h)
    {
      u32 row = (u32)((my - first_row) / row_h);
      if (row >= 2 && row - 2 < state->clip.track_count)
      {
        state->selected_track = row - 2;
        state->key_selected = false;
      }
    }
    return;
  }

  u32 row = (u32)((my - first_row) / row_h);
  if (row >= row_count)
  {
    return;
  }
  if (row == 0)
  {
    state->selected_event = -1;
    for (u32 i = 0; i < state->clip.event_count; ++i)
    {
      float x = s_editor_animation_time_x(
          graph, state, state->clip.events[i].time);
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
      return;
    }
  }
  else
  {
    u32 begin = row == 1 ? 0 : row - 2;
    u32 end = row == 1 ? state->clip.track_count : begin + 1;
    for (u32 t = begin; t < end; ++t)
    {
      LDKKeyframeTrack *track = &state->clip.tracks[t];
      for (u32 k = 0; k < track->count; ++k)
      {
        float x = s_editor_animation_time_x(
            graph, state, track->keys[k].time);
        if (fabsf(x - mx) < 7.0f)
        {
          state->selected_track = t;
          state->selected_key = k;
          state->key_selected = true;
          state->playing = false;
          state->dragging_key = row >= 2;
          state->drag_start_x = mx;
          state->drag_start_time = track->keys[k].time;
          s_editor_animation_set_time(track->keys[k].time);
          return;
        }
      }
    }
    if (row >= 2)
    {
      state->selected_track = row - 2;
      state->key_selected = false;
    }
  }
  state->playing = false;
  state->scrubbing = true;
  s_editor_animation_set_time(s_editor_animation_x_time(graph, state, mx));
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
    ldk_keyframe_animation_clear(&state->clip);
    state->root = root;
    state->source_animation_index = index;
    snprintf(state->path, sizeof(state->path), "%s", path);
    state->playhead = 0.0f;
    s_editor_animation_reset_selection();
    state->dirty = false;
    if (path[0] && !ldk_keyframe_animation_load(&state->clip, path))
    {
      ldki_editor_log_error(editor, "Unable to load selected animation.");
      state->path[0] = 0;
      state->source_animation_index = -1;
    }
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
  ldk_ui_set_next_width(ui, ldk_ui_px(125.0f));
  float duration = ldk_ui_slider_input(ui, state->clip.duration, 0.1f, 30.0f);
  if (duration > 0.0f && duration != state->clip.duration)
  {
    state->clip.duration = duration;
    state->dirty = true;
  }
  ldk_ui_set_next_width(ui, ldk_ui_px(65.0f));
  if (ldk_ui_button_flat(ui, "+ Key"))
  {
    if (state->selected_track < state->clip.track_count)
    {
      LDKKeyframeTrack *track = &state->clip.tracks[state->selected_track];
      LDKEntity target;
      Vec4 value;
      if (s_editor_animation_track_entity(state->selected_track, &target) &&
          ldk_keyframe_animation_value_from_transform(
              target, track->channel, &value) &&
          ldk_keyframe_animation_key_set(&state->clip,
              state->selected_track, state->playhead, value))
      {
        state->dirty = true;
        state->key_selected = false;
      }
    }
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

  s_editor_animation_dopesheet(ui);

  /* The only persistent control beneath the Dopesheet. */
  ldk_ui_set_next_height(ui, ldk_ui_px(30.0f));
  ldk_ui_begin_horizontal(ui);
  ldk_ui_set_padding(ui, 0.0f);
  ldk_ui_set_next_width(ui, ldk_ui_px(200.0f));
  if (ldk_ui_button(ui, "Add Property"))
  {
    ldk_ui_open_popup(ui, LDK_EDITOR_ANIM_ADD_PROPERTY_POPUP_ID);
  }
  ldk_ui_end_horizontal(ui);

  u64 target_path = 0;
  bool valid_target = has_selection && !x_handle_is_null(state->root) &&
      ldk_keyframe_entity_path(state->root, selected, &target_path);
  if (ldk_ui_begin_popup(ui, LDK_EDITOR_ANIM_ADD_PROPERTY_POPUP_ID))
  {
    if (valid_target)
    {
      for (u32 i = 0; i < 3; ++i)
      {
        LDKKeyframeTransformChannel channel = (LDKKeyframeTransformChannel)i;
        ldk_ui_push_id_u32(ui, i);
        if (ldk_ui_button_flat(ui, s_editor_animation_channel_name(channel)))
        {
          i32 track = ldk_keyframe_animation_track_add(
              &state->clip, target_path, channel);
          if (track >= 0)
          {
            state->selected_track = (u32)track;
            state->key_selected = false;
            state->dirty = true;
          }
          ldk_ui_close_current_popup(ui);
        }
        ldk_ui_pop_id(ui);
      }
    }
    else
    {
      ldk_ui_label(ui, "Select the root or one of its descendants.");
    }
    ldk_ui_end_popup(ui);
  }

  if (ldk_ui_begin_popup(ui, LDK_EDITOR_ANIM_EDIT_POPUP_ID))
  {
    bool track_valid = state->selected_track < state->clip.track_count;
    if (ldk_ui_button_flat(ui, "Delete selected key") &&
        track_valid && state->key_selected)
    {
      LDKKeyframeTrack *track = &state->clip.tracks[state->selected_track];
      if (state->selected_key < track->count &&
          ldk_keyframe_animation_key_remove(&state->clip,
              state->selected_track, state->selected_key))
      {
        state->key_selected = false;
        state->dirty = true;
      }
      ldk_ui_close_current_popup(ui);
    }
    if (ldk_ui_button_flat(ui, "Remove selected track") && track_valid)
    {
      s_editor_animation_restore();
      if (ldk_keyframe_animation_track_remove(
              &state->clip, state->selected_track))
      {
        s_editor_animation_reset_selection();
        state->dirty = true;
      }
      ldk_ui_close_current_popup(ui);
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
  if (s_editor_animation.initialized)
  {
    ldk_keyframe_animation_clear(&s_editor_animation.clip);
  }
  memset(&s_editor_animation, 0, sizeof(s_editor_animation));
}
