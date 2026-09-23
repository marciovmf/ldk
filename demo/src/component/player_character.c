#include "player_character.h"

#include <generated_component_metadata.h>
#include <ldk.h>
#include <module/ldk_component.h>
#include <module/ldk_ecs.h>

static PlayerCharacterComponent s_player_character_default(void)
{
  PlayerCharacterComponent player = {0};

  player.speed = 4.0f;
  player.acceleration = 18.0f;
  player.inertia = 0.15f;
  player.velocity = vec3_make(0.0f, 0.0f, 0.0f);
  return player;
}

static bool s_player_character_attach(LDKEntityRegistry *entity_registry,
    LDKComponentRegistry *component_registry, LDKEntity entity,
    void *component, u32 component_index, const void *initial_value,
    void *user)
{
  PlayerCharacterComponent *player = (PlayerCharacterComponent *)component;

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
    *player = s_player_character_default();
  }

  return true;
}

bool player_character_component_register(void)
{
  LDKECS *ecs = (LDKECS *)ldk_module_get(LDK_MODULE_ECS);
  LDKComponentDesc desc = {0};

  if (!ecs)
  {
    return false;
  }

  desc.name = "PlayerCharacterComponent";
  desc.type = ldk_component_type(PlayerCharacterComponent);
  desc.entry_size = sizeof(PlayerCharacterComponent);
  desc.initial_capacity = 4u;
  desc.attach = s_player_character_attach;
  desc.destroy = NULL;
  desc.user = NULL;

  if (ldk_component_is_registered(&ecs->component, desc.type))
  {
    return true;
  }

  return ldk_ecs_component_register(&desc);
}
