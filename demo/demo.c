#include <ldk_common.h>

#if defined(LDK_SHAREDLIB)
#define X_IMPL_MATH
#define X_IMPL_ARRAY
#define X_IMPL_STRING
#define X_IMPL_FILESYSTEM
#define X_IMPL_LOG
#define X_IMPL_HASHTABLE
#define X_IMPL_HPOOL
#define X_IMPL_MATH
#define X_IMPL_FILESYSTEM
#endif // LDK_SHAREDLIB

#include <ldk_game.h>
#include <component/ldk_camera.h>
#include <component/ldk_transform.h>
#include <stdx/stdx_math.h>

LDKGame game = {0};
typedef struct GameData
{
  LDKEntity cube_entity_0; 
  LDKEntity cube_entity_1; 
  i32 game_width;
  i32 game_height;

}GameData;

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
  ldk_log_info("Game initialize!!\n");
  LDKEventQueue *q = ldk_module_get(LDK_MODULE_EVENT);
  ldk_event_handler_add(q, on_window_event, LDK_EVENT_TYPE_WINDOW, NULL);
  return true;
}

bool game_start(LDKGame* game)
{
  ldk_log_info("Game start\n");
  GameData* game_data = (GameData*) game;
  const LDKConfig* cfg = ldk_engine_config_get();
  game_data->game_width = cfg->resolution_width;
  game_data->game_height = cfg->resolution_height;

  LDKAssetManager* assets = (LDKAssetManager*)ldk_module_get(LDK_MODULE_ASSET_MANAGER);

  LDKMeshVertex cube_vertices[] =
  {
    {{-0.5f, -0.5f, -0.5f}, { 0.0f,  0.0f, -1.0f}, {0.0f, 0.0f}, 0xFF00FF00u},
    {{ 0.5f, -0.5f, -0.5f}, { 0.0f,  0.0f, -1.0f}, {1.0f, 0.0f}, 0xFF00FF00u},
    {{ 0.5f,  0.5f, -0.5f}, { 0.0f,  0.0f, -1.0f}, {1.0f, 1.0f}, 0xFF00FF00u},
    {{-0.5f,  0.5f, -0.5f}, { 0.0f,  0.0f, -1.0f}, {0.0f, 1.0f}, 0xFF00FF00u},

    {{ 0.5f, -0.5f,  0.5f}, { 0.0f,  0.0f,  1.0f}, {0.0f, 0.0f}, 0xFF00FF00u},
    {{-0.5f, -0.5f,  0.5f}, { 0.0f,  0.0f,  1.0f}, {1.0f, 0.0f}, 0xFF00FF00u},
    {{-0.5f,  0.5f,  0.5f}, { 0.0f,  0.0f,  1.0f}, {1.0f, 1.0f}, 0xFF00FF00u},
    {{ 0.5f,  0.5f,  0.5f}, { 0.0f,  0.0f,  1.0f}, {0.0f, 1.0f}, 0xFF00FF00u},

    {{-0.5f, -0.5f,  0.5f}, {-1.0f,  0.0f,  0.0f}, {0.0f, 0.0f}, 0xFF00FF00u},
    {{-0.5f, -0.5f, -0.5f}, {-1.0f,  0.0f,  0.0f}, {1.0f, 0.0f}, 0xFF00FF00u},
    {{-0.5f,  0.5f, -0.5f}, {-1.0f,  0.0f,  0.0f}, {1.0f, 1.0f}, 0xFF00FF00u},
    {{-0.5f,  0.5f,  0.5f}, {-1.0f,  0.0f,  0.0f}, {0.0f, 1.0f}, 0xFF00FF00u},

    {{ 0.5f, -0.5f, -0.5f}, { 1.0f,  0.0f,  0.0f}, {0.0f, 0.0f}, 0xFF00FF00u},
    {{ 0.5f, -0.5f,  0.5f}, { 1.0f,  0.0f,  0.0f}, {1.0f, 0.0f}, 0xFF00FF00u},
    {{ 0.5f,  0.5f,  0.5f}, { 1.0f,  0.0f,  0.0f}, {1.0f, 1.0f}, 0xFF00FF00u},
    {{ 0.5f,  0.5f, -0.5f}, { 1.0f,  0.0f,  0.0f}, {0.0f, 1.0f}, 0xFF00FF00u},

    {{-0.5f,  0.5f, -0.5f}, { 0.0f,  1.0f,  0.0f}, {0.0f, 0.0f}, 0xFF00FF00u},
    {{ 0.5f,  0.5f, -0.5f}, { 0.0f,  1.0f,  0.0f}, {1.0f, 0.0f}, 0xFF00FF00u},
    {{ 0.5f,  0.5f,  0.5f}, { 0.0f,  1.0f,  0.0f}, {1.0f, 1.0f}, 0xFF00FF00u},
    {{-0.5f,  0.5f,  0.5f}, { 0.0f,  1.0f,  0.0f}, {0.0f, 1.0f}, 0xFF00FF00u},

    {{-0.5f, -0.5f,  0.5f}, { 0.0f, -1.0f,  0.0f}, {0.0f, 0.0f}, 0xFF00FF00u},
    {{ 0.5f, -0.5f,  0.5f}, { 0.0f, -1.0f,  0.0f}, {1.0f, 0.0f}, 0xFF00FF00u},
    {{ 0.5f, -0.5f, -0.5f}, { 0.0f, -1.0f,  0.0f}, {1.0f, 1.0f}, 0xFF00FF00u},
    {{-0.5f, -0.5f, -0.5f}, { 0.0f, -1.0f,  0.0f}, {0.0f, 1.0f}, 0xFF00FF00u},
  };

  u32 cube_indices[] =
  {
    0, 2, 1,  0, 3, 2,       // -Z
    4, 6, 5,  4, 7, 6,       // +Z
    8, 10, 9,  8, 11, 10,    // -X
    12, 14, 13,  12, 15, 14, // +X
    16, 18, 17,  16, 19, 18, // +Y
    20, 22, 21,  20, 23, 22  // -Y
  };

  LDKAssetMesh cube_asset = ldk_asset_manager_mesh_create(
      assets, cube_vertices, 24, cube_indices, 36);

  LDKEntity camera_entity = ldk_ecs_entity_create();
  ldk_transform_set_local_position(camera_entity, vec3_make(0.0f, 0.0f, 0.0f));

  LDKCamera camera = {0};
  camera.projection = LDK_CAMERA_PROJECTION_PERSPECTIVE;
  camera.role = LDK_CAMERA_ROLE_MAIN;
  camera.fov_y = deg_to_rad(40.0f);
  camera.near_plane = 0.1f;
  camera.far_plane = 100.0f;
  camera.enabled = true;
  ldk_ecs_component_add(camera_entity, LDK_COMPONENT_TYPE_CAMERA, &camera);
  ldk_camera_look_at(camera_entity, vec3_make(0.0f, 0.0f, -1.0f));

  LDKEntity cube_entity_0 = ldk_ecs_entity_create();
  ldk_transform_set_local_position(cube_entity_0, vec3_make(0.0f, 0.0f, -3.0f));
  ldk_transform_set_local_rotation(cube_entity_0, quat_axis_angle(vec3_make(0.0f, 1.0f, 1.0f), 10.0f));

  LDKEntity cube_entity_1 = ldk_ecs_entity_create();
  ldk_transform_set_parent(cube_entity_1, cube_entity_0);
  ldk_transform_set_local_position(cube_entity_1, vec3_make(0.0f, 0.0f, 1.2f));
  ldk_transform_set_local_scale(cube_entity_1, vec3_make(0.4f, 0.4f, 0.4f));
  ldk_transform_set_local_rotation(cube_entity_1, quat_axis_angle(vec3_make(0.0f, 0.0f, 1.0f), 10.0f));

  LDKMeshSource mesh_source = {0};
  ldk_mesh_source_set_data(&mesh_source, cube_asset);
  ldk_ecs_component_add(cube_entity_0, LDK_COMPONENT_TYPE_MESH_SOURCE, &mesh_source);
  ldk_ecs_component_add(cube_entity_1, LDK_COMPONENT_TYPE_MESH_SOURCE, &mesh_source);

  game_data->cube_entity_0 = cube_entity_0;
  game_data->cube_entity_1 = cube_entity_1;
  return true;
}

void game_update(LDKGame* game, float delta_time)
{
  GameData* game_data = (GameData*) game;
  LDKMouseState mouse_state;
  ldk_input_mouse_state_get(&mouse_state);

  if (mouse_state.cursor.x >= 0 && mouse_state.cursor.y >= 0)
  {
    float cursor_x = (float)mouse_state.cursor.x / game_data->game_width;
    float cursor_y = (float)mouse_state.cursor.y / game_data->game_height;
    float yaw = (cursor_x - 0.5f) * deg_to_rad(180.0f);
    float pitch = (cursor_y - 0.5f) * deg_to_rad(180.0f);

    ldk_transform_set_local_rotation(game_data->cube_entity_0,
        quat_axis_angle(vec3_make(0.0f, 1.0f, 0.0f), yaw));
    ldk_transform_set_local_rotation(game_data->cube_entity_1,
        quat_axis_angle(vec3_make(1.0f, 0.0f, 0.0f), pitch));
  }

  if (ldk_input_mouse_button_down(&mouse_state, LDK_MOUSE_BUTTON_LEFT))
  {
    ldk_log_info("Game input click at %d, %d\n",
        mouse_state.cursor.x, mouse_state.cursor.y);
  }

  (void)delta_time;
}

void game_terminate(LDKGame* game)
{
  LDKEventQueue *q = ldk_module_get(LDK_MODULE_EVENT);
  ldk_event_handler_remove(q, on_window_event);
  ldk_log_info("Game terminate\n");
}

void game_stop(LDKGame* game)
{
  ldk_log_info("Game stop\n");
}
