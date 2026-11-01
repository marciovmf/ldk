#include "tftf_player_character_system.h"

#include "island_terrain.h"
#include "../component/tftf_player_character.h"

#include <component/ldk_camera.h>
#include <component/ldk_terrain_follower.h>
#include <component/ldk_transform.h>
#include <generated_component_metadata.h>
#include <ldk.h>
#include <ldk_raycast.h>
#include <module/ldk_asset_manager.h>
#include <module/ldk_ecs.h>
#include <module/ldk_scenegraph.h>
#include <stdx/stdx_math.h>

#include <math.h>

typedef struct TFTFPlayerCameraSearch
{
  LDKEntity player;
  LDKEntity camera;
} TFTFPlayerCameraSearch;

static bool s_player_camera_search(LDKEntity entity, void *user)
{
  TFTFPlayerCameraSearch *search = (TFTFPlayerCameraSearch *)user;
  LDKEntity parent;
  LDKCamera *camera;

  if (!search)
  {
    return false;
  }

  camera = (LDKCamera *)ldk_ecs_component_get(
      entity, LDK_COMPONENT_TYPE_CAMERA);
  if (!camera || !camera->enabled || camera->role != LDK_CAMERA_ROLE_MAIN)
  {
    return true;
  }

  parent = ldk_transform_get_parent(entity);
  if (parent.index == search->player.index &&
      parent.version == search->player.version)
  {
    search->camera = entity;
    return false;
  }

  return true;
}

static LDKEntity s_player_camera_find(LDKEntity player)
{
  TFTFPlayerCameraSearch search = {0};

  search.player = player;
  search.camera = x_handle_null();
  (void)ldk_ecs_entity_foreach(s_player_camera_search, &search);
  return search.camera;
}

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

static Vec3 s_player_input_direction(
    LDKKeyboardState *keyboard, const Mat4 *camera_world)
{
  Vec3 direction = vec3_make(0.0f, 0.0f, 0.0f);
  Vec3 right = vec3_make(1.0f, 0.0f, 0.0f);
  Vec3 forward = vec3_make(0.0f, 0.0f, -1.0f);
  float horizontal = 0.0f;
  float vertical = 0.0f;

  if (!keyboard)
  {
    return direction;
  }

  if (ldk_input_keyboard_key_is_pressed(keyboard, LDK_KEYCODE_A))
  {
    horizontal -= 1.0f;
  }
  if (ldk_input_keyboard_key_is_pressed(keyboard, LDK_KEYCODE_D))
  {
    horizontal += 1.0f;
  }
  if (ldk_input_keyboard_key_is_pressed(keyboard, LDK_KEYCODE_W))
  {
    vertical += 1.0f;
  }
  if (ldk_input_keyboard_key_is_pressed(keyboard, LDK_KEYCODE_S))
  {
    vertical -= 1.0f;
  }

  if (camera_world)
  {
    right = vec3_make(camera_world->m[0], 0.0f, camera_world->m[2]);
    forward = vec3_make(-camera_world->m[8], 0.0f,
        -camera_world->m[10]);
    if (vec3_len2(right) <= STDXM_EPS * STDXM_EPS ||
        vec3_len2(forward) <= STDXM_EPS * STDXM_EPS)
    {
      return direction;
    }
    right = vec3_norm(right);
    forward = vec3_norm(forward);
  }

  direction = vec3_add(vec3_mul(right, horizontal),
      vec3_mul(forward, vertical));
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
    const TFTFPlayerCharacterComponent *player)
{
  Vec3 position;
  TFTFIslandTileKind tile;
  float speed_scale = 1.0f;
  float density;
  float min_height;

  if (!player || !s_entity_world_position(entity, &position))
  {
    return 1.0f;
  }

  if (!tftf_island_terrain_tile_kind_at(position, &tile))
  {
    return 1.0f;
  }

  speed_scale = s_tile_speed_scale(player, tile);

  if (island_terrain_grass_state_at(position, &density, &min_height) &&
      isfinite(density) && isfinite(min_height))
  {
    if (tile == TFTF_ISLAND_TILE_THORNS)
    {
      /* Low residual density is no longer an obstacle after cutting. */
      if (density >= s_finite_non_negative(player->thorns_density_min, 0.0f))
      {
        speed_scale *= s_finite_non_negative(
            player->thorns_speed_scale, 1.0f);
      }
    }
    else if (min_height >= s_finite_non_negative(
                 player->high_grass_height, 0.0f) &&
        density >= s_finite_non_negative(
            player->high_grass_density_min, 0.0f))
    {
      speed_scale *= s_finite_non_negative(
          player->high_grass_speed_scale, 1.0f);
    }
  }

  return isfinite(speed_scale) ? float_max(speed_scale, 0.0f) : 1.0f;
}

static bool s_player_face_direction(LDKEntity entity, Vec3 direction)
{
  if (vec3_len2(direction) <= STDXM_EPS * STDXM_EPS)
  {
    return false;
  }

  Quat facing = quat_axis_angle(vec3_make(0.0f, 1.0f, 0.0f),
      atan2f(-direction.x, -direction.z));
  return ldk_transform_set_local_rotation(entity, facing);
}

static void s_player_cut_vegetation(LDKEntity entity,
    const TFTFPlayerCharacterComponent *player)
{
  TFTFIslandTileKind tile;
  Mat4 world;
  Vec3 forward;
  Vec3 target;
  float density;
  float min_height;
  float cut_strength;
  float distance;

  if (!player || !ldk_scenegraph_update_entity(entity) ||
      !ldk_transform_get_world_matrix(entity, &world))
  {
    return;
  }

  distance = s_finite_non_negative(player->cut_distance, 1.0f);
  forward = mat4_mul_dir(world, vec3_make(0.0f, 0.0f, -1.0f));
  forward.y = 0.0f;
  if (distance <= 0.0f || vec3_len2(forward) <= STDXM_EPS * STDXM_EPS)
  {
    return;
  }

  forward = vec3_norm(forward);
  target = vec3_add(vec3_make(world.m[12], world.m[13], world.m[14]),
      vec3_mul(forward, distance));
  if (!tftf_island_terrain_tile_kind_at(target, &tile) ||
      !island_terrain_grass_state_at(target, &density, &min_height) ||
      !isfinite(density) || !isfinite(min_height) || density <= 0.0f)
  {
    return;
  }

  if (tile == TFTF_ISLAND_TILE_THORNS)
  {
    cut_strength = player->thorns_cut_density_scale;
  }
  else
  {
    if (min_height < s_finite_non_negative(player->high_grass_height, 0.0f))
    {
      return;
    }
    cut_strength = player->grass_cut_density_scale;
  }

  /* The island operation multiplies the density that remains, rather than
   * the fraction removed. A strength of 0 leaves it untouched; 1 clears it. */
  cut_strength = isfinite(cut_strength)
      ? float_clamp(cut_strength, 0.0f, 1.0f) : 0.0f;
  if (cut_strength > 0.0f)
  {
    (void)island_terrain_grass_density_multiply_at(
        target, 1.0f - cut_strength);
  }
}

/* Keep the camera's orbit in world space even while the player turns to aim.
 * The initial camera transform and initial player rotation are read from the
 * authored scene; no camera entity or scene changes are created at runtime. */
static bool s_player_camera_orbit(LDKEntity entity, LDKEntity camera,
    TFTFPlayerCharacterComponent *player, float orbit_delta)
{
  Quat player_rotation;
  Quat baseline;
  Quat desired_camera_rotation;
  Quat inverse_player;
  Vec3 desired_camera_position;

  if (!player || x_handle_is_null(camera) ||
      !ldk_transform_get_local_rotation(entity, &player_rotation))
  {
    return false;
  }

  if (!player->camera_initialized)
  {
    if (!ldk_transform_get_local_position(
            camera, &player->camera_initial_position) ||
        !ldk_transform_get_local_rotation(
            camera, &player->camera_initial_rotation))
    {
      return false;
    }
    player->player_initial_rotation = player_rotation;
    player->camera_initialized = true;
  }

  player->camera_yaw += orbit_delta;
  if (fabsf(player->camera_yaw) > 2.0f * STDXM_PI)
  {
    player->camera_yaw = fmodf(player->camera_yaw, 2.0f * STDXM_PI);
  }

  baseline = quat_mul(player->player_initial_rotation,
      quat_axis_angle(vec3_make(0.0f, 1.0f, 0.0f), player->camera_yaw));
  inverse_player = quat_inverse(player_rotation);
  desired_camera_position = quat_mul_vec3(quat_mul(inverse_player, baseline),
      player->camera_initial_position);
  desired_camera_rotation = quat_mul(quat_mul(inverse_player, baseline),
      player->camera_initial_rotation);

  return ldk_transform_set_local_position(camera, desired_camera_position) &&
      ldk_transform_set_local_rotation(camera, desired_camera_rotation) &&
      ldk_scenegraph_update_entity(camera);
}

static bool s_player_cursor_aim(LDKEntity entity, LDKEntity camera,
    LDKMouseState *mouse, Vec3 *out_direction, Vec3 *out_position)
{
  const LDKConfig *config = ldk_engine_config_get();
  LDKCamera *camera_component;
  LDKPoint cursor;
  Vec3 player_position;
  Vec3 plane_position;
  Vec3 camera_position;
  const LDKTerrainFollowerComponent *follower;
  Vec3 ray_origin;
  Vec3 ray_direction;
  Mat4 camera_world;
  Mat4 projection;
  LDKRay ray;
  LDKRaycastHit hit;
  float ndc_x;
  float ndc_y;
  float aspect;

  if (!config || !mouse || !out_direction || !out_position ||
      config->resolution_width <= 0 || config->resolution_height <= 0 ||
      x_handle_is_null(camera))
  {
    return false;
  }

  cursor = ldk_input_mouse_cursor(mouse);
  if (cursor.x < 0 || cursor.y < 0 ||
      cursor.x >= config->resolution_width ||
      cursor.y >= config->resolution_height)
  {
    return false;
  }

  camera_component = (LDKCamera *)ldk_ecs_component_get(
      camera, LDK_COMPONENT_TYPE_CAMERA);
  if (!camera_component || !s_entity_world_position(entity, &player_position) ||
      !ldk_scenegraph_update_entity(camera) ||
      !ldk_camera_get_world_matrix(camera, &camera_world))
  {
    return false;
  }

  aspect = (float)config->resolution_width / (float)config->resolution_height;
  if (!ldk_camera_get_projection_matrix(camera, aspect, &projection))
  {
    return false;
  }

  ndc_x = 2.0f * ((float)cursor.x + 0.5f) /
      (float)config->resolution_width - 1.0f;
  ndc_y = 1.0f - 2.0f * ((float)cursor.y + 0.5f) /
      (float)config->resolution_height;
  camera_position = vec3_make(camera_world.m[12],
      camera_world.m[13], camera_world.m[14]);

  if (camera_component->projection == LDK_CAMERA_PROJECTION_ORTHOGRAPHIC)
  {
    Vec3 local_offset = vec3_make(ndc_x /
        projection.m[0], ndc_y / projection.m[5], 0.0f);
    ray_origin = vec3_add(camera_position,
        mat4_mul_dir(camera_world, local_offset));
    ray_direction = mat4_mul_dir(camera_world,
        vec3_make(0.0f, 0.0f, -1.0f));
  }
  else
  {
    ray_origin = camera_position;
    ray_direction = mat4_mul_dir(camera_world,
        vec3_make(ndc_x / projection.m[0],
            ndc_y / projection.m[5], -1.0f));
  }

  /* Aim at the player's ground plane rather than the capsule's pivot. */
  plane_position = player_position;
  follower = (const LDKTerrainFollowerComponent *)ldk_ecs_component_get_const(
      entity, LDK_COMPONENT_TYPE_TERRAIN_FOLLOWER);
  if (follower && isfinite(follower->height_offset))
  {
    plane_position.y -= follower->height_offset;
  }

  if (!ldk_ray_make(ray_origin, ray_direction, &ray) ||
      !ldk_raycast_plane(ray, plane_position,
          vec3_make(0.0f, 1.0f, 0.0f), &hit))
  {
    return false;
  }

  *out_direction = vec3_sub(hit.position, player_position);
  out_direction->y = 0.0f;
  if (vec3_len2(*out_direction) <= STDXM_EPS * STDXM_EPS)
  {
    return false;
  }

  *out_direction = vec3_norm(*out_direction);
  *out_position = hit.position;
  return true;
}

/* The prefab owns every visual property of the aim marker. */
static void s_aim_marker_entity_destroy(LDKEntity entity)
{
  /* ECS entity destruction is not recursive. Destroy prefab descendants
   * before their parents so stopping/restarting Play does not leave entities. */
  LDKTransform *transform = (LDKTransform *)ldk_ecs_component_get(
      entity, LDK_COMPONENT_TYPE_TRANSFORM);
  if (transform)
  {
    LDKEntity child = transform->first_child;
    while (!x_handle_is_null(child))
    {
      LDKTransform *child_transform = (LDKTransform *)ldk_ecs_component_get(
          child, LDK_COMPONENT_TYPE_TRANSFORM);
      LDKEntity next = child_transform
          ? child_transform->next_sibling : x_handle_null();
      s_aim_marker_entity_destroy(child);
      child = next;
    }
  }
  ldk_ecs_entity_destroy(entity);
}

static void s_aim_marker_destroy(TFTFPlayerCharacter *system)
{
  if (!system || !system->aim_marker_created)
  {
    return;
  }
  s_aim_marker_entity_destroy(system->aim_marker_entity);
  system->aim_marker_entity = x_handle_null();
  system->aim_marker_bound_prefab = ldk_asset_prefab_null();
  system->aim_marker_created = false;
}

/* Keep the aim prefab alive for the whole Play session. Without aiming, its
 * transform is parked below the terrain and the terrain follower is disabled.
 * Re-enabling the follower lets the normal terrain system restore its height. */
#define TFTF_AIM_MARKER_HIDDEN_Y (-1000.0f)

static void s_aim_marker_hide(LDKEntity entity)
{
  Vec3 position;

  if (ldk_ecs_component_is_enabled(
          entity, LDK_COMPONENT_TYPE_TERRAIN_FOLLOWER))
  {
    (void)ldk_ecs_component_enabled_set(
        entity, LDK_COMPONENT_TYPE_TERRAIN_FOLLOWER, false);
  }

  if (ldk_transform_get_local_position(entity, &position) &&
      position.y != TFTF_AIM_MARKER_HIDDEN_Y)
  {
    position.y = TFTF_AIM_MARKER_HIDDEN_Y;
    (void)ldk_transform_set_local_position(entity, position);
  }
}

static void s_aim_marker_update(TFTFPlayerCharacter *system,
    bool aiming, Vec3 target)
{
  LDKAssetManager *assets;
  LDKAssetPrefab prefab;
  LDKEntity marker;
  Vec3 position;
  bool follows_terrain;

  if (!system)
  {
    return;
  }

  assets = (LDKAssetManager *)ldk_module_get(LDK_MODULE_ASSET_MANAGER);
  prefab = system->aim_marker_prefab;
  if (!assets || !ldk_asset_manager_prefab_is_alive(assets, prefab))
  {
    s_aim_marker_destroy(system);
    return;
  }

  /* Rebuild only when the configured asset changes, never on aim toggles. */
  if (system->aim_marker_created &&
      (system->aim_marker_bound_prefab.h.index != prefab.h.index ||
          system->aim_marker_bound_prefab.h.version != prefab.h.version))
  {
    s_aim_marker_destroy(system);
  }

  if (!system->aim_marker_created)
  {
    LDKEntity instance = ldk_ecs_prefab_instantiate(prefab);
    if (x_handle_is_null(instance))
    {
      return;
    }
    system->aim_marker_entity = instance;
    system->aim_marker_bound_prefab = prefab;
    system->aim_marker_created = true;
  }

  marker = system->aim_marker_entity;
  if (!aiming)
  {
    s_aim_marker_hide(marker);
    return;
  }

  if (!ldk_transform_get_local_position(marker, &position))
  {
    s_aim_marker_destroy(system);
    return;
  }

  follows_terrain = ldk_ecs_component_get_const(
      marker, LDK_COMPONENT_TYPE_TERRAIN_FOLLOWER) != NULL;

  position.x = target.x;
  position.z = target.z;
  if (!follows_terrain)
  {
    /* Optional fallback for prefabs without a TerrainFollower. */
    position.y = target.y;
  }

  if (!ldk_transform_set_local_position(marker, position))
  {
    s_aim_marker_destroy(system);
    return;
  }

  if (follows_terrain && !ldk_ecs_component_is_enabled(
          marker, LDK_COMPONENT_TYPE_TERRAIN_FOLLOWER))
  {
    (void)ldk_ecs_component_enabled_set(
        marker, LDK_COMPONENT_TYPE_TERRAIN_FOLLOWER, true);
  }
}

int tftf_player_character_system_initialize(void *data)
{
  TFTFPlayerCharacter *system = (TFTFPlayerCharacter *)data;

  if (!system)
  {
    return -1;
  }

  system->aim_marker_entity = x_handle_null();
  system->aim_marker_bound_prefab = ldk_asset_prefab_null();
  system->aim_marker_created = false;
  return 0;
}

void tftf_player_character_system_terminate(void *data)
{
  TFTFPlayerCharacter *system = (TFTFPlayerCharacter *)data;

  if (!system)
  {
    return;
  }

  s_aim_marker_destroy(system);
}

static void s_player_character_update(LDKEntity entity,
    TFTFPlayerCharacterComponent *player, Vec3 input_direction, float dt)
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
  speed *= s_environment_speed_scale(entity, player);
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
  LDKMouseState mouse;
  TFTFPlayerCharacter *system = (TFTFPlayerCharacter *)data;
  Vec3 marker_target = vec3_make(0.0f, 0.0f, 0.0f);
  bool marker_target_valid = false;
  bool aiming;
  bool cut_grass;
  float orbit_input = 0.0f;
  u32 component_type = ldk_component_type(TFTFPlayerCharacterComponent);

  if (!system || !group || !isfinite(dt) || dt <= 0.0f)
  {
    return;
  }

  ldk_input_keyboard_state_get(&keyboard);
  ldk_input_mouse_state_get(&mouse);
  aiming = ldk_input_mouse_button_is_pressed(&mouse, LDK_MOUSE_BUTTON_RIGHT);
  cut_grass = ldk_input_keyboard_key_down(&keyboard, LDK_KEYCODE_SPACE);

  if (ldk_input_keyboard_key_is_pressed(&keyboard, LDK_KEYCODE_Q))
  {
    orbit_input -= 1.0f;
  }
  if (ldk_input_keyboard_key_is_pressed(&keyboard, LDK_KEYCODE_E))
  {
    orbit_input += 1.0f;
  }

  for (u32 i = 0u; i < group->count; ++i)
  {
    LDKEntity entity = group->entities[i];
    LDKEntity camera = s_player_camera_find(entity);
    TFTFPlayerCharacterComponent *player =
        (TFTFPlayerCharacterComponent *)ldk_ecs_component_get(
            entity, component_type);
    Vec3 input_direction;
    Vec3 aim_direction;
    Vec3 aim_position;
    Mat4 camera_world;
    const Mat4 *camera_world_ptr = NULL;
    float orbit_delta;

    if (!player)
    {
      continue;
    }

    orbit_delta = deg_to_rad(
        s_finite_non_negative(player->camera_orbit_speed, 120.0f)) *
        orbit_input * dt;

    if (s_player_camera_orbit(entity, camera, player, orbit_delta) &&
        ldk_camera_get_world_matrix(camera, &camera_world))
    {
      camera_world_ptr = &camera_world;
    }
    input_direction = s_player_input_direction(&keyboard, camera_world_ptr);

    /* When aiming, face the cursor. Otherwise face the actual movement. */
    bool aim_valid = aiming && s_player_cursor_aim(
        entity, camera, &mouse, &aim_direction, &aim_position);
    if (aim_valid && !marker_target_valid)
    {
      marker_target = aim_position;
      marker_target_valid = true;
    }

    s_player_character_update(entity, player, input_direction, dt);

    if (aiming)
    {
      if (aim_valid && s_player_face_direction(entity, aim_direction))
      {
        player->aim_direction = aim_direction;
      }
    }
    else if (vec3_len2(player->velocity) > STDXM_EPS * STDXM_EPS)
    {
      (void)s_player_face_direction(entity, player->velocity);
    }

    if (cut_grass)
    {
      s_player_cut_vegetation(entity, player);
    }

    /* Restore the world orbit after rotating or moving the parent entity. */
    (void)s_player_camera_orbit(entity, camera, player, 0.0f);
  }

  s_aim_marker_update(system, marker_target_valid, marker_target);
}
