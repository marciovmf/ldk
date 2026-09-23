#ifndef DEMO_PLAYER_CHARACTER_H
#define DEMO_PLAYER_CHARACTER_H

#include <ldk_common.h>
#include <stdx/stdx_math.h>

//@component
typedef struct PlayerCharacterComponent
{
  //@inspect min=0
  float speed;
  //@inspect min=0
  float acceleration;
  /* Seconds to coast from full speed to rest after input is released. */
  //@inspect min=0
  float inertia;

  //@inspect runtime
  Vec3 velocity;
} PlayerCharacterComponent;

bool player_character_component_register(void);

#endif // DEMO_PLAYER_CHARACTER_H
