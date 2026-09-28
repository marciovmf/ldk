#include "tftf_projectile.h"

#include <generated_component_metadata.h>
#include <ldk.h>
#include <module/ldk_component.h>
#include <module/ldk_ecs.h>

#include <string.h>

static bool s_tftf_projectile_attach(LDKEntityRegistry *entity_registry,
    LDKComponentRegistry *component_registry, LDKEntity entity,
    void *component, u32 component_index, const void *initial_value,
    void *user)
{
  TFTFProjectileComponent *projectile =
      (TFTFProjectileComponent *)component;

  (void)component_registry;
  (void)entity;
  (void)component_index;
  (void)user;

  if (!entity_registry || !projectile)
  {
    return false;
  }

  if (!initial_value)
  {
    memset(projectile, 0, sizeof(*projectile));
    projectile->movement = TFTF_PROJECTILE_MOVEMENT_LINEAR;
    projectile->color = 0xffffffffu;
  }

  return true;
}

bool tftf_projectile_component_register(void)
{
  LDKECS *ecs = (LDKECS *)ldk_module_get(LDK_MODULE_ECS);
  LDKComponentDesc desc = {0};

  if (!ecs)
  {
    return false;
  }

  desc.name = "TFTFProjectileComponent";
  desc.type = ldk_component_type(TFTFProjectileComponent);
  desc.entry_size = sizeof(TFTFProjectileComponent);
  desc.initial_capacity = 1024u;
  desc.attach = s_tftf_projectile_attach;
  desc.destroy = NULL;
  desc.user = NULL;

  if (ldk_component_is_registered(&ecs->component, desc.type))
  {
    return true;
  }

  return ldk_ecs_component_register(&desc);
}
