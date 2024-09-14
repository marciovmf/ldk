#include "ldk/entity/instancedobject.h"
#include "common.h"
#include "ldk/maths.h"
#include "ldk/array.h"
#include <string.h>


LDKInstancedObject* ldkInstancedObjectEntityCreate(LDKInstancedObject* entity)
{
  entity->mesh = LDK_HASSET_INVALID;
  entity->instanceBuffer = ldkInstanceBufferCreate();
  entity->instanceArray = ldkArrayCreate(sizeof(LDKObjectInstance), 16);
  entity->instanceWorldMatrixArray = ldkArrayCreate(sizeof(Mat4), 16);
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
  LDKObjectInstance instance;
  instance.position = position;
  instance.scale    = scale;
  instance.rotation = rotation;

  ldkArrayAdd(entity->instanceArray, &instance);
  ldkArrayAdd(entity->instanceWorldMatrixArray, NULL); // just reserve space
}

void ldkInstancedObjectUpdate(LDKInstancedObject* io)
{
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

#ifdef LDK_EDITOR
     
void ldkInstancedObjectEntityOnEditorGetTransform(LDKEntitySelectionInfo* selection, Vec3* pos, Vec3* scale, Quat* rot)
{
  LDKInstancedObject* io = ldkEntityLookup(LDKInstancedObject, selection->handle);
  LDK_ASSERT(io != NULL);
  LDKObjectInstance* o = ldkInstancedObjectGet(io, selection->instanceIndex);
  LDK_ASSERT(o != NULL);
  if (pos)    *pos = o->position;
  if (scale)  *scale = o->scale;
  if (rot)    *rot = o->rotation;

}

void ldkInstancedObjectEntityOnEditorSetTransform(LDKEntitySelectionInfo*selection, Vec3 pos, Vec3 scale, Quat rot)
{
  LDKInstancedObject* io = ldkEntityLookup(LDKInstancedObject, selection->handle);
  LDK_ASSERT(io != NULL);
  LDKObjectInstance* o = ldkInstancedObjectGet(io, selection->instanceIndex);
  o->position = pos;
  o->scale = scale;
  o->rotation = rot;
  ldkInstancedObjectUpdate(io);
}
#endif // LDK_EDITOR
