#include "ldk/entity/camera.h"
#include "ldk/module/graphics.h"
#include "ldk/maths.h"
#include "ldk/os.h"
#include "ldk/maths.h"
#include <math.h>

LDKCamera* ldkCameraEntityCreate(LDKCamera* entity)
{
  entity->fovRadian = (float) degToRadian(40.0f);
  entity->nearPlane = 0.1f;
  entity->farPlane  = 1000.0f;
  entity->position  = vec3(0.0f, 0.0f, 10.0f);
  entity->target    = vec3Zero();
  entity->orthoSize = 3.0f;
  entity->type = LDK_CAMERA_PERSPECTIVE;

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
  if (camera->type == LDK_CAMERA_ORTHOGRAPHIC)
  {
    LDKSize vpSize = ldkGraphicsViewportSizeGet();
    float hSize = (camera->orthoSize * vpSize.width) / vpSize.height;

    float left = -hSize;
    float right = hSize;
    float top = camera->orthoSize;
    float bottom = -camera->orthoSize;
    return mat4Orthographic(left, right, bottom, top, camera->nearPlane, camera->farPlane);
  }
  else
  {
    return mat4Perspective(camera->fovRadian, camera->nearPlane, camera->farPlane, ldkGraphicsViewportRatio());
  }
}

Mat4 ldkCameraViewProjectMatrix(LDKCamera* camera)
{
  Mat4 view = ldkCameraViewMatrix(camera);
  Mat4 proj = ldkCameraProjectMatrix(camera);
  return mat4MulMat4(proj, view);
}

void ldkCameraUpdateFreeCamera(LDKCamera* camera, float deltaTime, float lookSpeed, float moveSpeed)
{
  Vec3 camDir = ldkCameraDirectionNormalized(camera);
  Vec3 sideDir = vec3Normalize(vec3Cross(camDir, vec3Up()));

  moveSpeed *= deltaTime;
  lookSpeed *= deltaTime;

  //
  // Look
  //
  {
    LDKMouseState mouseState;
    ldkOsMouseStateGet(&mouseState);
    bool looking = ldkOsMouseButtonIsPressed(&mouseState, LDK_MOUSE_BUTTON_RIGHT);
    bool strafing = ldkOsMouseButtonIsPressed(&mouseState, LDK_MOUSE_BUTTON_MIDDLE);

    Vec3 cam_up = vec3Normalize(vec3Cross(camDir, sideDir));
    LDKPoint relative = ldkOsMouseCursorRelative(&mouseState);
    int32 wheelDelta = ldkOsMouseWheelDelta(&mouseState);

    const int32 xRel = relative.x;
    const int32 yRel = relative.y;

    if (looking)
    {
      camDir.y += -(yRel * deltaTime * lookSpeed);

      camDir = vec3Add(camDir, vec3Mul(sideDir, (xRel * deltaTime * lookSpeed)));
      camera->target = vec3Add(camera->position, vec3Normalize(camDir));
    }

    if (strafing)
    {
      if (yRel < 0)
      {
        camera->position = vec3Add(camera->position, vec3Mul(cam_up, lookSpeed));
        camera->target = vec3Add(camera->target, vec3Mul(cam_up, lookSpeed));
      }
      else if (yRel > 0)
      {
        camera->position = vec3Sub(camera->position, vec3Mul(cam_up, lookSpeed));
        camera->target = vec3Sub(camera->target, vec3Mul(cam_up, lookSpeed));
      }

      if (xRel < 0)
      {
        camera->position = vec3Add(camera->position, vec3Mul(sideDir, lookSpeed));
        camera->target = vec3Add(camera->target, vec3Mul(sideDir, lookSpeed));
      }
      else if (xRel > 0)
      {
        camera->position = vec3Sub(camera->position, vec3Mul(sideDir, lookSpeed));
        camera->target = vec3Sub(camera->target, vec3Mul(sideDir, lookSpeed));
      }
    }

    if (wheelDelta > 0)
    {
      camera->position = vec3Add(camera->position, vec3Mul(camDir, moveSpeed));
      camera->target = vec3Add(camera->target, vec3Mul(camDir, moveSpeed));
    }
    else if (wheelDelta < 0)
    {
      camera->position = vec3Sub(camera->position, vec3Mul(camDir, moveSpeed));
      camera->target = vec3Sub(camera->target, vec3Mul(camDir, moveSpeed));
    }
  }


  //
  // Move
  //

  {
    LDKKeyboardState keyboardState;
    ldkOsKeyboardStateGet(&keyboardState);

    if (ldkOsKeyboardKeyIsPressed(&keyboardState, LDK_KEYCODE_W))
    {
      camera->position = vec3Add(camera->position, vec3Mul(camDir, moveSpeed));
      camera->target = vec3Add(camera->target, vec3Mul(camDir, moveSpeed));
    }

    if (ldkOsKeyboardKeyIsPressed(&keyboardState, LDK_KEYCODE_S))
    {
      camera->position = vec3Sub(camera->position, vec3Mul(camDir, moveSpeed));
      camera->target = vec3Sub(camera->target, vec3Mul(camDir, moveSpeed));
    }

    if (ldkOsKeyboardKeyIsPressed(&keyboardState, LDK_KEYCODE_D))
    {
      camera->position = vec3Add(camera->position, vec3Mul(sideDir, moveSpeed));
      camera->target = vec3Add(camera->target, vec3Mul(sideDir, moveSpeed));
    }

    if (ldkOsKeyboardKeyIsPressed(&keyboardState, LDK_KEYCODE_A))
    {
      camera->position = vec3Sub(camera->position, vec3Mul(sideDir, moveSpeed));
      camera->target = vec3Sub(camera->target, vec3Mul(sideDir, moveSpeed));
    }
  }
}


Vec3 ldkCameraScreenCoordToWorldRay(LDKCamera* camera, uint32 x, uint32 y)
{
  // Convert viewport coordinates to normalized device coordinates (NDC)
  LDKSize vpSize = ldkGraphicsViewportSizeGet();
  Vec2 ndc = vec2(
      (2.0f * x) / vpSize.width - 1.0f,
      (2.0f * (vpSize.height - y)) / vpSize.height - 1.0f);

  // Ensure correct NDC conversion
  LDK_ASSERT(ndc.x >= -1.0f && ndc.x <= 1.0f);
  LDK_ASSERT(ndc.y >= -1.0f && ndc.y <= 1.0f);

  // Transform NDC coordinates to view space
  Mat4 projectionInv = mat4Inverse(ldkCameraProjectMatrix(camera));
  Vec4 clipCoord = vec4(ndc.x, ndc.y, 0.0f, 1.0f);
  Vec4 eyeCoord = mat4MulVec4(projectionInv, clipCoord);
  eyeCoord.z = -1.0f; eyeCoord.w = 0.0f;

  // View to World space
  Mat4 viewInv = mat4Inverse(ldkCameraViewMatrix(camera));
  Vec4 worldPoint = mat4MulVec4(viewInv, eyeCoord);

  // Return a ray poiting from mouse position to world
  return vec3Normalize(vec3(worldPoint.x, worldPoint.y, worldPoint.z));
}

#ifdef LDK_EDITOR

void inspectVec3(Vec3 v, Vec3 z)
{
  ldkLogInfo("vec3(%4.2f,%4.2f,%4.2f) -> vec3(%4.2f,%4.2f,%4.2f)", v.x, v.y, v.z, z.x, z.y, z.z);
}

void ldkCameraEntityOnEditorGetTransform (LDKEntitySelectionInfo* selection, Vec3* pos, Vec3* scale, Quat* rot)
{
  LDKCamera* o = ldkEntityLookup(LDKCamera, selection->handle);
  LDK_ASSERT(o != NULL);
  if (pos)    *pos = o->position;
  if (scale)  *scale = vec3One();
  if (rot)    *rot = quatFromEuler(ldkCameraDirectionNormalized(o));
}

void ldkCameraEntityOnEditorSetTransform(LDKEntitySelectionInfo*selection, Vec3 pos, Vec3 _, Quat rot)
{
  LDKCamera* o = ldkEntityLookup(LDKCamera, selection->handle);
  LDK_ASSERT(o != NULL);

  float distance = vec3Length(vec3Sub(o->target, o->position));
  Vec3 direction = vec3Normalize(quatToEuler(rot));
  Vec3 t = vec3Add(pos, vec3Mul(direction, distance));
  o->target = t;
  vec3Print(o->target);
  o->position = pos;
}
#endif //LDK_EDITOR

