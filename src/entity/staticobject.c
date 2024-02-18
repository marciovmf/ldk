#include <ldk/entity/staticobject.h>
#include <ldk/maths.h>
#include <ldk/os.h>

LDKStaticObject* ldkStaticObjectEntityCreate(LDKStaticObject* entity)
{
  entity->position = vec3Zero();
  entity->scale = vec3One();
  entity->rotation = quatId();
  return entity;
}

void ldkStaticObjectEntityDestroy(LDKStaticObject* entity)
{
  ldkOsMemoryFree(entity);
}
