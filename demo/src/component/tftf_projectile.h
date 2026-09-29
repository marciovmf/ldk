#ifndef TFTF_PROJECTILE_H
#define TFTF_PROJECTILE_H

#include <ldk_common.h>
#include <stdx/stdx_math.h>

//@enum
typedef enum TFTFProjectileMovement
{
  TFTF_PROJECTILE_MOVEMENT_LINEAR = 0,
  TFTF_PROJECTILE_MOVEMENT_SINE
} TFTFProjectileMovement;

typedef enum TFTFProjectileFlags
{
  TFTF_PROJECTILE_FLAG_NONE = 0
} TFTFProjectileFlags;

//@component
typedef struct TFTFProjectileComponent
{
  //@inspect readonly runtime
  u32 flags;
  //@inspect readonly runtime
  TFTFProjectileMovement movement;

  //@inspect readonly runtime
  Vec3 previous_position;
  //@inspect readonly runtime
  Vec3 position;
  //@inspect readonly runtime
  Vec3 velocity;
  //@inspect readonly runtime
  Vec3 spawn_position;
  //@inspect readonly runtime
  Vec3 forward;
  //@inspect readonly runtime
  Vec3 side;

  //@inspect readonly runtime
  float initial_speed;
  //@inspect readonly runtime
  float acceleration;
  //@inspect readonly runtime
  float sine_amplitude;
  //@inspect readonly runtime
  float sine_frequency;
  //@inspect readonly runtime
  float sine_phase_degrees;

  //@inspect readonly runtime
  float age;
  //@inspect readonly runtime
  float lifetime;
  //@inspect readonly runtime
  float radius;
  //@inspect readonly runtime
  float damage;
  //@inspect readonly runtime
  float scale;
  //@inspect readonly runtime widget=COLOR
  u32 color;
} TFTFProjectileComponent;

bool tftf_projectile_component_register(void);

#endif // TFTF_PROJECTILE_H
