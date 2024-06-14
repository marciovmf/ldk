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
