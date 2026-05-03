#include <ldk_common.h>

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

#define X_IMPL_IO
#include <stdx/stdx_io.h>

#include <ldk.h>
#include <ldk_os.h>
#include <ldk_game.h>
#include <module/ldk_asset_manager.h>
#include <module/ldk_ecs.h>
#include <module/ldk_renderer.h>
#include <system/ldk_scenegraph.h>

#include "ldk_rhi_gl33.h" // we only have one backend inplementation at the moment

#include <signal.h>
#include <string.h>
#include <stdio.h>

// Editor UI defaults
#ifndef LDK_DEFATUL_UI_INITIAL_INDEX_CAPACITY
#define LDK_DEFATUL_UI_INITIAL_INDEX_CAPACITY 256
#endif

#ifndef LDK_DEFATUL_UI_INITIAL_VERTEX_CAPACITY
#define LDK_DEFATUL_UI_INITIAL_VERTEX_CAPACITY 1024
#endif

#ifndef LDK_DEFATUL_UI_INITIAL_COMMAND_CAPACITY 
#define LDK_DEFATUL_UI_INITIAL_COMMAND_CAPACITY 32
#endif

#ifndef LDK_DEFATUL_UI_INITIAL_WINDOW_CAPACITY 
#define LDK_DEFATUL_UI_INITIAL_WINDOW_CAPACITY 16
#endif

#ifndef LDK_DEFATUL_UI_INITIAL_STACK_CAPACITY 
#define LDK_DEFATUL_UI_INITIAL_STACK_CAPACITY 16
#endif

struct LDKRoot
{
  // Engine Modules
  LDKAssetManager       asset_manager;
  LDKECS                ecs;
  LDKConfig             config;
  LDKEventQueue         event_queue;
  LDKGame               game;
  LDKRHIContext         rhi;
  LDKRenderer           renderer;
  LDKUIContext          editor_ui;
  LDKAssetFont          editor_font;
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

// Stub game callbacks
static bool s_stub_game_initialize(LDKGame* game) { return true; }
static bool s_stub_game_start(LDKGame* game) { return true; }
static void s_stub_game_update(LDKGame* game, float delta_time) { }
static void s_stub_game_terminate(LDKGame* game) { }
static void s_stub_game_stop(LDKGame* game) { }

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
  ldk_ecs_system_registry_stop(&e->ecs);
  ldk_ui_terminate(&e->editor_ui);
  ldk_ecs_terminate();
  ldk_event_queue_terminate(&e->event_queue);
  ldk_asset_manager_terminate(&e->asset_manager);
  ldk_rhi_terminate(&e->rhi);
  ldk_os_terminate();

  ldk_log_info("LDK Terminated\n");
  x_log_close(&e->logger);
}

static bool s_config_resolve_path(XFSPath* out_path, const XFSPath* base_path, const char* value)
{
  X_ASSERT(out_path != NULL);
  X_ASSERT(base_path != NULL);

  if (value == NULL || value[0] == 0)
  {
    x_fs_path_set(out_path, "");
    return true;
  }

  if (x_fs_path_is_absolute_cstr(value))
  {
    x_fs_path_set(out_path, value);
  }
  else
  {
    x_fs_path(out_path, x_fs_path_cstr(base_path), value);
  }

  x_fs_path_normalize(out_path);
  return true;
}

static bool s_config_load_from_ini(LDKConfig* out_config, const char* config_ini_path)
{
  XIni ini;
  XIniError ini_error;

  X_ASSERT(out_config != NULL);
  X_ASSERT(config_ini_path != NULL);

  memset(out_config, 0, sizeof(*out_config));
  memset(&ini, 0, sizeof(ini));
  memset(&ini_error, 0, sizeof(ini_error));

  x_fs_path_set(&out_config->config_file_path, config_ini_path);
  x_fs_path_normalize(&out_config->config_file_path);

  x_fs_path_dirname(&out_config->config_file_path, &out_config->runtree_path);
  x_fs_path_normalize(&out_config->runtree_path);

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

  // scetion: general
  const char* asset_root = x_ini_get(&ini, "general", "asset_root", "assets");
  const char* log_file = x_ini_get(&ini, "general", "log_file", "ldk.log");
  const char* game_dll = x_ini_get(&ini, "general", "game_dll", "");
  s_config_resolve_path(&out_config->asset_root, &out_config->runtree_path, asset_root);
  s_config_resolve_path(&out_config->log_file, &out_config->runtree_path, log_file);
  s_config_resolve_path(&out_config->game_dll, &out_config->runtree_path, game_dll);

  // scetion: display
  out_config->width = x_ini_get_i32(&ini, "display", "width", 800);
  out_config->height = x_ini_get_i32(&ini, "display", "height", 600);
  out_config->fullscreen = x_ini_get_bool(&ini, "display", "fullscreen", false);
  const char* icon_path = x_ini_get(&ini, "display", "icon_path", "assets/ldk.ico");
  s_config_resolve_path(&out_config->icon_path, &out_config->runtree_path, icon_path);

  // section: editor
  x_smallstr_from_cstr(&out_config->title, x_ini_get(&ini, "display", "title", "LDK"));
  x_smallstr_from_cstr(&out_config->editor_theme, x_ini_get(&ini, "editor", "theme", "dark"));
  const char* editor_font = x_ini_get(&ini, "editor", "font", "InterDisplay-Regular.ttf");
  s_config_resolve_path(&out_config->editor_font, &out_config->runtree_path, editor_font);
  out_config->editor_font_size = x_ini_get_i32(&ini, "editor", "font_size", 12);

  x_ini_free(&ini);
  return true;
}

static LDKUITextInputState ui_text_input = {0};
static bool s_on_text_event(const LDKEvent* event, void* state)
{
  if (event->text_event.type == LDK_TEXT_EVENT_CHARACTER_INPUT)
  {
    if (ui_text_input.codepoint_count < LDK_UI_INPUT_CODEPOINTS_CAPACITY)
    {
      ui_text_input.codepoints[ui_text_input.codepoint_count++] = event->text_event.character;
      return true;
    }
  }
  return false;
}

float r = 0.0f;
float g = 0.0f;
float b = 0.0f;
bool bool_value = false;
char* msg = "Hello, Sailor!";

XSmallstr text_box1 = {0};
XSmallstr text_box2 = {0};

LDKUIRect w1 = { 10, 100, 400, 300 };
LDKUIRect w2 = { 10, 100, 400, 400 };
LDKUIRect w3 = {0};

typedef enum ConsoleState
{
  CONSOLE_IS_OPENED,
  CONSOLE_IS_OPENING,
  CONSOLE_IS_CLOSED,
  CONSOLE_IS_CLOSING,
} ConsoleState;

ConsoleState console_state = CONSOLE_IS_OPENED;
bool theme = true;

#define RGBA_U32(r, g, b) \
  (((u32)(r) & 0xFFu) << 24 | \
   ((u32)(g) & 0xFFu) << 16 | \
   ((u32)(b) & 0xFFu) << 8  | \
   0xFFu)

LDKUIPoint scroll_pos = {0};

void s_draw_editor_ui(LDKUIContext* ui, float delta_time)
{

  const float step = 1000.0f * delta_time;
  if (console_state == CONSOLE_IS_CLOSING)
  {
    w3.y -= step;
    if (w3.y < -w3.h)
    {
      w3.y = -w3.h;
      console_state = CONSOLE_IS_CLOSED;
    }
  }
  else if (console_state == CONSOLE_IS_OPENING)
  {
    w3.y += step;
    if (w3.y >= 0.0f)
    {
      w3.y = 0.0f;
      console_state = CONSOLE_IS_OPENED;
    }
  }

  {
    w1 = ldk_ui_begin_window(ui, "Window 1", w1);

    ldk_ui_begin_horizontal(ui);
    ldk_ui_set_next_expand_height(ui, true);
    if (ldk_ui_button(ui, "Toggle Console"))
    {
      if (console_state == CONSOLE_IS_OPENED || console_state == CONSOLE_IS_OPENING) {
        console_state = CONSOLE_IS_CLOSING;
      }
      else if (console_state == CONSOLE_IS_CLOSED || console_state == CONSOLE_IS_CLOSING) {
        console_state = CONSOLE_IS_OPENING;
      }
    }

    ldk_ui_set_next_expand_height(ui, true);
    if (ldk_ui_button(ui, "Theme"))
    {
      theme = !theme;
      ldk_ui_set_theme(ui, theme ? LDK_UI_THEME_DEFAULT_LIGHT : LDK_UI_THEME_DEFAULT_DARK, NULL);
    }

    ldk_ui_end_horizontal(ui);
    ldk_ui_set_next_expand_height(ui, true);
    bool_value = ldk_ui_toggle_button(ui, "Toggle", bool_value);

    ldk_ui_begin_horizontal(ui);

    ldk_ui_begin_vertical(ui);
    ldk_ui_set_next_expand_height(ui, true);
    r = ldk_ui_slider(ui, "Slider", r, 0.0f, 255.0f);
    ldk_ui_set_next_expand_height(ui, true);
    g = ldk_ui_slider(ui, "Slider", g, 0.0f, 255.0f);
    ldk_ui_set_next_expand_height(ui, true);
    b = ldk_ui_slider(ui, "Slider", b, 0.0f, 255.0f);
    ldk_ui_end_vertical(ui);

    ldk_ui_set_next_expand_height(ui, true);
    ldk_ui_color_view(ui, RGBA_U32(r, g, b));
    ldk_ui_end_horizontal(ui);


    ldk_ui_label(ui, msg);
    ldk_ui_end_window(ui);
  }
#if 0
  if (bool_value)
  {
    w2 = ldk_ui_begin_window(ui, "Scroll Window", w2);
    scroll_pos = ldk_ui_begin_vertical_scroll_area(ui, scroll_pos);


    ldk_ui_set_next_expand_height(ui, true);
    ui->theme.slider_track_height = ldk_ui_slider(ui, "Slider", ui->theme.slider_track_height, 0.0f, 1.0f);

    ldk_ui_set_next_expand_height(ui, true);
    ui->theme.slider_thumb_width = ldk_ui_slider(ui, "Slider", ui->theme.slider_thumb_width, 0.0f, 1.0f);

    for(u32 i = 0; i < 30; i++)
    {
      ldk_ui_button(ui, "Button");
    }
    ldk_ui_end_vertical_scroll_area(ui);
    ldk_ui_end_window(ui);
  }
#endif

  if (bool_value)
  {
    w2 = ldk_ui_begin_window(ui, "Scroll Window", w2);
    ldk_ui_label(ui, "Box 1");
    ldk_ui_text_box(ui, text_box1.buf, X_SMALLSTR_MAX_LENGTH);

    ldk_ui_begin_disabled(ui, true);
    ldk_ui_label(ui, "Box 2");
    ldk_ui_text_box(ui, text_box2.buf, X_SMALLSTR_MAX_LENGTH);
    ldk_ui_end_disabled(ui);

    if (ldk_ui_button(ui, "copy"))
    {
      memcpy((char*) &text_box2.buf, (char*) &text_box1.buf, strlen(text_box1.buf));
    }
    ldk_ui_end_window(ui);
  }


  if (console_state != CONSOLE_IS_CLOSED)
  { // console
    w3.w = ui->viewport.w;
    w3.h = ui->viewport.h / 3;
    ldk_ui_begin_pane(ui, w3);
    ldk_ui_label(ui, "This is a console with text\nIt suport multiple lines!\nHello, Sailor!");
    ldk_ui_end_pane(ui);
  }

  ui_text_input.codepoint_count = 0;
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
    case LDK_MODULE_ECS:
      return &g_engine.ecs;

    case LDK_MODULE_EVENT:
      return &g_engine.event_queue;

    case LDK_MODULE_LOG:
      return &g_engine.logger;

    case LDK_MODULE_UI:
      return &g_engine.editor_ui;

    case LDK_MODULE_RENDERER:
      return &g_engine.renderer;

    case LDK_MODULE_ASSET_MANAGER:
      return &g_engine.asset_manager;

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

  // Asssign stub functions for unregistered game callbacks
  if (!e->game.initialize) e->game.initialize = s_stub_game_initialize;
  if (!e->game.start) e->game.start = s_stub_game_start;
  if (!e->game.update) e->game.update = s_stub_game_update;
  if (!e->game.stop) e->game.stop = s_stub_game_stop;
  if (!e->game.terminate) e->game.terminate = s_stub_game_terminate;

  x_log_init(&e->logger, XLOG_OUTPUT_BOTH, XLOG_LEVEL_DEBUG, x_fs_path_cstr(&e->config.log_file));
  x_log_info(&e->logger, "========= LDK v%d.%d.%d =========\n", LDK_VERSION_MAJOR, LDK_VERSION_MINOR, LDK_VERSION_PATCH);

  if (!ldk_os_initialize())
  {
    ldk_log_error("Failed to initialize module: OS layer.");
    engine_init_failed = true;
  }

  e->graphics = ldk_os_graphics_context_opengl_create(3, 3, 24, 8);

  if (!ldk_rhi_gl33_initialize(&e->rhi))
  {
    ldk_log_error("Failed to initialize module: RHI.");
    ldk_engine_terminate();
    return false;
  }

  e->window = ldk_os_window_create_with_flags(e->config.title.buf, e->config.width, e->config.height, LDK_WINDOW_FLAG_HIDDEN);
  ldk_os_window_icon_set(e->window, e->config.icon_path.buf);
  ldk_os_graphics_context_current(e->window, e->graphics);

  if (!ldk_event_queue_initialize(&e->event_queue))
  {
    ldk_log_error("Failed to initialize module: Event Queue.");
    engine_init_failed = true;
  }

  if (!ldk_asset_manager_initialize(&e->asset_manager, 16, 1))
  {
    ldk_log_error("Failed to initialize module: Asset Manager.");
    engine_init_failed = true;
  }

  if (!ldk_ecs_initialize(&e->ecs, 64, 1))
  {
    ldk_log_error("Failed to initialize module: ECS.");
    engine_init_failed = true;
  }

  LDKRendererConfig renderer_config;
  renderer_config.rhi = &e->rhi;
  renderer_config.initial_ui_index_capacity = LDK_DEFATUL_UI_INITIAL_INDEX_CAPACITY;
  renderer_config.initial_ui_vertex_capacity = LDK_DEFATUL_UI_INITIAL_VERTEX_CAPACITY;
  if (!ldk_renderer_initialize(&e->renderer, &renderer_config))
  {
    ldk_log_error("Failed to initialize module: UI Renderer.");
    ldk_engine_terminate();
    return false;
  }

  // Load UI editor font
  e->editor_font = ldk_asset_manager_font_load(&e->asset_manager, e->config.editor_font.buf);
  LDKAssetFontData* editor_font_data = ldk_asset_manager_font_get(&e->asset_manager, e->editor_font);
  if (!editor_font_data || !editor_font_data->face)
  {
    ldk_log_error("Failed to load editor font '%s'.", e->config.editor_font.buf);
    ldk_engine_terminate();
    return false;
  }

  LDKFontAtlasDesc font_atlas_desc = {0};
  font_atlas_desc.padding = 1;
  font_atlas_desc.page_width = 256;
  font_atlas_desc.page_height = 256;

  LDKFontInstance* editor_font = ldk_font_get_instance(
      editor_font_data->face,
      (float)e->config.editor_font_size,
      &font_atlas_desc);

  if (!editor_font)
  {
    ldk_log_error("Failed to create editor font instance.");
    ldk_engine_terminate();
    return false;
  }

  { // UI Initialization
    LDKUIConfig ui_cfg = {0};
    ui_cfg.frame_arena_size = 1024 * 4;
    ui_cfg.initial_vertex_capacity = LDK_DEFATUL_UI_INITIAL_VERTEX_CAPACITY;
    ui_cfg.initial_index_capacity = LDK_DEFATUL_UI_INITIAL_INDEX_CAPACITY;
    ui_cfg.initial_command_capacity = LDK_DEFATUL_UI_INITIAL_COMMAND_CAPACITY;
    ui_cfg.initial_window_capacity = LDK_DEFATUL_UI_INITIAL_WINDOW_CAPACITY;
    ui_cfg.initial_id_stack_capacity = LDK_DEFATUL_UI_INITIAL_STACK_CAPACITY;
    ui_cfg.font = editor_font;

    if (strncmp(e->config.editor_theme.buf, "light", 5) == 0)
      ui_cfg.theme = LDK_UI_THEME_DEFAULT_LIGHT;
    else if (strncmp(e->config.editor_theme.buf, "dark", 4) == 0)
      ui_cfg.theme = LDK_UI_THEME_DEFAULT_DARK;
    else
    {
      ldk_log_warning("Unknown Editor theme name '%s'. Default to 'light'.", e->config.editor_theme.buf);
      ui_cfg.theme = LDK_UI_THEME_DEFAULT_LIGHT;
    }

    ui_cfg.font_texture_user = &e->renderer;
    ui_cfg.get_font_page_texture = ldk_renderer_get_font_page_texture_callback;

    if (!ldk_ui_initialize(&e->editor_ui, &ui_cfg))
    {
      ldk_log_error("Failed to initialize module: UI System.");
      engine_init_failed = true;
    }

    // Listen to text events for editor UI
    ldk_event_handler_add(&e->event_queue, s_on_text_event, LDK_EVENT_TYPE_TEXT, NULL);
  }


  g_engine_initialized = true;
  e->previous_ticks = 0;
  e->running = true;
  e->playing = false;
  e->game_initialized = false;

  LDK_ASSERT(!e->game_initialized);
  LDK_ASSERT(!e->ecs.system.is_started);

  // Initialize SceneGraph system
  if (!ldk_ecs_register_system(ldk_scenegraph_system_desc()))
  {
    ldk_log_error("Failed to register engine system: SceneGraph.");
    engine_init_failed = true;
  }

  // Initialize game
  if (!e->game.initialize(&e->game))
  {
    ldk_log_error("Failed to initialize game module.");
    engine_init_failed = true;
  }

  e->game_initialized = true;

  if (!ldk_ecs_system_registry_start(&e->ecs))
  {
    ldk_log_error("Failed to start ECS system registry.");
    engine_init_failed = true;
  }

  if (engine_init_failed)
  {
    ldk_engine_terminate();
    return false;
  }

  ldk_log_info("LDK initialized\n");
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

  if (e->playing)
  {
    return true;
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

  e->game.stop(&e->game);
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

#ifdef LDK_EDITOR
  /*
   * Editor mode is always active in editor builds. Scene view, picking, gizmos,
   * inspectors and other tools should update here even when play mode is off.
   */
  LDKMouseState     mouse_state;
  LDKKeyboardState  kbd_state;
  LDKUIRect ui_viewport = (LDKUIRect){.x = 0.0f, .y = 0.0f, .w = (float) window_size.w, .h = (float) window_size.h};
  ldk_os_mouse_state_get(&mouse_state);
  ldk_os_keyboard_state_get(&kbd_state);
#endif

  { // Update simulation
    ldk_ecs_run_system_bucket(&e->ecs, LDK_SYSTEM_BUCKET_PRE_UPDATE, delta_time);
    if (e->playing)
    {
      e->game.update(&e->game, delta_time);
    }
    ldk_ecs_run_system_bucket(&e->ecs, LDK_SYSTEM_BUCKET_UPDATE, delta_time);
    ldk_ecs_run_system_bucket(&e->ecs, LDK_SYSTEM_BUCKET_POST_UPDATE, delta_time);
  }

  event.type = LDK_EVENT_TYPE_RENDER_BEFORE;
  event.frame_event.ticks = current_ticks;
  event.frame_event.delta_time = delta_time;
  ldk_event_push(&e->event_queue, &event);
  ldk_event_queue_broadcast(&e->event_queue);

  { // Render scene/game

#ifdef LDK_EDITOR
    /* render editor overlays, gizmos and tool UI here */
    ldk_ui_begin_frame(&e->editor_ui, &mouse_state, &kbd_state, &ui_text_input, ui_viewport);
    s_draw_editor_ui(&e->editor_ui, delta_time);
    ldk_ui_end_frame(&e->editor_ui);
    const LDKUIRenderData* ui_data = ldk_ui_get_render_data(&e->editor_ui);
    ldk_renderer_submit_ui(&e->renderer, ui_data);
#endif

    current_ticks = ldk_os_time_ticks_get();

    LDKRendererFrameDesc frame_desc;
    frame_desc.framebuffer_width = window_size.w;
    frame_desc.framebuffer_height = window_size.h;
    frame_desc.clear_color = 0xABABABFFu;
    frame_desc.clear_color_enabled = true;
    ldk_renderer_render_frame(&e->renderer, &frame_desc);
  }

  event.type = LDK_EVENT_TYPE_RENDER_AFTER;
  event.frame_event.ticks = current_ticks;
  event.frame_event.delta_time = delta_time;
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

  // Stop game
  ldk_engine_play_stop();
  if (e->game_initialized)
  {
    e->game.terminate(&e->game);
  }

  e->game_initialized = false;

  // Stop engine modules
  s_terminate_all_modules(e);

  // Cleanup
  memset(e, 0, sizeof(*e));
  g_engine_initialized = false;
  g_handling_signal = 0;
  g_signal_requested_stop = 0;
  g_last_signal = 0;
}

/*
 * Standalone convenience path:
 * Editor hosts are expected to drive frames explicitly and toggle play mode
 * with ldk_engine_play_start/stop.
 */
i32 ldk_engine_run(void)
{
  LDKRoot* e = &g_engine;
  X_ASSERT(g_engine_initialized);
  e->running = true;
  ldk_os_window_show(e->window, true);
  ldk_os_window_fullscreen_set(e->window, e->config.fullscreen);

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
