#include <ldk_common.h>

#define X_IMPL_ARENA
#include <stdx/stdx_arena.h>

#define X_IMPL_ARRAY
#include <stdx/stdx_array.h>

#define X_IMPL_STRING
#include <stdx/stdx_string.h>

#define X_IMPL_STRBUILDER
#include <stdx/stdx_strbuilder.h>

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
#include <ldk_game.h>
#include <ldk_os.h>

#include <ldk_event.h>
#include <component/ldk_camera.h>
#include <component/ldk_mesh_source.h>
#include <component/ldk_transform.h>

#include <module/ldk_system.h>
#include <module/ldk_asset_manager.h>
#include <module/ldk_component.h>
#include <module/ldk_ecs.h>
#include <module/ldk_entity.h>
#include <module/ldk_renderer.h>
#include <module/ldk_scenegraph.h>

#include "ldk_rhi_gl33.h" // we only have one backend inplementation at the moment

#include <signal.h>
#include <string.h>
#include <stdio.h>

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
  XLogger               logger;
  i32                   exit_code;
  bool                  running;
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

static inline void s_broadcast_frame_event(LDKEventType type, float ticks, float delta_time)
{
  LDKRoot* e = &g_engine;
  LDKEvent event = (LDKEvent){
    .type = LDK_EVENT_TYPE_FRAME,
    .frame_event.type = type,
    .frame_event.ticks = ticks,
    .frame_event.delta_time = delta_time
  };

  ldk_event_push(&e->event_queue, &event);
  ldk_event_queue_broadcast(&e->event_queue);
}

void s_game_instance_init_default(LDKGame* game)
{
  if (game == NULL)
  {
    return;
  }

  game->lib = NULL;
  game->initialized = false;
  game->user_data = NULL;
  game->initialize = s_stub_game_initialize;
  game->start = s_stub_game_start;
  game->update = s_stub_game_update;
  game->stop = s_stub_game_stop;
  game->terminate = s_stub_game_terminate;
}

#ifdef LDK_MONOLITHIC
bool ldk_game_instance_load_static(void)
{
  X_ASSERT(g_engine_initialized);
  LDKGame* game = &g_engine.game;

  if (game->initialized)
  {
    return false;
  }

  s_game_instance_init_default(game);

  game->lib = NULL;
  game->initialize = game_initialize;
  game->start = game_start;
  game->update = game_update;
  game->stop = game_stop;
  game->terminate = game_terminate;

  return true;
}
#endif

bool ldk_game_instance_load_from_shared_lib(const char* path)
{
  LDKGame* game = &g_engine.game;
  LDKLibrary* lib;
  void* func_ptr;

  X_ASSERT(g_engine_initialized);

  if (path == NULL || path[0] == 0)
  {
    return false;
  }

  if (game->initialized)
  {
    return false;
  }

  if (game->lib)
  {
    return false;
  }

  lib = ldk_os_library_load(path);
  if (lib == NULL)
  {
    ldk_log_error("Failed to load '%s'\n", path);
    return false;
  }

  s_game_instance_init_default(game);

  game->lib = lib;

  func_ptr = ldk_os_library_fuction_ptr_get(lib, LDK_GAME_INITIALIZE_FUNC_NAME);
  if (func_ptr)
  {
    game->initialize = (LDKGameInitializeFunc) func_ptr;
  }

  func_ptr = ldk_os_library_fuction_ptr_get(lib, LDK_GAME_START_FUNC_NAME);
  if (func_ptr)
  {
    game->start = (LDKGameStartFunc) func_ptr;
  }

  func_ptr = ldk_os_library_fuction_ptr_get(lib, LDK_GAME_UPDATE_FUNC_NAME);
  if (func_ptr)
  {
    game->update = (LDKGameUpdateFunc) func_ptr;
  }

  func_ptr = ldk_os_library_fuction_ptr_get(lib, LDK_GAME_STOP_FUNC_NAME);
  if (func_ptr)
  {
    game->stop = (LDKGameStopFunc) func_ptr;
  }

  func_ptr = ldk_os_library_fuction_ptr_get(lib, LDK_GAME_TERMINATE_FUNC_NAME);
  if (func_ptr)
  {
    game->terminate = (LDKGameTerminateFunc) func_ptr;
  }

  return true;
}

bool ldk_engine_config_from_ini(LDKConfig* out_config, XIni* ini, const char* config_ini_path)
{
  X_ASSERT(out_config != NULL);

  memset(out_config, 0, sizeof(*out_config));

  x_fs_path_set(&out_config->config_file_path, config_ini_path);
  x_fs_path_normalize(&out_config->config_file_path);

  x_fs_path_dirname(&out_config->config_file_path, &out_config->runtree_path);
  x_fs_path_normalize(&out_config->runtree_path);

  XFSPath config_dir;
  const char* project_run_root;

  x_fs_path_dirname(&out_config->config_file_path, &config_dir);
  x_fs_path_normalize(&config_dir);

  // scetion: general
  out_config->initial_ui_index_capacity = x_ini_get_i32(ini, "general", "initial_ui_index_capacity", 256);
  out_config->initial_ui_vertex_capacity = x_ini_get_i32(ini, "general", "initial_ui_vertex_capacity", 256);
  const char* asset_root = x_ini_get(ini, "general", "asset_root", "assets");
  const char* log_file = x_ini_get(ini, "general", "log_file", "ldk.log");
  const char* game_dll = x_ini_get(ini, "general", "game_dll", "");

  s_config_resolve_path(&out_config->asset_root, &out_config->runtree_path, asset_root);
  s_config_resolve_path(&out_config->log_file, &out_config->runtree_path, log_file);
  s_config_resolve_path(&out_config->game_dll, &out_config->runtree_path, game_dll);

  // scetion: display
  out_config->width = x_ini_get_i32(ini, "display", "width", 800);
  out_config->height = x_ini_get_i32(ini, "display", "height", 600);
  out_config->fullscreen = x_ini_get_bool(ini, "display", "fullscreen", false);
  const char* icon_path = x_ini_get(ini, "display", "icon_path", "assets/ldk.ico");
  s_config_resolve_path(&out_config->icon_path, &out_config->runtree_path, icon_path);


  return true;
}

bool ldk_game_instance_initialize(void)
{
  LDKRoot* e = &g_engine;

  X_ASSERT(g_engine_initialized);

  if (e->game.initialized)
  {
    return true;
  }

  ldk_ecs_system_registry_stop(&e->ecs);

  e->game.initialized = e->game.initialize(&e->game);

  if (!ldk_ecs_system_registry_start(&e->ecs))
  {
    e->game.initialized = false;
    return false;
  }

  return e->game.initialized;
}

void ldk_game_instance_terminate(void)
{
  LDKRoot* e = &g_engine;

  X_ASSERT(g_engine_initialized);

  if (!e->game.initialized)
  {
    return;
  }

  e->game.terminate(&e->game);
  e->game.initialized = false;
}

bool ldk_game_instance_unload(void)
{
  LDKRoot* e = &g_engine;
  bool result = true;

  X_ASSERT(g_engine_initialized);

  if (e->game.initialized)
  {
    ldk_game_instance_terminate();
  }

  if (e->game.lib)
  {
    result = ldk_os_library_unload(e->game.lib);
  }

  s_game_instance_init_default(&e->game);
  return result;
}

LDKGame* ldk_game_get(void)
{
  LDKRoot* e = &g_engine;
  if (!e->game.initialized)
    return NULL;

  return &e->game;

}

bool ldk_engine_is_initialized(void)
{
  return g_engine_initialized;
}

bool ldk_game_instance_start(void)
{
  LDK_ASSERT(g_engine_initialized);
  LDKRoot* e = &g_engine;
  return e->game.start(&e->game);
}

void* ldk_module_get(LDKModuleType module_type)
{

  X_ASSERT(g_engine_initialized || module_type == LDK_MODULE_LOG);

  switch (module_type)
  {
    case LDK_MODULE_ECS:
      return &g_engine.ecs;

    case LDK_MODULE_EVENT:
      return &g_engine.event_queue;

    case LDK_MODULE_LOG:
      return &g_engine.logger;

    case LDK_MODULE_RENDERER:
      return &g_engine.renderer;

    case LDK_MODULE_ASSET_MANAGER:
      return &g_engine.asset_manager;

    default:
      break;
  }

  return NULL;
}

bool ldk_engine_initialize(const char* config_ini_path)
{
  X_ASSERT(config_ini_path != NULL);
  LDKConfig config;
  XIni ini;
  XIniError ini_error;
  memset(&ini, 0, sizeof(ini));
  memset(&ini_error, 0, sizeof(ini_error));

  if (!x_ini_load_file(config_ini_path, &ini, &ini_error))
  {
    x_log_error(
        &g_engine.logger,
        "Failed to load config file '%s'. Syntax error at %d:%d: %s",
        config_ini_path,
        ini_error.line,
        ini_error.column,
        ini_error.message ? ini_error.message : "Unknown error");
    return false;
  }

  if (!ldk_engine_config_from_ini(&config, &ini, config_ini_path))
  {
    return false;
  }

  x_fs_path_set(&config.config_file_path, config_ini_path);
  x_fs_path_normalize(&config.config_file_path);

  return ldk_engine_initialize_with_config(&config);
}

bool ldk_engine_initialize_with_config(const LDKConfig* config)
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

  e->window = ldk_os_window_create_with_flags(e->config.title.buf, e->config.width, e->config.height, LDK_WINDOW_FLAG_HIDDEN | LDK_WINDOW_FLAG_CENTERED | LDK_WINDOW_FLAG_NOTITLEBAR);
  ldk_os_window_icon_set(e->window, e->config.icon_path.buf);
  ldk_os_graphics_context_make_current(e->window, e->graphics);

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
  renderer_config.initial_ui_index_capacity = config->initial_ui_index_capacity;
  renderer_config.initial_ui_vertex_capacity = config->initial_ui_vertex_capacity;
  if (!ldk_renderer_initialize(&e->renderer, &renderer_config))
  {
    ldk_log_error("Failed to initialize module: UI Renderer.");
    ldk_engine_terminate();
    return false;
  }

  g_engine_initialized = true;
  e->previous_ticks = 0;
  e->running = true;
  s_game_instance_init_default(&e->game);

  LDK_ASSERT(!e->game.initialized);
  LDK_ASSERT(!e->ecs.system.is_started);

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

LDKWindow ldk_engine_main_window_get(void)
{
  LDK_ASSERT(g_engine_initialized);
  return g_engine.window;
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

  s_broadcast_frame_event(LDK_FRAME_EVENT_UPDATE_BEFORE, current_ticks, delta_time); 
  { // Update simulation
    ldk_ecs_system_bucket_run(&e->ecs, LDK_SYSTEM_BUCKET_PRE_UPDATE, delta_time);

    e->game.update(&e->game, delta_time);

    ldk_scenegraph_update(delta_time); // Update scenegraph
    ldk_ecs_system_bucket_run(&e->ecs, LDK_SYSTEM_BUCKET_UPDATE, delta_time);
    ldk_ecs_system_bucket_run(&e->ecs, LDK_SYSTEM_BUCKET_POST_UPDATE, delta_time);
  }
  s_broadcast_frame_event(LDK_FRAME_EVENT_UPDATE_AFTER, current_ticks, delta_time); 

  s_broadcast_frame_event(LDK_FRAME_EVENT_SUBMIT_BEFORE, current_ticks, delta_time); 
  { // Submit scene

    // Collect scene data from game
    LDKComponentRegistry* component_registry = ldk_ecs_component_registry_get();
    LDKEntityRegistry* entity_registry = ldk_ecs_entity_registry_get();

    // Main camera
    LDKCamera* main_camera = NULL;
    Mat4 camera_view;
    Mat4 camera_projection;
    XArray* all_camera = ldk_component_store_get(component_registry, LDK_COMPONENT_TYPE_CAMERA);
    XArray* camera_owners = ldk_component_owners_get(component_registry, LDK_COMPONENT_TYPE_CAMERA);

    u32 camera_count = x_array_count(all_camera);
    float aspect = (float)window_size.w / (float)window_size.h;

    for (u32 i = 0; i < camera_count; i++)
    {
      LDKCamera* camera = x_array_get(all_camera, i);
      LDKEntity* entity = x_array_get(camera_owners, i);
      LDK_ASSERT(camera);
      LDK_ASSERT(entity);
      if (!camera->enabled || camera->role != LDK_CAMERA_ROLE_MAIN)
      {
        continue;
      }
      ldk_camera_get_view_matrix(*entity, &camera_view);
      ldk_camera_get_projection_matrix(*entity, aspect, &camera_projection);
      ldk_renderer_submit_view(&e->renderer, camera_view, camera_projection);
      main_camera = camera;
      break;
    }

    if (!main_camera && e->game.initialized)
    {
      ldk_log_error("No main camera found!\n");
    }

    // Mesh sources
    XArray* all_mesh = ldk_component_store_get(component_registry, LDK_COMPONENT_TYPE_MESH_SOURCE);
    XArray* mesh_owners = ldk_component_owners_get(component_registry, LDK_COMPONENT_TYPE_MESH_SOURCE);
    u32 mesh_count = x_array_count(all_mesh);

    for (u32 i = 0; i < mesh_count; i++)
    {
      LDKMeshSource* mesh = x_array_get(all_mesh, i);
      LDKEntity* entity = x_array_get(mesh_owners, i);

      if (!mesh || !entity)
      {
        continue;
      }

      Mat4 mesh_world = mat4_identity();

      if (!ldk_transform_get_world_matrix(*entity, &mesh_world))
      {
        continue;
      }

      LDKAssetMeshData* mesh_data = ldk_asset_manager_mesh_get(&e->asset_manager, mesh->source_asset);
      if (mesh_data == NULL)
      {
        continue;
      }

      LDKRendererMeshDesc mesh_desc = {0};
      mesh_desc.vertices = mesh_data->mesh.vertices;
      mesh_desc.vertex_count = mesh_data->mesh.vertex_count;
      mesh_desc.indices = mesh_data->mesh.indices;
      mesh_desc.index_count = mesh_data->mesh.index_count;

      if (!ldk_renderer_mesh_is_valid(&e->renderer, mesh->renderer_mesh))
      {
        mesh->renderer_mesh = ldk_renderer_mesh_create(&e->renderer, &mesh_desc);
        mesh->dirty = false;
      }
      else if (mesh->dirty)
      {
        ldk_renderer_mesh_update(&e->renderer, mesh->renderer_mesh, &mesh_desc);
        mesh->dirty = false;
      }

      if (!ldk_renderer_mesh_is_valid(&e->renderer, mesh->renderer_mesh))
      {
        continue;
      }

      ldk_renderer_submit_mesh(&e->renderer, mesh->renderer_mesh, mesh_world);
    }
  }
  s_broadcast_frame_event(LDK_FRAME_EVENT_SUBMIT_AFTER, current_ticks, delta_time); 

  s_broadcast_frame_event(LDK_FRAME_EVENT_RENDER_BEFORE, current_ticks, delta_time); 
  { // Render scene
    current_ticks = ldk_os_time_ticks_get();
    LDKRendererFrameDesc frame_desc;
    frame_desc.framebuffer_width = window_size.w;
    frame_desc.framebuffer_height = window_size.h;
    frame_desc.clear_color = 0xABABABFFu;
    frame_desc.clear_color_enabled = true;
    ldk_renderer_render_frame(&e->renderer, &frame_desc);

    ldk_os_window_buffers_swap(e->window);
  }
  s_broadcast_frame_event(LDK_FRAME_EVENT_RENDER_AFTER, current_ticks, delta_time); 
}

void ldk_engine_terminate(void)
{
  LDKRoot* e = &g_engine;

  if (!g_engine_initialized)
  {
    return;
  }

  ldk_game_instance_unload();

  s_terminate_all_modules(e);

  memset(e, 0, sizeof(*e));
  g_engine_initialized = false;
  g_handling_signal = 0;
  g_signal_requested_stop = 0;
  g_last_signal = 0;
}

i32 ldk_engine_run(void)
{
  LDKRoot* e = &g_engine;

  X_ASSERT(g_engine_initialized);

  e->running = true;

  ldk_os_window_show(e->window, true);
  ldk_os_window_fullscreen_set(e->window, e->config.fullscreen);

  while (e->running)
  {
    ldk_engine_frame();

    if (ldk_os_window_should_close(e->window))
    {
      e->running = false;
    }
  }

  return e->exit_code;
}

LDKWindow ldk_main_window(void)
{
  return g_engine.window;
}
