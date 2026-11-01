#include "tftf_player_character_system.h"

#include "tftf_island_system.h"
#include "../component/tftf_player_character.h"

#include <component/ldk_transform.h>
#include <generated_component_metadata.h>
#include <ldk.h>
#include <module/ldk_ecs.h>
#include <module/ldk_scenegraph.h>
#include <system/ldk_terrain_system.h>
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

  if (tftf_island_tile_kind_at(position, &tile))
  {
    speed_scale *= s_tile_speed_scale(player, tile);
  }

  high_grass_height =
      s_finite_non_negative(player->high_grass_height, 0.0f);
  high_grass_density_min =
      s_finite_non_negative(player->high_grass_density_min, 0.0f);

  if (tftf_island_grass_state_at(position, &density, &min_height) &&
      isfinite(density) && isfinite(min_height) &&
      min_height >= high_grass_height)
  {
    if (cut_grass)
    {
      float cut_scale = isfinite(player->grass_cut_density_scale)
          ? float_clamp(player->grass_cut_density_scale, 0.0f, 1.0f)
          : 1.0f;
      if (tftf_island_grass_density_multiply_at(position, cut_scale))
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

/* The terrain's geometric triangle normal is the authority for walking.
 * Use short segments so a large frame step cannot tunnel through a cliff. */
static bool s_slope_segment_walkable(Vec3 origin, Vec3 destination,
    float max_slope_degrees)
{
  if (max_slope_degrees <= 0.0f || !ldk_terrain_system_is_active())
  {
    return true;
  }

  float dx = destination.x - origin.x;
  float dz = destination.z - origin.z;
  float distance = sqrtf(dx * dx + dz * dz);
  u32 steps = (u32)ceilf(distance / 0.4f);
  if (steps == 0u)
  {
    return true;
  }

  const float min_normal_y = cosf(max_slope_degrees *
      (STDXM_PI / 180.0f));
  for (u32 i = 1u; i <= steps; ++i)
  {
    float t = (float)i / (float)steps;
    Vec3 normal;
    if (!ldk_terrain_normal_at_world(origin.x + dx * t,
            origin.z + dz * t, &normal))
    {
      /* No heightfield: retain the controller's existing behavior. */
      continue;
    }
    if (normal.y < min_normal_y)
    {
      return false;
    }
  }
  return true;
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

  Vec3 delta = vec3_mul(player->velocity, dt);
  Vec3 world_position;
  float max_slope = isfinite(player->max_slope_degrees)
      ? float_clamp(player->max_slope_degrees, 0.0f, 89.0f)
      : 45.0f;

  if (s_entity_world_position(entity, &world_position))
  {
    Vec3 destination = vec3_add(world_position, delta);
    if (!s_slope_segment_walkable(world_position, destination, max_slope))
    {
      /* Try both horizontal axes separately to slide along steep walls. */
      Vec3 along_x = vec3_make(world_position.x + delta.x,
          world_position.y, world_position.z);
      Vec3 along_z = vec3_make(world_position.x,
          world_position.y, world_position.z + delta.z);
      bool x_ok = s_slope_segment_walkable(world_position,
          along_x, max_slope);
      bool z_ok = s_slope_segment_walkable(world_position,
          along_z, max_slope);
      if (x_ok && (!z_ok || fabsf(delta.x) >= fabsf(delta.z)))
      {
        delta.z = 0.0f;
        player->velocity.z = 0.0f;
      }
      else if (z_ok)
      {
        delta.x = 0.0f;
        player->velocity.x = 0.0f;
      }
      else
      {
        delta.x = 0.0f;
        delta.z = 0.0f;
        player->velocity.x = 0.0f;
        player->velocity.z = 0.0f;
      }
    }
  }

  position = vec3_add(position, delta);
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
