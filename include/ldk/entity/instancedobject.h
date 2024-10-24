/**
 * instancedobject.h
 *
 * An instanced mesh entity. It is possible to add, remove and tweak instances
 * at will. As this is implemented on top of an LDKArena, adding instances might
 * not necessarily require heap allocation, unless the arena needs to be
 * expanded. Retrieveing and modifying instnaces is also fast considering all
 * instances are stored in a large array. Updating the instance transformations
 * via ldkInstancedObjectUpdate() can be potentially slow depending on the
 * number of instances.
 * 
 */
#ifndef LDK_INSTANCEDMESH_H
#define LDK_INSTANCEDMESH_H

#include "../common.h"
#include "../module/rendererbackend.h"
#include "../maths.h"
#include "../array.h"
#include "../module/entity.h"

#ifdef __cplusplus
extern "C" {
#endif

  typedef struct
  {
    Vec3 position;
    Vec3 scale;
    Quat rotation;
    Mat4 world;
  } LDKObjectInstance;

  typedef struct
  {
    LDK_DECLARE_ENTITY;
    LDKArray* instanceArray;
    LDKArray* instanceWorldMatrixArray;
    LDKInstanceBuffer* instanceBuffer;
    LDKHAsset mesh;
  } LDKInstancedObject;

  LDK_API LDKInstancedObject* ldkInstancedObjectEntityCreate(LDKInstancedObject* entity);
  LDK_API void ldkInstancedObjectEntityDestroy(LDKInstancedObject* entity);

  LDK_API void ldkInstancedObjectAddInstance(LDKInstancedObject* io, Vec3 positoin, Vec3 scale, Quat rotation);
  LDK_API void ldkInstancedObjectUpdate(LDKInstancedObject* io);
  LDK_API uint32 ldkInstancedObjectCount(const LDKInstancedObject* io);
  LDK_API LDKObjectInstance* ldkInstancedObjectGetAll(const LDKInstancedObject* io, uint32* outCount);
  LDK_API LDKObjectInstance* ldkInstancedObjectGet(const LDKInstancedObject* io, uint32 index);
  LDK_API void ldkInstancedObjectDeleteInterval(LDKInstancedObject* io, uint32 index, uint32 count);
  LDK_API void ldkInstancedObjectDelete(LDKInstancedObject* io, uint32 index);
  LDK_API void ldkInstancedObjectEntityGetTransform(LDKHEntity handle, uint32 instanceId, Vec3* pos, Vec3* scale, Quat* rot);
  LDK_API void ldkInstancedObjectEntitySetTransform(LDKHEntity handle, uint32 instanceId, Vec3 pos, Vec3 scale, Quat rot);

#ifdef __cplusplus
}
#endif

#endif //LDK_INSTANCEDMESH_H

