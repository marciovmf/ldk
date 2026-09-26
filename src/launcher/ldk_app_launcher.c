#include <ldk_game.h>
#include <module/ldk_scene_manager.h>

static LDKProfilerSource s_profiler_update_source = {0};
static LDKProfilerSource s_profiler_submit_source = {0};
static LDKProfilerSource s_profiler_render_source = {0};

static bool s_profiler_event(const LDKEvent *event, void *optional)
{
  (void)optional;

  if (!event)
  {
    return false;
  }

  if (event->type == LDK_EVENT_TYPE_KEYBOARD)
  {
    if (event->keyboard_event.type == LDK_KEYBOARD_EVENT_KEY_DOWN &&
        event->keyboard_event.keyCode == LDK_KEYCODE_F8)
    {
      ldk_profiler_capture_toggle();
      return true;
    }
    return false;
  }

  if (event->type != LDK_EVENT_TYPE_FRAME)
  {
    return false;
  }

  switch (event->frame_event.type)
  {
  case LDK_FRAME_EVENT_UPDATE_BEFORE:
    ldk_profiler_frame_begin();
    ldk_profiler_zone_begin_source(&s_profiler_update_source,
        LDK_PROFILER_ZONE_PHASE, "Update", __FILE__, __func__, __LINE__);
    break;

  case LDK_FRAME_EVENT_UPDATE_AFTER:
    ldk_profiler_zone_end_kind(LDK_PROFILER_ZONE_PHASE);
    break;

  case LDK_FRAME_EVENT_SUBMIT_BEFORE:
    ldk_profiler_zone_begin_source(&s_profiler_submit_source,
        LDK_PROFILER_ZONE_PHASE, "Submit", __FILE__, __func__, __LINE__);
    break;

  case LDK_FRAME_EVENT_SUBMIT_AFTER:
    ldk_profiler_zone_end_kind(LDK_PROFILER_ZONE_PHASE);
    break;

  case LDK_FRAME_EVENT_RENDER_BEFORE:
    ldk_profiler_zone_begin_source(&s_profiler_render_source,
        LDK_PROFILER_ZONE_PHASE, "Render", __FILE__, __func__, __LINE__);
    break;

  case LDK_FRAME_EVENT_RENDER_AFTER:
    ldk_profiler_zone_end_kind(LDK_PROFILER_ZONE_PHASE);
    ldk_profiler_frame_end();
    break;

  default:
    break;
  }

  return false;
}

static void s_profiler_setup(void)
{
  const LDKConfig *config = ldk_engine_config_get();
  LDKEventQueue *events = ldk_module_get(LDK_MODULE_EVENT);

  if (!config || !ldk_profiler_initialize(config->runtree_path.buf))
  {
    ldk_log_warning("Failed to initialize launcher profiler.\n");
    return;
  }

  ldk_event_handler_add(events, s_profiler_event,
      LDK_EVENT_TYPE_KEYBOARD | LDK_EVENT_TYPE_FRAME, NULL);
  ldk_log_info("Profiler: press F8 to start/stop capture.\n");
}

static void s_profiler_teardown(void)
{
  LDKEventQueue *events = ldk_module_get(LDK_MODULE_EVENT);
  if (events)
  {
    ldk_event_handler_remove(events, s_profiler_event);
  }
  ldk_profiler_terminate();
}

static i32 s_run_game(const char* ini_file_path)
{
  if (!ldk_engine_initialize(ini_file_path))
  {
    return 1;
  }

  s_profiler_setup();

  if (!ldk_game_instance_load_static())
  {
    s_profiler_teardown();
    ldk_engine_terminate();
    return 1;
  }

  if (!ldk_game_instance_initialize())
  {
    s_profiler_teardown();
    ldk_engine_terminate();
    return 1;
  }

  LDKSceneManager *manager = ldk_module_get(LDK_MODULE_SCENE_MANAGER);
  LDKSceneResult result;
  if (!ldk_scene_manager_configure_file(
          manager, ini_file_path, &result) ||
      !ldk_scene_manager_load(manager, 0, &result))
  {
    ldk_log_error("Failed to load initial scene: %s\n", result.error);
    s_profiler_teardown();
    ldk_engine_terminate();
    return 1;
  }

  if (!ldk_game_instance_start())
  {
    s_profiler_teardown();
    ldk_engine_terminate();
    return 1;
  }

  u32 exit_code = ldk_engine_run();

  s_profiler_teardown();
  ldk_engine_terminate();
  return exit_code;
}

int main(i32 argc, char** argv)
{
  if (argc != 2)
  {
    printf("Usage:\n%s ini_file\n", argv[0]);
    return 1;
  }

  char* ini_file_path = argv[1];
  return s_run_game(ini_file_path);
}
