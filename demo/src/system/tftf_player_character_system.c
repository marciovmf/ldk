#include "tftf_player_character_system.h"

#include "island_terrain.h"
#include "../component/tftf_player_character.h"

#include <component/ldk_transform.h>
#include <generated_component_metadata.h>
#include <ldk.h>
#include <module/ldk_ecs.h>
#include <module/ldk_scenegraph.h>
#include <stdx/stdx_math.h>

#include <math.h>

static Vec3 s_move_towards(Vec3 current, Vec3 target, float max_delta)
{
  Vec3 delta = vec3_sub(target, current);
  float distance = vec3_len(delta);

  if (distance <= max_delta || distance <= STDXM_EPS)
  {
    return target;
  }

  return vec3_add(current, vec3_mul(delta, max_delta / distance));
}

static Vec3 s_player_input_direction(LDKKeyboardState *keyboard)
{
  Vec3 direction = vec3_make(0.0f, 0.0f, 0.0f);

  if (!keyboard)
  {
    return direction;
  }

  if (ldk_input_keyboard_key_is_pressed(keyboard, LDK_KEYCODE_A))
  {
    direction.x -= 1.0f;
  }
  if (ldk_input_keyboard_key_is_pressed(keyboard, LDK_KEYCODE_D))
  {
    direction.x += 1.0f;
  }
  if (ldk_input_keyboard_key_is_pressed(keyboard, LDK_KEYCODE_W))
  {
    direction.z -= 1.0f;
  }
  if (ldk_input_keyboard_key_is_pressed(keyboard, LDK_KEYCODE_S))
  {
    direction.z += 1.0f;
  }

  if (vec3_len2(direction) > 1.0f)
  {
    direction = vec3_norm(direction);
  }

  return direction;
}

static float s_finite_non_negative(float value, float fallback)
{
  return isfinite(value) ? float_max(value, 0.0f) : fallback;
}

static float s_tile_speed_scale(
    const TFTFPlayerCharacterComponent *player, TFTFIslandTileKind tile)
{
  float speed_scale = 1.0f;

  switch (tile)
  {
  case TFTF_ISLAND_TILE_DEEP_WATER:
    speed_scale = player->deep_water_speed_scale;
    break;
  case TFTF_ISLAND_TILE_SHALLOW_WATER:
    speed_scale = player->shallow_water_speed_scale;
    break;
  case TFTF_ISLAND_TILE_SHORE:
    speed_scale = player->shore_speed_scale;
    break;
  case TFTF_ISLAND_TILE_DRY:
    speed_scale = player->dry_speed_scale;
    break;
  case TFTF_ISLAND_TILE_GRASS:
    speed_scale = player->grass_speed_scale;
    break;
  case TFTF_ISLAND_TILE_FOREST:
    speed_scale = player->forest_speed_scale;
    break;
  case TFTF_ISLAND_TILE_MOUNTAIN:
    speed_scale = player->mountain_speed_scale;
    break;
  default:
    break;
  }

  return s_finite_non_negative(speed_scale, 1.0f);
}

static bool s_entity_world_position(LDKEntity entity, Vec3 *out_position)
{
  Mat4 world;

  if (!out_position || !ldk_scenegraph_update_entity(entity) ||
      !ldk_transform_get_world_matrix(entity, &world))
  {
    return false;
  }

  *out_position = vec3_make(world.m[12], world.m[13], world.m[14]);
  return true;
}

static float s_environment_speed_scale(LDKEntity entity,
    TFTFPlayerCharacterComponent *player, bool cut_grass)
{
  Vec3 position;
  TFTFIslandTileKind tile;
  float speed_scale = 1.0f;
  float density;
  float min_height;
  float high_grass_height;
  float high_grass_density_min;

  if (!player || !s_entity_world_position(entity, &position))
  {
    return 1.0f;
  }

  if (tftf_island_terrain_tile_kind_at(position, &tile))
  {
    speed_scale *= s_tile_speed_scale(player, tile);
  }

  high_grass_height =
      s_finite_non_negative(player->high_grass_height, 0.0f);
  high_grass_density_min =
      s_finite_non_negative(player->high_grass_density_min, 0.0f);

  if (island_terrain_grass_state_at(position, &density, &min_height) &&
      isfinite(density) && isfinite(min_height) &&
      min_height >= high_grass_height)
  {
    if (cut_grass)
    {
      float cut_scale = isfinite(player->grass_cut_density_scale)
          ? float_clamp(player->grass_cut_density_scale, 0.0f, 1.0f)
          : 1.0f;
      if (island_terrain_grass_density_multiply_at(position, cut_scale))
      {
        density *= cut_scale;
      }
    }

    if (density >= high_grass_density_min)
    {
      speed_scale *= s_finite_non_negative(
          player->high_grass_speed_scale, 1.0f);
    }
  }

  return isfinite(speed_scale) ? float_max(speed_scale, 0.0f) : 1.0f;
}

static void s_player_character_update(LDKEntity entity,
    TFTFPlayerCharacterComponent *player, Vec3 input_direction,
    bool cut_grass, float dt)
{
  Vec3 position;
  Vec3 target_velocity;
  float speed;
  float acceleration;
  float inertia;

  if (!player || !ldk_transform_get_local_position(entity, &position))
  {
    return;
  }

  speed = s_finite_non_negative(player->speed, 0.0f);
  speed *= s_environment_speed_scale(entity, player, cut_grass);
  acceleration = s_finite_non_negative(player->acceleration, 0.0f);
  inertia = s_finite_non_negative(player->inertia, 0.0f);

  player->velocity.y = 0.0f;

  if (vec3_len2(input_direction) > STDXM_EPS * STDXM_EPS && speed > 0.0f)
  {
    target_velocity = vec3_mul(input_direction, speed);
    player->velocity = s_move_towards(
        player->velocity, target_velocity, acceleration * dt);
  }
  else if (inertia > STDXM_EPS && speed > 0.0f)
  {
    float stop_acceleration = speed / inertia;
    player->velocity = s_move_towards(player->velocity,
        vec3_make(0.0f, 0.0f, 0.0f), stop_acceleration * dt);
  }
  else
  {
    player->velocity = vec3_make(0.0f, 0.0f, 0.0f);
  }

  position = vec3_add(position, vec3_mul(player->velocity, dt));
  (void)ldk_transform_set_local_position(entity, position);
}

void tftf_player_character_system_update(
    void *data, const LDKEntityGroup *group, float dt)
{
  LDKKeyboardState keyboard;
  Vec3 input_direction;
  bool cut_grass;
  u32 component_type = ldk_component_type(TFTFPlayerCharacterComponent);

  (void)data;

  if (!group || !isfinite(dt) || dt <= 0.0f)
  {
    return;
  }

  ldk_input_keyboard_state_get(&keyboard);
  input_direction = s_player_input_direction(&keyboard);
  cut_grass = ldk_input_keyboard_key_down(&keyboard, LDK_KEYCODE_SPACE);

  for (u32 i = 0u; i < group->count; ++i)
  {
    TFTFPlayerCharacterComponent *player =
        (TFTFPlayerCharacterComponent *)ldk_ecs_component_get(
            group->entities[i], component_type);

    if (!player)
    {
      continue;
    }

    s_player_character_update(
        group->entities[i], player, input_direction, cut_grass, dt);
  }
}
