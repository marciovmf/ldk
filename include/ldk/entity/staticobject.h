/**
 * staticobject.h
 *
 * A static mesh entity
 *
 */

#ifndef LDK_STATICOBJECT_H
#define LDK_STATICOBJECT_H

#include "../common.h"
#include "../maths.h"
#include "ldk/module/entity.h"

#ifdef __cplusplus
extern "C" {
#endif

  typedef struct 
  {
    LDK_DECLARE_ENTITY;
    Vec3 position;
    Vec3 scale;
    Quat rotation;
    LDKHAsset mesh;
    LDKHAsset* materials; // If this is not null, this material list will override the Mesh list
  } LDKStaticObject;

  LDK_API LDKStaticObject* ldkStaticObjectEntityCreate(LDKStaticObject* entity);
  LDK_API void ldkStaticObjectEntityDestroy(LDKStaticObject* entity);
  LDK_API void ldkStaticObjectSetMesh(LDKStaticObject* entity, LDKHAsset hMesh);
  LDK_API void ldkStaticObjectEntityGetTransform (LDKHEntity handle, uint32 instanceId, Vec3* pos, Vec3* scale, Quat* rot);
  LDK_API void ldkStaticObjectEntitySetTransform(LDKHEntity handle, uint32 instanceId, Vec3 pos, Vec3 scale, Quat rot);

#ifdef __cplusplus
}
#endif

#endif //LDK_STATICOBJECT_H

