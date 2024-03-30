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
  entity->mesh = LDK_HANDLE_INVALID;
  entity->materials = NULL;
  return entity;
}

void ldkStaticObjectEntityDestroy(LDKStaticObject* entity)
{
  if (entity->materials != NULL)
    ldkOsMemoryFree(entity->materials);

  ldkOsMemoryFree(entity);
}

void ldkStaticObjectSetMesh(LDKStaticObject* entity, LDKHandle hMesh)
{
  if (hMesh == LDK_HANDLE_INVALID)
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
