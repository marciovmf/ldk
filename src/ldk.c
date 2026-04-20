#include "ldk_common.h"
#include "ldk_eventqueue.h"
#include "ldk_gl.h"
#include "stdx/stdx_common.h"

#define X_IMPL_ARENA
#include <stdx/stdx_arena.h>

#define X_IMPL_ARRAY
#include <stdx/stdx_array.h>

#define X_IMPL_STRING
#include <stdx/stdx_string.h>

#define X_IMPL_HASHTABLE
#include <stdx/stdx_hashtable.h>

#define X_IMPL_HPOOL
#include <stdx/stdx_hpool.h>

#define X_IMPL_LOG
#include <stdx/stdx_log.h>

#define X_IMPL_FILESYSTEM
#include <stdx/stdx_filesystem.h>

#define X_IMPL_INI
#include <stdx/stdx_ini.h>

#define X_IMPL_MATH
#include <stdx/stdx_math.h>

#include <ldk.h>

#include <ldk_entity.h>
#include <ldk_component.h>
#include <ldk_system.h>
#include <ldk_system_scenegraph.h>
#include <ldk_os.h>
#include <ldk_ui.h>
#include <ldk_ui_renderer.h>
#include <ldk_game.h>

#include <signal.h>
#include <string.h>

struct LDKRoot
{
  // Engine Modules
  LDKEntityRegistry     entity_registry;
  LDKComponentRegistry  component_registry;
  LDKSystemRegistry     system_registry;
  LDKEventQueue         event_queue;
  LDKGame               game;
  LDKConfig             config;
  LDKUIContext          editor_ui;
  LDKUIRenderer         ui_renderer;
  XLogger               logger;
  // Runtime state
  i32                   exit_code;
  bool                  running;
  bool                  playing;
  bool                  game_initialized;
  LDKWindow             window;
  LDKGCtx               graphics;
  u64                   previous_ticks;
};

static LDKRoot g_engine;
static bool g_engine_initialized = false;

static volatile sig_atomic_t g_handling_signal = 0;
static volatile sig_atomic_t g_signal_requested_stop = 0;
static volatile sig_atomic_t g_last_signal = 0;

static void s_log_signal_info(i32 signal)
{
  const char* signal_name = "Unknown signal";

  switch (signal)
  {
    case SIGABRT: signal_name = (const char*) "SIGABRT"; break;
    case SIGFPE:  signal_name = (const char*) "SIGFPE"; break;
    case SIGILL:  signal_name = (const char*) "SIGILL"; break;
    case SIGINT:  signal_name = (const char*) "SIGINT"; break;
    case SIGSEGV: signal_name = (const char*) "SIGSEGV"; break;
    case SIGTERM: signal_name = (const char*) "SIGTERM"; break;
  }

  ldk_log_error("Signal %s\n", signal_name);
  ldk_os_stack_trace_print();
  ldk_engine_stop(signal);
}

static void s_on_signal(i32 signal)
{
  if (g_handling_signal)
  {
    return;
  }

  g_handling_signal = 1;
  g_last_signal = signal;
  g_signal_requested_stop = 1;
}

static void s_terminate_all_modules(LDKRoot* e)
{
  ldk_ui_terminate(&e->editor_ui);
  ldk_system_registry_terminate(&e->system_registry);
  ldk_component_registry_terminate(&e->component_registry);
  ldk_entity_module_terminate(&e->entity_registry);
  ldk_event_queue_terminate(&e->event_queue);
  ldk_os_terminate();

  ldk_log_info("LDK Terminated\n");
  x_log_close(&e->logger);
}

static bool s_config_resolve_path(XFSPath* out_path, const XFSPath* base_path, const char* value, const char* default_value)
{
  const char* source_value = value;

  if (source_value == NULL || source_value[0] == 0)
  {
    source_value = default_value;
  }

  if (source_value == NULL || source_value[0] == 0)
  {
    x_fs_path_set(out_path, "");
    return true;
  }

  if (x_fs_path_is_absolute_cstr(source_value))
  {
    x_fs_path_set(out_path, source_value);
  }
  else
  {
    x_fs_path(out_path, x_fs_path_cstr(base_path), source_value);
  }

  x_fs_path_normalize(out_path);
  return true;
}

static bool s_config_load_defaults(LDKConfig* out_config, const char* config_ini_path)
{
  X_ASSERT(out_config != NULL);
  X_ASSERT(config_ini_path != NULL);

  memset(out_config, 0, sizeof(*out_config));

  x_fs_path_set(&out_config->config_file_path, config_ini_path);
  x_fs_path_normalize(&out_config->config_file_path);

  x_fs_path_dirname(&out_config->config_file_path, &out_config->runtree_path);
  x_fs_path_normalize(&out_config->runtree_path);

  s_config_resolve_path(&out_config->asset_root, &out_config->runtree_path, NULL, "assets");
  s_config_resolve_path(&out_config->log_file, &out_config->runtree_path, NULL, "ldk.log");
  s_config_resolve_path(&out_config->icon_path, &out_config->runtree_path, NULL, "assets/ldk.ico");

  x_fs_path_set(&out_config->game_dll, "");

  x_smallstr_from_cstr(&out_config->title, "LDK");
  out_config->width = 800;
  out_config->height = 600;
  out_config->fullscreen = false;

  return true;
}

static bool s_config_load_from_ini(LDKConfig* out_config, const char* config_ini_path)
{
  XIni ini;
  XIniError ini_error;
  const char* asset_root;
  const char* log_file;
  const char* game_dll;
  const char* title;
  const char* icon_path;

  X_ASSERT(out_config != NULL);
  X_ASSERT(config_ini_path != NULL);

  if (!s_config_load_defaults(out_config, config_ini_path))
  {
    return false;
  }

  memset(&ini, 0, sizeof(ini));
  memset(&ini_error, 0, sizeof(ini_error));

  if (!x_ini_load_file(config_ini_path, &ini, &ini_error))
  {
    x_log_error(
        &g_engine.logger,
        "Failed to load ini '%s' at %d:%d: %s",
        config_ini_path,
        ini_error.line,
        ini_error.column,
        ini_error.message ? ini_error.message : "Unknown error"
        );
    return false;
  }

  asset_root = x_ini_get(&ini, "general", "asset_root", NULL);
  log_file   = x_ini_get(&ini, "general", "log_file", NULL);
  game_dll   = x_ini_get(&ini, "general", "game_dll", NULL);
  title      = x_ini_get(&ini, "display", "title", NULL);
  icon_path  = x_ini_get(&ini, "display", "icon_path", NULL);

  if (asset_root != NULL && asset_root[0] != 0)
  {
    s_config_resolve_path(&out_config->asset_root, &out_config->runtree_path, asset_root, NULL);
  }

  if (log_file != NULL && log_file[0] != 0)
  {
    s_config_resolve_path(&out_config->log_file, &out_config->runtree_path, log_file, NULL);
  }

  if (game_dll != NULL && game_dll[0] != 0)
  {
    s_config_resolve_path(&out_config->game_dll, &out_config->runtree_path, game_dll, NULL);
  }

  if (icon_path != NULL && icon_path[0] != 0)
  {
    s_config_resolve_path(&out_config->icon_path, &out_config->runtree_path, icon_path, NULL);
  }

  if (title != NULL && title[0] != 0)
  {
    x_smallstr_from_cstr(&out_config->title, title);
  }

  out_config->width = x_ini_get_i32(&ini, "display", "width", out_config->width);
  out_config->height = x_ini_get_i32(&ini, "display", "height", out_config->height);
  out_config->fullscreen = x_ini_get_bool(&ini, "display", "fullscreen", out_config->fullscreen);

  x_ini_free(&ini);
  return true;
}

float float_value = 0.5f;
bool bool_value = false;

void s_draw_editor_ui(LDKUIContext* ui)
{
  if (ldk_ui_begin_window(ui, "Window 1", (LDKUIRect){ 10, 100, 300, 200 }))
  {
    ldk_ui_begin_vertical(ui);

    ldk_ui_begin_horizontal(ui);
    if (ldk_ui_button_ex(ui, "btn0", "Button 1"))
    {
      ldk_log_info("Button 0 clicked\n");
    }

    if (ldk_ui_button_ex(ui, "btn1",  "Button 2"))
    {
      ldk_log_info("Button 1 clicked\n");
    }

    ldk_ui_end_horizontal(ui);

    if (ldk_ui_toggle_button_ex(ui, "toggle", "Toggle", &bool_value))
    {
      ldk_log_info("Toggle clicked\n");
    }

    if (ldk_ui_slider_float_ex(ui, "slider", "Slider", &float_value, 0.0f, 1.0f))
    {
      ldk_log_info("Slider value = %f\n", float_value);
    }
  }

  if (ldk_ui_begin_window(ui, "Window 2", (LDKUIRect){ 10, 100, 300, 200 }))
  {
    ldk_ui_begin_vertical(ui);

    if (ldk_ui_button_ex(ui, "btn0_a", "Button 1"))
    {
      ldk_log_info("WINDOW 2: Button 0 clicked\n");
    }

    if (ldk_ui_button_ex(ui, "btn1_a",  "Button 2"))
    {
      ldk_log_info("WINDOW 2: Button 1 clicked\n");
    }
  }

  ldk_ui_end_vertical(ui);
  ldk_ui_end_window(ui);
}

bool ldk_engine_is_initialized(void)
{
  return g_engine_initialized;
}

bool ldk_engine_is_playing(void)
{
  if (!g_engine_initialized)
  {
    return false;
  }

  return g_engine.playing;
}

void* ldk_module_get(LDKModuleType module_type)
{
  X_ASSERT(g_engine_initialized);

  switch (module_type)
  {
    case LDK_MODULE_ENTITY:
      return &g_engine.entity_registry;

    case LDK_MODULE_COMPONENT:
      return &g_engine.component_registry;

    case LDK_MODULE_SYSTEM:
      return &g_engine.system_registry;

    case LDK_MODULE_EVENT:
      return &g_engine.event_queue;

    case LDK_MODULE_LOG:
      return &g_engine.logger;

    case LDK_MODULE_UI:
      return &g_engine.editor_ui;

    case LDK_MODULE_UI_RENDERER:
      return &g_engine.ui_renderer;

    case LDK_MODULE_NONE:
    default:
      break;
  }

  return NULL;
}

bool ldk_engine_initialize(const LDKGame* game, const char* config_ini_path)
{
  LDKConfig config;

  X_ASSERT(config_ini_path != NULL);

  if (!s_config_load_from_ini(&config, config_ini_path))
  {
    return false;
  }

  return ldk_engine_initialize_with_config(game, &config);
}

bool ldk_engine_initialize_with_config(const LDKGame* game, const LDKConfig* config)
{
  LDKRoot* e = &g_engine;
  bool engine_init_failed = false;

  signal(SIGABRT, s_on_signal);
  signal(SIGFPE,  s_on_signal);
  signal(SIGILL,  s_on_signal);
  signal(SIGINT,  s_on_signal);
  signal(SIGSEGV, s_on_signal);
  signal(SIGTERM, s_on_signal);

  if (g_engine_initialized)
  {
    return true;
  }

  X_ASSERT(config != NULL);

  memset(e, 0, sizeof(*e));
  e->config = *config;

  if (game != NULL)
  {
    e->game = *game;
  }

  x_log_init(&e->logger, XLOG_OUTPUT_BOTH, XLOG_LEVEL_DEBUG, x_fs_path_cstr(&e->config.log_file));
  x_log_info(&e->logger, "Initializing LDK\n");

  // this is temporary. We need something to see for now
  ldk_os_initialize();
  e->graphics = ldk_os_graphics_context_opengl_create(3, 3, 24, 8);
  e->window = ldk_os_window_create_with_flags(e->config.title.buf, e->config.width, e->config.height, LDK_WINDOW_FLAG_HIDDEN);
  //e->window = ldk_os_window_create(e->config.title.buf, e->config.width, e->config.height);
  ldk_os_window_icon_set(e->window, e->config.icon_path.buf);
  ldk_os_graphics_context_current(e->window, e->graphics);

  if (!ldk_event_queue_initialize(&e->event_queue))
  {
    ldk_log_error("Failed to initialize module: Event Queue.");
    engine_init_failed = true;
  }

  if (!ldk_entity_module_initialize(&e->entity_registry, 1024, 1))
  {
    ldk_log_error("Failed to initialize module: Entity Registry.");
    engine_init_failed = true;
  }

  if (!ldk_component_registry_initialize(&e->component_registry))
  {
    ldk_log_error("Failed to initialize module: Component Registry.");
    engine_init_failed = true;
  }

  if (!ldk_system_registry_initialize(&e->system_registry))
  {
    ldk_log_error("Failed to initialize module: System Registry.");
    engine_init_failed = true;
  }

  LDKUIConfig ui_cfg;
  ui_cfg.frame_arena_size = 1024 * 4;
  ui_cfg.initial_vertex_capacity = 1024;
  ui_cfg.initial_index_capacity = 1024;
  ui_cfg.initial_command_capacity = 256;
  ui_cfg.initial_window_capacity = 64;
  ui_cfg.initial_id_stack_capacity = 64;

  if (!ldk_ui_initialize(&e->editor_ui, &ui_cfg))
  {
    ldk_log_error("Failed to initialize module: UI System.");
    engine_init_failed = true;
  }

  LDKUIRendererConfig ui_renderer_config;
  ui_renderer_config.initial_index_capacity = ui_cfg.initial_index_capacity;
  ui_renderer_config.initial_vertex_capacity = ui_cfg.initial_vertex_capacity;
  if (!ldk_ui_renderer_initialize(&e->ui_renderer, &ui_renderer_config))
  {
    ldk_log_error("Failed to initialize module: UI Renderer.");
    ldk_engine_terminate();
    return false;
  }

  if (engine_init_failed)
  {
    s_terminate_all_modules(e);
    return false;
  }

  if (!ldk_system_registry_register(&e->system_registry, ldk_scenegraph_system_desc()))
  {
    ldk_log_error("Failed to register engine system: SceneGraph.");
    ldk_engine_terminate();
    return false;
  }

  if (!ldk_system_registry_start(&e->system_registry))
  {
    ldk_log_error("Failed to start system registry.");
    ldk_engine_terminate();
    return false;
  }

  e->running = true;
  e->playing = false;
  e->previous_ticks = 0;
  g_engine_initialized = true;
  return true;
}

/*
 * Play mode enables runtime simulation on top of the currently loaded editor
 * state. The editor scene is expected to be the source of truth; later we can
 * build a dedicated runtime copy here without changing the public contract.
 */
bool ldk_engine_play_start(void)
{
  LDKRoot* e = &g_engine;

  X_ASSERT(g_engine_initialized);
  X_ASSERT(g_engine.game.start != NULL);

  if (e->playing)
  {
    return true;
  }

  if (!e->game_initialized && e->game.initialize != NULL)
  {
    if (!e->game.initialize(&e->game))
    {
      ldk_log_error("Failed to initialize game module.");
      return false;
    }

    e->game_initialized = true;
  }

  if (!e->game.start(&e->game))
  {
    return false;
  }

  e->playing = true;
  e->previous_ticks = ldk_os_time_ticks_get();
  return true;
}

void ldk_engine_play_stop(void)
{
  LDKRoot* e = &g_engine;

  X_ASSERT(g_engine_initialized);

  if (!e->playing)
  {
    return;
  }

  if (e->game.stop != NULL)
  {
    e->game.stop(&e->game);
  }

  e->playing = false;
  e->previous_ticks = 0;
}

void ldk_engine_stop(i32 exit_code)
{
  LDKRoot* e = &g_engine;
  e->exit_code = exit_code;
  e->running = false;
}

void ldk_engine_frame(void)
{
  LDKRoot* e = &g_engine;
  u64 current_ticks;
  float delta_time;
  LDKEvent event;

  X_ASSERT(g_engine_initialized);

  if (!e->running)
  {
    return;
  }

  if (g_signal_requested_stop)
  {
    g_signal_requested_stop = 0;
    s_log_signal_info((i32) g_last_signal);
  }

  while (ldk_os_events_poll(&event))
  {
    ldk_event_push(&e->event_queue, &event);
  }

  ldk_event_queue_broadcast(&e->event_queue);
  current_ticks = ldk_os_time_ticks_get();

  if (e->previous_ticks == 0)
  {
    e->previous_ticks = current_ticks;
  }

  delta_time = (float) ldk_os_time_ticks_interval_get_milliseconds(e->previous_ticks, current_ticks) / 1000.0f;
  e->previous_ticks = current_ticks;

  LDKSize window_size = ldk_os_window_client_area_size_get(e->window);
  ldk_log_info("%d,%d\n", window_size.width, window_size.height);
  glViewport(0, 0, window_size.width, window_size.height);

#ifdef LDK_EDITOR
  LDKMouseState     mouse_state;
  LDKKeyboardState  kbd_state;
  LDKUIRect ui_viewport = (LDKUIRect){.x = 0.0f, .y = 0.0f, .w = (float) window_size.width, .h = (float) window_size.height };
  ldk_os_mouse_state_get(&mouse_state);
  ldk_os_keyboard_state_get(&kbd_state);

  /*
   * Editor mode is always active in editor builds. Scene view, picking, gizmos,
   * inspectors and other tools should update here even when play mode is off.
   */
#endif

  if (!ldk_system_registry_run_bucket(&e->system_registry, LDK_SYSTEM_BUCKET_PRE_UPDATE, delta_time))
  {
    ldk_log_error("Failed to run system bucket: PRE_UPDATE.");
    ldk_engine_stop(1);
    return;
  }

  if (e->playing && e->game.update != NULL)
  {
    e->game.update(&e->game, delta_time);
  }

  if (!ldk_system_registry_run_bucket(&e->system_registry, LDK_SYSTEM_BUCKET_UPDATE, delta_time))
  {
    ldk_log_error("Failed to run system bucket: UPDATE.");
    ldk_engine_stop(1);
    return;
  }

  if (!ldk_system_registry_run_bucket(&e->system_registry, LDK_SYSTEM_BUCKET_POST_UPDATE, delta_time))
  {
    ldk_log_error("Failed to run system bucket: POST_UPDATE.");
    ldk_engine_stop(1);
    return;
  }

  event.type = LDK_EVENT_TYPE_RENDER_BEFORE;
  event.frameEvent.ticks = current_ticks;
  event.frameEvent.deltaTime = delta_time;
  ldk_event_push(&e->event_queue, &event);
  ldk_event_queue_broadcast(&e->event_queue);

  /* render scene/game view here */

  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

#ifdef LDK_EDITOR
  /* render editor overlays, gizmos and tool UI here */
  ldk_ui_begin_frame(&e->editor_ui, &mouse_state, &kbd_state, ui_viewport);
  s_draw_editor_ui(&e->editor_ui);
  ldk_ui_end_frame(&e->editor_ui);

#endif

  current_ticks = ldk_os_time_ticks_get();

  const LDKUIRenderData* ui_data = ldk_ui_get_render_data(&e->editor_ui);
  ldk_ui_renderer_draw(&e->ui_renderer,
      ui_data,
      window_size.width,
      window_size.height);

  event.type = LDK_EVENT_TYPE_RENDER_AFTER;
  event.frameEvent.ticks = current_ticks;
  event.frameEvent.deltaTime = delta_time;
  ldk_event_push(&e->event_queue, &event);
  ldk_event_queue_broadcast(&e->event_queue);

  ldk_os_window_buffers_swap(e->window);
}

void ldk_engine_terminate(void)
{
  LDKRoot* e = &g_engine;

  if (!g_engine_initialized)
  {
    return;
  }

  ldk_engine_play_stop();

  if (e->system_registry.is_started)
  {
    ldk_system_registry_stop(&e->system_registry);
  }

  if (e->game_initialized && e->game.terminate != NULL)
  {
    e->game.terminate(&e->game);
    e->game_initialized = false;
  }

  s_terminate_all_modules(e);

  memset(e, 0, sizeof(*e));
  g_engine_initialized = false;
  g_handling_signal = 0;
  g_signal_requested_stop = 0;
  g_last_signal = 0;
}

/*
 * Standalone convenience path. Editor hosts are expected to drive frames
 * explicitly and toggle play mode with ldk_engine_play_start/stop.
 */
i32 ldk_engine_run(void)
{
  LDKRoot* e = &g_engine;
  X_ASSERT(g_engine_initialized);
  e->running = true;
  ldk_os_window_show(e->window, true);
  ldk_os_window_fullscreen_set(e->window, e->config.fullscreen);

  //TODO: Remove GL stuff when we have a renderer!
  glClearColor(0, 0, 255, 0);
  glViewport(0, 0, e->config.width, e->config.height);

  if (!ldk_engine_play_start())
  {
    e->exit_code = 1;
    return e->exit_code;
  }

  while (e->running)
  {
    ldk_engine_frame();

    if (ldk_os_window_should_close(e->window))
    {
      e->running = false;
    }
  }

  ldk_engine_play_stop();
  return e->exit_code;
}
