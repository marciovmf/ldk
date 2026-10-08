#define LDK_IMPL_STDX
#include "../ldk_stdx.h"

#include <ldk_common.h>
#include <ldk_game.h>
#include <ldk_profiler.h>
#include <ldk_event.h>
#include <ldk_os.h>
#include <ldk_package.h>
#include <ldk_image.h>
#include <ldk_scene.h>
#include <ldk_project.h>
#include <component/ldk_camera.h>
#include <component/ldk_transform.h>
#include <module/ldk_ecs.h>
#include <module/ldk_ui.h>
#include <module/ldk_renderer.h>
#include <module/ldk_asset_manager.h>
#include <module/ldk_asset_source.h>
#include <module/ldk_scene_manager.h>
#include <module/ldk_scenegraph.h>
#include "ldk_editor_internal.h"
#include "ldk_editor_project_create.h"
#include "ldk_editor_scene_ops.h"
#include "ldk_editor_package_catalog.h"
#include "ldk_editor_theme.h"
#include "ldk_ui_drag_n_drop.h"

#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef LDK_DEFAULT_UI_INITIAL_INDEX_CAPACITY
#define LDK_DEFAULT_UI_INITIAL_INDEX_CAPACITY 256
#endif

#ifndef LDK_DEFAULT_UI_INITIAL_VERTEX_CAPACITY
#define LDK_DEFAULT_UI_INITIAL_VERTEX_CAPACITY 1024
#endif

#ifndef LDK_DEFAULT_UI_INITIAL_COMMAND_CAPACITY
#define LDK_DEFAULT_UI_INITIAL_COMMAND_CAPACITY 32
#endif

#ifndef LDK_DEFAULT_UI_INITIAL_WINDOW_CAPACITY
#define LDK_DEFAULT_UI_INITIAL_WINDOW_CAPACITY 16
#endif

#ifndef LDK_DEFAULT_UI_INITIAL_STACK_CAPACITY
#define LDK_DEFAULT_UI_INITIAL_STACK_CAPACITY 16
#endif

static void s_editor_statistics_only(LDKEditorContext *editor, i32 window_width,
    i32 window_height, float delta_time);

static void s_editor_update(LDKEditorContext *editor, i32 window_width,
    i32 window_height, float delta_time);
static bool s_editor_state_set_play(LDKEditorContext *editor);
static bool s_editor_state_enter_play(LDKEditorContext *editor);
static void s_editor_state_set_stop(LDKEditorContext *editor);
static bool s_project_load(
    LDKEditorContext *editor, const char *project_file_path);
static bool s_project_unload(LDKEditorContext *editor);
static bool s_project_import_packages_mount(LDKEditorContext *editor);
static bool s_editor_project_action_process(LDKEditorContext *editor);
static bool s_editor_project_build_request_with_continuation(
    LDKEditorContext *editor, LDKEditorProjectBuildContinuation continuation);
static bool s_editor_camera_ensure(LDKEditorContext *editor);
static bool s_project_game_module_load(LDKEditorContext *editor);
static void s_project_game_module_watch_update(LDKEditorContext *editor);
static bool s_editor_selected_entity_duplicate(LDKEditorContext *editor);

/**
 * Tiny function to return a static editor instance.
 */
static LDKEditorContext *s_editor_instance(void)
{
  static LDKEditorContext editor = {0};
  return &editor;
}

static float s_editor_ui_scale_validated(float scale)
{
  return isfinite(scale) && scale >= 0.5f && scale <= 3.0f ? scale : 1.0f;
}

void ldki_editor_mouse_state_get(
    const LDKEditorContext *editor, LDKMouseState *out_state)
{
  float scale;

  if (out_state == NULL)
  {
    return;
  }

  ldk_os_mouse_state_get(out_state);
  scale = editor != NULL
              ? s_editor_ui_scale_validated(editor->ui_frame_scale)
              : 1.0f;

  if (scale == 1.0f)
  {
    return;
  }

  out_state->cursor.x = (i32)lroundf((float)out_state->cursor.x / scale);
  out_state->cursor.y = (i32)lroundf((float)out_state->cursor.y / scale);
  out_state->cursor_relative.x =
      (i32)lroundf((float)out_state->cursor_relative.x / scale);
  out_state->cursor_relative.y =
      (i32)lroundf((float)out_state->cursor_relative.y / scale);
}

static LDKRendererViewId s_editor_view_id_from_entity(LDKEntity entity)
{
  return ((u64)entity.version << 32u) | ((u64)entity.index + 1u);
}

static void s_editor_camera_settings_apply(
    const LDKEditorContext *editor, LDKCamera *camera)
{
  if (editor == NULL || camera == NULL)
  {
    return;
  }

  camera->fov_y = deg_to_rad(editor->editor_camera_fov);
  camera->near_plane = editor->editor_camera_near_clip;
  camera->far_plane = editor->editor_camera_far_clip;
}

static bool s_editor_camera_ensure(LDKEditorContext *editor)
{
  LDKECS *ecs;
  LDKCamera *camera;
  LDKEntity entity;

  if (editor == NULL)
  {
    return false;
  }

  ecs = ldk_module_get(LDK_MODULE_ECS);
  if (ecs == NULL)
  {
    return false;
  }

  if (ldk_entity_is_alive(&ecs->entity, editor->editor_camera))
  {
    camera = (LDKCamera *)ldk_ecs_component_get(
        editor->editor_camera, LDK_COMPONENT_TYPE_CAMERA);

    if (camera != NULL && camera->role == LDK_CAMERA_ROLE_EDITOR)
    {
      s_editor_camera_settings_apply(editor, camera);
      editor->scene_view =
          s_editor_view_id_from_entity(editor->editor_camera);
      return true;
    }
  }

  editor->editor_camera = x_handle_null();
  editor->scene_view = LDK_RENDERER_VIEW_INVALID;

  entity = ldk_ecs_entity_create();
  if (x_handle_is_null(entity))
  {
    return false;
  }

  camera = (LDKCamera *)ldk_ecs_component_add(
      entity, LDK_COMPONENT_TYPE_CAMERA, NULL);
  if (camera == NULL)
  {
    ldk_ecs_entity_destroy(entity);
    return false;
  }

  camera->role = LDK_CAMERA_ROLE_EDITOR;
  camera->enabled = true;
  s_editor_camera_settings_apply(editor, camera);
  ldk_entity_internal_flags_add(
      &ecs->entity, entity, LDK_ENTITY_INTERNAL_EDITOR);
  ldk_transform_set_local_position(
      entity, vec3_make(8.0f, 8.0f, 8.0f));
  ldk_camera_look_at(entity, vec3_make(0.0f, 0.0f, 0.0f));

  if (!ldk_scenegraph_update_entity(entity))
  {
    ldk_ecs_entity_destroy(entity);
    return false;
  }

  editor->editor_camera = entity;
  editor->scene_view = s_editor_view_id_from_entity(entity);
  return true;
}

static bool s_editor_cmake_version_is_supported(const char *cmake_path)
{
  char command[X_SMALLSTR_MAX_LENGTH + 32];
  char version_text[128];
  i32 major;
  i32 minor;

  snprintf(command, sizeof(command), "\"%s\" --version", cmake_path);

  FILE *process = _popen(command, "r");
  if (!process)
    return false;

  bool output_read = fgets(version_text, sizeof(version_text), process) != NULL;

  i32 exit_code = _pclose(process);
  if (!output_read || exit_code != 0)
    return false;

  if (sscanf(version_text, "cmake version %d.%d", &major, &minor) != 2)
    return false;

  return major > 4 || (major == 4 && minor >= 3);
}

/**
 * Returns the path to a supported CMake executable.
 *
 * An empty path is returned if CMake cannot be found or the user cancels
 * the file dialog.
 */
static XFSPath s_editor_cmake_path_get(LDKWindow owner)
{
  XFSPath cmake_path = {0};

  if (x_fs_executable_find("cmake", &cmake_path) &&
      s_editor_cmake_version_is_supported(cmake_path.buf))
  {
    return cmake_path;
  }

  while (true)
  {
    bool selected = ldk_os_dialog_show_open_file(owner,
        "Locate CMake 4.3 or newer", "CMake executable\0cmake.exe\0\0",
        &cmake_path);

    if (!selected)
      return (XFSPath){0};

    x_fs_path_normalize(&cmake_path);

    if (x_fs_path_is_file(&cmake_path) &&
        s_editor_cmake_version_is_supported(cmake_path.buf))
    {
      return cmake_path;
    }

    ldk_os_dialog_show_error(owner, "Unsupported CMake",
        "The selected executable is not CMake 4.3 or newer.");
  }
}

const char *ldki_editor_cmake_native_arch_get(void)
{
#if defined(X_ARCH_X64)
  return "x64";
#elif defined(X_ARCH_X86)
  return "Win32";
#elif defined(X_ARCH_ARM64)
  return "ARM64";
#elif defined(X_ARCH_ARM32)
  return "ARM";
#else
  return "";
#endif
}

//----------------------------------------------------------
// Event Handlers
//----------------------------------------------------------

static bool _on_event_keyboard(const LDKEvent *event, void *state)
{
  LDKEditorContext *editor = (LDKEditorContext *)state;
  if (editor->exclusive_mode &&
      editor->editor_state == LDK_EDITOR_STATE_PLAYING)
  {
    if (event->keyboard_event.type == LDK_KEYBOARD_EVENT_KEY_DOWN &&
        event->keyboard_event.ctrl_is_down &&
        event->keyboard_event.keyCode == LDK_KEYCODE_P)
    {
      ldk_editor_state_set_stop(editor);
      return true;
    }
    return false;
  }
  if (event->keyboard_event.type == LDK_KEYBOARD_EVENT_KEY_DOWN)
  {
    bool entity_window_focused =
        ldki_editor_window_is_focused(editor, LDK_EDITOR_WINDOW_HIERARCHY) ||
        ldki_editor_window_is_focused(editor, LDK_EDITOR_WINDOW_SCENE);

    if (event->keyboard_event.ctrl_is_down &&
        event->keyboard_event.shift_is_down)
    {
      // CTRL+SHIFT+P
      if (event->keyboard_event.keyCode == LDK_KEYCODE_P)
      {
        ldk_editor_state_set_stop(editor);
        return true;
      }

      // CTRL+SHIFT+N
      if (!event->keyboard_event.alt_is_down && entity_window_focused &&
          event->keyboard_event.keyCode == LDK_KEYCODE_N)
      {
        LDKECS *ecs = ldk_module_get(LDK_MODULE_ECS);
        ldki_editor_entity_add(editor, ecs);
        return true;
      }
    }
    else if (event->keyboard_event.ctrl_is_down)
    {
      // CTRL+P
      if (event->keyboard_event.keyCode == LDK_KEYCODE_P)
      {
        if (editor->editor_state == LDK_EDITOR_STATE_PLAYING)
        {
          ldk_editor_state_set_stop(editor);
        }
        else
        {
          s_editor_state_set_play(editor);
        }
        return true;
      }

      // CTRL+O
      if (event->keyboard_event.keyCode == LDK_KEYCODE_O)
      {
        ldki_editor_show_open_project_dialog(editor, NULL);
        return true;
      }
    }

    // Entity shortcuts
    if (!event->keyboard_event.ctrl_is_down &&
        !event->keyboard_event.shift_is_down &&
        !event->keyboard_event.alt_is_down && entity_window_focused &&
        event->keyboard_event.keyCode == LDK_KEYCODE_DELETE)
    {
      LDKECS *ecs = ldk_module_get(LDK_MODULE_ECS);
      ldki_editor_selected_entity_remove(editor, ecs);
      return true;
    }

    // Scene Viewer Tools shortcuts
    if (ldki_editor_window_is_focused(editor, LDK_EDITOR_WINDOW_SCENE))
    {
      if (event->keyboard_event.keyCode == LDK_KEYCODE_W)
      {
        editor->gizmo.mode = (LDKEditorGizmoMode)LDK_EDITOR_GIZMO_MODE_TRANSLATE;
      }
      else if (event->keyboard_event.keyCode == LDK_KEYCODE_E)
      {
        editor->gizmo.mode = (LDKEditorGizmoMode)LDK_EDITOR_GIZMO_MODE_ROTATE;
      }
      else if (event->keyboard_event.keyCode == LDK_KEYCODE_R)
      {
        editor->gizmo.mode = (LDKEditorGizmoMode)LDK_EDITOR_GIZMO_MODE_SCALE;
      }
    }
  }
  return false;
}

static bool on_event_keyboard(const LDKEvent *event, void *state)
{
  LDKEditorContext *editor = (LDKEditorContext *)state;
  if (editor->exclusive_mode &&
      editor->editor_state == LDK_EDITOR_STATE_PLAYING)
  {
    if (event->keyboard_event.type == LDK_KEYBOARD_EVENT_KEY_DOWN &&
        event->keyboard_event.ctrl_is_down &&
        event->keyboard_event.keyCode == LDK_KEYCODE_P)
    {
      ldk_editor_state_set_stop(editor);
      return true;
    }
    return false;
  }
  if (event->keyboard_event.type == LDK_KEYBOARD_EVENT_KEY_DOWN)
  {
    bool entity_window_focused =
        ldki_editor_window_is_focused(editor, LDK_EDITOR_WINDOW_HIERARCHY) ||
        ldki_editor_window_is_focused(editor, LDK_EDITOR_WINDOW_SCENE);
    bool duplicate_window_focused =
        ldki_editor_window_is_focused(editor, LDK_EDITOR_WINDOW_INSPECTOR) ||
        ldki_editor_window_is_focused(editor, LDK_EDITOR_WINDOW_SCENE);

    if (event->keyboard_event.ctrl_is_down &&
        event->keyboard_event.shift_is_down)
    {
      // CTRL+SHIFT+P
      if (event->keyboard_event.keyCode == LDK_KEYCODE_P)
      {
        ldk_editor_state_set_stop(editor);
        return true;
      }

      // CTRL+SHIFT+N
      if (!event->keyboard_event.alt_is_down && entity_window_focused &&
          event->keyboard_event.keyCode == LDK_KEYCODE_N)
      {
        LDKECS *ecs = ldk_module_get(LDK_MODULE_ECS);
        ldki_editor_entity_add(editor, ecs);
        return true;
      }
    }
    else if (event->keyboard_event.ctrl_is_down)
    {
      // CTRL+D
      if (!event->keyboard_event.alt_is_down && duplicate_window_focused &&
          event->keyboard_event.keyCode == LDK_KEYCODE_D)
      {
        s_editor_selected_entity_duplicate(editor);
        return true;
      }

      // CTRL+P
      if (event->keyboard_event.keyCode == LDK_KEYCODE_P)
      {
        if (editor->editor_state == LDK_EDITOR_STATE_PLAYING)
        {
          ldk_editor_state_set_stop(editor);
        }
        else
        {
          s_editor_state_set_play(editor);
        }
        return true;
      }

      // CTRL+O
      if (event->keyboard_event.keyCode == LDK_KEYCODE_O)
      {
        ldki_editor_show_open_project_dialog(editor, NULL);
        return true;
      }
    }

    // Entity shortcuts
    if (!event->keyboard_event.ctrl_is_down &&
        !event->keyboard_event.shift_is_down &&
        !event->keyboard_event.alt_is_down && entity_window_focused &&
        event->keyboard_event.keyCode == LDK_KEYCODE_DELETE)
    {
      LDKECS *ecs = ldk_module_get(LDK_MODULE_ECS);
      ldki_editor_selected_entity_remove(editor, ecs);
      return true;
    }

    // Scene Viewer Tools shortcuts
    if (ldki_editor_window_is_focused(editor, LDK_EDITOR_WINDOW_SCENE) &&
        !event->keyboard_event.ctrl_is_down &&
        !event->keyboard_event.shift_is_down &&
        !event->keyboard_event.alt_is_down && !editor->gizmo.dragging &&
        !editor->camera_controller.panning &&
        !editor->camera_controller.orbiting)
    {
      if (event->keyboard_event.keyCode == LDK_KEYCODE_Q)
      {
        editor->gizmo.mode = LDK_EDITOR_GIZMO_MODE_PAN;
      }
      else if (event->keyboard_event.keyCode == LDK_KEYCODE_W)
      {
        editor->gizmo.mode =
            (LDKEditorGizmoMode)LDK_EDITOR_GIZMO_MODE_TRANSLATE;
      }
      else if (event->keyboard_event.keyCode == LDK_KEYCODE_E)
      {
        editor->gizmo.mode =
            (LDKEditorGizmoMode)LDK_EDITOR_GIZMO_MODE_ROTATE;
      }
      else if (event->keyboard_event.keyCode == LDK_KEYCODE_R)
      {
        editor->gizmo.mode =
            (LDKEditorGizmoMode)LDK_EDITOR_GIZMO_MODE_SCALE;
      }
      else if (event->keyboard_event.keyCode == LDK_KEYCODE_F)
      {
        ldki_editor_camera_focus_selected(editor);
        return true;
      }
    }
  }
  return false;
}


static bool on_event_text(const LDKEvent *event, void *state)
{
  LDKEditorContext *editor = (LDKEditorContext *)state;
  if (editor->exclusive_mode &&
      editor->editor_state == LDK_EDITOR_STATE_PLAYING)
  {
    return false;
  }
  if (event->text_event.type == LDK_TEXT_EVENT_CHARACTER_INPUT)
  {
    if (editor->text_input_state.codepoint_count <
        LDK_UI_INPUT_CODEPOINTS_CAPACITY)
    {
      editor->text_input_state
          .codepoints[editor->text_input_state.codepoint_count++] =
          event->text_event.character;
      return true;
    }
  }
  return false;
}

bool ldki_editor_profiler_path_get(
    const LDKEditorContext *editor, XFSPath *path)
{
  if (!editor || !editor->project.loaded ||
      !editor->project.game_dll_path.length ||
      !x_fs_path_dirname(&editor->project.game_dll_path, path) ||
      !x_fs_path_join(path, ".ldk/profiling/play.ldkp"))
  {
    return false;
  }
  x_fs_path_normalize(path);
  return true;
}

static void s_editor_profiler_stop(LDKEditorContext *editor)
{
  if (!editor->profiler_recording)
  {
    return;
  }
  if (editor->profiler_frame_open)
  {
    ldk_profiler_frame_end();
    editor->profiler_frame_open = false;
  }
  ldk_profiler_capture_stop();
  editor->profiler_recording = false;
  editor->profiler_revision += 1u;
}

static bool s_editor_profiler_session_requested(LDKEditorContext *editor)
{
  return editor->profile && editor->project.loaded &&
         editor->editor_state != LDK_EDITOR_STATE_STOPED;
}

static void s_editor_profiler_frame_event(
    LDKEditorContext *editor, const LDKEvent *event)
{
  static LDKProfilerSource update_source = {0};
  static LDKProfilerSource submit_source = {0};
  static LDKProfilerSource render_source = {0};

  if (event->frame_event.type == LDK_FRAME_EVENT_UPDATE_BEFORE)
  {
    XFSPath path = {0};
    bool requested = s_editor_profiler_session_requested(editor);
    bool running = editor->editor_state == LDK_EDITOR_STATE_PLAYING ||
                   editor->editor_state == LDK_EDITOR_STATE_STEPPING;
    bool has_path = ldki_editor_profiler_path_get(editor, &path);
    bool path_changed = strcmp(path.buf, editor->profiler_path.buf) != 0;

    if (editor->profiler_recording && (!requested || !has_path || path_changed))
    {
      s_editor_profiler_stop(editor);
    }
    if (!requested || path_changed)
    {
      editor->profiler_failed = false;
    }
    editor->profiler_path = path;

    if (requested && running && !editor->profiler_recording &&
        !editor->profiler_failed)
    {
      XFSPath root = {0};
      if (!has_path ||
          !x_fs_path_dirname(&editor->project.game_dll_path, &root) ||
          !x_fs_path_join(&root, ".ldk") ||
          !ldk_profiler_initialize(root.buf) ||
          !ldk_profiler_capture_start(path.buf))
      {
        editor->profiler_failed = true;
        ldki_editor_log_error(editor, "Failed to start Play profiling.");
      }
      else
      {
        editor->profiler_recording = true;
      }
    }
    if (editor->profiler_recording && running)
    {
      ldk_profiler_capture_collect(true);
      ldk_profiler_frame_begin();
      editor->profiler_frame_open = true;
      ldk_profiler_zone_begin_source(&update_source, LDK_PROFILER_ZONE_PHASE,
          "Update", __FILE__, __func__, __LINE__);
    }
    return;
  }

  if (!editor->profiler_frame_open)
  {
    if (editor->profiler_recording &&
        !s_editor_profiler_session_requested(editor))
    {
      s_editor_profiler_stop(editor);
    }
    return;
  }

  switch (event->frame_event.type)
  {
  case LDK_FRAME_EVENT_UPDATE_AFTER:
  case LDK_FRAME_EVENT_SUBMIT_AFTER:
    ldk_profiler_zone_end_kind(LDK_PROFILER_ZONE_PHASE);
    break;
  case LDK_FRAME_EVENT_SUBMIT_BEFORE:
    ldk_profiler_zone_begin_source(&submit_source, LDK_PROFILER_ZONE_PHASE,
        "Submit", __FILE__, __func__, __LINE__);
    break;
  case LDK_FRAME_EVENT_RENDER_BEFORE:
    ldk_profiler_zone_begin_source(&render_source, LDK_PROFILER_ZONE_PHASE,
        "Render", __FILE__, __func__, __LINE__);
    break;
  case LDK_FRAME_EVENT_RENDER_AFTER:
    ldk_profiler_zone_end_kind(LDK_PROFILER_ZONE_PHASE);
    ldk_profiler_frame_end();
    editor->profiler_frame_open = false;
    ldk_profiler_capture_collect(false);
    if (!s_editor_profiler_session_requested(editor))
    {
      s_editor_profiler_stop(editor);
    }
    break;
  default:
    break;
  }
}

static void s_editor_game_only_update(LDKEditorContext *editor)
{
  bool active = editor->exclusive_mode &&
      editor->editor_state == LDK_EDITOR_STATE_PLAYING;
  LDKECS *ecs = ldk_module_get(LDK_MODULE_ECS);
  LDKCamera *camera = ecs &&
      ldk_entity_is_alive(&ecs->entity, editor->editor_camera)
      ? (LDKCamera *)ldk_ecs_component_get(
            editor->editor_camera, LDK_COMPONENT_TYPE_CAMERA)
      : NULL;

  if (active && !editor->game_only_active)
  {
    editor->game_only_previous_present_game = editor->renderer->present_game;
    editor->game_only_previous_camera_enabled = camera && camera->enabled;
    editor->game_only_previous_width = editor->renderer->game_width;
    editor->game_only_previous_height = editor->renderer->game_height;
    editor->text_input_state.codepoint_count = 0;
  }
  else if (!active && editor->game_only_active)
  {
    editor->renderer->present_game = editor->game_only_previous_present_game;
    (void)ldk_engine_render_resolution_set(
        (i32)editor->game_only_previous_width,
        (i32)editor->game_only_previous_height);
    if (camera)
    {
      camera->enabled = editor->game_only_previous_camera_enabled;
    }
    ldk_input_game_view_clear();
  }
  editor->game_only_active = active;
  if (active)
  {
    LDKSize size = ldk_os_window_client_area_size_get(editor->window);
    editor->renderer->present_game = true;
    if (camera)
    {
      camera->enabled = false;
    }
    if (size.w > 0 && size.h > 0)
    {
      (void)ldk_engine_render_resolution_set(size.w, size.h);
      ldk_input_game_view_set(0.0f, 0.0f, (float)size.w, (float)size.h,
          editor->renderer->game_width, editor->renderer->game_height);
    }
  }
}

static bool on_event_frame(const LDKEvent *event, void *state)
{
  LDKEditorContext *editor = (LDKEditorContext *)state;
  if (event->type != LDK_EVENT_TYPE_FRAME)
    return false;

  /* Keep the profiler frame open through editor work performed after the
   * engine render phase. */
  if (event->frame_event.type != LDK_FRAME_EVENT_RENDER_AFTER)
  {
    s_editor_profiler_frame_event(editor, event);
  }

  if (event->frame_event.type == LDK_FRAME_EVENT_UPDATE_AFTER)
  {
    if (editor->editor_state != LDK_EDITOR_STATE_STOPED &&
        !ldk_game_instance_is_started())
    {
      s_editor_state_set_stop(editor);
    }
    else if (editor->editor_state == LDK_EDITOR_STATE_STEPPING &&
             !ldk_game_instance_is_stepping())
    {
      editor->editor_state = LDK_EDITOR_STATE_PAUSED;
    }

    s_editor_game_only_update(editor);
    if (editor->game_only_active)
    {
      return false;
    }
    s_editor_camera_ensure(editor);

    LDK_PROFILE_BEGIN("Editor Camera Update");
    ldki_editor_camera_update(editor, event->frame_event.delta_time);
    LDK_PROFILE_END();

    LDK_PROFILE_BEGIN("Editor Gizmo Update");
    ldki_editor_gizmo_update(editor);
    LDK_PROFILE_END();
    return false;
  }

  if (event->frame_event.type == LDK_FRAME_EVENT_RENDER_AFTER)
  {
    if (editor->game_only_active)
    {
      s_editor_profiler_frame_event(editor, event);
      return false;
    }

    LDK_PROFILE_BEGIN("Editor Project Actions");
    s_editor_project_action_process(editor);
    LDK_PROFILE_END();

    LDK_PROFILE_BEGIN("Editor Game Module Watch");
    s_project_game_module_watch_update(editor);
    LDK_PROFILE_END();

    if (editor->create_project_window_close_requested)
    {
      ldki_editor_window_remove(LDK_EDITOR_WINDOW_CREATE_PROJECT);
      editor->create_project_window_show = false;
      editor->create_project_window_close_requested = false;
    }

    if (editor->create_project_window_open_requested &&
        !editor->create_project_window_show)
    {
      LDKEditorWindow window = {.id = LDK_EDITOR_WINDOW_CREATE_PROJECT,
          .title = "Create Project",
          .function = ldki_editor_project_create_window,
          .data = NULL};

      if (ldk_editor_window_add((LDKEditor *)editor, &window))
      {
        editor->create_project_window_show = true;
      }
      else
      {
        ldki_editor_log_error(editor, "Failed to open Create Project window.");
      }

      editor->create_project_window_open_requested = false;
    }

    s_editor_profiler_frame_event(editor, event);
    return false;
  }

  if (event->frame_event.type != LDK_FRAME_EVENT_SUBMIT_AFTER)
    return false;

  // TODO: Replace this by widow event listener
  LDKSize size = ldk_os_window_client_area_size_get(editor->window);

  s_editor_game_only_update(editor);
  if (editor->game_only_active)
  {
    if (editor->show_statistics)
    {
      s_editor_statistics_only(
          editor, size.w, size.h, event->frame_event.delta_time);
    }
    return true;
  }
  s_editor_update(editor, size.w, size.h, event->frame_event.delta_time);

  LDK_PROFILE_BEGIN("Editor Grid Submit");
  ldki_editor_grid_submit(editor);
  LDK_PROFILE_END();

  LDK_PROFILE_BEGIN("Editor Gizmo Submit");
  ldki_editor_gizmo_submit(editor);
  LDK_PROFILE_END();
  return true;
}

static bool on_event_window(const LDKEvent *event, void *state)
{
  LDKEditorContext *editor = (LDKEditorContext *)state;

  if (event->window_event.type == LDK_WINDOW_EVENT_CLOSE)
  {
    if (editor->game_only_active)
    {
      s_editor_state_set_stop(editor);
      s_editor_game_only_update(editor);
    }
    ldki_editor_confirm_quit(editor);
    return true; // Do not propagate this message further
  }
  return false;
}

static void s_editor_set_title(LDKEditorContext *editor)
{
  XSmallstr title;
  x_smallstr_format(&title, "LDK Engine v%d.%d.%d - %s", LDK_VERSION_MAJOR,
      LDK_VERSION_MINOR, LDK_VERSION_PATCH,
      editor->project.loaded ? editor->project.name.buf : "<NO PROJECT>");
  ldk_os_window_title_set(editor->window, title.buf);
}

// Clone entity
static bool s_editor_entity_component_copy(LDKECS *ecs, LDKEntity source,
    LDKEntity duplicate, u32 component_type)
{
  LDKRegisteredComponent registered = {0};
  const void *source_component;
  const void *initial_value;
  XArray *snapshot;
  bool result;

  if (!ecs || component_type == LDK_COMPONENT_TYPE_TRANSFORM ||
      !ecs->component.table ||
      !x_hashtable_u32_registered_component_get(
          ecs->component.table, component_type, &registered) ||
      registered.desc.entry_size == 0)
  {
    return component_type == LDK_COMPONENT_TYPE_TRANSFORM;
  }

  source_component = ldk_ecs_component_get_const(source, component_type);
  if (!source_component)
  {
    return false;
  }

  /*
   * Adding a component can grow its packed store and invalidate a pointer to
   * the source component. Snapshot it before calling ldk_ecs_component_add().
   * Component attach callbacks still receive an ordinary initial_value and
   * can perform any component-specific deep copy they require.
   */
  snapshot = x_array_create(registered.desc.entry_size, 1);
  if (!snapshot)
  {
    return false;
  }

  if (x_array_add(snapshot, (void *)source_component) != XARRAY_OK)
  {
    x_array_destroy(snapshot);
    return false;
  }

  initial_value = x_array_get(snapshot, 0);
  result = initial_value != NULL &&
           ldk_ecs_component_add(duplicate, component_type, initial_value) !=
               NULL;
  x_array_destroy(snapshot);
  return result;
}

static bool s_editor_entity_duplicate_data(LDKECS *ecs, LDKEntity source,
    LDKEntity duplicate, LDKEntity parent)
{
  const LDKEntityInfo *source_info;
  const LDKTransform *source_transform;
  u32 component_types[LDK_ENTITY_MAX_COMPONENTS] = {0};
  char source_name[LDK_ENTITY_NAME_MAX_LEN] = {0};
  u32 component_count;
  u16 source_flags;
  Vec3 local_position;
  Quat local_rotation;
  Vec3 local_scale;
  const char *name;

  if (!ecs)
  {
    return false;
  }

  source_info = ldk_entity_info_get(&ecs->entity, source);
  source_transform = ldk_entity_transform_get_const(
      &ecs->entity, &ecs->component, source);
  if (!source_info || !source_transform)
  {
    return false;
  }

  component_count = source_info->components.component_count;
  for (u32 i = 0; i < component_count; ++i)
  {
    component_types[i] = source_info->components.component_type[i];
  }

  name = ldk_ecs_entity_name_get(source);
  if (name)
  {
    snprintf(source_name, sizeof(source_name), "%s", name);
  }

  source_flags = ldk_entity_flags_get(&ecs->entity, source);
  local_position = source_transform->local_position;
  local_rotation = source_transform->local_rotation;
  local_scale = source_transform->local_scale;

  /*
   * Preserve authored local TRS. Do not use ldk_scenegraph_set_parent() here:
   * that operation preserves world space and would rewrite the copied local
   * transform.
   */
  if ((source_name[0] != 0 &&
          !ldk_ecs_entity_name_set(duplicate, source_name)) ||
      !ldk_transform_set_local_position(duplicate, local_position) ||
      !ldk_transform_set_local_rotation(duplicate, local_rotation) ||
      !ldk_transform_set_local_scale(duplicate, local_scale) ||
      (!x_handle_is_null(parent) &&
          !ldk_transform_set_parent(duplicate, parent)))
  {
    return false;
  }

  ldk_entity_flags_set(&ecs->entity, duplicate, source_flags);

  for (u32 i = 0; i < component_count; ++i)
  {
    if (!s_editor_entity_component_copy(
            ecs, source, duplicate, component_types[i]))
    {
      return false;
    }
  }

  return true;
}

static bool s_editor_entity_duplicate_recursive(LDKECS *ecs,
    LDKEntity source, LDKEntity parent, XArray *sources, XArray *duplicates,
    LDKEntity *out_duplicate)
{
  LDKEntity duplicate;
  const LDKTransform *source_transform;
  LDKEntity child;
  LDKEntity last_child = x_handle_null();

  if (!ecs || !sources || !duplicates ||
      !ldk_entity_is_alive(&ecs->entity, source) ||
      ldk_entity_internal_flags_has(
          &ecs->entity, source, LDK_ENTITY_INTERNAL_EDITOR))
  {
    return false;
  }

  duplicate = ldk_ecs_entity_create();
  if (x_handle_is_null(duplicate))
  {
    return false;
  }

  if (x_array_add(sources, &source) != XARRAY_OK)
  {
    ldk_ecs_entity_destroy(duplicate);
    return false;
  }

  if (x_array_add(duplicates, &duplicate) != XARRAY_OK)
  {
    x_array_pop(sources);
    ldk_ecs_entity_destroy(duplicate);
    return false;
  }

  if (!s_editor_entity_duplicate_data(ecs, source, duplicate, parent))
  {
    return false;
  }

  if (out_duplicate)
  {
    *out_duplicate = duplicate;
  }

  source_transform = ldk_entity_transform_get_const(
      &ecs->entity, &ecs->component, source);
  if (!source_transform)
  {
    return false;
  }

  child = source_transform->first_child;
  while (!x_handle_is_null(child))
  {
    const LDKTransform *child_transform;

    if (!ldk_entity_is_alive(&ecs->entity, child))
    {
      return false;
    }

    child_transform = ldk_entity_transform_get_const(
        &ecs->entity, &ecs->component, child);
    if (!child_transform)
    {
      return false;
    }

    last_child = child;
    child = child_transform->next_sibling;
  }

  /*
   * ldk_transform_set_parent() inserts at first_child. Duplicate siblings from
   * last to first so the authored hierarchy order remains unchanged.
   */
  child = last_child;
  while (!x_handle_is_null(child))
  {
    const LDKTransform *child_transform =
        ldk_entity_transform_get_const(
            &ecs->entity, &ecs->component, child);
    LDKEntity previous_child;

    if (!child_transform)
    {
      return false;
    }

    previous_child = child_transform->prev_sibling;
    if (!ldk_entity_internal_flags_has(
            &ecs->entity, child, LDK_ENTITY_INTERNAL_EDITOR) &&
        !s_editor_entity_duplicate_recursive(
            ecs, child, duplicate, sources, duplicates, NULL))
    {
      return false;
    }

    child = previous_child;
  }

  return true;
}

static bool s_editor_entity_duplicate_references(LDKGame *game, LDKECS *ecs,
    XArray *sources, XArray *duplicates)
{
  u32 pair_count;

  if (!game || !ecs || !sources || !duplicates)
  {
    return true;
  }

  pair_count = x_array_count(sources);
  if (pair_count != x_array_count(duplicates))
  {
    return false;
  }

  for (u32 i = 0; i < pair_count; ++i)
  {
    LDKEntity *duplicate = x_array_get(duplicates, i);
    const LDKEntityInfo *duplicate_info;
    u32 component_types[LDK_ENTITY_MAX_COMPONENTS] = {0};
    u32 component_count;

    if (!duplicate)
    {
      return false;
    }

    duplicate_info = ldk_entity_info_get(&ecs->entity, *duplicate);
    if (!duplicate_info)
    {
      return false;
    }

    component_count = duplicate_info->components.component_count;
    for (u32 component_i = 0; component_i < component_count; ++component_i)
    {
      component_types[component_i] =
          duplicate_info->components.component_type[component_i];
    }

    for (u32 component_i = 0; component_i < component_count; ++component_i)
    {
      u32 component_type = component_types[component_i];
      const LDKComponentMeta *meta;
      LDKRegisteredComponent registered = {0};
      void *component;

      if (component_type == LDK_COMPONENT_TYPE_TRANSFORM)
      {
        continue;
      }

      meta = ldk_scene_component_meta_find_by_type(game, component_type);
      if (!meta)
      {
        continue;
      }

      if (!ecs->component.table ||
          !x_hashtable_u32_registered_component_get(
              ecs->component.table, component_type, &registered) ||
          registered.desc.entry_size == 0)
      {
        return false;
      }

      component = ldk_ecs_component_get(*duplicate, component_type);
      if (!component)
      {
        return false;
      }

      for (u32 field_i = 0; field_i < meta->field_count; ++field_i)
      {
        const LDKComponentFieldMeta *field = &meta->fields[field_i];
        LDKEntity *entity_ref;

        if (field->type != LDK_FIELD_ENTITY ||
            field->offset > registered.desc.entry_size ||
            sizeof(LDKEntity) > registered.desc.entry_size - field->offset)
        {
          continue;
        }

        entity_ref = (LDKEntity *)((u8 *)component + field->offset);
        if (x_handle_is_null(*entity_ref))
        {
          continue;
        }

        for (u32 pair_i = 0; pair_i < pair_count; ++pair_i)
        {
          LDKEntity *mapped_source = x_array_get(sources, pair_i);
          LDKEntity *mapped_duplicate = x_array_get(duplicates, pair_i);

          if (mapped_source && mapped_duplicate &&
              ldki_editor_entity_equal(*entity_ref, *mapped_source))
          {
            *entity_ref = *mapped_duplicate;
            break;
          }
        }
      }
    }
  }

  return true;
}

static void s_editor_entity_duplicate_rollback(
    LDKECS *ecs, XArray *duplicates)
{
  if (!ecs || !duplicates)
  {
    return;
  }

  for (u32 i = x_array_count(duplicates); i > 0; --i)
  {
    LDKEntity *duplicate = x_array_get(duplicates, i - 1u);
    if (duplicate && ldk_entity_is_alive(&ecs->entity, *duplicate))
    {
      ldk_ecs_entity_destroy(*duplicate);
    }
  }
}

static bool s_editor_selected_entity_duplicate(LDKEditorContext *editor)
{
  LDKECS *ecs;
  LDKEntity source = x_handle_null();
  LDKEntity duplicate = x_handle_null();
  const LDKTransform *source_transform;
  LDKEntity source_parent;
  XArray *sources;
  XArray *duplicates;
  bool result;

  if (!editor || !editor->project.loaded ||
      editor->editor_state != LDK_EDITOR_STATE_STOPED ||
      editor->current_scene_path.length == 0)
  {
    return false;
  }

  ecs = ldk_module_get(LDK_MODULE_ECS);
  if (!ecs || !ldki_editor_selected_entity_get(editor, ecs, &source) ||
      ldk_entity_internal_flags_has(
          &ecs->entity, source, LDK_ENTITY_INTERNAL_EDITOR))
  {
    return false;
  }

  source_transform = ldk_entity_transform_get_const(
      &ecs->entity, &ecs->component, source);
  if (!source_transform)
  {
    return false;
  }
  source_parent = source_transform->parent;

  sources = x_array_create(sizeof(LDKEntity), 16);
  duplicates = x_array_create(sizeof(LDKEntity), 16);
  if (!sources || !duplicates)
  {
    if (sources)
    {
      x_array_destroy(sources);
    }
    if (duplicates)
    {
      x_array_destroy(duplicates);
    }
    ldki_editor_log_error(editor, "Failed to duplicate selected entity.");
    return false;
  }

  result = s_editor_entity_duplicate_recursive(
      ecs, source, source_parent, sources, duplicates, &duplicate);
  if (result)
  {
    result = s_editor_entity_duplicate_references(
        ldk_game_get(), ecs, sources, duplicates);
  }
  if (result)
  {
    result = ldk_scenegraph_update_entity(duplicate);
  }

  if (!result)
  {
    s_editor_entity_duplicate_rollback(ecs, duplicates);
    ldki_editor_log_error(editor, "Failed to duplicate selected entity.");
  }
  else
  {
    editor->selected_entity = duplicate;
    editor->selected_system_id = 0;
  }

  x_array_destroy(duplicates);
  x_array_destroy(sources);
  return result;
}

bool ldki_editor_view_texture_show(LDKEditorContext *editor,
    LDKUITextureHandle texture, LDKUIId panel_id, LDKUIId image_id,
    LDKUIRect *out_image_rect)
{
  LDKUIContext *ui = &editor->ui;

  if (ui->current_layout == NULL ||
      editor->renderer->game_width == 0 ||
      editor->renderer->game_height == 0)
  {
    return false;
  }

  LDKUIRect content_rect = ui->current_layout->content_rect;
  float content_bottom = content_rect.y + content_rect.h;
  content_rect.y = ui->current_layout->cursor.y;
  content_rect.h = content_bottom - content_rect.y;

  if (content_rect.w <= 0.0f || content_rect.h <= 0.0f)
  {
    return false;
  }

  rgba32 panel_bg = ui->theme.colors[LDK_UI_COLOR_PANEL_BG];
  ui->theme.colors[LDK_UI_COLOR_PANEL_BG] = 0x000000FFu;
  ldk_ui_widget_panel(ui, panel_id, content_rect);
  ui->theme.colors[LDK_UI_COLOR_PANEL_BG] = panel_bg;

  if (texture == (LDKUITextureHandle)LDK_RHI_INVALID_RESOURCE)
  {
    return false;
  }

  float game_aspect = (float)editor->renderer->game_width /
                      (float)editor->renderer->game_height;
  float content_aspect = content_rect.w / content_rect.h;
  LDKUIRect image_rect = content_rect;

  if (content_aspect > game_aspect)
  {
    image_rect.w = content_rect.h * game_aspect;
    image_rect.x += (content_rect.w - image_rect.w) * 0.5f;
  }
  else
  {
    image_rect.h = content_rect.w / game_aspect;
    image_rect.y += (content_rect.h - image_rect.h) * 0.5f;
  }

  ldk_ui_widget_image(ui, image_id, texture,
      ldk_renderer_view_texture_uv_get(editor->renderer), image_rect);

  if (out_image_rect != NULL)
  {
    *out_image_rect = image_rect;
  }

  return true;
}

static void s_editor_game_statistics_overlay(
    LDKEditorContext *editor, LDKUIRect image_rect)
{
  if (editor == NULL || editor->renderer == NULL ||
      !editor->show_statistics || image_rect.w <= 0.0f ||
      image_rect.h <= 0.0f)
  {
    return;
  }

  LDKUIContext *ui = &editor->ui;
  LDKRendererFrameStats stats =
      ldk_renderer_last_frame_stats_get(editor->renderer);

  const float margin = 12.0f;
  const float padding = 8.0f;
  const float row_height = 18.0f;
  const float desired_width = 276.0f;
  const float value_column_width = 80.0f;
  const float column_gap = 8.0f;
  const u32 row_count = 21;

  float panel_width = desired_width;
  float max_width = image_rect.w - margin * 2.0f;

  if (panel_width > max_width)
  {
    panel_width = max_width;
  }

  if (panel_width < 220.0f)
  {
    return;
  }

  float panel_height = padding * 2.0f + row_height * (float)row_count;
  float max_height = image_rect.h - margin * 2.0f;

  if (panel_height > max_height)
  {
    panel_height = max_height;
  }

  if (panel_height < padding * 2.0f + row_height * 4.0f)
  {
    return;
  }

  LDKUIRect panel_rect = {
      image_rect.x + margin,
      image_rect.y + margin,
      panel_width,
      panel_height};

  rgba32 previous_panel_bg =
      ui->theme.colors[LDK_UI_COLOR_PANEL_BG];
  rgba32 previous_text =
      ui->theme.colors[LDK_UI_COLOR_TEXT];

  ui->theme.colors[LDK_UI_COLOR_PANEL_BG] = 0x101010C0u;
  ui->theme.colors[LDK_UI_COLOR_TEXT] = 0xffffffffu;

  ldk_ui_widget_panel(ui, 0x53544154u, panel_rect);

  float label_width =
      panel_rect.w - padding * 2.0f - column_gap - value_column_width;

  float value_x =
      panel_rect.x + padding + label_width + column_gap;

  char value_text[32];
  u32 row = 0;

  {
    LDKUIRect title_rect = {
        panel_rect.x + padding,
        panel_rect.y + padding,
        panel_rect.w - padding * 2.0f,
        row_height};

    ldk_ui_widget_label(
        ui, 0x53540000u, "Statistics", title_rect);

    ++row;
  }

#define LDK_EDITOR_STAT_ROW(label, ...)                                      \
  do                                                                         \
  {                                                                          \
    float row_y =                                                            \
        panel_rect.y + padding + row_height * (float)row;                    \
                                                                             \
    if (row_y + row_height <=                                                \
        panel_rect.y + panel_rect.h - padding)                               \
    {                                                                        \
      LDKUIRect label_rect = {                                               \
          panel_rect.x + padding,                                            \
          row_y,                                                             \
          label_width,                                                       \
          row_height};                                                       \
                                                                             \
      LDKUIRect value_rect = {                                               \
          value_x,                                                           \
          row_y,                                                             \
          value_column_width,                                                \
          row_height};                                                       \
                                                                             \
      snprintf(value_text, sizeof(value_text), __VA_ARGS__);                 \
                                                                             \
      ldk_ui_widget_label(                                                   \
          ui, 0x53541000u + row * 2u, label, label_rect);                   \
      ldk_ui_widget_label(                                                   \
          ui, 0x53541001u + row * 2u, value_text, value_rect);              \
    }                                                                        \
                                                                             \
    ++row;                                                                   \
  } while (0)

  float fps = editor->statistics_frame_time_ms > 0.0f
      ? 1000.0f / editor->statistics_frame_time_ms
      : 0.0f;

  LDK_EDITOR_STAT_ROW(
      "FPS",
      "%.1f",
      fps);

  LDK_EDITOR_STAT_ROW(
      "Frame (ms)",
      "%.2f",
      editor->statistics_frame_time_ms);

  LDK_EDITOR_STAT_ROW(
      "Renderer CPU (ms)",
      "%.2f",
      stats.cpu_time_ms);

  LDK_EDITOR_STAT_ROW(
      "Buffer +/-",
      "%u / %u",
      stats.rhi.buffer_create_count,
      stats.rhi.buffer_destroy_count);

  LDK_EDITOR_STAT_ROW(
      "Texture +/-",
      "%u / %u",
      stats.rhi.texture_create_count,
      stats.rhi.texture_destroy_count);

  LDK_EDITOR_STAT_ROW(
      "FBO +/-",
      "%u / %u",
      stats.rhi.framebuffer_create_count,
      stats.rhi.framebuffer_destroy_count);

  LDK_EDITOR_STAT_ROW(
      "Buffer updates",
      "%u",
      stats.rhi.buffer_update_count);

  LDK_EDITOR_STAT_ROW(
      "Buffer upload",
      "%.1f KB",
      (double)stats.rhi.buffer_update_bytes / 1024.0);

  LDK_EDITOR_STAT_ROW(
      "  Uniform",
      "%u / %.1f KB",
      stats.rhi.uniform_buffer_update_count,
      (double)stats.rhi.uniform_buffer_update_bytes / 1024.0);

  LDK_EDITOR_STAT_ROW(
      "  Vertex",
      "%u / %.1f KB",
      stats.rhi.vertex_buffer_update_count,
      (double)stats.rhi.vertex_buffer_update_bytes / 1024.0);

  LDK_EDITOR_STAT_ROW(
      "  Index",
      "%u / %.1f KB",
      stats.rhi.index_buffer_update_count,
      (double)stats.rhi.index_buffer_update_bytes / 1024.0);

  LDK_EDITOR_STAT_ROW(
      "  Instance",
      "%u / %.1f KB",
      stats.rhi.instance_buffer_update_count,
      (double)stats.rhi.instance_buffer_update_bytes / 1024.0);

  LDK_EDITOR_STAT_ROW(
      "  Other",
      "%u / %.1f KB",
      stats.rhi.other_buffer_update_count,
      (double)stats.rhi.other_buffer_update_bytes / 1024.0);

  LDK_EDITOR_STAT_ROW(
      "Game draw calls",
      "%u",
      stats.game.draw_call_count);

  LDK_EDITOR_STAT_ROW(
      "Editor draw calls",
      "%u",
      stats.non_game.draw_call_count);

  LDK_EDITOR_STAT_ROW(
      "Opaque items",
      "%u",
      stats.game.opaque_mesh_render_count);

  LDK_EDITOR_STAT_ROW(
      "Overlay items",
      "%u",
      stats.game.overlay_mesh_render_count);

  LDK_EDITOR_STAT_ROW(
      "Batches",
      "%u",
      stats.game.batch_count);

  LDK_EDITOR_STAT_ROW(
      "Instanced batches",
      "%u",
      stats.game.instanced_batch_count);

  LDK_EDITOR_STAT_ROW(
      "Instanced objects",
      "%u",
      stats.game.instanced_instance_count);

  LDK_EDITOR_STAT_ROW(
      "Max batch",
      "%u",
      stats.game.max_batch_size);

  LDK_EDITOR_STAT_ROW(
      "  Opaque mesh",
      "%u",
      stats.game.opaque_mesh_draw_call_count);

  LDK_EDITOR_STAT_ROW(
      "  Shadow",
      "%u",
      stats.game.shadow_draw_call_count);

  LDK_EDITOR_STAT_ROW(
      "  Overlay mesh",
      "%u",
      stats.game.overlay_mesh_draw_call_count);

#undef LDK_EDITOR_STAT_ROW

  ui->theme.colors[LDK_UI_COLOR_TEXT] = previous_text;
  ui->theme.colors[LDK_UI_COLOR_PANEL_BG] = previous_panel_bg;
}

static void s_editor_game_window(LDKEditor *opaque_editor, void *data)
{
  LDKEditorContext *editor = (LDKEditorContext *)opaque_editor;
  LDKUITextureHandle game_texture =
      ldk_renderer_game_texture_get(editor->renderer);
  LDKUIRect image_rect;
  (void)data;

  if (!ldki_editor_view_texture_show(editor, game_texture,
          0x47414D42u, 0x47414D45u, &image_rect))
  {
    return;
  }

  s_editor_game_statistics_overlay(editor, image_rect);

  ldk_input_game_view_set(image_rect.x * editor->ui_frame_scale,
      image_rect.y * editor->ui_frame_scale,
      image_rect.w * editor->ui_frame_scale,
      image_rect.h * editor->ui_frame_scale, editor->renderer->game_width,
      editor->renderer->game_height);
}

static void s_editor_inspector_content_window(
    LDKEditor *opaque_editor, void *data)
{
  (void)data;
  ldki_editor_inspector_show((LDKEditorContext *)opaque_editor);
}

static void s_editor_hierarchy_window(LDKEditor *opaque_editor, void *data)
{
  (void)data;
  LDKECS *ecs = ldk_module_get(LDK_MODULE_ECS);
  ldk_editor_hierarchy_show(opaque_editor, ecs);
}

//----------------------------------------------------------
// Editor Udpate
//----------------------------------------------------------

static void s_editor_profiler_window(LDKEditor *opaque_editor, void *data)
{
  (void)data;
  ldk_editor_profiler_show(opaque_editor);
}

static void s_editor_statistics_time_update(
    LDKEditorContext *editor, float delta_time)
{
  if (delta_time > 0.0f)
  {
    float frame_time_ms = delta_time * 1000.0f;
    float alpha = delta_time * 6.0f;
    if (alpha > 1.0f)
    {
      alpha = 1.0f;
    }

    if (editor->statistics_frame_time_ms <= 0.0f)
    {
      editor->statistics_frame_time_ms = frame_time_ms;
    }
    else
    {
      editor->statistics_frame_time_ms +=
          (frame_time_ms - editor->statistics_frame_time_ms) * alpha;
    }
  }
}

static void s_editor_statistics_only(LDKEditorContext *editor, i32 window_width,
    i32 window_height, float delta_time)
{
  editor->ui_frame_scale =
      s_editor_ui_scale_validated(editor->editor_ui_scale);
  LDKUIRect viewport = {0.0f, 0.0f,
      (float)window_width / editor->ui_frame_scale,
      (float)window_height / editor->ui_frame_scale};
  LDKMouseState mouse_state = {0};
  LDKKeyboardState keyboard_state = {0};
  LDKUITextInputState text_state = {0};
  s_editor_statistics_time_update(editor, delta_time);
  ldk_ui_begin_frame(&editor->ui, delta_time, &mouse_state, &keyboard_state,
      &text_state, viewport);
  s_editor_game_statistics_overlay(editor, viewport);
  ldk_ui_end_frame(&editor->ui);
  ldk_renderer_submit_ui(editor->renderer,
      ldk_ui_get_render_data(&editor->ui));
}

static void s_draw_editor_ui(LDKEditorContext *editor, float delta_time)
{
  LDK_PROFILE_BEGIN("Editor Profiler Update");
  ldki_editor_profiler_update();
  LDK_PROFILE_END();

  s_editor_statistics_time_update(editor, delta_time);

  LDK_PROFILE_BEGIN("Editor Window State Update");
  ldki_editor_file_explorer_update(editor);
  ldki_editor_console_update(editor);
  LDK_PROFILE_END();

  LDK_PROFILE_BEGIN("Editor Toolbar");
  ldki_editor_toolbar_show((LDKEditor *)editor);
  LDK_PROFILE_END();

  LDK_PROFILE_BEGIN("Editor Dock");
  ldk_editor_dock_update(editor);
  LDK_PROFILE_END();

  LDK_PROFILE_BEGIN("Editor Scene Catalog Sync");
  ldki_editor_scene_catalog_sync(editor);
  LDK_PROFILE_END();

  LDK_PROFILE_BEGIN("Editor Menubar");
  ldki_editor_menubar_show(editor);
  LDK_PROFILE_END();

  if (editor->show_input_window)
  {
    LDK_PROFILE_BEGIN("Editor Input Window");
    u32 input_result = ldki_editor_input_window(editor, "SAVE LAYOUT AS");

    if ((input_result & LDK_UI_INPUT_BOX_COMMITTED) != 0 &&
        ldki_editor_layout_save_as(editor))
    {
      editor->show_input_window = false;
    }
    LDK_PROFILE_END();
  }

  LDK_PROFILE_BEGIN("Editor Status");
  ldki_editor_status_show(editor);
  LDK_PROFILE_END();
}

static void s_editor_update(LDKEditorContext *editor, i32 window_width,
    i32 window_height, float delta_time)
{
  LDKMouseState mouse_state;
  LDKKeyboardState kbd_state;
  editor->ui_frame_scale =
      s_editor_ui_scale_validated(editor->editor_ui_scale);
  LDKUIRect ui_viewport = (LDKUIRect){.x = 0.0f,
      .y = 0.0f,
      .w = (float)window_width / editor->ui_frame_scale,
      .h = (float)window_height / editor->ui_frame_scale};
  ldki_editor_mouse_state_get(editor, &mouse_state);
  ldk_os_keyboard_state_get(&kbd_state);
  ldk_input_game_view_clear();
  ldki_editor_gizmo_begin_ui_frame(editor);

  LDK_PROFILE_BEGIN("Editor UI Begin Frame");
  ldk_ui_begin_frame(&editor->ui, delta_time, &mouse_state, &kbd_state,
      &editor->text_input_state, ui_viewport);
  LDK_PROFILE_END();

  if (ldk_os_mouse_button_down(&mouse_state, LDK_MOUSE_BUTTON_LEFT))
  {
    ldk_ui_drag_n_drop_payload_get_and_remove(NULL, NULL);
  }

  LDK_PROFILE_BEGIN("Editor UI Build");
  s_draw_editor_ui(editor, delta_time);
  LDK_PROFILE_END();

  if (!ldk_os_mouse_button_is_pressed(&mouse_state, LDK_MOUSE_BUTTON_LEFT))
  {
    ldk_ui_drag_n_drop_payload_get_and_remove(NULL, NULL);
  }

  LDK_PROFILE_BEGIN("Editor UI End Frame");
  ldk_ui_end_frame(&editor->ui);
  LDK_PROFILE_END();

  const LDKUIRenderData *ui_data = ldk_ui_get_render_data(&editor->ui);
  if (ui_data != NULL)
  {
    LDK_PROFILE_COUNTER_SET("Editor UI Vertices", ui_data->vertex_count);
    LDK_PROFILE_COUNTER_SET("Editor UI Indices", ui_data->index_count);
    LDK_PROFILE_COUNTER_SET("Editor UI Draw Commands", ui_data->command_count);
  }

  LDK_PROFILE_BEGIN("Editor UI Submit");
  ldk_renderer_submit_ui(ldk_module_get(LDK_MODULE_RENDERER), ui_data);
  LDK_PROFILE_END();

  editor->text_input_state.codepoint_count = 0;
}

//----------------------------------------------------------
// Editor Initialization
//----------------------------------------------------------

static bool s_editor_file_association_legacy_command_parse(
    const char *command, LDKEditorFileAssociation *association)
{
  const char *cursor;
  const char *begin;
  const char *end;
  size_t length;

  if (command == NULL || association == NULL)
  {
    return false;
  }

  cursor = command;
  while (*cursor != 0 && isspace((u8)*cursor))
  {
    ++cursor;
  }

  if (*cursor == 0)
  {
    return false;
  }

  if (*cursor == '"')
  {
    begin = ++cursor;
    while (*cursor != 0 && *cursor != '"')
    {
      ++cursor;
    }

    if (*cursor != '"')
    {
      return false;
    }

    end = cursor++;
  }
  else
  {
    begin = cursor;
    while (*cursor != 0 && !isspace((u8)*cursor))
    {
      ++cursor;
    }
    end = cursor;
  }

  length = (size_t)(end - begin);
  if (length == 0 || length >= sizeof(association->program))
  {
    return false;
  }

  memcpy(association->program, begin, length);
  association->program[length] = 0;

  while (*cursor != 0 && isspace((u8)*cursor))
  {
    ++cursor;
  }

  snprintf(association->arguments, sizeof(association->arguments), "%s",
      cursor);
  return true;
}

static LDKAssetFont s_editor_font_asset_load(
    LDKEditorContext *editor, const char *font_path)
{
  LDKAssetManager *asset_manager;
  LDKAssetFont result = ldk_asset_font_null();

  if (editor == NULL || font_path == NULL || font_path[0] == 0)
  {
    return result;
  }

  asset_manager = ldk_module_get(LDK_MODULE_ASSET_MANAGER);
  if (asset_manager == NULL)
  {
    return result;
  }

  if (!x_fs_path_is_absolute_cstr(font_path))
  {
    return ldk_asset_manager_font_load(asset_manager, font_path);
  }

  FILE *in = fopen(font_path, "rb");
  void *data = NULL;
  long file_size;

  if (in == NULL || fseek(in, 0, SEEK_END) != 0)
  {
    if (in != NULL)
    {
      fclose(in);
    }
    return result;
  }

  file_size = ftell(in);
  if (file_size <= 0 || (u64)file_size > UINT32_MAX ||
      fseek(in, 0, SEEK_SET) != 0)
  {
    fclose(in);
    return result;
  }

  data = malloc((size_t)file_size);
  if (data == NULL ||
      fread(data, 1, (size_t)file_size, in) != (size_t)file_size)
  {
    free(data);
    fclose(in);
    return result;
  }

  fclose(in);
  result = ldk_asset_manager_font_create(
      asset_manager, data, (u32)file_size);
  free(data);
  return result;
}

bool ldki_editor_font_apply(
    LDKEditorContext *editor, const char *font_path, i32 font_size)
{
  LDKAssetManager *asset_manager;
  LDKAssetFont new_font;
  LDKAssetFontData *font_data;
  LDKFontInstance *new_instance;
  LDKFontAtlasDesc font_atlas_desc = {0};
  XFSPath normalized_path = {0};

  if (editor == NULL || font_path == NULL || font_path[0] == 0 ||
      font_size < 6 || font_size > 96)
  {
    return false;
  }

  if (!x_fs_path_set(&normalized_path, font_path))
  {
    return false;
  }

  if (x_fs_path_is_absolute_cstr(normalized_path.buf))
  {
    x_fs_path_normalize(&normalized_path);
    if (!x_fs_path_is_file(&normalized_path))
    {
      return false;
    }
  }

  asset_manager = ldk_module_get(LDK_MODULE_ASSET_MANAGER);
  if (asset_manager == NULL)
  {
    return false;
  }

  new_font = s_editor_font_asset_load(editor, normalized_path.buf);
  font_data = ldk_asset_manager_font_get(asset_manager, new_font);
  if (font_data == NULL || font_data->face == NULL)
  {
    if (ldk_asset_manager_font_is_alive(asset_manager, new_font))
    {
      ldk_asset_manager_font_unload(asset_manager, new_font);
    }
    return false;
  }

  font_atlas_desc.padding = 1;
  font_atlas_desc.page_width = 256;
  font_atlas_desc.page_height = 256;
  new_instance = ldk_ttf_get_instance(
      font_data->face, (float)font_size, &font_atlas_desc);
  if (new_instance == NULL || !ldk_ttf_preload_basic_ascii(new_instance))
  {
    ldk_asset_manager_font_unload(asset_manager, new_font);
    return false;
  }

  editor->font = new_font;
  editor->font_instance = new_instance;
  editor->ui.font = new_instance;
  ldk_ui_windows_invalidate(&editor->ui);
  editor->editor_font = normalized_path;
  editor->editor_font_size = font_size;

  /* Keep the previous font asset alive until editor shutdown. UI draw commands
   * generated earlier in this frame may still reference its atlas textures,
   * and the renderer font cache keys those textures by font-instance pointer. */
  return true;
}

static bool s_editor_last_project_path_write(
    LDKEditorContext *editor, const XFSPath *project_path)
{
  XIni ini = {0};
  XIniError error = {0};
  const char *value;
  bool ok;

  if (editor == NULL || editor->editor_config_path.length == 0)
  {
    return false;
  }

  value = project_path != NULL ? project_path->buf : "";
  if (!x_ini_load_file(editor->editor_config_path.buf, &ini, &error))
  {
    return false;
  }

  ok = x_ini_set(&ini, ".editor", "last_project", value) &&
       x_ini_write_file(editor->editor_config_path.buf, &ini, &error);
  x_ini_free(&ini);
  return ok;
}

static void s_editor_last_project_path_update(LDKEditorContext *editor)
{
  if (editor == NULL || !editor->project.loaded)
  {
    return;
  }

  editor->last_project_path = editor->project.project_file_path;
  if (!s_editor_last_project_path_write(editor, &editor->last_project_path))
  {
    ldk_log_warning("Failed to persist the last opened project.\n");
  }
}

static bool s_editor_config_load_from_ini(
    LDKEditorContext *editor, XIni *ini, LDKConfig *config)
{
  LDK_ASSERT(editor);
  LDK_ASSERT(editor->renderer);
  LDK_ASSERT(!editor->initialized);

  // load .editor section
  const char *EDITOR = ".editor";
  const char *font_path;
  const char *last_project;
  rgba32 legacy_selection_color;

  editor->editor_font_size = x_ini_get_i32(ini, EDITOR, "font_size", 18);
  if (editor->editor_font_size < 6 || editor->editor_font_size > 96)
  {
    ldk_log_warning("Invalid .editor font_size. Falling back to 18.\n");
    editor->editor_font_size = 18;
  }

  editor->editor_ui_scale = x_ini_get_f32(ini, EDITOR, "ui_scale", 1.0f);
  if (!isfinite(editor->editor_ui_scale) || editor->editor_ui_scale < 0.5f ||
      editor->editor_ui_scale > 3.0f)
  {
    ldk_log_warning("Invalid .editor ui_scale. Falling back to 1.0.\n");
    editor->editor_ui_scale = 1.0f;
  }

  editor->debug_color = x_ini_get_u32(
      ini, EDITOR, "debug_color", LDK_EDITOR_DEBUG_COLOR_DEFAULT);

  legacy_selection_color = x_ini_get_u32(
      ini, EDITOR, "selection_color", LDK_EDITOR_SELECTION_COLOR_1_DEFAULT);
  editor->selection_color_1 = x_ini_get_u32(
      ini, EDITOR, "selection_color_1", legacy_selection_color);
  editor->selection_color_2 = x_ini_get_u32(
      ini, EDITOR, "selection_color_2", LDK_EDITOR_SELECTION_COLOR_2_DEFAULT);

  editor->debug_line_width = x_ini_get_f32(ini, EDITOR,
      "debug_line_width", LDK_EDITOR_DEBUG_LINE_WIDTH_DEFAULT);
  if (!isfinite(editor->debug_line_width) ||
      editor->debug_line_width < LDK_EDITOR_LINE_WIDTH_MIN ||
      editor->debug_line_width > LDK_EDITOR_LINE_WIDTH_MAX)
  {
    ldk_log_warning(
        "Invalid .editor debug_line_width. Falling back to 1.0.\n");
    editor->debug_line_width = LDK_EDITOR_DEBUG_LINE_WIDTH_DEFAULT;
  }

  editor->selection_line_width = x_ini_get_f32(ini, EDITOR,
      "selection_line_width", LDK_EDITOR_SELECTION_LINE_WIDTH_DEFAULT);
  if (!isfinite(editor->selection_line_width) ||
      editor->selection_line_width < LDK_EDITOR_LINE_WIDTH_MIN ||
      editor->selection_line_width > LDK_EDITOR_LINE_WIDTH_MAX)
  {
    ldk_log_warning(
        "Invalid .editor selection_line_width. Falling back to 1.0.\n");
    editor->selection_line_width = LDK_EDITOR_SELECTION_LINE_WIDTH_DEFAULT;
  }

  editor->selection_pulse_seconds = x_ini_get_f32(ini, EDITOR,
      "selection_pulse_seconds", LDK_EDITOR_SELECTION_PULSE_SECONDS_DEFAULT);
  if (!isfinite(editor->selection_pulse_seconds) ||
      editor->selection_pulse_seconds <
          LDK_EDITOR_SELECTION_PULSE_SECONDS_MIN ||
      editor->selection_pulse_seconds > LDK_EDITOR_SELECTION_PULSE_SECONDS_MAX)
  {
    ldk_log_warning(
        "Invalid .editor selection_pulse_seconds. Falling back to 3.0.\n");
    editor->selection_pulse_seconds =
        LDK_EDITOR_SELECTION_PULSE_SECONDS_DEFAULT;
  }

  editor->restore_last_project =
      x_ini_get_bool(ini, EDITOR, "restore_last_project", true);
  last_project = x_ini_get(ini, EDITOR, "last_project", "");
  if (last_project != NULL && last_project[0] != 0)
  {
    x_fs_path_set(&editor->last_project_path, last_project);
    x_fs_path_normalize(&editor->last_project_path);
  }

  x_smallstr_from_cstr(
      &editor->editor_theme, x_ini_get(ini, EDITOR, "theme", "dark"));
  font_path =
      x_ini_get(ini, EDITOR, "font", "assets/InterDisplay-Regular.ttf");
  if (font_path == NULL || font_path[0] == 0 ||
      !x_fs_path_set(&editor->editor_font, font_path))
  {
    ldk_log_error("Invalid .editor font path.\n");
    return false;
  }

  if (x_fs_path_is_absolute_cstr(editor->editor_font.buf))
  {
    x_fs_path_normalize(&editor->editor_font);
  }

  editor->editor_camera_fov =
      x_ini_get_f32(ini, EDITOR, "camera_fov", 60.0f);
  editor->editor_camera_near_clip =
      x_ini_get_f32(ini, EDITOR, "camera_near_clip", 0.1f);
  editor->editor_camera_far_clip =
      x_ini_get_f32(ini, EDITOR, "camera_far_clip", 1000.0f);
  editor->profile = x_ini_get_bool(ini, EDITOR, "profile", false);
  editor->show_statistics =
      x_ini_get_bool(ini, EDITOR, "show_statistics", false);
  editor->file_explorer_open_folders_single_click = x_ini_get_bool(
      ini, EDITOR, "file_explorer_open_folders_single_click", false);

  memset(editor->file_associations, 0, sizeof(editor->file_associations));
  editor->file_association_count = 0;

  const char *association_prefix = ".file_association.";
  size_t association_prefix_length = strlen(association_prefix);
  for (i32 section_i = 0; section_i < x_ini_section_count(ini) &&
                            editor->file_association_count <
                                LDK_EDITOR_FILE_ASSOCIATION_CAPACITY;
       ++section_i)
  {
    const char *section = x_ini_section_name(ini, section_i);
    if (section == NULL ||
        strncmp(section, association_prefix, association_prefix_length) != 0)
    {
      continue;
    }

    LDKEditorFileAssociation *association =
        &editor->file_associations[editor->file_association_count++];
    const char *fallback_name = section + association_prefix_length;
    const char *legacy_command;
    const char *program;

    snprintf(association->name, sizeof(association->name), "%s",
        x_ini_get(ini, section, "name", fallback_name));
    program = x_ini_get(ini, section, "program", "");
    snprintf(association->program, sizeof(association->program), "%s",
        program != NULL ? program : "");
    snprintf(association->arguments, sizeof(association->arguments), "%s",
        x_ini_get(ini, section, "arguments", ""));
    snprintf(association->extensions, sizeof(association->extensions), "%s",
        x_ini_get(ini, section, "extensions", ""));

    legacy_command = x_ini_get(ini, section, "command", NULL);
    if (association->program[0] == 0 && legacy_command != NULL)
    {
      s_editor_file_association_legacy_command_parse(
          legacy_command, association);
    }
  }

  if (editor->editor_camera_fov <= 1.0f ||
      editor->editor_camera_fov >= 179.0f)
  {
    ldk_log_warning(
        "Invalid .editor camera_fov. Falling back to 60 degrees.\n");
    editor->editor_camera_fov = 60.0f;
  }

  if (editor->editor_camera_near_clip <= 0.0f)
  {
    ldk_log_warning(
        "Invalid .editor camera_near_clip. Falling back to 0.1.\n");
    editor->editor_camera_near_clip = 0.1f;
  }

  if (editor->editor_camera_far_clip <= editor->editor_camera_near_clip)
  {
    ldk_log_warning(
        "Invalid .editor camera_far_clip. It must be greater than "
        "camera_near_clip.\n");
    editor->editor_camera_far_clip =
        float_max(1000.0f, editor->editor_camera_near_clip * 1000.0f);
  }

  // load a te texture atlas
  XFSPath atlas_path;

  x_fs_path(&atlas_path, config->runtree_path, "assets", "ui_atlas.png");
  LDKImage *image_atlas = ldk_image_create_from_memory(
      ldk_editor_icon_atlas_png, ldk_editor_icon_atlas_png_size);
  if (image_atlas == NULL)
  {
    ldk_log_error("Failed to load editor atas '%s'\n", atlas_path.buf);
    return false;
  }

  LDKRendererTextureOptions options = {0};
  ldk_renderer_texture_options_defaults(&options);
  options.generate_mipmaps = true;
  options.min_filter = LDK_RHI_FILTER_LINEAR;
  options.mag_filter = LDK_RHI_FILTER_LINEAR;
  options.mip_filter = LDK_RHI_FILTER_LINEAR;

  options.wrap_u = LDK_RHI_WRAP_REPEAT;
  options.wrap_v = LDK_RHI_WRAP_REPEAT;

  LDKResourceTexture texture_atlas = ldk_renderer_texture_create_from_image(
      ldk_module_get(LDK_MODULE_RENDERER), image_atlas, &options);

  if (ldk_renderer_texture_null().id == texture_atlas.id)
  {
    ldk_log_error(
        "Failed to create texture from image atas '%s'\n", atlas_path.buf);
    return false;
  }

  editor->ui_atlas = texture_atlas;
  editor->initialized = true;
  return true;
}

static bool s_editor_load_resources(LDKEditorContext *editor, LDKConfig *config)
{
  (void)config;
  LDK_ASSERT(editor);
  LDK_ASSERT(editor->initialized);

  if (!ldki_editor_font_apply(
          editor, editor->editor_font.buf, editor->editor_font_size))
  {
    ldk_log_error(
        "Failed to load editor font '%s'.\n", editor->editor_font.buf);
    return false;
  }

  return true;
}

static bool s_editor_gui_initialize(
    LDKEditorContext *editor, LDKRenderer *renderer)
{
  LDK_ASSERT(editor);

  // Editor UI Initialization
  LDKUIConfig ui_cfg = {0};
  ui_cfg.frame_arena_size = 1024 * 4;
  ui_cfg.initial_vertex_capacity = LDK_DEFAULT_UI_INITIAL_VERTEX_CAPACITY;
  ui_cfg.initial_index_capacity = LDK_DEFAULT_UI_INITIAL_INDEX_CAPACITY;
  ui_cfg.initial_command_capacity = LDK_DEFAULT_UI_INITIAL_COMMAND_CAPACITY;
  ui_cfg.initial_window_capacity = LDK_DEFAULT_UI_INITIAL_WINDOW_CAPACITY;
  ui_cfg.initial_id_stack_capacity = LDK_DEFAULT_UI_INITIAL_STACK_CAPACITY;
  ui_cfg.font = editor->font_instance;

  ui_cfg.font_texture_user = renderer;
  ui_cfg.get_font_page_texture = ldk_renderer_get_font_page_texture_callback;

  if (!ldk_ui_initialize(&editor->ui, &ui_cfg))
  {
    ldk_log_error("Failed to initialize module: UI System.");
    return false;
  }

  ldki_editor_theme_icons_set(editor, &editor->ui.theme); // set theme icons
  return true;
}

//----------------------------------------------------------
// Play / Stop
//----------------------------------------------------------

static void s_editor_scene_missing_systems_report(
    LDKEditorContext *editor, const LDKSceneSystems *systems)
{
  LDKECS *ecs = ldk_module_get(LDK_MODULE_ECS);

  if (!editor || !ecs)
  {
    return;
  }

  for (u32 i = 0; i < systems->count; ++i)
  {
    u64 id = systems->ids[i];
    LDKSystemDesc desc = {0};

    if (!ldk_system_registry_find_by_id(&ecs->system, id, &desc))
    {
      char message[256];
      const char *name = "<unknown>";
      LDKGame *game = ldk_game_get();
      if (game && game->system_metadata_count && game->system_metadata_get)
      {
        for (u32 j = 0; j < game->system_metadata_count(); ++j)
        {
          const LDKSystemMeta *meta = game->system_metadata_get(j);
          if (meta && meta->id == id)
          {
            name = meta->name ? meta->name : name;
            break;
          }
        }
      }
      snprintf(message, sizeof(message),
          "Scene references system %s (0x%016llx), but the game did not "
          "register it. Skipping.",
          name, (unsigned long long)id);
      ldki_editor_console_append(
          editor, LDK_EDITOR_CONSOLE_ENTRY_ERROR, message);
    }
  }
}

/* Restore the direct editor scene after a session or failed start. */
static void s_editor_play_scene_restore(LDKEditorContext *editor)
{
  XFSPath path = {0};
  bool apply_pending = ldki_editor_scene_play_apply_available(editor);

  if (editor->current_scene_path.length)
  {
    x_fs_path(&path, editor->project.run_root_path.buf,
        editor->current_scene_path.buf);
    x_fs_path_normalize(&path);
    if (ldki_editor_scene_load(editor, &path))
    {
      if (apply_pending && !ldki_editor_scene_play_apply_restore(editor))
      {
        ldki_editor_log_error(
            editor, "Failed to apply Play changes to scene.");
      }
      else if (!apply_pending)
      {
        ldki_editor_scene_play_apply_discard(editor);
      }
      return;
    }
  }

  ldki_editor_scene_play_apply_discard(editor);
  ldki_editor_scene_clear(editor);
}

/** Enter PLAY using the currently loaded game module. */
static bool s_editor_state_enter_play(LDKEditorContext *editor)
{
  LDKECS *ecs;

  if (!editor || !editor->project.loaded)
  {
    return false;
  }

  if (editor->editor_state == LDK_EDITOR_STATE_PAUSED)
  {
    if (!ldk_game_instance_resume())
    {
      return false;
    }

    editor->editor_state = LDK_EDITOR_STATE_PLAYING;
    return true;
  }

  if (editor->editor_state == LDK_EDITOR_STATE_PLAYING)
  {
    return true;
  }

  if (editor->editor_state != LDK_EDITOR_STATE_STOPED)
  {
    return false;
  }

  ecs = ldk_module_get(LDK_MODULE_ECS);
  if (!ecs || !ecs->system.is_started)
  {
    return false;
  }

  LDKSceneManager *manager = ldk_module_get(LDK_MODULE_SCENE_MANAGER);
  if (editor->project.play_current_scene)
  {
    if (editor->current_scene_path.length == 0)
    {
      ldki_editor_log_error(editor, "Open a scene before Play Current Scene.");
      return false;
    }
    if (!ldki_editor_scene_play_apply_begin(editor))
    {
      ldki_editor_log_error(
          editor, "Failed to capture the authoring scene before Play.");
      return false;
    }
    /* Use the scene already open in the editor, including unsaved edits.
     * It remains independent of the project catalog. */
    if (!ldk_scene_manager_current_reset(manager))
    {
      s_editor_play_scene_restore(editor);
      return false;
    }
    s_editor_scene_missing_systems_report(
        editor, &editor->current_scene_systems);
    if (!ldk_scene_systems_start(&ecs->system, &editor->current_scene_systems))
    {
      ldk_scene_systems_stop_missing(&ecs->system, NULL);
      ldki_editor_log_error(editor, "Failed to initialize scene systems.");
      s_editor_play_scene_restore(editor);
      return false;
    }
  }
  else
  {
    LDKSceneResult result;
    ldki_editor_scene_play_apply_discard(editor);
    if (!ldk_scene_manager_load(manager, 0, &result))
    {
      ldki_editor_log_error(editor, result.error);
      /* A failed load may have cleared the ECS after its preflight. */
      s_editor_play_scene_restore(editor);
      return false;
    }
    s_editor_scene_missing_systems_report(editor, &manager->current_systems);
  }

  if (!ldk_game_instance_start())
  {
    ldk_scene_systems_stop_missing(&ecs->system, NULL);
    s_editor_play_scene_restore(editor);
    return false;
  }

  editor->editor_state = LDK_EDITOR_STATE_PLAYING;
  return true;
}

static bool s_editor_state_set_play(LDKEditorContext *editor)
{
  if (editor == NULL || !editor->project.loaded)
  {
    return false;
  }

  if (editor->editor_state == LDK_EDITOR_STATE_PAUSED ||
      editor->editor_state == LDK_EDITOR_STATE_PLAYING)
  {
    return s_editor_state_enter_play(editor);
  }

  if (editor->editor_state != LDK_EDITOR_STATE_STOPED ||
      editor->project_build.active ||
      editor->pending_project_action.type != LDK_EDITOR_PROJECT_ACTION_NONE)
  {
    return false;
  }

  if (editor->project.build_on_play ||
      !x_fs_path_is_file(&editor->project.game_dll_path))
  {
    return s_editor_project_build_request_with_continuation(
        editor, LDK_EDITOR_PROJECT_BUILD_CONTINUATION_PLAY);
  }

  if (ldk_game_get() == NULL && !s_project_game_module_load(editor))
  {
    ldki_editor_log_error(editor, "Failed to load the game module.");
    return false;
  }

  return s_editor_state_enter_play(editor);
}

static void s_editor_state_set_stop(LDKEditorContext *editor)
{
  bool restore_scene;

  if (!editor)
  {
    return;
  }

  if (editor->editor_state == LDK_EDITOR_STATE_STOPED &&
      !ldk_game_instance_is_started())
  {
    return;
  }

  restore_scene = editor->project.loaded &&
                  editor->current_scene_path.length != 0;
  if (!ldk_game_instance_stop())
  {
    ldki_editor_log_error(editor, "Failed to stop the game session.");
    return;
  }

  editor->editor_state = LDK_EDITOR_STATE_STOPED;

  if (restore_scene)
  {
    s_editor_play_scene_restore(editor);
  }
  else
  {
    ldki_editor_scene_play_apply_discard(editor);
    ldki_editor_scene_clear(editor);
  }
}

static void s_editor_state_set_pause(LDKEditorContext *editor)
{
  if (!editor || editor->editor_state != LDK_EDITOR_STATE_PLAYING)
  {
    return;
  }

  if (ldk_game_instance_pause())
  {
    editor->editor_state = LDK_EDITOR_STATE_PAUSED;
  }
}

static void s_editor_state_set_step(LDKEditorContext *editor)
{
  if (!editor || editor->editor_state != LDK_EDITOR_STATE_PAUSED)
  {
    return;
  }

  if (ldk_game_instance_step())
  {
    editor->editor_state = LDK_EDITOR_STATE_STEPPING;
  }
}

//----------------------------------------------------------
// Project handling
//----------------------------------------------------------

typedef enum LDKEditorGameModuleReloadResult
{
  LDK_EDITOR_GAME_MODULE_RELOAD_RETRY = 0,
  LDK_EDITOR_GAME_MODULE_RELOAD_COMPLETE,
  LDK_EDITOR_GAME_MODULE_RELOAD_FAILED
} LDKEditorGameModuleReloadResult;

typedef struct LDKEditorGameModuleWatch
{
  XFSWatch *handle;
  XFSPath directory;
  XFSPath filename;
  bool reload_pending;
} LDKEditorGameModuleWatch;

static LDKEditorGameModuleWatch s_game_module_watch = {0};

static bool s_project_game_module_editor_path_get(
    const LDKProject *project, const char *filename, XFSPath *out_path)
{
  XFSPath editor_cache_path = {0};

  if (project == NULL || filename == NULL || filename[0] == 0 ||
      out_path == NULL || project->cache_path.length == 0)
  {
    return false;
  }

  if (!x_fs_path(&editor_cache_path, x_fs_path_cstr(&project->cache_path),
          "editor") ||
      !x_fs_path(out_path, x_fs_path_cstr(&editor_cache_path), filename))
  {
    return false;
  }

  x_fs_path_normalize(out_path);
  return true;
}

static bool s_project_editor_game_dll_path_get(
    const LDKProject *project, XFSPath *out_path)
{
  return s_project_game_module_editor_path_get(
      project, "game_editor.dll", out_path);
}

static void s_project_game_module_watch_close(void)
{
  if (s_game_module_watch.handle != NULL)
  {
    x_fs_watch_close(s_game_module_watch.handle);
  }

  memset(&s_game_module_watch, 0, sizeof(s_game_module_watch));
}

static bool s_project_game_module_watch_open(LDKEditorContext *editor)
{
  if (editor == NULL || !editor->project.loaded)
  {
    return false;
  }

  s_project_game_module_watch_close();

  if (x_fs_path_dirname(&editor->project.game_dll_path,
          &s_game_module_watch.directory) == 0 ||
      x_fs_path_basename(
          &editor->project.game_dll_path, &s_game_module_watch.filename) == 0)
  {
    return false;
  }

  s_game_module_watch.handle =
      x_fs_watch_open(x_fs_path_cstr(&s_game_module_watch.directory));
  if (s_game_module_watch.handle == NULL)
  {
    ldki_editor_log_warning(editor, "Failed to watch the game module output.");
    memset(&s_game_module_watch, 0, sizeof(s_game_module_watch));
    return false;
  }

  return true;
}

static bool s_project_game_module_watch_event_matches(
    const XFSWatchEvent *event)
{
  XFSPath event_path = {0};
  XFSPath event_filename = {0};

  if (event == NULL || event->filename == NULL || event->filename[0] == 0 ||
      s_game_module_watch.filename.length == 0)
  {
    return false;
  }

  x_fs_path_set(&event_path, event->filename);
  if (x_fs_path_basename(&event_path, &event_filename) == 0)
  {
    return false;
  }

  return strcmp(event_filename.buf, s_game_module_watch.filename.buf) == 0;
}

static void s_project_game_module_handles_invalidate(
    LDKEditorContext *editor)
{
  editor->selected_entity = x_handle_null();
  editor->editor_camera = x_handle_null();
  editor->scene_view = LDK_RENDERER_VIEW_INVALID;

  if (editor->hierarchy_expanded_entities != NULL)
  {
    x_array_clear(editor->hierarchy_expanded_entities);
  }
}

static bool s_project_game_module_runtime_load(LDKEditorContext *editor,
    const XFSPath *dll_path, const char *scene_snapshot)
{
  LDKSceneManager *scene_manager;
  LDKSceneResult scene_result;

  if (!ldk_game_instance_load_from_shared_lib(x_fs_path_cstr(dll_path)))
  {
    return false;
  }

  if (!ldk_game_instance_initialize())
  {
    ldk_game_instance_unload();
    return false;
  }

  scene_manager = ldk_module_get(LDK_MODULE_SCENE_MANAGER);
  if (!ldk_scene_manager_configure_file(scene_manager,
          editor->project.project_file_path.buf, &scene_result))
  {
    ldki_editor_log_error(editor, scene_result.error);
    ldk_game_instance_unload();
    return false;
  }

  if (scene_snapshot != NULL &&
      !ldk_scene_from_tml(scene_snapshot, &scene_result))
  {
    ldki_editor_log_error(editor, scene_result.error);
    ldk_game_instance_unload();
    return false;
  }

  return true;
}

static bool s_project_game_module_load(LDKEditorContext *editor)
{
  XFSPath editor_game_dll_path = {0};

  if (editor == NULL || !editor->project.loaded ||
      !x_fs_path_is_file(&editor->project.game_dll_path) ||
      !s_project_editor_game_dll_path_get(
          &editor->project, &editor_game_dll_path))
  {
    return false;
  }

  XFSPath editor_cache_path = {0};
  if (x_fs_path_dirname(&editor_game_dll_path, &editor_cache_path) == 0 ||
      !x_fs_directory_create_recursive(editor_cache_path.buf))
  {
    ldki_editor_log_error(editor, "Failed to create game module cache path.");
    return false;
  }

  if (x_fs_path_is_file(&editor_game_dll_path) &&
      !x_fs_file_delete(editor_game_dll_path.buf))
  {
    ldki_editor_log_error(
        editor, "Failed to replace the editor game module copy.");
    return false;
  }

  if (!x_fs_file_copy(
          editor->project.game_dll_path.buf, editor_game_dll_path.buf))
  {
    ldki_editor_log_error(editor, "Failed to prepare editor game module copy.");
    return false;
  }

  if (!s_project_game_module_runtime_load(
          editor, &editor_game_dll_path, NULL))
  {
    x_fs_file_delete(editor_game_dll_path.buf);
    return false;
  }

  s_project_game_module_watch_open(editor);
  return true;
}

static bool s_project_game_module_restore_file(
    const XFSPath *active_path, const XFSPath *previous_path)
{
  if (x_fs_path_is_file(active_path) &&
      !x_fs_file_delete(x_fs_path_cstr(active_path)))
  {
    return false;
  }

  if (x_fs_file_rename(previous_path->buf, active_path->buf))
  {
    return true;
  }

  return x_fs_file_copy(previous_path->buf, active_path->buf);
}

static LDKEditorGameModuleReloadResult s_project_game_module_reload(
    LDKEditorContext *editor)
{
  XFSPath active_path = {0};
  XFSPath next_path = {0};
  XFSPath previous_path = {0};
  XStrBuilder *scene_snapshot = NULL;
  LDKSceneResult scene_result;
  const char *scene_snapshot_text = NULL;

  if (editor == NULL || !editor->project.loaded ||
      editor->editor_state != LDK_EDITOR_STATE_STOPED ||
      ldk_game_instance_is_started())
  {
    return LDK_EDITOR_GAME_MODULE_RELOAD_RETRY;
  }

  if (!s_project_editor_game_dll_path_get(
          &editor->project, &active_path) ||
      !s_project_game_module_editor_path_get(
          &editor->project, "game_editor_next.dll", &next_path) ||
      !s_project_game_module_editor_path_get(
          &editor->project, "game_editor_previous.dll", &previous_path))
  {
    ldki_editor_log_error(editor, "Failed to resolve game module paths.");
    return LDK_EDITOR_GAME_MODULE_RELOAD_FAILED;
  }

  if (!x_fs_path_is_file(&editor->project.game_dll_path))
  {
    return LDK_EDITOR_GAME_MODULE_RELOAD_RETRY;
  }

  if (x_fs_path_is_file(&next_path) &&
      !x_fs_file_delete(next_path.buf))
  {
    return LDK_EDITOR_GAME_MODULE_RELOAD_RETRY;
  }

  if (!x_fs_file_copy(editor->project.game_dll_path.buf, next_path.buf))
  {
    return LDK_EDITOR_GAME_MODULE_RELOAD_RETRY;
  }

  if (editor->current_scene_path.length != 0)
  {
    scene_snapshot = x_strbuilder_create();
    if (scene_snapshot == NULL ||
        !ldk_scene_to_tml(scene_snapshot, &scene_result))
    {
      ldki_editor_log_error(editor, scene_snapshot == NULL
          ? "Failed to allocate scene snapshot for game module reload."
          : scene_result.error);
      x_strbuilder_destroy(scene_snapshot);
      x_fs_file_delete(next_path.buf);
      return LDK_EDITOR_GAME_MODULE_RELOAD_FAILED;
    }

    scene_snapshot_text = x_strbuilder_to_string(scene_snapshot);
  }

  if (x_fs_path_is_file(&previous_path) &&
      !x_fs_file_delete(previous_path.buf))
  {
    ldki_editor_log_error(
        editor, "Failed to remove the previous game module backup.");
    x_strbuilder_destroy(scene_snapshot);
    x_fs_file_delete(next_path.buf);
    return LDK_EDITOR_GAME_MODULE_RELOAD_FAILED;
  }

  if (!x_fs_path_is_file(&active_path) ||
      !x_fs_file_copy(active_path.buf, previous_path.buf))
  {
    ldki_editor_log_error(
        editor, "Failed to preserve the current editor game module.");
    x_strbuilder_destroy(scene_snapshot);
    x_fs_file_delete(next_path.buf);
    return LDK_EDITOR_GAME_MODULE_RELOAD_FAILED;
  }

  if (!ldk_game_instance_unload())
  {
    ldki_editor_log_error(editor, "Failed to unload the current game module.");
    x_strbuilder_destroy(scene_snapshot);
    x_fs_file_delete(next_path.buf);
    x_fs_file_delete(previous_path.buf);
    return LDK_EDITOR_GAME_MODULE_RELOAD_FAILED;
  }

  s_project_game_module_handles_invalidate(editor);

  if (!x_fs_file_delete(active_path.buf))
  {
    ldki_editor_log_error(editor, "Failed to replace the editor game module.");

    if (!s_project_game_module_runtime_load(
            editor, &active_path, scene_snapshot_text))
    {
      ldki_editor_log_error(
          editor, "Failed to restore the previous game module.");
      s_project_game_module_watch_close();
    }

    x_fs_file_delete(next_path.buf);
    x_fs_file_delete(previous_path.buf);
    x_strbuilder_destroy(scene_snapshot);
    return LDK_EDITOR_GAME_MODULE_RELOAD_FAILED;
  }

  if (!x_fs_file_rename(next_path.buf, active_path.buf))
  {
    ldki_editor_log_error(editor, "Failed to replace the editor game module.");

    if (!s_project_game_module_restore_file(&active_path, &previous_path) ||
        !s_project_game_module_runtime_load(
            editor, &active_path, scene_snapshot_text))
    {
      ldki_editor_log_error(
          editor, "Failed to restore the previous game module.");
      s_project_game_module_watch_close();
    }
    else if (x_fs_path_is_file(&previous_path))
    {
      x_fs_file_delete(previous_path.buf);
    }

    x_fs_file_delete(next_path.buf);
    x_strbuilder_destroy(scene_snapshot);
    return LDK_EDITOR_GAME_MODULE_RELOAD_FAILED;
  }

  if (s_project_game_module_runtime_load(
          editor, &active_path, scene_snapshot_text))
  {
    x_fs_file_delete(previous_path.buf);
    x_strbuilder_destroy(scene_snapshot);
    ldki_editor_log_info(editor, "Game module reloaded.");
    return LDK_EDITOR_GAME_MODULE_RELOAD_COMPLETE;
  }

  ldki_editor_log_error(
      editor, "Failed to load the new game module. Restoring previous module.");

  if (!s_project_game_module_restore_file(&active_path, &previous_path) ||
      !s_project_game_module_runtime_load(
          editor, &active_path, scene_snapshot_text))
  {
    ldki_editor_log_error(
        editor, "Failed to restore the previous game module.");
    s_project_game_module_watch_close();
  }
  else
  {
    if (x_fs_path_is_file(&previous_path))
    {
      x_fs_file_delete(previous_path.buf);
    }
    ldki_editor_log_warning(editor, "Previous game module restored.");
  }

  x_strbuilder_destroy(scene_snapshot);
  return LDK_EDITOR_GAME_MODULE_RELOAD_FAILED;
}

static bool s_project_game_module_refresh(LDKEditorContext *editor)
{
  LDKEditorGameModuleReloadResult reload_result;

  if (editor == NULL || !editor->project.loaded)
  {
    return false;
  }

  if (ldk_game_get() == NULL)
  {
    return s_project_game_module_load(editor);
  }

  reload_result = s_project_game_module_reload(editor);
  if (reload_result != LDK_EDITOR_GAME_MODULE_RELOAD_COMPLETE)
  {
    return false;
  }

  /* Reset the watcher after a build-driven reload so the file changes that
   * produced this DLL cannot trigger a second reload on the next frame. */
  s_project_game_module_watch_open(editor);
  return true;
}

static void s_project_game_module_watch_update(LDKEditorContext *editor)
{
  XFSWatchEvent events[16];
  i32 event_count;

  if (editor == NULL || !editor->project.loaded ||
      s_game_module_watch.handle == NULL)
  {
    return;
  }

  do
  {
    event_count = x_fs_watch_poll(s_game_module_watch.handle, events,
        (i32)(sizeof(events) / sizeof(events[0])));
    if (event_count < 0)
    {
      ldki_editor_log_warning(editor, "Game module file watcher failed.");
      s_project_game_module_watch_close();
      return;
    }

    for (i32 i = 0; i < event_count; ++i)
    {
      if (s_project_game_module_watch_event_matches(&events[i]))
      {
        s_game_module_watch.reload_pending = true;
      }
    }
  } while (event_count == (i32)(sizeof(events) / sizeof(events[0])));

  if (!s_game_module_watch.reload_pending ||
      editor->editor_state != LDK_EDITOR_STATE_STOPED ||
      ldk_game_instance_is_started() || editor->project_build.active)
  {
    return;
  }

  if (s_project_game_module_reload(editor) !=
      LDK_EDITOR_GAME_MODULE_RELOAD_RETRY)
  {
    s_game_module_watch.reload_pending = false;
  }
}

static bool s_project_import_packages_mount(LDKEditorContext *editor)
{
  XIni ini = {0};
  XIniError error = {0};
  i32 import_section = -1;

  if (!editor || !editor->project.loaded)
  {
    return false;
  }

  if (!x_ini_load_file(
          editor->project.project_file_path.buf, &ini, &error))
  {
    ldk_log_error("Failed to read project imports from '%s'.\n",
        editor->project.project_file_path.buf);
    return false;
  }

  for (i32 i = 0; i < x_ini_section_count(&ini); ++i)
  {
    const char *section_name = x_ini_section_name(&ini, i);
    if (section_name && (strcmp(section_name, ".box_imports") == 0 ||
                         strcmp(section_name, ".import") == 0))
    {
      import_section = i;
      break;
    }
  }

  if (import_section < 0)
  {
    x_ini_free(&ini);
    return true;
  }

  i32 import_count = x_ini_key_count(&ini, import_section);
  for (i32 i = 0; i < import_count; ++i)
  {
    const char *import_path = x_ini_key_name(&ini, import_section, i);
    const char *copy_value = x_ini_value_at(&ini, import_section, i);
    const XFSPath *root;
    const char *relative_path;
    XFSPath package_path = {0};

    if (!import_path || !import_path[0] || !copy_value ||
        (strcmp(copy_value, "0") != 0 && strcmp(copy_value, "1") != 0))
    {
      ldk_log_error("Invalid [.box_imports] entry in '%s'.\n",
          editor->project.project_file_path.buf);
      x_ini_free(&ini);
      return false;
    }

    if (x_fs_path_is_absolute_cstr(import_path) ||
        strstr(import_path, "../") || strstr(import_path, "..\\"))
    {
      ldk_log_error("Import path must stay relative to its RunTree: '%s'.\n",
          import_path);
      x_ini_free(&ini);
      return false;
    }

    XFSPath engine_runtree = {0};
    if (import_path[0] == '@')
    {
      x_fs_path(&engine_runtree, &editor->engine_root, "runtree");
      x_fs_path_normalize(&engine_runtree);
      root = &engine_runtree;
      relative_path = import_path + 1;
    }
    else
    {
      root = &editor->project.run_root_path;
      relative_path = import_path;
    }

    if (!relative_path[0] || x_fs_path_is_absolute_cstr(relative_path))
    {
      ldk_log_error("Import path is invalid in '%s': '%s'.\n",
          editor->project.project_file_path.buf, import_path);
      x_ini_free(&ini);
      return false;
    }

    const char *extension = strrchr(relative_path, '.');
    if (!extension || strcmp(extension, LDK_PACKAGE_FILE_EXTENSION) != 0)
    {
      ldk_log_error("Imported package must be a .box file: '%s'.\n",
          import_path);
      x_ini_free(&ini);
      return false;
    }

    x_fs_path(&package_path, root->buf, relative_path);
    x_fs_path_normalize(&package_path);
    if (!x_fs_path_is_file(&package_path))
    {
      ldk_log_error("Imported package not found: '%s'.\n", package_path.buf);
      x_ini_free(&ini);
      return false;
    }

    if (!ldki_editor_file_explorer_package_mount(editor, &package_path))
    {
      ldk_log_error("Failed to mount imported package '%s'.\n",
          package_path.buf);
      x_ini_free(&ini);
      return false;
    }
  }

  x_ini_free(&ini);
  return true;
}

bool ldki_editor_project_import_packages_reload(LDKEditorContext *editor)
{
  if (!editor || !editor->project.loaded)
  {
    return false;
  }

  ldki_editor_file_explorer_package_mounts_clear();
  return s_project_import_packages_mount(editor);
}

static bool s_project_unload(LDKEditorContext *editor)
{
  ldki_editor_scene_catalog_close(editor);
  XFSPath editor_game_dll_path = {0};
  XFSPath editor_next_dll_path = {0};
  XFSPath editor_previous_dll_path = {0};
  bool has_editor_game_dll_path;
  bool has_editor_next_dll_path;
  bool has_editor_previous_dll_path;

  s_project_game_module_watch_close();
  ldki_editor_scene_catalog_close(editor);

  LDK_ASSERT(editor);
  LDK_ASSERT(editor->initialized);

  if (!editor->project.loaded)
  {
    editor->selected_entity = x_handle_null();
    ldk_scene_systems_clear(&editor->current_scene_systems);
    if (editor->hierarchy_expanded_entities != NULL)
    {
      x_array_clear(editor->hierarchy_expanded_entities);
    }
    return true;
  }

  s_editor_state_set_stop(editor);
  s_editor_profiler_stop(editor);
  has_editor_game_dll_path =
      s_project_editor_game_dll_path_get(
          &editor->project, &editor_game_dll_path);
  has_editor_next_dll_path = s_project_game_module_editor_path_get(
      &editor->project, "game_editor_next.dll", &editor_next_dll_path);
  has_editor_previous_dll_path = s_project_game_module_editor_path_get(
      &editor->project, "game_editor_previous.dll",
      &editor_previous_dll_path);

  if (!ldk_game_instance_unload())
  {
    ldk_log_error("Failed to unload game instance for project '%s'.\n",
        editor->project.name.buf);
    return false;
  }

  if (has_editor_game_dll_path &&
      x_fs_path_is_file(&editor_game_dll_path) &&
      !x_fs_file_delete(editor_game_dll_path.buf))
  {
    ldk_log_warning("Failed to delete editor game module copy '%s'.\n",
        editor_game_dll_path.buf);
  }

  if (has_editor_next_dll_path &&
      x_fs_path_is_file(&editor_next_dll_path) &&
      !x_fs_file_delete(editor_next_dll_path.buf))
  {
    ldk_log_warning("Failed to delete editor game module copy '%s'.\n",
        editor_next_dll_path.buf);
  }

  if (has_editor_previous_dll_path &&
      x_fs_path_is_file(&editor_previous_dll_path) &&
      !x_fs_file_delete(editor_previous_dll_path.buf))
  {
    ldk_log_warning("Failed to delete editor game module copy '%s'.\n",
        editor_previous_dll_path.buf);
  }

  editor->selected_entity = x_handle_null();
  editor->editor_camera = x_handle_null();
  editor->scene_view = LDK_RENDERER_VIEW_INVALID;
  ldk_scene_systems_clear(&editor->current_scene_systems);
  if (editor->hierarchy_expanded_entities != NULL)
  {
    x_array_clear(editor->hierarchy_expanded_entities);
  }
  ldki_editor_file_explorer_package_mounts_clear();
  ldk_project_unload(&editor->project);
  editor->editor_state = LDK_EDITOR_STATE_STOPED;
  s_editor_set_title(editor);
  return true;
}

static bool s_project_load(
    LDKEditorContext *editor, const char *project_file_path)
{
  LDKSceneManager *scene_manager;
  LDKSceneResult scene_result;
  bool has_game_module;

  LDK_ASSERT(editor);
  LDK_ASSERT(editor->initialized);

  if (!project_file_path)
  {
    return false;
  }

  if (editor->project.loaded)
  {
    return false;
  }

  if (!ldk_project_load(&editor->project, project_file_path))
  {
    return false;
  }

  LDKAssetSource *asset_source = ldk_module_get(LDK_MODULE_ASSET_SOURCE);
  if (!asset_source || !ldk_asset_source_runtree_set(
                           asset_source, editor->project.run_root_path.buf))
  {
    ldk_log_error("Failed to configure project asset source.\n");
    goto fail;
  }

  /* Set the explorer root before mounting packages. Changing roots clears old
   * package mounts, so doing this afterwards would discard the packages that
   * were just mounted. */
  ldki_editor_file_explorer_focus_runtree(editor);

  if (!s_project_import_packages_mount(editor))
  {
    goto fail;
  }

  /* Package mounting selects the package root. Project load should always
   * finish with the project's RunTree selected instead. */
  ldki_editor_file_explorer_focus_runtree(editor);

  if (!ldk_engine_render_resolution_set(
          editor->project.project_resolution_width,
          editor->project.project_resolution_height))
  {
    ldk_log_error("Invalid project render resolution %dx%d.\n",
        editor->project.project_resolution_width,
        editor->project.project_resolution_height);
    goto fail;
  }

  if (!ldk_engine_shadow_settings_set(editor->project.shadow_map_resolution,
          editor->project.shadow_distance))
  {
    ldk_log_error("Failed to apply project shadow settings.\n");
    goto fail;
  }

  has_game_module = x_fs_path_is_file(&editor->project.game_dll_path);
  if (has_game_module)
  {
    if (!s_project_game_module_load(editor))
    {
      goto fail;
    }
  }
  else
  {
    /* The scene catalog is project data and can be loaded without game
     * metadata. Groupings are loaded later when the game module becomes
     * available. */
    scene_manager = ldk_module_get(LDK_MODULE_SCENE_MANAGER);
    if (!ldk_scene_manager_configure_file(scene_manager,
            editor->project.project_file_path.buf, &scene_result))
    {
      ldki_editor_log_error(editor, scene_result.error);
      goto fail;
    }
  }

  editor->editor_state = LDK_EDITOR_STATE_STOPED;
  s_editor_set_title(editor);

  if (!has_game_module &&
      !s_editor_project_build_request_with_continuation(editor,
          LDK_EDITOR_PROJECT_BUILD_CONTINUATION_LOAD_GAME_MODULE))
  {
    ldki_editor_log_error(
        editor, "Game DLL is missing and the automatic build could not start.");
  }

  s_editor_last_project_path_update(editor);
  return true;

fail:
  s_project_unload(editor);
  return false;
}

static bool s_project_switch(
    LDKEditorContext *editor, const char *project_file_path)
{
  XFSPath previous_project_file = {0};
  bool had_previous_project;

  if (editor == NULL || project_file_path == NULL || project_file_path[0] == 0)
  {
    return false;
  }

  had_previous_project = editor->project.loaded;
  if (had_previous_project)
  {
    previous_project_file = editor->project.project_file_path;
  }

  if (!s_project_unload(editor))
  {
    return false;
  }

  if (s_project_load(editor, project_file_path))
  {
    return true;
  }

  if (had_previous_project && previous_project_file.length > 0)
  {
    ldk_log_warning("Failed to load project '%s'. Restoring '%s'.\n",
        project_file_path, previous_project_file.buf);

    if (!s_project_load(editor, previous_project_file.buf))
    {
      ldk_log_error("Failed to restore previous project '%s'.\n",
          previous_project_file.buf);
    }
  }

  return false;
}

static bool s_editor_project_cmake_root_can_clean(const LDKProject *project)
{
  XFSPath relative = {0};

  if (project == NULL || project->project_root_path.length == 0 ||
      project->cmake_root_path.length == 0 ||
      x_fs_path_compare(
          &project->project_root_path, &project->cmake_root_path) == 0 ||
      x_fs_path_compare(&project->source_root_path, &project->cmake_root_path) ==
          0 ||
      x_fs_path_compare(&project->run_root_path, &project->cmake_root_path) ==
          0 ||
      x_fs_path_compare(&project->cache_path, &project->cmake_root_path) == 0 ||
      x_fs_path_relative_to(&project->project_root_path,
          &project->cmake_root_path, &relative) == 0 ||
      relative.length == 0 || strcmp(relative.buf, ".") == 0 ||
      strcmp(relative.buf, "..") == 0 || strncmp(relative.buf, "../", 3) == 0 ||
      strncmp(relative.buf, "..\\", 3) == 0)
  {
    return false;
  }

  return true;
}

bool ldki_editor_project_settings_apply(
    LDKEditorContext *editor, const LDKProject *project)
{
  LDKProject updated;
  XFSPath output_directory = {0};
  bool build_config_changed;
  bool generator_changed;

  if (editor == NULL || project == NULL || !editor->project.loaded ||
      !project->loaded || editor->editor_state != LDK_EDITOR_STATE_STOPED ||
      editor->project_build.active ||
      editor->pending_project_action.type != LDK_EDITOR_PROJECT_ACTION_NONE ||
      strcmp(editor->project.project_file_path.buf,
          project->project_file_path.buf) != 0)
  {
    return false;
  }

  updated = *project;
  build_config_changed = strcmp(editor->project.build_config.buf,
                             updated.build_config.buf) != 0;
  generator_changed = strcmp(editor->project.cmake_generator.buf,
                          updated.cmake_generator.buf) != 0 ||
                      strcmp(editor->project.cmake_arch.buf,
                          updated.cmake_arch.buf) != 0;

  if (generator_changed && x_fs_path_exists(&editor->project.cmake_root_path))
  {
    if (!s_editor_project_cmake_root_can_clean(&editor->project) ||
        !x_fs_directory_delete_recursive(editor->project.cmake_root_path.buf))
    {
      ldki_editor_log_error(
          editor, "Failed to safely reset the CMake build directory.");
      return false;
    }
  }

  if (!ldk_project_save(&updated))
  {
    return false;
  }

  editor->project = updated;

  if (!ldk_engine_render_resolution_set(
          editor->project.project_resolution_width,
          editor->project.project_resolution_height))
  {
    ldki_editor_log_warning(
        editor, "Project saved, but target resolution could not be applied.");
  }

  if (!build_config_changed)
  {
    return true;
  }

  s_project_game_module_watch_close();
  if (x_fs_path_dirname(
          &editor->project.game_dll_path, &output_directory) != 0)
  {
    x_fs_directory_create_recursive(output_directory.buf);
  }

  if (x_fs_path_is_file(&editor->project.game_dll_path))
  {
    if (!s_project_game_module_refresh(editor))
    {
      ldki_editor_log_error(editor,
          "Project settings were saved, but the selected game module could "
          "not be loaded.");
    }
  }
  else if (ldk_game_get() != NULL)
  {
    s_project_game_module_watch_open(editor);
  }

  return true;
}

bool ldki_editor_project_clean_build_request(LDKEditorContext *editor)
{
  if (editor == NULL || !editor->project.loaded ||
      editor->editor_state != LDK_EDITOR_STATE_STOPED ||
      editor->project_build.active ||
      editor->pending_project_action.type != LDK_EDITOR_PROJECT_ACTION_NONE)
  {
    return false;
  }

  editor->pending_project_action = (LDKEditorProjectAction){0};
  editor->pending_project_action.type = LDK_EDITOR_PROJECT_ACTION_CLEAN;
  return true;
}

static bool s_editor_project_build_desc_init(LDKEditorContext *editor,
    const char *config, LDKProjectBuildDesc *out_desc)
{
  if (editor == NULL || out_desc == NULL)
  {
    return false;
  }

  if (editor->cmake_path.length == 0 ||
      !x_fs_path_is_file(&editor->cmake_path) ||
      !s_editor_cmake_version_is_supported(editor->cmake_path.buf))
  {
    editor->cmake_path = s_editor_cmake_path_get(editor->window);
  }

  if (editor->cmake_path.length == 0)
  {
    ldk_os_dialog_show_error(editor->window, "CMake not found",
        "CMake 4.3 or newer is required to build a project.");
    return false;
  }

  if (editor->engine_root.length == 0)
  {
    return false;
  }

  *out_desc = (LDKProjectBuildDesc){0};
  out_desc->cmake_path = x_fs_path_cstr(&editor->cmake_path);
  out_desc->ldk_root_path = x_fs_path_cstr(&editor->engine_root);
  out_desc->config = config;
  out_desc->new_console = false;
  return true;
}

static const char *s_editor_project_build_config(
    const LDKEditorProjectBuild *build)
{
  if (build != NULL && build->action_type == LDK_EDITOR_PROJECT_ACTION_RELEASE)
  {
    return "Release";
  }

  if (build != NULL && build->project.build_config.buf[0] != 0)
  {
    return build->project.build_config.buf;
  }

  return "Debug";
}

static const char *s_editor_project_build_stage_label(
    LDKEditorProjectBuildStage stage)
{
  switch (stage)
  {
  case LDK_EDITOR_PROJECT_BUILD_STAGE_GAME_CONFIGURE:
    return "CMake configure";
  case LDK_EDITOR_PROJECT_BUILD_STAGE_GAME_BUILD:
    return "Game build";
  case LDK_EDITOR_PROJECT_BUILD_STAGE_GAME_CLEAN:
    return "Game clean";
  case LDK_EDITOR_PROJECT_BUILD_STAGE_RELEASE_CONFIGURE:
    return "Release configure";
  case LDK_EDITOR_PROJECT_BUILD_STAGE_RELEASE_BUILD:
    return "Release build";
  case LDK_EDITOR_PROJECT_BUILD_STAGE_PACKAGE:
    return "Package build";
  default:
    return "Build";
  }
}

static const char *s_editor_project_build_log_name(
    LDKEditorProjectBuildStage stage)
{
  switch (stage)
  {
  case LDK_EDITOR_PROJECT_BUILD_STAGE_GAME_CONFIGURE:
    return "configure.log";
  case LDK_EDITOR_PROJECT_BUILD_STAGE_GAME_BUILD:
    return "build.log";
  case LDK_EDITOR_PROJECT_BUILD_STAGE_GAME_CLEAN:
    return "clean.log";
  case LDK_EDITOR_PROJECT_BUILD_STAGE_RELEASE_CONFIGURE:
    return "release-configure.log";
  case LDK_EDITOR_PROJECT_BUILD_STAGE_RELEASE_BUILD:
    return "release-build.log";
  case LDK_EDITOR_PROJECT_BUILD_STAGE_PACKAGE:
    return "package-build.log";
  default:
    return "build.log";
  }
}

static bool s_editor_project_build_log_begin(LDKEditorProjectBuild *build)
{
  XFSPath logs_path;
  FILE *file;

  if (build == NULL || !build->project.loaded)
  {
    return false;
  }

  x_fs_path(&logs_path, x_fs_path_cstr(&build->project.cache_path), "logs");
  x_fs_path_normalize(&logs_path);
  if (!x_fs_directory_create_recursive(logs_path.buf))
  {
    return false;
  }

  x_fs_path(&build->log_path, logs_path.buf,
      s_editor_project_build_log_name(build->stage));
  x_fs_path_normalize(&build->log_path);

  file = fopen(build->log_path.buf, "wb");
  if (file == NULL)
  {
    build->log_path = (XFSPath){0};
    return false;
  }

  fclose(file);
  return true;
}

static void s_editor_project_build_output_drain(LDKEditorContext *editor)
{
  LDKEditorProjectBuild *build;
  char buffer[4096];
  size_t bytes_read;
  FILE *log_file = NULL;

  if (editor == NULL || !editor->project_build.active ||
      editor->project_build.process == NULL)
  {
    return;
  }

  build = &editor->project_build;
  if (build->log_path.length > 0)
  {
    log_file = fopen(build->log_path.buf, "ab");
  }

  while ((bytes_read = ldk_os_process_output_read(
              build->process, buffer, sizeof(buffer))) > 0)
  {
    if (editor->console_sb != NULL)
    {
      x_strbuilder_append_substring(editor->console_sb, buffer, bytes_read);
    }

    if (log_file != NULL)
    {
      fwrite(buffer, 1, bytes_read, log_file);
    }
  }

  if (log_file != NULL)
  {
    fclose(log_file);
  }
}

static void s_editor_job_history_push(LDKEditorContext *editor,
    LDKEditorProjectActionType action_type, LDKEditorJobStatus status)
{
  u32 count;

  if (editor == NULL || action_type == LDK_EDITOR_PROJECT_ACTION_NONE)
  {
    return;
  }

  count = editor->job_history_count;
  if (count < LDK_EDITOR_JOB_HISTORY_CAPACITY)
  {
    count += 1;
  }

  for (u32 i = count; i > 1; --i)
  {
    editor->job_history[i - 1] = editor->job_history[i - 2];
  }

  editor->job_history[0].action_type = action_type;
  editor->job_history[0].status = status;
  editor->job_history_count = count;
}

static void s_editor_project_build_state_clear(LDKEditorContext *editor)
{
  LDKEditorProjectBuild *build;

  if (editor == NULL)
  {
    return;
  }

  build = &editor->project_build;
  if (build->process != NULL)
  {
    ldk_os_process_destroy(build->process);
    build->process = NULL;
  }

  if (build->package_arguments != NULL)
  {
    x_strbuilder_destroy(build->package_arguments);
    build->package_arguments = NULL;
  }

  ldk_project_unload(&build->project);
  *build = (LDKEditorProjectBuild){0};
}

static bool s_editor_project_build_stage_start(LDKEditorContext *editor)
{
  LDKEditorProjectBuild *build;
  LDKProjectBuildDesc build_desc;
  LDKOSProcessResult process_result = {0};
  const char *config;

  if (editor == NULL || !editor->project_build.active)
  {
    return false;
  }

  build = &editor->project_build;
  config = s_editor_project_build_config(build);
  if (build->stage != LDK_EDITOR_PROJECT_BUILD_STAGE_PACKAGE &&
      !s_editor_project_build_desc_init(editor, config, &build_desc))
  {
    return false;
  }

  if (!s_editor_project_build_log_begin(build))
  {
    ldki_editor_log_warning(editor, "Failed to create build log file.");
  }

  if (editor->console_sb != NULL)
  {
    x_strbuilder_append_format(editor->console_sb, "\n[%s]\n",
        s_editor_project_build_stage_label(build->stage));
    if (build->log_path.length > 0)
    {
      x_strbuilder_append_format(
          editor->console_sb, "Log: %s\n", build->log_path.buf);
    }
  }

  switch (build->stage)
  {
  case LDK_EDITOR_PROJECT_BUILD_STAGE_GAME_CONFIGURE:
    build->process = ldk_project_generate_game_module_start(
        &build->project, &build_desc, &process_result);
    break;
  case LDK_EDITOR_PROJECT_BUILD_STAGE_GAME_BUILD:
    build->process = ldk_project_build_game_module_start(
        &build->project, &build_desc, &process_result);
    break;
  case LDK_EDITOR_PROJECT_BUILD_STAGE_GAME_CLEAN:
    build->process = ldk_project_clean_game_module_start(
        &build->project, &build_desc, &process_result);
    break;
  case LDK_EDITOR_PROJECT_BUILD_STAGE_RELEASE_CONFIGURE:
    build->process = ldk_project_generate_game_launcher_start(
        &build->project, &build_desc, &process_result);
    break;
  case LDK_EDITOR_PROJECT_BUILD_STAGE_RELEASE_BUILD:
    build->process = ldk_project_build_game_launcher_start(
        &build->project, &build_desc, &process_result);
    break;
  case LDK_EDITOR_PROJECT_BUILD_STAGE_PACKAGE:
  {
    XFSPath executable = {0};
    LDKOSProcessDesc process_desc = {0};
#ifdef _WIN32
    const char *executable_name = "box.exe";
#else
    const char *executable_name = "box";
#endif
    XFSPath executable_directory = {0};
    if (build->package_arguments == NULL ||
        !x_fs_path_from_executable(&executable_directory) ||
        !x_fs_path_dirname(&executable_directory, &executable_directory) ||
        !x_fs_path(&executable, x_fs_path_cstr(&executable_directory),
            executable_name) ||
        !x_fs_path_is_file(&executable))
    {
      return false;
    }

    process_desc.executable = x_fs_path_cstr(&executable);
    process_desc.arguments =
        x_strbuilder_to_string(build->package_arguments);
    process_desc.working_directory =
        x_fs_path_cstr(&build->project.project_root_path);
    process_desc.new_console = false;
    build->process = ldk_os_process_start(&process_desc, &process_result);
    break;
  }
  default:
    return false;
  }

  if (build->process == NULL)
  {
    if (editor->console_sb != NULL)
    {
      x_strbuilder_append_format(editor->console_sb,
          "Failed to start process. OS error: %u\n", process_result.os_error);
    }
    return false;
  }

  return true;
}

static void s_editor_project_build_report_finish(
    LDKEditorContext *editor, bool success, bool cancelled)
{
  LDKEditorProjectBuild *build;
  LDKEditorProjectActionType action_type;
  LDKEditorProjectBuildContinuation continuation;
  LDKEditorJobStatus job_status;
  XFSPath project_file_path;
  XSmallstr project_name;

  if (editor == NULL)
  {
    return;
  }

  build = &editor->project_build;
  action_type = build->action_type;
  continuation = build->continuation;
  project_file_path = build->project_file_path;
  project_name = build->project.name;

  job_status = cancelled ? LDK_EDITOR_JOB_STATUS_CANCELLED
                         : success ? LDK_EDITOR_JOB_STATUS_DONE
                                   : LDK_EDITOR_JOB_STATUS_FAILED;
  s_editor_job_history_push(editor, action_type, job_status);
  s_editor_project_build_state_clear(editor);

  if (action_type == LDK_EDITOR_PROJECT_ACTION_CLEAN && ldk_game_get() != NULL)
  {
    s_project_game_module_watch_open(editor);
  }

  if (cancelled)
  {
    if (action_type == LDK_EDITOR_PROJECT_ACTION_CREATE)
    {
      ldki_editor_log_warning(editor,
          "Project creation cancelled. Generated files were left on disk.");
    }
    else if (action_type == LDK_EDITOR_PROJECT_ACTION_CLEAN)
    {
      ldki_editor_log_warning(editor, "Project clean cancelled.");
    }
    else
    {
      ldki_editor_log_warning(editor, "Project build cancelled.");
    }
    return;
  }

  if (!success)
  {
    if (action_type == LDK_EDITOR_PROJECT_ACTION_CREATE)
    {
      ldki_editor_log_error(editor, "Failed to create project.");
      ldk_os_dialog_show_error(editor->window, "Failed to create project",
          project_name.buf);
    }
    else if (action_type == LDK_EDITOR_PROJECT_ACTION_RELEASE)
    {
      ldki_editor_log_error(editor, "Project release build failed.");
    }
    else if (action_type == LDK_EDITOR_PROJECT_ACTION_PACKAGE)
    {
      ldki_editor_log_error(editor, "Package build failed.");
    }
    else if (action_type == LDK_EDITOR_PROJECT_ACTION_CLEAN)
    {
      ldki_editor_log_error(editor, "Project clean failed.");
    }
    else
    {
      ldki_editor_log_error(editor, "Project build failed.");
    }
    return;
  }

  if (action_type == LDK_EDITOR_PROJECT_ACTION_CREATE)
  {
    if (!s_project_switch(editor, project_file_path.buf))
    {
      ldki_editor_log_error(editor, "Failed to load created project.");
      ldk_os_dialog_show_error(editor->window, "Failed to load project",
          project_file_path.buf);
      return;
    }

    ldki_editor_log_info(editor, "Project created.");
    editor->create_project_window_close_requested = true;
    return;
  }

  if (action_type == LDK_EDITOR_PROJECT_ACTION_RELEASE)
  {
    ldki_editor_log_info(editor, "Project release build completed.");
    return;
  }

  if (action_type == LDK_EDITOR_PROJECT_ACTION_PACKAGE)
  {
    ldki_editor_log_info(editor, "Package build completed.");
    return;
  }

  if (action_type == LDK_EDITOR_PROJECT_ACTION_CLEAN)
  {
    ldki_editor_log_info(editor, "Project clean completed.");
    return;
  }

  ldki_editor_log_info(editor, "Project build completed.");

  if (continuation == LDK_EDITOR_PROJECT_BUILD_CONTINUATION_NONE)
  {
    return;
  }

  if (!s_project_game_module_refresh(editor))
  {
    ldki_editor_log_error(editor, "Failed to load the built game module.");
    return;
  }

  if (continuation == LDK_EDITOR_PROJECT_BUILD_CONTINUATION_PLAY &&
      !s_editor_state_enter_play(editor))
  {
    ldki_editor_log_error(editor, "Failed to enter Play mode after build.");
  }
}

static bool s_editor_project_build_output_validate(
    LDKEditorProjectBuild *build)
{
  XFSPath output_path;
  const char *config;

  if (build == NULL || !build->project.loaded)
  {
    return false;
  }

  config = s_editor_project_build_config(build);
  if (build->action_type == LDK_EDITOR_PROJECT_ACTION_RELEASE)
  {
    if (!ldk_project_game_launcher_output_path_get(
            &build->project, config, &output_path))
    {
      return false;
    }
  }
  else
  {
    if (!ldk_project_game_module_output_path_get(
            &build->project, config, &output_path))
    {
      return false;
    }
  }

  return x_fs_path_is_file(&output_path);
}

static bool s_editor_project_build_update(LDKEditorContext *editor)
{
  LDKEditorProjectBuild *build;
  LDKOSProcessResult process_result = {0};
  bool poll_ok;
  bool process_cancelled;

  if (editor == NULL || !editor->project_build.active)
  {
    return true;
  }

  build = &editor->project_build;
  if (build->process == NULL)
  {
    s_editor_project_build_report_finish(editor, false, false);
    return false;
  }

  s_editor_project_build_output_drain(editor);

  if (build->cancel_requested && !build->cancel_sent)
  {
    if (ldk_os_process_cancel(build->process))
    {
      build->cancel_sent = true;
      if (editor->console_sb != NULL)
      {
        x_strbuilder_append(editor->console_sb,
            "\n[build cancellation requested]\n");
      }
    }
  }

  poll_ok = ldk_os_process_poll(build->process, &process_result);
  s_editor_project_build_output_drain(editor);

  if (!poll_ok)
  {
    if (editor->console_sb != NULL)
    {
      x_strbuilder_append_format(editor->console_sb,
          "\nProcess polling failed. OS error: %u\n", process_result.os_error);
    }
    s_editor_project_build_report_finish(editor, false, false);
    return false;
  }

  if (!process_result.completed)
  {
    return true;
  }

  process_cancelled = ldk_os_process_was_cancelled(build->process);
  ldk_os_process_destroy(build->process);
  build->process = NULL;

  if (build->cancel_requested || process_cancelled)
  {
    s_editor_project_build_report_finish(editor, false, true);
    return true;
  }

  if (process_result.exit_code != 0)
  {
    if (editor->console_sb != NULL)
    {
      x_strbuilder_append_format(editor->console_sb,
          "\nProcess exited with code %u.\n", process_result.exit_code);
    }
    s_editor_project_build_report_finish(editor, false, false);
    return false;
  }

  switch (build->stage)
  {
  case LDK_EDITOR_PROJECT_BUILD_STAGE_GAME_CONFIGURE:
    build->stage = LDK_EDITOR_PROJECT_BUILD_STAGE_GAME_BUILD;
    break;
  case LDK_EDITOR_PROJECT_BUILD_STAGE_RELEASE_CONFIGURE:
    build->stage = LDK_EDITOR_PROJECT_BUILD_STAGE_RELEASE_BUILD;
    break;
  case LDK_EDITOR_PROJECT_BUILD_STAGE_PACKAGE:
  case LDK_EDITOR_PROJECT_BUILD_STAGE_GAME_CLEAN:
    s_editor_project_build_report_finish(editor, true, false);
    return true;
  case LDK_EDITOR_PROJECT_BUILD_STAGE_GAME_BUILD:
  case LDK_EDITOR_PROJECT_BUILD_STAGE_RELEASE_BUILD:
    if (!s_editor_project_build_output_validate(build))
    {
      s_editor_project_build_report_finish(editor, false, false);
      return false;
    }

    s_editor_project_build_report_finish(editor, true, false);
    return true;
  default:
    s_editor_project_build_report_finish(editor, false, false);
    return false;
  }

  if (!s_editor_project_build_stage_start(editor))
  {
    s_editor_project_build_report_finish(editor, false, false);
    return false;
  }

  return true;
}

static bool s_editor_project_build_begin(
    LDKEditorContext *editor, const LDKEditorProjectAction *action)
{
  LDKEditorProjectBuild *build;
  LDKProjectBuildDesc build_desc;
  LDKProjectCreateDesc create_desc = {0};
  XFSPath project_file_path;
  const char *config;

  if (editor == NULL || action == NULL || editor->project_build.active)
  {
    return false;
  }

  config = action->type == LDK_EDITOR_PROJECT_ACTION_RELEASE
               ? "Release"
               : editor->project.loaded && editor->project.build_config.buf[0]
                   ? editor->project.build_config.buf
                   : "Debug";
  if (!s_editor_project_build_desc_init(editor, config, &build_desc))
  {
    return false;
  }

  build = &editor->project_build;
  *build = (LDKEditorProjectBuild){0};
  build->action_type = action->type;
  build->continuation = action->build_continuation;

  if (action->type == LDK_EDITOR_PROJECT_ACTION_CREATE)
  {
    create_desc.project_name = action->project_name.buf;
    create_desc.project_root_path = action->project_root_path.buf;
    create_desc.cmake_generator = action->cmake_generator.buf;
    create_desc.cmake_arch = action->cmake_arch.buf;

    if (!ldk_project_create(&create_desc))
    {
      return false;
    }

    x_fs_path(&project_file_path, action->project_root_path.buf,
        action->project_name.buf);
    x_fs_path_change_extension(&project_file_path, "ldk");
    x_fs_path_normalize(&project_file_path);

    if (!ldk_project_load(&build->project, project_file_path.buf))
    {
      *build = (LDKEditorProjectBuild){0};
      return false;
    }

    build->project_file_path = project_file_path;
    build->stage = LDK_EDITOR_PROJECT_BUILD_STAGE_GAME_CONFIGURE;
  }
  else
  {
    if (!editor->project.loaded)
    {
      *build = (LDKEditorProjectBuild){0};
      return false;
    }

    build->project = editor->project;
    build->project_file_path = editor->project.project_file_path;
    if (action->type == LDK_EDITOR_PROJECT_ACTION_RELEASE)
    {
      build->stage = LDK_EDITOR_PROJECT_BUILD_STAGE_RELEASE_CONFIGURE;
    }
    else if (action->type == LDK_EDITOR_PROJECT_ACTION_CLEAN)
    {
      build->stage = LDK_EDITOR_PROJECT_BUILD_STAGE_GAME_CLEAN;
    }
    else
    {
      build->stage = LDK_EDITOR_PROJECT_BUILD_STAGE_GAME_CONFIGURE;
    }
  }

  if (action->type != LDK_EDITOR_PROJECT_ACTION_CLEAN &&
      !ldk_project_write_runtime_ini(&build->project))
  {
    ldk_project_unload(&build->project);
    *build = (LDKEditorProjectBuild){0};
    return false;
  }

  if (action->type == LDK_EDITOR_PROJECT_ACTION_CLEAN)
  {
    s_project_game_module_watch_close();
  }

  build->active = true;
  editor->jobs_popup_open_requested = true;
  if (!s_editor_project_build_stage_start(editor))
  {
    LDKEditorProjectActionType action_type = build->action_type;
    s_editor_job_history_push(
        editor, build->action_type, LDK_EDITOR_JOB_STATUS_FAILED);
    s_editor_project_build_state_clear(editor);
    if (action_type == LDK_EDITOR_PROJECT_ACTION_CLEAN && ldk_game_get() != NULL)
    {
      s_project_game_module_watch_open(editor);
    }
    return false;
  }

  return true;
}

static bool s_editor_project_action_process(LDKEditorContext *editor)
{
  LDKEditorProjectAction action;
  bool result;

  if (editor == NULL)
  {
    return false;
  }

  if (editor->project_build.active)
  {
    return s_editor_project_build_update(editor);
  }

  if (editor->pending_project_action.type == LDK_EDITOR_PROJECT_ACTION_NONE)
  {
    return true;
  }

  action = editor->pending_project_action;
  editor->pending_project_action = (LDKEditorProjectAction){0};

  if (action.type == LDK_EDITOR_PROJECT_ACTION_OPEN)
  {
    result = s_project_switch(editor, action.project_file_path.buf);
    if (!result)
    {
      ldk_os_dialog_show_error(editor->window, "Failed to load project",
          action.project_file_path.buf);
    }
    return result;
  }

  if (action.type == LDK_EDITOR_PROJECT_ACTION_CREATE ||
      action.type == LDK_EDITOR_PROJECT_ACTION_BUILD ||
      action.type == LDK_EDITOR_PROJECT_ACTION_CLEAN ||
      action.type == LDK_EDITOR_PROJECT_ACTION_RELEASE)
  {
    result = s_editor_project_build_begin(editor, &action);
    if (!result && action.type == LDK_EDITOR_PROJECT_ACTION_CREATE)
    {
      ldki_editor_log_error(editor, "Failed to create project.");
      ldk_os_dialog_show_error(editor->window, "Failed to create project",
          action.project_name.buf);
    }
    else if (!result && action.type == LDK_EDITOR_PROJECT_ACTION_RELEASE)
    {
      ldki_editor_log_error(editor, "Project release build failed.");
    }
    else if (!result && action.type == LDK_EDITOR_PROJECT_ACTION_CLEAN)
    {
      ldki_editor_log_error(editor, "Project clean failed.");
    }
    else if (!result)
    {
      ldki_editor_log_error(editor, "Project build failed.");
    }
    return result;
  }

  return false;
}

static void s_editor_terminate(LDKEditorContext *editor)
{
  s_editor_profiler_stop(editor);
  ldk_profiler_terminate();
  ldki_editor_profiler_terminate();
  s_project_game_module_watch_close();
  ldki_editor_scene_catalog_close(editor);
  ldki_editor_scene_play_apply_discard(editor);
  ldk_scene_systems_clear(&editor->current_scene_systems);
  ldk_scene_diagnostic_handler_set(NULL, NULL);
  LDKEventQueue *eq = ldk_module_get(LDK_MODULE_EVENT);

  if (editor->project_build.active)
  {
    s_editor_project_build_state_clear(editor);
  }
  ldk_event_handler_remove(eq, on_event_text);
  ldk_event_handler_remove(eq, on_event_frame);
  ldk_event_handler_remove(eq, on_event_keyboard);
  ldk_event_handler_remove(eq, on_event_window);
  ldki_editor_gizmo_terminate(editor);
  ldki_editor_theme_terminate(editor);
  ldk_editor_dock_terminate(editor);
}

//----------------------------------------------------------
// public Internal functions
//----------------------------------------------------------

void ldki_editor_confirm_quit(LDKEditorContext *editor)
{
  bool close = false;

  if (!editor->project.loaded)
    close = true;

  else if (ldk_os_dialog_show_yes_no(editor->window, "Quit editor ?",
               "Are you sure you want to quit the editor ?"))
  {
    close = true;
  }

  if (close)
  {
    ldk_log_info("Closing game window\n");
    ldk_engine_stop(0);
  }
}

void ldki_editor_theme_icons_set(LDKEditorContext *editor, LDKUITheme *theme)
{
  LDKUIIcon icon = {0};
  icon.color = 0xFFFFFFFF;
  icon.size = ldk_sizef(24, 24);
  icon.texture =
      ldk_renderer_texture_ui_handle(editor->renderer, editor->ui_atlas);

  icon.uv = ldk_editor_icon_rects[LDK_EDITOR_ICON_CHEV_RIGHT];
  theme->icons[LDK_UI_THEME_ICON_TREE_NODE_COLLAPSED] = icon;

  icon.uv = ldk_editor_icon_rects[LDK_EDITOR_ICON_CHEV_DOWN];
  theme->icons[LDK_UI_THEME_ICON_TREE_NODE_EXPANDED] = icon;

  icon.uv = ldk_editor_icon_rects[LDK_EDITOR_ICON_CHECKBOX_UNCHECKED];
  theme->icons[LDK_UI_THEME_ICON_TOGGLE_UNCHECKED] = icon;

  icon.uv = ldk_editor_icon_rects[LDK_EDITOR_ICON_CHECKBOX_CHECKED];
  theme->icons[LDK_UI_THEME_ICON_TOGGLE_CHECKED] = icon;

  icon.uv = ldk_editor_icon_rects[LDK_EDITOR_ICON_MORE_HORIZ];
  theme->icons[LDK_UI_THEME_ICON_MORE_HORIZ] = icon;

  icon.uv = ldk_editor_icon_rects[LDK_EDITOR_ICON_MORE_VERT];
  theme->icons[LDK_UI_THEME_ICON_MORE_VERT] = icon;

  icon.uv = ldk_editor_icon_rects[LDK_EDITOR_ICON_EJECT];
  theme->icons[LDK_UI_THEME_ICON_EJECT] = icon;
}

bool ldki_editor_project_open_request(
    LDKEditorContext *editor, const char *project_file_path)
{
  if (editor == NULL || project_file_path == NULL ||
      project_file_path[0] == 0 || editor->project_build.active ||
      editor->pending_project_action.type != LDK_EDITOR_PROJECT_ACTION_NONE)
  {
    return false;
  }

  editor->pending_project_action = (LDKEditorProjectAction){0};
  editor->pending_project_action.type = LDK_EDITOR_PROJECT_ACTION_OPEN;
  x_fs_path_set(
      &editor->pending_project_action.project_file_path, project_file_path);
  x_fs_path_normalize(&editor->pending_project_action.project_file_path);
  return true;
}

static bool s_editor_project_build_request_with_continuation(
    LDKEditorContext *editor, LDKEditorProjectBuildContinuation continuation)
{
  if (editor == NULL || !editor->project.loaded ||
      editor->project_build.active ||
      editor->pending_project_action.type != LDK_EDITOR_PROJECT_ACTION_NONE)
  {
    return false;
  }

  editor->pending_project_action = (LDKEditorProjectAction){0};
  editor->pending_project_action.type = LDK_EDITOR_PROJECT_ACTION_BUILD;
  editor->pending_project_action.build_continuation = continuation;
  return true;
}

bool ldki_editor_project_build_request(LDKEditorContext *editor)
{
  LDKEditorProjectBuildContinuation continuation =
      ldk_game_get() == NULL
          ? LDK_EDITOR_PROJECT_BUILD_CONTINUATION_LOAD_GAME_MODULE
          : LDK_EDITOR_PROJECT_BUILD_CONTINUATION_NONE;
  return s_editor_project_build_request_with_continuation(editor, continuation);
}

bool ldki_editor_package_build_request(
    LDKEditorContext *editor, const char *arguments)
{
  LDKEditorProjectBuild *build;

  if (editor == NULL || !editor->project.loaded || arguments == NULL ||
      arguments[0] == 0 || editor->project_build.active ||
      editor->pending_project_action.type != LDK_EDITOR_PROJECT_ACTION_NONE)
  {
    return false;
  }

  build = &editor->project_build;
  *build = (LDKEditorProjectBuild){0};
  build->package_arguments = x_strbuilder_create();
  if (build->package_arguments == NULL)
  {
    *build = (LDKEditorProjectBuild){0};
    return false;
  }
  x_strbuilder_append(build->package_arguments, arguments);

  build->project = editor->project;
  build->project_file_path = editor->project.project_file_path;
  build->action_type = LDK_EDITOR_PROJECT_ACTION_PACKAGE;
  build->stage = LDK_EDITOR_PROJECT_BUILD_STAGE_PACKAGE;
  build->active = true;
  editor->jobs_popup_open_requested = true;

  if (!s_editor_project_build_stage_start(editor))
  {
    s_editor_job_history_push(
        editor, build->action_type, LDK_EDITOR_JOB_STATUS_FAILED);
    s_editor_project_build_state_clear(editor);
    return false;
  }

  return true;
}

bool ldki_editor_project_release_request(LDKEditorContext *editor)
{
  if (editor == NULL || !editor->project.loaded ||
      editor->project_build.active ||
      editor->pending_project_action.type != LDK_EDITOR_PROJECT_ACTION_NONE)
  {
    return false;
  }

  editor->pending_project_action = (LDKEditorProjectAction){0};
  editor->pending_project_action.type = LDK_EDITOR_PROJECT_ACTION_RELEASE;
  return true;
}

bool ldki_editor_project_build_cancel_request(LDKEditorContext *editor)
{
  if (editor == NULL)
  {
    return false;
  }

  if (editor->project_build.active)
  {
    editor->project_build.cancel_requested = true;
    return true;
  }

  if (editor->pending_project_action.type == LDK_EDITOR_PROJECT_ACTION_CREATE ||
      editor->pending_project_action.type == LDK_EDITOR_PROJECT_ACTION_BUILD ||
      editor->pending_project_action.type == LDK_EDITOR_PROJECT_ACTION_CLEAN ||
      editor->pending_project_action.type == LDK_EDITOR_PROJECT_ACTION_RELEASE)
  {
    editor->pending_project_action = (LDKEditorProjectAction){0};
    return true;
  }

  return false;
}

bool ldki_editor_show_open_project_dialog(
    LDKEditorContext *editor, XFSPath *project_path_out)
{
  XFSPath out = {0};

  if (editor == NULL)
  {
    return false;
  }

  if (!ldk_os_dialog_show_open_file(
          editor->window, "Open Project", "ldk Project\0*.ldk\0\0\0",
          &out))
  {
    return false;
  }

  x_fs_path_normalize(&out);

  if (!ldki_editor_project_open_request(editor, out.buf))
  {
    return false;
  }

  if (project_path_out)
  {
    *project_path_out = out;
  }

  return true;
}

bool ldki_editor_project_create_window_open(LDKEditorContext *editor)
{
  if (editor == NULL)
  {
    return false;
  }

  if (editor->create_project_window_show)
  {
    return ldki_editor_window_show(LDK_EDITOR_WINDOW_CREATE_PROJECT);
  }

  if (editor->create_project_window_open_requested)
  {
    return true;
  }

  editor->create_project_window_open_requested = true;
  return true;
}

bool ldki_editor_project_create_request(LDKEditorContext *editor,
    const char *project_name, const char *project_root_path,
    const char *cmake_generator, const char *cmake_arch)
{
  const char *effective_project_name;

  if (editor == NULL || project_root_path == NULL ||
      project_root_path[0] == 0 || cmake_generator == NULL ||
      cmake_generator[0] == 0 || editor->project_build.active ||
      editor->pending_project_action.type != LDK_EDITOR_PROJECT_ACTION_NONE)
  {
    return false;
  }

  effective_project_name =
      project_name != NULL && project_name[0] != 0 ? project_name : "Game";

  editor->pending_project_action = (LDKEditorProjectAction){0};
  editor->pending_project_action.type = LDK_EDITOR_PROJECT_ACTION_CREATE;
  x_smallstr_from_cstr(
      &editor->pending_project_action.project_name, effective_project_name);
  x_fs_path_set(
      &editor->pending_project_action.project_root_path, project_root_path);
  x_fs_path_normalize(&editor->pending_project_action.project_root_path);
  x_smallstr_from_cstr(
      &editor->pending_project_action.cmake_generator, cmake_generator);
  x_smallstr_from_cstr(&editor->pending_project_action.cmake_arch,
      cmake_arch != NULL ? cmake_arch : "");
  return true;
}

//----------------------------------------------------------
// Public API
//----------------------------------------------------------

LDKEditor *ldk_editor_get()
{
  return (LDKEditor *)s_editor_instance();
}

void ldk_editor_state_set_play(LDKEditor *editor)
{
  s_editor_state_set_play((LDKEditorContext *)editor);
}

void ldk_editor_state_set_stop(LDKEditor *editor)
{
  s_editor_state_set_stop((LDKEditorContext *)editor);
}

void ldk_editor_state_set_pause(LDKEditor *editor)
{
  s_editor_state_set_pause((LDKEditorContext *)editor);
}

void ldk_editor_state_play_one_frame(LDKEditor *editor)
{
  s_editor_state_set_step((LDKEditorContext *)editor);
}

bool ldk_editor_project_load(LDKEditor *editor, const char *project_path)
{
  return ldki_editor_project_open_request(
      (LDKEditorContext *)editor, project_path);
}

void ldk_editor_quit(LDKEditor *editor)
{
  ldki_editor_confirm_quit((LDKEditorContext *)editor);
}

//----------------------------------------------------------
// Entrypoint
//----------------------------------------------------------

static void s_editor_scene_diagnostic(const char *message, void *user)
{
  /* The scene loader already emitted this to the engine logger. */
  ldki_editor_console_append(
      user, LDK_EDITOR_CONSOLE_ENTRY_ERROR, message);
}

static i32 s_editor_main(const char *project_file_path)
{
  LDKEditorContext *editor = s_editor_instance();
  editor->console_sb = x_strbuilder_create();
  ldk_scene_diagnostic_handler_set(s_editor_scene_diagnostic, editor);

  ldki_editor_register_commands(editor);

  XIni ini = {0};
  XIniError ini_error = {0};
  LDKConfig config;
  XFSPath default_editor_ini_path;
  XFSPath editor_config_directory;
  XFSPath editor_ini_path;

  /*
   * Locate the default editor.ini in the engine runtree.
   */
  x_fs_path_from_executable(&editor->engine_runtree);
  x_fs_path_dirname(&editor->engine_runtree, &editor->engine_runtree);
  x_fs_path_join(&editor->engine_runtree, "..", "..", "runtree");
  x_fs_path_normalize(&editor->engine_runtree);

  x_fs_path_dirname(&editor->engine_runtree, &editor->engine_root);
  x_fs_path_normalize(&editor->engine_root);

  x_fs_path(&default_editor_ini_path, &editor->engine_runtree, "editor.ini");

  /*
   * Create %APPDATA%/ldk/editor.ini from the default configuration
   * when no user configuration exists yet.
   */
  const char *appdata = getenv("APPDATA");
  if (!appdata || !appdata[0])
  {
    ldk_log_error("The APPDATA environment variable is not defined.\n");
    return 1;
  }

  x_fs_path(&editor_config_directory, appdata, "ldk");
  x_fs_path(&editor_ini_path, &editor_config_directory, "editor.ini");
  editor->editor_config_path = editor_ini_path;

  if (!x_fs_path_exists(&editor_ini_path))
  {
    if (!x_fs_directory_create_recursive(editor_config_directory.buf))
    {
      ldk_log_error("Failed to create editor configuration directory '%s'.\n",
          editor_config_directory.buf);
      return 1;
    }

    if (!x_fs_file_copy(default_editor_ini_path.buf, editor_ini_path.buf))
    {
      ldk_log_error("Failed to copy default editor configuration from "
                    "'%s' to '%s'.\n",
          default_editor_ini_path.buf, editor_ini_path.buf);
      return 1;
    }
  }

  if (!x_ini_load_file(editor_ini_path.buf, &ini, &ini_error))
  {
    ldk_log_error("Failed to load config file '%s'. Syntax error at %d:%d: %s",
        editor_ini_path.buf, ini_error.line, ini_error.column,
        ini_error.message ? ini_error.message : "Unknown error");
    return false;
  }

  /*
   * Resolve relative engine paths against the engine runtree rather than
   * %APPDATA%/ldk, since the copied configuration still refers to engine
   * resources such as assets/.
   */
  if (!ldk_engine_config_from_ini(&config, &ini, default_editor_ini_path.buf))
  {
    x_ini_free(&ini);
    return 1;
  }

  // Initialize engine. Must be initialized before editor and projects
  if (!ldk_engine_initialize_with_config(&config))
  {
    x_ini_free(&ini);
    return 1;
  }

  // Listen to text events for editor UI
  LDKEventQueue *module_event = ldk_module_get(LDK_MODULE_EVENT);
  ldk_event_handler_add(
      module_event, on_event_text, LDK_EVENT_TYPE_TEXT, editor);
  ldk_event_handler_add(
      module_event, on_event_keyboard, LDK_EVENT_TYPE_KEYBOARD, editor);
  ldk_event_handler_add(
      module_event, on_event_frame, LDK_EVENT_TYPE_FRAME, editor);
  ldk_event_handler_add(
      module_event, on_event_window, LDK_EVENT_TYPE_WINDOW, editor);

  editor->window = ldk_engine_main_window_get();
  editor->renderer = ldk_module_get(LDK_MODULE_RENDERER);

  // Initialize editor
  if (!s_editor_config_load_from_ini(editor, &ini, &config))
  {
    x_ini_free(&ini);
    ldk_engine_terminate();
    return 1;
  }
  x_ini_free(&ini);

  // Load editor resources
  if (!s_editor_load_resources(editor, &config))
  {
    ldk_engine_terminate();
    return 1;
  }

  if (!s_editor_gui_initialize(editor, ldk_module_get(LDK_MODULE_RENDERER)))
  {
    ldk_engine_terminate();
    return 1;
  }

  if (!ldki_editor_theme_initialize(editor, editor_config_directory.buf))
  {
    ldk_log_warning("Could not initialize the theme catalog. "
                    "Using the built-in Dark theme.\n");
    LDKUITheme theme;
    if (ldk_ui_theme_get(LDK_UI_THEME_DEFAULT_DARK, &theme))
    {
      ldki_editor_theme_icons_set(editor, &theme);
      ldk_ui_theme_set(&editor->ui, &theme);
    }
  }

  LDKEditorWindow game_window = {.id = LDK_EDITOR_WINDOW_GAME,
      .title = "Game",
      .function = s_editor_game_window,
      .data = NULL};

  if (!ldk_editor_window_add((LDKEditor *)editor, &game_window))
  {
    ldk_log_error("Failed to register the Game editor window.\n");
    ldk_engine_terminate();
    return 1;
  }

  LDKEditorWindow inspector_window = {.id = LDK_EDITOR_WINDOW_INSPECTOR,
      .title = "Inspector",
      .function = s_editor_inspector_content_window,
      .data = NULL};

  if (!ldk_editor_window_add((LDKEditor *)editor, &inspector_window))
  {
    ldk_log_error("Failed to register the Inspector editor window.\n");
    ldk_engine_terminate();
    return 1;
  }

  LDKEditorWindow hierarchy_window = {.id = LDK_EDITOR_WINDOW_HIERARCHY,
      .title = "Hierarchy",
      .function = s_editor_hierarchy_window,
      .data = NULL};

  if (!ldk_editor_window_add((LDKEditor *)editor, &hierarchy_window))
  {
    ldk_log_error("Failed to register the Hierarchy editor window.\n");
    ldk_engine_terminate();
    return 1;
  }

  LDKEditorWindow profiler_window = {.id = LDK_EDITOR_WINDOW_PROFILER,
      .title = "Profiler",
      .function = s_editor_profiler_window,
      .data = NULL};

  if (!ldk_editor_window_add((LDKEditor *)editor, &profiler_window))
  {
    ldk_log_error("Failed to register the Profiler editor window.\n");
    ldk_engine_terminate();
    return 1;
  }
  ldki_editor_window_hide(LDK_EDITOR_WINDOW_PROFILER);

  LDKEditorWindow catalog_window = {.id = LDK_EDITOR_WINDOW_SCENE_CATALOG,
      .title = "Scene Catalog",
      .function = ldki_editor_scene_catalog_show,
      .data = NULL};

  if (!ldk_editor_window_add((LDKEditor *)editor, &catalog_window))
  {
    ldk_log_error("Failed to register the Scene Catalog editor window.\n");
    ldk_engine_terminate();
    return 1;
  }
  ldki_editor_window_hide(LDK_EDITOR_WINDOW_SCENE_CATALOG);

  LDKEditorWindow tag_catalog_window = {.id = LDK_EDITOR_WINDOW_TAG_CATALOG,
      .title = "Tag Catalog",
      .function = ldki_editor_tag_catalog_show,
      .data = NULL};

  if (!ldk_editor_window_add((LDKEditor *)editor, &tag_catalog_window))
  {
    ldk_log_error("Failed to register the Tag Catalog editor window.\n");
    ldk_engine_terminate();
    return 1;
  }
  ldki_editor_window_hide(LDK_EDITOR_WINDOW_TAG_CATALOG);

  LDKEditorWindow grouping_catalog_window = {
      .id = LDK_EDITOR_WINDOW_GROUPING_CATALOG,
      .title = "Grouping Catalog",
      .function = ldki_editor_grouping_catalog_show,
      .data = NULL};

  if (!ldk_editor_window_add((LDKEditor *)editor, &grouping_catalog_window))
  {
    ldk_log_error("Failed to register the Grouping Catalog editor window.\n");
    ldk_engine_terminate();
    return 1;
  }
  ldki_editor_window_hide(LDK_EDITOR_WINDOW_GROUPING_CATALOG);

  LDKEditorWindow package_catalog_window = {.id = LDK_EDITOR_WINDOW_PACKAGE_CATALOG,
      .title = "Packages",
      .function = ldki_editor_package_catalog_show,
      .data = NULL};

  if (!ldk_editor_window_add((LDKEditor *)editor, &package_catalog_window))
  {
    ldk_log_error("Failed to register the Packages editor window.\n");
    ldk_engine_terminate();
    return 1;
  }
  ldki_editor_window_hide(LDK_EDITOR_WINDOW_PACKAGE_CATALOG);

  if (!ldk_editor_dock_init(editor))
  {
    ldk_log_error("Failed to initialize the editor dock system.\n");
    ldk_engine_terminate();
    return 1;
  }

  // load layout
  ldki_editor_dock_layout_load(NULL);

  s_editor_set_title(editor);
  editor->cmake_path = s_editor_cmake_path_get(editor->window);
  ldk_log_info("CMake path is %s\n", editor->cmake_path.buf);

  // Command-line project paths take precedence over the restore setting.
  if (project_file_path != NULL)
  {
    s_project_load(editor, project_file_path);
  }
  else if (editor->restore_last_project &&
           editor->last_project_path.length != 0)
  {
    if (!x_fs_path_is_file(&editor->last_project_path) ||
        !s_project_load(editor, editor->last_project_path.buf))
    {
      memset(&editor->last_project_path, 0, sizeof(editor->last_project_path));
      s_editor_last_project_path_write(editor, NULL);
    }
  }

  i32 exit_code = ldk_engine_run();

  s_editor_terminate(editor);
  ldk_engine_terminate();
  return exit_code;
}

int main(i32 argc, char **argv)
{
  char *project_file_path;

  if (argc == 1)
    project_file_path = NULL;
  else if (argc == 2)
    project_file_path = argv[1];
  else
  {
    printf("Usage:\n%s [project_file]\n", argv[0]);
    return 1;
  }

  return s_editor_main(project_file_path);
}
