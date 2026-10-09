#ifndef TFTF_BULB_PLANT_AI_SYSTEM_H
#define TFTF_BULB_PLANT_AI_SYSTEM_H

#include <module/ldk_system.h>

//@system components=LDK_COMPONENT_TFTFBulbPlantAIComponent name=TFTFBulbPlantAISystem order=10
void tftf_bulb_plant_ai_system_update(
    void *data, const LDKEntityGroup *group, float dt);

#endif // TFTF_BULB_PLANT_AI_SYSTEM_H
