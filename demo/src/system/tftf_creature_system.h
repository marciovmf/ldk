#ifndef TFTF_CREATURE_SYSTEM_H
#define TFTF_CREATURE_SYSTEM_H

#include <module/ldk_system.h>

int tftf_creature_system_initialize(void *data);
//@system components=LDK_COMPONENT_TFTFCreatureComponent name=TFTFCreatureSystem initialize=tftf_creature_system_initialize terminate=tftf_creature_system_terminate order=30
void tftf_creature_system_update(
    void *data, const LDKEntityGroup *group, float dt);
void tftf_creature_system_terminate(void *data);
/* Editor-only rendering path; shares geometry functions, not runtime state. */
void tftf_creature_editor_preview_update(void);

#endif // TFTF_CREATURE_SYSTEM_H
