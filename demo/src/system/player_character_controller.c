#include "player_character_controller.h"

#include "../component/player_character.h"

#include <component/ldk_transform.h>
#include <generated_component_metadata.h>
#include <ldk.h>
#include <module/ldk_ecs.h>
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

static void s_player_character_update(LDKEntity entity,
    PlayerCharacterComponent *player, Vec3 input_direction, float dt)
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

  speed = isfinite(player->speed) ? float_max(player->speed, 0.0f) : 0.0f;
  acceleration = isfinite(player->acceleration)
      ? float_max(player->acceleration, 0.0f)
      : 0.0f;
  inertia = isfinite(player->inertia)
      ? float_max(player->inertia, 0.0f)
      : 0.0f;

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

void player_character_controller_system_update(
    void *data, const LDKEntityGroup *group, float dt)
{
  LDKKeyboardState keyboard;
  Vec3 input_direction;
  u32 component_type = ldk_component_type(PlayerCharacterComponent);

  (void)data;

  if (!group || !isfinite(dt) || dt <= 0.0f)
  {
    return;
  }

  ldk_input_keyboard_state_get(&keyboard);
  input_direction = s_player_input_direction(&keyboard);

  for (u32 i = 0u; i < group->count; ++i)
  {
    PlayerCharacterComponent *player =
        (PlayerCharacterComponent *)ldk_ecs_component_get(
            group->entities[i], component_type);

    if (!player)
    {
      continue;
    }

    s_player_character_update(
        group->entities[i], player, input_direction, dt);
  }
}
