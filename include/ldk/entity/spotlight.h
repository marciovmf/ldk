/**
 * spotlight.h
 *
 * A spotlight light entity
 *
 */

#ifndef LDK_SPOTLIGHT_H
#define LDK_SPOTLIGHT_H

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
    Vec3 direction;
    Vec3 colorDiffuse;
    Vec3 colorSpecular;
    float cutOffInner;
    float cutOffOuter;
    LDKLightAttenuation attenuation;
  } LDKSpotLight;

  LDK_API LDKSpotLight* ldkSpotLightEntityCreate(LDKSpotLight* entity);
  LDK_API void ldkSpotLightEntityDestroy(LDKSpotLight* entity);
  LDK_API void ldkSpotLightEntityGetTransform(LDKHEntity handle, uint32 instanceIndex, Vec3* pos, Vec3* scale, Quat* rot);
  LDK_API void ldkSpotLightEntitySetTransform(LDKHEntity handle, uint32 instanceIndex, Vec3 pos, Vec3 scale, Quat rot);

#ifdef __cplusplus
}
#endif

#endif //LDK_SPOTLIGHT_H

