#include "ldk/entity/instancedobject.h"
#include "ldk/module/renderer.h"
#include "ldk/maths.h"
#include "ldk/os.h"
#include "ldk/array.h"
#include <string.h>

LDKInstancedObject* ldkInstancedObjectEntityCreate(LDKInstancedObject* entity)
{
  entity->mesh = LDK_HANDLE_INVALID;
  entity->instanceBuffer = ldkInstanceBufferCreate();
  entity->instanceList = ldkArrayCreate(sizeof(LDKObjectInstance), 16);
  entity->instanceWorldMatrixList = ldkArrayCreate(sizeof(Mat4), 16);
  return entity;
}

void ldkInstancedObjectEntityDestroy(LDKInstancedObject* entity)
{
  ldkArrayDestroy(entity->instanceList);
  ldkArrayDestroy(entity->instanceWorldMatrixList);
  entity->instanceList = NULL;
  entity->instanceWorldMatrixList = NULL;
  entity->mesh = LDK_HANDLE_INVALID;
  ldkInstanceBufferDestroy(entity->instanceBuffer);
}

void ldkInstancedObjectAddInstance(LDKInstancedObject* entity, Vec3 position, Vec3 scale, Quat rotation)
{
  LDKObjectInstance instance;
  instance.position = position;
  instance.scale    = scale;
  instance.rotation = rotation;

  ldkArrayAdd(entity->instanceWorldMatrixList, NULL); // just reserve space
  ldkArrayAdd(entity->instanceList, &instance);
}

void ldkInstancedObjectUpdate(LDKInstancedObject* io)
{
  uint32 count = ldkInstancedObjectCount(io);
  LDKObjectInstance* allInstances = ldkArrayGetData(io->instanceList);
  Mat4* allWorldMatrices = ldkArrayGetData(io->instanceWorldMatrixList);

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
  size_t count = ldkArrayCount(io->instanceList);
  return (uint32) count;
}

LDKObjectInstance* ldkInstancedObjectGetAll(const LDKInstancedObject* io, uint32* outCount)
{
  if (outCount)
    *outCount = ldkInstancedObjectCount(io);
  return ldkArrayGetData(io->instanceList);
}

LDKObjectInstance* ldkInstancedObjectGet(const LDKInstancedObject* io, uint32 index)
{
  const uint32 count = ldkInstancedObjectCount(io);
  if (index >= count)
    return NULL;

  return ldkArrayGet(io->instanceList, index);
}

void ldkInstancedObjectDeleteInterval(LDKInstancedObject* io, uint32 index, uint32 count)
{
  ldkArrayDeleteRange(io->instanceList, index, index + count);
}

void ldkInstancedObjectDelete(LDKInstancedObject* io, uint32 index)
{
  ldkArrayDeleteAt(io->instanceList, index);
}

void ldkInstancedObjectDeleteAll(LDKInstancedObject* io)
{
  ldkArrayClear(io->instanceList);
  ldkArrayClear(io->instanceWorldMatrixList);
}
