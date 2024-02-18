#include "ldk/entity/camera.h"
#include "ldk/module/graphics.h"
#include "ldk/maths.h"
#include "ldk/os.h"

LDKCamera* ldkCameraEntityCreate(LDKCamera* entity)
{
  entity->fovRadian = (float) degToRadian(40.0f);
  entity->nearPlane = 0.1f;
  entity->farPlane  = 100.0f;
  entity->position  = vec3(0.0f, 0.0f, 10.0f);
  entity->target    = vec3Zero();

  return entity;
}

void ldkCameraEntityDestroy(LDKCamera* entity)
{
}

Vec3 ldkCameraDirectionNormalized(LDKCamera* camera)
{
  return vec3Normalize(vec3Sub(camera->target, camera->position));
}

Mat4 ldkCameraViewMatrix(LDKCamera* camera)
{
  return mat4ViewLookAt(camera->position, camera->target, vec3Up());
}

Mat4 ldkCameraProjectMatrix(LDKCamera* camera)
{
  return mat4Perspective(camera->fovRadian, camera->nearPlane, camera->farPlane, ldkGraphicsViewportRatio());
}

Mat4 ldkCameraViewProjectMatrix(LDKCamera* camera)
{
  Mat4 view = ldkCameraViewMatrix(camera);
  Mat4 proj = ldkCameraProjectMatrix(camera);
  return mat4MulMat4(proj, view);
}

