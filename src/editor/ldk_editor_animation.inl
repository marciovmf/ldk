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
  char path[256];
  char event_text[128];
  char event_number[32];
  float playhead;
  u32 selected_track;
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

static void s_editor_animation_window(LDKEditor *opaque, void *data)
{
  (void)data;
  LDKEditorContext *editor = (LDKEditorContext *)opaque;
  LDKEditorAnimationState *state = &s_editor_animation;
  LDKUIContext *ui = &editor->ui;
  LDKECS *ecs = (LDKECS *)ldk_module_get(LDK_MODULE_ECS);
  LDKEntity selected = x_handle_null();
  bool has_selection = ecs &&
      ldki_editor_selected_entity_get(editor, ecs, &selected);
  if (!state->initialized)
  {
    ldk_keyframe_animation_init(&state->clip);
    state->root = x_handle_null();
    state->looping = true;
    state->initialized = true;
  }
  if (editor->editor_state != LDK_EDITOR_STATE_STOPED && state->preview)
  {
    s_editor_animation_restore();
  }

  ldk_ui_begin_horizontal(ui);
  ldk_ui_label(ui, "Clip (.anim):");
  ldk_ui_input_box(ui, state->path, sizeof(state->path));
  if (ldk_ui_button(ui, "New"))
  {
    s_editor_animation_restore();
    ldk_keyframe_animation_clear(&state->clip);
    state->playhead = 0;
    state->selected_track = 0;
    state->dirty = false;
  }
  if (ldk_ui_button(ui, "Load"))
  {
    s_editor_animation_restore();
    if (ldk_keyframe_animation_load(&state->clip, state->path))
    {
      state->playhead = 0;
      state->selected_track = 0;
      state->dirty = false;
    }
    else
    {
      ldki_editor_log_error(editor, "Unable to load .anim asset.");
    }
  }
  if (ldk_ui_button(ui, "Save"))
  {
    if (ldk_keyframe_animation_save(&state->clip, state->path))
    {
      LDKAssetManager *assets = ldk_module_get(LDK_MODULE_ASSET_MANAGER);
      if (assets && !ldk_asset_manager_keyframe_animation_reload(
              assets, state->path))
      {
        ldki_editor_log_error(editor, "Saved .anim but could not refresh its loaded asset.");
      }
      state->dirty = false;
    }
    else
    {
      ldki_editor_log_error(editor, "Unable to save .anim asset.");
    }
  }
  ldk_ui_end_horizontal(ui);

  ldk_ui_begin_horizontal(ui);
  if (ldk_ui_button(ui, "Use selection as root") && has_selection)
  {
    s_editor_animation_restore();
    state->root = selected;
  }
  if (ldk_ui_button(ui, "Load selected source") && has_selection)
  {
    const LDKKeyFrameAnimationSource *source = ldk_ecs_component_get_const(
        selected, LDK_COMPONENT_TYPE_KEYFRAME_ANIMATION_SOURCE);
    LDKAssetManager *assets = ldk_module_get(LDK_MODULE_ASSET_MANAGER);
    LDKAssetHandle generic;
    const LDKAssetInfo *info;
    generic.h = source ? source->animation.h : x_handle_null();
    info = assets ? ldk_asset_get_info_const(assets, generic) : NULL;
    if (info && info->type == LDK_ASSET_TYPE_KEYFRAME_ANIMATION &&
        ldk_keyframe_animation_load(&state->clip, info->asset_path.buf))
    {
      s_editor_animation_restore();
      state->root = selected;
      snprintf(state->path, sizeof(state->path), "%s", info->asset_path.buf);
      state->playhead = 0.0f;
      state->selected_track = 0;
      state->dirty = false;
    }
    else
    {
      ldki_editor_log_error(editor, "Select an entity with a valid animation source.");
    }
  }
  if (x_handle_is_null(state->root) && has_selection)
  {
    state->root = selected;
  }
  char description[144];
  if (ldk_ecs_component_get(state->root, LDK_COMPONENT_TYPE_TRANSFORM))
  {
    const char *name = ldk_ecs_entity_name_get(state->root);
    snprintf(description, sizeof(description), "Root: %s",
        name && name[0] ? name : "(unnamed)");
  }
  else
  {
    snprintf(description, sizeof(description), "Root: (select an entity)");
    s_editor_animation_restore();
    state->root = x_handle_null();
  }
  ldk_ui_label(ui, description);
  ldk_ui_end_horizontal(ui);

  ldk_ui_horizontal_line(ui);
  if (editor->editor_state != LDK_EDITOR_STATE_STOPED)
  {
    ldk_ui_label(ui, "Stop the game to preview or edit animation.");
    return;
  }
  ldk_ui_begin_horizontal(ui);
  if (ldk_ui_button(ui, "Play") && !x_handle_is_null(state->root) &&
      s_editor_animation_preview_begin())
  {
    if (state->playhead >= state->clip.duration)
    {
      state->playhead = 0.0f;
    }
    s_editor_animation_set_time(state->playhead);
    state->playing = true;
  }
  if (ldk_ui_button(ui, "Pause"))
  {
    state->playing = false;
  }
  if (ldk_ui_button(ui, "Stop"))
  {
    s_editor_animation_restore();
    state->playhead = 0;
  }
  ldk_ui_label(ui, "Loop");
  state->looping = ldk_ui_toggle(ui, state->looping);
  ldk_ui_label(ui, "Duration");
  float duration = ldk_ui_slider_input(ui, state->clip.duration, 0.1f, 30.0f);
  if (duration > 0 && duration != state->clip.duration)
  {
    state->clip.duration = duration;
    state->dirty = true;
  }
  ldk_ui_end_horizontal(ui);

  float time = ldk_ui_slider_input(ui, state->playhead, 0.0f,
      state->clip.duration);
  if (time != state->playhead)
  {
    state->playing = false;
    s_editor_animation_set_time(time);
  }
  snprintf(description, sizeof(description), "%.3fs / %.3fs %s",
      state->playhead, state->clip.duration, state->dirty ? "*" : "");
  ldk_ui_label(ui, description);

  ldk_ui_horizontal_line(ui);
  u64 target_path;
  bool valid_target = has_selection && !x_handle_is_null(state->root) &&
      ldk_keyframe_entity_path(state->root, selected, &target_path);
  if (valid_target)
  {
    ldk_ui_label(ui, "Add track for selected entity:");
    ldk_ui_begin_horizontal(ui);
    for (u32 i = 0; i < 3; i++)
    {
      LDKKeyframeTransformChannel channel = (LDKKeyframeTransformChannel)i;
      ldk_ui_push_id_u32(ui, i);
      if (ldk_ui_button(ui, s_editor_animation_channel_name(channel)))
      {
        i32 track = ldk_keyframe_animation_track_add(&state->clip,
            target_path, channel);
        if (track >= 0)
        {
          state->selected_track = (u32)track;
          state->dirty = true;
        }
      }
      ldk_ui_pop_id(ui);
    }
    ldk_ui_end_horizontal(ui);
  }
  else
  {
    ldk_ui_label(ui, "Select the root or one of its descendants to add tracks.");
  }

  ldk_ui_horizontal_line(ui);
  ldk_ui_label(ui, "Tracks");
  for (u32 i = 0; i < state->clip.track_count; i++)
  {
    const LDKKeyframeTrack *track = &state->clip.tracks[i];
    snprintf(description, sizeof(description), "%s %016" PRIx64 " (%u keys)%s",
        s_editor_animation_channel_name(track->channel),
        track->target_path, track->count,
        i == state->selected_track ? "  [selected]" : "");
    ldk_ui_push_id_u32(ui, i);
    if (ldk_ui_button(ui, description))
    {
      state->selected_track = i;
    }
    ldk_ui_pop_id(ui);
  }
  if (state->selected_track >= state->clip.track_count)
  {
    state->selected_track = 0;
  }
  if (state->clip.track_count)
  {
    LDKKeyframeTrack *track = &state->clip.tracks[state->selected_track];
    /* A track can be keyed only when its target is currently selected. */
    bool selected_matches = valid_target && track->target_path == target_path;
    ldk_ui_begin_horizontal(ui);
    if (ldk_ui_button(ui, "+ Key at playhead") && selected_matches)
    {
      Vec4 value;
      if (ldk_keyframe_animation_value_from_transform(
              selected, track->channel, &value) &&
          ldk_keyframe_animation_key_set(&state->clip,
              state->selected_track, state->playhead, value))
      {
        state->dirty = true;
      }
    }
    if (ldk_ui_button(ui, "Remove Track"))
    {
      s_editor_animation_restore();
      if (ldk_keyframe_animation_track_remove(
              &state->clip, state->selected_track))
      {
        state->dirty = true;
        state->selected_track = 0;
      }
      ldk_ui_end_horizontal(ui);
      return;
    }
    ldk_ui_end_horizontal(ui);
    if (!selected_matches)
    {
      ldk_ui_label(ui, "Select this track's target to capture its Transform.");
    }
    for (u32 i = 0; i < track->count; i++)
    {
      ldk_ui_push_id_u32(ui, i);
      snprintf(description, sizeof(description), "Key %.3fs", track->keys[i].time);
      ldk_ui_begin_horizontal(ui);
      if (ldk_ui_button(ui, description))
      {
        state->playing = false;
        s_editor_animation_set_time(track->keys[i].time);
      }
      if (ldk_ui_button(ui, "Delete"))
      {
        if (ldk_keyframe_animation_key_remove(&state->clip,
                state->selected_track, i))
        {
          state->dirty = true;
        }
        ldk_ui_end_horizontal(ui);
        ldk_ui_pop_id(ui);
        break;
      }
      ldk_ui_end_horizontal(ui);
      ldk_ui_pop_id(ui);
    }
  }

  ldk_ui_horizontal_line(ui);
  ldk_ui_label(ui, "Events (dispatched by runtime API, not preview)");
  ldk_ui_begin_horizontal(ui);
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
  ldk_ui_input_box(ui, state->event_text, sizeof(state->event_text));
  if (ldk_ui_button(ui, "Add string event") &&
      ldk_keyframe_animation_event_add_string(
          &state->clip, state->playhead, state->event_text))
  {
    state->dirty = true;
  }
  ldk_ui_end_horizontal(ui);
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
