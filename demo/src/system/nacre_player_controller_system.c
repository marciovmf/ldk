/**
 * @file nacre_player_controller_system.c
 * @brief Dungeon grid movement and camera behavior, without physics coupling.
 *
 * The runtime never edits the generator's map.  It owns only the player
 * prefab subtree.  The existing main camera remains scene-owned and is NOT
 * destroyed or parented to the player.  If the map is regenerated, its backing
 * cell allocation changes; on the next update the player is snapped to the
 * new entry.  The world origin is read from NacreDungeonSystem itself.
 *
 * This is a playtest controller, not yet an animation/combat system. In
 * particular, camera occlusion is deliberately NOT solved: this lets us
 * observe the actual visibility behavior around walls and passages.
 */
#include "nacre_player_controller_system.h"
#include "nacre_dungeon_navigation.h"
#include "nacre_dungeon_system.h"

#include <component/ldk_camera.h>
#include <component/ldk_transform.h>
#include <ldk.h>
#include <module/ldk_asset_manager.h>
#include <module/ldk_ecs.h>

#include <math.h>
#include <stdio.h>
#include <string.h>

#define NACRE_PLAYER_PI 3.14159265358979323846f
/* Diagonal room framing: the scene camera starts between the grid axes. */
#define NACRE_PLAYER_DIAGONAL_YAW (NACRE_PLAYER_PI * .25f)
#define NACRE_PLAYER_DEG_TO_RAD (NACRE_PLAYER_PI / 180.0f)

typedef struct NacrePlayerRuntime
{
  NacrePlayerControllerSystem *owner;
  LDKEntity player;
  LDKEntity camera;
  const uint8_t *map_cells_identity;
  Vec3 dungeon_origin;
  uint32_t current_tile;
  uint32_t target_tile;
  uint32_t last_cracked_from, last_cracked_to;
  bool last_cracked_valid;
  uint32_t last_door_from,last_door_to;
  bool last_door_valid;
  uint32_t last_gate_from,last_gate_to;
  bool last_gate_valid;
  bool entering_gate_code;
  NacreDungeonDirection gate_code[4];
  uint32_t gate_code_length;
  Vec3 start_world, target_world, visual_world;
  float step_elapsed;
  float block_cooldown;
  float camera_angle;
  float camera_target_angle;
  Vec3 camera_position;
  Vec3 camera_original_position;
  Quat camera_original_rotation;
  LDKEntity camera_original_entity;
  bool camera_original_captured;
  bool camera_initialized;
  bool player_owned;
} NacrePlayerRuntime;

static NacrePlayerRuntime nacre_player_runtime;

static void nacre_player_status(NacrePlayerControllerSystem *s, const char *message)
{
  x_smallstr_from_cstr(&s->status, message);
}

/* Destroy a prefab INCLUDING nested children. ldk_ecs_entity_destroy()
 * itself deliberately leaves surviving children detached. */
static void nacre_player_destroy_tree(LDKEntity entity)
{
  if (x_handle_is_null(entity) ||
      !ldk_ecs_component_get_const(entity, LDK_COMPONENT_TYPE_TRANSFORM))
    return;
  for (;;)
  {
    const LDKTransform *t = (const LDKTransform *)
        ldk_ecs_component_get_const(entity, LDK_COMPONENT_TYPE_TRANSFORM);
    if (!t || x_handle_is_null(t->first_child)) break;
    nacre_player_destroy_tree(t->first_child);
  }
  ldk_ecs_entity_destroy(entity);
}

static bool nacre_player_main_camera_search(LDKEntity entity, void *user)
{
  LDKEntity *found = (LDKEntity *)user;
  const LDKCamera *cam = (const LDKCamera *)
      ldk_ecs_component_get_const(entity, LDK_COMPONENT_TYPE_CAMERA);
  if (!cam || !cam->enabled || cam->role != LDK_CAMERA_ROLE_MAIN)
    return true;
  /* Writing a local position to a parented camera would not correspond to a
   * world position. Do not alter an authored camera hierarchy silently. */
  if (!x_handle_is_null(ldk_transform_get_parent(entity)))
    return true;
  *found = entity;
  return false;
}

static LDKEntity nacre_player_find_main_camera(void)
{
  LDKEntity found = x_handle_null();
  (void)ldk_ecs_entity_foreach(nacre_player_main_camera_search, &found);
  return found;
}

static Vec3 nacre_player_tile_world(const NacreDungeonMap *map,
    Vec3 origin, uint32_t tile, float y_offset)
{
  return vec3_make(origin.x + (float)(tile % map->width) + .5f -
          .5f * (float)map->width,
      origin.y + y_offset,
      origin.z + (float)(tile / map->width) + .5f -
          .5f * (float)map->height);
}

static void nacre_player_publish_cell(NacrePlayerControllerSystem *s,
    const NacreDungeonMap *map)
{
  s->grid_x = nacre_player_runtime.current_tile % map->width;
  s->grid_z = nacre_player_runtime.current_tile / map->width;
}

static bool nacre_player_ensure_entity(NacrePlayerControllerSystem *s)
{
  if (!x_handle_is_null(nacre_player_runtime.player) &&
      ldk_ecs_component_get_const(nacre_player_runtime.player,
          LDK_COMPONENT_TYPE_TRANSFORM))
    return true;

  LDKEntity entity = x_handle_null();
  LDKAssetManager *manager = (LDKAssetManager *)
      ldk_module_get(LDK_MODULE_ASSET_MANAGER);
  LDKAssetPrefab prefab = s->player_prefab;
  if (prefab.h.index == 0u && prefab.h.version == 0u)
    prefab = ldk_asset_prefab_null();
  if (manager && !x_handle_is_null(prefab.h) &&
      ldk_asset_manager_prefab_is_alive(manager, prefab))
    entity = ldk_ecs_prefab_instantiate(prefab);
  else
    entity = ldk_ecs_entity_create();

  if (x_handle_is_null(entity))
  {
    nacre_player_status(s, "Cannot create player entity");
    return false;
  }
  nacre_player_runtime.player = entity;
  nacre_player_runtime.player_owned = true;
  (void)ldk_ecs_entity_name_set(entity, "Nacre Player [runtime]");
  return true;
}

static bool nacre_player_spawn(NacrePlayerControllerSystem *s,
    const NacreDungeonMap *map, Vec3 origin)
{
  if (!nacre_player_ensure_entity(s) || map->entry >= map->width * map->height ||
      !map->cells[map->entry])
    return false;

  NacrePlayerRuntime *r = &nacre_player_runtime;
  const Vec3 world = nacre_player_tile_world(map, origin, map->entry,
      s->ground_offset);
  if (!ldk_transform_set_local_position(r->player, world))
    return false;
  r->map_cells_identity = map->cells;
  r->dungeon_origin = origin;
  r->current_tile = map->entry;
  r->target_tile = map->entry;
  r->start_world = world;
  r->target_world = world;
  r->visual_world = world;
  r->step_elapsed = 0.0f;
  r->last_cracked_valid = false;
  r->last_door_valid = false;
  r->last_gate_valid = false;
  r->entering_gate_code = false;
  r->gate_code_length = 0u;
  r->block_cooldown = 0.0f;
  r->camera_initialized = false;
  /* 45 degrees is the new default view, even for previously serialized
   * controllers whose newly introduced yaw adjustment is zero. */
  const float yaw_adjust = isfinite(s->camera_yaw_adjust_degrees)
      ? s->camera_yaw_adjust_degrees : 0.0f;
  r->camera_angle = NACRE_PLAYER_DIAGONAL_YAW +
      yaw_adjust * NACRE_PLAYER_DEG_TO_RAD;
  r->camera_target_angle = r->camera_angle;
  s->camera_quarter_turns = 0;
  s->player_spawned = true;
  s->player_moving = false;
  nacre_player_publish_cell(s, map);
  (void)nacre_dungeon_system_visible_tiles_set(map->entry, map->entry);
  nacre_player_status(s, "Ready (WASD move, Q/E rotate camera)");
  return true;
}

/* World cardinal directions. Camera quarter-turn affects the movement frame
 * only in multiples of 90 degrees; the player's coordinates never become
 * diagonal or slide along walls. */
static void nacre_player_rotate_input(
    int32_t *dx, int32_t *dz, int32_t quarter_turns)
{
  uint32_t turns = (uint32_t)quarter_turns & 3u;
  while (turns--)
  {
    const int32_t x = *dx, z = *dz;
    *dx = z;
    *dz = -x;
  }
}

static float nacre_player_approach(float value, float target, float amount)
{
  if (value < target)
    return fminf(value + amount, target);
  return fmaxf(value - amount, target);
}

static void nacre_player_update_camera(
    NacrePlayerControllerSystem *s, float dt)
{
  NacrePlayerRuntime *r = &nacre_player_runtime;
  if (!s->follow_main_camera || !s->player_spawned) return;
  if (x_handle_is_null(r->camera) ||
      !ldk_ecs_component_get_const(r->camera, LDK_COMPONENT_TYPE_CAMERA))
    r->camera = nacre_player_find_main_camera();
  if (x_handle_is_null(r->camera)) return;
  if (!r->camera_original_captured &&
      ldk_transform_get_local_position(r->camera,
          &r->camera_original_position) &&
      ldk_transform_get_local_rotation(r->camera,
          &r->camera_original_rotation))
  {
    r->camera_original_entity = r->camera;
    r->camera_original_captured = true;
  }

  const float speed = fmaxf(s->orbit_degrees_per_second, 20.0f) *
      NACRE_PLAYER_DEG_TO_RAD;
  if (!r->camera_initialized)
    r->camera_angle = r->camera_target_angle;
  else
    r->camera_angle = nacre_player_approach(r->camera_angle,
        r->camera_target_angle, speed * fmaxf(0.0f, dt));

  const float co = cosf(r->camera_angle), si = sinf(r->camera_angle);
  const Vec3 offset = s->camera_offset;
  const Vec3 target_position = vec3_make(
      r->visual_world.x + offset.x * co + offset.z * si,
      r->visual_world.y + offset.y,
      r->visual_world.z - offset.x * si + offset.z * co);
  if (!r->camera_initialized || s->follow_sharpness <= 0.0f)
    r->camera_position = target_position;
  else
  {
    const float alpha = 1.0f - expf(-s->follow_sharpness * fmaxf(0.0f, dt));
    r->camera_position = vec3_add(r->camera_position,
        vec3_mul(vec3_sub(target_position, r->camera_position), alpha));
  }
  if (ldk_transform_set_local_position(r->camera, r->camera_position))
  {
    /* Keep the camera pointed at the character, even during a single tile
     * transition. No camera is instantiated and no component is replaced. */
    (void)ldk_camera_look_at(r->camera,
        vec3_make(r->visual_world.x,
            r->visual_world.y + s->look_at_height,
            r->visual_world.z));
    r->camera_initialized = true;
  }
}

int nacre_player_controller_system_initialize(void *data)
{
  NacrePlayerControllerSystem *s = (NacrePlayerControllerSystem *)data;
  if (!s || nacre_player_runtime.owner) return -1;
  memset(&nacre_player_runtime, 0, sizeof(nacre_player_runtime));
  nacre_player_runtime.owner = s;
  nacre_player_runtime.player = x_handle_null();
  nacre_player_runtime.camera = x_handle_null();

  /* These defaults are applied ONLY to a pristine controller. Existing
   * inspector values stay intact; zero follow_sharpness remains meaningful. */
  if (s->step_duration <= 0.0f || !isfinite(s->step_duration))
  {
    s->step_duration = .22f;
    s->follow_main_camera = true;
    s->camera_offset = vec3_make(0.0f, 2.25f, 2.9f);
    s->look_at_height = .55f;
    s->follow_sharpness = 12.0f;
    s->orbit_degrees_per_second = 360.0f;
  }
  s->player_spawned = false;
  s->player_moving = false;
  s->grid_x = s->grid_z = 0u;
  s->successful_steps = s->blocked_steps = 0u;
  s->camera_quarter_turns = 0;
  nacre_player_status(s, "Waiting for generated dungeon");
  return 0;
}

void nacre_player_controller_system_update(
    void *data, const LDKEntityGroup *group, float dt)
{
  NacrePlayerControllerSystem *s = (NacrePlayerControllerSystem *)data;
  NacrePlayerRuntime *r = &nacre_player_runtime;
  (void)group;
  if (!s || r->owner != s) return;

  const NacreDungeonMap *map = nacre_dungeon_system_map_get();
  Vec3 origin;
  if (!map || !map->cells || !nacre_dungeon_system_world_origin_get(&origin))
  {
    s->player_spawned = false;
    s->player_moving = false;
    r->map_cells_identity = NULL;
    nacre_player_status(s, "Waiting for generated dungeon");
    return;
  }

  if (!s->player_spawned || r->map_cells_identity != map->cells ||
      r->dungeon_origin.x != origin.x ||
      r->dungeon_origin.y != origin.y ||
      r->dungeon_origin.z != origin.z)
  {
    if (!nacre_player_spawn(s, map, origin)) return;
  }

  if (!isfinite(dt) || dt < 0.0f) dt = 0.0f;
  if (r->block_cooldown > 0.0f)
    r->block_cooldown = fmaxf(0.0f, r->block_cooldown - dt);

  LDKKeyboardState kb;
  ldk_input_keyboard_state_get(&kb);
  /* Temporary wall-breaking input for level-design testing. Gameplay may
   * later replace this with a weapon/interaction system; the authoritative
   * action is on the dungeon system, not on the player controller. */
  if (!s->player_moving && ldk_input_keyboard_key_down(&kb, LDK_KEYCODE_F))
  {
    if (r->last_cracked_valid && nacre_dungeon_system_cracked_wall_break(
        r->last_cracked_from, r->last_cracked_to))
      nacre_player_status(s, "Cracked wall destroyed");
    else if (r->last_door_valid && nacre_dungeon_system_door_open(
        r->last_door_from, r->last_door_to))
      nacre_player_status(s, "Door opened");
    r->last_cracked_valid = false;
    r->last_door_valid = false;
  }
  if (!s->player_moving && ldk_input_keyboard_key_down(&kb, LDK_KEYCODE_T))
  {
    /* A totem is an object standing in a room, not a floor pressure plate:
     * allow interaction from its own tile or from an adjacent traversable
     * tile. Query the same navigation used by movement to prevent reading
     * through walls, closed doors or portcullises. */
    NacreDungeonTotem totem;
    bool discovered = nacre_dungeon_system_totem_discover(
        r->current_tile, &totem);
    if (!discovered)
    {
      static const int32_t offsets[4][2] = {
        {-1, 0}, {1, 0}, {0, -1}, {0, 1}
      };
      for (u32 i = 0u; i < 4u && !discovered; ++i)
      {
        u32 neighbor = 0u;
        if (nacre_dungeon_navigation_step(map, r->current_tile,
            offsets[i][0], offsets[i][1], &neighbor) ==
            NACRE_DUNGEON_MOVE_ALLOWED)
          discovered = nacre_dungeon_system_totem_discover(neighbor, &totem);
      }
    }
    if (discovered)
    {
      static const char *directions[]={"UP","DOWN","LEFT","RIGHT"};
      static const char *colors[]={"RED","GREEN","BLUE"};
      ldk_log_info("Nacre totem %u: %s - %s %s %s %s\n",totem.id,
          colors[totem.color],directions[totem.sequence[0]],
          directions[totem.sequence[1]],directions[totem.sequence[2]],
          directions[totem.sequence[3]]);
      /* Keep the password in the controller status, not only in the log,
       * so the value remains inspectable while testing the game. */
      char message[128];
      snprintf(message, sizeof(message), "Totem %s: %s %s %s %s",
          colors[totem.color], directions[totem.sequence[0]],
          directions[totem.sequence[1]], directions[totem.sequence[2]],
          directions[totem.sequence[3]]);
      nacre_player_status(s, message);
    }
    else
    {
      nacre_player_status(s, "No accessible totem nearby");
    }
  }
  if (!s->player_moving && r->last_gate_valid &&
      ldk_input_keyboard_key_down(&kb, LDK_KEYCODE_G))
  {
    r->entering_gate_code=true;
    r->gate_code_length=0u;
    nacre_player_status(s, "Gate code: 4 arrow keys");
  }
  if (r->entering_gate_code)
  {
    int32_t direction=-1;
    if(ldk_input_keyboard_key_down(&kb, LDK_KEYCODE_UP))
      direction=NACRE_DUNGEON_DIRECTION_UP;
    else if(ldk_input_keyboard_key_down(&kb, LDK_KEYCODE_DOWN))
      direction=NACRE_DUNGEON_DIRECTION_DOWN;
    else if(ldk_input_keyboard_key_down(&kb, LDK_KEYCODE_LEFT))
      direction=NACRE_DUNGEON_DIRECTION_LEFT;
    else if(ldk_input_keyboard_key_down(&kb, LDK_KEYCODE_RIGHT))
      direction=NACRE_DUNGEON_DIRECTION_RIGHT;
    if(direction>=0)
    {
      r->gate_code[r->gate_code_length++]=(NacreDungeonDirection)direction;
      if(r->gate_code_length==4u)
      {
        bool opened=nacre_dungeon_system_portcullis_open(
            r->last_gate_from,r->last_gate_to,r->gate_code);
        nacre_player_status(s,opened?"Portcullis opened":"Invalid or unknown gate code");
        r->entering_gate_code=false;
        r->last_gate_valid=false;
      }
    }
  }
  if (ldk_input_keyboard_key_down(&kb, LDK_KEYCODE_Q))
    --s->camera_quarter_turns;
  if (ldk_input_keyboard_key_down(&kb, LDK_KEYCODE_E))
    ++s->camera_quarter_turns;

  /* Preserve grid-relative movement and 90-degree Q/E turns. The additive
   * adjustment can be edited during Play without resetting the camera.
   * Starting from +45 degrees makes both room walls visible at once. */
  const float yaw_adjust = isfinite(s->camera_yaw_adjust_degrees)
      ? s->camera_yaw_adjust_degrees : 0.0f;
  r->camera_target_angle = NACRE_PLAYER_DIAGONAL_YAW +
      yaw_adjust * NACRE_PLAYER_DEG_TO_RAD +
      (float)s->camera_quarter_turns * (NACRE_PLAYER_PI * .5f);

  /* Complete exactly one checked cardinal transition. The logical position
   * remains on the source tile until the interpolation finishes. */
  if (s->player_moving)
  {
    r->step_elapsed += dt;
    const float duration = fmaxf(.03f, s->step_duration);
    float t = fminf(1.0f, r->step_elapsed / duration);
    /* Smoothstep changes only animation timing, never the checked path. */
    t = t * t * (3.0f - 2.0f * t);
    r->visual_world = vec3_add(r->start_world,
        vec3_mul(vec3_sub(r->target_world, r->start_world), t));
    if (r->step_elapsed >= duration)
    {
      r->current_tile = r->target_tile;
      r->visual_world = r->target_world;
      s->player_moving = false;
      nacre_player_publish_cell(s, map);
      (void)nacre_dungeon_system_visible_tiles_set(
          r->current_tile, r->current_tile);
      ++s->successful_steps;
      /* The v21 prototype treats stepping onto an objective as collecting
       * its key or activating its lever. No combat/UI dependency here. */
      if(nacre_dungeon_system_objective_complete(r->current_tile))
        nacre_player_status(s,"Objective completed");
      if(r->current_tile==map->exit)
        nacre_player_status(s,nacre_dungeon_system_exit_can_use(r->current_tile)?
            "Exit unlocked":"Exit requires more objectives");
    }
  }

  if (!s->player_moving && r->block_cooldown <= 0.0f)
  {
    int32_t dx = 0, dz = 0;
    /* One direction at a time (no corner-cutting or diagonal moves). */
    if (ldk_input_keyboard_key_is_pressed(&kb, LDK_KEYCODE_W)) dz = -1;
    else if (ldk_input_keyboard_key_is_pressed(&kb, LDK_KEYCODE_S)) dz = 1;
    else if (ldk_input_keyboard_key_is_pressed(&kb, LDK_KEYCODE_A)) dx = -1;
    else if (ldk_input_keyboard_key_is_pressed(&kb, LDK_KEYCODE_D)) dx = 1;

    if (dx || dz)
    {
      nacre_player_rotate_input(&dx, &dz, s->camera_quarter_turns);
      uint32_t dest = r->current_tile;
      const NacreDungeonMoveResult check = nacre_dungeon_navigation_step(
          map, r->current_tile, dx, dz, &dest);
      if (check == NACRE_DUNGEON_MOVE_ALLOWED)
      {
        r->last_cracked_valid = false;
        r->last_door_valid = false;
        r->last_gate_valid = false;
        r->entering_gate_code = false;
        /* Reveal the destination only during the doorway crossing. It stays
         * visible while the player's interpolation spans the two regions. */
        (void)nacre_dungeon_system_visible_tiles_set(r->current_tile, dest);
        r->target_tile = dest;
        r->start_world = r->visual_world;
        r->target_world = nacre_player_tile_world(map, origin, dest,
            s->ground_offset);
        r->step_elapsed = 0.0f;
        s->player_moving = true;
        (void)ldk_transform_set_local_rotation(r->player,
            quat_axis_angle(vec3_make(0.0f, 1.0f, 0.0f),
                atan2f((float)dx, (float)-dz)));
        nacre_player_status(s, "Moving");
      }
      else
      {
        r->last_cracked_valid = check == NACRE_DUNGEON_MOVE_CRACKED;
        r->last_door_valid = check == NACRE_DUNGEON_MOVE_DOOR;
        r->last_gate_valid = check == NACRE_DUNGEON_MOVE_PORTCULLIS;
        if (r->last_cracked_valid || r->last_door_valid || r->last_gate_valid)
        {
          const int32_t nx = (int32_t)(r->current_tile % map->width) + dx;
          const int32_t nz = (int32_t)(r->current_tile / map->width) + dz;
          r->last_cracked_from = r->current_tile;
          r->last_cracked_to = (uint32_t)nz * map->width + (uint32_t)nx;
          r->last_door_from = r->last_gate_from = r->last_cracked_from;
          r->last_door_to = r->last_gate_to = r->last_cracked_to;
        }
        ++s->blocked_steps;
        r->block_cooldown = .14f;
        nacre_player_status(s,
            check == NACRE_DUNGEON_MOVE_CRACKED ? "Cracked wall: F to break" :
            check == NACRE_DUNGEON_MOVE_DOOR ? "Door: press F to open" :
            check == NACRE_DUNGEON_MOVE_PORTCULLIS ? "Gate: press G, enter code" :
            check == NACRE_DUNGEON_MOVE_WALL ? "Blocked: wall" :
            "Blocked: outside walkable grid");
      }
    }
  }

  (void)ldk_transform_set_local_position(r->player, r->visual_world);
  nacre_player_update_camera(s, dt);
}

void nacre_player_controller_system_terminate(void *data)
{
  NacrePlayerControllerSystem *s = (NacrePlayerControllerSystem *)data;
  NacrePlayerRuntime *r = &nacre_player_runtime;
  if (!s || r->owner != s) return;
  if (r->camera_original_captured &&
      ldk_ecs_component_get_const(r->camera_original_entity,
          LDK_COMPONENT_TYPE_CAMERA))
  {
    (void)ldk_transform_set_local_position(r->camera_original_entity,
        r->camera_original_position);
    (void)ldk_transform_set_local_rotation(r->camera_original_entity,
        r->camera_original_rotation);
  }
  if (r->player_owned) nacre_player_destroy_tree(r->player);
  memset(r, 0, sizeof(*r));
  s->player_spawned = false;
  s->player_moving = false;
  nacre_player_status(s, "Stopped");
}
