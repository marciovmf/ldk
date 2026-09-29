#ifndef TFTF_BULLET_SYSTEM_H
#define TFTF_BULLET_SYSTEM_H

#include "../component/tftf_projectile.h"

#include <ldk_asset.h>
#include <ldk_common.h>
#include <module/ldk_entity.h>
#include <module/ldk_system.h>
#include <stdx/stdx_math.h>

#define TFTF_BULLET_PATTERN_SLOT_COUNT 4u

//@enum
typedef enum TFTFBulletPatternName
{
  TFTF_BULLET_PATTERN_NAME_NONE = 0,
  TFTF_BULLET_PATTERN_NAME_RING,
  TFTF_BULLET_PATTERN_NAME_RING_FAST,
  TFTF_BULLET_PATTERN_NAME_RING_DENSE,
  TFTF_BULLET_PATTERN_NAME_RING_BIG,
  TFTF_BULLET_PATTERN_NAME_FAN
} TFTFBulletPatternName;

//@enum
typedef enum TFTFBulletPatternType
{
  TFTF_BULLET_PATTERN_TYPE_NONE = 0,
  TFTF_BULLET_PATTERN_TYPE_RING,
  TFTF_BULLET_PATTERN_TYPE_FAN
} TFTFBulletPatternType;

int tftf_bullet_system_initialize(void *data);
void tftf_bullet_system_update(
    void *data, const LDKEntityGroup *group, float dt);
void tftf_bullet_system_terminate(void *data);

bool tftf_burst(
    TFTFBulletPatternName name, Vec3 origin, Vec3 direction);

//@system initialize=tftf_bullet_system_initialize update=tftf_bullet_system_update terminate=tftf_bullet_system_terminate order=20
typedef struct TFTFBulletSystem
{
  //@begin_group "Visual"
  LDKAssetMesh projectile_mesh;
  LDKAssetMaterial projectile_material;
  //@end_group

  //@begin_group "Pattern 0"
  TFTFBulletPatternName pattern_0_name;
  TFTFBulletPatternType pattern_0_type;
  TFTFProjectileMovement pattern_0_movement;
  //@inspect min=1
  u32 pattern_0_count;
  //@inspect min=0
  float pattern_0_speed;
  float pattern_0_acceleration;
  //@inspect min=0
  float pattern_0_lifetime;
  //@inspect min=0
  float pattern_0_radius;
  //@inspect min=0
  float pattern_0_damage;
  //@inspect min=0
  float pattern_0_scale;
  float pattern_0_height_offset;
  float pattern_0_angle_offset_degrees;
  //@inspect min=0 max=360
  float pattern_0_spread_degrees;
  //@inspect min=0
  float pattern_0_sine_amplitude;
  //@inspect min=0
  float pattern_0_sine_frequency;
  float pattern_0_sine_phase_degrees;
  //@inspect widget=COLOR
  u32 pattern_0_color;
  //@end_group

  //@begin_group "Pattern 1"
  TFTFBulletPatternName pattern_1_name;
  TFTFBulletPatternType pattern_1_type;
  TFTFProjectileMovement pattern_1_movement;
  //@inspect min=1
  u32 pattern_1_count;
  //@inspect min=0
  float pattern_1_speed;
  float pattern_1_acceleration;
  //@inspect min=0
  float pattern_1_lifetime;
  //@inspect min=0
  float pattern_1_radius;
  //@inspect min=0
  float pattern_1_damage;
  //@inspect min=0
  float pattern_1_scale;
  float pattern_1_height_offset;
  float pattern_1_angle_offset_degrees;
  //@inspect min=0 max=360
  float pattern_1_spread_degrees;
  //@inspect min=0
  float pattern_1_sine_amplitude;
  //@inspect min=0
  float pattern_1_sine_frequency;
  float pattern_1_sine_phase_degrees;
  //@inspect widget=COLOR
  u32 pattern_1_color;
  //@end_group

  //@begin_group "Pattern 2"
  TFTFBulletPatternName pattern_2_name;
  TFTFBulletPatternType pattern_2_type;
  TFTFProjectileMovement pattern_2_movement;
  //@inspect min=1
  u32 pattern_2_count;
  //@inspect min=0
  float pattern_2_speed;
  float pattern_2_acceleration;
  //@inspect min=0
  float pattern_2_lifetime;
  //@inspect min=0
  float pattern_2_radius;
  //@inspect min=0
  float pattern_2_damage;
  //@inspect min=0
  float pattern_2_scale;
  float pattern_2_height_offset;
  float pattern_2_angle_offset_degrees;
  //@inspect min=0 max=360
  float pattern_2_spread_degrees;
  //@inspect min=0
  float pattern_2_sine_amplitude;
  //@inspect min=0
  float pattern_2_sine_frequency;
  float pattern_2_sine_phase_degrees;
  //@inspect widget=COLOR
  u32 pattern_2_color;
  //@end_group

  //@begin_group "Pattern 3"
  TFTFBulletPatternName pattern_3_name;
  TFTFBulletPatternType pattern_3_type;
  TFTFProjectileMovement pattern_3_movement;
  //@inspect min=1
  u32 pattern_3_count;
  //@inspect min=0
  float pattern_3_speed;
  float pattern_3_acceleration;
  //@inspect min=0
  float pattern_3_lifetime;
  //@inspect min=0
  float pattern_3_radius;
  //@inspect min=0
  float pattern_3_damage;
  //@inspect min=0
  float pattern_3_scale;
  float pattern_3_height_offset;
  float pattern_3_angle_offset_degrees;
  //@inspect min=0 max=360
  float pattern_3_spread_degrees;
  //@inspect min=0
  float pattern_3_sine_amplitude;
  //@inspect min=0
  float pattern_3_sine_frequency;
  float pattern_3_sine_phase_degrees;
  //@inspect widget=COLOR
  u32 pattern_3_color;
  //@end_group

  //@inspect hidden runtime
  LDKEntity render_proxy;
} TFTFBulletSystem;

#endif // TFTF_BULLET_SYSTEM_H
