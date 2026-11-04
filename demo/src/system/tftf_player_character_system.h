#ifndef TFTF_PLAYER_CHARACTER_SYSTEM_H
#define TFTF_PLAYER_CHARACTER_SYSTEM_H

#include <ldk_asset.h>
#include <module/ldk_entity.h>
#include <module/ldk_system.h>

int tftf_player_character_system_initialize(void *data);
void tftf_player_character_system_update(
    void *data, const LDKEntityGroup *group, float dt);
void tftf_player_character_system_terminate(void *data);

//@system components=LDK_COMPONENT_TFTFPlayerCharacterComponent name=TFTFPlayerCharacter initialize=tftf_player_character_system_initialize update=tftf_player_character_system_update terminate=tftf_player_character_system_terminate order=0
typedef struct TFTFPlayerCharacter
{
  //@begin_group "Aim Marker"
  /* Optional world-space aim marker. Its visuals belong entirely to the prefab. */
  LDKAssetPrefab aim_marker_prefab;
  //@end_group

  //@inspect hidden runtime
  LDKEntity aim_marker_entity;
  //@inspect hidden runtime
  LDKAssetPrefab aim_marker_bound_prefab;
  //@inspect hidden runtime
  bool aim_marker_created;
} TFTFPlayerCharacter;

#endif // TFTF_PLAYER_CHARACTER_SYSTEM_H
