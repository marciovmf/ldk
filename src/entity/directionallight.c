#include "module/entity.h"
#include <ldk/entity/directionallight.h>
#include <ldk/asset/mesh.h>
#include <ldk/maths.h>
#include <ldk/os.h>
#include <memory.h>

LDKDirectionalLight* ldkDirectionalLightEntityCreate(LDKDirectionalLight* entity)
{
  entity->position = vec3Zero();
  entity->direction = vec3Zero();
  entity->colorDiffuse = vec3One();
  entity->colorSpecular = vec3One();
  return entity;
}

void ldkDirectionalLightEntityDestroy(LDKDirectionalLight* entity)
{
}

#ifdef LDK_EDITOR

void ldkDirectionalLightEntityOnEditorGetTransform (LDKEntitySelectionInfo* selection, Vec3* pos, Vec3* scale, Quat* rot)
{
  LDKDirectionalLight* o = ldkEntityLookup(LDKDirectionalLight, selection->handle);
  LDK_ASSERT(o != NULL);
  if(pos) *pos = o->position;
  if(scale) *scale = vec3One();
  if(rot) *rot = quatFromEuler(o->direction);
}

void ldkDirectionalLightEntityOnEditorSetTransform(LDKEntitySelectionInfo*selection, Vec3 pos, Vec3 _, Quat rot)
{
  LDKDirectionalLight* o = ldkEntityLookup(LDKDirectionalLight, selection->handle);
  LDK_ASSERT(o != NULL);
  o->position = pos;
  o->direction = quatToEuler(rot);
}

#endif // LDK_EDITOR
