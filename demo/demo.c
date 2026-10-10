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

#include "src/component/tftf_bulb_plant_ai.h"
#include "src/component/tftf_projectile.h"
#include "src/component/tftf_player_character.h"
#include "src/system/hello.h"
#include "src/system/island_terrain.h"
#include "src/system/tftf_bulb_plant_ai_system.h"
#include "src/system/tftf_bullet_system.h"
#include "src/system/tftf_player_character_system.h"
#include <generated_component_metadata.h>

LDKGame game = {0};

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
  LDKEntity animated_cube;
  LDKAssetKeyframeAnimation animated_clip;
  bool animated_cube_created;
  i32 game_width;
  i32 game_height;

} GameData;

static GameData s_game_data;

/**
 * @brief Create a visible cube and play a code-authored rotation animation.
 * @param game_data Demo state that owns the resulting entity and asset.
 * @param assets Initialized engine asset manager.
 * @param cube_asset Engine-owned primitive cube mesh.
 * @return True if the entity, clip, and playback were initialized.
 */
static bool demo_create_rotation_animation(GameData *game_data,
    LDKAssetManager *assets, LDKAssetMesh cube_asset)
{
  if (!game_data || !assets || x_handle_is_null(cube_asset.h))
  {
    return false;
  }

  LDKEntity entity = ldk_ecs_entity_create();
  if (x_handle_is_null(entity))
  {
    return false;
  }

  LDKKeyframeAnimation clip;
  ldk_keyframe_animation_init(&clip);
  LDKAssetKeyframeAnimation asset = ldk_asset_keyframe_animation_null();
  bool success = false;

  LDKMeshSource mesh_source = {0};
  LDKMaterialDesc material;
  ldk_material_desc_defaults(LDK_MATERIAL_TYPE_VERTEX_COLOR, &material);
  material.args.vertex_color.color = 0xffa040ffu;

  if (!ldk_transform_set_local_position(
          entity, vec3_make(2.0f, 0.0f, -3.0f)) ||
      !ldk_mesh_source_set_data(&mesh_source, cube_asset) ||
      !ldk_mesh_source_set_material(&mesh_source, &material) ||
      !ldk_ecs_component_add(
          entity, LDK_COMPONENT_TYPE_MESH_SOURCE, &mesh_source))
  {
    goto cleanup;
  }

  LDKKeyFrameAnimationSource source =
      ldk_keyframe_animation_source_make_default();
  source.loop = true;
  source.play_on_start = false; /* Explicit Play below. */
  if (!ldk_ecs_component_add(
          entity, LDK_COMPONENT_TYPE_KEYFRAME_ANIMATION_SOURCE, &source))
  {
    goto cleanup;
  }

  i32 track = ldk_keyframe_animation_track_add(&clip,
      ldk_keyframe_path_root(), LDK_COMPONENT_TYPE_TRANSFORM,
      "local_rotation");
  if (track < 0)
  {
    goto cleanup;
  }

  const Quat rotations[3] = {
      quat_id(),
      quat_axis_angle(vec3_make(0.0f, 1.0f, 0.0f), deg_to_rad(90.0f)),
      quat_id(),
  };
  const float times[3] = {0.0f, 1.0f, 2.0f};

  for (u32 i = 0; i < 3; ++i)
  {
    LDKPropertyValue value = {0};
    value.type = LDK_FIELD_QUAT;
    value.vector = vec4_make(rotations[i].x, rotations[i].y,
        rotations[i].z, rotations[i].w);
    if (!ldk_keyframe_animation_key_set(&clip, (u32)track, times[i], value))
    {
      goto cleanup;
    }
  }

  asset = ldk_asset_manager_keyframe_animation_create(assets, &clip);
  if (!ldk_asset_manager_keyframe_animation_is_alive(assets, asset) ||
      !ldk_keyframe_animation_source_add(entity, asset) ||
      !ldk_keyframe_animation_source_play(entity))
  {
    goto cleanup;
  }

  game_data->animated_cube = entity;
  game_data->animated_clip = asset;
  game_data->animated_cube_created = true;
  success = true;

cleanup:
  /* The asset manager deep-copies the clip; its temporary memory is ours. */
  ldk_keyframe_animation_clear(&clip);
  if (!success)
  {
    /* Destroy the source before unloading the asset referenced by it. */
    ldk_ecs_entity_destroy(entity);
    if (ldk_asset_manager_keyframe_animation_is_alive(assets, asset))
    {
      ldk_asset_manager_keyframe_animation_unload(assets, asset);
    }
  }
  return success;
}

/**
 * @brief Release the demo entity and its temporary in-memory animation asset.
 * @param game_data Demo state whose animation should be released.
 * @return Nothing.
 */
static void demo_destroy_rotation_animation(GameData *game_data)
{
  if (!game_data || !game_data->animated_cube_created)
  {
    return;
  }

  ldk_ecs_entity_destroy(game_data->animated_cube);
  LDKAssetManager *assets =
      (LDKAssetManager *)ldk_module_get(LDK_MODULE_ASSET_MANAGER);
  if (assets && ldk_asset_manager_keyframe_animation_is_alive(
          assets, game_data->animated_clip))
  {
    ldk_asset_manager_keyframe_animation_unload(
        assets, game_data->animated_clip);
  }
  game_data->animated_cube_created = false;
}

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

  if (!tftf_player_character_component_register())
  {
    ldk_log_error("Failed to register TFTFPlayerCharacterComponent.\n");
    return false;
  }

  if (!tftf_projectile_component_register())
  {
    ldk_log_error("Failed to register TFTFProjectileComponent.\n");
    return false;
  }

  if (!tftf_bulb_plant_ai_component_register())
  {
    ldk_log_error("Failed to register TFTFBulbPlantAIComponent.\n");
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

  if (!demo_create_rotation_animation(game_data, assets, cube_asset))
  {
    ldk_log_error("Failed to create the procedural rotation animation.\n");
    return false;
  }

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
  demo_destroy_rotation_animation(&s_game_data);
  LDKEventQueue *q = ldk_module_get(LDK_MODULE_EVENT);
  ldk_event_handler_remove(q, on_window_event);
  ldk_log_info("Game terminate\n");
}

void game_stop(LDKGame *game)
{
  demo_destroy_rotation_animation(&s_game_data);
  ldk_log_info("Game stop\n");
}
