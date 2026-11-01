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

  //@begin_group "Camera"
  /* Camera orbit angular speed in degrees per second (Q/E). */
  //@inspect min=0
  float camera_orbit_speed;
  //@end_group

  //@begin_group "High Grass"
  //@inspect min=0
  float high_grass_height;
  //@inspect min=0
  float high_grass_density_min;
  //@inspect slider min=0 max=1
  float high_grass_speed_scale;
  /* Fraction of tall grass removed per cut: 0 = none, 1 = all. */
  //@inspect slider min=0 max=1
  float grass_cut_density_scale;
  //@end_group

  //@begin_group "Thorns"
  /* Movement multiplier while standing in uncleared thorns. */
  //@inspect slider min=0 max=1
  float thorns_speed_scale;
  /* Thorn density below this value no longer slows movement. */
  //@inspect min=0
  float thorns_density_min;
  /* Fraction of thorns removed per cut: 0 = none, 1 = all. */
  //@inspect slider min=0 max=1
  float thorns_cut_density_scale;
  //@end_group

  //@begin_group "Cutting"
  /* Distance in front of the character sampled when cutting (metres). */
  //@inspect min=0
  float cut_distance;
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
  //@inspect runtime
  Vec3 aim_direction;
  //@inspect runtime
  float camera_yaw;
  //@inspect hidden runtime
  Vec3 camera_initial_position;
  //@inspect hidden runtime
  Quat camera_initial_rotation;
  //@inspect hidden runtime
  Quat player_initial_rotation;
  //@inspect hidden runtime
  bool camera_initialized;
  //@end_group
} TFTFPlayerCharacterComponent;

bool tftf_player_character_component_register(void);

#endif // TFTF_PLAYER_CHARACTER_H
