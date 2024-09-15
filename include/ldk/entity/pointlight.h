/**
 * pointlight.h
 *
 * A point light entity
 *
 */

#ifndef LDK_POINTLIGHT_H
#define LDK_POINTLIGHT_H

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
    Vec3 colorDiffuse;
    Vec3 colorSpecular;
    LDKLightAttenuation attenuation;
  } LDKPointLight;

  LDK_API LDKPointLight* ldkPointLightEntityCreate(LDKPointLight* entity);
  LDK_API void ldkPointLightEntityDestroy(LDKPointLight* entity);
  LDK_API void ldkPointLightEntityGetTransform(LDKHEntity handle, uint32 instanceId, Vec3* pos, Vec3* scale, Quat* rot);
  LDK_API void ldkPointLightEntitySetTransform(LDKHEntity handle, uint32 instanceId, Vec3 pos, Vec3 scale, Quat rot);

#ifdef __cplusplus
}
#endif

#endif //LDK_POINTLIGHT_H

