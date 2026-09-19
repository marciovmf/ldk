#ifndef DEMO_PLAYER_CHARACTER_CONTROLLER_H
#define DEMO_PLAYER_CHARACTER_CONTROLLER_H

#include <module/ldk_system.h>

//@system name=PlayerCharacterController
void player_character_controller_system_update(
    void *data, const LDKEntityGroup *group, float dt);

#endif // DEMO_PLAYER_CHARACTER_CONTROLLER_H
