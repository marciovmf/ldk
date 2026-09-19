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
#include <ldk_mesh.h>
#include <module/ldk_asset_manager.h>
#include <component/ldk_transform.h>
#include <stdx/stdx_math.h>

#include "src/component/player_character.h"
#include "src/system/hello.h"
#include "src/system/island_terrain.h"
#include <generated_component_metadata.h>

#include <math.h>

#define DEMO_ISLAND_MAP_WIDTH 128u
#define DEMO_ISLAND_MAP_HEIGHT 128u

LDKGame game = {0};

static u32 s_island_colors[DEMO_ISLAND_MAP_WIDTH * DEMO_ISLAND_MAP_HEIGHT];

static void s_demo_island_map_build(void)
{
  const u32 deep_water = 0x426F7DFFu;
  const u32 shallow_water = 0x6696A0FFu;
  const u32 sand = 0xC8AA74FFu;
  const u32 grass = 0x78945AFFu;
  const u32 grass_dark = 0x657F4DFFu;
  const u32 rock = 0x77766FFFu;

  for (u32 y = 0u; y < DEMO_ISLAND_MAP_HEIGHT; ++y)
  {
    for (u32 x = 0u; x < DEMO_ISLAND_MAP_WIDTH; ++x)
    {
      float nx =
          ((float)x + 0.5f) / (float)DEMO_ISLAND_MAP_WIDTH * 2.0f - 1.0f;
      float ny =
          ((float)y + 0.5f) / (float)DEMO_ISLAND_MAP_HEIGHT * 2.0f - 1.0f;
      float distortion = 0.06f * sinf((float)x * 0.21f) +
                         0.04f * cosf((float)y * 0.17f);
      float distance = sqrtf(nx * nx + ny * ny) + distortion;
      u32 color;

      if (distance > 0.92f)
      {
        color = deep_water;
      }
      else if (distance > 0.82f)
      {
        color = shallow_water;
      }
      else if (distance > 0.73f)
      {
        color = sand;
      }
      else if (((x / 9u) + (y / 7u)) % 11u == 0u)
      {
        color = rock;
      }
      else if (((x / 5u) + (y / 6u)) % 2u == 0u)
      {
        color = grass;
      }
      else
      {
        color = grass_dark;
      }

      s_island_colors[y * DEMO_ISLAND_MAP_WIDTH + x] = color;
    }
  }
}

void hello_system_update(void *data, const LDKEntityGroup *group, float dt)
{
  Hello *system = (Hello *)data;
  (void)dt;

  if (!system)
  {
    return;
  }

  system->update_count += 1u;
  if (system->log_every_n_updates == 0u ||
      system->update_count % system->log_every_n_updates == 0u)
  {
    ldk_log_info("HELLO! Received %d entities\n", group->count);
  }
}

typedef struct GameData
{
  LDKEntity cube_entity_0;
  LDKEntity cube_entity_1;
  i32 game_width;
  i32 game_height;

} GameData;

static GameData s_game_data;

bool on_window_event(const LDKEvent *event, void *state)
{
  if (event->window_event.type == LDK_WINDOW_EVENT_CLOSE)
  {
    ldk_log_info("Closing game window\n");
    ldk_engine_stop(0);
    return true;
  }

  return false;
}

bool game_initialize(LDKGame *game)
{
  ldk_log_info("Game initialize!!\n");
  game->user_data = &s_game_data;

  s_demo_island_map_build();
  if (!island_terrain_map_set(
          s_island_colors, DEMO_ISLAND_MAP_WIDTH, DEMO_ISLAND_MAP_HEIGHT))
  {
    ldk_log_error("Failed to configure island terrain color map.\n");
    return false;
  }

  if (!player_character_component_register())
  {
    ldk_log_error("Failed to register PlayerCharacterComponent.\n");
    return false;
  }

  if (!game_register_systems())
  {
    ldk_log_error("Failed to register game systems.\n");
    return false;
  }

  LDKEventQueue *q = ldk_module_get(LDK_MODULE_EVENT);
  ldk_event_handler_add(q, on_window_event, LDK_EVENT_TYPE_WINDOW, NULL);
  return true;
}

bool game_start(LDKGame *game)
{
  ldk_log_info("Game start\n");
  GameData *game_data = (GameData *)game->user_data;
  const LDKConfig *cfg = ldk_engine_config_get();
  game_data->game_width = cfg->resolution_width;
  game_data->game_height = cfg->resolution_height;

  LDKAssetManager *assets =
      (LDKAssetManager *)ldk_module_get(LDK_MODULE_ASSET_MANAGER);
  LDKAssetMesh cube_asset =
      ldk_mesh_primitive_asset_get(assets, LDK_MESH_PRIMITIVE_CUBE);

  if (x_handle_is_null(cube_asset.h))
  {
    ldk_log_error("Failed to get built-in cube mesh.\n");
    return false;
  }

  /*
   * The scene already owns the main camera. Do not create another camera here:
   * the terrain system's camera grouping must contain one tracked entity.
   */

  LDKEntity cube_entity_0 = ldk_ecs_entity_create();
  ldk_transform_set_local_position(
      cube_entity_0, vec3_make(0.0f, 0.0f, -3.0f));
  ldk_transform_set_local_rotation(cube_entity_0,
      quat_axis_angle(vec3_make(0.0f, 1.0f, 1.0f), 10.0f));

  LDKEntity cube_entity_1 = ldk_ecs_entity_create();
  ldk_transform_set_parent(cube_entity_1, cube_entity_0);
  ldk_transform_set_local_position(
      cube_entity_1, vec3_make(0.0f, 0.0f, 1.2f));
  ldk_transform_set_local_scale(
      cube_entity_1, vec3_make(0.4f, 0.4f, 0.4f));
  ldk_transform_set_local_rotation(cube_entity_1,
      quat_axis_angle(vec3_make(1.0f, 0.0f, 0.0f), 10.0f));

  LDKMeshSource mesh_source = {0};
  ldk_mesh_source_set_data(&mesh_source, cube_asset);

  // Set material
  LDKMaterialDesc material;
  ldk_material_desc_defaults(LDK_MATERIAL_TYPE_VERTEX_COLOR, &material);
  material.args.vertex_color.color = 0xff0000ffu;
  ldk_mesh_source_set_material(&mesh_source, &material);

  // Set Texture to material descriptor
  /* RGBA bytes: white/blue checkerboard. */
  const u8 pixels[] = {
      255, 255, 255, 255, 0, 80, 255, 255,
      0, 80, 255, 255, 255, 255, 255, 255,
  };

  LDKAssetImage image = ldk_asset_manager_image_create(assets, 2, 2, pixels);

  ldk_material_desc_defaults(LDK_MATERIAL_TYPE_TEXTURED, &material);
  material.args.textured.texture = image;

  ldk_mesh_source_set_material(&mesh_source, &material);

  ldk_ecs_component_add(
      cube_entity_0, LDK_COMPONENT_TYPE_MESH_SOURCE, &mesh_source);
  ldk_ecs_component_add(
      cube_entity_1, LDK_COMPONENT_TYPE_MESH_SOURCE, &mesh_source);

  game_data->cube_entity_0 = cube_entity_0;
  game_data->cube_entity_1 = cube_entity_1;
  return true;
}

void game_update(LDKGame *game, float delta_time)
{
  GameData *game_data = (GameData *)game->user_data;
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
    ldk_log_info("Game input click at %d, %d\n", mouse_state.cursor.x,
        mouse_state.cursor.y);
  }

  (void)delta_time;
}

void game_terminate(LDKGame *game)
{
  LDKEventQueue *q = ldk_module_get(LDK_MODULE_EVENT);
  ldk_event_handler_remove(q, on_window_event);
  ldk_log_info("Game terminate\n");
}

void game_stop(LDKGame *game)
{
  ldk_log_info("Game stop\n");
}
