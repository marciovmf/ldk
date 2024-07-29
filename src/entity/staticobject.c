#include "common.h"
#include "module/entity.h"
#include <ldk/entity/staticobject.h>
#include <ldk/asset/mesh.h>
#include <ldk/maths.h>
#include <ldk/os.h>
#include <memory.h>

LDKStaticObject* ldkStaticObjectEntityCreate(LDKStaticObject* entity)
{
  entity->position = vec3Zero();
  entity->scale = vec3One();
  entity->rotation = quatId();
  entity->mesh = LDK_HASSET_INVALID;
  entity->materials = NULL;
  return entity;
}

void ldkStaticObjectEntityDestroy(LDKStaticObject* entity)
{
  if (entity->materials != NULL)
    ldkOsMemoryFree(entity->materials);

  ldkOsMemoryFree(entity);
}

void ldkStaticObjectSetMesh(LDKStaticObject* entity, LDKHAsset hMesh)
{
  if (!ldkHandleIsValid(hMesh))
    return;

  LDKMesh* mesh = ldkAssetLookup(LDKMesh, hMesh);
  if (mesh == NULL)
  {
    ldkLogWarning("Failed to lookup mesh %zu", hMesh);
    return;
  }

  if (entity->materials != NULL)
  {
    ldkOsMemoryFree(entity->materials);
    entity->materials = NULL;
  }

  if (entity->materials == NULL)
  {
    size_t size = mesh->numMaterials * sizeof(LDKMaterial);
    entity->materials = ldkOsMemoryAlloc(size);
    memcpy(entity->materials, mesh->materials, size);
  }

  entity->mesh = hMesh;
}

#ifdef LDK_EDITOR

void ldkStaticObjectEntityOnEditorGetTransform(LDKEntitySelectionInfo* selection, Vec3* pos, Vec3* scale, Quat* rot)
{
  LDKStaticObject* o = ldkEntityLookup(LDKStaticObject, selection->handle);
  LDK_ASSERT(o != NULL);
  if (pos)    *pos = o->position;
  if (scale)  *scale = o->scale;
  if (rot)    *rot = o->rotation;
}

void ldkStaticObjectEntityOnEditorSetTransform(LDKEntitySelectionInfo*selection, Vec3 pos, Vec3 scale, Quat rot)
{
  LDKStaticObject* o = ldkEntityLookup(LDKStaticObject, selection->handle);
  LDK_ASSERT(o != NULL);
  o->position = pos;
  o->scale = scale;
  o->rotation = rot;
}

#endif // LDK_EDITOR
