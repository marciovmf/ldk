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
#include "ldk/module/entity.h"

#ifdef __cplusplus
extern "C" {
#endif

  typedef enum
  {
    LDK_CAMERA_PERSPECTIVE = 0,
    LDK_CAMERA_ORTHOGRAPHIC = 1
  } LDKCameraType;


  typedef struct
  {
    LDK_DECLARE_ENTITY;
    LDKCameraType type;
    Vec3 position;
    Vec3 target;
    float orthoSize;
    float fovRadian; 
    float nearPlane;
    float farPlane;
  } LDKCamera; 


  LDK_API LDKCamera* ldkCameraEntityCreate(LDKCamera* entity);
  LDK_API void ldkCameraEntityDestroy(LDKCamera* entity);
  LDK_API Vec3 ldkCameraDirectionNormalized(LDKCamera* camera);
  LDK_API Mat4 ldkCameraViewMatrix(LDKCamera* camera);
  LDK_API Mat4 ldkCameraProjectMatrix(LDKCamera* camera);
  LDK_API Mat4 ldkCameraViewProjectMatrix(LDKCamera* camera);
  LDK_API void ldkCameraUpdateFreeCamera(LDKCamera* camera, float deltaTime, float lookSpeed, float moveSpeed);

#ifdef __cplusplus
}
#endif

#endif //LDK_CAMERA_H

