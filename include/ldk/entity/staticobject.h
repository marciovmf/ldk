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
    LDKHandle mesh;
  } LDKStaticObject;

  LDK_API LDKStaticObject* ldkStaticObjectEntityCreate(LDKStaticObject* entity);
  LDK_API void ldkStaticObjectEntityDestroy(LDKStaticObject* entity);

#ifdef __cplusplus
}
#endif

#endif //LDK_STATICOBJECT_H

