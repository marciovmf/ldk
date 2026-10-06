#ifndef TFTF_PLAYER_CHARACTER_H
#define TFTF_PLAYER_CHARACTER_H

#include <ldk_common.h>
#include <stdx/stdx_math.h>

//@component
typedef struct TFTFPlayerCharacterComponent
{
  //@begin_group "Movement"
  //@inspect min=0
  float speed;
  //@inspect min=0
  float acceleration;
  /* Seconds to coast from full speed to rest after input is released. */
  //@inspect min=0
  float inertia;
  //@end_group

  //@begin_group "High Grass"
  //@inspect min=0
  float high_grass_height;
  //@inspect min=0
  float high_grass_density_min;
  //@inspect slider min=0 max=1
  float high_grass_speed_scale;
  //@inspect slider min=0 max=1
  float grass_cut_density_scale;
  //@end_group

  //@begin_group "Tile Movement"
  //@inspect slider min=0 max=1
  float deep_water_speed_scale;
  //@inspect slider min=0 max=1
  float shallow_water_speed_scale;
  //@inspect slider min=0 max=1
  float shore_speed_scale;
  //@inspect slider min=0 max=1
  float dry_speed_scale;
  //@inspect slider min=0 max=1
  float grass_speed_scale;
  //@inspect slider min=0 max=1
  float forest_speed_scale;
  //@inspect slider min=0 max=1
  float mountain_speed_scale;
  //@end_group

  //@begin_group "Runtime"
  //@inspect runtime
  Vec3 velocity;
  //@end_group
} TFTFPlayerCharacterComponent;

bool tftf_player_character_component_register(void);

#endif // TFTF_PLAYER_CHARACTER_H
