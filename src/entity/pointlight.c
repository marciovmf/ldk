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
