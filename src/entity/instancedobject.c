#include "ldk/entity/instancedobject.h"
#include "ldk/module/renderer.h"
#include "ldk/maths.h"
#include "ldk/os.h"
#include "ldk/arena.h"
#include <string.h>

LDKInstancedObject* ldkInstancedObjectEntityCreate(LDKInstancedObject* entity)
{
  const uint32 initialInstancePool = 64;
  entity->mesh = LDK_HANDLE_INVALID;
  entity->instanceBuffer = ldkInstanceBufferCreate();
  memset(&entity->instanceList, 0, sizeof(LDKArena));
  memset(&entity->instanceWorldMatrixList, 0, sizeof(LDKArena));
  ldkArenaCreate(&entity->instanceList, sizeof(LDKObjectInstance) * initialInstancePool);
  ldkArenaCreate(&entity->instanceWorldMatrixList, sizeof(Mat4) * initialInstancePool);
  return entity;
}

void ldkInstancedObjectEntityDestroy(LDKInstancedObject* entity)
{
  ldkArenaDestroy(&entity->instanceList);
  ldkArenaDestroy(&entity->instanceWorldMatrixList);
}

void ldkInstancedObjectAddInstance(LDKInstancedObject* entity, Vec3 position, Vec3 scale, Quat rotation)
{
  ldkArenaAllocate(&entity->instanceWorldMatrixList, sizeof(LDKObjectInstance)); // We just reserve space for now.
  LDKObjectInstance* instance = (LDKObjectInstance*) ldkArenaAllocate(&entity->instanceList, sizeof(LDKObjectInstance));
  instance->position = position;
  instance->scale = scale;
  instance->rotation = rotation;
}

void ldkInstancedObjectUpdate(LDKInstancedObject* io)
{
  uint32 count = ldkInstancedObjectCount(io);
  LDKObjectInstance* allInstances = (LDKObjectInstance*) ldkArenaDataGet(&io->instanceList);
  Mat4* allWorldMatrices = (Mat4*) ldkArenaDataGet(&io->instanceWorldMatrixList);

  for (uint32 i = 0; i < count; i++)
  {
    LDKObjectInstance* instance = allInstances++;
    Mat4 world = mat4World(instance->position, instance->scale, instance->rotation);
    allWorldMatrices[i] = mat4Transpose(world);
  }

  if (io->mesh != LDK_HANDLE_INVALID)
  {
    LDKMesh* mesh = ldkAssetLookup(LDKMesh, io->mesh);
    size_t bufferSize = count * sizeof(Mat4);
    ldkInstanceBufferSetData(io->instanceBuffer, bufferSize, allWorldMatrices, false); 
  }
}

uint32 ldkInstancedObjectCount(LDKInstancedObject* io)
{
  size_t count = ldkArenaUsedGet(&io->instanceList) / sizeof(LDKObjectInstance);
  return (uint32) count;
}
