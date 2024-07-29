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

#ifdef LDK_EDITOR

void ldkPointLightEntityOnEditorGetTransform (LDKEntitySelectionInfo* selection, Vec3* pos, Vec3* scale, Quat* rot)
{
  LDKPointLight* o = ldkEntityLookup(LDKPointLight, selection->handle);
  LDK_ASSERT(o != NULL);
  if(pos) *pos = o->position;
}

void ldkPointLightEntityOnEditorSetTransform(LDKEntitySelectionInfo*selection, Vec3 pos, Vec3 scale, Quat rot)
{
  LDKPointLight* o = ldkEntityLookup(LDKPointLight, selection->handle);
  LDK_ASSERT(o != NULL);
  o->position = pos;
}

#endif // LDK_EDITOR
