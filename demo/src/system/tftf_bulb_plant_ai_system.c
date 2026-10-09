#include "tftf_bulb_plant_ai_system.h"

#include "../component/tftf_player_character.h"
#include "../component/tftf_bulb_plant_ai.h"
#include "tftf_bullet_system.h"

#include <component/ldk_transform.h>
#include <generated_component_metadata.h>
#include <module/ldk_ecs.h>
#include <module/ldk_scenegraph.h>
#include <stdx/stdx_math.h>

#include <math.h>

typedef struct TFTFPlayerSearch
{
  LDKEntity entity;
  bool found;
} TFTFPlayerSearch;

static bool s_tftf_find_player(LDKEntity entity, void *user)
{
  TFTFPlayerSearch *search = (TFTFPlayerSearch *)user;

  if (!search)
  {
    return false;
  }

  if (ldk_ecs_component_is_enabled(
          entity, ldk_component_type(TFTFPlayerCharacterComponent)))
  {
    search->entity = entity;
    search->found = true;
    return false;
  }

  return true;
}

static bool s_tftf_entity_world_position(
    LDKEntity entity, Vec3 *out_position)
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

void tftf_bulb_plant_ai_system_update(
    void *data, const LDKEntityGroup *group, float dt)
{
  TFTFPlayerSearch player = {0};
  Vec3 player_position;
  u32 ai_type = ldk_component_type(TFTFBulbPlantAIComponent);

  (void)data;

  if (!group || !isfinite(dt) || dt <= 0.0f || group->count == 0u ||
      !ldk_ecs_entity_foreach(s_tftf_find_player, &player) || !player.found ||
      !s_tftf_entity_world_position(player.entity, &player_position))
  {
    return;
  }

  for (u32 i = 0u; i < group->count; ++i)
  {
    LDKEntity entity = group->entities[i];
    TFTFBulbPlantAIComponent *ai =
        (TFTFBulbPlantAIComponent *)ldk_ecs_component_get(entity, ai_type);
    Vec3 enemy_position;
    Vec3 to_player;
    float range;

    if (!ai || !ai->enabled)
    {
      continue;
    }

    ai->cooldown_remaining = float_max(ai->cooldown_remaining - dt, 0.0f);
    if (ai->cooldown_remaining > 0.0f ||
        !s_tftf_entity_world_position(entity, &enemy_position))
    {
      continue;
    }

    range = isfinite(ai->activation_range)
        ? float_max(ai->activation_range, 0.0f)
        : 0.0f;
    to_player = vec3_sub(player_position, enemy_position);
    to_player.y = 0.0f;
    if (vec3_len2(to_player) > range * range)
    {
      continue;
    }

    enemy_position = vec3_add(enemy_position, ai->burst_offset);
    if (tftf_burst(
            TFTF_BULLET_PATTERN_NAME_RING, enemy_position, to_player))
    {
      ai->cooldown_remaining = isfinite(ai->cooldown)
          ? float_max(ai->cooldown, 0.0f)
          : 0.0f;
    }
  }
}
