#include <ldk_common.h>

#if defined(LDK_GAME)
#if defined(LDK_SHAREDLIB)
#define X_IMPL_ARRAY
#define X_IMPL_STRING
#define X_IMPL_FILESYSTEM
#define X_IMPL_LOG
#define X_IMPL_HASHTABLE
#define X_IMPL_HPOOL
#define X_IMPL_MATH
#define X_IMPL_FILESYSTEM
#endif // LDK_SHAREDLIB
#endif // LDK_GAME

#include <ldk_game.h>
#include <component/ldk_camera.h>


LDKGame game = {0};
const char* game_data = NULL;

bool on_window_event(const LDKEvent* event, void* state)
{
  if (event->window_event.type == LDK_WINDOW_EVENT_CLOSE)
  {
    ldk_log_info("Closing game window\n");
    ldk_engine_stop(0);
    return true;
  }

  return false;
}

bool game_initialize(LDKGame* game)
{
  ldk_log_info("Game initialize\n");
  LDKEventQueue *q = ldk_module_get(LDK_MODULE_EVENT);
  ldk_event_handler_add(q, on_window_event, LDK_EVENT_TYPE_WINDOW, NULL);

  LDKAssetManager* assets = (LDKAssetManager*)ldk_module_get(LDK_MODULE_ASSET_MANAGER);

  LDKMeshVertex cube_vertices[] =
  {
    {{-0.5f, -0.5f, -0.5f}, { 0.0f,  0.0f, -1.0f}, {0.0f, 0.0f}, 0xFF00FF00u},
    {{ 0.5f, -0.5f, -0.5f}, { 0.0f,  0.0f, -1.0f}, {1.0f, 0.0f}, 0xFF00FF00u},
    {{ 0.5f,  0.5f, -0.5f}, { 0.0f,  0.0f, -1.0f}, {1.0f, 1.0f}, 0xFF00FF00u},
    {{-0.5f,  0.5f, -0.5f}, { 0.0f,  0.0f, -1.0f}, {0.0f, 1.0f}, 0xFF00FF00u},

    {{-0.5f, -0.5f,  0.5f}, { 0.0f,  0.0f,  1.0f}, {0.0f, 0.0f}, 0xFF00FF00u},
    {{ 0.5f, -0.5f,  0.5f}, { 0.0f,  0.0f,  1.0f}, {1.0f, 0.0f}, 0xFF00FF00u},
    {{ 0.5f,  0.5f,  0.5f}, { 0.0f,  0.0f,  1.0f}, {1.0f, 1.0f}, 0xFF00FF00u},
    {{-0.5f,  0.5f,  0.5f}, { 0.0f,  0.0f,  1.0f}, {0.0f, 1.0f}, 0xFF00FF00u},
  };

  u32 cube_indices[] =
  {
    0, 1, 2,  2, 3, 0,
    5, 4, 7,  7, 6, 5,
    4, 0, 3,  3, 7, 4,
    1, 5, 6,  6, 2, 1,
    3, 2, 6,  6, 7, 3,
    4, 5, 1,  1, 0, 4
  };

  LDKAssetMesh cube_asset = ldk_asset_manager_mesh_create(
      assets,
      cube_vertices,
      8,
      cube_indices,
      36);

  LDKEntity camera_entity = ldk_ecs_entity_create();

  LDKTransform camera_transform = ldk_transform_make_default();
  camera_transform.local_position = (Vec3){0.0f, 0.0f, 0.0f};
  ldk_ecs_component_add(camera_entity, LDK_COMPONENT_TYPE_TRANSFORM, &camera_transform);

  LDKCamera camera = {0};
  camera.projection = LDK_CAMERA_PROJECTION_PERSPECTIVE;
  camera.role = LDK_CAMERA_ROLE_MAIN;
  camera.fov_y = deg_to_rad(60.0f);
  camera.near_plane = 0.1f;
  camera.far_plane = 100.0f;
  camera.enabled = true;
  ldk_ecs_component_add(camera_entity, LDK_COMPONENT_TYPE_CAMERA, &camera);
  ldk_camera_look_at(camera_entity, vec3_make(0.0f, 0.0f, -2.0f));

  LDKEntity cube_entity = ldk_ecs_entity_create();

  LDKTransform cube_transform = ldk_transform_make_default();
  cube_transform.local_position = (Vec3){0.0f, 0.0f, -2.0f};
  ldk_ecs_component_add(cube_entity, LDK_COMPONENT_TYPE_TRANSFORM, &cube_transform);

  LDKMeshSource mesh_source = {0};
  ldk_mesh_source_set_data(&mesh_source, cube_asset);
  ldk_ecs_component_add(cube_entity, LDK_COMPONENT_TYPE_MESH_SOURCE, &mesh_source);
  return true;
}

bool game_start(LDKGame* game)
{
  ldk_log_info("Game start\n");
  return true;
}

void game_update(LDKGame* game, float delta_time)
{
}

void game_terminate(LDKGame* game)
{
  ldk_log_info("Game terminate\n");
}

void game_stop(LDKGame* game)
{
  ldk_log_info("Game stop\n");
}

i32 run_game(const char* ini_file_path)
{
  game.user_data = &game_data;
  game.initialize = game_initialize;
  game.start = game_start;
  game.update = game_update;
  game.stop = game_stop;
  game.terminate = game_terminate;

  if (!ldk_engine_initialize(&game, ini_file_path))
  {
    return 1;
  }

  ldk_engine_run();
  ldk_engine_terminate();
  return 0;
}

int main(i32 argc, char** argv)
{
  if (argc != 2)
    return 1;

  return run_game(argv[1]);
}

