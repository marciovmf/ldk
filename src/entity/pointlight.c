#include "common.h"
#include <ldk/entity/pointlight.h>
#include <ldk/maths.h>
#include <ldk/os.h>
#include <memory.h>

LDKPointLight* ldkPointLightEntityCreate(LDKPointLight* entity)
{
  entity->position = vec3Zero();
  entity->colorDiffuse = vec3One();
  entity->colorSpecular = vec3One();
  ldkLightAttenuationForDistance(&entity->attenuation, 15.0f);
  return entity;
}

void ldkPointLightEntityDestroy(LDKPointLight* entity)
{
}

void ldkPointLightEntityGetTransform(LDKHEntity handle, uint32 instanceId, Vec3* pos, Vec3* scale, Quat* rot)
{
  LDKPointLight* o = ldkEntityLookup(LDKPointLight, handle);
  LDK_ASSERT(o != NULL);
  if(pos) *pos = o->position;
}

void ldkPointLightEntitySetTransform(LDKHEntity handle, uint32 instanceId, Vec3 pos, Vec3 scale, Quat rot)
{
  LDKPointLight* o = ldkEntityLookup(LDKPointLight, handle);
  LDK_ASSERT(o != NULL);
  o->position = pos;
}

