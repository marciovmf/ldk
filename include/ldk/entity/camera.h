/**
 * camera.h
 *
 * A Perspective camera entity
 *
 */

#ifndef LDK_CAMERA_H
#define LDK_CAMERA_H

#include "ldk/common.h"
#include "ldk/maths.h"

#ifdef __cplusplus
extern "C" {
#endif

  typedef struct
  {
    Vec3 position;
    Vec3 target;
    float fovRadian; 
    float nearPlane;
    float farPlane;
  } LDKCamera;


  LDK_API LDKCamera* ldkCameraCreate();
  LDK_API void ldkCameraDestroy(LDKCamera* camera);
  LDK_API Vec3 ldkCameraDirectionNormalized(LDKCamera* camera);
  LDK_API Mat4 ldkCameraViewMatrix(LDKCamera* camera);
  LDK_API Mat4 ldkCameraProjectMatrix(LDKCamera* camera);
  LDK_API Mat4 ldkCameraViewProjectMatrix(LDKCamera* camera);

#ifdef __cplusplus
}
#endif

#endif //LDK_CAMERA_H

