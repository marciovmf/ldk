#include "tftf_bulb_plant_ai.h"

#include <generated_component_metadata.h>
#include <ldk.h>
#include <module/ldk_component.h>
#include <module/ldk_ecs.h>

static TFTFBulbPlantAIComponent s_tftf_bulb_plant_ai_default(void)
{
  TFTFBulbPlantAIComponent ai = {0};

  ai.enabled = true;
  ai.activation_range = 12.0f;
  ai.cooldown = 1.5f;
  ai.burst_offset = vec3_make(0.0f, 0.5f, 0.0f);
  return ai;
}

static bool s_tftf_bulb_plant_ai_attach(
    LDKEntityRegistry *entity_registry,
    LDKComponentRegistry *component_registry, LDKEntity entity,
    void *component, u32 component_index, const void *initial_value,
    void *user)
{
  TFTFBulbPlantAIComponent *ai = (TFTFBulbPlantAIComponent *)component;

  (void)component_registry;
  (void)entity;
  (void)component_index;
  (void)user;

  if (!entity_registry || !ai)
  {
    return false;
  }

  if (!initial_value)
  {
    *ai = s_tftf_bulb_plant_ai_default();
  }

  return true;
}

bool tftf_bulb_plant_ai_component_register(void)
{
  LDKECS *ecs = (LDKECS *)ldk_module_get(LDK_MODULE_ECS);
  LDKComponentDesc desc = {0};

  if (!ecs)
  {
    return false;
  }

  desc.name = "TFTFBulbPlantAIComponent";
  desc.type = ldk_component_type(TFTFBulbPlantAIComponent);
  desc.entry_size = sizeof(TFTFBulbPlantAIComponent);
  desc.initial_capacity = 16u;
  desc.attach = s_tftf_bulb_plant_ai_attach;
  desc.destroy = NULL;
  desc.user = NULL;

  if (ldk_component_is_registered(&ecs->component, desc.type))
  {
    return true;
  }

  return ldk_ecs_component_register(&desc);
}
