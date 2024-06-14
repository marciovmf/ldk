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
    float range;  // If an objects distance is greater than the range, the light has no effect on the object. 

    // Attenuation
    float linear;
    float quadratic;
  } LDKPointLight;

  LDK_API LDKPointLight* ldkPointLightEntityCreate(LDKPointLight* entity);
  LDK_API void ldkPointLightEntityDestroy(LDKPointLight* entity);
  LDK_API void ldkPointLightSetAttenuationPreset(LDKPointLight* entity, float range);
  LDK_API void ldkPointLightSetAttenuation(LDKPointLight* entity, float range, float linear, float quadratic);


#ifdef __cplusplus
}
#endif

#endif //LDK_POINTLIGHT_H

