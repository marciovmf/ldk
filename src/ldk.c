#include "ldk_common.h"
#include "ldk_event.h"
#include "ldk_eventqueue.h"
#include "ldk_ttf.h"
#include "ldk_gl.h"
#include "stdx/stdx_common.h"
#include <stdio.h>

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

#include <ldk_entity.h>
#include <ldk_component.h>
#include <ldk_system.h>
#include <ldk_system_scenegraph.h>
#include <ldk_os.h>
#include <ldk_ui.h>
#include <ldk_ui_renderer.h>
#include <ldk_font_cache.h>
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
  LDKFontCache          font_cache;
  LDKFontFace*          editor_font_face;
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
  ldk_font_cache_terminate(&e->font_cache);
  ldk_system_registry_terminate(&e->system_registry);
  ldk_component_registry_terminate(&e->component_registry);
  ldk_entity_module_terminate(&e->entity_registry);
  ldk_event_queue_terminate(&e->event_queue);
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
  if (event->textEvent.type == LDK_TEXT_EVENT_CHARACTER_INPUT)
  {
    if (ui_text_input.codepoint_count < LDK_UI_INPUT_CODEPOINTS_CAPACITY)
    {
      ui_text_input.codepoints[ui_text_input.codepoint_count++] = event->textEvent.character;
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

    ldk_ui_label(ui, "Box 2");
    ldk_ui_text_box(ui, text_box2.buf, X_SMALLSTR_MAX_LENGTH);

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

    case LDK_MODULE_FONT_CACHE:
      return &g_engine.font_cache;

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
  x_log_info(&e->logger, "=========LDK=========\n");

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
  ui_cfg.font_size = e->config.editor_font_size;
  ui_cfg.font = e->config.editor_font.buf;
  if (strncmp(e->config.editor_theme.buf, "light", 5) == 0)
    ui_cfg.theme = LDK_UI_THEME_DEFAULT_LIGHT;
  else if (strncmp(e->config.editor_theme.buf, "dark", 4) == 0)
    ui_cfg.theme = LDK_UI_THEME_DEFAULT_DARK;
  else
  {
    ldk_log_error("Unknown Editor theme name '%s'. Default to 'light'.", e->config.editor_theme.buf);
    ui_cfg.theme = LDK_UI_THEME_DEFAULT_LIGHT;
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

  { // Font cache initialization
    LDKFontCacheConfig font_cache_config;
    memset(&font_cache_config, 0, sizeof(font_cache_config));
    font_cache_config.initial_page_capacity = 4;

    XFile* font_file = x_io_open(config->editor_font.buf, "rb");
    size_t font_file_size = 0;
    if (!font_file)
    {
      ldk_log_error("Failed to load editor font '%s' .", config->editor_font.buf);
      ldk_engine_terminate();
      return false;
    }
    const char* font_file_data = x_io_read_all(font_file, &font_file_size);
    if (! font_file_data)
    {
      ldk_log_error("Failed to read editor font '%s' .", config->editor_font.buf);
      ldk_engine_terminate();
      return false;
    }

    LDKFontAtlasDesc font_atlas_desc = {0};
    font_atlas_desc.padding = 1;
    font_atlas_desc.page_width = 256;
    font_atlas_desc.page_height = 256;

    e->editor_font_face = ldk_font_face_create(font_file_data, (u32)font_file_size);
    LDKFontInstance* editor_font =  ldk_font_get_instance(
        e->editor_font_face,
        (float)e->config.editor_font_size,
        &font_atlas_desc);

    if (!ldk_font_cache_initialize(&e->font_cache, editor_font, &font_cache_config))
    {
      ldk_log_error("Failed to initialize module: UI Renderer.");
      ldk_engine_terminate();
      return false;
    }
  }

  { // UI Initialization
    ui_cfg.font_texture_user = &e->font_cache;
    ui_cfg.get_font_page_texture = ldk_font_cache_get_page_texture_callback;

    if (!ldk_ui_initialize(&e->editor_ui, &ui_cfg))
    {
      ldk_log_error("Failed to initialize module: UI System.");
      engine_init_failed = true;
    }
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

  ldk_event_handler_add(&e->event_queue, s_on_text_event, LDK_EVENT_TYPE_TEXT, NULL);

  e->running = true;
  e->playing = false;
  e->previous_ticks = 0;
  g_engine_initialized = true;
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

#ifdef LDK_EDITOR
  LDKMouseState     mouse_state;
  LDKKeyboardState  kbd_state;
  LDKUIRect ui_viewport = (LDKUIRect){.x = 0.0f, .y = 0.0f, .w = (float) window_size.w, .h = (float) window_size.h};
  ldk_os_mouse_state_get(&mouse_state);
  ldk_os_keyboard_state_get(&kbd_state);

  /*
   * Editor mode is always active in editor builds. Scene view, picking, gizmos,
   * inspectors and other tools should update here even when play mode is off.
   */
#endif

  glViewport(0, 0, window_size.w, window_size.h);

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
  ldk_ui_begin_frame(&e->editor_ui, &mouse_state, &kbd_state, &ui_text_input, ui_viewport);
  s_draw_editor_ui(&e->editor_ui, delta_time);
  ldk_ui_end_frame(&e->editor_ui);
#endif

  current_ticks = ldk_os_time_ticks_get();

  const LDKUIRenderData* ui_data = ldk_ui_get_render_data(&e->editor_ui);
  ldk_ui_renderer_draw(&e->ui_renderer,
      ui_data,
      window_size.w,
      window_size.h);

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
