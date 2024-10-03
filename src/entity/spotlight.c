#include "common.h"
#include <ldk/entity/pointlight.h>
#include <ldk/entity/spotlight.h>
#include <ldk/maths.h>
#include <ldk/os.h>
#include <memory.h>
#include <math.h>

LDKSpotLight* ldkSpotLightEntityCreate(LDKSpotLight* entity)
{
  entity->position = vec3Zero();
  entity->direction = vec3Neg(vec3Up());
  entity->colorDiffuse = vec3One();
  entity->colorSpecular = vec3One();
  entity->cutOffInner = (float) cos(degToRadian(20.00));
  entity->cutOffOuter = (float) cos(degToRadian(25.00));
  ldkLightAttenuationForDistance(&entity->attenuation, 1000.0f);
  return entity;
}

void ldkSpotLightEntityDestroy(LDKSpotLight* entity)
{
}

void ldkSpotLightEntityGetTransform(LDKHEntity handle, uint32 instanceId, Vec3* pos, Vec3* scale, Quat* rot)
{
  LDKSpotLight* o = ldkEntityLookup(LDKSpotLight, handle);
  LDK_ASSERT(o != NULL);
  if(pos) *pos = o->position;
  if(scale) *scale = vec3One();
  if(rot) *rot = quatFromDirection(o->direction);
}

void ldkSpotLightEntitySetTransform(LDKHEntity handle, uint32 instanceId, Vec3 pos, Vec3 scale, Quat rot)
{
  LDKSpotLight* o = ldkEntityLookup(LDKSpotLight, handle);
  LDK_ASSERT(o != NULL);
  o->position = pos;
  o->direction = quatToDirection(rot);
}
