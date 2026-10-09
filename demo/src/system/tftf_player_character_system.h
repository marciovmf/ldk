#ifndef TFTF_PLAYER_CHARACTER_SYSTEM_H
#define TFTF_PLAYER_CHARACTER_SYSTEM_H

#include <module/ldk_system.h>

//@system components=LDK_COMPONENT_TFTFPlayerCharacterComponent name=TFTFPlayerCharacter order=0
void tftf_player_character_system_update(
    void *data, const LDKEntityGroup *group, float dt);

#endif // TFTF_PLAYER_CHARACTER_SYSTEM_H
