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
  ldkInstanceBufferDestroy(entity->instanceBuffer);
  entity->mesh = LDK_HANDLE_INVALID;
}

void ldkInstancedObjectAddInstance(LDKInstancedObject* entity, Vec3 position, Vec3 scale, Quat rotation)
{
  ldkArenaAllocate(&entity->instanceWorldMatrixList, LDKObjectInstance); // We just reserve space for now.
  LDKObjectInstance* instance = (LDKObjectInstance*) ldkArenaAllocate(&entity->instanceList, LDKObjectInstance);
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
    size_t bufferSize = count * sizeof(Mat4);
    ldkInstanceBufferSetData(io->instanceBuffer, bufferSize, allWorldMatrices, false); 
  }
}

uint32 ldkInstancedObjectCount(const LDKInstancedObject* io)
{
  size_t count = ldkArenaUsedGet(&io->instanceList) / sizeof(LDKObjectInstance);
  return (uint32) count;
}

LDKObjectInstance* ldkInstancedObjectGetAll(const LDKInstancedObject* io, uint32* outCount)
{
  if (outCount)
    *outCount = ldkInstancedObjectCount(io);
  return (LDKObjectInstance*) io->instanceList.data;
}

LDKObjectInstance* ldkInstancedObjectGet(const LDKInstancedObject* io, uint32 index)
{
  const uint32 count = ldkInstancedObjectCount(io);
  if (index >= count)
    return NULL;

  return ((LDKObjectInstance*) io->instanceList.data) + index;
}

void ldkInstancedObjectDeleteInterval(LDKInstancedObject* io, uint32 index, uint32 count)
{
  if (count == 0)
    return;

  const uint32 instanceCount = ldkInstancedObjectCount(io);
  if (index >= instanceCount)
    return;

  if ((index + count) < instanceCount - 1)
  {
    const LDKObjectInstance* src = ((LDKObjectInstance*) io->instanceList.data) + index + count;
    LDKObjectInstance* dst = ((LDKObjectInstance*) io->instanceList.data) + index;
    size_t size = (instanceCount - index - count) * sizeof(LDKObjectInstance);
    memmove(dst, src, size);
  }

  ldkArenaFreeSize(&io->instanceList, count * sizeof(LDKObjectInstance));
}

void ldkInstancedObjectDelete(LDKInstancedObject* io, uint32 index)
{
  ldkInstancedObjectDeleteInterval(io, index, 1);
}

void ldkInstancedObjectDeleteAll(LDKInstancedObject* io)
{
  ldkArenaReset(&io->instanceList);
  ldkArenaReset(&io->instanceWorldMatrixList);
}
