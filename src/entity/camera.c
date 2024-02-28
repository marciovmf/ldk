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


void ldkCameraUpdateFreeCamera(LDKCamera* camera, float deltaTime)
{
  const float speed = 30.0f * deltaTime;
  const float speedWheel = 35.0f * deltaTime;
  const float moveSpeed = 30 * deltaTime;
  Vec3 cam_dir = ldkCameraDirectionNormalized(camera);
  Vec3 side_dir = vec3Normalize(vec3Cross(cam_dir, vec3Up()));

  //
  // Look
  //
  {
    LDKMouseState mouseState;
    ldkOsMouseStateGet(&mouseState);
    bool looking = ldkOsMouseButtonIsPressed(&mouseState, LDK_MOUSE_BUTTON_RIGHT);
    bool strafing = ldkOsMouseButtonIsPressed(&mouseState, LDK_MOUSE_BUTTON_MIDDLE);

    Vec3 cam_up = vec3Normalize(vec3Cross(cam_dir, side_dir));
    LDKPoint relative = ldkOsMouseCursorRelative(&mouseState);
    int32 wheelDelta = ldkOsMouseWheelDelta(&mouseState);

    const int32 xRel = relative.x;
    const int32 yRel = relative.y;

    if (looking)
    {
      cam_dir.y += -(yRel * deltaTime * speed);

      cam_dir = vec3Add(cam_dir, vec3Mul(side_dir, (xRel * deltaTime * speed)));
      camera->target = vec3Add(camera->position, vec3Normalize(cam_dir));
    }

    if (strafing)
    {
      if (yRel < 0)
      {
        camera->position = vec3Add(camera->position, vec3Mul(cam_up, speed));
        camera->target = vec3Add(camera->target, vec3Mul(cam_up, speed));
      }
      else if (yRel > 0)
      {
        camera->position = vec3Sub(camera->position, vec3Mul(cam_up, speed));
        camera->target = vec3Sub(camera->target, vec3Mul(cam_up, speed));
      }

      if (xRel < 0)
      {
        camera->position = vec3Add(camera->position, vec3Mul(side_dir, speed));
        camera->target = vec3Add(camera->target, vec3Mul(side_dir, speed));
      }
      else if (xRel > 0)
      {
        camera->position = vec3Sub(camera->position, vec3Mul(side_dir, speed));
        camera->target = vec3Sub(camera->target, vec3Mul(side_dir, speed));
      }
    }

    if (wheelDelta > 0)
    {
      camera->position = vec3Add(camera->position, vec3Mul(cam_dir, speedWheel));
      camera->target = vec3Add(camera->target, vec3Mul(cam_dir, speedWheel));
    }
    else if (wheelDelta < 0)
    {
      camera->position = vec3Sub(camera->position, vec3Mul(cam_dir, speedWheel));
      camera->target = vec3Sub(camera->target, vec3Mul(cam_dir, speedWheel));
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
      camera->position = vec3Add(camera->position, vec3Mul(cam_dir, moveSpeed));
      camera->target = vec3Add(camera->target, vec3Mul(cam_dir, speed));
    }

    if (ldkOsKeyboardKeyIsPressed(&keyboardState, LDK_KEYCODE_S))
    {
      camera->position = vec3Sub(camera->position, vec3Mul(cam_dir, moveSpeed));
      camera->target = vec3Sub(camera->target, vec3Mul(cam_dir, speed));
    }

    if (ldkOsKeyboardKeyIsPressed(&keyboardState, LDK_KEYCODE_D))
    {
      camera->position = vec3Add(camera->position, vec3Mul(side_dir, moveSpeed));
      camera->target = vec3Add(camera->target, vec3Mul(side_dir, speed));
    }

    if (ldkOsKeyboardKeyIsPressed(&keyboardState, LDK_KEYCODE_A))
    {
      camera->position = vec3Sub(camera->position, vec3Mul(side_dir, moveSpeed));
      camera->target = vec3Sub(camera->target, vec3Mul(side_dir, speed));
    }
  }

}
