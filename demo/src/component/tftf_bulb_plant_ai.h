#ifndef TFTF_BULB_PLANT_AI_H
#define TFTF_BULB_PLANT_AI_H

#include <ldk_common.h>
#include <stdx/stdx_math.h>

//@component
typedef struct TFTFBulbPlantAIComponent
{
  bool enabled;
  //@inspect min=0
  float activation_range;
  //@inspect min=0
  float cooldown;
  Vec3 burst_offset;

  //@inspect readonly runtime
  float cooldown_remaining;
} TFTFBulbPlantAIComponent;

bool tftf_bulb_plant_ai_component_register(void);

#endif // TFTF_BULB_PLANT_AI_H
