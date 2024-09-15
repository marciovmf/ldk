#include "ldk/entity/instancedobject.h"
#include "common.h"
#include "ldk/maths.h"
#include "ldk/array.h"
#include <string.h>


LDKInstancedObject* ldkInstancedObjectEntityCreate(LDKInstancedObject* entity)
{
  entity->mesh = LDK_HASSET_INVALID;
  entity->instanceBuffer = ldkInstanceBufferCreate();
  LDK_ASSERT(entity->instanceBuffer != NULL);
  entity->instanceArray = ldkArrayCreate(sizeof(LDKObjectInstance), 16);
  LDK_ASSERT(entity->instanceArray != NULL);
  entity->instanceWorldMatrixArray = ldkArrayCreate(sizeof(Mat4), 16);
  LDK_ASSERT(entity->instanceWorldMatrixArray != NULL);
  return entity;
}

void ldkInstancedObjectEntityDestroy(LDKInstancedObject* entity)
{
  ldkArrayDestroy(entity->instanceArray);
  ldkArrayDestroy(entity->instanceWorldMatrixArray);
  entity->instanceArray = NULL;
  entity->instanceWorldMatrixArray = NULL;
  entity->mesh = LDK_HASSET_INVALID;
  ldkInstanceBufferDestroy(entity->instanceBuffer);
}

void ldkInstancedObjectAddInstance(LDKInstancedObject* entity, Vec3 position, Vec3 scale, Quat rotation)
{
  LDK_ASSERT(entity != NULL);
  LDK_ASSERT(entity->instanceWorldMatrixArray != NULL);
  LDK_ASSERT(entity->instanceBuffer != NULL);
  LDK_ASSERT(entity->instanceArray != NULL);

  LDKObjectInstance instance;
  instance.position = position;
  instance.scale    = scale;
  instance.rotation = rotation;

  ldkArrayAdd(entity->instanceWorldMatrixArray, NULL); // just reserve space
  ldkArrayAdd(entity->instanceArray, &instance);
}

void ldkInstancedObjectUpdate(LDKInstancedObject* io)
{
  LDK_ASSERT(io != NULL);
  LDK_ASSERT(io->instanceWorldMatrixArray != NULL);
  LDK_ASSERT(io->instanceBuffer != NULL);
  LDK_ASSERT(io->instanceArray != NULL);

  uint32 count = ldkInstancedObjectCount(io);
  LDKObjectInstance* allInstances = ldkArrayGetData(io->instanceArray);
  Mat4* allWorldMatrices = ldkArrayGetData(io->instanceWorldMatrixArray);

  for (uint32 i = 0; i < count; i++)
  {
    LDKObjectInstance* instance = allInstances++;
    Mat4 world = mat4World(instance->position, instance->scale, instance->rotation);
    allWorldMatrices[i] = mat4Transpose(world);
  }

  if (io->mesh.value != LDK_HANDLE_INVALID)
  {
    size_t bufferSize = count * sizeof(Mat4);
    ldkInstanceBufferSetData(io->instanceBuffer, bufferSize, allWorldMatrices, false); 
  }
}

uint32 ldkInstancedObjectCount(const LDKInstancedObject* io)
{
  return ldkArrayCount(io->instanceArray);
}

LDKObjectInstance* ldkInstancedObjectGetAll(const LDKInstancedObject* io, uint32* outCount)
{
  if (outCount)
    *outCount = ldkInstancedObjectCount(io);
  return ldkArrayGetData(io->instanceArray);
}

LDKObjectInstance* ldkInstancedObjectGet(const LDKInstancedObject* io, uint32 index)
{
  const uint32 count = ldkInstancedObjectCount(io);
  if (index >= count)
    return NULL;

  return ldkArrayGet(io->instanceArray, index);
}

void ldkInstancedObjectDeleteInterval(LDKInstancedObject* io, uint32 index, uint32 count)
{
  ldkArrayDeleteRange(io->instanceArray, index, index + count);
}

void ldkInstancedObjectDelete(LDKInstancedObject* io, uint32 index)
{
  ldkArrayDeleteAt(io->instanceArray, index);
}

void ldkInstancedObjectDeleteAll(LDKInstancedObject* io)
{
  ldkArrayClear(io->instanceArray);
  ldkArrayClear(io->instanceWorldMatrixArray);
}

void ldkInstancedObjectEntityGetTransform(LDKHEntity handle, uint32 instanceId, Vec3* pos, Vec3* scale, Quat* rot)
{
  LDKInstancedObject* io = ldkEntityLookup(LDKInstancedObject, handle);
  LDK_ASSERT(io != NULL);
  LDKObjectInstance* o = ldkInstancedObjectGet(io, instanceId);
  LDK_ASSERT(o != NULL);
  if (pos)    *pos = o->position;
  if (scale)  *scale = o->scale;
  if (rot)    *rot = o->rotation;

}

void ldkInstancedObjectEntitySetTransform(LDKHEntity handle, uint32 instanceId, Vec3 pos, Vec3 scale, Quat rot)
{
  LDKInstancedObject* io = ldkEntityLookup(LDKInstancedObject, handle);
  LDK_ASSERT(io != NULL);
  LDKObjectInstance* o = ldkInstancedObjectGet(io, instanceId);
  o->position = pos;
  o->scale = scale;
  o->rotation = rot;
  ldkInstancedObjectUpdate(io);
}

