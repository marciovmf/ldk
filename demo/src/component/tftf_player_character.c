#include "tftf_player_character.h"

#include <generated_component_metadata.h>
#include <ldk.h>
#include <module/ldk_component.h>
#include <module/ldk_ecs.h>

static TFTFPlayerCharacterComponent s_tftf_player_character_default(void)
{
  TFTFPlayerCharacterComponent player = {0};

  player.speed = 4.0f;
  player.acceleration = 18.0f;
  player.inertia = 0.15f;
  player.camera_orbit_speed = 120.0f;

  player.high_grass_height = 0.6f;
  player.high_grass_density_min = 90.0f;
  player.high_grass_speed_scale = 0.8f;
  player.grass_cut_density_scale = 0.75f;
  player.thorns_speed_scale = 0.5f;
  player.thorns_density_min = 5.0f;
  player.thorns_cut_density_scale = 0.75f;
  player.cut_distance = 1.0f;

  player.deep_water_speed_scale = 1.0f;
  player.shallow_water_speed_scale = 1.0f;
  player.shore_speed_scale = 1.0f;
  player.dry_speed_scale = 1.0f;
  player.grass_speed_scale = 1.0f;
  player.forest_speed_scale = 1.0f;
  player.mountain_speed_scale = 1.0f;

  player.velocity = vec3_make(0.0f, 0.0f, 0.0f);
  player.aim_direction = vec3_make(0.0f, 0.0f, -1.0f);
  player.camera_initial_rotation = quat_id();
  player.player_initial_rotation = quat_id();
  return player;
}

static bool s_tftf_player_character_attach(
    LDKEntityRegistry *entity_registry,
    LDKComponentRegistry *component_registry, LDKEntity entity,
    void *component, u32 component_index, const void *initial_value,
    void *user)
{
  TFTFPlayerCharacterComponent *player =
      (TFTFPlayerCharacterComponent *)component;

  (void)component_registry;
  (void)entity;
  (void)component_index;
  (void)user;

  if (!entity_registry || !player)
  {
    return false;
  }

  if (!initial_value)
  {
    *player = s_tftf_player_character_default();
  }

  /* Existing scene files will not contain the new camera setting. */
  if (player->camera_orbit_speed <= 0.0f)
  {
    player->camera_orbit_speed = 120.0f;
  }

  player->velocity = vec3_make(0.0f, 0.0f, 0.0f);
  player->aim_direction = vec3_make(0.0f, 0.0f, -1.0f);
  player->camera_yaw = 0.0f;
  player->camera_initial_position = vec3_make(0.0f, 0.0f, 0.0f);
  player->camera_initial_rotation = quat_id();
  player->player_initial_rotation = quat_id();
  player->camera_initialized = false;
  return true;
}

bool tftf_player_character_component_register(void)
{
  LDKECS *ecs = (LDKECS *)ldk_module_get(LDK_MODULE_ECS);
  LDKComponentDesc desc = {0};

  if (!ecs)
  {
    return false;
  }

  desc.name = "TFTFPlayerCharacterComponent";
  desc.type = ldk_component_type(TFTFPlayerCharacterComponent);
  desc.entry_size = sizeof(TFTFPlayerCharacterComponent);
  desc.initial_capacity = 4u;
  desc.attach = s_tftf_player_character_attach;

  if (ldk_component_is_registered(&ecs->component, desc.type))
  {
    return true;
  }

  return ldk_ecs_component_register(&desc);
}
