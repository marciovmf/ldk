/**
 * staticmesh.h
 *
 */
#ifndef LDK_INSTANCEDMESH_H
#define LDK_INSTANCEDMESH_H

#include "../common.h"
#include "../maths.h"
#include "../arena.h"
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
  LDKArena instanceList;
  LDKArena instanceWorldMatrixList;
  LDKHandle mesh;
  LDKInstanceBuffer* instanceBuffer;
} LDKInstancedObject;

LDK_API LDKInstancedObject* ldkInstancedObjectEntityCreate(LDKInstancedObject* entity);
LDK_API void ldkInstancedObjectEntityDestroy(LDKInstancedObject* entity);

LDK_API void ldkInstancedObjectAddInstance(LDKInstancedObject* io, Vec3 positoin, Vec3 scale, Quat rotation);
LDK_API void ldkInstancedObjectUpdate(LDKInstancedObject* io);
LDK_API uint32 ldkInstancedObjectCount(LDKInstancedObject* io);

#ifdef __cplusplus
}
#endif

#endif //LDK_INSTANCEDMESH_H

