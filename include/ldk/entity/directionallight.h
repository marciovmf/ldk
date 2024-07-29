/**
 * directionallight.h
 *
 * A directinal light entity
 *
 */

#ifndef LDK_DIRECTIONALLIGHT_H
#define LDK_DIRECTIONALLIGHT_H

#include "../common.h"
#include "../maths.h"
#include "ldk/module/entity.h"

#ifdef __cplusplus
extern "C" {
#endif

  typedef struct 
  {
    LDK_DECLARE_ENTITY;
    Vec3 position; // This is not used by light calculation but for editor gizmo
    Vec3 direction;
    Vec3 colorDiffuse;
    Vec3 colorSpecular;
  } LDKDirectionalLight;

  LDK_API LDKDirectionalLight* ldkDirectionalLightEntityCreate(LDKDirectionalLight* entity);
  LDK_API void ldkDirectionalLightEntityDestroy(LDKDirectionalLight* entity);

#ifdef LDK_EDITOR
  LDK_API void ldkDirectionalLightEntityOnEditorGetTransform (LDKEntitySelectionInfo* selection, Vec3* pos, Vec3* scale, Quat* rot);
  LDK_API void ldkDirectionalLightEntityOnEditorSetTransform(LDKEntitySelectionInfo*selection, Vec3 pos, Vec3 scale, Quat rot);
#endif // LDK_EDITOR

#ifdef __cplusplus
}
#endif

#endif //LDK_DIRECTIONALLIGHT_H

